/**
 * \file ui-obj-list.c
 * \brief Interfaz de usuario de la lista de objetos.
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
#include "init.h"
#include "obj-list.h"
#include "obj-util.h"
#include "ui-object.h"
#include "ui-obj-list.h"
#include "ui-output.h"
#include "z-textblock.h"

/**
 * Formatear una sección de la lista de objetos: un encabezado seguido de las
 * filas de entrada de la lista de objetos.
 *
 * Esta función procesará cada entrada para la sección dada. Mostrará:
 * - el carácter del objeto;
 * - número de objetos;
 * - nombre del objeto (truncado, si es necesario para que quepa en la línea);
 * - distancia del objeto al jugador (alineada al lado derecho de la lista).
 * Pasando un textblock NULL, se puede encontrar el ancho máximo de línea de la
 * sección.
 *
 * \param list es la lista de objetos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param section es la sección de la lista a formatear.
 * \param lines_to_display son el número de entradas a mostrar (sin incluir el encabezado).
 * \param max_width es el ancho máximo de línea.
 * \param prefix es el comienzo del encabezado; el resto se añade con el número de objetos.
 * \param show_others si es true, ajustará el cálculo de la longitud máxima de línea
 * para la línea que menciona el número de objetos no mostrados en la lista.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 */
static void object_list_format_section(const object_list_t *list,
									   textblock *tb,
									   object_list_section_t section,
									   int lines_to_display, int max_width,
									   const char *prefix, bool show_others,
									   size_t *max_width_result)
{
	int remaining_object_total = 0;
	int line_count = 0;
	int entry_index;
	int total;
	char line_buffer[200];
	const char *punctuation = (lines_to_display == 0) ? "." : ":";
	const char *others = (show_others) ? "otros " : "";
	size_t max_line_length = 0;

	if (list == NULL || list->entries == NULL)
		return;

	total = list->distinct_entries;

	if (list->total_entries[section] == 0) {
		max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
								  "%s ningún objeto.\n", prefix);

		if (tb != NULL)
			textblock_append(tb, "%s", line_buffer);

		/* Forzar un ancho mínimo para que el mensaje no se corte. */
		if (max_width_result != NULL)
			*max_width_result = MAX(max_line_length, 40);

		return;
	}

	max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
							  "%s %d %sobjeto%s%s\n", prefix,
							  list->total_entries[section], others,
							  PLURAL(list->total_entries[section]),
							  punctuation);

	if (tb != NULL)
		textblock_append(tb, "%s", line_buffer);

	for (entry_index = 0; entry_index < total && line_count < lines_to_display;
		 entry_index++) {
		char location[20] = { '\0' };
		uint8_t line_attr;
		size_t full_width;
		const char *direction_y = (list->entries[entry_index].dy <= 0) ? "N" : "S";
		const char *direction_x = (list->entries[entry_index].dx <= 0) ? "O" : "E";

		line_buffer[0] = '\0';

		if (list->entries[entry_index].count[section] == 0)
			continue;

		/* Construir la cadena de ubicación. */
		strnfmt(location, sizeof(location), " %d %s %d %s",
				abs(list->entries[entry_index].dy), direction_y,
				abs(list->entries[entry_index].dx), direction_x);

		/* Obtener ancho disponible para el nombre del objeto: 2 para carácter y espacio; location
		 * incluye relleno; último -1 por alguna razón. */
		full_width = max_width - 2 - utf8_strlen(location) - 1;

		/* Añadir el recuento de objetos y recortar el nombre del objeto para que quepa. */
		object_list_format_name(&list->entries[entry_index], line_buffer,
								sizeof(line_buffer));
		utf8_clipto(line_buffer, full_width);

		/* Calcular el ancho de la línea para el tamaño dinámico; usar un ancho máximo fijo
		 * para la ubicación y el carácter del objeto. */
		max_line_length = MAX(max_line_length,
							  utf8_strlen(line_buffer) + 12 + 2);

		/* textblock_append_pict agregará de forma segura el símbolo del objeto, independientemente
		 * del modo ASCII/gráficos. */
		if (tb != NULL && tile_width == 1 && tile_height == 1) {
			uint8_t a = COLOUR_RED;
			wchar_t c = L'*';

			if (!is_unknown(list->entries[entry_index].object) &&
				list->entries[entry_index].object->kind != NULL) {
				a = object_kind_attr(list->entries[entry_index].object->kind);
				c = object_kind_char(list->entries[entry_index].object->kind);
			}

			textblock_append_pict(tb, a, c);
			textblock_append(tb, " ");
		}

		/* Añadir el nombre del objeto alineado a la izquierda y con relleno, lo que alineará la
		 * ubicación a la derecha. */
		if (tb != NULL) {
			/*
			 * Hack - Debido a que las cadenas de nombre de objeto son UTF8, tenemos que añadir
			 * relleno adicional para los bytes brutos que podrían consolidarse en un carácter mostrado.
			 */
			full_width += strlen(line_buffer) - utf8_strlen(line_buffer);
			line_attr = object_list_entry_line_attribute(&list->entries[entry_index]);
			textblock_append_c(tb, line_attr, "%-*s%s\n",
				(int) full_width, line_buffer, location);
		}

		line_count++;
	}

	/* No preocuparse por la línea "...y otros", ya que probablemente es más corta que lo que ya se ha impreso. */
	if (max_width_result != NULL)
		*max_width_result = max_line_length;

	/* Salir ya que no tenemos suficiente espacio para mostrar el recuento restante o
	 * ya los hemos mostrado todos. */
	if (lines_to_display <= 0 ||
		lines_to_display >= list->total_entries[section])
		return;

	/* Contar los objetos restantes, comenzando donde lo dejamos en el bucle anterior. */
	remaining_object_total = total - entry_index;

	if (tb != NULL)
		textblock_append(tb, "%6s...y %d otros.\n", " ", remaining_object_total);
}

/**
 * Permitir que el formato de lista estándar sea omitido para casos especiales.
 *
 * Devolver true omitirá cualquier otro formato en object_list_format_textblock().
 *
 * \param list es la lista de objetos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param max_lines es el número máximo de líneas que se pueden mostrar.
 * \param max_width es el ancho máximo de línea que se puede mostrar.
 * \param max_height_result devuelve el número de líneas necesarias para formatear la lista sin truncamiento.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 * \return true si se debe omitir el formateo adicional.
 */
static bool object_list_format_special(const object_list_t *list, textblock *tb,
									   int max_lines, int max_width,
									   size_t *max_height_result,
									   size_t *max_width_result)
{
	return false;
}

/**
 * Formatear toda la lista de objetos con los parámetros dados.
 *
 * Esta función se puede utilizar para calcular las dimensiones preferidas para la lista
 * pasando un textblock NULL. Esta función llama a object_list_format_special() primero; si
 * esa función devuelve true, omitirá el formato de lista normal.
 *
 * \param list es la lista de objetos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param max_lines es el número máximo de líneas que se pueden mostrar.
 * \param max_width es el ancho máximo de línea que se puede mostrar.
 * \param max_height_result devuelve el número de líneas necesarias para formatear la lista sin truncamiento.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 */
static void object_list_format_textblock(const object_list_t *list,
										 textblock *tb, int max_lines,
										 int max_width,
										 size_t *max_height_result,
										 size_t *max_width_result)
{
	int header_lines = 1;
	int lines_remaining;
	int los_lines_to_display;
	int no_los_lines_to_display;
	size_t max_los_line = 0;
	size_t max_no_los_line = 0;

	if (list == NULL || list->entries == NULL)
		return;

	if (object_list_format_special(list, tb, max_lines, max_width,
								   max_height_result, max_width_result))
		return;

	los_lines_to_display = list->total_entries[OBJECT_LIST_SECTION_LOS];
	no_los_lines_to_display = list->total_entries[OBJECT_LIST_SECTION_NO_LOS];
                                                          
	if (list->total_entries[OBJECT_LIST_SECTION_NO_LOS] > 0)
		header_lines += 2;

 	if (max_height_result != NULL)
		*max_height_result = header_lines + los_lines_to_display + 
			no_los_lines_to_display;

	lines_remaining = max_lines - header_lines - 
		list->total_entries[OBJECT_LIST_SECTION_LOS];

	/* Eliminar líneas NO_LOS según sea necesario */
	if (lines_remaining < list->total_entries[OBJECT_LIST_SECTION_NO_LOS])
		no_los_lines_to_display = MAX(lines_remaining - 1, 0);

	/* Si ni siquiera tenemos suficiente espacio para el encabezado NO_LOS, comenzar a eliminar
	 * líneas LOS, dejando una para los "...otros". */
	if (lines_remaining < 0)
		los_lines_to_display = list->total_entries[OBJECT_LIST_SECTION_LOS] -
                        abs(lines_remaining) - 1;

	/* Mostrar solo encabezados si no tenemos suficiente espacio. */
	if (header_lines >= max_lines) {
		los_lines_to_display = 0;
		no_los_lines_to_display = 0;
	}
        
	object_list_format_section(list, tb, OBJECT_LIST_SECTION_LOS,
							   los_lines_to_display, max_width,
							   "Puedes ver", false, &max_los_line);

	if (list->total_entries[OBJECT_LIST_SECTION_NO_LOS] > 0) {
         bool show_others = list->total_objects[OBJECT_LIST_SECTION_LOS] > 0;

         if (tb != NULL)
			 textblock_append(tb, "\n");

         object_list_format_section(list, tb, OBJECT_LIST_SECTION_NO_LOS,
									no_los_lines_to_display, max_width,
									"Eres consciente de", show_others,
									&max_no_los_line);
	}

                                                                        
	if (max_width_result != NULL)
		*max_width_result = MAX(max_los_line, max_no_los_line);
}

/**
 * Mostrar la lista de objetos estáticamente. Esto forzará que la lista se muestre
 * con las dimensiones proporcionadas. El contenido se ajustará en consecuencia.
 *
 * Para ser más eficiente, esta función utiliza un objeto de lista compartido para
 * no estar constantemente asignando y liberando la lista.
 *
 * \param height es la altura de la lista.
 * \param width es el ancho de la lista.
 */
void object_list_show_subwindow(int height, int width)
{
	textblock *tb;
	object_list_t *list;

	if (height < 1 || width < 1)
		return;

	tb = textblock_new();
	list = object_list_shared_instance();

	object_list_reset(list);
	object_list_collect(list);
	object_list_sort(list, object_list_standard_compare);

	/* Dibujar la lista para que quepa exactamente en la subventana. */
	object_list_format_textblock(list, tb, height, width, NULL, NULL);
	textui_textblock_place(tb, SCREEN_REGION, NULL);

	textblock_free(tb);
}

/**
 * Mostrar la lista de objetos de forma interactiva. Esto dimensionará dinámicamente la lista
 * para obtener la mejor apariencia. Esto solo debe usarse en el terminal principal.
 *
 * \param height es el límite de altura para la lista.
 * \param width es el límite de ancho para la lista.
 */
void object_list_show_interactive(int height, int width)
{
	textblock *tb;
	object_list_t *list;
	size_t max_width = 0, max_height = 0;
	int safe_height, safe_width;
	region r;

	if (height < 1 || width < 1)
		return;

	tb = textblock_new();
	list = object_list_new();

	object_list_collect(list);
	object_list_sort(list, object_list_standard_compare);

	/*
	 * Calcular el rectángulo de visualización óptimo. Se pasan números grandes como límite de altura
	 * y ancho para que podamos calcular el número máximo de filas y columnas para mostrar la lista
	 * adecuadamente. Luego ajustamos esos valores según sea necesario para que quepan en el terminal principal.
	 * La altura se ajusta para tener en cuenta el mensaje del textblock. La lista se posiciona en el lado derecho
	 * del terminal debajo de la línea de estado.
	 */
	object_list_format_textblock(list, NULL, 1000, 1000, &max_height,
								 &max_width);
	safe_height = MIN(height - 2, (int)max_height + 2);
	safe_width = MIN(width - 13, (int)max_width);
	r.col = -safe_width;
	r.row = 1;
	r.width = safe_width;
	r.page_rows = safe_height;

	/*
	 * Dibujar la lista realmente. Pasamos max_height a la función de formato para que
	 * todas las líneas se añadan al textblock. El textblock en sí mismo se encargará de
	 * encajarlo en la región. Sin embargo, tenemos que pasar safe_width para que la función de
	 * formato pueda rellenar las líneas correctamente de modo que la cadena de ubicación
	 * esté alineada al borde derecho de la lista.
	 */
	object_list_format_textblock(list, tb, (int)max_height, safe_width, NULL,
								 NULL);
	region_erase_bordered(&r);
	textui_textblock_show(tb, r, NULL);

	textblock_free(tb);
	object_list_free(list);
}