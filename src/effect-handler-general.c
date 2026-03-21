/**
 * \archivo effect-handler-general.c
 * \brief Funciones manejadoras para efectos generales
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
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

#include "cave.h"
#include "effect-handler.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "obj-chest.h"
#include "obj-curse.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "source.h"
#include "target.h"
#include "trap.h"


/**
 * Establece el valor para una cadena de efectos
 */
static int set_value = 0;

int effect_calculate_value(effect_handler_context_t *context, bool use_boost)
{
	int final = 0;

	if (set_value) {
		return set_value;
	}

	if (context->value.base > 0 ||
		(context->value.dice > 0 && context->value.sides > 0)) {
		final = context->value.base +
			damroll(context->value.dice, context->value.sides);
	}

	/* Aumento por dispositivo */
	if (use_boost) {
		final *= (100 + context->boost);
		final /= 100;
	}

	return final;
}

/**
 * Adjetivos de estadísticas
 */
static const char *desc_stat(int stat, bool positive)
{
	struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_STAT, stat);
	if (positive) {
		return prop->adjective;
	}
	return prop->neg_adj;
}

/**
 * Comprueba si un monstruo está apuntando a otro monstruo
 */
struct monster *monster_target_monster(effect_handler_context_t *context)
{
	if (context->origin.what == SRC_MONSTER) {
		struct monster *mon = cave_monster(cave, context->origin.which.monster);
		if (!mon) return NULL;
		if (mon->target.midx > 0) {
			struct monster *t_mon = cave_monster(cave, mon->target.midx);
			assert(t_mon);
			return t_mon;
		}
	}
	return NULL;
}

/**
 * Comprueba si una cuadrícula es suficiente para usarla como destino de teletransporte.
 *
 * \param c es el chunk a examinar.
 * \param grid es la cuadrícula a probar.
 * \param is_player_moving es verdadero si un jugador está siendo teletransportado;
 * es falso si un monstruo está siendo teletransportado.
 * \return verdadero si la cuadrícula especificada es suficiente para usarla como destino
 * de teletransporte; de lo contrario, devuelve falso
 *
 * En 4.2.4, los requisitos suficientes eran una cuadrícula de suelo sin jugadores
 * ni monstruos, sin trampas de jugador, sin telarañas y sin objetos. Después de 4.2.4,
 * los requisitos son:
 *     1) transitable pero no dañino ni que active automáticamente una transición
 *         a un nivel o entorno diferente (es decir, una tienda)
 *     2) no tiene ya un jugador o monstruo
 *     3) no tiene telarañas
 *     4) si se mueve un jugador, no tiene trampas de jugador
 *     5) si se mueve un monstruo, no tiene un glifo de protección
 * Hay algo de discusión aquí,
 * https://angband.live/forums/forum/angband/vanilla/10323-the-evil-eye-commands-you-to-return-or-not .
 */
static bool has_teleport_destination_prereqs(struct chunk *c, struct loc grid,
		bool is_player_moving)
{
	if (is_player_moving) {
		if (!square_ispassable(c, grid)) {
			return false;
		}
		if (square_isplayertrap(c, grid)) {
			return false;
		}
	} else {
		if (!square_is_monster_walkable(c, grid)) {
			return false;
		}
		if (square_iswarded(c, grid)) {
			return false;
		}
	}
	if (square(c, grid)->mon
			|| square_isdamaging(c, grid)
			|| square_iswebbed(c, grid)
			|| square_isshop(c, grid)) {
		return false;
	}
	return true;
}

/**
 * Selecciona objetos que tienen al menos una maldición removible.
 */
static bool item_tester_uncursable(const struct object *obj)
{
	struct curse_data *c = obj->known->curses;
	if (c) {
		size_t i;
		for (i = 1; i < z_info->curse_max; i++) {
			if (c[i].power > 0 && c[i].power < 100) {
				return true;
			}
		}
	}
	return false;
}

/**
 * Intenta eliminar una maldición de un objeto.
 */
static bool uncurse_object(struct object *obj, int strength, char *dice_string)
{
	int index = 0;
	int old_weight = obj->number * object_weight_one(obj);
	int new_weight = old_weight;

	if (get_curse(&index, obj, dice_string)) {
		struct curse_data curse = obj->curses[index];
		char o_name[80];

		if (curse.power >= 100) {
			/* La maldición es permanente */
			return false;
		} else if (strength >= curse.power) {
			/* Se eliminó esta maldición con éxito */
			remove_object_curse(obj->known, index, false);
			remove_object_curse(obj, index, true);
			new_weight = obj->number * object_weight_one(obj);
		} else if (!of_has(obj->flags, OF_FRAGILE)) {
			/* Fallo al eliminar, el objeto ahora es frágil */
			object_desc(o_name, sizeof(o_name), obj, ODESC_FULL,
				player);
			msgt(MSG_CURSED, "El hechizo falla; tu %s ahora es frágil.", o_name);
			of_on(obj->flags, OF_FRAGILE);
			player_learn_flag(player, OF_FRAGILE);
		} else if (one_in_(4)) {
			/* Falla - el objeto frágil desafortunado es destruido */
			struct object *destroyed;
			bool none_left = false;
			int dam = damroll(5, 5);
			char dam_text[16] = "";

			dam = player_apply_damage_reduction(player, dam);
			if (dam > 0 && OPT(player, show_damage)) {
				strnfmt(dam_text, sizeof(dam_text), " (%d)",
					dam);
			}
			msg("%s%s", "¡Hay una explosión y un destello!", dam_text);
			if (object_is_carried(player, obj)) {
				destroyed = gear_object_for_use(player, obj,
					1, false, &none_left);
				if (destroyed->artifact) {
					/* Los artefactos se marcan como perdidos */
					history_lose_artifact(player, destroyed->artifact);
				}
				object_delete(player->cave, NULL, &destroyed->known);
				object_delete(cave, player->cave, &destroyed);
			} else {
				square_delete_object(cave, obj->grid, obj, true, true);
			}
			take_hit(player, dam, "Fallo al eliminar maldición");
		} else {
			/* Falla no destructiva */
			msg("La eliminación falla.");
		}
	} else {
		return false;
	}
	player->upkeep->total_weight += new_weight - old_weight;
	player->upkeep->notice |= (PN_COMBINE);
	player->upkeep->update |= (PU_BONUS);
	player->upkeep->redraw |= (PR_EQUIP | PR_INVEN);
	return true;
}

/**
 * Selecciona objetos que tienen al menos una runa desconocida.
 */
static bool item_tester_unknown(const struct object *obj)
{
    return object_runes_known(obj) ? false : true;
}

/**
 * Utilizado por la función enchant() (probabilidad de fallo)
 */
static const int enchant_table[16] =
{
	0, 10,  20, 40, 80,
	160, 280, 400, 550, 700,
	800, 900, 950, 970, 990,
	1000
};

/**
 * Intenta aumentar la puntuación de bonificación de un objeto, si es posible.
 *
 * \returns verdadero si la bonificación aumentó
 */
static bool enchant_score(int16_t *score, bool is_artifact)
{
	int chance;

	/* Los artefactos resisten el encantamiento la mitad de las veces */
	if (is_artifact && randint0(100) < 50) return false;

	/* Calcular la probabilidad de encantar */
	if (*score < 0) chance = 0;
	else if (*score > 15) chance = 1000;
	else chance = enchant_table[*score];

	/* Si tiramos menor o igual que la probabilidad, falla */
	if (randint1(1000) <= chance) return false;

	/* Incrementar la puntuación */
	++*score;

	return true;
}

/**
 * Función auxiliar para enchant() que intenta aumentar las bonificaciones de un objeto
 *
 * \returns verdadero si se aumentó una bonificación
 */
static bool enchant2(struct object *obj, int16_t *score)
{
	bool result = false;
	bool is_artifact = obj->artifact ? true : false;
	if (enchant_score(score, is_artifact)) result = true;
	return result;
}

/**
 * Encantar un objeto
 *
 * ¡Revisado! Ahora toma un puntero al objeto, número de veces para intentar encantar, y una
 * bandera de qué intentar encantar. Los artefactos resisten el encantamiento algunas
 * veces. Además, cualquier intento de encantamiento (incluso si falla) inicia un intento
 * paralelo de eliminar la maldición de un objeto maldito.
 *
 * Ten en cuenta que un objeto técnicamente puede ser encantado hasta +15 si esperas
 * mucho, mucho tiempo. Pasar de +9 a +10 funciona solo alrededor del 5% de las veces,
 * y de +10 a +11 solo alrededor del 1% de las veces.
 *
 * Ten en cuenta que esta función ahora se puede usar en "montones" de objetos, y cuanto
 * más grande es el montón, menor es la probabilidad de éxito.
 *
 * \returns verdadero si el objeto fue modificado de alguna manera
 */
static bool enchant(struct object *obj, int n, int eflag)
{
	int i, prob;
	bool res = false;

	/* Los montones grandes resisten el encantamiento */
	prob = obj->number * 100;

	/* Los proyectiles son fáciles de encantar */
	if (tval_is_ammo(obj)) prob = prob / 20;

	/* Intentar "n" veces */
	for (i = 0; i < n; i++)
	{
		/* Tirada para resistencia del montón */
		if (prob > 100 && randint0(prob) >= 100) continue;

		/* Intentar los tres tipos de encantamiento que podemos hacer */
		if ((eflag & ENCH_TOHIT) && enchant2(obj, &obj->to_h)) res = true;
		if ((eflag & ENCH_TODAM) && enchant2(obj, &obj->to_d)) res = true;
		if ((eflag & ENCH_TOAC)  && enchant2(obj, &obj->to_a)) res = true;
	}

	/* Actualizar conocimiento */
	assert(obj->known);
	obj->known->to_h = obj->to_h;
	obj->known->to_d = obj->to_d;
	obj->known->to_a = obj->to_a;

	/* Falla */
	if (!res) return (false);

	/* Recalcular bonificaciones, equipo */
	player->upkeep->update |= (PU_BONUS | PU_INVEN);

	/* Combinar la mochila (más tarde) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redibujar cosas */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP );

	/* Éxito */
	return (true);
}

/**
 * Encantar un objeto (en el inventario o en el suelo)
 * Ten en cuenta que "num_ac" requiere armadura, de lo contrario arma
 * Devuelve verdadero si se intentó, falso si se canceló
 *
 * Encantar con la bandera TOBOTH intentará encantar
 * tanto to_hit como to_dam con la misma bandera. Esto
 * puede no ser el comportamiento más deseable (ACB).
 */
static bool enchant_spell(int num_hit, int num_dam, int num_ac, struct command *cmd)
{
	bool okay = false;

	struct object *obj;

	char o_name[80];

	const char *q, *s;
	int itemmode = (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR);
	item_tester filter = num_ac ? tval_is_armor : tval_is_weapon;

	/* Obtener un objeto */
	q = "¿Encantar qué objeto? ";
	s = "No tienes nada para encantar.";
	if (cmd) {
		if (cmd_get_item(cmd, "tgtitem", &obj, q, s, filter,
				itemmode)) {
			return false;
		}
	} else if (!get_item(&obj, q, s, 0, filter, itemmode))
		return false;

	/* Descripción */
	object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, player);

	/* Describir */
	msg("%s %s brilla%s intensamente!",
		(object_is_carried(player, obj) ? "Tu" : "El"), o_name,
			   ((obj->number > 1) ? "n" : ""));

	/* Encantar */
	if (num_dam && enchant(obj, num_hit, ENCH_TOBOTH)) okay = true;
	else if (enchant(obj, num_hit, ENCH_TOHIT)) okay = true;
	else if (enchant(obj, num_dam, ENCH_TODAM)) okay = true;
	if (enchant(obj, num_ac, ENCH_TOAC)) okay = true;

	/* Falla */
	if (!okay) {
		event_signal(EVENT_INPUT_FLUSH);

		/* Mensaje */
		msg("El encantamiento falló.");
	}

	/* Algo sucedió */
	return (true);
}

/**
 * Marcar armas (o munición)
 *
 * Convierte el objeto (no mágico) en un objeto-ego de 'brand_type'.
 */
static void brand_object(struct object *obj, const char *name)
{
	int i;
	struct ego_item *ego;
	bool ok = false;

	/* Nunca puedes modificar artefactos, objetos ego u objetos sin valor */
	if (obj && obj->kind->cost && !obj->artifact && !obj->ego) {
		char o_name[80];
		char brand[20];

		object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, player);
		strnfmt(brand, sizeof(brand), "de %s", name);

		/* Describir */
		msg("El %s %s rodeado%s por un aura de %s.", o_name,
			(obj->number > 1) ? "están" : "está",
			(obj->number > 1) ? "s" : "", name);

		/* Obtener el tipo ego correcto para el objeto */
		for (i = 0; i < z_info->e_max; i++) {
			ego = &e_info[i];

			/* Coincidir con el nombre */
			if (!ego->name) continue;
			if (streq(ego->name, brand)) {
				struct poss_item *poss;
				for (poss = ego->poss_items; poss; poss = poss->next)
					if (poss->kidx == obj->kind->kidx)
						ok = true;
			}
			if (ok) break;
		}

		assert(ok);

		/* Convertirlo en un objeto ego */
		obj->ego = &e_info[i];
		ego_apply_magic(obj, 0);
		player_know_object(player, obj);

		/* Actualizar el equipo */
		player->upkeep->update |= (PU_INVEN);

		/* Combinar la mochila (más tarde) */
		player->upkeep->notice |= (PN_COMBINE);

		/* Cosas de ventana */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

		/* Encantar */
		enchant(obj, randint0(3) + 4, ENCH_TOHIT | ENCH_TODAM);
	} else {
		event_signal(EVENT_INPUT_FLUSH);
		msg("El marcado falló.");
	}
}

/**
 * ------------------------------------------------------------------------
 * Manejadores de efectos
 * ------------------------------------------------------------------------ */
/**
 * Efecto ficticio, para indicar al código de efectos que elija uno de los
 * siguientes efectos context->value.base al azar.
 */
bool effect_handler_RANDOM(effect_handler_context_t *context)
{
	return true;
}

/**
 * Alimentar al jugador, o establecer su nivel de saciedad.
 */
bool effect_handler_NOURISH(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	amount *= z_info->food_value;
	if (context->subtype == 0) {
		/* Aumentar el nivel de comida en amount */
		player_inc_timed(player, TMD_FOOD, MAX(amount, 0), false,
			context->origin.what != SRC_PLAYER || !context->aware,
			false);
	} else if (context->subtype == 1) {
		/* Disminuir el nivel de comida en amount */
		player_dec_timed(player, TMD_FOOD, MAX(amount, 0), false,
			context->origin.what != SRC_PLAYER || !context->aware);
	} else if (context->subtype == 2) {
		/* Establecer el nivel de comida a amount, vomitando si es necesario */
		bool message = player->timed[TMD_FOOD] > amount;
		if (message) {
			msg("¡Vomitas!");
		}
		player_set_timed(player, TMD_FOOD, MAX(amount, 0), false,
			context->origin.what != SRC_PLAYER || !context->aware);
	} else if (context->subtype == 3) {
		/* Aumentar el nivel de comida a amount si es necesario */
		if (player->timed[TMD_FOOD] < amount) {
			player_set_timed(player, TMD_FOOD, MAX(amount + 1, 0),
				false, context->origin.what != SRC_PLAYER
				|| !context->aware);
		}
	} else {
		return false;
	}
	context->ident = true;
	return true;
}

bool effect_handler_CRUNCH(effect_handler_context_t *context)
{
	if (one_in_(2))
		msg("Está crujiente.");
	else
		msg("¡Casi te rompe un diente!");
	context->ident = true;
	return true;
}

/**
 * Curar una condición de estado del jugador.
 */
bool effect_handler_CURE(effect_handler_context_t *context)
{
	int type = context->subtype;
	(void) player_clear_timed(player, type, true,
		context->origin.what != SRC_PLAYER || !context->aware);
	context->ident = true;
	return true;
}

/**
 * Establecer una condición de estado (positiva o negativa) del jugador.
 */
bool effect_handler_TIMED_SET(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	player_set_timed(player, context->subtype, MAX(amount, 0), true,
		context->origin.what != SRC_PLAYER || !context->aware);
	context->ident = true;
	return true;

}

/**
 * Extender una condición de estado (positiva o negativa) del jugador.
 * Si context->other está establecido, aumentar en esa cantidad si el jugador ya
 * tiene el estado
 */
bool effect_handler_TIMED_INC(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	struct monster *t_mon = monster_target_monster(context);
	struct loc decoy = cave_find_decoy(cave);

	context->ident = true;

	/* Destruir señuelo si es un ataque de monstruo */
	if (cave->mon_current > 0 && decoy.y && decoy.x) {
		square_destroy_decoy(cave, decoy);
		return true;
	}

	/* Comprobar si un monstruo apunta a otro monstruo */
	if (t_mon) {
		int mon_tmd_effect = -1;

		/* Funcionará hasta que se fusionen los efectos temporizados de monstruos y jugadores */
		switch (context->subtype) {
			case TMD_CONFUSED: {
				mon_tmd_effect = MON_TMD_CONF;
				break;
			}
			case TMD_SLOW: {
				mon_tmd_effect = MON_TMD_SLOW;
				break;
			}
			case TMD_PARALYZED: {
				mon_tmd_effect = MON_TMD_HOLD;
				break;
			}
			case TMD_BLIND: {
				mon_tmd_effect = MON_TMD_STUN;
				break;
			}
			case TMD_AFRAID: {
				mon_tmd_effect = MON_TMD_FEAR;
				break;
			}
			case TMD_AMNESIA: {
				mon_tmd_effect = MON_TMD_SLEEP;
				break;
			}
			default: {
				break;
			}
		}
		if (mon_tmd_effect >= 0) {
			mon_inc_timed(t_mon, mon_tmd_effect, MAX(amount, 0), 0);
		}
		return true;
	}

	if (!player->timed[context->subtype] || !context->other) {
		player_inc_timed(player, context->subtype, MAX(amount, 0), true,
			context->origin.what != SRC_PLAYER || !context->aware,
			true);
	} else {
		player_inc_timed(player, context->subtype, context->other, true,
			context->origin.what != SRC_PLAYER || !context->aware,
			true);
	}
	return true;
}

/**
 * Extender una condición de estado (positiva o negativa) del jugador sin resistencia.
 * Si context->other está establecido, aumentar en esa cantidad si el jugador ya
 * tiene el estado
 */
bool effect_handler_TIMED_INC_NO_RES(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);

	if (!player->timed[context->subtype] || !context->other)
		player_inc_timed(player, context->subtype, MAX(amount, 0),
			true,
			context->origin.what != SRC_PLAYER || !context->aware,
			false);
	else
		player_inc_timed(player, context->subtype, context->other, true,
			context->origin.what != SRC_PLAYER || !context->aware,
			false);
	context->ident = true;
	return true;
}

/**
 * Extender una condición de estado (positiva o negativa) de un monstruo.
 */
bool effect_handler_MON_TIMED_INC(effect_handler_context_t *context)
{
	assert(context->origin.what == SRC_MONSTER);

	int amount = effect_calculate_value(context, false);
	struct monster *mon = cave_monster(cave, context->origin.which.monster);

	if (mon) {
		mon_inc_timed(mon, context->subtype, MAX(amount, 0), 0);
		context->ident = true;
	}

	return true;
}

/**
 * Reducir una condición de estado (positiva o negativa) del jugador.
 * Si context->other está establecido, disminuir por el valor actual / context->other
 */
bool effect_handler_TIMED_DEC(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	if (context->other)
		amount = player->timed[context->subtype] / context->other;
	(void) player_dec_timed(player, context->subtype, MAX(amount, 0), true,
		context->origin.what != SRC_PLAYER || !context->aware);
	context->ident = true;
	return true;
}

/**
 * Crear un glifo.
 */
bool effect_handler_GLYPH(effect_handler_context_t *context)
{
	struct loc decoy = cave_find_decoy(cave);

	/* Siempre notar */
	context->ident = true;

	/* Solo un señuelo a la vez */
	if (!loc_is_zero(decoy) && (context->subtype == GLYPH_DECOY)) {
		msg("Solo puedes desplegar un señuelo a la vez.");
		return false;
	}

	/* Ver si el efecto funciona */
	if (!square_istrappable(cave, player->grid)) {
		msg("No hay suelo despejado para lanzar el hechizo.");
		return false;
	}

	/* Empujar objetos fuera de la cuadrícula */
	if (square_object(cave, player->grid))
		push_object(player->grid);

	/* Crear un glifo */
	square_add_glyph(cave, player->grid, context->subtype);

	return true;
}

/**
 * Crear una telaraña.
 */
bool effect_handler_WEB(effect_handler_context_t *context)
{
	int rad = 1;
	struct monster *mon = NULL;
	struct loc grid;

	/* Obtener el monstruo creador */
	if (cave->mon_current > 0) {
		mon = cave_monster(cave, cave->mon_current);
	} else {
		/* El jugador no puede crear telarañas actualmente */
		return false;
	}

	/* Siempre notar */
	context->ident = true;

	/* Aumentar el radio para mayor poder de hechizo */
	if (mon->race->spell_power > 40) rad++;
	if (mon->race->spell_power > 80) rad++;

	/* Comprobar dentro del radio si hay suelo despejado */
	for (grid.y = mon->grid.y - rad; grid.y <= mon->grid.y + rad; grid.y++) {
		for (grid.x = mon->grid.x - rad; grid.x <= mon->grid.x + rad; grid.x++){
			if (distance(grid, mon->grid) > rad ||
				!square_in_bounds_fully(cave, grid)) continue;

			/* Requiere una cuadrícula de suelo sin trampas ni glifos existentes */
			if (!square_iswebbable(cave, grid)) continue;

			/* Crear una telaraña */
			square_add_web(cave, grid);
		}
	}

	return true;
}

/**
 * Restaurar una estadística; el índice de estadística es context->subtype
 */
bool effect_handler_RESTORE_STAT(effect_handler_context_t *context)
{
	int stat = context->subtype;

	/* ID */
	context->ident = true;

	/* Comprobar límites */
	if (stat < 0 || stat >= STAT_MAX) return false;

	/* No es necesario */
	if (player->stat_cur[stat] == player->stat_max[stat])
		return true;

	/* Restaurar */
	player->stat_cur[stat] = player->stat_max[stat];

	/* Recalcular bonificaciones */
	player->upkeep->update |= (PU_BONUS);
	update_stuff(player);

	/* Mensaje */
	msg("Te sientes menos %s.", desc_stat(stat, false));

	return (true);
}

/**
 * Drenar una estadística temporalmente. El índice de estadística es context->subtype.
 */
bool effect_handler_DRAIN_STAT(effect_handler_context_t *context)
{
	int stat = context->subtype;
	int flag = sustain_flag(stat);

	/* Comprobar límites */
	if (flag < 0) return false;

	/* ID */
	context->ident = true;

	/* Sostener */
	if (player_of_has(player, flag)) {
		/* Notificar efecto */
		equip_learn_flag(player, flag);

		/* Mensaje */
		msg("Te sientes muy %s por un momento, pero la sensación pasa.",
				   desc_stat(stat, false));

		return (true);
	}

	/* Intentar reducir la estadística */
	if (player_stat_dec(player, stat, false)){
		int dam = effect_calculate_value(context, false);
		char dam_text[32] = "";

		dam = player_apply_damage_reduction(player, dam);

		/* Notificar efecto */
		equip_learn_flag(player, flag);

		/* Mensaje */
		if (dam > 0 && OPT(player, show_damage)) {
			strnfmt(dam_text, sizeof(dam_text), " (%d)", dam);
		}
		msgt(MSG_DRAIN_STAT, "Te sientes muy %s.%s",
			desc_stat(stat, false), dam_text);
		take_hit(player, dam, "drenaje de estadística");
	}

	return (true);
}

/**
 * Perder un punto de estadística permanentemente, en una estadística diferente a la
 * especificada en context->subtype.
 */
bool effect_handler_LOSE_RANDOM_STAT(effect_handler_context_t *context)
{
	int safe_stat = context->subtype;
	int loss_stat = randint1(STAT_MAX - 1);

	/* Evitar la estadística segura */
	loss_stat = (loss_stat + safe_stat) % STAT_MAX;

	/* Intentar reducir la estadística */
	if (player_stat_dec(player, loss_stat, true)) {
		msgt(MSG_DRAIN_STAT, "Te sientes muy %s.", desc_stat(loss_stat, false));
	}

	/* ID */
	context->ident = true;

	return (true);
}


/**
 * Ganar un punto de estadística. El índice de estadística es context->subtype.
 */
bool effect_handler_GAIN_STAT(effect_handler_context_t *context)
{
	int stat = context->subtype;

	/* Intentar aumentar */
	if (player_stat_inc(player, stat)) {
		msg("¡Te sientes muy %s!", desc_stat(stat, true));
	}

	/* Notificar */
	context->ident = true;

	return (true);
}

/**
 * Restaura cualquier experiencia drenada
 */
bool effect_handler_RESTORE_EXP(effect_handler_context_t *context)
{
	/* Restaurar experiencia */
	if (player->exp < player->max_exp) {
		/* Mensaje */
		if (context->origin.what != SRC_NONE)
			msg("Sientes que tus energías vitales regresan.");
		player_exp_gain(player, player->max_exp - player->exp);

		/* Recalcular puntos de golpe máximos */
		update_stuff(player);
	}

	/* Algo sucedió */
	context->ident = true;

	return (true);
}

/* Notar el divisor de 2, un pequeño truco para simplificar la descripción de comida */
bool effect_handler_GAIN_EXP(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	if (player->exp < PY_MAX_EXP) {
		msg("Te sientes más experimentado.");
		player_exp_gain(player, amount / 2);
	}
	context->ident = true;

	return true;
}

/**
 * Drenar algo de luz de la fuente de luz del jugador, si es posible
 */
bool effect_handler_DRAIN_LIGHT(effect_handler_context_t *context)
{
	int drain = effect_calculate_value(context, false);

	int light_slot = slot_by_name(player, "light");
	struct object *obj = slot_object(player, light_slot);

	if (obj && !of_has(obj->flags, OF_NO_FUEL) && (obj->timeout > 0)) {
		/* Reducir combustible */
		obj->timeout -= drain;
		if (obj->timeout < 1) obj->timeout = 1;

		/* Notificar */
		if (!player->timed[TMD_BLIND]) {
			msg("Tu luz se atenúa.");
			context->ident = true;
		}

		/* Redibujar cosas */
		player->upkeep->redraw |= (PR_EQUIP);
	}

	return true;
}

/**
 * Drenar mana del jugador, sanando al lanzador.
 */
bool effect_handler_DRAIN_MANA(effect_handler_context_t *context)
{
	int drain = effect_calculate_value(context, false);
	bool monster = context->origin.what != SRC_TRAP;
	char m_name[80];
	struct monster *mon = NULL;
	struct monster *t_mon = monster_target_monster(context);
	struct loc decoy = cave_find_decoy(cave);

	context->ident = true;

	if (monster) {
		assert(context->origin.what == SRC_MONSTER);

		mon = cave_monster(cave, context->origin.which.monster);

		/* Obtener el nombre del monstruo (o "eso") */
		monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
	}

	/* El objetivo es otro monstruo - desencantarlo */
	if (t_mon) {
		mon_inc_timed(t_mon, MON_TMD_DISEN, MAX(drain, 0), 0);
		return true;
	}

	/* El objetivo era un señuelo - destruirlo */
	if (decoy.y && decoy.x) {
		square_destroy_decoy(cave, decoy);
		return true;
	}

	/* El jugador no tiene mana */
	if (!player->csp) {
		msg("El drenaje falla.");
		if (monster) {
			update_smart_learn(mon, player, 0, PF_NO_MANA, -1);
		}
		return true;
	}

	/* Drenar la cantidad dada si el jugador tiene esa cantidad, o todo */
	if (drain >= player->csp) {
		drain = player->csp;
		player->csp = 0;
		player->csp_frac = 0;
	} else {
		player->csp -= drain;
	}

	/* Sanar al monstruo */
	if (monster) {
		if (mon->hp < mon->maxhp) {
			mon->hp += (6 * drain);
			if (mon->hp > mon->maxhp)
				mon->hp = mon->maxhp;

			/* Redibujar (más tarde) si es necesario */
			if (player->upkeep->health_who == mon)
				player->upkeep->redraw |= (PR_HEALTH);

			/* Mensaje especial */
			if (monster_is_visible(mon))
				msg("%s parece más saludable.", m_name);
		}
	}

	/* Redibujar mana */
	player->upkeep->redraw |= PR_MANA;

	return true;
}

bool effect_handler_RESTORE_MANA(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	if (!amount) amount = player->msp;
	if (player->csp < player->msp) {
		player->csp += amount;
		if (player->csp > player->msp) {
			player->csp = player->msp;
			player->csp_frac = 0;
			msg("Sientes que tu cabeza se despeja.");
		} else
			msg("Sientes que tu cabeza se despeja un poco.");
		player->upkeep->redraw |= (PR_MANA);
	}
	context->ident = true;

	return true;
}

/**
 * Intentar eliminar una maldición de un objeto
 */
bool effect_handler_REMOVE_CURSE(effect_handler_context_t *context)
{
	const char *prompt = "¿Eliminar maldición de qué objeto? ";
	const char *rejmsg = "No tienes maldiciones para eliminar.";
	int itemmode = (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR);
	int strength = effect_calculate_value(context, false);
	struct object *obj = NULL;
	char dice_string[20];

	context->ident = true;

	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, prompt,
				rejmsg, item_tester_uncursable, itemmode)) {
			return false;
		}
	} else if (!get_item(&obj, prompt, rejmsg, 0, item_tester_uncursable,
			itemmode))
		return false;

	/* Obtener las posibles cadenas de dados */
	if ((context->value.dice == 1) && context->value.base) {
		strnfmt(dice_string, sizeof(dice_string), "%d+d%d",
				context->value.base, context->value.sides);
	} else if (context->value.dice && context->value.base) {
		strnfmt(dice_string, sizeof(dice_string), "%d+%dd%d",
				context->value.base, context->value.dice, context->value.sides);
	} else if (context->value.dice == 1) {
		strnfmt(dice_string, sizeof(dice_string), "d%d", context->value.sides);
	} else if (context->value.dice) {
		strnfmt(dice_string, sizeof(dice_string), "%dd%d",
				context->value.dice, context->value.sides);
	} else {
		strnfmt(dice_string, sizeof(dice_string), "%d", context->value.base);
	}

	return uncurse_object(obj, strength, dice_string);
}

/**
 * Establecer palabra de retorno según corresponda
 */
bool effect_handler_RECALL(effect_handler_context_t *context)
{
	int target_depth;
	context->ident = true;

	/* Sin retorno */
	if (OPT(player, birth_no_recall) && !player->total_winner) {
		msg("No pasa nada.");
		return true;
	}

	/* Sin retorno desde niveles de misión con force_descend */
	if (OPT(player, birth_force_descend)
			&& is_quest(player, player->depth)) {
		msg("No pasa nada.");
		return true;
	}

	/* Sin retorno desde combate singular */
	if (player->upkeep->arena_level) {
		msg("No pasa nada.");
		return true;
	}

	/* Advertir al jugador si está descendiendo a un nivel no retornable */
	target_depth = dungeon_get_next_level(player, player->max_depth, 1);
	if (OPT(player, birth_force_descend) && !(player->depth)
			&& is_quest(player, target_depth)) {
		if (!get_check("¿Estás seguro de que quieres descender? ")) {
			return false;
		}
	}

	/* Activar retorno */
	if (!player->word_recall) {
		/* Restablecer profundidad de retorno */
		if (player->depth > 0) {
			if (player->depth != player->max_depth
					&& !OPT(player, birth_levels_persist)) {
				if (get_check("¿Establecer profundidad de retorno a la profundidad actual? ")) {
					player->recall_depth = player->max_depth = player->depth;
				}
			} else {
				player->recall_depth = player->max_depth;
			}
		} else {
			if (OPT(player, birth_levels_persist)) {
				/* Los jugadores con niveles persistentes pueden elegir */
				if (!player_get_recall_depth(player)) return false;
			}
		}

		player->word_recall = randint0(20) + 15;
		msg("El aire a tu alrededor se carga...");
	} else {
		/* Desactivar retorno */
		if (!get_check("Palabra de Retorno ya está activa. ¿Quieres cancelarla? "))
			return false;

		player->word_recall = 0;
		msg("Una tensión abandona el aire a tu alrededor...");
	}

	/* Redibujar línea de estado */
	player->upkeep->redraw |= PR_STATUS;
	handle_stuff(player);

	return true;
}

bool effect_handler_DEEP_DESCENT(effect_handler_context_t *context)
{
	/* Calcular profundidad objetivo */
	int target_increment = (4 / z_info->stair_skip) + 1;
	int target_depth = dungeon_get_next_level(player, player->max_depth,
		target_increment);

	if (target_depth > player->depth) {
		msgt(MSG_TPLEVEL, "El aire a tu alrededor comienza a arremolinarse...");
		player->deep_descent = 3 + randint1(4);

		/* Redibujar línea de estado */
		player->upkeep->redraw |= PR_STATUS;
		handle_stuff(player);
	} else {
		msgt(MSG_TPLEVEL, "Sientes una presencia malévola bloqueando el paso a los niveles inferiores.");
	}
	context->ident = true;
	return true;
}

bool effect_handler_ALTER_REALITY(effect_handler_context_t *context)
{
	/* No permitir en arenas de combate singular. */
	if (player->upkeep->arena_level) return true;
	msg("¡El mundo cambia!");
	dungeon_change_level(player, player->depth);
	context->ident = true;
	return true;
}

/**
 * Mapear un área alrededor de un punto, generalmente el jugador.
 * La altura a mapear arriba y abajo del jugador es context->y,
 * el ancho a cada lado del jugador es context->x.
 * Para áreas dependientes del nivel del jugador, usamos el truco de aplicar los dados de valor
 * y las caras como la altura y el ancho.
 */
bool effect_handler_MAP_AREA(effect_handler_context_t *context)
{
	int i, x, y;
	int x1, x2, y1, y2;
	int dist_y = context->y ? context->y : context->value.dice;
	int dist_x = context->x ? context->x : context->value.sides;
	struct loc centre = origin_get_loc(context->origin);

	/* Elegir un área para mapear */
	y1 = centre.y - dist_y;
	y2 = centre.y + dist_y;
	x1 = centre.x - dist_x;
	x2 = centre.x + dist_x;

	/* Ajustar las coordenadas a la mazmorra */
	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear la mazmorra */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			struct loc grid = loc(x, y);

			/* Algunos cuadrados no se pueden mapear */
			if (square_isno_map(cave, grid)) continue;

			/* Todos los no-muros están "marcados" */
			if (!square_seemslikewall(cave, grid)) {
				if (!square_in_bounds_fully(cave, grid)) continue;

				/* Memorizar características normales */
				if (!square_isfloor(cave, grid))
					square_memorize(cave, grid);

				/* Memorizar muros conocidos */
				for (i = 0; i < 8; i++) {
					int yy = y + ddy_ddd[i];
					int xx = x + ddx_ddd[i];

					/* Memorizar muros (etc) */
					if (square_seemslikewall(cave, loc(xx, yy)))
						square_memorize(cave, loc(xx, yy));
				}
			}

			/*
			 * Olvidar cuadrículas que no están procesadas y
			 * mal recordadas en el área de mapeo.
			 */
			if (square_ismemorybad(cave, grid)) {
				square_forget(cave, grid);
			}
		}
	}

	/* Desmarcar cuadrículas */
	for (y = y1 - 1; y < y2 + 1; y++) {
		for (x = x1 - 1; x < x2 + 1; x++) {
			struct loc grid = loc(x, y);
			if (!square_in_bounds(cave, grid)) continue;
			square_unmark(cave, grid);
		}
	}

	/* Actualizar completamente los elementos visuales */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redibujar todo el mapa, lista de monstruos */
	player->upkeep->redraw |= (PR_MAP | PR_MONLIST | PR_ITEMLIST);

	/* Notificar */
	context->ident = true;

	return true;
}

/**
 * Mapear un área alrededor de los monstruos detectados recientemente.
 * La altura a mapear arriba y abajo de cada monstruo es context->y,
 * el ancho a cada lado de cada monstruo es context->x.
 * Para áreas dependientes del nivel del jugador, usamos el truco de aplicar los dados de valor
 * y las caras como la altura y el ancho.
 */
bool effect_handler_READ_MINDS(effect_handler_context_t *context)
{
	int i;
	int dist_y = context->y ? context->y : context->value.dice;
	int dist_x = context->x ? context->x : context->value.sides;
	bool found = false;

	/* Escanear monstruos */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Saltar monstruos muertos */
		if (!mon->race) continue;

		/* Detectar todos los monstruos apropiados */
		if (mflag_has(mon->mflag, MFLAG_MARK)) {
			/* Mapear alrededor */
			effect_simple(EF_MAP_AREA, source_monster(i), "0", 0, 0, 0,
						  dist_y, dist_x, NULL);
			found = true;
		}
	}

	if (found) {
		msg("¡Se forman imágenes en tu mente!");
		context->ident = true;
	}

	return true;
}

/**
 * Detectar trampas alrededor del jugador. La altura a detectar arriba y abajo del
 * jugador es context->y, el ancho a cada lado del jugador es context->x.
 */
bool effect_handler_DETECT_TRAPS(effect_handler_context_t *context)
{
	int x, y;
	int x1, x2, y1, y2;

	bool detect = false;

	struct object *obj;

	/* Elegir un área para detectar */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;


	/* Escanear la mazmorra */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			struct loc grid = loc(x, y);

			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Detectar trampas */
			if (square_isplayertrap(cave, grid))
				/* Revelar trampa */
				if (square_reveal_trap(cave, grid, true, false))
					detect = true;

			/* Escanear todos los objetos en la cuadrícula para buscar trampas en cofres */
			for (obj = square_object(cave, grid); obj; obj = obj->next) {
				/* Saltar cualquier cosa que no sea un cofre con trampa */
				if (!is_trapped_chest(obj)
						|| ignore_item_ok(player, obj)) {
					continue;
				}

				/* Identificar una vez */
				if (!obj->known || obj->known->pval != obj->pval) {
					/* Truco - ver el objeto */
					object_see(player, obj);

					/* Conocer la trampa */
					obj->known->pval = obj->pval;

					/* Encontramos algo que detectar */
					detect = true;
				}
			}
			/* Marcar como área con trampas detectadas */
			sqinfo_on(square(cave, loc(x, y))->info, SQUARE_DTRAP);
		}
	}

	/* Describir */
	if (detect)
		msg("¡Sientes la presencia de trampas!");

	/* La detección de trampas siempre te hace consciente, incluso si no hay trampas */
	else
		msg("No sientes trampas.");

	/* Notificar */
	context->ident = true;

	return true;
}

/**
 * Detectar puertas alrededor del jugador. La altura a detectar arriba y abajo del
 * jugador es context->y, el ancho a cada lado del jugador es context->x.
 */
bool effect_handler_DETECT_DOORS(effect_handler_context_t *context)
{
	int x, y;
	int x1, x2, y1, y2;

	bool doors = false;

	/* Elegir un área para detectar */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear la mazmorra */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			struct loc grid = loc(x, y);

			if (!square_in_bounds_fully(cave, grid)) continue;

			if (square_issecretdoor(cave, grid)) {
				/* Detectar puertas secretas */
				/* Colocar una puerta real */
				place_closed_door(cave, grid);

				/* Memorizar */
				square_memorize(cave, grid);
				square_light_spot(cave, grid);

				/* Obvio */
				doors = true;
			} else if (square_isdoor(cave, grid)) {
				/* Detectar otros tipos de puertas. */
				if (square_ismemorybad(cave, grid)) {
					square_memorize(cave, grid);
					square_light_spot(cave, grid);
					doors = true;
				}
			} else if (square_isdoor(player->cave, grid)
					&& square_ismemorybad(cave, grid)) {
				/*
				 * Olvidar puertas mal recordadas en el área
				 * de mapeo.
				 */
				square_forget(cave, grid);
			}
		}
	}

	/* Describir */
	if (doors)
		msg("¡Sientes la presencia de puertas!");
	else if (context->aware)
		msg("No sientes puertas.");

	context->ident = true;

	return true;
}

/**
 * Detectar escaleras alrededor del jugador. La altura a detectar arriba y abajo del
 * jugador es context->y, el ancho a cada lado del jugador es context->x.
 */
bool effect_handler_DETECT_STAIRS(effect_handler_context_t *context)
{
	int x, y;
	int x1, x2, y1, y2;

	bool stairs = false;

	/* Elegir un área para detectar */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear la mazmorra */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			struct loc grid = loc(x, y);

			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Detectar escaleras */
			if (square_isstairs(cave, grid)) {
				/* Memorizar */
				square_memorize(cave, grid);
				square_light_spot(cave, grid);

				/* Obvio */
				stairs = true;
			}
		}
	}

	/* Describir */
	if (stairs)
		msg("¡Sientes la presencia de escaleras!");
	else if (context->aware)
		msg("No sientes escaleras.");

	context->ident = true;
	return true;
}


/**
 * Detectar oro enterrado alrededor del jugador. La altura a detectar arriba y abajo
 * del jugador es context->y, el ancho a cada lado del jugador es context->x.
 */
bool effect_handler_DETECT_ORE(effect_handler_context_t *context)
{
	int x, y;
	int x1, x2, y1, y2;

	bool gold_buried = false;

	/* Elegir un área para detectar */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear la mazmorra */
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			struct loc grid = loc(x, y);

			if (!square_in_bounds_fully(cave, grid)) continue;

			/* Magma/Cuarzo + Oro conocido */
			if (square_hasgoldvein(cave, grid)) {
				/* Memorizar */
				square_memorize(cave, grid);
				square_light_spot(cave, grid);

				/* Detectar */
				gold_buried = true;
			} else if (square_hasgoldvein(player->cave, grid)) {
				/* Algo eliminado visto previamente o
				 * oro enterrado detectado. Notar el cambio. */
				square_forget(cave, grid);
			}
		}
	}

	/* Mensaje a menos que estemos detectando silenciosamente */
	if (context->origin.what != SRC_NONE) {
		if (gold_buried) {
			msg("¡Sientes la presencia de tesoro enterrado!");
		} else if (context->aware) {
			msg("No sientes tesoro enterrado.");
		}
	}

	context->ident = true;
	return true;
}

/**
 * Ayuda para effect_handler_SENSE_GOLD() o effect_handler_SENSE_OBJECTS(): sentir
 * objetos de una clase determinada alrededor del jugador. El rango de detección en y
 * está dentro de context->y del jugador. El rango de detección en x está
 * dentro de context->x del jugador.
 */
static bool sense_stuff(effect_handler_context_t *context,
		bool (*pred)(const struct object*),
		const struct object_kind *unknown_kind)
{
	int x, y;
	int x1, x2, y1, y2;

	bool have_stuff = false;

	/* Elegir un área para sentir */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear el área */
	for (y = y1; y <= y2; y++) {
		for (x = x1; x <= x2; x++) {
			struct loc grid = loc(x, y);
			struct object *obj = square_object(cave, grid);

			for (; !have_stuff && obj; obj = obj->next) {
				if ((*pred)(obj)
						&& (!obj->known
						|| obj->known->kind == unknown_kind
						|| !ignore_item_ok(player, obj))) {
					have_stuff = true;
				}
			}

			/*
			 * Tomar conciencia de las partes del montón que coinciden
			 * con el predicado. Olvidar partes recordadas que coinciden
			 * con el predicado que ya no están allí.
			 */
			square_sense_pile(cave, grid, pred);
		}
	}

	return have_stuff;
}

/**
 * Ayuda para effect_handler_DETECT_GOLD() y effect_handler_DETECT_OBJECTS():
 * detectar objetos de una clase determinada alrededor del jugador. El rango de detección
 * en y está dentro de context->y del jugador. El rango de detección en x está
 * dentro de context->x del jugador.
 */
static bool detect_stuff(effect_handler_context_t *context,
		bool (*pred)(const struct object*))
{
	int x, y;
	int x1, x2, y1, y2;

	bool have_stuff = false;

	/* Elegir un área para detectar */
	y1 = player->grid.y - context->y;
	y2 = player->grid.y + context->y;
	x1 = player->grid.x - context->x;
	x2 = player->grid.x + context->x;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear el área */
	for (y = y1; y <= y2; y++) {
		for (x = x1; x <= x2; x++) {
			struct loc grid = loc(x, y);
			struct object *obj = square_object(cave, grid);

			/*
			 * ¿Hay algún objeto que coincida con el predicado que
			 * no esté ignorado?
			 */
			for (; !have_stuff && obj; obj = obj->next) {
				if ((*pred)(obj) && !ignore_item_ok(player, obj)) {
					have_stuff = true;
				}
			}

			/*
			 * Marcar las partes del montón que coinciden con el predicado
			 * como vistas. Olvidar partes recordadas que coinciden
			 * con el predicado que ya no están allí.
			 */
			square_know_pile(cave, grid, pred);
		}
	}

	return have_stuff;
}

/**
 * Sentir dinero en el suelo alrededor del jugador.
 */
bool effect_handler_SENSE_GOLD(effect_handler_context_t *context)
{
	bool money = sense_stuff(context, tval_is_money, unknown_gold_kind);

	if (money) {
		msg("¡Sientes la presencia de oro!");
	} else if (context->aware) {
		msg("No sientes oro.");
	}

	context->ident = true;
	return true;
}

/**
 * Detectar dinero en el suelo alrededor del jugador.
 */
bool effect_handler_DETECT_GOLD(effect_handler_context_t *context)
{
	bool money = detect_stuff(context, tval_is_money);

	if (money) {
		msg("¡Detectas la presencia de oro!");
	} else if (context->aware) {
		msg("No detectas oro.");
	}

	context->ident = true;
	return true;
}

/**
 * Ayuda para effect_handler_SENSE_OBJECTS() y effect_handler_DETECT_OBJECTS():
 * negar tval_is_money().
 */
static bool tval_is_not_money(const struct object *o)
{
	return !tval_is_money(o);
}

/**
 * Sentir objetos que no son dinero alrededor del jugador.
 */
bool effect_handler_SENSE_OBJECTS(effect_handler_context_t *context)
{
	bool objects = sense_stuff(context, tval_is_not_money,
		unknown_item_kind);

	if (objects) {
		msg("¡Sientes la presencia de objetos!");
	} else if (context->aware) {
		msg("No sientes objetos.");
	}

	/* Redibujar lista de objetos */
	player->upkeep->redraw |= PR_ITEMLIST;

	context->ident = true;
	return true;
}

/**
 * Detectar objetos que no son dinero alrededor del jugador.
 */
bool effect_handler_DETECT_OBJECTS(effect_handler_context_t *context)
{
	bool objects = detect_stuff(context, tval_is_not_money);

	if (objects) {
		msg("¡Detectas la presencia de objetos!");
	} else if (context->aware) {
		msg("No detectas objetos.");
	}

	/* Redibujar lista de objetos */
	player->upkeep->redraw |= PR_ITEMLIST;

	context->ident = true;
	return true;
}

/**
 * Detectar monstruos que satisfacen el predicado dado alrededor del jugador.
 * La altura a detectar arriba y abajo del jugador es y_dist,
 * el ancho a cada lado del jugador es x_dist.
 */
static bool detect_monsters(int y_dist, int x_dist, monster_predicate pred)
{
	int i, x, y;
	int x1, x2, y1, y2;

	bool monsters = false;

	/* Establecer el área de detección */
	y1 = player->grid.y - y_dist;
	y2 = player->grid.y + y_dist;
	x1 = player->grid.x - x_dist;
	x2 = player->grid.x + x_dist;

	if (y1 < 0) y1 = 0;
	if (x1 < 0) x1 = 0;
	if (y2 > cave->height - 1) y2 = cave->height - 1;
	if (x2 > cave->width - 1) x2 = cave->width - 1;

	/* Escanear monstruos */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Saltar monstruos muertos */
		if (!mon->race) continue;

		/* Ubicación */
		y = mon->grid.y;
		x = mon->grid.x;

		/* Solo detectar monstruos cercanos */
		if (x < x1 || y < y1 || x > x2 || y > y2) continue;

		/* Detectar todos los monstruos apropiados y obvios */
		if (pred(mon) && !monster_is_camouflaged(mon)) {
			/* Detectar el monstruo */
			mflag_on(mon->mflag, MFLAG_MARK);
			mflag_on(mon->mflag, MFLAG_SHOW);

			/* Notar monstruos invisibles */
			if (monster_is_invisible(mon)) {
				struct monster_lore *lore = get_lore(mon->race);
				rf_on(lore->flags, RF_INVISIBLE);
			}

			/* Actualizar ventana de recuerdo de monstruo */
			if (player->upkeep->monster_race == mon->race)
				/* Redibujar cosas */
				player->upkeep->redraw |= (PR_MONSTER);

			/* Actualizar el monstruo */
			update_mon(mon, cave, false);

			/* Detectar */
			monsters = true;
		}
	}

	return monsters;
}

/**
 * Detectar monstruos vivos alrededor del jugador. La altura a detectar arriba y
 * abajo del jugador es context->value.dice, el ancho a cada lado del jugador
 * es context->value.sides.
 */
bool effect_handler_DETECT_LIVING_MONSTERS(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x, monster_is_living);

	if (monsters)
		msg("¡Sientes vida!");
	else if (context->aware)
		msg("No sientes vida.");

	context->ident = true;
	return true;
}


/**
 * Detectar monstruos visibles alrededor del jugador; nota que esto significa monstruos
 * que son en principio visibles, no monstruos que el jugador puede ver actualmente.
 *
 * La altura a detectar arriba y
 * abajo del jugador es context->value.dice, el ancho a cada lado del jugador
 * es context->value.sides.
 */
bool effect_handler_DETECT_VISIBLE_MONSTERS(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x,
									monster_is_not_invisible);

	if (monsters)
		msg("¡Sientes la presencia de monstruos!");
	else if (context->aware)
		msg("No sientes monstruos.");

	context->ident = true;
	return true;
}


/**
 * Detectar monstruos invisibles alrededor del jugador. La altura a detectar arriba y
 * abajo del jugador es context->value.dice, el ancho a cada lado del jugador
 * es context->value.sides.
 */
bool effect_handler_DETECT_INVISIBLE_MONSTERS(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x,
									monster_is_invisible);

	if (monsters)
		msg("¡Sientes la presencia de criaturas invisibles!");
	else if (context->aware)
		msg("No sientes criaturas invisibles.");

	context->ident = true;
	return true;
}

/**
 * Detectar monstruos susceptibles al miedo alrededor del jugador. La altura a detectar
 * arriba y abajo del jugador es context->value.dice, el ancho a cada lado de
 * el jugador es context->value.sides.
 */
bool effect_handler_DETECT_FEARFUL_MONSTERS(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x, monster_is_fearful);

	if (monsters)
		msg("Estos monstruos podrían proporcionar buen deporte.");
	else if (context->aware)
		msg("No hueles miedo en el aire.");

	context->ident = true;
	return true;
}

/**
 * Detectar monstruos malignos alrededor del jugador. La altura a detectar arriba y
 * abajo del jugador es context->value.dice, el ancho a cada lado del jugador
 * es context->value.sides.
 */
bool effect_handler_DETECT_EVIL(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x, monster_is_evil);

	if (monsters)
		msg("¡Sientes la presencia de criaturas malignas!");
	else if (context->aware)
		msg("No sientes criaturas malignas.");

	context->ident = true;
	return true;
}

/**
 * Detectar monstruos que poseen un espíritu alrededor del jugador.
 * La altura a detectar arriba y abajo del jugador es context->value.dice,
 * el ancho a cada lado del jugador es context->value.sides.
 */
bool effect_handler_DETECT_SOUL(effect_handler_context_t *context)
{
	bool monsters = detect_monsters(context->y, context->x, monster_has_spirit);

	if (monsters)
		msg("¡Sientes la presencia de espíritus!");
	else if (context->aware)
		msg("No sientes espíritus.");

	context->ident = true;
	return true;
}

/**
 * Identificar una runa desconocida de un objeto.
 */
bool effect_handler_IDENTIFY(effect_handler_context_t *context)
{
	struct object *obj;
	const char *q, *s;
	int itemmode = (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR);
	bool used = false;

	context->ident = true;

	/* Obtener un objeto */
	q = "¿Identificar qué objeto? ";
	s = "No tienes nada para identificar.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				item_tester_unknown, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, item_tester_unknown, itemmode))
		return used;

	/* Identificar el objeto */
	object_learn_unknown_rune(player, obj);

	return true;
}


/**
 * Crear escaleras en la ubicación del jugador
 */
bool effect_handler_CREATE_STAIRS(effect_handler_context_t *context)
{
	context->ident = true;

	/* Solo permitir crear escaleras en suelo vacío */
	if (!square_isfloor(cave, player->grid)) {
		msg("No hay suelo vacío aquí.");
		return false;
	}

	/* Fallo para niveles persistentes (por ahora) y arenas */
	if (OPT(player, birth_levels_persist) || player->upkeep->arena_level) {
		msg("¡No pasa nada!");
		return false;
	}

	/* Empujar objetos fuera de la cuadrícula */
	if (square_object(cave, player->grid))
		push_object(player->grid);

	square_add_stairs(cave, player->grid, player->depth);

	return true;
}

/**
 * Aplicar desencantamiento a las cosas del jugador.
 */
bool effect_handler_DISENCHANT(effect_handler_context_t *context)
{
	int i, count = 0;
	struct object *obj;
	char o_name[80];

	/* Contar espacios */
	for (i = 0; i < player->body.count; i++) {
		/* Ignorar anillos, amuletos y luces */
		if (slot_type_is(player, i, EQUIP_RING)) continue;
		if (slot_type_is(player, i, EQUIP_AMULET)) continue;
		if (slot_type_is(player, i, EQUIP_LIGHT)) continue;

		/* Contar espacios desencantables */
		count++;
	}

	/* Elegir uno al azar */
	for (i = player->body.count - 1; i >= 0; i--) {
		/* Ignorar anillos, amuletos y luces */
		if (slot_type_is(player, i, EQUIP_RING)) continue;
		if (slot_type_is(player, i, EQUIP_AMULET)) continue;
		if (slot_type_is(player, i, EQUIP_LIGHT)) continue;

		if (one_in_(count--)) break;
	}

	/* Notificar */
	context->ident = true;

	/* Obtener el objeto */
	obj = slot_object(player, i);

	/* Sin objeto, no pasa nada */
	if (!obj) return true;

	/* Nada que desencantar */
	if ((obj->to_h <= 0) && (obj->to_d <= 0) && (obj->to_a <= 0))
		return true;

	/* Describir el objeto */
	object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, player);

	/* Los artefactos tienen un 60% de probabilidad de resistir */
	if (obj->artifact && (randint0(100) < 60)) {
		/* Mensaje */
		msg("¡Tu %s (%c) resist%s el desencantamiento!", o_name,
			gear_to_label(player, obj),
			((obj->number != 1) ? "en" : "e"));

		return true;
	}

	/* Aplicar desencantamiento, dependiendo del tipo de equipo */
	if (slot_type_is(player, i, EQUIP_WEAPON)
			|| slot_type_is(player, i, EQUIP_BOW)) {
		/* Desencantar golpe */
		if (obj->to_h > 0) obj->to_h--;
		if ((obj->to_h > 5) && (randint0(100) < 20)) obj->to_h--;
		obj->known->to_h = obj->to_h;

		/* Desencantar daño */
		if (obj->to_d > 0) obj->to_d--;
		if ((obj->to_d > 5) && (randint0(100) < 20)) obj->to_d--;
		obj->known->to_d = obj->to_d;
	} else {
		/* Desencantar AC */
		if (obj->to_a > 0) obj->to_a--;
		if ((obj->to_a > 5) && (randint0(100) < 20)) obj->to_a--;
		obj->known->to_a = obj->to_a;
	}

	/* Mensaje */
	msg("Tu %s (%c) %s desencantad%s!", o_name,
		gear_to_label(player, obj),
		((obj->number != 1) ? "fueron" : "fue"),
		((obj->number != 1) ? "s" : ""));

	/* Recalcular bonificaciones */
	player->upkeep->update |= (PU_BONUS);

	/* Cosas de ventana */
	player->upkeep->redraw |= (PR_EQUIP);

	return true;
}

/**
 * Encantar un objeto (en el inventario o en el suelo)
 * Nota que la armadura, golpe o daño están controlados por context->subtype
 *
 * El trabajo para incorporar enchant_spell() se ha pospuesto...NRM
 */
bool effect_handler_ENCHANT(effect_handler_context_t *context)
{
	int value = randcalc(context->value, player->depth, RANDOMISE);
	bool used = false;
	context->ident = true;

	if ((context->subtype & ENCH_TOBOTH) == ENCH_TOBOTH) {
		if (enchant_spell(value, value, 0, context->cmd))
			used = true;
	}
	else if (context->subtype & ENCH_TOHIT) {
		if (enchant_spell(value, 0, 0, context->cmd))
			used = true;
	}
	else if (context->subtype & ENCH_TODAM) {
		if (enchant_spell(0, value, 0, context->cmd))
			used = true;
	}
	if (context->subtype & ENCH_TOAC) {
		if (enchant_spell(0, 0, value, context->cmd))
			used = true;
	}

	return used;
}

/**
 * Recargar una varita o bastón de la mochila o del suelo. La fuerza de recarga
 * es context->value.base.
 *
 * Es más difícil recargar varitas de alto nivel y con muchas cargas.
 */
bool effect_handler_RECHARGE(effect_handler_context_t *context)
{
	int i, t;
	int strength = context->value.base;
	int itemmode = (USE_INVEN | USE_FLOOR | SHOW_RECHARGE);
	struct object *obj;
	bool used = false;
	const char *q, *s;

/* Inmediatamente obvio */
	context->ident = true;

	/* Se usa para mostrar tasas de fallo de recarga */
	player->upkeep->recharge_pow = strength;

	/* Obtener un objeto */
	q = "¿Recargar qué objeto? ";
	s = "No tienes nada para recargar.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_can_have_charges, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, tval_can_have_charges, itemmode)) {
		return (used);
	}

	i = recharge_failure_chance(obj, strength);
	/* Retroceso */
	if ((i <= 1) || one_in_(i)) {
		struct object *destroyed;
		bool none_left = false;

		msg("¡La recarga falla peligrosamente!");
		msg("Hay un destello brillante de luz.");

		/* Reducir y describir inventario */
		if (object_is_carried(player, obj)) {
			destroyed = gear_object_for_use(player, obj, 1, true,
				&none_left);
		} else {
			destroyed = floor_object_for_use(player, obj, 1, true,
				&none_left);
		}
		if (destroyed->known)
			object_delete(player->cave, NULL, &destroyed->known);
		object_delete(cave, player->cave, &destroyed);
	} else {
		/* Extraer un "poder" */
		int ease_of_recharge = (100 - obj->kind->level) / 10;
		t = (strength / (10 - ease_of_recharge)) + 1;

		/* Recargar basado en el poder */
		if (t > 0) obj->pval += 2 + randint1(t);
	}

	/* Combinar la mochila (más tarde) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redibujar cosas */
	player->upkeep->redraw |= (PR_INVEN);

	/* Se hizo algo */
	return true;
}

bool effect_handler_ACQUIRE(effect_handler_context_t *context)
{
	int num = effect_calculate_value(context, false);
	acquirement(player->grid, player->depth, num, true);
	context->ident = true;
	return true;
}

/**
 * Despertar a todos los monstruos en línea de visión
 */
bool effect_handler_WAKE(effect_handler_context_t *context)
{
	int i;
	bool woken = false;

	struct loc origin = origin_get_loc(context->origin);

	/* Despertar a todos los cercanos */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);
		if (mon->race) {
			int radius = z_info->max_sight * 2;
			int dist = distance(origin, mon->grid);

			/* Saltar monstruos demasiado lejanos */
			if ((dist < radius) && mon->m_timed[MON_TMD_SLEEP]) {
				/* El monstruo se despierta, más cerca significa más probable que se vuelva consciente */
				monster_wake(mon, false, 100 - 2 * dist);
				woken = true;
			}
		}
	}

	/* Mensajes */
	if (woken) {
		msg("¡Escuchas un repentino revuelo en la distancia!");
	}

	context->ident = true;

	return true;
}

/**
 * Invocar context->value monstruos de tipo context->subtype.
 */
bool effect_handler_SUMMON(effect_handler_context_t *context)
{
	int summon_max = effect_calculate_value(context, false);
	int summon_type = context->subtype;
	int level_boost = context->other;
	int message_type = summon_message_type(summon_type);
	int fallback_type = summon_fallback_type(summon_type);
	int count = 0, val = 0, attempts = 0;

	sound(message_type);

	/* Sin invocación en niveles de arena */
	if (player->upkeep->arena_level) return true;

	/* Invocación de monstruo */
	if (context->origin.what == SRC_MONSTER) {
		struct monster *mon = cave_monster(cave, context->origin.which.monster);
		int rlev;

		assert(mon);

		/* Establecer kin_base si es necesario */
		if (summon_type == summon_name_to_idx("KIN")) {
			kin_base = mon->race->base;
		}

		/* Continuar invocando hasta alcanzar el nivel actual de la mazmorra */
		rlev = mon->race->level;
		while ((val < player->depth * rlev) && (attempts < summon_max)) {
			int temp;

			/* Obtener un monstruo */
			temp = summon_specific(mon->grid, rlev + level_boost, summon_type,
								   false, false);

			val += temp * temp;

			/* Aumentar el intento en caso de que no haya monstruos disponibles. */
			attempts++;

			/* Aumentar el recuento de monstruos invocados */
			if (val > 0)
				count++;
		}

		/* Si la invocación falló y hay un tipo alternativo, usar ese */
		if ((count == 0) && (fallback_type >= 0)) {
			attempts = 0;
			while ((val < player->depth * rlev) && (attempts < summon_max)) {
				int temp;

				/* Obtener un monstruo */
				temp = summon_specific(mon->grid, rlev + level_boost,
									   fallback_type, false, false);

				val += temp * temp;

				/* Aumentar el intento en caso de que no haya monstruos disponibles. */
				attempts++;

				/* Aumentar el recuento de monstruos invocados */
				if (val > 0)
					count++;
			}
		}

		/* El invocador falló */
		if (!count)
			msg("Pero no viene nada.");
	} else {
		/* Si no es una invocación de monstruo, es simple */
		while (summon_max) {
			count += summon_specific(player->grid, player->depth + level_boost,
									 summon_type, true, one_in_(4));
			summon_max--;
		}
	}

	/* Identificar */
	context->ident = true;

	/* Mensaje para los ciegos */
	if (count && player->timed[TMD_BLIND])
		msgt(message_type, "Escuchas %s aparecer cerca.",
			 (count > 1 ? "muchas cosas" : "algo"));

	return true;
}

/**
 * Eliminar todos los monstruos no únicos de un determinado "tipo" del nivel
 * -------
 * Advertencia - esta función asume que el símbolo de monstruo ingresado es un carácter
 *		   ASCII, lo que puede no ser cierto en el futuro - NRM
 * -------
 */
bool effect_handler_BANISH(effect_handler_context_t *context)
{
	int i;
	unsigned dam = 0;

	char typ;

	context->ident = true;

	/* No permitir en una arena. */
	if (player->upkeep->arena_level) {
		msg("No pasa nada.");
		return true;
	}

	if (!get_com("Elige una raza de monstruo (por símbolo) para desterrar: ", &typ))
		return false;

	/* Eliminar los monstruos de ese "tipo" */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Paranoia -- Saltar monstruos muertos */
		if (!mon->race) continue;

		/* Saltar Monstruos Únicos */
		if (monster_is_unique(mon)) continue;

		/*
		 * Saltar monstruos "equivocados" (ver advertencia arriba); para cambiaformas
		 * es la raza original lo que importa, no la forma que tiene el monstruo ahora.
		 */
		if (mon->original_race) {
			if ((char) mon->original_race->d_char != typ) continue;
		} else {
			if ((char) mon->race->d_char != typ) continue;
		}

		/* Eliminar el monstruo */
		delete_monster_idx(cave, i);

		/* Recibir algo de daño */
		dam += randint1(4);
	}

	/* Dañar al jugador */
	dam = player_apply_damage_reduction(player, dam);
	if (dam > 0 && OPT(player, show_damage)) {
		msg("Recibes %d de daño.", dam);
	}
	take_hit(player, dam, "la tensión de lanzar Destierro");

	/* Actualizar ventana de lista de monstruos */
	player->upkeep->redraw |= PR_MONLIST;

	/* Éxito */
	return true;
}

/**
 * Eliminar todos los monstruos cercanos (no únicos). El radio de efecto es
 * context->radius si se pasa, de lo contrario el radio de visión del jugador.
 */
bool effect_handler_MASS_BANISH(effect_handler_context_t *context)
{
	int i;
	int radius = context->radius ? context->radius : z_info->max_sight;
	unsigned dam = 0;

	context->ident = true;

	/* No permitir en una arena. */
	if (player->upkeep->arena_level) {
		msg("No pasa nada.");
		return true;
	}

	/* Eliminar los monstruos (cercanos) */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Paranoia -- Saltar monstruos muertos */
		if (!mon->race) continue;

		/* Saltar monstruos únicos */
		if (monster_is_unique(mon)) continue;

		/* Saltar monstruos distantes */
		if (mon->cdis > radius) continue;

		/* Eliminar el monstruo */
		delete_monster_idx(cave, i);

		/* Recibir algo de daño */
		dam += randint1(3);
	}

	/* Dañar al jugador */
	dam = player_apply_damage_reduction(player, dam);
	if (dam > 0 && OPT(player, show_damage)) {
		msg("Recibes %d de daño.", dam);
	}
	take_hit(player, dam, "la tensión de lanzar Destierro Masivo");

	/* Actualizar ventana de lista de monstruos */
	player->upkeep->redraw |= PR_MONLIST;

	return true;
}

/**
 * Probar monstruos cercanos
 */
bool effect_handler_PROBE(effect_handler_context_t *context)
{
	int i;

	bool probe = false;

	/* Probar todos los monstruos (cercanos) */
	for (i = 1; i < cave_monster_max(cave); i++) {
		struct monster *mon = cave_monster(cave, i);

		/* Paranoia -- Saltar monstruos muertos */
		if (!mon->race) continue;

		/* Requerir línea de visión */
		if (!square_isview(cave, mon->grid)) continue;

		/* Probar monstruos visibles */
		if (monster_is_visible(mon)) {
			char m_name[80];

			/* Iniciar el mensaje */
			if (!probe) msg("Probando...");

			/* Obtener "el monstruo" o "algo" */
			monster_desc(m_name, sizeof(m_name), mon,
				MDESC_IND_HID | MDESC_CAPITAL | MDESC_COMMA);

			/* Describir el monstruo */
			msg("%s tiene %d punto%s de golpe.", m_name, mon->hp, (mon->hp == 1) ? "" : "s");

			/* Aprender todas las banderas no relacionadas con hechizos ni tesoros */
			lore_do_probe(mon);

			/* La prueba funcionó */
			probe = true;
		}
	}

	/* Hecho */
	if (probe) {
		msg("Eso es todo.");
		context->ident = true;
	}

	return true;
}

/**
 * Teletransportar al jugador o monstruo hasta context->value.base cuadrículas de distancia.
 *
 * Si no hay espacios disponibles fácilmente, la distancia puede aumentar.
 * Intentar muy fuerte mover al jugador/monstruo al menos un cuarto de esa distancia.
 * Establecer context->subtype permite que los monstruos teletransporten al jugador.
 * Establecer context->y y context->x los trata como coordenadas y e x
 * y teletransporta al monstruo desde esa cuadrícula.
 */
bool effect_handler_TELEPORT(effect_handler_context_t *context)
{
	struct loc start = loc(context->x, context->y);
	int dis = context->value.base;
	int perc = context->value.m_bonus;
	int pick;
	struct loc grid;

	struct jumps {
		struct loc grid;
		struct jumps *next;
	} *spots = NULL;
	int num_spots = 0;
	int current_score = 2 * MAX(z_info->dungeon_wid, z_info->dungeon_hgt);
	bool only_vault_grids_possible = true;

	bool is_player = (context->origin.what != SRC_MONSTER || context->subtype);
	struct monster *t_mon = monster_target_monster(context);

	context->ident = true;

	/* No teletransporte en niveles de arena */
	if (player->upkeep->arena_level) return true;

	/* Establecer las coordenadas desde las que teletransportar, si no las sabemos ya */
	if (!loc_is_zero(start)) {
		/* Estamos bien */
	} else if (t_mon) {
		/* Monstruo apuntando a otro monstruo */
		start = t_mon->grid;
	} else if (is_player) {
		/* Los señuelos son destruidos */
		struct loc decoy = cave_find_decoy(cave);
		if (!loc_is_zero(decoy) && context->subtype) {
			square_destroy_decoy(cave, decoy);
			return true;
		}

		start = player->grid;

		/* Comprobar si hay una cuadrícula sin teletransporte */
		if (square_isno_teleport(cave, start) &&
			((dis > 10) || (dis == 0))) {
			msg("¡Teletransporte prohibido!");
			return true;
		}

		/* Comprobar si hay una maldición de no teletransporte */
		if (player_of_has(player, OF_NO_TELEPORT)) {
			equip_learn_flag(player, OF_NO_TELEPORT);
			msg("¡Teletransporte prohibido!");
			return true;
		}
	} else {
		assert(context->origin.what == SRC_MONSTER);
		struct monster *mon = cave_monster(cave, context->origin.which.monster);
		start = mon->grid;
	}

	/* Porcentaje de la mayor distancia cardinal a un borde */
	if (perc) {
		int vertical = MAX(start.y, cave->height - start.y);
		int horizontal = MAX(start.x, cave->width - start.x);
		dis = (MAX(vertical, horizontal) * perc) / 100;
	}

	/* Aleatorizar un poco la distancia */
	if (one_in_(2)) {
		dis -= randint0(dis / 4);
	} else {
		dis += randint0(dis / 4);
	}

	/* Hacer una lista de las mejores cuadrículas, puntuando por qué tan buena aproximación
	 * es la distancia desde el inicio a la distancia que queremos */
	for (grid.y = 1; grid.y < cave->height - 1; grid.y++) {
		for (grid.x = 1; grid.x < cave->width - 1; grid.x++) {
			int d = distance(grid, start);
			int score = ABS(d - dis);
			struct jumps *new;

			/* Debe moverse */
			if (d == 0) continue;

			if (!has_teleport_destination_prereqs(cave, grid,
					is_player)) continue;

			/* No teletransportarse a bóvedas y similares, a menos que no haya elección */
			if (square_isvault(cave, grid)) {
				if (!only_vault_grids_possible) {
					continue;
				}
			} else {
				/* Recién comenzando a considerar cuadrículas sin bóveda, así que restablecer puntuación */
				if (only_vault_grids_possible) {
					current_score = 2 * MAX(z_info->dungeon_wid,
											z_info->dungeon_hgt);
				}
				only_vault_grids_possible = false;
			}

			/* ¿Ya tenemos mejores lugares? */
			if (score > current_score) continue;

			/* Hacer un nuevo lugar */
			new = mem_zalloc(sizeof(struct jumps));
			new->grid = grid;

			/* Si mejora, comenzar una nueva lista, de lo contrario extender la anterior */
			if (score < current_score) {
				current_score = score;
				while (spots) {
					struct jumps *next = spots->next;
					mem_free(spots);
					spots = next;
				}
				spots = new;
				num_spots = 1;
			} else {
				new->next = spots;
				spots = new;
				num_spots++;
			}
		}
	}

	/* Reportar fallo (muy improbable) */
	if (!num_spots) {
		if (is_player) {
			msg("¡Fallo al encontrar destino de teletransporte!");
		} else {
			/*
			 * Con teletransporte propio o teletransporte de otro, será
			 * el lanzador el que está desconcertado.
			 */
			struct monster *mon = cave_monster(cave,
				context->origin.which.monster);

			if (square_isseen(cave, mon->grid)) {
				add_monster_message(mon, MON_MSG_BRIEF_PUZZLE,
					true);
			}
		}
		return true;
	}

	/* Elegir un lugar */
	pick = randint0(num_spots);
	while (pick) {
		struct jumps *next = spots->next;
		mem_free(spots);
		spots = next;
		pick--;
	}

	/* Sonido */
	sound(is_player ? MSG_TELEPORT : MSG_TPOTHER);

	/* Mover al jugador o monstruo */
	monster_swap(start, spots->grid);
	if (is_player) {
		player_handle_post_move(player, true,
			context->origin.what == SRC_MONSTER);
	}

	/* Limpiar cualquier marcador de proyección para evitar procesamiento doble */
	sqinfo_off(square(cave, spots->grid)->info, SQUARE_PROJECT);

	/* Limpiar objetivo del monstruo si ya no es visible */
	if (!target_able(target_get_monster())) {
		target_set_monster(NULL);
	}

	/* Muchas actualizaciones después de monster_swap */
	handle_stuff(player);

	while (spots) {
		struct jumps *next = spots->next;
		mem_free(spots);
		spots = next;
	}

	return true;
}

/**
 * Teletransportar al jugador o monstruo objetivo a una cuadrícula cerca de la ubicación dada
 * Establecer context->y y context->x los trata como coordenadas y e x
 * Establecer context->subtype permite que los monstruos teletransporten hacia el jugador.
 *
 * Esta función es ligeramente obsesiva con la corrección.
 * Esta función permite teletransportarse a bóvedas (!)
 */
bool effect_handler_TELEPORT_TO(effect_handler_context_t *context)
{
	struct monster *mon = NULL;
	struct loc start, aim, land;
	int dis = 0, ctr = 0, dir = DIR_TARGET;
	struct monster *t_mon = monster_target_monster(context);
	bool dim_door = false;
	bool player_moves = false;

	context->ident = true;

	/* No teletransporte en niveles de arena */
	if (player->upkeep->arena_level) return true;

	if (context->origin.what == SRC_MONSTER) {
		mon = cave_monster(cave, context->origin.which.monster);
		assert(mon);
	}

	/* ¿De dónde venimos? */
	if (t_mon) {
		/* Monstruo siendo teletransportado */
		start = t_mon->grid;
	} else if (context->subtype) {
		if (!mon) {
			msg("Error: efecto TELEPORT_TO:SELF usado que no es "
				"lanzado por un monstruo.");
			return true;
		}
		/* Monstruo teletransportándose al jugador */
		start = mon->grid;
	} else {
		/* Los señuelos objetivo son destruidos */
		if (mon && monster_is_decoyed(mon)) {
			square_destroy_decoy(cave, cave_find_decoy(cave));
			return true;
		}

		/* Jugador siendo teletransportado */
		player_moves = true;
		start = player->grid;

		/* Comprobar si hay una cuadrícula sin teletransporte */
		if (square_isno_teleport(cave, start)) {
			msg("¡Teletransporte prohibido!");
			return true;
		}

		/* Comprobar si hay una maldición de no teletransporte */
		if (player_of_has(player, OF_NO_TELEPORT)) {
			equip_learn_flag(player, OF_NO_TELEPORT);
			msg("¡Teletransporte prohibido!");
			return true;
		}
	}

	/* ¿A dónde vamos? */
	if (context->y && context->x) {
		/* Al efecto se le dieron coordenadas */
		aim = loc(context->x, context->y);
	} else if (mon) {
		/* Hechizo lanzado por monstruo */
		if (context->subtype) {
			/* Monstruo teletransportándose al jugador */
			aim = player->grid;
			dis = 2;
		} else {
			/* Jugador siendo teletransportado al monstruo */
			aim = mon->grid;
		}
	} else {
		/* Elección del jugador */
		do {
			if (!get_aim_dir(&dir)) return false;
		} while (dir == DIR_TARGET && !target_okay());

		if (dir == DIR_TARGET)
			target_get(&aim);
		else
			aim = loc_offset(start, ddx[dir], ddy[dir]);

		/* Aleatorizar un poco el aterrizaje si es una bóveda */
		if (square_isvault(cave, aim)) dis = 10;
		dim_door = true;
	}

	/* Encontrar una ubicación utilizable */
	while (1) {
		/* Elegir una ubicación legal cercana */
		while (1) {
			land = rand_loc(aim, dis, dis);
			if (square_in_bounds_fully(cave, land)) break;
		}

		if (has_teleport_destination_prereqs(cave, land,
				player_moves)) break;

		/* Ocasionalmente avanzar la distancia */
		if (++ctr > (4 * dis * dis + 4 * dis + 1)) {
			ctr = 0;
			dis++;
		}
	}

	/* Sonido */
	sound(MSG_TELEPORT);

	/* Mover al jugador o monstruo */
	monster_swap(start, land);
	if (player_moves) {
		player_handle_post_move(player, true,
			context->origin.what == SRC_MONSTER);
	}

	/* Cancelar objetivo si es necesario */
	if (dim_door) {
		target_set_location(0, 0);
	}

	/* Limpiar cualquier marcador de proyección para evitar procesamiento doble */
	sqinfo_off(square(cave, land)->info, SQUARE_PROJECT);

	/* Muchas actualizaciones después de monster_swap */
	handle_stuff(player);

	return true;
}

/**
 * Teletransportar al jugador un nivel hacia arriba o abajo (aleatorio cuando es legal)
 */
bool effect_handler_TELEPORT_LEVEL(effect_handler_context_t *context)
{
	bool up = true;
	bool down = true;
	int target_depth = dungeon_get_next_level(player, player->max_depth, 1);
	struct monster *t_mon = monster_target_monster(context);
	struct loc decoy = cave_find_decoy(cave);

	context->ident = true;

	/* No teletransporte en niveles de arena */
	if (player->upkeep->arena_level) return true;

	/* Comprobar si un monstruo apunta a otro monstruo */
	if (t_mon) {
		/* El monstruo simplemente desaparece */
		add_monster_message(t_mon, MON_MSG_DISAPPEAR, false);
		delete_monster_idx(cave, t_mon->midx);
		return true;
	}

	/* Los señuelos objetivo son destruidos */
	if (decoy.y && decoy.x) {
		square_destroy_decoy(cave, decoy);
		return true;
	}

	/* Comprobar si hay una cuadrícula sin teletransporte */
	if (square_isno_teleport(cave, player->grid)) {
		msg("¡Teletransporte prohibido!");
		return true;
	}

	/* Comprobar si hay una maldición de no teletransporte */
	if (player_of_has(player, OF_NO_TELEPORT)) {
		equip_learn_flag(player, OF_NO_TELEPORT);
		msg("¡Teletransporte prohibido!");
		return true;
	}

	/* Resistir teletransporte hostil */
	if (context->origin.what == SRC_MONSTER &&
			player_resists(player, ELEM_NEXUS)) {
		msg("¡Resistes el efecto!");
		return true;
	}

	/* No subir con force_descend o en la ciudad */
	if (OPT(player, birth_force_descend) || !player->depth)
		up = false;

	/* No forzar al jugador a bajar a niveles de misión si no puede salir */
	if (!up && is_quest(player, target_depth))
		down = false;

	/* No puede salir de niveles de misión o bajar más profundo que la mazmorra */
	if (is_quest(player, player->depth)
			|| (player->depth >= z_info->max_depth - 1))
		down = false;

	/* Determinar arriba/abajo si no se ha hecho ya */
	if (up && down) {
		if (randint0(100) < 50)
			up = false;
		else
			down = false;
	}

	/*
	 * Ahora realmente hacer el cambio de nivel; vaciar la cola de comandos para
	 * evitar que el personaje pierda una acción al entrar por primera vez
	 * en el nuevo nivel (por ejemplo, el jugador se mueve poniendo un comando
	 * de recogida automática en la cola y luego es golpeado por un hechizo de
	 * teletransporte de nivel)
	 */
	if (up) {
		msgt(MSG_TPLEVEL, "Te elevas a través del techo.");
		cmdq_flush();
		target_depth = dungeon_get_next_level(player,
			player->depth, -1);
		dungeon_change_level(player, target_depth);
	} else if (down) {
		msgt(MSG_TPLEVEL, "Te hundes a través del suelo.");

		cmdq_flush();
		if (OPT(player, birth_force_descend)) {
			target_depth = dungeon_get_next_level(player,
				player->max_depth, 1);
			dungeon_change_level(player, target_depth);
		} else {
			target_depth = dungeon_get_next_level(player,
				player->depth, 1);
			dungeon_change_level(player, target_depth);
		}
	} else {
		msg("No pasa nada.");
	}

	return true;
}

/**
 * El efecto de escombros
 *
 * Esto hace que caigan escombros en cuadrados vacíos.
 */
bool effect_handler_RUBBLE(effect_handler_context_t *context)
{
	/*
	 * Primero calculamos cuántas cuadrículas queremos llenar con escombros. Luego
	 * comprobamos que realmente podemos hacer esto, contando el número de cuadrículas
	 * disponibles, limitando el número de cuadrículas de escombros a este número si
	 * es necesario.
	 */
	int rubble_grids = randint1(3);
	int open_grids = count_neighbors(NULL, cave, player->grid,
		square_isempty, false);

	if (rubble_grids > open_grids) {
		rubble_grids = open_grids;
	}

	/* Evitar bucles infinitos */
	int iterations = 0;

	while (rubble_grids > 0 && iterations < 10) {
		/* Mirar alrededor del jugador */
		for (int d = 0; d < 8; d++) {
			/* Extraer ubicación adyacente (legal) */
			struct loc grid = loc_sum(player->grid, ddgrid_ddd[d]);
			if (!square_in_bounds_fully(cave, grid)) continue;
			if (!square_isempty(cave, grid)) continue;

			if (one_in_(3)) {
				if (one_in_(2))
					square_set_feat(cave, grid, FEAT_PASS_RUBBLE);
				else
					square_set_feat(cave, grid, FEAT_RUBBLE);
				if (cave->depth == 0)
					expose_to_sun(cave, grid, is_daytime());
				rubble_grids--;
			}
		}

		iterations++;
	}

	context->ident = true;

	/* Actualizar completamente los elementos visuales */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redibujar lista de monstruos */
	player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);

	return true;
}

bool effect_handler_GRANITE(effect_handler_context_t *context)
{
	struct trap *trap = context->origin.which.trap;
	square_set_feat(cave, trap->grid, FEAT_GRANITE);
	if (cave->depth == 0) expose_to_sun(cave, trap->grid, is_daytime());

	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);

	return true;
}

bool effect_handler_LIGHT_LEVEL(effect_handler_context_t *context)
{
	bool full = context->value.base ? true : false;
	if (full)
		msg("Una imagen de tu entorno se forma en tu mente...");
	wiz_light(cave, player, full);
	context->ident = true;
	return true;
}

bool effect_handler_DARKEN_LEVEL(effect_handler_context_t *context)
{
	bool full = context->value.base ? true : false;
	if (full)
		msg("Una gran oscuridad recorre la mazmorra...");
	wiz_dark(cave, player, full);
	context->ident = true;
	return true;
}

/**
 * Llamar luz alrededor del jugador
 */
bool effect_handler_LIGHT_AREA(effect_handler_context_t *context)
{
	/* Mensaje */
	if (!player->timed[TMD_BLIND])
		msg("Estás rodeado por una luz blanca.");

	/* Iluminar la habitación */
	light_room(player->grid, true);

	/* Asumir visto */
	context->ident = true;
	return (true);
}


/**
 * Llamar oscuridad alrededor del jugador o monstruo objetivo
 */
bool effect_handler_DARKEN_AREA(effect_handler_context_t *context)
{
	struct loc target = player->grid;
	bool message = player->timed[TMD_BLIND] ? false : true;
	struct monster *mon = NULL;
	struct monster *t_mon = monster_target_monster(context);
	struct loc decoy = cave_find_decoy(cave);
	bool decoy_unseen = false;

	if (context->origin.what == SRC_MONSTER) {
		mon = cave_monster(cave, context->origin.which.monster);
	}

	/* Comprobar si un monstruo apunta a otro monstruo */
	if (t_mon) {
		char m_name[80];
		target = t_mon->grid;
		monster_desc(m_name, sizeof(m_name), t_mon, MDESC_TARG);
		if (message) {
			msg("La oscuridad rodea a %s.", m_name);
			message = false;
		}
	}

	/* Comprobar señuelo */
	if (mon && monster_is_decoyed(mon)) {
		target = decoy;
		if (!los(cave, player->grid, decoy) ||
			player->timed[TMD_BLIND]) {
			decoy_unseen = true;
		}
		if (message && !decoy_unseen) {
			msg("La oscuridad rodea al señuelo.");
			message = false;
		}
	}

	if (message) {
		msg("La oscuridad te rodea.");
	}

	/* Oscurecer la habitación */
	light_room(target, false);

	/* Truco - cegar al jugador directamente si es lanzado por el jugador */
	if (context->origin.what == SRC_PLAYER &&
		!player_resists(player, ELEM_DARK)) {
		(void)player_inc_timed(player, TMD_BLIND, 3 + randint1(5),
			true, !context->aware, true);
	}

	/* Asumir visto */
	context->ident = !decoy_unseen;
	return (true);
}

/**
 * Maldice la armadura del jugador
 */
bool effect_handler_CURSE_ARMOR(effect_handler_context_t *context)
{
	struct object *obj;

	char o_name[80];

	/* Maldice la armadura corporal */
	obj = equipped_item_by_slot_name(player, "body");

	/* Nada que maldecir */
	if (!obj) return (true);

	/* Describir */
	object_desc(o_name, sizeof(o_name), obj, ODESC_FULL, player);

	/* Intentar una tirada de salvación para artefactos */
	if (obj->artifact && (randint0(100) < 50)) {
		msg("Un %s intenta %s, ¡pero tu %s resiste los efectos!",
				   "aura negra terrible", "rodear tu armadura", o_name);
	} else {
		int num = randint1(3);
		int max_tries = 20;
		int old_weight = obj->number * object_weight_one(obj);

		msg("¡Un aura negra terrible golpea tu %s!", o_name);

		/* Reducir un poco la bonificación */
		obj->to_a -= randint1(3);

		/* Intentar encontrar suficientes maldiciones apropiadas */
		while (num && max_tries) {
			int pick = randint1(z_info->curse_max - 1);
			int power = 10 * m_bonus(9, player->depth);
			if (!curses[pick].poss[obj->tval]) {
				max_tries--;
				continue;
			}
			append_object_curse(obj, pick, power);
			num--;
		}

		/* Contabilizar un cambio de peso, si lo hay */
		player->upkeep->total_weight +=
			(obj->number * object_weight_one(obj)) - old_weight;

		/* Recalcular bonificaciones */
		player->upkeep->update |= (PU_BONUS);

		/* Recalcular mana */
		player->upkeep->update |= (PU_MANA);

		/* Cosas de ventana */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	}

	context->ident = true;

	return (true);
}


/**
 * Maldice el arma del jugador
 */
bool effect_handler_CURSE_WEAPON(effect_handler_context_t *context)
{
	struct object *obj;

	char o_name[80];

	/* Maldice el arma */
	obj = equipped_item_by_slot_name(player, "weapon");

	/* Nada que maldecir */
	if (!obj) return (true);

	/* Describir */
	object_desc(o_name, sizeof(o_name), obj, ODESC_FULL, player);

	/* Intentar una tirada de salvación */
	if (obj->artifact && (randint0(100) < 50)) {
		msg("Un %s intenta %s, ¡pero tu %s resiste los efectos!",
				   "aura negra terrible", "rodear tu arma", o_name);
	} else {
		int num = randint1(3);
		int max_tries = 20;
		int old_weight = obj->number * object_weight_one(obj);

		msg("¡Un aura negra terrible golpea tu %s!", o_name);

		/* Dañarlo un poco */
		obj->to_h = 0 - randint1(3);
		obj->to_d = 0 - randint1(3);

		/* Maldirlo */
		while (num && max_tries) {
			int pick = randint1(z_info->curse_max - 1);
			int power = 10 * m_bonus(9, player->depth);
			if (!curses[pick].poss[obj->tval]) {
				max_tries--;
				continue;
			}
			append_object_curse(obj, pick, power);
			num--;
		}

		/* Contabilizar un cambio de peso, si lo hay */
		player->upkeep->total_weight +=
			(obj->number * object_weight_one(obj)) - old_weight;

		/* Recalcular bonificaciones */
		player->upkeep->update |= (PU_BONUS);

		/* Recalcular mana */
		player->upkeep->update |= (PU_MANA);

		/* Cosas de ventana */
		player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
	}

	context->ident = true;

	/* Notificar */
	return (true);
}


/**
 * Marcar el arma actual
 */
bool effect_handler_BRAND_WEAPON(effect_handler_context_t *context)
{
	struct object *obj = equipped_item_by_slot_name(player, "weapon");

	/* Seleccionar la marca */
	const char *brand = one_in_(2) ? "Llama" : "Escarcha";

	/* Marcar el arma */
	brand_object(obj, brand);

	context->ident = true;
	return true;
}


/**
 * Marcar algo de munición (no mágica)
 */
bool effect_handler_BRAND_AMMO(effect_handler_context_t *context)
{
	struct object *obj;
	const char *q, *s;
	int itemmode = (USE_INVEN | USE_QUIVER | USE_FLOOR);
	bool used = false;

	/* Seleccionar la marca */
	const char *brand = one_in_(3) ? "Llama" : (one_in_(2) ? "Escarcha" : "Veneno");

	context->ident = true;

	/* Obtener un objeto */
	q = "¿Marcar qué tipo de munición? ";
	s = "No tienes nada para marcar.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_is_ammo, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, tval_is_ammo, itemmode))
		return used;

	/* Marcar la munición */
	brand_object(obj, brand);

	/* Hecho */
	return (true);
}

/**
 * Encantar algunos virote (no mágicos)
 */
bool effect_handler_BRAND_BOLTS(effect_handler_context_t *context)
{
	struct object *obj;
	const char *q, *s;
	int itemmode = (USE_INVEN | USE_QUIVER | USE_FLOOR);
	bool used = false;

	context->ident = true;

	/* Obtener un objeto */
	q = "¿Marcar qué virote? ";
	s = "No tienes virote para marcar.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_is_bolt, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, tval_is_bolt, itemmode))
		return used;

	/* Marcar los virote */
	brand_object(obj, "Llama");

	/* Hecho */
	return (true);
}


/**
 * Convertir un bastón en flechas
 */
bool effect_handler_CREATE_ARROWS(effect_handler_context_t *context)
{
	int lev;
	struct object *obj, *staff, *arrows;
	const char *q, *s;
	int itemmode = (USE_INVEN | USE_FLOOR);
	bool good = false, great = false;
	bool none_left = false;

	/* Obtener un objeto */
	q = "¿Hacer flechas de qué bastón? ";
	s = "No tienes ningún bastón para usar.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_is_staff, itemmode)) {
			return false;
		}
	} else if (!get_item(&obj, q, s, 0, tval_is_staff, itemmode)) {
		return false;
	}

	/* Extraer el "nivel" del objeto */
	lev = obj->kind->level;

	/* Tirar para bueno */
	if (randint1(lev) > 25) {
		good = true;
		/* Tirar para excelente */
		if (randint1(lev) > 50) {
			great = true;
		}
	}

	/* Destruir el bastón */
	if (object_is_carried(player, obj)) {
		staff = gear_object_for_use(player, obj, 1, true, &none_left);
	} else {
		staff = floor_object_for_use(player, obj, 1, true, &none_left);
	}

	if (staff->known) {
		object_delete(player->cave, NULL, &staff->known);
	}
	object_delete(cave, player->cave, &staff);

	/* Hacer algunas flechas */
	arrows = make_object(cave, player->lev, good, great, false, NULL, TV_ARROW);
	drop_near(cave, &arrows, 0, player->grid, true, true);

	return true;
}

/**
 * Extraer energía de un dispositivo mágico
 */
bool effect_handler_TAP_DEVICE(effect_handler_context_t *context)
{
	int lev;
	int energy = 0;
	struct object *obj;
	bool used = false;
	int itemmode = (USE_INVEN | USE_FLOOR);
	const char *q, *s;
	const char *item = "";

	/* Obtener un objeto */
	q = "¿Drenar cargas de qué objeto? ";
	s = "No tienes nada de qué drenar cargas.";
	if (context->cmd) {
		if (cmd_get_item(context->cmd, "tgtitem", &obj, q, s,
				tval_can_have_charges, itemmode)) {
			return used;
		}
	} else if (!get_item(&obj, q, s, 0, tval_can_have_charges, itemmode)) {
		return (used);
	}

	/* Extraer el "nivel" del objeto */
	lev = obj->kind->level;

	/* Extraer la energía del objeto y obtener su nombre genérico. */
	if (tval_is_staff(obj)) {
		energy = (5 + lev) * 3 * obj->pval / 2;
		item = "bastón";
	} else if (tval_is_wand(obj)) {
		energy = (5 + lev) * 3 * obj->pval / 2;
		item = "varita";
	}

	/* Convertir energía en mana. */
	if (energy < 36) {
		/* Requerir una cantidad razonable de energía */
		msg("Ese %s no tenía energía utilizable", item);
	} else {
		/* Si el mana está por debajo del máximo, aumentar el mana y drenar el objeto. */
		if (player->csp < player->msp) {
			/* Drenar el objeto. */
			obj->pval = 0;


			/* Combinar / Reordenar la mochila (más tarde) */
			player->upkeep->notice |= (PN_COMBINE);

			/* Redibujar cosas */
			player->upkeep->redraw |= (PR_INVEN);

			/* Aumentar mana. */
			player->csp += energy / 6;
			player->csp_frac = 0;
			if (player->csp > player->msp) {
				(player->csp = player->msp);
			}

			msg("Sientes que tu cabeza se despeja.");
			used = true;
			player_inc_timed(player, TMD_STUN, randint1(2), true,
				context->origin.what != SRC_PLAYER
				|| !context->aware, true);

			player->upkeep->redraw |= (PR_MANA);
		} else {
			char *cap = string_make(item);
			my_strcap(cap);
			msg("Tu mana ya estaba al máximo. %s no drenado.", cap);
			string_free(cap);
		}
	}

	return (used);
}

/**
 * Realizar un cambio de forma del jugador
 */
bool effect_handler_SHAPECHANGE(effect_handler_context_t *context)
{
	struct player_shape *shape = player_shape_by_idx(context->subtype);
	bool ident = false;

	assert(shape);

	/* Cambiar forma */
	player->shape = lookup_player_shape(shape->name);
	msg("¡Adoptas la forma de %s!", shape->name);
	msg("Tu equipo se fusiona con tu cuerpo.");

	/* Hacer efecto */
	if (shape->effect) {
		(void) effect_do(shape->effect, source_player(), NULL, &ident, true,
						 0, 0, 0, NULL);
	}

	/* Actualizar */
	shape_learn_on_assume(player, shape->name);
	player->upkeep->update |= (PU_BONUS);
	player->upkeep->redraw |= (PR_TITLE | PR_MISC);
	handle_stuff(player);

	return true;
}

/**
 * Tomar control de un monstruo
 */
bool effect_handler_COMMAND(effect_handler_context_t *context)
{
	int amount = effect_calculate_value(context, false);
	struct monster *mon = target_get_monster();

	context->ident = true;

	/* Necesitas elegir un monstruo, no solo apuntar */
	if (!mon) {
		msg("¡Ningún monstruo seleccionado!");
		return false;
	}

	/* Despertar, volverse consciente */
	monster_wake(mon, false, 100);

	/* Tirada de salvación explícita */
	if (randint1(player->lev) < randint1(mon->race->level)) {
		char m_name[80];
		monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
		msg("¡%s resiste tu comando!", m_name);
		/* Tomar un turno y deducir mana cuando el monstruo resiste. */
		return true;
	}

	/* El jugador está comandando */
	player_set_timed(player, TMD_COMMAND, MAX(amount, 0), false, false);

	/* El monstruo está comandado */
	mon_inc_timed(mon, MON_TMD_COMMAND, MAX(amount, 0), 0);

	return true;
}

/**
 * Activación del Anillo Único
 */
bool effect_handler_BIZARRE(effect_handler_context_t *context)
{
	context->ident = true;

	/* Elegir un efecto aleatorio */
	switch (randint1(10))
	{
		case 1:
		case 2:
		{
			/* Mensaje */
			msg("Estás rodeado por un aura maligna.");

			/* Disminuir todas las estadísticas (permanentemente) */
			player_stat_dec(player, STAT_STR, true);
			player_stat_dec(player, STAT_INT, true);
			player_stat_dec(player, STAT_WIS, true);
			player_stat_dec(player, STAT_DEX, true);
			player_stat_dec(player, STAT_CON, true);

			/* Perder algo de experiencia (permanentemente) */
			player_exp_lose(player, player->exp / 4, true);

			return true;
		}

		case 3:
		{
			/* Mensaje */
			msg("Estás rodeado por un aura poderosa.");

			/* Dispersar monstruos */
			effect_simple(EF_PROJECT_LOS, context->origin, "1000", PROJ_DISP_ALL, 0, 0, 0, 0, NULL);

			return true;
		}

		case 4:
		case 5:
		case 6:
		{
			/* Bola de Mana */
			int flg = PROJECT_THRU | PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;
			struct loc target = loc_sum(player->grid, ddgrid[context->dir]);

			/* Pedir un objetivo si no se dio dirección */
			if ((context->dir == DIR_TARGET) && target_okay()) {
				flg &= ~(PROJECT_STOP | PROJECT_THRU);

				target_get(&target);
			}

			/* Apuntar al objetivo, explotar */
			return (project(source_player(), 3, target, 300, PROJ_MANA, flg, 0,
							0, context->obj));
		}

		case 7:
		case 8:
		case 9:
		case 10:
		{
			/* Rayo de Mana */
			int flg = PROJECT_STOP | PROJECT_KILL | PROJECT_THRU;
			struct loc target = loc_sum(player->grid, ddgrid[context->dir]);

			/* Usar un objetivo real */
			if ((context->dir == DIR_TARGET) && target_okay())
				target_get(&target);

			/* Apuntar al objetivo, NO explotar */
			return project(source_player(), 0, target, 250, PROJ_MANA, flg, 0,
						   0, context->obj);
		}
	}

	return false;
}

/**
 * Efecto ficticio, para indicar al código de efectos que elija uno de los
 * siguientes efectos context->value.base según la selección del jugador o, si el efecto
 * no fue iniciado por el jugador, al azar.
 */
bool effect_handler_SELECT(effect_handler_context_t *context)
{
	return true;
}

/**
 * Efecto ficticio, para indicar al código de efectos que establezca un valor para una cadena de
 * efectos siguientes que usar, en lugar de establecer su propio valor.
 * El valor no usará el aumento por dispositivo, lo que no debería ser un problema
 * ya que es poco probable que se use para daño (el caso de uso principal es
 * sincronizar el final de los efectos temporizados).
 */
bool effect_handler_SET_VALUE(effect_handler_context_t *context)
{
	set_value = effect_calculate_value(context, false);
	return true;
}

/**
 * Efecto ficticio, para indicar al código de efectos que borre un valor establecido por el
 * efecto SET_VALUE.
 */
bool effect_handler_CLEAR_VALUE(effect_handler_context_t *context)
{
	set_value = 0;
	return true;
}

/**
 * Mezclar las estadísticas del jugador. Esto solo está destinado para uso por el
 * efecto temporizado, TMD_SCRAMBLE. Otras cadenas de efectos que quieran incurrir en un
 * efecto de mezcla deberían usar TIMED_INC:SCRAMBLE o TIMED_INC_NO_RES:SCRAMBLE.
 */
bool effect_handler_SCRAMBLE_STATS(effect_handler_context_t *context)
{
	player_scramble_stats(player);
	return true;
}

/**
 * Deshacer la mezcla de las estadísticas del jugador. Esto solo está destinado para uso por el
 * efecto temporizado, TMD_SCRAMBLE. Otras cadenas de efectos que quieran deshacer un
 * efecto de mezcla deberían usar CURE:SCRAMBLE (o quizás TIMED_DEC:SCRAMBLE
 * para simplemente reducir la duración de un efecto de mezcla existente).
 */
bool effect_handler_UNSCRAMBLE_STATS(effect_handler_context_t *context)
{
	player_fix_scramble(player);
	return true;
}