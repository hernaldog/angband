/**
 * \file obj-list.c
 * \brief Construcción de listas de objetos.
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Ben Semmler
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
#include "game-world.h"
#include "obj-desc.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-list.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "project.h"

/**
 * Asignar una nueva lista de objetos.
 */
object_list_t *object_list_new(void)
{
	object_list_t *list = mem_zalloc(sizeof(object_list_t));
	size_t size = MAX_ITEMLIST;

	if (list == NULL)
		return NULL;

	list->entries = mem_zalloc(size * sizeof(object_list_entry_t));

	if (list->entries == NULL) {
		mem_free(list);
		return NULL;
	}

	list->entries_size = size;

	return list;
}

/**
 * Liberar una lista de objetos.
 */
void object_list_free(object_list_t *list)
{
	if (list == NULL)
		return;

	if (list->entries != NULL) {
		mem_free(list->entries);
		list->entries = NULL;
	}

	mem_free(list);
}

/**
 * Instancia compartida de la lista de objetos.
 */
static object_list_t *object_list_subwindow = NULL;

/**
 * Inicializar el módulo de listas de objetos.
 */
void object_list_init(void)
{
	object_list_subwindow = NULL;
}

/**
 * Finalizar el módulo de listas de objetos.
 */
void object_list_finalize(void)
{
	object_list_free(object_list_subwindow);
}

/**
 * Devuelve una instancia común de la lista de objetos.
 */
object_list_t *object_list_shared_instance(void)
{
	if (object_list_subwindow == NULL) {
		object_list_subwindow = object_list_new();
	}

	return object_list_subwindow;
}

/**
 * Devuelve verdadero si la lista necesita ser actualizada. Normalmente esto es cada turno.
 */
static bool object_list_needs_update(const object_list_t *list)
{
	if (list == NULL || list->entries == NULL)
		return false;

	/* Por ahora, siempre actualizar cuando se solicite. */
	return true;
}

/**
 * Vaciar el contenido de una lista de objetos.
 */
void object_list_reset(object_list_t *list)
{
	if (list == NULL || list->entries == NULL)
		return;

	if (!object_list_needs_update(list))
		return;

	memset(list->entries, 0, list->entries_size * sizeof(object_list_entry_t));
	memset(list->total_entries, 0, OBJECT_LIST_SECTION_MAX * sizeof(uint16_t));
	memset(list->total_objects, 0, OBJECT_LIST_SECTION_MAX * sizeof(uint16_t));
	list->distinct_entries = 0;
	list->creation_turn = 0;
	list->sorted = false;
}

/**
 * Devuelve verdadero si el objeto debe omitirse de la lista de objetos.
 */
static bool object_list_should_ignore_object(const struct player *p,
		const struct object *obj)
{
	struct object *base_obj = cave->objects[obj->oidx];

	assert(obj->kind);
	assert(base_obj);

	if (!is_unknown(base_obj) && ignore_known_item_ok(p, obj))
		return true;

	if (tval_is_money(base_obj))
		return true;

	return false;
}

/**
 * Recopilar información de objetos de la cueva actual.
 */
void object_list_collect(object_list_t *list)
{
	int i;
	struct loc pgrid = player->grid;

	if (list == NULL || list->entries == NULL)
		return;

	if (!object_list_needs_update(list))
		return;

	/* Escanear cada objeto en la mazmorra. */
	for (i = 1; i < player->cave->obj_max; i++) {
		object_list_entry_t *entry = NULL;
		int entry_index;
		int current_distance;
		int entry_distance;
		struct loc grid;
		int field;
		bool los = false;
		struct object *obj = player->cave->objects[i];

		/* Saltar entradas vacías, objetos desconocidos y objetos sostenidos por monstruos */
		if (!obj) continue;
		if (loc_is_zero(obj->grid)) {
			continue;
		} else {
			grid = obj->grid;
		}

		/* Determinar en qué sección de la lista debe estar la entrada del objeto */
		los = projectable(cave, pgrid, grid, PROJECT_NONE) ||
			loc_eq(grid, pgrid);
		field = (los) ? OBJECT_LIST_SECTION_LOS : OBJECT_LIST_SECTION_NO_LOS;

		if (object_list_should_ignore_object(player, obj)) continue;

		/* Encontrar o añadir una entrada en la lista. */
		for (entry_index = 0; entry_index < (int)list->entries_size;
			 entry_index++) {
			int j;
			struct object *list_obj = list->entries[entry_index].object;

			if (list_obj == NULL) {
				/* Encontramos un espacio vacío, así que añadimos este objeto aquí. */
				list->entries[entry_index].object = obj;
				for (j = 0; j < OBJECT_LIST_SECTION_MAX; j++)
					list->entries[entry_index].count[j] = 0;
				list->entries[entry_index].dy = grid.y - pgrid.y;
				list->entries[entry_index].dx = grid.x - pgrid.x;
				entry = &list->entries[entry_index];
				break;
			}
		}

		if (entry == NULL)
			return;

		/* Solo sabemos el número de objetos que realmente hemos visto */
		if (obj->kind == cave->objects[obj->oidx]->kind)
			entry->count[field] += obj->number;
		else
			entry->count[field] = 1;

		/* Almacenar la distancia al objeto en la pila que está
		 * más cerca del jugador. */
		current_distance = (grid.y - pgrid.y) * (grid.y - pgrid.y) +
			(grid.x - pgrid.x) * (grid.x - pgrid.x);
		entry_distance = entry->dy * entry->dy + entry->dx * entry->dx;

		if (current_distance < entry_distance) {
			entry->dy = grid.y - pgrid.y;
			entry->dx = grid.x - pgrid.x;
		}
	}

	/* Recopilar totales para cálculos más fáciles de la lista. */
	for (i = 0; i < (int)list->entries_size; i++) {
		if (list->entries[i].object == NULL)
			continue;

		if (list->entries[i].count[OBJECT_LIST_SECTION_LOS] > 0)
			list->total_entries[OBJECT_LIST_SECTION_LOS]++;

		if (list->entries[i].count[OBJECT_LIST_SECTION_NO_LOS] > 0)
			list->total_entries[OBJECT_LIST_SECTION_NO_LOS]++;

		list->total_objects[OBJECT_LIST_SECTION_LOS] +=
			list->entries[i].count[OBJECT_LIST_SECTION_LOS];
		list->total_objects[OBJECT_LIST_SECTION_NO_LOS] +=
			list->entries[i].count[OBJECT_LIST_SECTION_NO_LOS];
		list->distinct_entries++;
	}

	list->creation_turn = turn;
	list->sorted = false;
}

/**
 * Comparador de distancia de objetos: más cercano a más lejano.
 */
static int object_list_distance_compare(const void *a, const void *b)
{
	const object_list_entry_t *ae = (object_list_entry_t *)a;
	const object_list_entry_t *be = (object_list_entry_t *)b;
	int a_distance = ae->dy * ae->dy + ae->dx * ae->dx;
	int b_distance = be->dy * be->dy + be->dx * be->dx;

	if (a_distance < b_distance)
		return -1;
	else if (a_distance > b_distance)
		return 1;

	return 0;
}

/**
 * Función de comparación estándar para la lista de objetos. Usa compare_items().
 */
int object_list_standard_compare(const void *a, const void *b)
{
	int result;
	const struct object *ao = cave->objects[(((object_list_entry_t *)a)->object)->oidx];
	const struct object *bo = cave->objects[(((object_list_entry_t *)b)->object)->oidx];

	/* Si esto sucede, algo podría estar mal en la función de recopilación. */
	if (ao == NULL || bo == NULL)
		return 1;

	result = compare_items(ao, bo);

	/* Si los objetos son equivalentes, ordenar del más cercano al más lejano. */
	if (result == 0)
		result = object_list_distance_compare(a, b);

	return result;
}

/**
 * Ordenar la lista de objetos con la función de ordenación dada.
 */
void object_list_sort(object_list_t *list,
					  int (*compare)(const void *, const void *))
{
	size_t elements;

	if (list == NULL || list->entries == NULL)
		return;

	if (list->sorted)
		return;

	elements = list->distinct_entries;

	if (elements <= 1)
		return;

	sort(list->entries, elements, sizeof(list->entries[0]), compare);
	list->sorted = true;
}

/**
 * Devuelve un atributo con el que mostrar una entrada de lista particular.
 *
 * \param entry es la entrada de la lista de objetos a mostrar.
 * \return un atributo de terminal para la entrada del objeto.
 */
uint8_t object_list_entry_line_attribute(const object_list_entry_t *entry)
{
	uint8_t attr;
	struct object *base_obj;

	if (entry == NULL || entry->object == NULL || entry->object->kind == NULL)
		return COLOUR_WHITE;

	base_obj = cave->objects[entry->object->oidx];

	if (is_unknown(base_obj))
		/* objeto desconocido */
		attr = COLOUR_RED;
	else if (base_obj->known->artifact)
		/* artefacto conocido */
		attr = COLOUR_VIOLET;
	else if (!object_flavor_is_aware(base_obj))
		/* no se conoce el tipo */
		attr = COLOUR_L_RED;
	else if (base_obj->kind->cost == 0)
		/* sin valor */
		attr = COLOUR_SLATE;
	else
		/* predeterminado */
		attr = COLOUR_WHITE;

	return attr;
}

/**
 * Formatear el nombre del objeto para que el prefijo esté alineado a la derecha en una
 * columna común.
 *
 * Esto usa la lógica predeterminada de object_desc() para manejar sabores,
 * artefactos, vocales, etc. Fue más fácil hacer esto y luego usar strtok()
 * para dividirlo que hacer cualquier otra cosa.
 *
 * \param entry es la entrada de la lista de objetos que tiene un nombre a formatear.
 * \param line_buffer es el búfer donde formatear.
 * \param size es el tamaño de line_buffer.
 * Fix traduc nuevo parámetro "out_count" devuelve el número aparte
 */
void object_list_format_name(const object_list_entry_t *entry,
                             char *line_buffer, size_t size, int *out_count)
{
    char name[80]; //fix traduc varios cambios
	bool los = false;
	int field;
	struct loc pgrid = player->grid;
	struct object *base_obj;
	struct loc grid;

	if (entry == NULL || entry->object == NULL || entry->object->kind == NULL)
		return;

	base_obj = cave->objects[entry->object->oidx];
	grid = entry->object->grid;

    /* Determinar si el objeto está a la vista */
	los = projectable(cave, pgrid, grid, PROJECT_NONE) || loc_eq(grid, pgrid);
	field = los ? OBJECT_LIST_SECTION_LOS : OBJECT_LIST_SECTION_NO_LOS;

    /* Exportar el conteo para que el caller lo imprima como columna */
    if (out_count != NULL)
        *out_count = entry->count[field];

    /* Obtener el nombre sin prefijo numérico */
    object_desc(name, sizeof(name), base_obj,
        ODESC_FULL | ODESC_ALTNUM | (entry->count[field] << 16), player);

    my_strcpy(line_buffer, name, size);
}