/**
 * \file player-util.c
 * \brief Funciones de utilidad del jugador
 *
 * Copyright (c) 2011 The Angband Developers. See COPYING.
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
#include "cmd-core.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-predicate.h"
#include "obj-chest.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "score.h"
#include "store.h"
#include "target.h"
#include "trap.h"
#include "ui-input.h"

/**
 * Incrementar al siguiente nivel o decrementar al nivel anterior
   teniendo en cuenta el valor de salto de escaleras en las constantes
   Recordar verificar todos los niveles intermedios para misiones no omitibles
*/
int dungeon_get_next_level(struct player *p, int dlev, int added)
{
	int target_level, i;

	/* Obtener nivel objetivo */
	target_level = dlev + added * z_info->stair_skip;

	/* No permitir niveles por debajo del máximo */
	if (target_level > z_info->max_depth - 1)
		target_level = z_info->max_depth - 1;

	/* No permitir niveles por encima de la ciudad */
	if (target_level < 0) target_level = 0;

	/* Verificar niveles intermedios para misiones */
	for (i = dlev; i <= target_level; i++) {
		if (is_quest(p, i)) return i;
	}

	return target_level;
}

/**
 * Establecer la profundidad de retorno para un jugador que recuerda desde la ciudad
 */
void player_set_recall_depth(struct player *p)
{
	/* Tener en cuenta el descenso forzado */
	if (OPT(p, birth_force_descend)) {
		/* Forzar descenso a un nivel inferior si está permitido */
		if (p->max_depth < z_info->max_depth - 1
				&& !is_quest(p, p->max_depth)) {
			p->recall_depth = dungeon_get_next_level(p,
				p->max_depth, 1);
		}
	}

	/* Los jugadores que nunca han salido de la ciudad van al nivel 1 */
	p->recall_depth = MAX(p->recall_depth, 1);
}

/**
 * Dar al jugador la opción de nivel persistente al que recordar. Nótese que si
 * se elige un nivel mayor que la profundidad máxima del jugador, vamos silenciosamente
 * a la profundidad máxima.
 */
bool player_get_recall_depth(struct player *p)
{
	bool level_ok = false;
	int new = 0;

	/*
	 * Sin opción cuando no se ha entrado en la mazmorra o el descenso es forzado,
	 * así que no preguntar.
	 */
	if (p->max_depth <= 0 || OPT(p, birth_force_descend)) {
		return true;
	}
	while (!level_ok) {
		const char *prompt =
			"¿A qué nivel deseas volver (0 para cancelar)? ";
		int i;

		/* Elegir el nivel */
		new = get_quantity(prompt, p->max_depth);
		if (new == 0) {
			return false;
		}

		/* ¿Es válido ese nivel? */
		for (i = 0; i < chunk_list_max; i++) {
			if (chunk_list[i]->depth == new) {
				level_ok = true;
				break;
			}
		}
		if (!level_ok) {
			msg("Debes elegir un nivel que hayas visitado anteriormente.");
		}
	}
	p->recall_depth = new;
	return true;
}

/**
 * Cambiar de nivel de mazmorra - ej. subiendo escaleras o con Palabra de Retorno.
 */
void dungeon_change_level(struct player *p, int dlev)
{
	/* Nueva profundidad */
	p->depth = dlev;

	/* Si volvemos a la ciudad, actualizar el contenido de las tiendas
	   según cuánto tiempo hemos estado fuera */
	if (!dlev && daycount)
		store_update();

	/* Salir, hacer nuevo nivel */
	p->upkeep->generate_level = true;

	/* Guardar la partida cuando lleguemos al nuevo nivel. */
	p->upkeep->autosave = true;
}


/**
 * Devuelve cuál sería una cantidad de daño entrante después de aplicar la reducción
 * de daño del jugador.
 *
 * \param p es el jugador de interés.
 * \param dam es la cantidad de daño entrante.
 * \return el daño después de la reducción de daño del jugador, si la hay.
 */
int player_apply_damage_reduction(struct player *p, int dam)
{
	/* Mega-Truco -- Aplicar "in vulnerabilidad" */
	if (p->timed[TMD_INVULN] && (dam < 9000)) return 0;

	dam -= p->state.dam_red;
	if (dam > 0 && p->state.perc_dam_red) {
		dam -= (dam * p->state.perc_dam_red) / 100 ;
	}

	return (dam < 0) ? 0 : dam;
}


/**
 * Disminuye los puntos de golpe del jugador y establece la bandera de muerte si es necesario
 *
 * \param p es el jugador de interés.
 * \param dam es la cantidad de daño a aplicar. Si dam es menor o igual a cero, no se hará nada. La cantidad de daño debería haber sido procesada con player_apply_damage_reduction(); eso no se hace internamente aquí para que la función llamadora pueda mostrar mensajes que incluyan la cantidad de daño.
 * \param kb_str es la cadena terminada en nulo que describe la causa del daño.
 *
 * Esta función permite al usuario guardar (o salir) del juego
 * cuando muere, ya que el mensaje "Moriste." se muestra antes de establecer
 * al jugador como "muerto".
 */
void take_hit(struct player *p, int dam, const char *kb_str)
{
	int old_chp = p->chp;

	int warning = (p->mhp * p->opts.hitpoint_warn / 10);

	/* Paranoia */
	if (p->is_dead || dam <= 0) return;

	/* Molestar */
	disturb(p);

	/* Herir al jugador */
	p->chp -= dam;

	/* Recompensar a los personajes COMBAT_REGEN con maná por los puntos de golpe perdidos
	 * La tarea poco envidiable de separar lo que debería y no debería causar ira
	 * Si eliminamos los casos más explotables debería estar bien.
	 * Todas las trampas y lava actualmente dan maná, lo que podría ser explotado */
	if (player_has(p, PF_COMBAT_REGEN)  && !streq(kb_str, "veneno")
		&& !streq(kb_str, "una herida mortal") && !streq(kb_str, "inanición")) {
		/* perder X% de puntos de golpe obtener X% de puntos de hechizo */
		int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 65536)
			/ (int32_t)p->mhp * dam;
		player_adjust_mana_precise(p, sp_gain);
	}

	/* Mostrar los puntos de golpe */
	p->upkeep->redraw |= (PR_HP);

	/* Jugador muerto */
	if (p->chp < 0) {
		/* Desde el corazón del infierno te apuñalo */
		if (p->timed[TMD_BLOODLUST]
			&& (p->chp + p->timed[TMD_BLOODLUST] + p->lev >= 0)) {
			if (randint0(10)) {
				msg("¡Tu sed de sangre te mantiene con vida!");
			} else {
				msg("Tan grande era su destreza y habilidad en la guerra, que los Elfos decían: ");
				msg("'El Mormegil no puede ser asesinado, salvo por desgracia.'");
			}
		} else {
			/*
			 * Anotar la causa de la muerte. Hacerlo aquí para que los manejadores de EVENT_CHEAT_DEATH
			 * o las cosas que buscan el mensaje "¿Morir? " (el borg, por ejemplo), tengan acceso a ella.
			 */
			my_strcpy(p->died_from, kb_str, sizeof(p->died_from));

			if ((p->wizard || OPT(p, cheat_live))
					&& !get_check("¿Morir? ")) {
				event_signal(EVENT_CHEAT_DEATH);
			} else {
				/* Anotar muerte */
				msgt(MSG_DEATH, "Moriste.");
				event_signal(EVENT_MESSAGE_FLUSH);

				/* Ya no es un ganador */
				p->total_winner = false;

				/* Anotar muerte */
				p->is_dead = true;

				/* Muerto */
				return;
			}
		}
	}

	/* Advertencia de puntos de golpe */
	if (p->chp < warning) {
		/* Tocar campana en el primer aviso */
		if (old_chp > warning)
			bell();

		/* Mensaje */
		msgt(MSG_HITPOINT_WARN, "*** ¡ADVERTENCIA DE PUNTOS DE GOLPE BAJOS! ***");
		event_signal(EVENT_MESSAGE_FLUSH);
	}
}

/**
 * Ganador o no, conocer inventario, objetos del hogar e historia al morir, entrar en puntuación
 */
void death_knowledge(struct player *p)
{
	struct store *home = &stores[f_info[FEAT_HOME].shopnum - 1];
	struct object *obj;
	time_t death_time = (time_t)0;

	/* Retirarse en la ciudad en buen estado */
	if (p->total_winner) {
		p->depth = 0;
		my_strcpy(p->died_from, WINNING_HOW, sizeof(p->died_from));
		p->exp = p->max_exp;
		p->lev = p->max_lev;
		p->au += 10000000L;
	}

	player_learn_all_runes(p);
	for (obj = p->gear; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	for (obj = home->stock; obj; obj = obj->next) {
		object_flavor_aware(p, obj);
		obj->known->effect = obj->effect;
		obj->known->activation = obj->activation;
	}

	history_unmask_unknown(p);

	/* Obtener hora de la muerte */
	(void)time(&death_time);
	enter_score(p, &death_time);

	/* Recalcular bonificaciones */
	p->upkeep->update |= (PU_BONUS);
	handle_stuff(p);
}

/**
 * Energía por movimiento, teniendo en cuenta los movimientos extra
 */
int energy_per_move(struct player *p)
{
	int num = p->state.num_moves;
	int energy = z_info->move_energy;
	return (energy * (1 + ABS(num) - num)) / (1 + ABS(num));
}

/**
 * Modificar un valor de estadística por un "modificador", devolver nuevo valor
 *
 * Las estadísticas suben: 3,4,...,17,18,18/10,18/20,...,18/220
 * O incluso: 18/13, 18/23, 18/33, ..., 18/220
 *
 * Las estadísticas bajan: 18/220, 18/210,..., 18/10, 18, 17, ..., 3
 * O incluso: 18/13, 18/03, 18, 17, ..., 3
 */
int16_t modify_stat_value(int value, int amount)
{
	int i;

	/* Recompensa o penalización */
	if (amount > 0) {
		/* Aplicar cada punto */
		for (i = 0; i < amount; i++) {
			/* Un punto a la vez */
			if (value < 18) value++;

			/* Diez "puntos" a la vez */
			else value += 10;
		}
	} else if (amount < 0) {
		/* Aplicar cada punto */
		for (i = 0; i < (0 - amount); i++) {
			/* Diez puntos a la vez */
			if (value >= 18+10) value -= 10;

			/* Prevenir rarezas */
			else if (value > 18) value = 18;

			/* Un punto a la vez */
			else if (value > 3) value--;
		}
	}

	/* Devolver nuevo valor */
	return (value);
}

/**
 * Intercambiar las estadísticas del jugador al azar, reteniendo información para que puedan ser
 * revertidas a su estado original.
 */
void player_scramble_stats(struct player *p)
{
	int max1, cur1, max2, cur2, i, j, swap;

	/* Algoritmo de mezcla de Fisher-Yates */
	for (i = STAT_MAX - 1; i > 0; --i) {
		j = randint0(i);

		max1 = p->stat_max[i];
		cur1 = p->stat_cur[i];
		max2 = p->stat_max[j];
		cur2 = p->stat_cur[j];

		p->stat_max[i] = max2;
		p->stat_cur[i] = cur2;
		p->stat_max[j] = max1;
		p->stat_cur[j] = cur1;

		/* Registrar lo que hicimos */
		swap = p->stat_map[i];
		assert(swap >= 0 && swap < STAT_MAX);
		p->stat_map[i] = p->stat_map[j];
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		p->stat_map[j] = swap;
	}

	/* Marcar qué más necesita ser actualizado */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Revertir todos los intercambios anteriores de las estadísticas del jugador. No tiene efecto si las
 * estadísticas no han sido intercambiadas.
 */
void player_fix_scramble(struct player *p)
{
	/* Averiguar cuáles deberían ser las estadísticas */
	int new_cur[STAT_MAX];
	int new_max[STAT_MAX];
	int i;

	for (i = 0; i < STAT_MAX; ++i) {
		assert(p->stat_map[i] >= 0 && p->stat_map[i] < STAT_MAX);
		new_cur[p->stat_map[i]] = p->stat_cur[i];
		new_max[p->stat_map[i]] = p->stat_max[i];
	}

	/* Aplicar nuevas estadísticas y reiniciar stat_map */
	for (i = 0; i < STAT_MAX; ++i) {
		p->stat_cur[i] = new_cur[i];
		p->stat_max[i] = new_max[i];
		p->stat_map[i] = i;
	}

	/* Marcar qué más necesita ser actualizado */
	p->upkeep->update |= (PU_BONUS);
}

/**
 * Regenerar el valor de un turno de puntos de golpe
 */
void player_regen_hp(struct player *p)
{
	int32_t hp_gain;
	int percent = 0;/* máx 32k -> 50% de mhp; más exactamente "pertwobytes" */
	int fed_pct, old_chp = p->chp;

	/* Regeneración por defecto */
	if (p->timed[TMD_FOOD] >= PY_FOOD_WEAK) {
		percent = PY_REGEN_NORMAL;
	} else if (p->timed[TMD_FOOD] >= PY_FOOD_FAINT) {
		percent = PY_REGEN_WEAK;
	} else if (p->timed[TMD_FOOD] >= PY_FOOD_STARVE) {
		percent = PY_REGEN_FAINT;
	}

	/* Bonificación por comida - los jugadores mejor alimentados regeneran hasta 1/3 más rápido */
	fed_pct = p->timed[TMD_FOOD] / z_info->food_value;
	percent *= 100 + fed_pct / 3;
	percent /= 100;

	/* Varias cosas aceleran la regeneración */
	if (player_of_has(p, OF_REGEN))
		percent *= 2;
	if (player_resting_can_regenerate(p))
		percent *= 2;

	/* Algunas cosas la ralentizan */
	if (player_of_has(p, OF_IMPAIR_HP))
		percent /= 2;

	/* Varias cosas interfieren con la curación física */
	if (p->timed[TMD_PARALYZED]) percent = 0;
	if (p->timed[TMD_POISONED]) percent = 0;
	if (p->timed[TMD_STUN]) percent = 0;
	if (p->timed[TMD_CUT]) percent = 0;

	/* Extraer los nuevos puntos de golpe */
	hp_gain = (int32_t)(p->mhp * percent) + PY_REGEN_HPBASE;
	player_adjust_hp_precise(p, hp_gain);

	/* Notar cambios */
	if (old_chp != p->chp) {
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_HP);
	}
}


/**
 * Regenerar el valor de un turno de maná
 */
void player_regen_mana(struct player *p)
{
	int32_t sp_gain;
	int percent, old_csp = p->csp;

	/* Guardar los viejos puntos de hechizo */
	old_csp = p->csp;

	/* Regeneración por defecto */
	percent = PY_REGEN_NORMAL;

	/* Varias cosas aceleran la regeneración, pero no deberían castigar a los BG saludables */
	if (!(player_has(p, PF_COMBAT_REGEN) && p->chp  > p->mhp / 2)) {
		if (player_of_has(p, OF_REGEN))
			percent *= 2;
		if (player_resting_can_regenerate(p))
			percent *= 2;
	}

	/* Algunas cosas la ralentizan */
	if (player_has(p, PF_COMBAT_REGEN)) {
		percent /= -2;
	} else if (player_of_has(p, OF_IMPAIR_MANA)) {
		percent /= 2;
	}

	/* Regenerar maná */
	sp_gain = (int32_t)(p->msp * percent);
	if (percent >= 0)
		sp_gain += PY_REGEN_MNBASE;
	sp_gain = player_adjust_mana_precise(p, sp_gain);

	/* La degen de SP cura a los BG al doble de eficiencia que lanzar */
	if (sp_gain < 0  && player_has(p, PF_COMBAT_REGEN)) {
		convert_mana_to_hp(p, -sp_gain * 2);
	}

	/* Notar cambios */
	if (old_csp != p->csp) {
		p->upkeep->redraw |= (PR_MANA);
		equip_learn_flag(p, OF_REGEN);
		equip_learn_flag(p, OF_IMPAIR_MANA);
	}
}

void player_adjust_hp_precise(struct player *p, int32_t hp_gain)
{
	int16_t old_16 = p->chp;
	/* Cargar todo en formato de 4 bytes */
	int32_t old_32 = ((int32_t) old_16) * 65536 + p->chp_frac, new_32;

	/* Comprobar desbordamiento */
	if (hp_gain >= 0) {
		new_32 = (old_32 < INT32_MAX - hp_gain) ?
			old_32 + hp_gain : INT32_MAX;
	} else {
		new_32 = (old_32 > INT32_MIN - hp_gain) ?
			old_32 + hp_gain : INT32_MIN;
	}

	/* Descomponerlo de nuevo */
	if (new_32 < 0) {
		/*
		 * No usar desplazamiento de bits a la derecha en valores negativos: si
		 * los bits de la izquierda son cero o uno depende del sistema.
		 */
		int32_t remainder = new_32 % 65536;

		p->chp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->chp_frac = (uint16_t) (65536 + remainder);
			assert(p->chp > INT16_MIN);
			p->chp -= 1;
		} else {
			p->chp_frac = 0;
		}
	} else {
		p->chp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->chp_frac = (uint16_t)(new_32 & 0xFFFF); /* mod 65536 */
	}

	/* Completamente curado */
	if (p->chp >= p->mhp) {
		p->chp = p->mhp;
		p->chp_frac = 0;
	}

	if (p->chp != old_16) {
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Aceptar un entero con signo de 4 bytes, dividirlo por 65k, y añadirlo
 * a los puntos de hechizo actuales. p->csp y csp_frac tienen 2 bytes cada uno.
 */
int32_t player_adjust_mana_precise(struct player *p, int32_t sp_gain)
{
	int16_t old_16 = p->csp;
	/* Cargar todo en formato de 4 bytes*/
	int32_t old_32 = ((int32_t) p->csp) * 65536 + p->csp_frac, new_32;

	if (sp_gain == 0) return 0;

	/* Comprobar desbordamiento */
	if (sp_gain > 0) {
		if (old_32 < INT32_MAX - sp_gain) {
			new_32 = old_32 + sp_gain;
		} else {
			new_32 = INT32_MAX;
			sp_gain = 0;
		}
	} else if (old_32 > INT32_MIN - sp_gain) {
		new_32 = old_32 + sp_gain;
	} else {
		new_32 = INT32_MIN;
		sp_gain = 0;
	}

	/* Descomponerlo de nuevo*/
	if (new_32 < 0) {
		/*
		 * No usar desplazamiento de bits a la derecha en valores negativos: si
		 * los bits de la izquierda son cero o uno depende del sistema.
		 */
		int32_t remainder = new_32 % 65536;

		p->csp = (int16_t) (new_32 / 65536);
		if (remainder) {
			assert(remainder < 0);
			p->csp_frac = (uint16_t) (65536 + remainder);
			assert(p->csp > INT16_MIN);
			p->csp -= 1;
		} else {
			p->csp_frac = 0;
		}
	} else {
		p->csp = (int16_t)(new_32 >> 16);   /* div 65536 */
		p->csp_frac = (uint16_t)(new_32 & 0xFFFF);    /* mod 65536 */
	}

	/* SP máx/mín */
	if (p->csp >= p->msp) {
		p->csp = p->msp;
		p->csp_frac = 0;
		sp_gain = 0;
	} else if (p->csp < 0) {
		p->csp = 0;
		p->csp_frac = 0;
		sp_gain = 0;
	}

	/* Notar cambios */
	if (old_16 != p->csp) {
		p->upkeep->redraw |= (PR_MANA);
	}

	if (sp_gain == 0) {
		/* Recalcular */
		new_32 = ((int32_t) p->csp) * 65536 + p->csp_frac;
		sp_gain = new_32 - old_32;
	}

	return sp_gain;
}

void convert_mana_to_hp(struct player *p, int32_t sp_long) {
	int32_t hp_gain, sp_ratio;

	if (sp_long <= 0 || p->msp == 0 || p->mhp == p->chp) return;

	/* HP totales desde el máximo */
	hp_gain = ((int32_t)(p->mhp - p->chp)) * 65536;
	hp_gain -= (int32_t)p->chp_frac;

	/* Gastar X% de SP obtener X/2% de HP perdidos. Ej., al 50% HP obtener X/4% */
	/* La ganancia se mantiene baja con msp<10 porque las ganancias de MP son generosas con msp<10 */
	/* sp_ratio es sp máximo a sp gastado, duplicado para ajustarse a la tasa objetivo. */
	sp_ratio = (((int32_t)MAX(10, (int32_t)p->msp)) * 131072) / sp_long;

	/* Limitar la curación máxima al 25% del daño; por lo tanto, gastar > 50% de msp
	 * es ineficiente */
	if (sp_ratio < 4) {sp_ratio = 4;}
	hp_gain /= sp_ratio;

	/* DAVIDTODO Comentarios descriptivos sobre grandes ganancias serían divertidos e informativos */

	player_adjust_hp_precise(p, hp_gain);
}

/**
 * Actualizar el combustible de la luz del jugador
 */
void player_update_light(struct player *p)
{
	/* Verificar si se está usando una luz */
	struct object *obj = equipped_item_by_slot_name(p, "light");

	/* Quemar algo de combustible en la luz actual */
	if (obj && tval_is_light(obj)) {
		bool burn_fuel = true;

		/* Apagar la quema imprudente de luz durante el día en la ciudad */
		if (!p->depth && is_daytime())
			burn_fuel = false;

		/* Si la luz tiene la bandera NO_FUEL, pues... */
		if (of_has(obj->flags, OF_NO_FUEL))
		    burn_fuel = false;

		/* Usar algo de combustible (excepto en artefactos, o durante el día) */
		if (burn_fuel && obj->timeout > 0) {
			/* Disminuir vida útil */
			obj->timeout--;

			/* Notar pasos de combustible interesantes */
			if ((obj->timeout < 100) || (!(obj->timeout % 100)))
				/* Redibujar cosas */
				p->upkeep->redraw |= (PR_EQUIP);

			/* Tratamiento especial cuando está ciego */
			if (p->timed[TMD_BLIND]) {
				/* Guardar algo de luz para después */
				if (obj->timeout == 0) obj->timeout++;
			} else if (obj->timeout == 0) {
				/* La luz ahora se ha apagado */
				disturb(p);
				msg("¡Tu luz se ha apagado!");

				/* Si es una antorcha, ahora es el momento de eliminarla */
				if (of_has(obj->flags, OF_BURNS_OUT)) {
					bool dummy;
					struct object *burnt =
						gear_object_for_use(p, obj, 1,
						false, &dummy);
					if (burnt->known)
						object_delete(p->cave, NULL, &burnt->known);
					object_delete(cave, p->cave, &burnt);
				}
			} else if ((obj->timeout < 50) && (!(obj->timeout % 20))) {
				/* La luz se está volviendo tenue */
				disturb(p);
				msg("Tu luz se está volviendo tenue.");
			}
		}
	}

	/* Calcular radio de la antorcha */
	p->upkeep->update |= (PU_TORCH);
}

/**
 * Encontrar la mejor herramienta de excavación del jugador. Si forbid_stack es true, ignora
 * montones de más de un objeto.
 */
struct object *player_best_digger(struct player *p, bool forbid_stack)
{
	int weapon_slot = slot_by_name(p, "weapon");
	struct object *current_weapon = slot_object(p, weapon_slot);
	struct object *obj, *best = NULL;
	/* Preferir cualquier arma cuerpo a cuerpo sobre la excavación sin armas, ej. best == NULL. */
	int best_score = -1;
	struct player_state local_state;

	for (obj = p->gear; obj; obj = obj->next) {
		int score, old_number;
		if (!tval_is_melee_weapon(obj)) continue;
		if (obj->number < 1 || (forbid_stack && obj->number > 1)) continue;
		/* No usarlo si tiene una maldición pegajosa. */
		if (!obj_can_takeoff(obj)) continue;

		/* Intercambiar temporalmente para el cálculo de calc_bonuses(). */
		old_number = obj->number;
		if (obj != current_weapon) {
			obj->number = 1;
			p->body.slots[weapon_slot].obj = obj;
		}

		/*
		 * Evitar efectos secundarios de usar update establecido a false
		 * con calc_bonuses().
		 */
		local_state.stat_ind[STAT_STR] = 0;
		local_state.stat_ind[STAT_DEX] = 0;
		calc_bonuses(p, &local_state, true, false);
		score = local_state.skills[SKILL_DIGGING];

		/* Intercambiar de vuelta. */
		if (obj != current_weapon) {
			obj->number = old_number;
			p->body.slots[weapon_slot].obj = current_weapon;
		}

		if (score > best_score) {
			best = obj;
			best_score = score;
		}
	}

	return best;
}

/**
 * Atacar cuerpo a cuerpo a un monstruo adyacente aleatorio
 */
bool player_attack_random_monster(struct player *p)
{
	int i, dir = randint0(8);

	/* Los jugadores confundidos tienen un pase libre */
	if (p->timed[TMD_CONFUSED]) return false;

	/* Buscar un monstruo, atacar */
	for (i = 0; i < 8; i++, dir++) {
		struct loc grid = loc_sum(p->grid, ddgrid_ddd[dir % 8]);
		const struct monster *mon = square_monster(cave, grid);
		if (mon && !monster_is_camouflaged(mon)) {
			p->upkeep->energy_use = z_info->move_energy;
			msg("¡Atacas con furia a un enemigo cercano!");
			py_attack(p, grid);
			return true;
		}
	}
	return false;
}

/**
 * Que le sucedan cosas malas aleatorias al jugador por sobreesfuerzo
 *
 * Esta función usa las banderas PY_EXERT_*
 */
void player_over_exert(struct player *p, int flag, int chance, int amount)
{
	if (chance <= 0) return;

	/* Daño a CON */
	if (flag & PY_EXERT_CON) {
		if (randint0(100) < chance) {
			/* Truco - solo permanente con alta probabilidad (lanzamiento sin maná) */
			bool perm = (randint0(100) < chance / 2) && (chance >= 50);
			msg("¡Has dañado tu salud!");
			player_stat_dec(p, STAT_CON, perm);
		}
	}

	/* Desmayo */
	if (flag & PY_EXERT_FAINT) {
		if (randint0(100) < chance) {
			msg("¡Te desmayas por el esfuerzo!");

			/* Omitir acción libre */
			(void)player_inc_timed(p, TMD_PARALYZED,
				randint1(amount), true, true, false);
		}
	}

	/* Estadísticas mezcladas */
	if (flag & PY_EXERT_SCRAMBLE) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_SCRAMBLE,
				randint1(amount), true, true, true);
		}
	}

	/* Daño por cortes */
	if (flag & PY_EXERT_CUT) {
		if (randint0(100) < chance) {
			msg("¡Aparecen heridas en tu cuerpo!");
			(void)player_inc_timed(p, TMD_CUT, randint1(amount),
				true, true, false);
		}
	}

	/* Confusión */
	if (flag & PY_EXERT_CONF) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_CONFUSED,
				randint1(amount), true, true, true);
		}
	}

	/* Alucinación */
	if (flag & PY_EXERT_HALLU) {
		if (randint0(100) < chance) {
			(void)player_inc_timed(p, TMD_IMAGE, randint1(amount),
				true, true, true);
		}
	}

	/* Ralentización */
	if (flag & PY_EXERT_SLOW) {
		if (randint0(100) < chance) {
			msg("De repente te sientes letárgico.");
			(void)player_inc_timed(p, TMD_SLOW, randint1(amount),
				true, true, false);
		}
	}

	/* HP */
	if (flag & PY_EXERT_HP) {
		if (randint0(100) < chance) {
			int dam = player_apply_damage_reduction(p,
				randint1(amount));
			char dam_text[32] = "";

			if (dam > 0 && OPT(p, show_damage)) {
				strnfmt(dam_text, sizeof(dam_text),
					" (%d)", dam);
			}
			msg("¡Gritas de repentino dolor!%s", dam_text);
			take_hit(p, dam, "sobreesfuerzo");
		}
	}
}


/**
 * Ver cuánto daño recibirá el jugador del terreno.
 *
 * \param p es el jugador a verificar
 * \param grid es la ubicación del terreno
 * \param actual, si es true, hará que el jugador aprenda las runas apropiadas
 * si el equipo o los efectos mitigan el daño.
 */
int player_check_terrain_damage(struct player *p, struct loc grid, bool actual)
{
	int dam_taken = 0;

	if (square_isfiery(cave, grid)) {
		int base_dam = 100 + randint1(100);
		int res = p->state.el_info[ELEM_FIRE].res_level;

		/* Daño de fuego */
		dam_taken = adjust_dam(p, ELEM_FIRE, base_dam, RANDOMISE, res,
			actual);

		/* Caída de pluma hace a uno ligero de pies. */
		if (player_of_has(p, OF_FEATHER)) {
			dam_taken /= 2;
			if (actual) {
				equip_learn_flag(p, OF_FEATHER);
			}
		}
	}

	return dam_taken;
}

/**
 * El terreno daña al jugador
 */
void player_take_terrain_damage(struct player *p, struct loc grid)
{
	int dam_taken = player_check_terrain_damage(p, grid, true);
	int dam_reduced;

	if (!dam_taken) {
		return;
	}

	/*
	 * Dañar al jugador y al inventario; el daño al inventario se basa en
	 * el daño entrante bruto y no en el valor que tiene en cuenta la
	 * reducción de daño del jugador.
	 */
	dam_reduced = player_apply_damage_reduction(p, dam_taken);
	if (square_isfiery(cave, grid)) {
		char dam_text[32] = "";

		if (dam_reduced > 0 && OPT(p, show_damage)) {
			strnfmt(dam_text, sizeof(dam_text), " (%d)",
				dam_reduced);
		}
		msg("%s%s", square_feat(cave, grid)->hurt_msg, dam_text);
		inven_damage(p, PROJ_FIRE, dam_taken);
	}
	take_hit(p, dam_reduced, square_feat(cave, grid)->die_msg);
}

/**
 * Encontrar una forma de jugador a partir del nombre
 */
struct player_shape *lookup_player_shape(const char *name)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (streq(shape->name, name)) {
			return shape;
		}
		shape = shape->next;
	}
	msg("¡No se pudo encontrar la forma %s!", name);
	return NULL;
}

/**
 * Encontrar un índice de forma de jugador a partir del nombre de la forma
 */
int shape_name_to_idx(const char *name)
{
	struct player_shape *shape = lookup_player_shape(name);
	if (shape) {
		return shape->sidx;
	} else {
		return -1;
	}
}

/**
 * Encontrar una forma de jugador a partir del índice
 */
struct player_shape *player_shape_by_idx(int index)
{
	struct player_shape *shape = shapes;
	while (shape) {
		if (shape->sidx == index) {
			return shape;
		}
		shape = shape->next;
	}
	msg("¡No se pudo encontrar la forma %d!", index);
	return NULL;
}

/**
 * Dar a los jugadores con forma cambiada la opción de volver a la forma normal y
 * realizar un comando, solo volver a la forma normal sin actuar, o
 * cancelar.
 *
 * \param p el jugador
 * \param cmd el comando que se está realizando
 * \return true si el jugador quiere continuar con su comando
 */
bool player_get_resume_normal_shape(struct player *p, struct command *cmd)
{
	if (player_is_shapechanged(p)) {
		msg("No puedes hacer esto mientras estás en forma de %s.", p->shape->name);
		char prompt[100];
		strnfmt(prompt, sizeof(prompt),
		        "¿Cambiar y %s (s/n) o (v)olver a la forma normal? ",
		        cmd_verb(cmd->code));
		char answer = get_char(prompt, "svn", 3, 'n');

		// Cambiar de vuelta a la forma normal
		if (answer == 's' || answer == 'v') {
			player_resume_normal_shape(p);
		}

		// Los jugadores solo pueden actuar si vuelven a la forma normal
		return answer == 's';
	}

	// Los jugadores en forma normal pueden proceder como siempre
	return true;
}

/**
 * Revertir a la forma normal
 */
void player_resume_normal_shape(struct player *p)
{
	p->shape = lookup_player_shape("normal");
	msg("Retomas tu forma habitual.");

	/* Matar ataque de vampiro */
	(void) player_clear_timed(p, TMD_ATT_VAMP, true, false);

	/* Actualizar */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->redraw |= (PR_TITLE | PR_MISC);
	handle_stuff(p);
}

/**
 * Verificar si el jugador ha cambiado de forma
 */
bool player_is_shapechanged(const struct player *p)
{
	return streq(p->shape->name, "normal") ? false : true;
}

/**
 * Verificar si el jugador es inmune a las trampas
 */
bool player_is_trapsafe(const struct player *p)
{
	if (p->timed[TMD_TRAPSAFE]) return true;
	if (player_of_has(p, OF_TRAP_IMMUNE)) return true;
	return false;
}

/**
 * Devolver true si el jugador puede lanzar un hechizo.
 *
 * \param p es el jugador
 * \param show_msg debe ser true si se debe mostrar un mensaje de fallo.
 */
bool player_can_cast(const struct player *p, bool show_msg)
{
	if (!p->class->magic.total_spells) {
		if (show_msg) {
			msg("No puedes rezar o producir magias.");
		}
		return false;
	}

	if (p->timed[TMD_BLIND] || no_light(p)) {
		if (show_msg) {
			msg("¡No puedes ver!");
		}
		return false;
	}

	if (p->timed[TMD_CONFUSED]) {
		if (show_msg) {
			msg("¡Estás demasiado confundido!");
		}
		return false;
	}

	return true;
}

/**
 * Devolver true si el jugador puede estudiar un hechizo.
 *
 * \param p es el jugador
 * \param show_msg debe ser true si se debe mostrar un mensaje de fallo.
 */
bool player_can_study(const struct player *p, bool show_msg)
{
	if (!player_can_cast(p, show_msg))
		return false;

	if (!p->upkeep->new_spells) {
		if (show_msg) {
			int count;
			struct magic_realm *r = class_magic_realms(p->class, &count), *r1;
			char buf[120];

			my_strcpy(buf, r->spell_noun, sizeof(buf));
			my_strcat(buf, "s", sizeof(buf));
			r1 = r->next;
			mem_free(r);
			r = r1;
			if (count > 1) {
				while (r) {
					count--;
					if (count) {
						my_strcat(buf, ", ", sizeof(buf));
					} else {
						my_strcat(buf, " o ", sizeof(buf));
					}
					my_strcat(buf, r->spell_noun, sizeof(buf));
					my_strcat(buf, "s", sizeof(buf));
					r1 = r->next;
					mem_free(r);
					r = r1;
				}
			}
			msg("¡No puedes aprender ningún %s nuevo!", buf);
		}
		return false;
	}

	return true;
}

/**
 * Devolver true si el jugador puede leer pergaminos o libros.
 *
 * \param p es el jugador
 * \param show_msg debe ser true si se debe mostrar un mensaje de fallo.
 */
bool player_can_read(const struct player *p, bool show_msg)
{
	if (p->timed[TMD_BLIND]) {
		if (show_msg)
			msg("No puedes ver nada.");

		return false;
	}

	if (no_light(p)) {
		if (show_msg)
			msg("No tienes luz para leer.");

		return false;
	}

	if (p->timed[TMD_CONFUSED]) {
		if (show_msg)
			msg("¡Estás demasiado confundido para leer!");

		return false;
	}

	if (p->timed[TMD_AMNESIA]) {
		if (show_msg)
			msg("¡No recuerdas cómo leer!");

		return false;
	}

	return true;
}

/**
 * Devolver true si el jugador puede disparar algo con un lanzador.
 *
 * \param p es el jugador
 * \param show_msg debe ser true si se debe mostrar un mensaje de fallo.
 */
bool player_can_fire(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "shooting");

	/* Requerir un lanzador utilizable */
	if (!obj || !p->state.ammo_tval) {
		if (show_msg)
			msg("No tienes nada con qué disparar.");
		return false;
	}

	return true;
}

/**
 * Devolver true si el jugador puede recargar su fuente de luz.
 *
 * \param p es el jugador
 * \param show_msg debe ser true si se debe mostrar un mensaje de fallo.
 */
bool player_can_refuel(struct player *p, bool show_msg)
{
	struct object *obj = equipped_item_by_slot_name(p, "light");

	if (obj && of_has(obj->flags, OF_TAKES_FUEL)) {
		return true;
	}

	if (show_msg) {
		msg("Tu luz no se puede recargar.");
	}

	return false;
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_cast_prereq(void)
{
	return player_can_cast(player, true);
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_study_prereq(void)
{
	return player_can_study(player, true);
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_read_prereq(void)
{
	/*
	 * Acomodar trucos en otras partes: 'r' está sobrecargado para significar
	 * liberar un monstruo comandado cuando TMD_COMMAND está activo.
	 */
	return (player->timed[TMD_COMMAND]) ?
		true : player_can_read(player, true);
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_fire_prereq(void)
{
	return player_can_fire(player, true);
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_refuel_prereq(void)
{
	return player_can_refuel(player, true);
}

/**
 * Función de requisito previo para comandos. Ver struct cmd_info en ui-input.h y
 * su uso en ui-game.c.
 */
bool player_can_debug_prereq(void)
{
	if (player->noscore & NOSCORE_DEBUG) {
		return true;
	}
	if (confirm_debug()) {
		/* Marcar archivo guardado */
		player->noscore |= NOSCORE_DEBUG;
		return true;
	}
	return false;
}


/**
 * Devolver true si el jugador tiene acceso a un libro que tiene hechizos no aprendidos.
 *
 * \param p es el jugador
 */
bool player_book_has_unlearned_spells(struct player *p)
{
	int i, j;
	int item_max = z_info->pack_size + z_info->floor_size;
	struct object **item_list = mem_zalloc(item_max * sizeof(struct object *));
	int item_num;

	/* Verificar si el jugador puede aprender nuevos hechizos */
	if (!p->upkeep->new_spells) {
		mem_free(item_list);
		return false;
	}

	/* Verificar todos los libros disponibles */
	item_num = scan_items(item_list, item_max, p, USE_INVEN | USE_FLOOR,
		obj_can_study);
	for (i = 0; i < item_num; i++) {
		const struct class_book *book = player_object_to_book(p, item_list[i]);
		if (!book) continue;

		/* Extraer hechizos */
		for (j = 0; j < book->num_spells; j++)
			if (spell_okay_to_study(p, book->spells[j].sidx)) {
				/* Hay un hechizo que el jugador puede estudiar */
				mem_free(item_list);
				return true;
			}
	}

	mem_free(item_list);
	return false;
}

/**
 * Aplicar confusión, si es necesario, a una dirección
 *
 * Mostrar un mensaje y devolver true si la dirección cambia.
 */
bool player_confuse_dir(struct player *p, int *dp, bool too)
{
	int dir = *dp;

	if (p->timed[TMD_CONFUSED]) {
		if ((dir == 5) || (randint0(100) < 75)) {
			/* Dirección aleatoria */
			dir = ddd[randint0(8)];
		}

	/* Los intentos de correr siempre fallan */
	if (too) {
		msg("Estás demasiado confundido.");
		return true;
	}

	if (*dp != dir) {
		msg("Estás confundido.");
		*dp = dir;
		return true;
	}
	}

	return false;
}

/**
 * Devolver true si el recuento proporcionado es uno de los REST_ condicionales.
 */
bool player_resting_is_special(int16_t count)
{
	switch (count) {
		case REST_COMPLETE:
		case REST_ALL_POINTS:
		case REST_SOME_POINTS:
			return true;
	}

	return false;
}

/**
 * Devolver true si el jugador está descansando.
 */
bool player_is_resting(const struct player *p)
{
	return (p->upkeep->resting > 0 ||
			player_resting_is_special(p->upkeep->resting));
}

/**
 * Devolver el número restante de turnos de descanso.
 */
int16_t player_resting_count(const struct player *p)
{
	return p->upkeep->resting;
}

/**
 * Para evitar la bonificación de regeneración de los primeros turnos, tenemos que
 * almacenar el número de turnos que el jugador ha descansado. De lo contrario, los primeros
 * pocos turnos tendrán la bonificación y los últimos no.
 */
static int player_turns_rested = 0;
static bool player_rest_disturb = false;

/**
 * Establecer el número de turnos de descanso.
 *
 * \param p es el jugador que intenta descansar.
 * \param count es el número de turnos a descansar o una de las constantes REST_.
 */
void player_resting_set_count(struct player *p, int16_t count)
{
	/* Cancelar si el jugador es molestado */
	if (player_rest_disturb) {
		p->upkeep->resting = 0;
		player_rest_disturb = false;
		return;
	}

	/* Ignorar si el recuento de descanso es negativo. */
	if ((count < 0) && !player_resting_is_special(count)) {
		p->upkeep->resting = 0;
		return;
	}

	/* Guardar el código de descanso */
	p->upkeep->resting = count;

	/* Truncar valores demasiado grandes */
	if (p->upkeep->resting > 9999) p->upkeep->resting = 9999;
}

/**
 * Cancelar el descanso actual.
 */
void player_resting_cancel(struct player *p, bool disturb)
{
	player_resting_set_count(p, 0);
	player_turns_rested = 0;
	player_rest_disturb = disturb;
}

/**
 * Devolver true si el jugador debería obtener una bonificación de regeneración por el
 * descanso actual.
 */
bool player_resting_can_regenerate(const struct player *p)
{
	return player_turns_rested >= REST_REQUIRED_FOR_REGEN ||
		player_resting_is_special(p->upkeep->resting);
}

/**
 * Realizar un turno de descanso. Esto solo maneja la contabilidad del descanso
 * en sí mismo, y no calcula ningún posible otro efecto del descanso (ver
 * process_world() para la regeneración).
 */
void player_resting_step_turn(struct player *p)
{
	/* Descanso cronometrado */
	if (p->upkeep->resting > 0) {
		/* Reducir contador de descanso */
		p->upkeep->resting--;

		/* Redibujar el estado */
		p->upkeep->redraw |= (PR_STATE);
	}

	/* Gastar un turno */
	p->upkeep->energy_use = z_info->move_energy;

	/* Incrementar los contadores de descanso */
	p->resting_turn++;
	player_turns_rested++;
}

/**
 * Manejar las condiciones para el descanso condicional (descansar con las constantes
 * REST_).
 */
void player_resting_complete_special(struct player *p)
{
	/* Descanso completo */
	if (!player_resting_is_special(p->upkeep->resting)) return;

	if (p->upkeep->resting == REST_ALL_POINTS) {
		if ((p->chp == p->mhp) && (p->csp == p->msp))
			/* Dejar de descansar */
			disturb(p);
	} else if (p->upkeep->resting == REST_COMPLETE) {
		if ((p->chp == p->mhp) &&
			(p->csp == p->msp || player_has(p, PF_COMBAT_REGEN)) &&
			!p->timed[TMD_BLIND] && !p->timed[TMD_CONFUSED] &&
			!p->timed[TMD_POISONED] && !p->timed[TMD_AFRAID] &&
			!p->timed[TMD_TERROR] && !p->timed[TMD_STUN] &&
			!p->timed[TMD_CUT] && !p->timed[TMD_SLOW] &&
			!p->timed[TMD_PARALYZED] && !p->timed[TMD_IMAGE] &&
			!p->word_recall && !p->deep_descent)
			/* Dejar de descansar */
			disturb(p);
	} else if (p->upkeep->resting == REST_SOME_POINTS) {
		if ((p->chp == p->mhp) || (p->csp == p->msp))
			/* Dejar de descansar */
			disturb(p);
	}
}

/* Registrar el último recuento de descanso del jugador para repetir */
static int player_resting_repeat_count = 0;

/**
 * Obtener el número de turnos de descanso a repetir.
 *
 * \param p El jugador actual.
 */
int player_get_resting_repeat_count(struct player *p)
{
	return player_resting_repeat_count;
}

/**
 * Establecer el número de turnos de descanso a repetir.
 *
 * \param p es el jugador que intenta descansar.
 * \param count es el número de turnos solicitados para descansar más recientemente.
 */
void player_set_resting_repeat_count(struct player *p, int16_t count)
{
	player_resting_repeat_count = count;
}

/**
 * Verificar si el estado del jugador tiene la bandera OF_ dada.
 */
bool player_of_has(const struct player *p, int flag)
{
	assert(p);
	return of_has(p->state.flags, flag);
}

/**
 * Verificar si el jugador resiste (o mejor) un elemento
 */
bool player_resists(const struct player *p, int element)
{
	return (p->state.el_info[element].res_level > 0);
}

/**
 * Verificar si el jugador resiste (o mejor) un elemento
 */
bool player_is_immune(const struct player *p, int element)
{
	return (p->state.el_info[element].res_level == 3);
}

/**
 * Coloca al jugador en las coordenadas dadas en la cueva.
 */
void player_place(struct chunk *c, struct player *p, struct loc grid)
{
	assert(!square_monster(c, grid));

	/* Guardar ubicación del jugador */
	p->grid = grid;

	/* Marcar casilla de la cueva */
	square_set_mon(c, grid, -1);

	/* Limpiar creación de escaleras */
	p->upkeep->create_down_stair = false;
	p->upkeep->create_up_stair = false;
}

/*
 * Ocuparse de la contabilidad después de mover al jugador con monster_swap().
 *
 * \param p es el jugador que fue movido.
 * \param eval_trap, si es true, causará la evaluación (posiblemente afectando al
 * jugador) de las trampas en la casilla.
 * \param is_involuntary, si es true, hará acciones apropiadas (vaciar la
 * cola de comandos) para un movimiento no esperado por el jugador.
 */
void player_handle_post_move(struct player *p, bool eval_trap,
		bool is_involuntary)
{
	/* Manejar puertas de tiendas, o notar objetos */
	if (square_isshop(cave, p->grid)) {
		if (player_is_shapechanged(p)) {
			if (square(cave, p->grid)->feat != FEAT_HOME) {
				msg("¡Se oye un grito y la puerta se cierra de golpe!");
			}
			return;
		}
		disturb(p);
		if (is_involuntary) {
			cmdq_flush();
		}
		event_signal(EVENT_ENTER_STORE);
		event_remove_handler_type(EVENT_ENTER_STORE);
		event_signal(EVENT_USE_STORE);
		event_remove_handler_type(EVENT_USE_STORE);
		event_signal(EVENT_LEAVE_STORE);
		event_remove_handler_type(EVENT_LEAVE_STORE);
	} else {
		if (is_involuntary) {
			cmdq_flush();
		}
		square_know_pile(cave, p->grid, NULL);
	}

	/* Descubrir trampas invisibles, activar las visibles */
	if (eval_trap && square_isplayertrap(cave, p->grid)
			&& !square_isdisabledtrap(cave, p->grid)) {
		hit_trap(p->grid, 0);
	}

	/* Actualizar vista y búsqueda */
	update_view(cave, p);
	search(p);
}

/*
 * Algo ha sucedido para molestar al jugador.
 *
 * Toda molestia cancela comandos repetidos, descanso y carrera.
 *
 * XXX-AS: Hacer que los llamadores pasen un comando
 * o llamar a cmd_cancel_repeat dentro de la función que llama a esto
 */
void disturb(struct player *p)
{
	/* Cancelar comandos repetidos */
	cmd_cancel_repeat();

	/* Cancelar Descanso */
	if (player_is_resting(p)) {
		player_resting_cancel(p, true);
		p->upkeep->redraw |= PR_STATE;
	}

	/* Cancelar carrera */
	if (p->upkeep->running) {
		p->upkeep->running = 0;
		mem_free(p->upkeep->steps);
		p->upkeep->steps = NULL;

		/* Cancelar comandos en cola */
		cmdq_flush();

		/* Verificar nuevo panel si corresponde */
		event_signal(EVENT_PLAYERMOVED);
		p->upkeep->update |= PU_TORCH;

		/* Marcar todo el mapa para ser redibujado */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	/* Vaciar entrada */
	event_signal(EVENT_INPUT_FLUSH);
}

/**
 * Buscar trampas o puertas secretas
 */
void search(struct player *p)
{
	struct loc grid;

	/* Varias condiciones significan no buscar */
	if (p->timed[TMD_BLIND] || no_light(p) ||
		p->timed[TMD_CONFUSED] || p->timed[TMD_IMAGE])
		return;

	/* Buscar en las casillas cercanas, que siempre están dentro de los límites */
	for (grid.y = (p->grid.y - 1); grid.y <= (p->grid.y + 1); grid.y++) {
		for (grid.x = (p->grid.x - 1); grid.x <= (p->grid.x + 1); grid.x++) {
			struct object *obj;

			/* Puertas secretas */
			if (square_issecretdoor(cave, grid)) {
				msg("Has encontrado una puerta secreta.");
				place_closed_door(cave, grid);
				disturb(p);
			}

			/* Trampas en cofres */
			for (obj = square_object(cave, grid); obj; obj = obj->next) {
				if (!obj->known || ignore_item_ok(p, obj)
						|| !is_trapped_chest(obj)) {
					continue;
				}

				if (obj->known->pval != obj->pval) {
					msg("¡Has descubierto una trampa en el cofre!");
					obj->known->pval = obj->pval;
					disturb(p);
				}
			}
		}
	}
}

/**
 * Probar si hay algún monstruo que el jugador conozca en el campo de visión.
 */
bool player_has_monster_in_view(const struct player *p)
{
	int n = cave_monster_max(cave), i;

	for (i = 1; i < n; ++i) {
		const struct monster *mon = cave_monster(cave, i);

		if (monster_is_obvious(mon) && monster_is_in_view(mon)) {
			return true;
		}
	}
	return false;
}