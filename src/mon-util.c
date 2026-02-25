/**
 * \file mon-util.c
 * \brief Utilidades de manipulación de monstruos.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "cmd-core.h"
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-summon.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "trap.h"

/**
 * ------------------------------------------------------------------------
 * Utilidades de registro (lore)
 * ------------------------------------------------------------------------ */
static const struct monster_flag monster_flag_table[] =
{
	#define RF(a, b, c) { RF_##a, b, c },
	#include "list-mon-race-flags.h"
	#undef RF
	{RF_MAX, 0, NULL}
};

/**
 * Devuelve una descripción para el flag de raza de monstruo dado.
 *
 * Devuelve una cadena vacía si el flag está fuera de rango.
 *
 * \param flag es uno de los flags RF_.
 */
const char *describe_race_flag(int flag)
{
	const struct monster_flag *rf = &monster_flag_table[flag];

	if (flag <= RF_NONE || flag >= RF_MAX)
		return "";

	return rf->desc;
}

/**
 * Crea una máscara de flags de monstruo de un tipo específico.
 *
 * \param f es el array de flags que estamos rellenando
 * \param ... es la lista de flags que estamos buscando
 *
 * N.B. RFT_MAX debe ser el último elemento en la lista ...
 */
void create_mon_flag_mask(bitflag *f, ...)
{
	const struct monster_flag *rf;
	int i;
	va_list args;

	rf_wipe(f);

	va_start(args, f);

	/* Process each type in the va_args */
    for (i = va_arg(args, int); i != RFT_MAX; i = va_arg(args, int)) {
		for (rf = monster_flag_table; rf->index < RF_MAX; rf++)
			if (rf->type == i)
				rf_on(f, rf->index);
	}

	va_end(args);

	return;
}


/**
 * ------------------------------------------------------------------------
 * Utilidades de búsqueda
 * ------------------------------------------------------------------------ */
/**
 * Devuelve el monstruo con el nombre dado. Si ningún monstruo tiene exactamente
 * el nombre indicado, devuelve el primero cuyo nombre contenga la cadena dada
 * (sin distinción de mayúsculas/minúsculas).
 */
struct monster_race *lookup_monster(const char *name)
{
	int i;
	struct monster_race *closest = NULL;

	/* Look for it */
	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];
		if (!race->name)
			continue;

		/* Test for equality */
		if (my_stricmp(name, race->name) == 0)
			return race;

		/* Test for close matches */
		if (!closest && my_stristr(race->name, name))
			closest = race;
	}

	/* Return our best match */
	return closest;
}

/**
 * Devuelve la base de monstruo que coincide con el nombre dado.
 */
struct monster_base *lookup_monster_base(const char *name)
{
	struct monster_base *base;

	/* Look for it */
	for (base = rb_info; base; base = base->next) {
		if (streq(name, base->name))
			return base;
	}

	return NULL;
}

/**
 * Devuelve si la base dada coincide con alguno de los nombres proporcionados.
 *
 * Acepta una lista de cadenas de nombres de longitud variable. La lista debe terminar con NULL.
 *
 * Esta función actualmente no se usa, excepto en un test... -NRM-
 */
bool match_monster_bases(const struct monster_base *base, ...)
{
	bool ok = false;
	va_list vp;
	char *name;

	va_start(vp, base);
	while (!ok && ((name = va_arg(vp, char *)) != NULL))
		ok = base == lookup_monster_base(name);
	va_end(vp);

	return ok;
}

/**
 * Devuelve el monstruo actualmente comandado, o NULL
 */
struct monster *get_commanded_monster(void)
{
	int i;

	/* Look for it */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Skip dead monsters */
		if (!mon->race) continue;

		/* Test for control */
		if (mon->m_timed[MON_TMD_COMMAND]) return mon;
	}

	return NULL;
}

/**
 * ------------------------------------------------------------------------
 * Actualizaciones de monstruos
 * ------------------------------------------------------------------------ */
/**
 * Analiza el camino desde el jugador hasta el monstruo visto por infravision
 * y olvida las casillas que habrían bloqueado la línea de visión
 */
static void path_analyse(struct chunk *c, struct loc grid)
{
	int path_n, i;
	struct loc path_g[256];

	if (c != cave) {
		return;
	}

	/* Plot the path. */
	path_n = project_path(c, path_g, z_info->max_range, player->grid,
		grid, PROJECT_NONE);

	/* Project along the path */
	for (i = 0; i < path_n - 1; ++i) {
		/* Forget grids which would block los */
		if (!square_allowslos(player->cave, path_g[i])) {
			sqinfo_off(square(c, path_g[i])->info, SQUARE_SEEN);
			square_forget(c, path_g[i]);
			square_light_spot(c, path_g[i]);
		}
	}
}

/**
 * Esta función actualiza el registro del monstruo dado.
 *
 * Esto implica calcular la distancia al jugador (si se solicita),
 * y luego comprobar la visibilidad (natural, infravision, ver-invisible,
 * telepatía), actualizando el flag de visibilidad del monstruo, redibujando
 * (o borrando) el monstruo cuando cambia su visibilidad, y tomando nota
 * de cualquier flag interesante del monstruo (sangre fría, invisible, etc).
 *
 * Nótese el nuevo campo "mflag" que codifica varios flags de estado del
 * monstruo, incluyendo "view" para cuando el monstruo está actualmente en
 * línea de visión, y "mark" para cuando el monstruo es visible mediante
 * detección.
 *
 * Los únicos campos del monstruo que se modifican aquí son "cdis" (la
 * distancia al jugador), "ml" (visible para el jugador), y
 * "mflag" (para mantener el flag "MFLAG_VIEW").
 *
 * Nótese la función especial "update_monsters()" que puede usarse para
 * llamar a esta función una vez por cada monstruo.
 *
 * Nótese el flag "full" que solicita que el campo "cdis" sea actualizado;
 * esto solo es necesario cuando el monstruo (o el jugador) se ha movido.
 *
 * Cada vez que un monstruo se mueve, debemos llamar a esta función para ese
 * monstruo, y actualizar la distancia y la visibilidad. Cada vez que
 * el jugador se mueve, debemos llamar a esta función para cada monstruo, y
 * actualizar la distancia y la visibilidad. Siempre que el "estado" del
 * jugador cambie de ciertas formas ("ceguera", "infravision", "telepatía"
 * y "ver invisible"), debemos llamar a esta función para cada monstruo
 * y actualizar la visibilidad.
 *
 * Las rutinas que cambian la "iluminación" de una casilla también deben
 * llamar a esta función para cualquier monstruo en esa casilla, ya que la
 * "visibilidad" de algunos monstruos puede depender de la iluminación
 * de su casilla.
 *
 * Nótese que esta función se llama una vez por monstruo cada vez que el
 * jugador se mueve. Cuando el jugador corre, esta función es uno de los
 * principales cuellos de botella, junto con "update_view()" y el código
 * de "process_monsters()", por lo que la eficiencia es importante.
 *
 * Nótese la versión "inline" optimizada de la función "distance()".
 *
 * Un monstruo es "visible" para el jugador si (1) ha sido detectado
 * por el jugador, (2) está cerca del jugador y el jugador tiene
 * telepatía, o (3) está cerca del jugador, en línea de visión
 * del jugador, y está "iluminado" por alguna combinación de
 * infravision, luz de antorcha, o luz permanente (los monstruos
 * invisibles solo son afectados por la "luz" si el jugador puede ver
 * invisible).
 *
 * Los monstruos que no están en el panel actual pueden ser "visibles" para
 * el jugador, y sus descripciones incluirán una referencia "fuera de pantalla".
 * Actualmente, los monstruos fuera de pantalla no pueden ser apuntados
 * ni vistos directamente, pero los objetivos antiguos permanecerán. XXX XXX
 *
 * El jugador puede elegir ser perturbado por varias cosas, incluyendo
 * "OPT(player, disturb_near)" (monstruo que es "fácilmente" visible se mueve
 * de alguna forma). Nótese que "moverse" incluye "aparecer" y "desaparecer".
 */
void update_mon(struct monster *mon, struct chunk *c, bool full)
{
	struct monster_lore *lore;

	int d;

	/* If still generating the level, measure distances from the middle */
	struct loc pgrid = character_dungeon ? player->grid :
		loc(c->width / 2, c->height / 2);

	/* Seen at all */
	bool flag = false;

	/* Seen by vision */
	bool easy = false;

	/* ESP permitted */
	bool telepathy_ok = player_of_has(player, OF_TELEPATHY);

	assert(mon != NULL);

	/* Return if this is not the current level */
	if (c != cave) {
		return;
	}

	lore = get_lore(mon->race);
	
	/* Compute distance, or just use the current one */
	if (full) {
		/* Distance components */
		int dy = ABS(pgrid.y - mon->grid.y);
		int dx = ABS(pgrid.x - mon->grid.x);

		/* Approximate distance */
		d = (dy > dx) ? (dy + (dx >>  1)) : (dx + (dy >> 1));

		/* Restrict distance */
		if (d > 255) d = 255;

		/* Save the distance */
		mon->cdis = d;
	} else {
		/* Extract the distance */
		d = mon->cdis;
	}

	/* Detected */
	if (mflag_has(mon->mflag, MFLAG_MARK)) flag = true;

	/* Check if telepathy works here */
	if (square_isno_esp(c, mon->grid) || square_isno_esp(c, pgrid)) {
		telepathy_ok = false;
	}

	/* Nearby */
	if (d <= z_info->max_sight) {
		/* Basic telepathy */
		if (telepathy_ok && monster_is_esp_detectable(mon)) {
			/* Detectable */
			flag = true;

			/* Check for LOS so that MFLAG_VIEW is set later */
			if (square_isview(c, mon->grid)) easy = true;
		}

		/* Normal line of sight and player is not blind */
		if (square_isview(c, mon->grid) && !player->timed[TMD_BLIND]) {
			/* Use "infravision" */
			if (d <= player->state.see_infra) {
				/* Learn about warm/cold blood */
				rf_on(lore->flags, RF_COLD_BLOOD);

				/* Handle "warm blooded" monsters */
				if (!rf_has(mon->race->flags, RF_COLD_BLOOD)) {
					/* Easy to see */
					easy = flag = true;
				}
			}

			/* Use illumination */
			if (square_isseen(c, mon->grid)) {
				/* Learn about invisibility */
				rf_on(lore->flags, RF_INVISIBLE);

				/* Handle invisibility */
				if (monster_is_invisible(mon)) {
					/* See invisible */
					if (player_of_has(player, OF_SEE_INVIS)) {
						/* Easy to see */
						easy = flag = true;
					}
				} else {
					/* Easy to see */
					easy = flag = true;
				}
			}

			/* Learn about intervening squares */
			path_analyse(c, mon->grid);
		}
	}

	/* If a mimic looks like an ignored item, it's not seen */
	if (monster_is_mimicking(mon)) {
		struct object *obj = mon->mimicked_obj;
		if (ignore_item_ok(player, obj))
			easy = flag = false;
	}

	/* Is the monster is now visible? */
	if (flag) {
		/* Learn about the monster's mind */
		if (telepathy_ok) {
			flags_set(lore->flags, RF_SIZE, RF_EMPTY_MIND, RF_WEIRD_MIND,
					  RF_SMART, RF_STUPID, FLAG_END);
		}

		/* It was previously unseen */
		if (!monster_is_visible(mon)) {
			/* Mark as visible */
			mflag_on(mon->mflag, MFLAG_VISIBLE);

			/* Draw the monster */
			square_light_spot(c, mon->grid);

			/* Update health bar as needed */
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			/* Count "fresh" sightings */
			if (lore->sights < SHRT_MAX)
				lore->sights++;

			/* Window stuff */
			player->upkeep->redraw |= PR_MONLIST;
		}
	} else if (monster_is_visible(mon)) {
		/* Not visible but was previously seen - treat mimics differently */
		if (!mon->mimicked_obj
				|| ignore_item_ok(player, mon->mimicked_obj)) {
			/* Mark as not visible */
			mflag_off(mon->mflag, MFLAG_VISIBLE);

			/* Erase the monster */
			square_light_spot(c, mon->grid);

			/* Update health bar as needed */
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			/* Window stuff */
			player->upkeep->redraw |= PR_MONLIST;
		}
	}


	/* Is the monster is now easily visible? */
	if (easy) {
		/* Change */
		if (!monster_is_in_view(mon)) {
			/* Mark as easily visible */
			mflag_on(mon->mflag, MFLAG_VIEW);

			/* Disturb on appearance */
			if (OPT(player, disturb_near))
				disturb(player);

			/* Re-draw monster window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	} else {
		/* Change */
		if (monster_is_in_view(mon)) {
			/* Mark as not easily visible */
			mflag_off(mon->mflag, MFLAG_VIEW);

			/* Disturb on disappearance */
			if (OPT(player, disturb_near) && !monster_is_camouflaged(mon))
				disturb(player);

			/* Re-draw monster list window */
			player->upkeep->redraw |= PR_MONLIST;
		}
	}
}

/**
 * Actualiza todos los monstruos (no muertos) mediante update_mon().
 */
void update_monsters(bool full)
{
	int i;

	/* Update each (live) monster */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Update the monster if alive */
		if (mon->race)
			update_mon(mon, cave, full);
	}
}


/**
 * ------------------------------------------------------------------------
 * Movimiento real de monstruos (y jugador)
 * ------------------------------------------------------------------------ */
/**
 * Se llama cuando el jugador acaba de abandonar grid1 hacia grid2.
 */
static void player_leaving(struct loc grid1, struct loc grid2)
{
	struct loc decoy = cave_find_decoy(cave);

	/* Decoys get destroyed if player is too far away */
	if (!loc_is_zero(decoy) &&
		distance(decoy, grid2) > z_info->max_sight) {
		square_destroy_decoy(cave, decoy);
	}

	/* Delayed traps trigger when the player leaves. */
	hit_trap(grid1, 1);
}

/**
 * Función auxiliar para mover un objeto mimético cuando el mimo (desconocido
 * para el jugador) es movido. Asume que el llamador ejecutará
 * square_light_spot() para la casilla de origen.
 */
static void move_mimicked_object(struct chunk *c, struct monster *mon,
	struct loc src, struct loc dest)
{
	struct object *mimicked = mon->mimicked_obj;
	/*
	 * Move a copy so, if necessary, the original can remain as a
	 * placeholder for the known version of the object in the player's
	 * view of the cave.
	 */
	struct object *moved = object_new();
	bool dummy = true;

	assert(mimicked);
	object_copy(moved, mimicked);
	moved->oidx = 0;
	mimicked->mimicking_m_idx = 0;
	if (mimicked->known) {
		moved->known = object_new();
		object_copy(moved->known, mimicked->known);
		moved->known->oidx = 0;
		moved->known->grid = loc(0,0);
	}
	if (floor_carry(c, dest, moved, &dummy)) {
		mon->mimicked_obj = moved;
	} else {
		/* Could not move the object so cancel mimicry. */
		moved->mimicking_m_idx = 0;
		mon->mimicked_obj = NULL;
		/* Give object to monster if appropriate; otherwise, delete. */
		if (!rf_has(mon->race->flags, RF_MIMIC_INV) ||
			!monster_carry(c, mon, moved)) {
			struct chunk *p_c = (c == cave) ? player->cave : NULL;
			if (moved->known) {
				object_delete(p_c, NULL, &moved->known);
			}
			object_delete(c, p_c, &moved);
		}
	}
	square_delete_object(c, src, mimicked, true, false);
}

/**
 * Intercambia los jugadores/monstruos (si los hay) en dos ubicaciones.
 */
void monster_swap(struct loc grid1, struct loc grid2)
{
	int m1, m2;
	struct monster *mon;
	struct loc pgrid = player->grid;

	/* Monsters */
	m1 = cave->squares[grid1.y][grid1.x].mon;
	m2 = cave->squares[grid2.y][grid2.x].mon;

	/* Update grids */
	square_set_mon(cave, grid1, m2);
	square_set_mon(cave, grid2, m1);

	/* Monster 1 */
	if (m1 > 0) {
		/* Monster */
		mon = cave_monster(cave, m1);

		/* Update monster */
		if (monster_is_camouflaged(mon)) {
			/*
			 * Become aware if the player can see the grid with
			 * the camouflaged monster before or after the swap.
			 */
			if (monster_is_in_view(mon) ||
				(m2 >= 0 && los(cave, pgrid, grid2)) ||
				(m2 < 0 && los(cave, grid1, grid2))) {
				become_aware(cave, mon);
			} else if (monster_is_mimicking(mon)) {
				move_mimicked_object(cave, mon, grid1, grid2);
				player->upkeep->redraw |= (PR_ITEMLIST);
			}
		}
		mon->grid = grid2;
		update_mon(mon, cave, true);

		/* Affect light? */
		if (mon->race->light != 0)
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m1 < 0) {
		/* Player */
		player->grid = grid2;
		player_leaving(pgrid, player->grid);

		/* Update the trap detection status */
		player->upkeep->redraw |= (PR_DTRAP);

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Monster 2 */
	if (m2 > 0) {
		/* Monster */
		mon = cave_monster(cave, m2);

		/* Update monster */
		if (monster_is_camouflaged(mon)) {
			/*
			 * Become aware if the player can see the grid with
			 * the camouflaged monster before or after the swap.
			 */
			if (monster_is_in_view(mon) ||
				(m1 >= 0 && los(cave, pgrid, grid1)) ||
				(m1 < 0 && los(cave, grid2, grid1))) {
				become_aware(cave, mon);
			} else if (monster_is_mimicking(mon)) {
				move_mimicked_object(cave, mon, grid2, grid1);
				player->upkeep->redraw |= (PR_ITEMLIST);
			}
		}
		mon->grid = grid1;
		update_mon(mon, cave, true);

		/* Affect light? */
		if (mon->race->light != 0)
			player->upkeep->update |= PU_UPDATE_VIEW | PU_MONSTERS;

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);
	} else if (m2 < 0) {
		/* Player */
		player->grid = grid1;
		player_leaving(pgrid, player->grid);

		/* Update the trap detection status */
		player->upkeep->redraw |= (PR_DTRAP);

		/* Updates */
		player->upkeep->update |= (PU_PANEL | PU_UPDATE_VIEW | PU_DISTANCE);

		/* Redraw monster list */
		player->upkeep->redraw |= (PR_MONLIST);

		/* Don't allow command repeat if moved away from item used. */
		cmd_disable_repeat_floor_item();
	}

	/* Redraw */
	square_light_spot(cave, grid1);
	square_light_spot(cave, grid2);
}

/**
 * ------------------------------------------------------------------------
 * Consciencia y aprendizaje
 * ------------------------------------------------------------------------ */
/**
 * El monstruo se despierta y posiblemente se hace consciente del jugador
 */
void monster_wake(struct monster *mon, bool notify, int aware_chance)
{
	int flag = notify ? MON_TMD_FLG_NOTIFY : MON_TMD_FLG_NOMESSAGE;
	mon_clear_timed(mon, MON_TMD_SLEEP, flag);
	if (randint0(100) < aware_chance) {
		mflag_on(mon->mflag, MFLAG_AWARE);
	}
}

/**
 * El monstruo puede ver una casilla
 */
bool monster_can_see(struct chunk *c, struct monster *mon, struct loc grid)
{
	return los(c, mon->grid, grid);
}

/**
 * Hace que el jugador sea plenamente consciente del mimo dado.
 *
 * \param c Es el chunk con el monstruo.
 * \param mon Es el monstruo.
 * Cuando el jugador se hace consciente de un mimo, actualizamos la memoria
 * del monstruo y eliminamos el "objeto falso" que el monstruo estaba mimando.
 */
void become_aware(struct chunk *c, struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);

	if (mflag_has(mon->mflag, MFLAG_CAMOUFLAGE)) {
		mflag_off(mon->mflag, MFLAG_CAMOUFLAGE);

		/* Learn about mimicry */
		if (rf_has(mon->race->flags, RF_UNAWARE))
			rf_on(lore->flags, RF_UNAWARE);

		/* Delete any false items */
		if (mon->mimicked_obj) {
			struct object *obj = mon->mimicked_obj;
			char o_name[80];
			object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, player);

			/* Print a message */
			if (square_isseen(c, obj->grid))
				msg("¡El %s era realmente un monstruo!", o_name);

			/* Clear the mimicry */
			obj->mimicking_m_idx = 0;
			mon->mimicked_obj = NULL;

			/*
			 * Give a copy of the object to the monster if
			 * appropriate.
			 */
			if (rf_has(mon->race->flags, RF_MIMIC_INV)) {
				struct object* given = object_new();

				object_copy(given, obj);
				given->oidx = 0;
				if (obj->known) {
					given->known = object_new();
					object_copy(given->known, obj->known);
					given->known->oidx = 0;
					given->known->grid = loc(0, 0);
				}
				if (!monster_carry(c, mon, given)) {
					struct chunk *p_c = (c == cave) ? player->cave : NULL;
					if (given->known) {
						object_delete(p_c, NULL, &given->known);
					}
					object_delete(c, p_c, &given);
				}
			}

			/*
			 * Delete the mimicked object; noting and lighting
			 * done below outside of the if block.
			 */
			square_delete_object(c, obj->grid, obj, false, false);

			/* Since mimicry affects visibility, update that. */
			update_mon(mon, c, false);
		}

		/* Update monster and item lists */
		if (mon->race->light != 0) {
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		}
		player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
	}

	square_note_spot(c, mon->grid);
	square_light_spot(c, mon->grid);
}

/**
 * El monstruo dado aprende sobre una resistencia "observada" u otra
 * propiedad del estado del jugador, o su ausencia.
 *
 * Nótese que esta función es robusta ante ser llamada con `element` como
 * un tipo PROJ_ arbitrario
 */
void update_smart_learn(struct monster *mon, struct player *p, int flag,
						int pflag, int element)
{
	bool element_ok = ((element >= 0) && (element < ELEM_MAX));

	/* Sanity check */
	if (!flag && !element_ok) return;

	/* Anything a monster might learn, the player should learn */
	if (flag) equip_learn_flag(p, flag);
	if (element_ok) equip_learn_element(p, element);

	/* Not allowed to learn */
	if (!OPT(p, birth_ai_learn)) return;

	/* Too stupid to learn anything */
	if (monster_is_stupid(mon)) return;

	/* Not intelligent, only learn sometimes */
	if (!monster_is_smart(mon) && one_in_(2)) return;

	/* Analyze the knowledge; fail very rarely */
	if (one_in_(100))
		return;

	/* Learn the flag */
	if (flag) {
		if (player_of_has(p, flag)) {
			of_on(mon->known_pstate.flags, flag);
		} else {
			of_off(mon->known_pstate.flags, flag);
		}
	}

	/* Learn the pflag */
	if (pflag) {
		if (pf_has(p->state.pflags, pflag)) {
			of_on(mon->known_pstate.pflags, pflag);
		} else {
			of_off(mon->known_pstate.pflags, pflag);
		}
	}

	/* Learn the element */
	if (element_ok)
		mon->known_pstate.el_info[element].res_level
			= p->state.el_info[element].res_level;
}

/**
 * ------------------------------------------------------------------------
 * Curación de monstruos
 * ------------------------------------------------------------------------ */
#define MAX_KIN_RADIUS			5
#define MAX_KIN_DISTANCE		5

/**
 * Dado un chunk de mazmorra, un monstruo y una ubicación, comprueba si hay
 * un monstruo herido del mismo tipo base en línea de visión y a menos de
 * MAX_KIN_DISTANCE de distancia.
 */
static struct monster *get_injured_kin(struct chunk *c,
									   const struct monster *mon,
									   struct loc grid)
{
	/* Ignore the monster itself */
	if (loc_eq(grid, mon->grid))
		return NULL;

	/* Check kin */
	struct monster *kin = square_monster(c, grid);
	if (!kin)
		return NULL;

	if (kin->race->base != mon->race->base)
		return NULL;

	/* Check line of sight */
	if (los(c, mon->grid, grid) == false)
		return NULL;

	/* Check injury */
	if (kin->hp == kin->maxhp)
		return NULL;

	/* Check distance */
	if (distance(mon->grid, grid) > MAX_KIN_DISTANCE)
		return NULL;

	return kin;
}

/**
 * Averigua si hay monstruos heridos cercanos.
 *
 * Véase get_injured_kin() arriba para más detalles sobre qué monstruos
 * califican.
 */
bool find_any_nearby_injured_kin(struct chunk *c, const struct monster *mon)
{
	struct loc grid;
	for (grid.y = mon->grid.y - MAX_KIN_RADIUS;
		 grid.y <= mon->grid.y + MAX_KIN_RADIUS; grid.y++) {
		for (grid.x = mon->grid.x - MAX_KIN_RADIUS;
			 grid.x <= mon->grid.x + MAX_KIN_RADIUS; grid.x++) {
			if (get_injured_kin(c, mon, grid) != NULL) {
				return true;
			}
		}
	}

	return false;
}

/**
 * Elige un monstruo herido del mismo tipo base en línea de visión del
 * monstruo indicado.
 *
 * Escanea MAX_KIN_RADIUS casillas alrededor del monstruo para encontrar
 * casillas candidatas, usando muestreo de reserva con k = 1 para elegir
 * una aleatoria.
 */
struct monster *choose_nearby_injured_kin(struct chunk *c,
                                          const struct monster *mon)
{
	struct loc grid;
	int nseen = 0;
	struct monster *found = NULL;

	for (grid.y = mon->grid.y - MAX_KIN_RADIUS;
		 grid.y <= mon->grid.y + MAX_KIN_RADIUS; grid.y++) {
		for (grid.x = mon->grid.x - MAX_KIN_RADIUS;
			 grid.x <= mon->grid.x + MAX_KIN_RADIUS; grid.x++) {
			struct monster *kin = get_injured_kin(c, mon, grid);
			if (kin) {
				nseen++;
				if (!randint0(nseen))
					found = kin;
			}
		}
	}

	return found;
}


/**
 * ------------------------------------------------------------------------
 * Utilidades de daño y muerte de monstruos
 * ------------------------------------------------------------------------ */
/**
 * Gestiona la "muerte" de un monstruo.
 *
 * Dispersa los tesoros transportados por el monstruo centrados en su
 * ubicación. Nótese que los objetos soltados pueden desaparecer en salas
 * abarrotadas.
 *
 * Comprueba si se completa alguna "Misión" cuando se mata a un monstruo
 * de misión.
 *
 * Nótese que solo el jugador puede provocar "monster_death()" en los
 * Únicos. Por tanto (por ahora) todos los monstruos de misión deben ser
 * Únicos.
 *
 * Si `stats` es true, se omite la actualización de la memoria del monstruo.
 * Esto lo usa el código de generación de estadísticas, por eficiencia.
 */
void monster_death(struct monster *mon, struct player *p, bool stats)
{
	int dump_item = 0;
	int dump_gold = 0;
	struct object *obj = mon->held_obj;

	bool visible = monster_is_visible(mon) || monster_is_unique(mon);

	/* Delete any mimicked objects */
	if (mon->mimicked_obj) {
		square_delete_object(cave, mon->grid, mon->mimicked_obj, true, true);
		mon->mimicked_obj = NULL;
	}

	/* Drop objects being carried */
	while (obj) {
		struct object *next = obj->next;

		/* Object no longer held */
		obj->held_m_idx = 0;
		pile_excise(&mon->held_obj, obj);

		/* Count it and drop it - refactor once origin is a bitflag */
		if (!stats) {
			if (tval_is_money(obj) && (obj->origin != ORIGIN_STOLEN))
				dump_gold++;
			else if (!tval_is_money(obj) && ((obj->origin == ORIGIN_DROP)
					|| (obj->origin == ORIGIN_DROP_PIT)
					|| (obj->origin == ORIGIN_DROP_VAULT)
					|| (obj->origin == ORIGIN_DROP_SUMMON)
					|| (obj->origin == ORIGIN_DROP_SPECIAL)
					|| (obj->origin == ORIGIN_DROP_BREED)
					|| (obj->origin == ORIGIN_DROP_POLY)
					|| (obj->origin == ORIGIN_DROP_WIZARD)))
				dump_item++;
		}

		/* Change origin if monster is invisible, unless we're in stats mode */
		if (!visible && !stats)
			obj->origin = ORIGIN_DROP_UNKNOWN;

		drop_near(cave, &obj, 0, mon->grid, true, false);
		obj = next;
	}

	/* Forget objects */
	mon->held_obj = NULL;

	/* Take note of any dropped treasure */
	if (visible && (dump_item || dump_gold))
		lore_treasure(mon, dump_item, dump_gold);

	/* Update monster list window */
	p->upkeep->redraw |= PR_MONLIST;

	/* Check if we finished a quest */
	quest_check(p, mon);
}

/**
 * Gestiona las consecuencias de que el jugador mate a un monstruo
 */
static void player_kill_monster(struct monster *mon, struct player *p,
		const char *note)
{
	int32_t div, new_exp, new_exp_frac;
	struct monster_lore *lore = get_lore(mon->race);
	char m_name[80];
	char buf[80];
	int desc_mode = MDESC_DEFAULT | ((note) ? MDESC_COMMA : 0);

	/* Assume normal death sound */
	int soundfx = MSG_KILL;

	/* Extract monster name */
	monster_desc(m_name, sizeof(m_name), mon, desc_mode);

	/* Shapechanged monsters revert on death */
	if (mon->original_race) {
		monster_revert_shape(mon);
		lore = get_lore(mon->race);
		monster_desc(m_name, sizeof(m_name), mon, desc_mode);
	}

	/* Play a special sound if the monster was unique */
	if (monster_is_unique(mon)) {
		if (mon->race->base == lookup_monster_base("Morgoth"))
			soundfx = MSG_KILL_KING;
		else
			soundfx = MSG_KILL_UNIQUE;
	}

	/* Death message */
	if (note) {
		if (strlen(note) <= 1) {
			/* Death by Spell attack - messages handled by project_m() */
		} else {
			/* Make sure to flush any monster messages first */
			notice_stuff(p);

			/* Death by Missile attack */
			my_strcap(m_name);
			msgt(soundfx, "%s%s", m_name, note);
		}
	} else {
		/* Make sure to flush any monster messages first */
		notice_stuff(p);

		if (!monster_is_visible(mon))
			/* Death by physical attack -- invisible monster */
			msgt(soundfx, "Has matado a %s.", m_name);
		else if (monster_is_destroyed(mon))
			/* Death by Physical attack -- non-living monster */
			msgt(soundfx, "Has destruido a %s.", m_name);
		else
			/* Death by Physical attack -- living monster */
			msgt(soundfx, "Has derrotado a %s.", m_name);
	}

	/* Player level */
	div = p->lev;

	/* Give some experience for the kill */
	new_exp = ((long)mon->race->mexp * mon->race->level) / div;

	/* Handle fractional experience */
	new_exp_frac = ((((long)mon->race->mexp * mon->race->level) % div)
					* 0x10000L / div) + p->exp_frac;

	/* Keep track of experience */
	if (new_exp_frac >= 0x10000L) {
		new_exp++;
		p->exp_frac = (uint16_t)(new_exp_frac - 0x10000L);
	} else {
		p->exp_frac = (uint16_t)new_exp_frac;
	}

	/* When the player kills a Unique, it stays dead */
	if (monster_is_unique(mon)) {
		char unique_name[80];
		assert(mon->original_race == NULL);
		mon->race->max_num = 0;

		/*
		 * This gets the correct name if we slay an invisible
		 * unique and don't have See Invisible.
		 */
		monster_desc(unique_name, sizeof(unique_name), mon,
					 MDESC_DIED_FROM);

		/* Log the slaying of a unique */
		strnfmt(buf, sizeof(buf), "Mató a %s", unique_name);
		history_add(p, buf, HIST_SLAY_UNIQUE);
	}

	/* Gain experience */
	player_exp_gain(p, new_exp);

	/* Generate treasure */
	monster_death(mon, p, false);

	/* Bloodlust bonus */
	if (p->timed[TMD_BLOODLUST]) {
		player_inc_timed(p, TMD_BLOODLUST, 10, false, false, true);
		player_over_exert(p, PY_EXERT_CONF, 5, 3);
		player_over_exert(p, PY_EXERT_HALLU, 5, 10);
	}

	/* Recall even invisible uniques or winners */
	if (monster_is_visible(mon) || monster_is_unique(mon)) {
		/* Count kills this life */
		if (lore->pkills < SHRT_MAX) lore->pkills++;

		/* Count kills in all lives */
		if (lore->tkills < SHRT_MAX) lore->tkills++;

		/* Update lore and tracking */
		lore_update(mon->race, lore);
		monster_race_track(p->upkeep, mon->race);
	}

	/* Delete the monster */
	delete_monster_idx(cave, mon->midx);
}

/**
 * Analiza cómo reacciona un monstruo al daño recibido
 */
static bool monster_scared_by_damage(struct monster *mon, int dam)
{
	int current_fear = mon->m_timed[MON_TMD_FEAR];

	/* Pain can reduce or cancel existing fear, or cause fear */
	if (current_fear) {
		int tmp = randint1(dam);

		/* Cure a little or all fear */
		if (tmp < current_fear) {
			/* Reduce fear */
			mon_dec_timed(mon, MON_TMD_FEAR, tmp, MON_TMD_FLG_NOMESSAGE);
		} else {
			/* Cure fear */
			mon_clear_timed(mon, MON_TMD_FEAR, MON_TMD_FLG_NOMESSAGE);
			return false;
		}
	} else if (monster_can_be_scared(mon)) {
		/* Percentage of fully healthy */
		int percentage = (100L * mon->hp) / mon->maxhp;

		/* Run (sometimes) if at 10% or less of max hit points... */
		bool low_hp = randint1(10) >= percentage;

		/* ...or (usually) when hit for half its current hit points */
		bool big_hit = (dam >= mon->hp) && (randint0(100) < 80);

		if (low_hp || big_hit) {
			int time = randint1(10);
			if ((dam >= mon->hp) && (percentage > 7)) {
				time += 20;
			} else {
				time += (11 - percentage) * 5;
			}

			/* Note fear */
			mon_inc_timed(mon, MON_TMD_FEAR, time,
						  MON_TMD_FLG_NOMESSAGE | MON_TMD_FLG_NOFAIL);
			return true;
		}
	}
	return false;
}

/**
 * Inflige daño a un monstruo desde otro monstruo (o al menos no del jugador).
 *
 * Es una función auxiliar para los manejadores de combate cuerpo a cuerpo.
 * Es muy similar a mon_take_hit(), pero elimina las partes orientadas al
 * jugador de esa función.
 *
 * \param dam es la cantidad de daño a infligir
 * \param t_mon es el monstruo al que se daña
 * \param hurt_msg es el mensaje, si lo hay, a usar cuando el monstruo resulta herido
 * \param die_msg es el mensaje, si lo hay, a usar cuando el monstruo muere
 * \return true si el monstruo murió, false si sigue con vida
 */
bool mon_take_nonplayer_hit(int dam, struct monster *t_mon,
							enum mon_messages hurt_msg,
							enum mon_messages die_msg)
{
	assert(t_mon);

	/* "Unique" or arena monsters can only be "killed" by the player */
	if (monster_is_unique(t_mon) || player->upkeep->arena_level) {
		/* Reduce monster hp to zero, but don't kill it. */
		if (dam > t_mon->hp) dam = t_mon->hp;
	}

	/* Redraw (later) if needed */
	if (player->upkeep->health_who == t_mon)
		player->upkeep->redraw |= (PR_HEALTH);

	/* Wake the monster up, doesn't become aware of the player */
	monster_wake(t_mon, false, 0);

	/* Hurt the monster */
	t_mon->hp -= dam;

	/* Dead or damaged monster */
	if (t_mon->hp < 0) {
		/* Shapechanged monsters revert on death */
		if (t_mon->original_race) {
			monster_revert_shape(t_mon);
		}

		/* Death message */
		add_monster_message(t_mon, die_msg, false);

		/* Generate treasure, etc */
		monster_death(t_mon, player, false);

		/* Delete the monster */
		delete_monster_idx(cave, t_mon->midx);
		return true;
	} else if (!monster_is_camouflaged(t_mon)) {
		/* Give detailed messages if visible */
		if (hurt_msg != MON_MSG_NONE) {
			add_monster_message(t_mon, hurt_msg, false);
		} else if (dam > 0) {
			message_pain(t_mon, dam);
		}
	}

	/* Sometimes a monster gets scared by damage */
	if (!t_mon->m_timed[MON_TMD_FEAR] && dam > 0) {
		(void) monster_scared_by_damage(t_mon, dam);
	}

	return false;
}

/**
 * Reduce los puntos de vida de un monstruo en `dam` y gestiona su muerte.
 *
 * "Retrasamos" los mensajes de miedo pasando un flag "fear".
 *
 * Anunciamos la muerte del monstruo (usando un "mensaje de muerte" opcional
 * (`note`) si se proporciona, o de lo contrario un mensaje genérico de
 * matado/destruido).
 *
 * Devuelve true si el monstruo ha sido eliminado (y borrado).
 *
 * TODO: Considerar reducir la experiencia del monstruo con el tiempo, por
 * ejemplo usando "(m_exp * m_lev * (m_lev)) / (p_lev * (m_lev + n_killed))"
 * en lugar de simplemente "(m_exp * m_lev) / (p_lev)", para que el primer
 * monstruo valga más que los siguientes. Esto también requeriría cambios
 * en el código de recuerdo de monstruos. XXX XXX XXX
 **/
bool mon_take_hit(struct monster *mon, struct player *p, int dam, bool *fear,
		const char *note)
{
	/* Redraw (later) if needed */
	if (p->upkeep->health_who == mon)
		p->upkeep->redraw |= (PR_HEALTH);

	/* If the hit doesn't kill, wake it up, make it aware of the player */
	if (dam <= mon->hp) {
		monster_wake(mon, false, 100);
		mon_clear_timed(mon, MON_TMD_HOLD, MON_TMD_FLG_NOTIFY);
	}

	/* Become aware of its presence */
	if (monster_is_camouflaged(mon))
		become_aware(cave, mon);

	/* No damage, we're done */
	if (dam == 0) return false;

	/* Covering tracks is no longer possible */
	p->timed[TMD_COVERTRACKS] = 0;

	/* Hurt it */
	mon->hp -= dam;
	if (mon->hp < 0) {
		/* Deal with arena monsters */
		if (p->upkeep->arena_level) {
			p->upkeep->generate_level = true;
			p->upkeep->health_who = mon;
			(*fear) = false;
			return true;
		}

		/* It is dead now */
		player_kill_monster(mon, p, note);

		/* Not afraid */
		(*fear) = false;

		/* Monster is dead */
		return true;
	} else {
		/* Did it get frightened? */
		(*fear) = monster_scared_by_damage(mon, dam);

		/* Not dead yet */
		return false;
	}
}

void kill_arena_monster(struct monster *mon)
{
	struct monster *old_mon = cave_monster(cave, mon->midx);
	assert(old_mon);
	update_mon(old_mon, cave, true);
	old_mon->hp = -1;
	player_kill_monster(old_mon, player, " ¡ha sido derrotado!");
}

/**
 * El terreno daña al monstruo
 */
void monster_take_terrain_damage(struct monster *mon)
{
	/* Damage the monster */
	if (square_isfiery(cave, mon->grid)) {
		bool fear = false;

		if (!rf_has(mon->race->flags, RF_IM_FIRE)) {
			mon_take_nonplayer_hit(100 + randint1(100), mon, MON_MSG_CATCH_FIRE,
								   MON_MSG_DISINTEGRATES);
		}

		if (fear && monster_is_visible(mon)) {
			add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
		}
	}	
}

/**
 * El terreno está dañando actualmente al monstruo
 */
bool monster_taking_terrain_damage(struct chunk *c, struct monster *mon)
{
	if (square_isdamaging(c, mon->grid) &&
		!rf_has(mon->race->flags, square_feat(c, mon->grid)->resist_flag)) {
		return true;
	}

	return false;
}


/**
 * ------------------------------------------------------------------------
 * Utilidades de inventario de monstruos
 * ------------------------------------------------------------------------ */
/**
 * Añade el objeto dado al inventario del monstruo dado.
 *
 * Actualmente siempre devuelve true — se deja como bool en lugar de
 * void por si en el futuro se propone un límite al tamaño del inventario
 * del monstruo.
 */
bool monster_carry(struct chunk *c, struct monster *mon, struct object *obj)
{
	struct object *held_obj;

	/* Scan objects already being held for combination */
	for (held_obj = mon->held_obj; held_obj; held_obj = held_obj->next) {
		/* Check for combination */
		if (object_mergeable(held_obj, obj, OSTACK_MONSTER)) {
			/* Combine the items */
			object_absorb(held_obj, obj);

			/* Result */
			return true;
		}
	}

	/* Forget location */
	obj->grid = loc(0, 0);

	/* Link the object to the monster */
	obj->held_m_idx = mon->midx;

	/* Add the object to the monster's inventory */
	list_object(c, obj);
	if (obj->known) {
		obj->known->oidx = obj->oidx;
		player->cave->objects[obj->oidx] = obj->known;
	}
	pile_insert(&mon->held_obj, obj);

	/* Result */
	return true;
}

/**
 * Obtiene un objeto aleatorio del inventario de un monstruo
 */
struct object *get_random_monster_object(struct monster *mon)
{
    struct object *obj, *pick = NULL;
    int i = 1;

    /* Pick a random object */
    for (obj = mon->held_obj; obj; obj = obj->next)
    {
        /* Check it isn't a quest artifact */
        if (obj->artifact && kf_has(obj->kind->kind_flags, KF_QUEST_ART))
            continue;

        if (one_in_(i)) pick = obj;
        i++;
    }

    return pick;
}

/**
 * El jugador o un monstruo con midx roba un objeto a un monstruo
 *
 * \param mon Monstruo al que se le roba
 * \param midx Índice del ladrón
 */
void steal_monster_item(struct monster *mon, int midx)
{
	struct object *obj = get_random_monster_object(mon);
	struct monster_lore *lore = get_lore(mon->race);
	struct monster *thief = NULL;
	char m_name[80];

	/* Get the target monster name (or "it") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	if (midx < 0) {
		/* Base monster protection and player stealing skill */
		bool unique = monster_is_unique(mon);
		int guard = (mon->race->level * (unique ? 4 : 3)) / 4 +
			mon->mspeed - player->state.speed;
		int steal_skill = player->state.skills[SKILL_STEALTH] +
			adj_dex_th[player->state.stat_ind[STAT_DEX]];
		int monster_reaction;

		/* No object */
		if (!obj) {
			msg("No encuentras nada que robarle a %s.", m_name);
			if (one_in_(3)) {
				/* Monster notices */
				monster_wake(mon, false, 100);
			}
			return;
		}

		/* Penalize some status conditions */
		if (player->timed[TMD_BLIND] || player->timed[TMD_CONFUSED] ||
			player->timed[TMD_IMAGE]) {
			steal_skill /= 4;
		}
		if (mon->m_timed[MON_TMD_SLEEP]) {
			guard /= 2;
		}

		/* Monster base reaction, plus allowance for item weight */
		monster_reaction = guard / 2 + randint1(MAX(guard, 1));
		monster_reaction += (obj->number * object_weight_one(obj)) / 20;

		/* Try and steal */
		if (monster_reaction < steal_skill) {
			int wake = 35 - player->state.skills[SKILL_STEALTH];

			/* Success! */
			obj->held_m_idx = 0;
			pile_excise(&mon->held_obj, obj);
			if (tval_is_money(obj)) {
				msg("Robas %d piezas de oro en tesoro.", obj->pval);
				player->au += obj->pval;
				player->upkeep->redraw |= (PR_GOLD);
				delist_object(cave, obj);
				object_delete(cave, player->cave, &obj);
			} else {
				object_grab(player, obj);
				delist_object(player->cave, obj->known);
				delist_object(cave, obj);
				/* Drop immediately if ignored,
				   or if inventory already full to prevent pack overflow */
				if (ignore_item_ok(player, obj) || !inven_carry_okay(obj)) {
					char o_name[80];
					object_desc(o_name, sizeof(o_name), obj,
						ODESC_PREFIX | ODESC_FULL,
						player);
					drop_near(cave, &obj, 0, player->grid, true, true);
					msg("Dejas caer %s.", o_name);
				} else {
					inven_carry(player, obj, true, true);
				}
			}

			/* Track thefts */
			lore->thefts++;

			/* Monster wakes a little */
			mon_dec_timed(mon, MON_TMD_SLEEP, wake, MON_TMD_FLG_NOTIFY);
		} else if (monster_reaction / 2 < steal_skill) {
			/* Decent attempt, at least */
			char o_name[80];

			object_see(player, obj);
			if (tval_is_money(obj)) {
				(void)strnfmt(o_name, sizeof(o_name), "tesoro");
			} else {
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_PREFIX | ODESC_FULL, player);
			}
			msg("Fallas al intentar robar %s a %s.", o_name, m_name);
			/* Monster wakes, may notice */
			monster_wake(mon, true, 50);
		} else {
			/* Bungled it */
			monster_wake(mon, true, 100);
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
			msg("¡%s grita enfurecido!", m_name);
			effect_simple(EF_WAKE, source_monster(mon->midx), "", 0, 0, 0, 0, 0,
						  NULL);
		}

		/* Player hit and run */
		if (player->timed[TMD_ATT_RUN]) {
			const char *near = "20";
			msg("¡Te desvaneces entre las sombras!");
			effect_simple(EF_TELEPORT, source_player(), near, 0, 0, 0, 0, 0,
						  NULL);
			(void) player_clear_timed(player, TMD_ATT_RUN, false,
				false);
		}
	} else {
		/* Get the thief details */
		char t_name[80];
		thief = cave_monster(cave, midx);
		assert(thief);
		monster_desc(t_name, sizeof(t_name), thief, MDESC_STANDARD);

		/* Try to steal */
		if (!obj || react_to_slay(obj, thief)) {
			/* Fail to steal */
			msg("%s intenta robarle algo a %s, pero falla.", t_name,
				m_name);
		} else {
			msg("¡%s le roba algo a %s!", t_name, m_name);

			/* Steal and carry */
			obj->held_m_idx = 0;
			pile_excise(&mon->held_obj, obj);
			(void)monster_carry(cave, thief, obj);
		}
	}
}


/**
 * ------------------------------------------------------------------------
 * Utilidades de transformación de monstruos
 * ------------------------------------------------------------------------ */
/**
 * La base de forma para las transformaciones
 */
struct monster_base *shape_base;

/**
 * Función predicado para get_mon_num_prep
 * Comprueba si la raza del monstruo tiene la misma base que la forma deseada
 */
static bool monster_base_shape_okay(struct monster_race *race)
{
	assert(race);

	/* Check if it matches */
	if (race->base != shape_base) return false;

	return true;
}

/**
 * Transformación de monstruo
 */
bool monster_change_shape(struct monster *mon)
{
	struct monster_shape *shape = mon->race->shapes;
	struct monster_race *race = NULL;

	/* Use the monster's preferred shapes if any */
	if (shape) {
		/* Pick one */
		int choice = randint0(mon->race->num_shapes);
		while (choice--) {
			shape = shape->next;
		}

		/* Race or base? */
		if (shape->race) {
			/* Simple */
			race = shape->race;
		} else {
			/* Set the shape base */
			shape_base = shape->base;

			/* Choose a race of the given base */
			get_mon_num_prep(monster_base_shape_okay);

			/* Pick a random race */
			race = get_mon_num(player->depth + 5, player->depth);

			/* Reset allocation table */
			get_mon_num_prep(NULL);
		}
	} else {
		/* Choose something the monster can summon */
		bitflag summon_spells[RSF_SIZE];
		int i, poss = 0, which, index, summon_type;
		const struct monster_spell *spell;

		/* Extract the summon spells */
		create_mon_spell_mask(summon_spells, RST_SUMMON, RST_NONE);
		rsf_inter(summon_spells, mon->race->spell_flags);

		/* Count possibilities */
		for (i = rsf_next(summon_spells, FLAG_START); i != FLAG_END;
			 i = rsf_next(summon_spells, i + 1)) {
			poss++;
		}

		/* Pick one */
		which = randint0(poss);
		index = rsf_next(summon_spells, FLAG_START);
		for (i = 0; i < which; i++) {
			index = rsf_next(summon_spells, index);
		}
		spell = monster_spell_by_index(index);

		/* Set the summon type, and the kin_base if necessary */
		summon_type = spell->effect->subtype;
		if (summon_type == summon_name_to_idx("KIN")) {
			kin_base = mon->race->base;
		}

		/* Choose a race */
		race = select_shape(mon, summon_type);
	}

	/* Print a message immediately, update visuals */
	if (monster_is_obvious(mon)) {
		char m_name[80];
		monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
		msgt(MSG_GENERIC, "%s %s", m_name, "�se transforma y cambia de forma!");
		if (player->upkeep->health_who == mon)
			player->upkeep->redraw |= (PR_HEALTH);

		player->upkeep->redraw |= (PR_MONLIST);
		square_light_spot(cave, mon->grid);
	}

	/* Set the race */
	if (race) {
		if (!mon->original_race) mon->original_race = mon->race;
		mon->race = race;
		mon->mspeed += mon->race->speed - mon->original_race->speed;
	}

	/* Emergency teleport if needed */
	if (!monster_passes_walls(mon) &&
		!square_is_monster_walkable(cave, mon->grid)) {
		effect_simple(EF_TELEPORT, source_monster(mon->midx), "1", 0, 0, 0,
					  mon->grid.y, mon->grid.x, NULL);
	}

	return mon->original_race != NULL;
}

/**
 * Reversión de transformación de monstruo
 */
bool monster_revert_shape(struct monster *mon)
{
	if (mon->original_race) {
		if (monster_is_obvious(mon)) {
			char m_name[80];
			monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
			msgt(MSG_GENERIC, "%s %s", m_name, "�se transforma y cambia de forma!");
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			player->upkeep->redraw |= (PR_MONLIST);
			square_light_spot(cave, mon->grid);
		}
		mon->mspeed += mon->original_race->speed - mon->race->speed;
		mon->race = mon->original_race;
		mon->original_race = NULL;

		/* Emergency teleport if needed */
		if (!monster_passes_walls(mon) &&
			!square_is_monster_walkable(cave, mon->grid)) {
			effect_simple(EF_TELEPORT, source_monster(mon->midx), "1", 0, 0, 0,
						  mon->grid.y, mon->grid.x, NULL);
		}

		return true;
	}

	return false;
}
