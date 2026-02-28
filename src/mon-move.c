/**
 * \file mon-move.c
 * \brief Movimiento de monstruos
 *
 * IA de monstruos que afecta el movimiento y los hechizos, procesa un monstruo
 * (con hechizos y acciones de todo tipo, reproducción, efectos de cualquier
 * terreno en el movimiento del monstruo, recoger y destruir objetos),
 * procesa todos los monstruos.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "angband.h"
#include "cave.h"
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-desc.h"
#include "mon-group.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-move.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "mon-timed.h"
#include "obj-desc.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"


/**
 * ------------------------------------------------------------------------
 * Rutinas para permitir decisiones sobre el comportamiento del monstruo
 * ------------------------------------------------------------------------ */
/**
 * De Will Asher en DJA:
 * Encontrar si un monstruo está cerca de una pared permanente
 *
 * esto decide si los monstruos PASS_WALL & KILL_WALL usan el código de flujo de monstruos
 */
static bool monster_near_permwall(const struct monster *mon)
{
	struct loc gp[512];
	int path_grids, j;

	/* Si el jugador está en LdV, no hay necesidad de rodear paredes */
    if (projectable(cave, mon->grid, player->grid, PROJECT_SHORT)) return false;

    /* Los monstruos PASS_WALL & KILL_WALL ocasionalmente fluyen por un turno de todos modos */
    if (randint0(99) < 5) return true;

	/* Encontrar la ruta más corta */
	path_grids = project_path(cave, gp, z_info->max_sight, mon->grid,
		player->grid, PROJECT_ROCK);

	/* Ver si podemos "ver" al jugador sin golpear pared permanente */
	for (j = 0; j < path_grids; j++) {
		if (square_isperm(cave, gp[j])) return true;
		if (square_isplayer(cave, gp[j])) return false;
	}

	return false;
}

/**
 * Verificar si el monstruo puede ver al jugador
 */
static bool monster_can_see_player(struct monster *mon)
{
	if (!square_isview(cave, mon->grid)) return false;
	if (player->timed[TMD_COVERTRACKS] && (mon->cdis > z_info->max_sight / 4)) {
		return false;
	}
	return true;
}

/**
 * Verificar si el monstruo puede oír algo
 */
static bool monster_can_hear(struct monster *mon)
{
	int base_hearing = mon->race->hearing
		- player->state.skills[SKILL_STEALTH] / 3;
	if (cave->noise.grids[mon->grid.y][mon->grid.x] == 0) {
		return false;
	}
	return base_hearing > cave->noise.grids[mon->grid.y][mon->grid.x];
}

/**
 * Verificar si el monstruo puede oler algo
 */
static bool monster_can_smell(struct monster *mon)
{
	if (cave->scent.grids[mon->grid.y][mon->grid.x] == 0) {
		return false;
	}
	return mon->race->smell > cave->scent.grids[mon->grid.y][mon->grid.x];
}

/**
 * Comparar la "fuerza" de dos monstruos XXX XXX XXX
 */
static int compare_monsters(const struct monster *mon1,
							const struct monster *mon2)
{
	uint32_t mexp1 = (mon1->original_race) ?
		mon1->original_race->mexp : mon1->race->mexp;
	uint32_t mexp2 = (mon2->original_race) ?
		mon2->original_race->mexp : mon2->race->mexp;

	/* Comparar */
	if (mexp1 < mexp2) return (-1);
	if (mexp1 > mexp2) return (1);

	/* Asumir iguales */
	return (0);
}

/**
 * Verificar si el monstruo puede matar a cualquier monstruo en la casilla relevante
 */
static bool monster_can_kill(struct monster *mon, struct loc grid)
{
	struct monster *mon1 = square_monster(cave, grid);

	/* Sin monstruo */
	if (!mon1) return true;

	/* No pisotear únicos */
	if (monster_is_unique(mon1)) {
		return false;
	}

	if (rf_has(mon->race->flags, RF_KILL_BODY) &&
		compare_monsters(mon, mon1) > 0) {
		return true;
	}

	return false;
}

/**
 * Verificar si el monstruo puede mover a cualquier monstruo en la casilla relevante
 */
static bool monster_can_move(struct monster *mon, struct loc grid)
{
	struct monster *mon1 = square_monster(cave, grid);

	/* Sin monstruo */
	if (!mon1) return true;

	if (rf_has(mon->race->flags, RF_MOVE_BODY) &&
		compare_monsters(mon, mon1) > 0) {
		return true;
	}

	return false;
}

/**
 * Verificar si el monstruo puede ocupar una casilla de forma segura
 */
static bool monster_hates_grid(struct monster *mon, struct loc grid)
{
	/* Solo algunas criaturas pueden manejar terreno dañino */
	if (square_isdamaging(cave, grid) &&
		!rf_has(mon->race->flags, square_feat(cave, grid)->resist_flag)) {
		return true;
	}
	return false;
}

/**
 * ------------------------------------------------------------------------
 * Rutinas de movimiento de monstruos
 * Estas rutinas, que culminan en get_move(), eligen si y dónde se moverá
 * un monstruo en su turno
 * ------------------------------------------------------------------------ */
/**
 * Calcular los rangos de combate mínimo y deseado.  -BR-
 *
 * Los monstruos asustados establecerán esto a su distancia de huida máxima.
 * Actualmente esto se recalcula cada turno - si se convierte en una sobrecarga
 * significativa podría calcularse solo cuando algo ha cambiado (PG del monstruo,
 * probabilidad de escapar, etc.)
 */
static void get_move_find_range(struct monster *mon)
{
	uint16_t p_lev, m_lev;
	uint16_t p_chp, p_mhp;
	uint16_t m_chp, m_mhp;
	uint32_t p_val, m_val;

	/* Los monstruos huirán hasta z_info->flee_range casillas fuera de la vista */
	int flee_range = z_info->max_sight + z_info->flee_range;

	/* Todos los monstruos "asustados" huirán */
	if (mon->m_timed[MON_TMD_FEAR] || rf_has(mon->race->flags, RF_FRIGHTENED)) {
		mon->min_range = flee_range;
	} else if (mon->group_info[PRIMARY_GROUP].role == MON_GROUP_BODYGUARD) {
		/* Los guardaespaldas no huyen */
		mon->min_range = 1;
	} else {
		/* Distancia mínima - permanecer al menos a esta distancia si es posible */
		mon->min_range = 1;

		/* Los monstruos provocados solo quieren enfrentarse */
		if (player->timed[TMD_TAUNT]) return;

		/* Examinar el poder del jugador (nivel) */
		p_lev = player->lev;

		/* Truco - aumentar p_lev basado en habilidades especiales */

		/* Examinar el poder del monstruo (nivel más moral) */
		m_lev = mon->race->level + (mon->midx & 0x08) + 25;

		/* Casos simples primero */
		if (m_lev + 3 < p_lev) {
			mon->min_range = flee_range;
		} else if (m_lev - 5 < p_lev) {

			/* Examinar la salud del jugador */
			p_chp = player->chp;
			p_mhp = player->mhp;

			/* Examinar la salud del monstruo */
			m_chp = mon->hp;
			m_mhp = mon->maxhp;

			/* Prepararse para optimizar el cálculo */
			p_val = (p_lev * p_mhp) + (p_chp << 2);	/* div p_mhp */
			m_val = (m_lev * m_mhp) + (m_chp << 2);	/* div m_mhp */

			/* Los jugadores fuertes asustan a los monstruos fuertes */
			if (p_val * m_mhp > m_val * p_mhp)
				mon->min_range = flee_range;
		}
	}

	if (mon->min_range < flee_range) {
		/* A las criaturas que no se mueven nunca les gusta acercarse demasiado */
		if (rf_has(mon->race->flags, RF_NEVER_MOVE))
			mon->min_range += 3;

		/* A los lanzadores de hechizos que nunca golpean nunca les gusta acercarse demasiado */
		if (rf_has(mon->race->flags, RF_NEVER_BLOW))
			mon->min_range += 3;
	}

	/* Rango máximo para huir */
	if (!(mon->min_range < flee_range)) {
		mon->min_range = flee_range;
	} else if (mon->cdis < z_info->turn_range) {
		/* Los monstruos cercanos no huirán */
		mon->min_range = 1;
	}

	/* Ahora encontrar el rango preferido */
	mon->best_range = mon->min_range;

	/* Los arqueros están bastante contentos a buena distancia */
	if (monster_loves_archery(mon)) {
		mon->best_range += 3;
	}

	/* Los que exhalan aliento prefieren el alcance de punto en blanco */
	if (mon->race->freq_innate > 24) {
		if (monster_breathes(mon) && (mon->hp > mon->maxhp / 2)) {
			mon->best_range = MAX(1, mon->best_range);
		}
	} else if (mon->race->freq_spell > 24) {
		/* Otros lanzadores de hechizos se mantendrán atrás y lanzarán */
		mon->best_range += 3;
	}
}

/**
 * Elegir la mejor dirección para un guardaespaldas.
 *
 * La idea es permanecer cerca del líder del grupo, pero atacar al jugador si
 * surge la oportunidad
 */
static bool get_move_bodyguard(struct monster *mon)
{
	int i;
	struct monster *leader = monster_group_leader(cave, mon);
	int dist;
	struct loc best;
	bool found = false;

	if (!leader) return false;

	/* Obtener distancia */
	dist = distance(mon->grid, leader->grid);

	/* Si actualmente está adyacente al líder, podemos permitirnos un movimiento */
	if (dist <= 1) return false;

	/* Si el líder está demasiado fuera de la vista y lejos, sálvate */
	if (!los(cave, mon->grid, leader->grid) && (dist > 10)) return false;

	/* Verificar las casillas adyacentes cercanas y evaluar */
	for (i = 0; i < 8; i++) {
		/* Obtener la ubicación */
		struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
		int new_dist = distance(grid, leader->grid);
		int char_dist = distance(grid, player->grid);

		/* Verificar límites */
		if (!square_in_bounds(cave, grid)) {
			continue;
		}

		/* Hay un monstruo bloqueando que no podemos manejar */
		if (!monster_can_kill(mon, grid) && !monster_can_move(mon, grid)){
			continue;
		}

		/* Hay terreno dañino */
		if (monster_hates_grid(mon, grid)) {
			continue;
		}

		/* Más cerca del líder siempre es mejor */
		if (new_dist < dist) {
			best = grid;
			found = true;
			/* Si hay una casilla que también está más cerca del jugador, esa gana */
			if (char_dist < mon->cdis) {
				break;
			}
		}
	}

	/* Si encontramos una, establecer el objetivo */
	if (found) {
		mon->target.grid = best;
		return true;
	}

	return false;
}


/**
 * Elegir la mejor dirección para avanzar hacia el jugador, usando sonido u olor.
 *
 * Los fantasmas y devoradores de rocas generalmente van directamente hacia el jugador.
 * Otros monstruos intentan ver, luego el sonido actual guardado en cave->noise.grids[y][x],
 * luego el olor actual guardado en cave->scent.grids[y][x].
 *
 * Esta función asume que el monstruo se mueve a una casilla adyacente, y por lo tanto el
 * ruido puede ser más fuerte como máximo en 1. La casilla objetivo del monstruo establecida por
 * rastreo de sonido u olor en esta función será una casilla a la que pueden dar un paso en un turno,
 * por lo que es la opción preferida para get_move() a menos que haya alguna razón
 * para no usarla.
 *
 * El rastreo por 'olor' significa que los monstruos terminan lo suficientemente cerca del
 * jugador para cambiar a 'sonido' (ruido), o terminan en algún lugar al que el jugador
 * se teletransportó. Teletransportarse lejos de una ubicación hará que los monstruos
 * que estaban persiguiendo al jugador converjan en esa ubicación mientras el jugador
 * todavía esté lo suficientemente cerca como para "molestarlos" sin estar lo suficientemente
 * cerca para perseguirlos directamente.
 */
static bool get_move_advance(struct monster *mon, bool *track)
{
	int i;
	struct loc target = monster_is_decoyed(mon) ? cave_find_decoy(cave) :
		player->grid;

	int base_hearing = mon->race->hearing
		- player->state.skills[SKILL_STEALTH] / 3;
	int current_noise = base_hearing
		- cave->noise.grids[mon->grid.y][mon->grid.x];
	int best_scent = 0;

	struct loc best_grid;
	struct loc backup_grid;
	bool found = false;
	bool found_backup = false;

	/* Los guardaespaldas son especiales */
	if (mon->group_info[PRIMARY_GROUP].role == MON_GROUP_BODYGUARD) {
		if (get_move_bodyguard(mon)) {
			return true;
		}
	}

	/* Si el monstruo puede pasar a través de paredes cercanas, hacer eso */
	if (monster_passes_walls(mon) && !monster_near_permwall(mon)) {
		mon->target.grid = target;
		return true;
	}

	/* Si el jugador puede ver al monstruo, establecer objetivo y correr hacia ellos */
	if (monster_can_see_player(mon)) {
		mon->target.grid = target;
		return true;
	}

	/* Intentar usar el sonido */
	if (monster_can_hear(mon)) {
		/* Verificar sonido cercano, dando preferencia a las direcciones cardinales */
		for (i = 0; i < 8; i++) {
			/* Obtener la ubicación */
			struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
			int heard_noise = base_hearing - cave->noise.grids[grid.y][grid.x];

			/* Verificar límites */
			if (!square_in_bounds(cave, grid)) {
				continue;
			}

			/* Debe haber algo de ruido */
			if (cave->noise.grids[grid.y][grid.x] == 0) {
				continue;
			}

			/* Hay un monstruo bloqueando que no podemos manejar */
			if (!monster_can_kill(mon, grid) && !monster_can_move(mon, grid)) {
				continue;
			}

			/* Hay terreno dañino */
			if (monster_hates_grid(mon, grid)) {
				continue;
			}

			/* Si es mejor que el ruido actual, elegir esta dirección */
			if (heard_noise > current_noise) {
				best_grid = grid;
				found = true;
				break;
			} else if (heard_noise == current_noise) {
				/* Movimiento posible si no podemos acercarnos realmente */
				backup_grid = grid;
				found_backup = true;
				continue;
			}
		}
	}

	/* Si tanto la visión como el sonido no sirven, usar el olor */
	if (monster_can_smell(mon) && !found) {
		for (i = 0; i < 8; i++) {
			/* Obtener la ubicación */
			struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);
			int smelled_scent;

			/* Si aún no hay buen sonido, usar el olor */
			smelled_scent = mon->race->smell
				- cave->scent.grids[grid.y][grid.x];
			if ((smelled_scent > best_scent) &&
				(cave->scent.grids[grid.y][grid.x] != 0)) {
				best_scent = smelled_scent;
				best_grid = grid;
				found = true;
			}
		}
	}

	/* Establecer el objetivo */
	if (found) {
		mon->target.grid = best_grid;
		*track = true;
		return true;
	} else if (found_backup) {
		/* Moverse para intentar mejorar la posición */
		mon->target.grid = backup_grid;
		*track = true;
		return true;
	}

	/* Sin razón para avanzar */
	return false;
}

/**
 * Elegir una casilla adyacente transitable aleatoria cerca del monstruo ya que no tiene mejor
 * estrategia.
 */
static struct loc get_move_random(struct monster *mon)
{
	int attempts[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	int nleft = 8;

	while (nleft > 0) {
		int itry = randint0(nleft);
		struct loc trygrid;

		trygrid = loc_sum(mon->grid, ddgrid_ddd[attempts[itry]]);
		if (square_is_monster_walkable(cave, trygrid) &&
				!monster_hates_grid(mon, trygrid)) {
			return ddgrid_ddd[attempts[itry]];
		} else {
			int tmp = attempts[itry];

			--nleft;
			attempts[itry] = attempts[nleft];
			attempts[nleft] = tmp;
		}
	}

	return loc(0, 0);
}

/**
 * Elegir una ubicación "segura" cerca de un monstruo para que huya hacia ella.
 *
 * Una ubicación es "segura" si puede ser alcanzada rápidamente y el jugador
 * no puede disparar hacia ella (no es un "tiro limpio"). Así que esto hará
 * que los monstruos se "agachen" detrás de las paredes. Con suerte, los monstruos también
 * intentarán correr hacia las aberturas de los pasillos si están en una habitación.
 *
 * Esta función puede consumir mucho tiempo de CPU si muchos monstruos están huyendo.
 *
 * Devolver true si hay una ubicación segura disponible.
 */
static bool get_move_find_safety(struct monster *mon)
{
	int i, dy, dx, d, dis, gdis = 0;

	const int *y_offsets;
	const int *x_offsets;

	/* Comenzar con ubicaciones adyacentes, extenderse más */
	for (d = 1; d < 10; d++) {
		struct loc best = loc(0, 0);

		/* Obtener las listas de puntos con una distancia d desde (fx, fy) */
		y_offsets = dist_offsets_y[d];
		x_offsets = dist_offsets_x[d];

		/* Verificar las ubicaciones */
		for (i = 0, dx = x_offsets[0], dy = y_offsets[0];
		     dx != 0 || dy != 0;
		     i++, dx = x_offsets[i], dy = y_offsets[i]) {
			struct loc grid = loc_sum(mon->grid, loc(dx, dy));

			/* Saltar ubicaciones ilegales */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Saltar ubicaciones en una pared */
			if (!square_ispassable(cave, grid)) continue;

			/* Ignorar casillas demasiado distantes */
			if (cave->noise.grids[grid.y][grid.x] >
				cave->noise.grids[mon->grid.y][mon->grid.x] + 2 * d)
				continue;

			/* Ignorar terreno dañino si no pueden manejarlo */
			if (monster_hates_grid(mon, grid)) continue;

			/* Verificar ausencia de tiro (más o menos) */
			if (!square_isview(cave, grid)) {
				/* Calcular distancia desde el jugador */
				dis = distance(grid, player->grid);

				/* Recordar si más lejos que el anterior */
				if (dis > gdis) {
					best = grid;
					gdis = dis;
				}
			}
		}

		/* Verificar éxito */
		if (gdis > 0) {
			/* Buena ubicación */
			mon->target.grid = best;
			return (true);
		}
	}

	/* Sin lugar seguro */
	return (false);
}

/**
 * Elegir un buen escondite cerca de un monstruo para que huya hacia él.
 *
 * Los monstruos en manada usarán esto para "emboscar" al jugador y atraerlo
 * fuera de los pasillos hacia el espacio abierto para que puedan acosarlo.
 *
 * Devolver true si hay una buena ubicación disponible.
 */
static bool get_move_find_hiding(struct monster *mon)
{
	int i, dy, dx, d, dis, gdis = 999, min;

	const int *y_offsets, *x_offsets;

	/* Distancia más cercana a alcanzar */
	min = distance(player->grid, mon->grid) * 3 / 4 + 2;

	/* Comenzar con ubicaciones adyacentes, extenderse más */
	for (d = 1; d < 10; d++) {
		struct loc best = loc(0, 0);

		/* Obtener las listas de puntos con una distancia d desde el monstruo */
		y_offsets = dist_offsets_y[d];
		x_offsets = dist_offsets_x[d];

		/* Verificar las ubicaciones */
		for (i = 0, dx = x_offsets[0], dy = y_offsets[0];
		     dx != 0 || dy != 0;
		     i++, dx = x_offsets[i], dy = y_offsets[i]) {
			struct loc grid = loc_sum(mon->grid, loc(dx, dy));

			/* Saltar ubicaciones ilegales */
			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Saltar ubicaciones ocupadas */
			if (!square_isempty(cave, grid)) continue;

			/* Verificar si hay una casilla oculta y disponible */
			if (!square_isview(cave, grid) &&
				projectable(cave, mon->grid, grid, PROJECT_STOP)) {
				/* Calcular distancia desde el jugador */
				dis = distance(grid, player->grid);

				/* Recordar si más cerca que el anterior */
				if (dis < gdis && dis >= min) {
					best = grid;
					gdis = dis;
				}
			}
		}

		/* Verificar éxito */
		if (gdis < 999) {
			/* Buena ubicación */
			mon->target.grid = best;
			return (true);
		}
	}

	/* Sin buen lugar */
	return (false);
}

/**
 * Proporcionar una ubicación para huir, pero darle un amplio margen al jugador.
 *
 * Un monstruo puede desear huir a una ubicación que está detrás del jugador,
 * pero en lugar de dirigirse directamente hacia ella, el monstruo debería "desviarse"
 * alrededor del jugador para que tenga menos probabilidades de ser golpeado.
 */
static bool get_move_flee(struct monster *mon)
{
	int i;
	struct loc best = loc(0, 0);
	int best_score = -1;

	/* Recibir daño del terreno hace que moverse sea vital */
	if (!monster_taking_terrain_damage(cave, mon)) {
		/* Si el jugador no está actualmente cerca del monstruo, no hay razón para fluir */
		if (mon->cdis >= mon->best_range) {
			return false;
		}

		/* El monstruo está demasiado lejos para usar el sonido o el olor */
		if (!monster_can_hear(mon) && !monster_can_smell(mon)) {
			return false;
		}
	}

	/* Verificar casillas cercanas, diagonales primero */
	for (i = 7; i >= 0; i--) {
		int dis, score;

		/* Obtener la ubicación */
		struct loc grid = loc_sum(mon->grid, ddgrid_ddd[i]);

		/* Verificar límites */
		if (!square_in_bounds(cave, grid)) continue;

		/* Calcular distancia de esta casilla desde nuestro objetivo */
		dis = distance(grid, mon->target.grid);

		/* Puntuar esta casilla
		 * La primera mitad del cálculo es inversamente proporcional a la distancia
		 * La segunda mitad es inversamente proporcional a la distancia de la casilla al jugador
		 */
		score = 5000 / (dis + 3) - 500 /(cave->noise.grids[grid.y][grid.x] + 1);

		/* Sin puntuaciones negativas */
		if (score < 0) score = 0;

		/* Ignorar puntuaciones más bajas */
		if (score < best_score) continue;

		/* Guardar la puntuación */
		best_score = score;

		/* Guardar la ubicación */
		best = grid;
	}

	/* Establecer el objetivo inmediato */
	mon->target.grid = best;

	/* Éxito */
	return true;
}

/**
 * Elegir la dirección básica de movimiento, y si inclinarse a la izquierda o derecha
 * si la dirección principal está bloqueada.
 *
 * Nótese que la entrada es un desplazamiento de la posición actual del monstruo, y
 * la dirección de salida está pensada como un índice en la matriz side_dirs.
 */
static int get_move_choose_direction(struct loc offset)
{
	int dir = 0;
	int dx = offset.x, dy = offset.y;

	/* Extraer las "distancias absolutas" */
	int ay = ABS(dy);
	int ax = ABS(dx);

	/* Principalmente queremos movernos verticalmente */
	if (ay > (ax * 2)) {
		/* Elegir entre las direcciones '8' y '2' */
		if (dy > 0) {
			/* Nos dirigimos hacia abajo */
			dir = 2;
			if ((dx > 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* Nos dirigimos hacia arriba */
			dir = 8;
			if ((dx < 0) || (dx == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* Principalmente queremos movernos horizontalmente */
	else if (ax > (ay * 2)) {
		/* Elegir entre las direcciones '4' y '6' */
		if (dx > 0) {
			/* Nos dirigimos hacia la derecha */
			dir = 6;
			if ((dy < 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		} else {
			/* Nos dirigimos hacia la izquierda */
			dir = 4;
			if ((dy > 0) || (dy == 0 && turn % 2 == 0))
				dir += 10;
		}
	}

	/* Queremos movernos hacia abajo y lateralmente */
	else if (dy > 0) {
		/* Elegir entre las direcciones '1' y '3' */
		if (dx > 0) {
			/* Nos dirigimos hacia abajo y derecha */
			dir = 3;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* Nos dirigimos hacia abajo e izquierda */
			dir = 1;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	/* Queremos movernos hacia arriba y lateralmente */
	else {
		/* Elegir entre las direcciones '7' y '9' */
		if (dx > 0) {
			/* Nos dirigimos hacia arriba y derecha */
			dir = 9;
			if ((ay > ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		} else {
			/* Nos dirigimos hacia arriba e izquierda */
			dir = 7;
			if ((ay < ax) || (ay == ax && turn % 2 == 0))
				dir += 10;
		}
	}

	return dir;
}

/**
 * Elegir direcciones "lógicas" para el movimiento del monstruo
 *
 * Esta función es responsable de decidir hacia dónde quiere moverse el monstruo,
 * y por lo tanto es el núcleo de la "IA" del monstruo.
 *
 * Primero, calculamos la mejor manera de avanzar hacia el jugador:
 * - Intentar dirigirse hacia el jugador directamente si podemos atravesar paredes o
 *   si podemos verlos
 * - Si eso falla, seguir al jugador por el sonido, o si eso falla, por el olor
 * - Si nada de eso funciona, simplemente ir en la dirección general
 * Luego observamos posibles razones para no solo avanzar:
 * - Si somos parte de una manada, intentar atraer al jugador al espacio abierto
 * - Si tenemos miedo, intentar encontrar un lugar seguro para huir, y si no hay lugar seguro
 *   simplemente correr en la dirección opuesta al movimiento de avance
 * - Si podemos ver al jugador y somos parte de un grupo, intentar rodearlos
 *
 * La función luego devuelve false si ya estamos donde queremos estar, y
 * de lo contrario establece la dirección elegida para dar un paso y devuelve true.
 */
static bool get_move(struct monster *mon, int *dir, bool *good)
{
	struct loc target = monster_is_decoyed(mon) ? cave_find_decoy(cave) :
		player->grid;
	bool group_ai = rf_has(mon->race->flags, RF_GROUP_AI);

	/* Desplazamiento a la posición actual para moverse hacia */
	struct loc grid = loc(0, 0);

	/* Los monstruos huirán hasta z_info->flee_range casillas fuera de la vista */
	int flee_range = z_info->max_sight + z_info->flee_range;

	bool done = false;

	/* Calcular rango */
	get_move_find_range(mon);

	/* Asumir que nos dirigimos hacia el jugador */
	if (get_move_advance(mon, good)) {
		/* Tenemos un buen movimiento, usarlo */
		grid = loc_diff(mon->target.grid, mon->grid);
		mflag_on(mon->mflag, MFLAG_TRACKING);
	} else {
		/* Intentar seguir a alguien que sabe a dónde va */
		struct monster *tracker = group_monster_tracking(cave, mon);
		if (tracker && los(cave, mon->grid, tracker->grid)) { /* ¿Necesita LdV? */
			grid = loc_diff(tracker->grid, mon->grid);
			/* Ya no está rastreando */
			mflag_off(mon->mflag, MFLAG_TRACKING);
		} else {
			if (mflag_has(mon->mflag, MFLAG_TRACKING)) {
				/* Seguir dirigiéndose al objetivo más reciente. */
				grid = loc_diff(mon->target.grid, mon->grid);
			}
			if (loc_is_zero(grid)) {
				/* Intentar un movimiento aleatorio y ya no rastrear. */
				grid = get_move_random(mon);
				mflag_off(mon->mflag, MFLAG_TRACKING);
			}
		}
	}

	/* El monstruo está recibiendo daño del terreno */
	if (monster_taking_terrain_damage(cave, mon)) {
		/* Intentar encontrar un lugar seguro */
		if (get_move_find_safety(mon)) {
			/* Establecer un rumbo hacia el lugar seguro */
			get_move_flee(mon);
			grid = loc_diff(mon->target.grid, mon->grid);
			done = true;
		}
	}

	/* Las manadas de animales normales intentan sacar al jugador de los pasillos. */
	if (!done && group_ai && !monster_passes_walls(mon)) {
		int i, open = 0;

		/* Contar casillas vacías junto al jugador */
		for (i = 0; i < 8; i++) {
			/* Verificar casilla alrededor del jugador para interior de habitación (las paredes de habitación cuentan)
			 * u otro espacio vacío */
			struct loc test = loc_sum(target, ddgrid_ddd[i]);
			if (square_ispassable(cave, test) || square_isroom(cave, test)) {
				/* Una casilla abierta más */
				open++;
			}
		}

		/* No en un espacio vacío y jugador fuerte */
		if ((open < 5) && (player->chp > player->mhp / 2)) {
			/* Encontrar escondite para una emboscada */
			if (get_move_find_hiding(mon)) {
				done = true;
				grid = loc_diff(mon->target.grid, mon->grid);

				/* Ya no está rastreando */
				mflag_off(mon->mflag, MFLAG_TRACKING);
			}
		}
	}

	/* No escondiéndose y el monstruo tiene miedo */
	if (!done && (mon->min_range == flee_range)) {
		/* Intentar encontrar un lugar seguro */
		if (get_move_find_safety(mon)) {
			/* Establecer un rumbo hacia el lugar seguro */
			get_move_flee(mon);
			grid = loc_diff(mon->target.grid, mon->grid);
		} else {
			/* Simplemente huir del jugador */
			grid = loc_diff(loc(0, 0), grid);
		}

		/* Ya no está rastreando */
		mflag_off(mon->mflag, MFLAG_TRACKING);
		done = true;
	}

	/* Los grupos de monstruos intentan rodear al jugador si están a la vista */
	if (!done && group_ai && square_isview(cave, mon->grid)) {
		int i;
		struct loc grid1 = mon->target.grid;

		/* Si aún no estamos adyacentes */
		if (mon->cdis > 1) {
			/* Encontrar una casilla vacía cerca del jugador para llenar */
			int tmp = randint0(8);
			for (i = 0; i < 8; i++) {
				/* Elegir casillas cerca del jugador (pseudoaleatoriamente) */
				grid1 = loc_sum(target, ddgrid_ddd[(tmp + i) % 8]);

				/* Ignorar casillas ocupadas */
				if (!square_isempty(cave, grid1)) continue;

				/* Intentar llenar este hueco */
				break;
			}
		}

		/* Dirigirse en la dirección de la casilla elegida */
		grid = loc_diff(grid1, mon->grid);
	}

	/* Verificar si el monstruo ya ha alcanzado su objetivo */
	if (loc_is_zero(grid)) return (false);

	/* Elegir la dirección correcta */
	*dir = get_move_choose_direction(grid);

	/* Quiere moverse */
	return (true);
}


/**
 * ------------------------------------------------------------------------
 * Rutinas de turno de monstruo
 * Estas rutinas, que culminan en monster_turn(), deciden cómo un monstruo
 * usa su turno
 * ------------------------------------------------------------------------ */
/**
 * Permite que el monstruo dado intente reproducirse.
 *
 * Nótese que la "reproducción" REQUIERE espacio vacío.
 *
 * Devuelve true si el monstruo se reprodujo con éxito.
 */
bool multiply_monster(const struct monster *mon)
{
	struct loc grid;
	bool result;
	struct monster_group_info info = { 0, 0 };

	/*
	 * Elegir una ubicación vacía excepto para únicos: nunca pueden
	 * multiplicarse (necesidad de verificación aquí ya que las de place_new_monster()
	 * no son suficientes para una forma única de un monstruo con forma cambiada
	 * ya que puede tener cero para cur_num en la estructura de raza para la
	 * forma).
	 */
	if (!monster_is_shape_unique(mon) && scatter_ext(cave, &grid,
			1, mon->grid, 1, true, square_isempty) > 0) {
		/* Crear un nuevo monstruo (despierto, sin grupos) */
		result = place_new_monster(cave, grid, mon->race, false, false,
			info, ORIGIN_DROP_BREED);
		/*
		 * Arreglar para que multiplicar un monstruo camuflado revelado cree
		 * otro monstruo camuflado revelado.
		 */
		if (result) {
			struct monster *child = square_monster(cave, grid);

			if (child && monster_is_camouflaged(child)
					&& !monster_is_camouflaged(mon)) {
				become_aware(cave, child);
			}
		}
	} else {
		result = false;
	}

	/* Resultado */
	return (result);
}

/**
 * Intentar reproducirse, si es posible. Todos los monstruos se verifican aquí para
 * propósitos de saber, los no aptos fallan.
 */
static bool monster_turn_multiply(struct monster *mon)
{
	int k = 0, y, x;

	struct monster_lore *lore = get_lore(mon->race);

	/* Demasiados reproductores en el nivel ya */
	if (cave->num_repro >= z_info->repro_monster_max) return false;

	/* Sin reproducción en combate singular */
	if (player->upkeep->arena_level) return false;  

	/* Contar los monstruos adyacentes */
	for (y = mon->grid.y - 1; y <= mon->grid.y + 1; y++)
		for (x = mon->grid.x - 1; x <= mon->grid.x + 1; x++)
			if (square(cave, loc(x, y))->mon > 0) k++;

	/* Multiplicar más lento en áreas concurridas */
	if ((k < 4) && (k == 0 || one_in_(k * z_info->repro_monster_rate))) {
		/* Intento de reproducción exitoso, aprender sobre eso ahora */
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_MULTIPLY);

		/* Salir ahora si no es un reproductor */
		if (!rf_has(mon->race->flags, RF_MULTIPLY))
			return false;

		/* Intentar multiplicarse */
		if (multiply_monster(mon)) {
			/* Hacer un sonido */
			if (monster_is_visible(mon))
				sound(MSG_MULTIPLY);

			/* Multiplicarse consume energía */
			return true;
		}
	}

	return false;
}

/**
 * Verificar si un monstruo debería tambalearse (es decir, dar un paso al azar) o no.
 * Siempre tambalearse cuando está confundido, pero también lidiar con movimiento
 * aleatorio para monstruos RAND_25 y RAND_50.
 */
static enum monster_stagger monster_turn_should_stagger(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int chance = 0, confused_chance, roll;

	/* Aumentar la probabilidad de ser errático por cada nivel de confusión */
	int conf_level = monster_effect_level(mon, MON_TMD_CONF);
	while (conf_level) {
		int accuracy = 100 - chance;
		accuracy *= (100 - CONF_ERRATIC_CHANCE);
		accuracy /= 100;
		chance = 100 - accuracy;
		conf_level--;
	}
	confused_chance = chance;

	/* RAND_25 y RAND_50 son acumulativos */
	if (rf_has(mon->race->flags, RF_RAND_25)) {
		chance += 25;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_25);
	}

	if (rf_has(mon->race->flags, RF_RAND_50)) {
		chance += 50;
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_RAND_50);
	}

	roll = randint0(100);
	return (roll < confused_chance) ?
		 CONFUSED_STAGGER :
		 ((roll < chance) ? INNATE_STAGGER : NO_STAGGER);
}


/**
 * Función auxiliar para monster_turn_can_move() para mostrar un mensaje para un
 * movimiento confundido hacia terreno no transitable.
 */
static void monster_display_confused_move_msg(struct monster *mon,
											  const char *m_name,
											  struct loc new)
{
	if (monster_is_visible(mon) && monster_is_in_view(mon)) {
		const char *m = square_feat(cave, new)->confused_msg;

		msg("%s %s.", m_name, (m) ? m : "tropieza");
	}
}


/**
 * Función auxiliar para monster_turn_can_move() para aturdir ligeramente a un monstruo
 * ocasionalmente debido a chocar con algo.
 */
static void monster_slightly_stun_by_move(struct monster *mon)
{
	if (mon->m_timed[MON_TMD_STUN] < 5 && one_in_(3)) {
		mon_inc_timed(mon, MON_TMD_STUN, 3, 0);
	}
}


/**
 * Determinar si un monstruo puede moverse a través de la casilla, si es necesario derribando
 * puertas en el camino.
 *
 * Devuelve true si el monstruo puede moverse a través de la casilla.
 */
static bool monster_turn_can_move(struct monster *mon, const char *m_name,
								  struct loc new, bool confused,
								  bool *did_something)
{
	struct monster_lore *lore = get_lore(mon->race);

	/* Siempre permitir un ataque al jugador o señuelo. */
	if (square_isplayer(cave, new) || square_isdecoyed(cave, new)) {
		return true;
	}

	/* Terreno peligroso en el camino */
	if (!confused && monster_hates_grid(mon, new)) {
		return false;
	}

	/* ¿El suelo está despejado? */
	if (square_ispassable(cave, new)) {
		return true;
	}

	/* Pared permanente en el camino */
	if (square_isperm(cave, new)) {
		if (confused) {
			*did_something = true;
			monster_display_confused_move_msg(mon, m_name, new);
			monster_slightly_stun_by_move(mon);
		}
		return false;
	}

	/* Pared normal, puerta o puerta secreta en el camino */

	/* Hay algún tipo de característica en el camino, así que aprender sobre
	 * kill-wall y pass-wall ahora */
	if (monster_is_visible(mon)) {
		rf_on(lore->flags, RF_PASS_WALL);
		rf_on(lore->flags, RF_KILL_WALL);
		rf_on(lore->flags, RF_SMASH_WALL);
	}

	/* El monstruo puede ser capaz de lidiar con paredes y puertas */
	if (rf_has(mon->race->flags, RF_PASS_WALL)) {
		return true;
	} else if (rf_has(mon->race->flags, RF_SMASH_WALL)) {
		/* Eliminar la pared y gran parte de lo que está cerca */
		square_smash_wall(cave, new);

		/* Notar cambios en la región visible */
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return true;
	} else if (rf_has(mon->race->flags, RF_KILL_WALL)) {
		/* Eliminar la pared */
		square_destroy_wall(cave, new);

		/* Notar cambios en la región visible */
		if (square_isview(cave, new))
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return true;
	} else if (square_iscloseddoor(cave, new)|| square_issecretdoor(cave, new)){
		/* No permitir que un movimiento confundido abra una puerta. */
		bool can_open = rf_has(mon->race->flags, RF_OPEN_DOOR) &&
			!confused;
		/* Durante un movimiento confundido, un monstruo solo derriba a veces. */
		bool can_bash = rf_has(mon->race->flags, RF_BASH_DOOR) &&
			(!confused || one_in_(3));
		bool will_bash = false;

		/* Gastar un turno */
		if (can_open || can_bash) *did_something = true;

		/* Aprender sobre habilidades con puertas */
		if (!confused && monster_is_visible(mon)) {
			rf_on(lore->flags, RF_OPEN_DOOR);
			rf_on(lore->flags, RF_BASH_DOOR);
		}

		/* Si la criatura puede abrir o derribar puertas, tomar una decisión */
		if (can_open) {
			/* A veces derribar de todos modos (impaciente) */
			if (can_bash) {
				will_bash = one_in_(2) ? true : false;
			}
		} else if (can_bash) {
			/* Única opción */
			will_bash = true;
		} else {
			/* La puerta es un obstáculo insuperable */
			if (confused) {
				*did_something = true;
				monster_display_confused_move_msg(mon, m_name, new);
				monster_slightly_stun_by_move(mon);
			}
			return false;
		}

		/* Ahora el resultado depende del tipo de puerta */
		if (square_islockeddoor(cave, new)) {
			/* Puerta cerrada con llave -- probar fuerza del monstruo contra fuerza de la puerta */
			int k = square_door_power(cave, new);
			if (randint0(mon->hp / 10) > k) {
				if (will_bash) {
					msg("%s se estrella contra la puerta.", m_name);
				} else {
					msg("%s manipula la cerradura.", m_name);
				}

				/* Reducir la fuerza de la puerta en uno */
				square_set_door_lock(cave, new, k - 1);
			}
			if (confused) {
				/* No aprendió arriba; aplicar ahora ya que intentó derribar. */
				if (monster_is_visible(mon)) {
					rf_on(lore->flags, RF_BASH_DOOR);
				}
				/* Cuando está confundido, puede aturdirse mientras derriba. */
				monster_slightly_stun_by_move(mon);
			}
		} else {
			/* Puerta cerrada o secreta -- siempre abrir o derribar */
			if (square_isview(cave, new))
				player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			if (will_bash) {
				square_smash_door(cave, new);

				msg("¡Escuchas una puerta abrirse de golpe!");
				disturb(player);

				if (confused) {
					/* No aprendió arriba; aplicar ya que derribó la puerta. */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_BASH_DOOR);
					}
					/* Cuando está confundido, puede aturdirse mientras derriba. */
					monster_slightly_stun_by_move(mon);
				}

				/* Caer en la puerta */
				return true;
			} else {
				square_open_door(cave, new);
			}
		}
	} else if (confused) {
		*did_something = true;
		monster_display_confused_move_msg(mon, m_name, new);
		monster_slightly_stun_by_move(mon);
	}

	return false;
}

/**
 * Intentar romper un glifo.
 */
static bool monster_turn_attack_glyph(struct monster *mon, struct loc new)
{
	assert(square_iswarded(cave, new));

	/* Romper la protección */
	if (randint1(z_info->glyph_hardness) < mon->race->level) {
		struct trap_kind *rune = lookup_trap("glyph of warding");

		/* Describir rotura observable */
		if (square_isseen(cave, new)) {
			msg("¡La runa de protección está rota!");
		}

		/* Romper la runa */
		assert(rune);
		square_remove_all_traps_of_type(cave, new, rune->tidx);

		return true;
	}

	/* Protección no rota - no puede moverse */
	return false;
}

/**
 * Intentar empujar / matar a otro monstruo. Devuelve true en caso de éxito.
 */
static bool monster_turn_try_push(struct monster *mon, const char *m_name,
								  struct loc new)
{
	struct monster *mon1 = square_monster(cave, new);
	struct monster_lore *lore = get_lore(mon->race);

	/* Matar monstruos más débiles */
	int kill_ok = monster_can_kill(mon, new);

	/* Mover monstruos más débiles si pueden intercambiar lugares */
	/* (no en una pared) */
	int move_ok = (monster_can_move(mon, new) &&
				   square_ispassable(cave, mon->grid));

	if (kill_ok || move_ok) {
		/* Obtener los nombres de los monstruos involucrados */
		char n_name[80];
		monster_desc(n_name, sizeof(n_name), mon1, MDESC_IND_HID);

		/* Aprender sobre empujar y dar empujones */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_KILL_BODY);
			rf_on(lore->flags, RF_MOVE_BODY);
		}

		/* Revelar monstruos camuflados */
		if (monster_is_camouflaged(mon1))
			become_aware(cave, mon1);

		/* Notar si es visible */
		if (monster_is_visible(mon) && monster_is_in_view(mon))
			msg("%s %s %s.", m_name, kill_ok ? "pisotea" : "empuja a",
				n_name);

		/* El monstruo se comió a otro monstruo */
		if (kill_ok)
			delete_monster(cave, new);

		monster_swap(mon->grid, new);
		return true;
	}

	return false;
}

/**
 * Agarrar todos los objetos de la casilla.
 */
static void monster_turn_grab_objects(struct monster *mon, const char *m_name,
									  struct loc new)
{
	struct monster_lore *lore = get_lore(mon->race);
	struct object *obj;
	bool visible = monster_is_visible(mon);

	/* Aprender sobre comportamiento de recogida de objetos */
	for (obj = square_object(cave, new); obj; obj = obj->next) {
		if (!tval_is_money(obj) && visible) {
			rf_on(lore->flags, RF_TAKE_ITEM);
			rf_on(lore->flags, RF_KILL_ITEM);
			break;
		}
	}

	/* Abortar si no puede recoger/matar */
	if (!rf_has(mon->race->flags, RF_TAKE_ITEM) &&
		!rf_has(mon->race->flags, RF_KILL_ITEM)) {
		return;
	}

	/* Tomar o matar objetos en el suelo */
	obj = square_object(cave, new);
	while (obj) {
		char o_name[80];
		bool safe = obj->artifact ? true : false;
		struct object *next = obj->next;

		/* Saltar oro */
		if (tval_is_money(obj)) {
			obj = next;
			continue;
		}

		/* Saltar objetos imitados */
		if (obj->mimicking_m_idx) {
			obj = next;
			continue;
		}

		/* Obtener el nombre del objeto */
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL, player);

		/* Reaccionar a objetos que dañan al monstruo */
		if (react_to_slay(obj, mon))
			safe = true;

		/* Intentar recoger, o aplastar */
		if (safe) {
			/* Solo dar un mensaje para "take_item" */
			if (rf_has(mon->race->flags, RF_TAKE_ITEM)
					&& visible
					&& square_isview(cave, new)
					&& !ignore_item_ok(player, obj)) {
				/* Volcar un mensaje */
				msg("%s intenta recoger %s, pero falla.", m_name, o_name);
			}
		} else if (rf_has(mon->race->flags, RF_TAKE_ITEM)) {
			/*
			 * Hacer una copia para que el original pueda permanecer como
			 * marcador de posición si el jugador recuerda haber visto el
			 * objeto.
			 */
			struct object *taken = object_new();

			object_copy(taken, obj);
			taken->oidx = 0;
			if (obj->known) {
				taken->known = object_new();
				object_copy(taken->known, obj->known);
				taken->known->oidx = 0;
				taken->known->grid = loc(0, 0);
			}

			/* Intentar llevar la copia */
			if (monster_carry(cave, mon, taken)) {
				/* Describir situaciones observables */
				if (square_isseen(cave, new) && !ignore_item_ok(player, obj)) {
					msg("%s recoge %s.", m_name, o_name);
				}

				/* Eliminar el objeto */
				square_delete_object(cave, new, obj, true, true);
			} else {
				if (taken->known) {
					object_delete(player->cave, NULL, &taken->known);
				}
				object_delete(cave, player->cave, &taken);
			}
		} else {
			/* Describir situaciones observables */
			if (square_isseen(cave, new) && !ignore_item_ok(player, obj)) {
				msgt(MSG_DESTROY, "%s aplasta %s.", m_name, o_name);
			}

			/* Eliminar el objeto */
			square_delete_object(cave, new, obj, true, true);
		}

		/* Siguiente objeto */
		obj = next;
	}
}


/**
 * Procesar el turno de un monstruo
 *
 * En varios casos, actualizamos directamente el saber del monstruo
 *
 * Nótese que a un monstruo solo se le permite "reproducirse" si hay
 * un número limitado de monstruos "reproductores" en el nivel
 * actual. Esto debería evitar que el nivel sea "invadido" por
 * monstruos reproductores. También permite que una gran masa de ratones
 * evite que un piojo se multiplique, pero este es un pequeño precio a
 * pagar por un método de multiplicación simple.
 *
 * XXX El miedo del monstruo es ligeramente extraño, en particular, los monstruos
 * se fijarán en abrir una puerta incluso si no pueden abrirla. En realidad,
 * lo mismo le sucede a los monstruos normales cuando golpean una puerta
 *
 * Además, los monstruos que *no pueden* abrir o derribar una puerta
 * seguirán parados allí intentando abrirla... XXX XXX XXX
 *
 * Técnicamente, necesitan verificar si hay un monstruo en el camino combinado
 * con que ese monstruo esté en una pared (¿o puerta?) XXX
 */
static void monster_turn(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);

	bool did_something = false;

	int i;
	int dir = 0;
	enum monster_stagger stagger;
	bool tracking = false;
	char m_name[80];

	/* Obtener el nombre del monstruo */
	monster_desc(m_name, sizeof(m_name), mon,
		MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, mon->grid)) {
		/* Aprender comportamiento de telarañas */
		if (monster_is_visible(mon)) {
			rf_on(lore->flags, RF_CLEAR_WEB);
			rf_on(lore->flags, RF_PASS_WEB);
		}

		/* Si podemos pasar, no es necesario limpiar */
		if (!rf_has(mon->race->flags, RF_PASS_WEB)) {
			/* Aprender comportamiento de paredes */
			if (monster_is_visible(mon)) {
				rf_on(lore->flags, RF_PASS_WALL);
				rf_on(lore->flags, RF_KILL_WALL);
			}

			/* Ahora varias posibilidades */
			if (rf_has(mon->race->flags, RF_PASS_WALL)) {
				/* Los monstruos insustanciales atraviesan directamente */
			} else if (monster_passes_walls(mon)) {
				/* Si puedes destruir una pared, puedes destruir una telaraña */
				struct trap_kind *web = lookup_trap("web");

				assert(web);
				square_remove_all_traps_of_type(cave,
					mon->grid, web->tidx);
			} else if (rf_has(mon->race->flags, RF_CLEAR_WEB)) {
				/* Limpiar cuesta un turno (asumir que no hay otras "trampas") */
				struct trap_kind *web = lookup_trap("web");

				assert(web);
				square_remove_all_traps_of_type(cave,
					mon->grid, web->tidx);
				return;
			} else {
				/* Atascado */
				return;
			}
		}
	}

	/* Informar a otros monstruos del grupo sobre el jugador */
	monster_group_rouse(cave, mon);

	/* Intentar multiplicarse - esto puede consumir un turno */
	if (monster_turn_multiply(mon))
		return;

	/* Intentar un ataque a distancia */
	if (make_ranged_attack(mon)) return;

	/* Determinar qué tipo de movimiento usar - movimiento aleatorio o IA */
	stagger = monster_turn_should_stagger(mon);
	if (stagger == NO_STAGGER) {
		/* Si no hay movimiento sensato, hemos terminado */
		if (!get_move(mon, &dir, &tracking)) return;
	}

	/* Intentar moverse primero en la dirección elegida, o luego a cada lado de la
	 * dirección elegida, o luego en ángulo recto con la dirección elegida.
	 * Los monstruos que rastrean por sonido u olor no se moverán si no
	 * pueden moverse en su dirección elegida. */
	for (i = 0; i < 5 && !did_something; i++) {
		/* Obtener la dirección (o tambaleo) */
		int d = (stagger != NO_STAGGER) ? ddd[randint0(8)] : side_dirs[dir][i];

		/* Obtener la casilla a la que dar un paso o atacar */
		struct loc new = loc_sum(mon->grid, ddgrid[d]);

		/* Los monstruos que rastrean tienen su mejor dirección, no cambiar */
		if ((i > 0) && stagger == NO_STAGGER &&
			!square_isview(cave, mon->grid) && tracking) {
			break;
		}

		/* Verificar si podemos movernos */
		if (!monster_turn_can_move(mon, m_name, new,
								   stagger == CONFUSED_STAGGER, &did_something))
			continue;

		/* Intentar romper el glifo si lo hay. Esto puede suceder varias veces
		 * por turno porque el fallo no rompe el bucle */
		if (square_iswarded(cave, new) && !monster_turn_attack_glyph(mon, new))
			continue;

		/* Romper un señuelo si lo hay */
		if (square_isdecoyed(cave, new)) {
			/* Aprender sobre si el monstruo ataca */
			if (monster_is_visible(mon))
				rf_on(lore->flags, RF_NEVER_BLOW);

			/* Algunos monstruos nunca atacan */
			if (rf_has(mon->race->flags, RF_NEVER_BLOW))
				continue;

			/* Esperar un minuto... */
			square_destroy_decoy(cave, new);
			did_something = true;
			break;
		}

		/* El jugador está en el camino. */
		if (square_isplayer(cave, new)) {
			/* Aprender sobre si el monstruo ataca */
			if (monster_is_visible(mon))
				rf_on(lore->flags, RF_NEVER_BLOW);

			/* Algunos monstruos nunca atacan */
			if (rf_has(mon->race->flags, RF_NEVER_BLOW))
				continue;

			/* De lo contrario, atacar al jugador */
			make_attack_normal(mon, player);

			did_something = true;
			break;
		} else {
			/* Algunos monstruos nunca se mueven */
			if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
				/* Aprender sobre falta de movimiento */
				if (monster_is_visible(mon))
					rf_on(lore->flags, RF_NEVER_MOVE);

				return;
			}
		}

		/* Un monstruo está en el camino, intentar empujar/matar */
		if (square_monster(cave, new)) {
			did_something = monster_turn_try_push(mon, m_name, new);
		} else {
			/* De lo contrario, podemos simplemente movernos */
			monster_swap(mon->grid, new);
			did_something = true;
		}

		/* Escanear todos los objetos en la casilla, si la alcanzamos */
		if (mon == square_monster(cave, new)) {
			monster_turn_grab_objects(mon, m_name, new);
		}
	}

	if (did_something) {
		/* Aprender sobre no falta de movimiento */
		if (monster_is_visible(mon))
			rf_on(lore->flags, RF_NEVER_MOVE);

		/* Posible molestia */
		if (monster_is_visible(mon) && monster_is_in_view(mon) && 
			OPT(player, disturb_near))
			disturb(player);		
	}

	/* Sin opciones - el monstruo está paralizado por el miedo (a menos que sea atacado) */
	if (!did_something && mon->m_timed[MON_TMD_FEAR]) {
		int amount = mon->m_timed[MON_TMD_FEAR];
		mon_clear_timed(mon, MON_TMD_FEAR, MON_TMD_FLG_NOMESSAGE);
		mon_inc_timed(mon, MON_TMD_HOLD, amount, MON_TMD_FLG_NOTIFY);
	}

	/* Si vemos que un monstruo no consciente hace algo, volverse consciente de él */
	if (did_something && monster_is_camouflaged(mon))
		become_aware(cave, mon);
}


/**
 * ------------------------------------------------------------------------
 * Rutinas de procesamiento que le suceden a un monstruo independientemente de si
 * obtiene un turno, y/o para decidir si obtiene un turno
 * ------------------------------------------------------------------------ */
/**
 * Determinar si un monstruo está activo o pasivo
 */
static bool monster_check_active(struct monster *mon)
{
	if ((mon->cdis <= mon->race->hearing) && monster_passes_walls(mon)) {
		/* El personaje está dentro del rango de escaneo, el monstruo puede ir directamente allí */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (mon->hp < mon->maxhp) {
		/* El monstruo está herido */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (square_isview(cave, mon->grid)) {
		/* El monstruo puede "ver" al jugador (verificado al revés) */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (monster_can_hear(mon)) {
		/* El monstruo puede oír al jugador */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (monster_can_smell(mon)) {
		/* El monstruo puede oler al jugador */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else if (monster_taking_terrain_damage(cave, mon)) {
		/* El monstruo está recibiendo daño del terreno */
		mflag_on(mon->mflag, MFLAG_ACTIVE);
	} else {
		/* De lo contrario, volverse pasivo */
		mflag_off(mon->mflag, MFLAG_ACTIVE);
	}

	return mflag_has(mon->mflag, MFLAG_ACTIVE) ? true : false;
}

/**
 * Despertar a un monstruo o reducir su profundidad de sueño
 *
 * La probabilidad de despertar depende solo del sigilo del jugador, pero la
 * cantidad de reducción de sueño tiene en cuenta la distancia del monstruo al
 * jugador. Actualmente se usa la distancia en línea recta; posiblemente esto
 * debería tener en cuenta la estructura de la mazmorra.
 */
static void monster_reduce_sleep(struct monster *mon)
{
	int stealth = player->state.skills[SKILL_STEALTH];
	uint32_t player_noise = ((uint32_t) 1) << (30 - stealth);
	uint32_t notice = (uint32_t) randint0(1024);
	struct monster_lore *lore = get_lore(mon->race);

	/* Agravación */
	if (player_of_has(player, OF_AGGRAVATE)) {
		char m_name[80];

		/* Despertar al monstruo, hacerlo consciente */
		monster_wake(mon, false, 100);

		/* Obtener el nombre del monstruo */
		monster_desc(m_name, sizeof(m_name), mon,
			MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

		/* Notificar al jugador si está consciente */
		if (monster_is_obvious(mon)) {
			msg("%s se despierta.", m_name);
			equip_learn_flag(player, OF_AGGRAVATE);
		}
	} else if ((notice * notice * notice) <= player_noise) {
		int sleep_reduction = 1;
		int local_noise = cave->noise.grids[mon->grid.y][mon->grid.x];
		bool woke_up = false;

		/* Probar - despertar más rápido en la distancia de oído del jugador
		 * Notar que no hay dependencia del sigilo por ahora */
		if ((local_noise > 0) && (local_noise < 50)) {
			sleep_reduction = (100 / local_noise);
		}

		/* Notar un despertar completo */
		if (mon->m_timed[MON_TMD_SLEEP] <= sleep_reduction) {
			woke_up = true;
		}

		/* El monstruo se despierta un poco */
		mon_dec_timed(mon, MON_TMD_SLEEP, sleep_reduction, MON_TMD_FLG_NOTIFY);

		/* Actualizar conocimiento */
		if (monster_is_obvious(mon)) {
			if (!woke_up && lore->ignore < UCHAR_MAX)
				lore->ignore++;
			else if (woke_up && lore->wake < UCHAR_MAX)
				lore->wake++;
			lore_update(mon->race, lore);
		}
	}
}

/**
 * Procesar los efectos temporales de un monstruo, ej. disminuirlos.
 *
 * Devuelve true si el monstruo está saltándose su turno.
 */
static bool process_monster_timed(struct monster *mon)
{
	/* Si el monstruo está dormido o acaba de despertarse, entonces no actúa */
	if (mon->m_timed[MON_TMD_SLEEP]) {
		monster_reduce_sleep(mon);
		return true;
	} else {
		/* Los monstruos despiertos y activos pueden volverse conscientes */
		if (one_in_(10) && mflag_has(mon->mflag, MFLAG_ACTIVE)) {
			mflag_on(mon->mflag, MFLAG_AWARE);
		}
	}

	if (mon->m_timed[MON_TMD_FAST])
		mon_dec_timed(mon, MON_TMD_FAST, 1, 0);

	if (mon->m_timed[MON_TMD_SLOW])
		mon_dec_timed(mon, MON_TMD_SLOW, 1, 0);

	if (mon->m_timed[MON_TMD_HOLD])
		mon_dec_timed(mon, MON_TMD_HOLD, 1, 0);

	if (mon->m_timed[MON_TMD_DISEN])
		mon_dec_timed(mon, MON_TMD_DISEN, 1, 0);

	if (mon->m_timed[MON_TMD_STUN])
		mon_dec_timed(mon, MON_TMD_STUN, 1, MON_TMD_FLG_NOTIFY);

	if (mon->m_timed[MON_TMD_CONF]) {
		mon_dec_timed(mon, MON_TMD_CONF, 1, MON_TMD_FLG_NOTIFY);
	}

	if (mon->m_timed[MON_TMD_CHANGED]) {
		mon_dec_timed(mon, MON_TMD_CHANGED, 1, MON_TMD_FLG_NOTIFY);
	}

	if (mon->m_timed[MON_TMD_FEAR]) {
		int d = randint1(mon->race->level / 10 + 1);
		mon_dec_timed(mon, MON_TMD_FEAR, d, MON_TMD_FLG_NOTIFY);
	}

	/* Siempre perder el turno si está paralizado o comandado, una de cada STUN_MISS_CHANCE
	 * probabilidad de perderlo si está aturdido */
	if (mon->m_timed[MON_TMD_HOLD] || mon->m_timed[MON_TMD_COMMAND]) {
		return true;
	} else if (mon->m_timed[MON_TMD_STUN]) {
		return one_in_(STUN_MISS_CHANCE);
	} else {
		return false;
	}
}

/**
 * Regeneración de PG del monstruo.
 */
static void regen_monster(struct monster *mon, int num)
{
	/* Regenerar (si es necesario) */
	if (mon->hp < mon->maxhp) {
		/* Regeneración base */
		int frac = mon->maxhp / 100;

		/* Tasa de regeneración mínima */
		if (!frac) frac = 1;

		/* Algunos monstruos se regeneran rápidamente */
		if (rf_has(mon->race->flags, RF_REGENERATE)) frac *= 2;

		/* Multiplicar por número de regeneraciones */
		frac *= num;

		/* Regenerar */
		mon->hp += frac;

		/* No sobre-regenerar */
		if (mon->hp > mon->maxhp) mon->hp = mon->maxhp;

		/* Redibujar (después) si es necesario */
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);
	}
}


/**
 * ------------------------------------------------------------------------
 * Rutinas de procesamiento de monstruos para ser llamadas por el bucle principal del juego
 * ------------------------------------------------------------------------ */
/**
 * Procesar todos los monstruos "vivos", una vez por turno de juego.
 *
 * Durante cada turno de juego, escaneamos la lista de todos los monstruos "vivos",
 * (hacia atrás, para poder eliminar cualquier monstruo "recién muerto"), energizando
 * cada monstruo, y permitiendo que los monstruos completamente energizados se muevan,
 * ataquen, pasen, etc.
 *
 * Esta función y sus hijas son responsables de una fracción considerable
 * del tiempo de procesador en situaciones normales, mayor si el personaje está
 * descansando.
 */
void process_monsters(int minimum_energy)
{
	int i;
	int mspeed;

	/* Solo procesar algunas cosas de vez en cuando */
	bool regen = false;

	/* Regenerar puntos de golpe y maná cada 100 turnos de juego */
	if (turn % 100 == 0)
		regen = true;

	/* Procesar los monstruos (hacia atrás) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		struct monster *mon;
		bool moving;

		/* Manejar "salir" */
		if (player->is_dead || player->upkeep->generate_level) break;

		/* Obtener un monstruo 'vivo' */
		mon = cave_monster(cave, i);
		if (!mon->race) continue;

		/* Ignorar monstruos que ya han sido manejados */
		if (mflag_has(mon->mflag, MFLAG_HANDLED))
			continue;

		/* No tiene suficiente energía para moverse todavía */
		if (mon->energy < minimum_energy) continue;

		/* ¿Tiene este monstruo suficiente energía para moverse? */
		moving = mon->energy >= z_info->move_energy ? true : false;

		/* Prevenir reprocesamiento */
		mflag_on(mon->mflag, MFLAG_HANDLED);

		/* Manejar la regeneración del monstruo si se solicita */
		if (regen)
			regen_monster(mon, 1);

		/* Calcular la velocidad neta */
		mspeed = mon->mspeed;
		if (mon->m_timed[MON_TMD_FAST])
			mspeed += 10;
		if (mon->m_timed[MON_TMD_SLOW]) {
			int slow_level = monster_effect_level(mon, MON_TMD_SLOW);
			mspeed -= (2 * slow_level);
		}

		/* Dar algo de energía a este monstruo */
		mon->energy += turn_energy(mspeed);

		/* Terminar el turno de monstruos sin suficiente energía para moverse */
		if (!moving)
			continue;

		/* Usar "algo" de energía */
		mon->energy -= z_info->move_energy;

		/* Los imitadores esperan al acecho */
		if (monster_is_mimicking(mon)) continue;

		/* Verificar si el monstruo está activo */
		if (monster_check_active(mon)) {
			/* Procesar efectos temporales - saltar turno si es necesario */
			if (process_monster_timed(mon))
				continue;

			/* Establecer este monstruo como el actor actual */
			cave->mon_current = i;

			/* El monstruo toma su turno */
			monster_turn(mon);

			/*
			 * Por simetría con el jugador, el monstruo puede recibir
			 * daño del terreno después de su turno.
			 */
			monster_take_terrain_damage(mon);

			/* El monstruo ya no es el actual */
			cave->mon_current = -1;
		}
	}

	/* Actualizar la visibilidad de los monstruos después de esto */
	/* XXX Esto puede no ser necesario */
	player->upkeep->update |= PU_MONSTERS;
}

/**
 * Limpiar el estado 'movido' de todos los monstruos.
 *
 * Limpiar el ruido si corresponde.
 */
void reset_monsters(void)
{
	int i;
	struct monster *mon;

	/* Procesar los monstruos (hacia atrás) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		/* Acceder al monstruo */
		mon = cave_monster(cave, i);

		/* El monstruo está listo para actuar de nuevo */
		mflag_off(mon->mflag, MFLAG_HANDLED);
	}
}

/**
 * Permitir que los monstruos en un nivel persistente congelado se recuperen
 */
void restore_monsters(void)
{
	int i;
	struct monster *mon;

	/* Obtener el número de turnos que han pasado */
	int num_turns = turn - cave->turn;

	/* Procesar los monstruos (hacia atrás) */
	for (i = cave_monster_max(cave) - 1; i >= 1; i--) {
		int status, status_red;

		/* Acceder al monstruo */
		mon = cave_monster(cave, i);

		/* Regenerar */
		regen_monster(mon, num_turns / 100);

		/* Manejar efectos temporales */
		status_red = num_turns * turn_energy(mon->mspeed) / z_info->move_energy;
		if (status_red > 0) {
			for (status = 0; status < MON_TMD_MAX; status++) {
				if (mon->m_timed[status]) {
					mon_dec_timed(mon, status, status_red, 0);
				}
			}
		}
	}
}