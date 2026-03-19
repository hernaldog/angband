/**
 * \file obj-gear.c
 * \brief gestión del inventario, equipo y aljaba
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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
#include "game-event.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-util.h"

static const struct slot_info {
	int index;
	bool acid_vuln;
	bool name_in_desc;
	const char *mention;
	const char *heavy_describe;
	const char *describe;
} slot_table[] = {
	#define EQUIP(a, b, c, d, e, f) { EQUIP_##a, b, c, d, e, f },
	#include "list-equip-slots.h"
	#undef EQUIP
	{ EQUIP_MAX, false, false, NULL, NULL, NULL }
};

/**
 * Devuelve el número de ranura para un nombre dado, o termina el juego
 */
int slot_by_name(struct player *p, const char *name)
{
	int i;

	/* Buscar la ranura con el nombre correcto */
	for (i = 0; i < p->body.count; i++) {
		if (streq(name, p->body.slots[i].name)) {
			break;
		}
	}

	assert(i < p->body.count);

	/* Índice para esa ranura */
	return i;
}

/**
 * Obtiene una ranura del tipo dado, preferentemente vacía a menos que full sea verdadero
 */
static int slot_by_type(struct player *p, int type, bool full)
{
	int i, fallback = p->body.count;

	/* Buscar un tipo de ranura correcto */
	for (i = 0; i < p->body.count; i++) {
		if (type == p->body.slots[i].type) {
			if (full) {
				/* Ranura ocupada encontrada */
				if (p->body.slots[i].obj != NULL) break;
			} else {
				/* Ranura vacía encontrada */
				if (p->body.slots[i].obj == NULL) break;
			}
			/* No es correcto para lleno/vacío, pero sigue siendo el tipo correcto */
			if (fallback == p->body.count)
				fallback = i;
		}
	}

	/* Índice para la mejor ranura que encontramos, o p->body.count si no se encontró ninguna */
	return (i != p->body.count) ? i : fallback;
}

/**
 * Indica si una ranura es de un tipo dado.
 *
 * \param p es el jugador a probar; si es NULL, asumirá el plan corporal predeterminado.
 * \param slot es el índice de la ranura para el jugador.
 * \param type es una de las constantes EQUIP_* de list-equip-slots.h.
 * \return verdadero si la ranura puede contener ese tipo; falso en caso contrario
 */
bool slot_type_is(struct player *p, int slot, int type)
{
	/* Asumir cuerpo predeterminado si no hay jugador */
	struct player_body body = p ? p->body : bodies[0];

	return body.slots[slot].type == type ? true : false;
}

/**
 * Obtiene el objeto en una ranura específica (si lo hay). Termina si el índice de ranura no es válido.
 */
struct object *slot_object(struct player *p, int slot)
{
	/* Verificar límites */
	assert(slot >= 0 && slot < p->body.count);

	/* Asegurar un cuerpo válido */
	if (p->body.slots && p->body.slots[slot].obj) {
		return p->body.slots[slot].obj;
	}

	return NULL;
}

struct object *equipped_item_by_slot_name(struct player *p, const char *name)
{
	/* Asegurar un cuerpo válido */
	if (p->body.slots) {
		return slot_object(p, slot_by_name(p, name));
	}

	return NULL;
}

int object_slot(struct player_body body, const struct object *obj)
{
	int i;

	for (i = 0; i < body.count; i++) {
		if (obj == body.slots[i].obj) {
			break;
		}
	}

	return i;
}

bool object_is_equipped(struct player_body body, const struct object *obj)
{
	return object_slot(body, obj) < body.count;
}

bool object_is_carried(struct player *p, const struct object *obj)
{
	return pile_contains(p->gear, obj);
}

/**
 * Verifica si un objeto está en la aljaba
 */
bool object_is_in_quiver(struct player *p, const struct object *obj)
{
	int i;

	for (i = 0; i < z_info->quiver_size; i++) {
		if (obj == p->upkeep->quiver[i]) {
			return true;
		}
	}

	return false;
}

/**
 * Obtiene el número total de objetos en la mochila o aljaba que son similares al
 * objeto dado.
 *
 * \param p es el jugador cuyo inventario se utiliza para el cálculo.
 * \param obj es la plantilla para los objetos a buscar.
 * \param ignore_inscrip si es verdadero, ignora las inscripciones al probar si
 * un objeto es similar; de lo contrario, prueba también las inscripciones.
 * \param first si no es NULL, se establece en la primera pila como obj (por orden en
 * la aljaba o mochila con prioridad de la aljaba sobre la mochila; si la mochila
 * y la aljaba no se han calculado, será la primera pila no equipada
 * en el equipo).
 */
uint16_t object_pack_total(struct player *p, const struct object *obj,
		bool ignore_inscrip, struct object **first)
{
	uint16_t total = 0;
	char first_label = '\0';
	struct object *cursor;

	if (first) {
		*first = NULL;
	}
	for (cursor = p->gear; cursor; cursor = cursor->next) {
		bool like;

		if (cursor == obj) {
			/*
			 * object_similar() excluye cursor == obj, así que si
			 * obj no está equipado, contabilizarlo aquí.
			 */
			like = !object_is_equipped(p->body, obj);
		} else if (ignore_inscrip) {
			like = object_similar(obj, cursor, OSTACK_PACK);
		} else {
			like = object_stackable(obj, cursor, OSTACK_PACK);
		}
		if (like) {
			total += cursor->number;
			if (first) {
				char test_label = gear_to_label(p, cursor);

				if (!*first) {
					*first = cursor;
					first_label = test_label;
				} else {
					if (test_label >= 'a'
							&& test_label <= 'z') {
						if (first_label == '\0'
								|| (first_label >= 'a'
								&& first_label <= 'z'
								&& test_label < first_label)) {
							*first = cursor;
							first_label = test_label;
						}
					} else if (test_label >= '0'
							&& test_label <= '9') {
						if (first_label == '\0'
								|| (first_label >= 'a'
								&& first_label <= 'z')
								|| (first_label >= '0'
								&& first_label <= '9'
								&& test_label < first_label)) {
							*first = cursor;
							first_label = test_label;
						}
					}
				}
			}
		}
	}

	return total;
}

/**
 * Calcula el número de espacios de mochila utilizados por el equipo actual.
 *
 * Nótese que esta función no verifica que haya espacios adecuados en la
 * aljaba, solo la cantidad total de proyectiles.
 */
int pack_slots_used(const struct player *p)
{
	const struct object *obj;
	int i, pack_slots = 0;
	int quiver_ammo = 0;

	for (obj = p->gear; obj; obj = obj->next) {
		bool found = false;
		/* El equipo no cuenta */
		if (!object_is_equipped(p->body, obj)) {
			/* Verificar si está en la aljaba */
			if (tval_is_ammo(obj) ||
					of_has(obj->flags, OF_THROWING)) {
				for (i = 0; i < z_info->quiver_size; i++) {
					if (p->upkeep->quiver[i] == obj) {
						quiver_ammo += obj->number *
							(tval_is_ammo(obj) ?
							1 : z_info->thrown_quiver_mult);
						found = true;
						break;
					}
				}
			}
			if (!found) {
				/* Contar espacios regulares */
				pack_slots++;
			}
		}
	}

	/* Espacios completos */
	pack_slots += quiver_ammo / z_info->quiver_slot_size;

	/* Más uno para cualquier resto */
	if (quiver_ammo % z_info->quiver_slot_size) {
		pack_slots++;
	}

	return pack_slots;
}

/*
 * Devuelve una cadena que menciona cómo se lleva un objeto dado
 */
const char *equip_mention(struct player *p, int slot)
{
	int type = p->body.slots[slot].type;

	/* Pesado */
	if ((type == EQUIP_WEAPON && p->state.heavy_wield) ||
			(type == EQUIP_WEAPON && p->state.heavy_shoot))
		return slot_table[type].heavy_describe;
	else if (slot_table[type].name_in_desc)
		return format(slot_table[type].mention, p->body.slots[slot].name);
	else
		return slot_table[type].mention;
}


/*
 * Devuelve una cadena que describe cómo se lleva puesto un objeto dado.
 * Actualmente, solo se usa para objetos en el equipo, no en el inventario.
 */
const char *equip_describe(struct player *p, int slot)
{
	int type = p->body.slots[slot].type;

	/* Pesado */
	if ((type == EQUIP_WEAPON && p->state.heavy_wield) ||
			(type == EQUIP_WEAPON && p->state.heavy_shoot))
		return slot_table[type].heavy_describe;
	else if (slot_table[type].name_in_desc)
		return format(slot_table[type].describe, p->body.slots[slot].name);
	else
		return slot_table[type].describe;
}

/**
 * Determina qué ranura de equipo (si la hay) prefiere un objeto. La ranura podría (o
 * podría no) estar abierta, pero es una ranura en la que el objeto podría equiparse.
 *
 * Para objetos donde múltiples ranuras podrían funcionar (ej., anillos), la función
 * intentará devolver una ranura abierta si es posible.
 */
int wield_slot(const struct object *obj)
{
	/* Ranura para equipo */
	switch (obj->tval)
	{
		case TV_BOW: return slot_by_type(player, EQUIP_BOW, false);
		case TV_AMULET: return slot_by_type(player, EQUIP_AMULET, false);
		case TV_CLOAK: return slot_by_type(player, EQUIP_CLOAK, false);
		case TV_SHIELD: return slot_by_type(player, EQUIP_SHIELD, false);
		case TV_GLOVES: return slot_by_type(player, EQUIP_GLOVES, false);
		case TV_BOOTS: return slot_by_type(player, EQUIP_BOOTS, false);
	}

	if (tval_is_melee_weapon(obj))
		return slot_by_type(player, EQUIP_WEAPON, false);
	else if (tval_is_ring(obj))
		return slot_by_type(player, EQUIP_RING, false);
	else if (tval_is_light(obj))
		return slot_by_type(player, EQUIP_LIGHT, false);
	else if (tval_is_body_armor(obj))
		return slot_by_type(player, EQUIP_BODY_ARMOR, false);
	else if (tval_is_head_armor(obj))
		return slot_by_type(player, EQUIP_HAT, false);

	/* No hay ranura disponible */
	return -1;
}


/**
 * El ácido ha golpeado al jugador, intenta afectar alguna armadura.
 *
 * Nótese que la "armadura base" de un objeto nunca cambia.
 * Si alguna armadura es dañada (o resiste), el jugador recibe menos daño.
 */
bool minus_ac(struct player *p)
{
	int i, count = 0;
	struct object *obj = NULL;

	/* Evitar fallo durante cálculos de poder de monstruo */
	if (!p->gear) return false;

	/* Contar las ranuras de armadura */
	for (i = 0; i < p->body.count; i++) {
		/* Ignorar no-armadura */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;

		/* Añadir */
		count++;
	}

	/* Elegir una al azar */
	for (i = p->body.count - 1; i >= 0; i--) {
		/* Ignorar no-armadura */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;

		if (one_in_(count--)) break;
	}

	/* Obtener el objeto */
	obj = slot_object(p, i);

	/* Si aún podemos dañar el objeto */
	if (obj && (obj->ac + obj->to_a > 0)) {
		char o_name[80];
		object_desc(o_name, sizeof(o_name), obj, ODESC_BASE, p);

		/* El objeto resiste */
		if (obj->el_info[ELEM_ACID].flags & EL_INFO_IGNORE) {
			msg("¡Tu %s no resulta afectado!", o_name);
		} else {
			msg("¡Tu %s está dañado!", o_name);

			/* Dañar el objeto */
			obj->to_a--;
			if (p->obj_k->to_a)
				obj->known->to_a = obj->to_a;

			p->upkeep->update |= (PU_BONUS);
			p->upkeep->redraw |= (PR_EQUIP);
		}

		/* Hubo un efecto */
		return true;
	} else {
		/* Sin daño ni efecto */
		return false;
	}
}

/**
 * Convierte un objeto del equipo en una etiqueta de un carácter.
 */
char gear_to_label(struct player *p, struct object *obj)
{
	/* Omitir teclas de movimiento de dirección al estilo roguelike. */
	const char labels[] =
		 "abcdefgimnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	/* El equipo es fácil */
	if (object_is_equipped(p->body, obj)) {
		return labels[equipped_item_slot(p->body, obj)];
	}

	/* Verificar la aljaba */
	for (i = 0; i < z_info->quiver_size; i++) {
		if (p->upkeep->quiver[i] == obj) {
			return I2D(i);
		}
	}

	/* Verificar el inventario */
	for (i = 0; i < z_info->pack_size; i++) {
		if (p->upkeep->inven[i] == obj) {
			return labels[i];
		}
	}

	return '\0';
}

/**
 * Elimina un objeto de la lista de equipo, dejándolo sin adjuntar
 * \param p el jugador a afectar
 * \param obj el objeto a eliminar
 * \return si se eliminó un objeto
 */
static bool gear_excise_object(struct player *p, struct object *obj)
{
	int i;

	pile_excise(&p->gear_k, obj->known);
	pile_excise(&p->gear, obj);

	/* Cambiar el peso */
	p->upkeep->total_weight -= obj->number * object_weight_one(obj);

	/* Asegurarse de que no siga equipado */
	for (i = 0; i < p->body.count; i++) {
		if (slot_object(p, i) == obj) {
			p->body.slots[i].obj = NULL;
			p->upkeep->equip_cnt--;
		}
	}

	/* Actualizar el equipo */
	calc_inventory(p);

	/* Mantenimiento */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->notice |= (PN_COMBINE);
	p->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	return true;
}

struct object *gear_last_item(struct player *p)
{
	return pile_last_item(p->gear);
}

void gear_insert_end(struct player *p, struct object *obj)
{
	pile_insert_end(&p->gear, obj);
	pile_insert_end(&p->gear_k, obj->known);
}

/**
 * Elimina una cantidad de un objeto del inventario o la aljaba, devolviendo
 * un objeto separado que puede usarse.
 *
 * Opcionalmente describe lo que queda.
 */
struct object *gear_object_for_use(struct player *p, struct object *obj,
	int num, bool message, bool *none_left)
{
	struct object *usable;
	struct object *first_remainder = NULL;
	char name[80];
	char label = gear_to_label(p, obj);
	bool artifact = (obj->known->artifact != NULL);

	/* Verificar límites */
	num = MIN(num, obj->number);

	/* Separar un objeto utilizable si es necesario */
	if (obj->number > num) {
		usable = object_split(obj, num);

		/* Cambiar el peso */
		p->upkeep->total_weight -= num * object_weight_one(obj);

		if (message) {
			uint16_t total;

			/*
			 * No mostrar total agregado en la mochila si está equipado o
			 * si la descripción podría tener un número de cargas
			 * o aviso de recarga específico de la pila (no
			 * agregando esas cantidades, por lo que habría
			 * confusión si se agrega el conteo).
			 */
			if (object_is_equipped(p->body, obj)
					|| tval_can_have_charges(obj)
					|| tval_is_rod(obj)
					|| obj->timeout > 0) {
				total = obj->number;
			} else {
				total = object_pack_total(p, obj, false,
					&first_remainder);
				assert(total >= first_remainder->number);
				if (total == first_remainder->number) {
					first_remainder = NULL;
				}
			}
			object_desc(name, sizeof(name), obj,
				ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
				(total << 16), p);
		}
	} else {
		if (message) {
			if (artifact) {
				object_desc(name, sizeof(name), obj,
					ODESC_FULL | ODESC_SINGULAR, p);
			} else {
				uint16_t total;

				/*
				 * Usar la misma lógica que arriba para mostrar un
				 * total agregado.
				 */
				if (object_is_equipped(p->body, obj)
						|| tval_can_have_charges(obj)
						|| tval_is_rod(obj)
						|| obj->timeout > 0) {
					total = obj->number;
				} else {
					total = object_pack_total(p, obj,
						false, &first_remainder);
				}

				assert(total >= num);
				total -= num;
				if (!total || total <= first_remainder->number) {
					first_remainder = NULL;
				}
				object_desc(name, sizeof(name), obj,
					ODESC_PREFIX | ODESC_FULL |
					ODESC_ALTNUM | (total << 16), p);
			}
		}

		/* Estamos usando toda la pila */
		usable = obj;
		gear_excise_object(p, usable);
		*none_left = true;

		/* Dejar de rastrear objeto */
		if (tracked_object_is(p->upkeep, obj))
			track_object(p->upkeep, NULL);

		/* El inventario ha cambiado, así que deshabilitar el comando de repetición */
		cmd_disable_repeat();
	}

	/* Mantenimiento */
	p->upkeep->update |= (PU_BONUS);
	p->upkeep->notice |= (PN_COMBINE);
	p->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	/* Imprimir un mensaje si se desea */
	if (message) {
		if (artifact) {
			msg("Ya no tienes6 %s (%c).", name, label);
		} else if (first_remainder) {
			label = gear_to_label(p, first_remainder);
			msg("Tienes4 %s (1er %c).", name, label);
		} else {
			msg("Tienes5 %s (%c).", name, label);
		}
	}

	return usable;
}

/**
 * Verifica cuántos proyectiles se pueden poner en la aljaba con un límite sobre si
 * la aljaba puede expandirse para ocupar más espacios en la mochila.
 *
 * \param p Es el jugador con la aljaba a usar.
 * \param obj Es el objeto a añadir.
 * \param n_add_pack Al entrar, *n_add_pack es el número máximo de espacios adicionales
 * de mochila para ceder a la aljaba. Al salir, *n_add_pack será el número
 * de esos espacios que no se usaron para expandir la aljaba.
 * \param n_to_quiver Al salir, *n_to_quiver será el número que se puede
 * añadir a la aljaba. No será mayor que obj->number. El valor de
 * *n_to_quiver al entrar no se usa.
 */
static void quiver_absorb_num(const struct player *p, const struct object *obj,
		int *n_add_pack, int *n_to_quiver)
{
	bool ammo = tval_is_ammo(obj);

	/* Debe ser munición o apto para lanzar */
	if (ammo || of_has(obj->flags, OF_THROWING)) {
		int i, quiver_count = 0, space_free = 0, n_empty = 0;
		int desired_slot = preferred_quiver_slot(obj);
		bool displaces = false;

		/* Contar el espacio actual donde este objeto podría ir. */
		for (i = 0; i < z_info->quiver_size; i++) {
			const struct object *quiver_obj = p->upkeep->quiver[i];
			if (quiver_obj) {
				int mult = tval_is_ammo(quiver_obj) ?
					1 : z_info->thrown_quiver_mult;

				quiver_count += quiver_obj->number * mult;
				if (object_stackable(quiver_obj, obj, OSTACK_PACK)) {
					assert(quiver_obj->number * mult <=
						z_info->quiver_slot_size);
					space_free += z_info->quiver_slot_size -
						quiver_obj->number * mult;
				} else if (desired_slot == i &&
						preferred_quiver_slot(quiver_obj) != i) {
					/*
					 * El objeto a añadir prefiere ir
					 * en esta ranura, pero está ocupada por
					 * algo que podría ser desplazado
					 * a otra ranura de la aljaba, si hay una
					 * disponible.
					 */
					displaces = true;
					assert(quiver_obj->number * mult <=
						z_info->quiver_slot_size);
					/*
					 * Evitar doble conteo en el caso de munición
					 * ya que el espacio vacío, si lo hay,
					 * para la pila desplazada se trata
					 * como completamente disponible.
					 */
					if (ammo) {
						space_free += z_info->quiver_slot_size
							- quiver_obj->number
							* mult;
					} else {
						space_free += z_info->quiver_slot_size;
					}
				}
			} else {
				++n_empty;
				/*
				 * La munición puede caber en cualquier espacio vacío de la aljaba.
				 * Los objetos arrojadizos que no son munición están restringidos a
				 * su ranura preferida.
				 */
				if (ammo || desired_slot == i) {
					space_free += z_info->quiver_slot_size;
				}
			}
		}

		/*
		 * Solo es posible añadir si hay espacio libre en la aljaba
		 * y se está desplazando una pila con un espacio de aljaba vacío
		 * disponible para ella o no se está desplazando una pila en absoluto.
		 */
		if (space_free && ((displaces && n_empty) || !displaces)) {
			int mult = ammo ? 1 : z_info->thrown_quiver_mult;
			/*
			 * Cuando quiver_count % quiver_slot_size es cero, añadir
			 * cualquier cosa requerirá un espacio de mochila.
			 */
			int remainder = quiver_count % z_info->quiver_slot_size;
			int limit_from_pack = (remainder) ?
				z_info->quiver_slot_size - remainder : 0;

			if (*n_add_pack > 0) {
				limit_from_pack += *n_add_pack *
					z_info->quiver_slot_size;
			}

			/* Devolver el número o cantidad que cabe. */
			space_free = MIN(space_free, limit_from_pack);
			*n_to_quiver = MIN(obj->number, space_free / mult);
			*n_add_pack -= (*n_to_quiver * mult +
				z_info->quiver_slot_size - 1 -
				remainder) / z_info->quiver_slot_size;
			return;
		}
	}

	/* No apto para la aljaba o sin espacio */
	*n_to_quiver = 0;
}

/**
 * Calcula cuánto de un objeto se puede llevar en el inventario o la aljaba.
 */
int inven_carry_num(const struct player *p, const struct object *obj)
{
	int n_free_slot = z_info->pack_size - pack_slots_used(p);
	int num_to_quiver, num_left, i;

	/* El tesoro siempre se puede recoger. */
	if (tval_is_money(obj) && lookup_kind(obj->tval, obj->sval)) {
		return obj->number;
	}

	/* Absorber tantos como podamos en la aljaba. */
	quiver_absorb_num(p, obj, &n_free_slot, &num_to_quiver);

	/* La aljaba recibirá todo, o la mochila puede contener lo que queda. */
	if (num_to_quiver == obj->number || n_free_slot > 0) {
		return obj->number;
	}

	/* Ver si podemos añadir a un espacio de inventario parcialmente lleno. */
	num_left = obj->number - num_to_quiver;
	for (i = 0; i < z_info->pack_size; i++) {
		struct object *inven_obj = p->upkeep->inven[i];
		if (inven_obj && object_stackable(inven_obj, obj, OSTACK_PACK)) {
			num_left -= inven_obj->kind->base->max_stack -
				inven_obj->number;
			if (num_left <= 0) break;
		}
	}

	/* Devolver el número que podemos absorber */
	return obj->number - MAX(num_left, 0);
}

/**
 * Verifica si tenemos espacio para algo de un objeto en la mochila.
 */
bool inven_carry_okay(const struct object *obj)
{
	return inven_carry_num(player, obj) > 0;
}

/**
 * Describe las cargas en un objeto en el inventario.
 */
void inven_item_charges(struct object *obj)
{
	/* Requiere varita/varal */
	if (tval_can_have_charges(obj) && object_flavor_is_aware(obj)) {
		msg("Te quedan %d carga%s.",
				obj->pval,
				PLURAL(obj->pval));
	}
}

/**
 * Añade un objeto al inventario del jugador.
 *
 * Si el nuevo objeto puede combinarse con un objeto existente en el inventario,
 * lo hará, usando object_mergeable() y object_absorb(); de lo contrario,
 * el objeto se colocará en el primer índice disponible del arreglo de equipo.
 *
 * Esta función puede usarse para "sobrecargar" la mochila del jugador, pero solo
 * una vez, y tal acción debe activar el código de "desbordamiento" inmediatamente.
 * Nótese que cuando la mochila se está "sobrecargando", el nuevo objeto debe
 * colocarse en el espacio de "desbordamiento", y el "desbordamiento" debe tener lugar
 * antes de que la mochila se reordene, pero (opcionalmente) después de que la mochila se
 * combine. Esto puede ser complicado. Ver "dungeon.c" para información.
 *
 * Nótese que este código elimina cualquier información de ubicación del objeto una vez
 * que se coloca en el inventario, pero no se responsabiliza de eliminar
 * el objeto de cualquier otra pila en la que estuviera.
 */
void inven_carry(struct player *p, struct object *obj, bool absorb,
				 bool message)
{
	bool combining = false;

	/* Verificar para combinar, si corresponde */
	if (absorb) {
		struct object *combine_item = NULL;

		struct object *gear_obj = p->gear;
		while ((combine_item == NULL) && (gear_obj != NULL)) {
			object_stack_t stack_mode =
				object_is_in_quiver(p, gear_obj) ?
				OSTACK_QUIVER : OSTACK_PACK;

			if (!object_is_equipped(p->body, gear_obj) &&
					object_mergeable(gear_obj, obj, stack_mode)) {
				combine_item = gear_obj;
			}

			gear_obj = gear_obj->next;
		}

		if (combine_item) {
			/* Aumentar el peso */
			p->upkeep->total_weight +=
				obj->number * object_weight_one(obj);

			/* Combinar los objetos, y sus versiones conocidas */
			object_absorb(combine_item->known, obj->known);
			obj->known = NULL;
			object_absorb(combine_item, obj);

			/* Asegurar que los números están alineados (no debería ser necesario, pero seguro) */
			combine_item->known->number = combine_item->number;

			obj = combine_item;
			combining = true;
		}
	}

	/* No logramos encontrar un objeto con el que combinar */
	if (!combining) {
		/* Paranoia */
		assert(pack_slots_used(p) <= z_info->pack_size);

		gear_insert_end(p, obj);
		apply_autoinscription(p, obj);

		/* Eliminar detalles del objeto en la cueva */
		obj->held_m_idx = 0;
		obj->grid = loc(0, 0);
		obj->known->grid = loc(0, 0);

		/* Actualizar el inventario */
		p->upkeep->total_weight += obj->number * object_weight_one(obj);
		p->upkeep->notice |= (PN_COMBINE);

		/* Los hobbits identifican setas al recoger, los gnomos identifican varitas y varales al recoger */
		if (!object_flavor_is_aware(obj)) {
			if (player_has(p, PF_KNOW_MUSHROOM) && tval_is_mushroom(obj)) {
				object_flavor_aware(p, obj);
				msg("¡Setas para desayunar!");
			} else if (player_has(p, PF_KNOW_ZAPPER) && tval_is_zapper(obj))
				object_flavor_aware(p, obj);
		}
	}

	p->upkeep->update |= (PU_BONUS | PU_INVEN);
	p->upkeep->redraw |= (PR_INVEN);
	update_stuff(p);

	if (message) {
		char o_name[80];
		struct object *first;
		uint16_t total;
		char label;

		/*
		 * Mostrar un total agregado si la descripción no tiene
		 * un aviso de carga/recarga que sea específico de la pila.
		 */
		if (tval_can_have_charges(obj) || tval_is_rod(obj)
				|| obj->timeout > 0) {
			total = obj->number;
			first = obj;
		} else {
			total = object_pack_total(p, obj, false, &first);
		}
		assert(first && total >= first->number);
		object_desc(o_name, sizeof(o_name), obj,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
			(total << 16), p);
		label = gear_to_label(p, first);
		if (total > first->number) {
			msg("Recogiste %s (1er %c).", o_name, label);
		} else {
			assert(first == obj);
			msg("Recogiste %s (%c).", o_name, label);
		}
	}

	if (object_is_in_quiver(p, obj))
		sound(MSG_QUIVER);
}


/**
 * Empuña o usa un solo objeto de la mochila o del suelo
 */
void inven_wield(struct object *obj, int slot)
{
	struct object *wielded, *old = player->body.slots[slot].obj;

	const char *fmt;
	char o_name[80];
	bool dummy = false;

	/* Aumentar contador de equipo si el espacio está vacío */
	if (old == NULL)
		player->upkeep->equip_cnt++;

	/* Tomar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Es un objeto del equipo o un objeto del suelo */
	if (object_is_carried(player, obj)) {
		/* Separar un nuevo objeto si es necesario */
		if (obj->number > 1) {
			wielded = gear_object_for_use(player, obj, 1, false,
				&dummy);

			/* Todavía se lleva; mantener su peso en el total. */
			assert(wielded->number == 1);
			player->upkeep->total_weight +=
				object_weight_one(wielded);

			/* El nuevo objeto necesita nuevas entradas de equipo y equipo conocido */
			wielded->next = obj->next;
			obj->next = wielded;
			wielded->prev = obj;
			if (wielded->next)
				(wielded->next)->prev = wielded;
			wielded->known->next = obj->known->next;
			obj->known->next = wielded->known;
			wielded->known->prev = obj->known;
			if (wielded->known->next)
				(wielded->known->next)->prev = wielded->known;
		} else {
			/* Simplemente usar el objeto directamente */
			wielded = obj;
		}
	} else {
		/* Obtener un objeto del suelo y llevarlo */
		wielded = floor_object_for_use(player, obj, 1, false, &dummy);
		inven_carry(player, wielded, false, false);
	}

	/* Usar las cosas nuevas */
	player->body.slots[slot].obj = wielded;

	/* Hacer cualquier ID al empuñar */
	object_learn_on_wield(player, wielded);

	/* Dónde está el objeto ahora */
	if (tval_is_melee_weapon(wielded))
		fmt = "Estás empuñando %s (%c).";
	else if (wielded->tval == TV_BOW)
		fmt = "Estás disparando con %s (%c).";
	else if (tval_is_light(wielded))
		fmt = "Tu fuente de luz es %s (%c).";
	else
		fmt = "Llevas puesto %s (%c).";

	/* Describir el resultado */
	object_desc(o_name, sizeof(o_name), wielded,
		ODESC_PREFIX | ODESC_FULL, player);

	/* Mensaje */
	msgt(MSG_WIELD, fmt, o_name, gear_to_label(player, wielded));

	/* La bandera pegajosa recibe una mención especial */
	if (of_has(wielded->flags, OF_STICKY)) {
		/* Advertir al jugador */
		msgt(MSG_CURSED, "¡Vaya! ¡Se siente mortalmente frío!");
	}

	/* Ver si tenemos que desbordar la mochila */
	combine_pack(player);
	pack_overflow(old);

	/* Recalcular bonificaciones, antorcha, maná, equipo */
	player->upkeep->notice |= (PN_IGNORE);
	player->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_ARMOR);
	player->upkeep->redraw |= (PR_STATS | PR_HP | PR_MANA | PR_SPEED);
	update_stuff(player);

	/* Deshabilitar repeticiones */
	cmd_disable_repeat();
}


/**
 * Quita un objeto de equipo no maldito
 *
 * Nótese que quitar un objeto cuando está "lleno" puede hacer que ese objeto
 * caiga al suelo.
 *
 * Nótese también que esta función no intenta combinar el objeto quitado
 * con otros objetos del inventario; eso debe hacerlo la función que llama.
 */
void inven_takeoff(struct object *obj)
{
	int slot = equipped_item_slot(player->body, obj);
	const char *act;
	char o_name[80];

	/* Paranoia */
	if (slot == player->body.count) return;

	/* Describir el objeto */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Describir la eliminación por ranura */
	if (slot_type_is(player, slot, EQUIP_WEAPON))
		act = "Estabas empuñando";
	else if (slot_type_is(player, slot, EQUIP_BOW))
		act = "Estabas sosteniendo";
	else if (slot_type_is(player, slot, EQUIP_LIGHT))
		act = "Estabas sosteniendo";
	else
		act = "Te desequipaste";

	/* Des-equipar el objeto */
	player->body.slots[slot].obj = NULL;
	player->upkeep->equip_cnt--;

	player->upkeep->update |= (PU_BONUS | PU_INVEN | PU_UPDATE_VIEW);
	player->upkeep->notice |= (PN_IGNORE);
	update_stuff(player);

	/* Mensaje */
	msgt(MSG_WIELD, "%s %s (%c).", act, o_name, gear_to_label(player, obj));

	return;
}


/**
 * Suelta (parte de) un objeto de inventario/equipo no maldito "cerca" de la ubicación
 * actual
 *
 * Hay dos casos aquí: se suelta un solo objeto o una pila completa,
 * o se separa parte de una pila y se suelta
 */
void inven_drop(struct object *obj, int amt)
{
	struct object *dropped;
	bool none_left = false;
	bool equipped = false;
	bool quiver;

	char name[80];
	char label;

	/* Verificación de error */
	if (amt <= 0)
		return;

	/* Verificar que todavía se sostiene, en caso de que hubiera dos comandos de soltar encolados
	 * para este objeto. Esto en teoría no es ideal, pero en la práctica debería
	 * ser seguro. */
	if (!object_is_carried(player, obj))
		return;

	/* Obtener dónde está el objeto ahora */
	label = gear_to_label(player, obj);

	/* ¿Está en la aljaba? */
	quiver = object_is_in_quiver(player, obj);

	/* No demasiados */
	if (amt > obj->number) amt = obj->number;

	/* Quitar equipo, no combinar */
	if (object_is_equipped(player->body, obj)) {
		equipped = true;
		inven_takeoff(obj);
	}

	/* Obtener el objeto */
	dropped = gear_object_for_use(player, obj, amt, false, &none_left);

	/* Describir el objeto soltado */
	object_desc(name, sizeof(name), dropped, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Mensaje soltar objeto "Soltaste Pergamino" si el "un" */
	char *pos = strstr(name, "un "); //fix traduc
	if (pos != NULL) {
		msg("Soltaste %s (%c).", pos + 3, label);  //Fix traduc. Saltamos 3 caracteres de "un "  
	}
	else {		
		msg("Soltaste %s (%c).", name, label);  // Texto normal: Soltaste 7 Rations of ....
	}

	/* Describir lo que queda */
	if (dropped->artifact) {
		object_desc(name, sizeof(name), dropped,
			ODESC_FULL | ODESC_SINGULAR, player);
		msg("Ya no tienes %s (%c).", name, label);
	} else {
		struct object *first;
		struct object *desc_target;
		uint16_t total;

		/*
		 * Como gear_object_for_use(), no mostrar un total agregado
		 * si estaba equipado o el objeto tiene cargas/aviso de recarga
		 * que es específico de la pila.
		 */
		if (equipped || tval_can_have_charges(obj) || tval_is_rod(obj)
				|| obj->timeout > 0) {
			first = NULL;
			if (none_left) {
				total = 0;
				desc_target = dropped;
			} else {
				total = obj->number;
				desc_target = obj;
			}
		} else {
			total = object_pack_total(player, obj, false, &first);
			desc_target = (total) ? obj : dropped;
		}

		object_desc(name, sizeof(name), desc_target,
			ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
			(total << 16), player);
			
		if (!first) {
		    // Fix traduc. Truco para poner una frase más entendible
			char *pos = strstr(name, "no más");
			
			// Si encontramos "no más", ignoramos el "Tienes" y lanzamos el mensaje corregido
            // Saltamos el "no más " para obtener solo el nombre del objeto
            msg("No te quedan %s (%c).", pos + 8, label);  // es 8 ya que son la cantidad de caracteres de "no más  "          
            //msg("Tienes %s (%c).", name, label); //resp
		} else {
			label = gear_to_label(player, first);
			if (total > first->number) {
				msg("Tienes2 %s (1er %c).", name, label);
			} else {
				msg("Te quedan %s (%c).", name, label);  // fix traduc. Cuando soltaste 3 de 4 Rations of Food ahora dice "Te quedan 4 Rations..."
			}
		}
	}

	/* Soltarlo cerca del jugador */
	drop_near(cave, &dropped, 0, player->grid, false, true);

	/* Sonido para objetos de la aljaba */
	if (quiver)
		sound(MSG_QUIVER);

	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * Devuelve si cada pila de objetos se puede fusionar en dos pilas desiguales.
 */
static bool inven_can_stack_partial(struct player *p, const struct object *obj1,
	const struct object *obj2, object_stack_t mode1, object_stack_t mode2)
{
	object_stack_t cmode = mode1 | mode2;

	if (! object_stackable(obj1, obj2, cmode)) {
		return false;
	}

	/*
	 * Ahora verificar que los números son adecuados para pilas desiguales. Queremos
	 * que la pila principal, obj1, tenga su conteo maximizado.
	 */
	if (!(cmode & OSTACK_STORE)) {
		/* La aljaba puede tener límites más estrictos. */
		if (mode1 & OSTACK_QUIVER) {
			int qlimit = z_info->quiver_slot_size /
				(tval_is_ammo(obj1) ?
				1 : z_info->thrown_quiver_mult);

			/*
			 * No hay razón para combinar si ya está en el límite.
			 */
			if (obj1->number == qlimit) {
				return false;
			}

			/*
			 * Verificado los límites por pila. Si se intenta mover
			 * objetos a la aljaba, también verificar los límites generales
			 * de la aljaba para evitar combinar y luego dividir en
			 * calc_inventory().
			 */
			if (mode2 & ~OSTACK_QUIVER) {
				int n_free_slot = z_info->pack_size -
					pack_slots_used(p);
				int num_to_quiver;

				quiver_absorb_num(p, obj2, &n_free_slot,
					&num_to_quiver);
				if (num_to_quiver <= 0) {
					return false;
				}
			}
		} else if (obj1->number == obj1->kind->base->max_stack) {
			/*
			 * No hay razón para combinar si ya está en el límite.
			 */
			return false;
		}
	}

	return true;
}


/**
 * Combinar objetos en la mochila, confirmando que no hay objetos vacíos ni oro
 */
void combine_pack(struct player *p)
{
	struct object *obj1, *obj2, *prev;
	bool display_message = false;
	bool disable_repeat = false;

	/* Combinar la mochila (hacia atrás) */
	obj1 = gear_last_item(p);
	while (obj1) {
		assert(obj1->kind);
		assert(!tval_is_money(obj1));
		prev = obj1->prev;

		/* Escanear los objetos encima de ese objeto */
		for (obj2 = p->gear; obj2 && obj2 != obj1; obj2 = obj2->next) {
			object_stack_t stack_mode2 =
				object_is_in_quiver(p, obj2) ?
				OSTACK_QUIVER : OSTACK_PACK;

			assert(obj2->kind);

			/* ¿Podemos soltar "obj1" sobre "obj2"? */
			if (object_mergeable(obj2, obj1, stack_mode2)) {
				display_message = true;
				disable_repeat = true;
				object_absorb(obj2->known, obj1->known);
				obj1->known = NULL;
				object_absorb(obj2, obj1);

				/* Asegurar que los números se alinean (no debería ser necesario, pero más seguro) */
				obj2->known->number = obj2->number;

				break;
			} else {
				object_stack_t stack_mode1 =
					object_is_in_quiver(p, obj1) ?
					OSTACK_QUIVER : OSTACK_PACK;

				if (inven_can_stack_partial(p, obj2, obj1,
						stack_mode2, stack_mode1)) {
					/*
					 * No mostrar un mensaje para este
					 * caso: mover objetos entre pilas
					 * no es interesante para el jugador.
					 */
					object_absorb_partial(obj2->known,
						obj1->known, stack_mode2,
						stack_mode1);
					object_absorb_partial(obj2, obj1,
						stack_mode2, stack_mode1);
					/*
					 * Asegurar que los números se alinean (no debería ser
					 * necesario, pero más seguro)
					 */
					obj2->known->number = obj2->number;
					obj1->known->number = obj1->number;

					break;
				}
			}
		}
		obj1 = prev;
	}

	calc_inventory(p);

	/* Redibujar equipo */
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);

	/* Mensaje */
	if (display_message) {
		msg("Combinaste algunos objetos en tu mochila.");

		/*
		 * Detener que "repetir último comando" funcione si una pila se
		 * combinó completamente con otra.
		 */
		if (disable_repeat) cmd_disable_repeat();
	}
}

/**
 * Devuelve si la mochila tiene el número máximo de objetos.
 */
bool pack_is_full(void)
{
	return pack_slots_used(player) == z_info->pack_size;
}

/**
 * Devuelve si la mochila tiene más que el número máximo de objetos.
 * Si esto es verdadero, llamar a pack_overflow() provocará un desbordamiento de la mochila.
 */
bool pack_is_overfull(void)
{
	return pack_slots_used(player) > z_info->pack_size;
}

/**
 * Desborda un objeto de la mochila, si está sobrecargada.
 */
void pack_overflow(struct object *obj)
{
	int i;
	char o_name[80];

	if (!pack_is_overfull()) return;

	/* Molestar */
	disturb(player);

	/* Advertencia */
	msg("¡Tu mochila se desborda!");

	/* Obtener el último objeto propio */
	for (i = 1; i <= z_info->pack_size; i++)
		if (!player->upkeep->inven[i])
			break;

	/* Soltar el último objeto del inventario a menos que se solicite lo contrario */
	if (!obj) {
		obj = player->upkeep->inven[i - 1];
	}

	/* Descartar rarezas (como mochila llena, pero inventario vacío) */
	assert(obj != NULL);

	/* Describir */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);

	/* Mensaje */
	msg("Soltaste %s.", o_name);

	/* Extraer el objeto y soltarlo (con cuidado) cerca del jugador */
	gear_excise_object(player, obj);
	drop_near(cave, &obj, 0, player->grid, false, true);

	/* Describir */
	msg("Ya no tienes %s.", o_name);

	/* Notificar, actualizar, redibujar */
	if (player->upkeep->notice) notice_stuff(player);
	if (player->upkeep->update) update_stuff(player);
	if (player->upkeep->redraw) redraw_stuff(player);
}

/**
 * Mirar la inscripción de un objeto para determinar dónde quiere colocarse en
 * la aljaba. Si el objeto no es apropiado para la aljaba o no está
 * inscrito apropiadamente, devolver -1.
 */
int preferred_quiver_slot(const struct object *obj)
{
	int desired_slot = -1;

	if (obj->note && (tval_is_ammo(obj) ||
			of_has(obj->flags, OF_THROWING))) {
		const char *s = strchr(quark_str(obj->note), '@');
		char fire_key, throw_key;

		/*
		 * Sería bueno usar cmd_lookup_key() para esto, pero eso es
		 * parte de la capa de interfaz de usuario (declarado en ui-game.h). En su lugar,
		 * establecer directamente las teclas para los comandos de disparar y lanzar.
		 */
		if (OPT(player, rogue_like_commands)) {
			fire_key = 't';
		} else {
			fire_key = 'f';
		}
		throw_key = 'v';
		while (1) {
			if (!s) break;
			if (s[1] == fire_key || s[1] == throw_key) {
				desired_slot = s[2] - '0';
				break;
			}
			s = strchr(s + 1, '@');
		}
	}

	return desired_slot;
}