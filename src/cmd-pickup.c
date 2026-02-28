/**
 * \file cmd-pickup.c
 * \brief Código de recogida de objetos
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Copyright (c) 2007 Leon Marrick
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
#include "cmds.h"
#include "game-event.h"
#include "game-input.h"
#include "generate.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-util.h"
#include "trap.h"

/**
 * Recoger todo el oro en la ubicación actual del jugador.
 */
static void player_pickup_gold(struct player *p)
{
	int32_t total_gold = 0L;
	char name[30] = "";

	struct object *obj = square_object(cave, p->grid), *next;

	int sound_msg;
	bool verbal = false;
	bool at_most_one = true;

	/* Recoger todos los objetos de oro ordinarios */
	while (obj) {
		struct object_kind *kind = NULL;

		/* Obtener siguiente objeto */
		next = obj->next;

		/* Ignorar si no es tesoro legal */
		kind = lookup_kind(obj->tval, obj->sval);
		if (!tval_is_money(obj) || !kind) {
			obj = next;
			continue;
		}

		/* Múltiples tipos si tenemos un segundo nombre, de lo contrario registrar el nombre */
		if (total_gold && !streq(kind->name, name))
			at_most_one = false;
		else
			my_strcpy(name, kind->name, sizeof(name));

		/* Recordar si el mensaje de retroalimentación es apropiado */
		if (!ignore_item_ok(p, obj))
			verbal = true;

		/* Incrementar valor total */
		total_gold += (int32_t)obj->pval;

		/* Eliminar el oro */
		if (obj->known) {
			square_delete_object(p->cave, p->grid, obj->known, false, false);
		}
		square_delete_object(cave, p->grid, obj, false, false);
		obj = next;
	}

	/* Recoger el oro, si está presente */
	if (total_gold) {
		char buf[100];

		/* Construir un mensaje */
		(void)strnfmt(buf, sizeof(buf),
			"Has encontrado %ld piezas de oro en ", (long)total_gold);

		/* Un tipo de tesoro.. */
		if (at_most_one)
			my_strcat(buf, name, sizeof(buf));
		/* ... o más */
		else
			my_strcat(buf, "tesoros", sizeof(buf));
		my_strcat(buf, ".", sizeof(buf));

		/* Determinar qué sonido reproducir */
		if      (total_gold < 200) sound_msg = MSG_MONEY1;
		else if (total_gold < 600) sound_msg = MSG_MONEY2;
		else                       sound_msg = MSG_MONEY3;

		/* Mostrar el mensaje */
		if (verbal)
			msgt(sound_msg, "%s", buf);

		/* Añadir oro al monedero */
		p->au += total_gold;

		/* Redibujar oro */
		p->upkeep->redraw |= (PR_GOLD);
	}
}


/**
 * Encontrar el objeto especificado en el inventario (no equipo)
 */
static const struct object *find_stack_object_in_inventory(const struct object *obj, const struct object *start)
{
	const struct object *gear_obj;
	for (gear_obj = (start) ? start : player->gear; gear_obj; gear_obj = gear_obj->next) {
		if (!object_is_equipped(player->body, gear_obj) &&
				object_similar(gear_obj, obj, OSTACK_PACK)) {
			/* Encontramos el objeto */
			return gear_obj;
		}
	}

	return NULL;
}


/**
 * Determinar si un objeto puede ser recogido automáticamente y devolver el
 * número a recoger.
 */
static int auto_pickup_okay(const struct object *obj)
{
        /*
	 * Usar las siguientes inscripciones para guiar la recogida, siendo la última
	 * tomada prestada de Unangband:
	 *
	 * !g     no recoger
	 * =g     recoger
	 * =g<n>  (ej. =g5) recoger si se tiene menos de n
	 *
	 * !g tiene prioridad sobre cualquiera de las otras si un objeto está
	 * inscrito con ella y cualquiera de las otras. =g sin valor tiene
	 * prioridad sobre =g<n> si un objeto está inscrito con ambas. En
	 * general, las inscripciones en el objeto en el suelo se examinan primero
	 * y las de un objeto coincidente en la mochila solo se tendrán en
	 * cuenta si las del objeto en el suelo no fuerzan o
	 * rechazan la recogida. Al examinar inscripciones en la mochila, solo
	 * usar las del primer montón.
	 *
	 * La opción del jugador de recoger siempre anula todas esas
	 * inscripciones. La opción del jugador de recoger si está en el inventario
	 * respeta esas inscripciones.
	 */
	int num = inven_carry_num(player, obj);
	unsigned obj_has_auto, obj_has_maxauto;
	int obj_maxauto;

	if (!num) return 0;

	if (OPT(player, pickup_always)) return num;
	if (check_for_inscrip(obj, "!g")) return 0;

	obj_has_auto = check_for_inscrip(obj, "=g");
	obj_maxauto = INT_MAX;
	obj_has_maxauto = check_for_inscrip_with_int(obj, "=g", &obj_maxauto);
	if (obj_has_auto > obj_has_maxauto) return num;

	if (OPT(player, pickup_inven) || obj_has_maxauto) {
		const struct object *gear_obj = find_stack_object_in_inventory(obj, NULL);
		if (!gear_obj) {
			if (obj_has_maxauto) {
				return (num < obj_maxauto) ? num : obj_maxauto;
			}
			return 0;
		}
		if (!check_for_inscrip(gear_obj, "!g")) {
			unsigned int gear_has_auto = check_for_inscrip(gear_obj, "=g");
			unsigned int gear_has_maxauto;
			int gear_maxauto;

			gear_has_maxauto = check_for_inscrip_with_int(gear_obj, "=g", &gear_maxauto);
			if (gear_has_auto > gear_has_maxauto) {
				return num;
			}
			if (obj_has_maxauto || gear_has_maxauto) {
				/* Usar la inscripción de la mochila si se tienen ambas. */
				int max_num = (gear_has_maxauto) ?
					gear_maxauto : obj_maxauto;
				/* Determinar el número total en la mochila. */
				int pack_num = gear_obj->number;

				while (1) {
					if (!gear_obj->next) {
						break;
					}
					gear_obj = find_stack_object_in_inventory(obj, gear_obj->next);
					if (!gear_obj) {
						break;
					}
					pack_num += gear_obj->number;
				}
				if (pack_num >= max_num) {
					return 0;
				}
				return (num < max_num - pack_num) ?
					num : max_num - pack_num;
			}
			return num;
		}
	}

	return 0;
}


/**
 * Mover un objeto de un montón del suelo al equipo del jugador, comprobando primero
 * si se necesita recogida parcial
 */
static void player_pickup_aux(struct player *p, struct object *obj,
							  int auto_max, bool domsg)
{
	int max = inven_carry_num(p, obj);

	/* Confirmar que al menos parte del objeto puede ser recogida */
	if (max == 0)
		quit_fmt("Recogida fallida de %s", obj->kind->name);

	/* Establecer estado de ignorar */
	p->upkeep->notice |= PN_IGNORE;

	/* Permitir que la recogida automática limite el número si quiere */
	if (auto_max && max > auto_max) {
		max = auto_max;
	}

	/* Llevar el objeto, solicitando número si es necesario */
	if (max == obj->number) {
		if (obj->known) {
			square_excise_object(p->cave, p->grid, obj->known);
			delist_object(p->cave, obj->known);
		}
		square_excise_object(cave, p->grid, obj);
		delist_object(cave, obj);
		inven_carry(p, obj, true, domsg);
	} else {
		int num;
		bool dummy;
		struct object *picked_up;

		if (auto_max)
			num = auto_max;
		else
			num = get_quantity(NULL, max);
		if (!num) return;
		picked_up = floor_object_for_use(p, obj, num, false, &dummy);
		inven_carry(p, picked_up, true, domsg);
	}
}

/**
 * Recoger objetos y tesoro del suelo.  -LM-
 *
 * Escanear la lista de objetos en esa casilla del suelo. Recoger oro automáticamente.
 * Recoger objetos automáticamente hasta que el espacio de la mochila esté lleno si
 * la opción de recogida automática está activada; de lo contrario, almacenar objetos en
 * el suelo en una matriz, y contar tanto cuántos hay como cuántos se pueden recoger.
 *
 * Si no se recoge nada, indicar objetos en el suelo. Hacer lo mismo
 * si no tenemos espacio para nada.
 *
 * Recoger múltiples objetos usando el sistema de menús de Tim Baker. Llamar recursivamente
 * a esta función (forzando menús para cualquier número de objetos) hasta que
 * los objetos se hayan ido, la mochila esté llena, o el jugador esté satisfecho.
 *
 * Llevamos la cuenta del número de objetos recogidos para calcular el tiempo empleado.
 * Este recuento se incrementa incluso para la recogida automática, por lo que tenemos cuidado
 * (en "dungeon.c" y en otros lugares) de manejar la recogida como un movimiento
 * automatizado separado o una parte sin coste del comando de quedarse quieto o 'g'et.
 *
 * Notar la falta de oportunidad de que el personaje sea molestado por objetos
 * no marcados. Son verdaderamente "desconocidos".
 *
 * \param p es el jugador que recoge el objeto.
 * \param obj es el objeto a recoger.
 * \param menu es si se debe presentar un menú al jugador.
 */
static uint8_t player_pickup_item(struct player *p, struct object *obj, bool menu)
{
	struct object *current = NULL;

	int floor_max = z_info->floor_size + 1;
	struct object **floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
	int floor_num = 0;

	int i;
	int can_pickup = 0;
	bool call_function_again = false;

	bool domsg = true;

	/* Objetos recogidos. Se usa para determinar el coste de tiempo del comando. */
	uint8_t objs_picked_up = 0;

	/* Siempre saber lo que hay en el suelo */
	square_know_pile(cave, p->grid, NULL);

	/* Siempre recoger oro, sin esfuerzo */
	player_pickup_gold(p);

	/* Nada más que recoger -- regresar */
	if (!square_object(cave, p->grid)) {
		mem_free(floor_list);
		return objs_picked_up;
	}

	/* Se nos da un objeto - recogerlo */
	if (obj) {
		mem_free(floor_list);
		if (inven_carry_num(p, obj) > 0) {
			player_pickup_aux(p, obj, 0, domsg);
			objs_picked_up = 1;
		}
		return objs_picked_up;
	}

	/* Contar objetos que pueden ser al menos parcialmente recogidos. */
	floor_num = scan_floor(floor_list, floor_max, p, OFLOOR_VISIBLE, NULL);
	for (i = 0; i < floor_num; i++)
	    if (inven_carry_num(p, floor_list[i]) > 0)
			can_pickup++;

	if (!can_pickup) {
	    event_signal(EVENT_SEEFLOOR);
		mem_free(floor_list);
	    return objs_picked_up;
	}

	/* Usar una interfaz de menú para múltiples objetos, o recoger objetos individuales */
	if (!menu && !current) {
		if (floor_num > 1)
			menu = true;
		else
			current = floor_list[0];
	}

	/* Mostrar una lista si se solicita. */
	if (menu && !current) {
		const char *q, *s;
		struct object *obj_local = NULL;

		/* Obtener un objeto o salir. */
		q = "¿Coger qué objeto?";
		s = "No ves nada ahí.";
		if (!get_item(&obj_local, q, s, CMD_PICKUP, inven_carry_okay, USE_FLOOR)) {
			mem_free(floor_list);
			return (objs_picked_up);
		}

		current = obj_local;
		call_function_again = true;

		/* Con una lista, no necesitamos mensajes de recogida explícitos */
		domsg = true;
	}

	/* Recoger objeto, si es legal */
	if (current) {
		/* Recoger el objeto */
		player_pickup_aux(p, current, 0, domsg);

		/* Indicar un objeto recogido. */
		objs_picked_up = 1;
	}

	/*
	 * Si se solicita, llamar a esta función recursivamente. Contar objetos recogidos.
	 * Forzar la visualización de un menú en todos los casos.
	 */
	if (call_function_again)
		objs_picked_up += player_pickup_item(p, NULL, true);

	mem_free(floor_list);

	/* Indicar cuántos objetos han sido recogidos. */
	return (objs_picked_up);
}

/**
 * Recoger todo en el suelo que no requiera acción del jugador
 */
int do_autopickup(struct player *p)
{
	struct object *obj, *next;
	uint8_t objs_picked_up = 0;

	/* Nada que recoger -- regresar */
	if (!square_object(cave, p->grid))
		return 0;

	/* Siempre recoger oro, sin esfuerzo */
	player_pickup_gold(p);

	/* Escanear los objetos restantes */
	obj = square_object(cave, p->grid);
	while (obj) {
		next = obj->next;

		/* Ignorar todos los objetos ocultos y no-objetos */
		if (!ignore_item_ok(p, obj)) {
			int auto_num;

			/* Molestar */
			disturb(p);

			/* Recoger automáticamente objetos en la mochila */
			auto_num = auto_pickup_okay(obj);
			if (auto_num) {
				/* Recoger el objeto (tanto como sea posible) con mensaje */
				player_pickup_aux(p, obj, auto_num, true);
				objs_picked_up++;
			}
		}
		obj = next;
	}

	return objs_picked_up;
}

/**
 * Recoger objetos a petición del jugador
 */
void do_cmd_pickup(struct command *cmd)
{
	int energy_cost = 0;
	struct object *obj = NULL;

	/* Ver si ya tenemos un objeto */
	(void) cmd_get_arg_item(cmd, "item", &obj);

	/* Recoger objetos del suelo con un menú para múltiples objetos */
	energy_cost += player_pickup_item(player, obj, false)
		* z_info->move_energy / 10;

	/* Límite */
	if (energy_cost > z_info->move_energy) energy_cost = z_info->move_energy;

	/* Cobrar esta cantidad de energía. */
	player->upkeep->energy_use = energy_cost;

	/* Redibujar la lista de objetos usando la bandera de mantenimiento para que la actualización
	 * pueda ser algo coalescente. Usar event_signal(EVENT_ITEMLIST) para forzar la actualización. */
	player->upkeep->redraw |= (PR_ITEMLIST);
}

/**
 * Recoger o mirar objetos en una casilla cuando el jugador pisa sobre ella
 */
void do_cmd_autopickup(struct command *cmd)
{
	/* Obtener las cosas obvias */
	player->upkeep->energy_use = do_autopickup(player)
		* z_info->move_energy / 10;
	if (player->upkeep->energy_use > z_info->move_energy)
		player->upkeep->energy_use = z_info->move_energy;

	/* Mirar o sentir lo que queda */
	event_signal(EVENT_SEEFLOOR);

	/* Redibujar la lista de objetos usando la bandera de mantenimiento para que la actualización
	 * pueda ser algo coalescente. Usar event_signal(EVENT_ITEMLIST) para forzar la actualización. */
	player->upkeep->redraw |= (PR_ITEMLIST);
}