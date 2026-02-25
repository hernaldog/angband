/**
 * \file ui-mon-list.c
 * \brief Interfaz de usuario de la lista de monstruos.
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

#include "mon-desc.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-util.h"
#include "player-timed.h"
#include "ui-mon-list.h"
#include "ui-output.h"
#include "ui-prefs.h"
#include "ui-term.h"

/**
 * Formatear una sección de la lista de monstruos: un encabezado seguido de las
 * filas de entrada de la lista de monstruos.
 *
 * Esta función procesará cada entrada para la sección dada. Mostrará:
 * - el carácter del monstruo;
 * - número de monstruos;
 * - nombre del monstruo (truncado, si es necesario para que quepa en la línea);
 * - si el monstruo está dormido o no (y cuántos si están en un grupo);
 * - distancia del monstruo al jugador (alineada al lado derecho de la lista).
 * Pasando un textblock NULL, se puede encontrar el ancho máximo de línea de la
 * sección.
 *
 * \param list es la lista de monstruos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param section es la sección de la lista de monstruos a formatear.
 * \param lines_to_display son el número de entradas a mostrar (sin incluir el encabezado).
 * \param max_width es el ancho máximo de línea.
 * \param prefix es el comienzo del encabezado; el resto se añade con el número de monstruos.
 * \param show_others se usa para añadir "otros monstruos" al encabezado, después del número de monstruos.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 */
static void monster_list_format_section(const monster_list_t *list, textblock *tb, monster_list_section_t section, int lines_to_display, int max_width, const char *prefix, bool show_others, size_t *max_width_result)
{
	int remaining_monster_total = 0;
	int line_count = 0;
	int index;
	int total;
	char line_buffer[200];
	const char *punctuation = (lines_to_display == 0) ? "." : ":";
	const char *others = (show_others) ? "otros " : "";
	size_t max_line_length = 0;

	if (list == NULL || list->entries == NULL)
		return;

	total = list->distinct_entries;

	if (list->total_monsters[section] == 0) {
		max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
								  "%s ningún monstruo.\n", prefix);

		if (tb != NULL)
			textblock_append(tb, "%s", line_buffer);

		/* Forzar un ancho mínimo para que el mensaje no se corte. */
		if (max_width_result != NULL)
			*max_width_result = MAX(max_line_length, 40);

		return;
	}

	max_line_length = strnfmt(line_buffer, sizeof(line_buffer),
							  "%s %d %smonstruo%s%s\n",
							  prefix,
							  list->total_monsters[section],
							  others,
							  PLURAL(list->total_monsters[section]),
							  punctuation);

	if (tb != NULL)
		textblock_append(tb, "%s", line_buffer);

	for (index = 0; index < total && line_count < lines_to_display; index++) {
		char asleep[20] = { '\0' };
		char location[20] = { '\0' };
		uint8_t line_attr;
		size_t full_width;
		size_t name_width;
		uint16_t count_in_section = 0;
		uint16_t asleep_in_section = 0;

		line_buffer[0] = '\0';

		if (list->entries[index].count[section] == 0)
			continue;

		/* Solo mostrar direcciones para el caso de un solo monstruo. */
		if (list->entries[index].count[section] == 1) {
			const char *direction1 =
				(list->entries[index].dy[section] <= 0)	? "N" : "S";
			const char *direction2 =
				(list->entries[index].dx[section] <= 0) ? "O" : "E";
			strnfmt(location, sizeof(location), " %d %s %d %s",
					abs(list->entries[index].dy[section]), direction1,
					abs(list->entries[index].dx[section]), direction2);
		}

		/* Obtener ancho disponible para el nombre del monstruo y la etiqueta de sueño: 2 para el carácter y el espacio; location incluye relleno; último -1 por alguna razón. */
		full_width = max_width - 2 - utf8_strlen(location) - 1;

		asleep_in_section = list->entries[index].asleep[section];
		count_in_section = list->entries[index].count[section];

		if (asleep_in_section > 0 && count_in_section > 1)
			strnfmt(asleep, sizeof(asleep), " (%d dormidos)", asleep_in_section);
		else if (asleep_in_section == 1 && count_in_section == 1)
			strnfmt(asleep, sizeof(asleep), " (dormido)");

		/* Recortar el nombre del monstruo para que quepa, y añadir la etiqueta de sueño. */
		name_width = MIN(full_width - utf8_strlen(asleep), sizeof(line_buffer));
		get_mon_name(line_buffer, sizeof(line_buffer),
					 list->entries[index].race,
					 list->entries[index].count[section]);
		utf8_clipto(line_buffer, name_width);
		my_strcat(line_buffer, asleep, sizeof(line_buffer));

		/* Calcular el ancho de la línea para el tamaño dinámico; usar un ancho máximo fijo para la ubicación y el carácter del monstruo. */
		max_line_length = MAX(max_line_length,
							  utf8_strlen(line_buffer) + 12 + 2);

		/* textblock_append_pict agregará de forma segura el símbolo del monstruo, independientemente del modo ASCII/gráficos. */
		if (tb != NULL && tile_width == 1 && tile_height == 1) {
			textblock_append_pict(tb, list->entries[index].attr, monster_x_char[list->entries[index].race->ridx]);
			textblock_append(tb, " ");
		}

		/* Añadir el nombre del monstruo alineado a la izquierda y con relleno, lo que alineará la ubicación a la derecha. */
		if (tb != NULL) {
			/* Hack - Debido a que las cadenas de raza de monstruo son UTF8, tenemos que añadir
			 * relleno adicional para los bytes brutos que podrían consolidarse en un carácter mostrado. */
			full_width += strlen(line_buffer) - utf8_strlen(line_buffer);
			line_attr = monster_list_entry_line_color(&list->entries[index]);
			textblock_append_c(tb, line_attr, "%-*s%s\n",
				(int) full_width, line_buffer, location);
		}

		line_count++;
	}

	/* No preocuparse por la línea "...y otros", ya que probablemente es más corta que lo que ya se ha impreso. */
	if (max_width_result != NULL)
		*max_width_result = max_line_length;

	/* Salir ya que no tenemos suficiente espacio para mostrar el recuento restante o ya los hemos mostrado todos. */
	if (lines_to_display <= 0 ||
		lines_to_display >= list->total_entries[section])
		return;

	/* Sumar los monstruos restantes; comenzar donde lo dejamos en el bucle anterior. */
	while (index < total) {
		remaining_monster_total += list->entries[index].count[section];
		index++;
	}

	if (tb != NULL)
		textblock_append(tb, "%6s...y %d otros.\n", " ",
						 remaining_monster_total);
}

/**
 * Permitir que el formato de lista estándar sea omitido para casos especiales.
 *
 * Devolver true omitirá cualquier otro formato en monster_list_format_textblock().
 *
 * \param list es la lista de monstruos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param max_lines es el número máximo de líneas que se pueden mostrar.
 * \param max_width es el ancho máximo de línea que se puede mostrar.
 * \param max_height_result devuelve el número de líneas necesarias para formatear la lista sin truncamiento.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 * \return true si se debe omitir el formateo adicional.
 */
static bool monster_list_format_special(const monster_list_t *list, textblock *tb, int max_lines, int max_width, size_t *max_height_result, size_t *max_width_result)
{
	if (player->timed[TMD_IMAGE] > 0) {
		/* Hack - el mensaje necesita una nueva línea para calcular el ancho correctamente. */
		const char *message = "Tus alucinaciones son demasiado salvajes para ver las cosas con claridad.\n";

		if (max_height_result != NULL)
			*max_height_result = 1;

		if (max_width_result != NULL)
			*max_width_result = strlen(message);

		if (tb != NULL)
			textblock_append_c(tb, COLOUR_ORANGE, "%s", message);

		return true;
	}

	return false;
}

/**
 * Formatear toda la lista de monstruos con los parámetros dados.
 *
 * Esta función se puede utilizar para calcular las dimensiones preferidas para la lista
 * pasando un textblock NULL. La sección LOS de la lista siempre se mostrará,
 * mientras que la otra sección se añadirá condicionalmente. Además, esta
 * función llama a monster_list_format_special() primero; si esa función devuelve
 * true, omitirá el formato de lista normal.
 *
 * \param list es la lista de monstruos a formatear.
 * \param tb es el textblock a producir o NULL si solo se necesitan calcular las dimensiones.
 * \param max_lines es el número máximo de líneas que se pueden mostrar.
 * \param max_width es el ancho máximo de línea que se puede mostrar.
 * \param max_height_result devuelve el número de líneas necesarias para formatear la lista sin truncamiento.
 * \param max_width_result devuelve el ancho necesario para formatear la lista sin truncamiento.
 */
static void monster_list_format_textblock(const monster_list_t *list, textblock *tb, int max_lines, int max_width, size_t *max_height_result, size_t *max_width_result)
{
	int header_lines = 1;
	int lines_remaining;
	int los_lines_to_display;
	int esp_lines_to_display;
	size_t max_los_line = 0;
	size_t max_esp_line = 0;

	if (list == NULL || list->entries == NULL)
		return;

	if (monster_list_format_special(list, tb, max_lines, max_width,
									max_height_result, max_width_result))
		return;

	los_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_LOS];
	esp_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_ESP];

	if (list->total_entries[MONSTER_LIST_SECTION_ESP] > 0)
		header_lines += 2;

	if (max_height_result != NULL)
		*max_height_result = header_lines + los_lines_to_display +
			esp_lines_to_display;

	lines_remaining = max_lines - header_lines -
		list->total_entries[MONSTER_LIST_SECTION_LOS];

	/* Eliminar líneas de ESP según sea necesario. */
	if (lines_remaining < list->total_entries[MONSTER_LIST_SECTION_ESP])
		esp_lines_to_display = MAX(lines_remaining - 1, 0);

	/* Si ni siquiera tenemos suficiente espacio para el encabezado de ESP, comenzar a eliminar
	 * líneas de LOS, dejando una para los "...otros". */
	if (lines_remaining < 0)
		los_lines_to_display = list->total_entries[MONSTER_LIST_SECTION_LOS] -
			abs(lines_remaining) - 1;

	/* Mostrar solo encabezados si no tenemos suficiente espacio. */
	if (header_lines >= max_lines) {
		los_lines_to_display = 0;
		esp_lines_to_display = 0;
	}

	monster_list_format_section(list, tb, MONSTER_LIST_SECTION_LOS,
								los_lines_to_display, max_width,
								"Puedes ver", false, &max_los_line);

	if (list->total_entries[MONSTER_LIST_SECTION_ESP] > 0) {
		bool show_others = list->total_monsters[MONSTER_LIST_SECTION_LOS] > 0;

		if (tb != NULL)
			textblock_append(tb, "\n");

		monster_list_format_section(list, tb, MONSTER_LIST_SECTION_ESP,
									esp_lines_to_display, max_width,
									"Eres consciente de", show_others,
									&max_esp_line);
	}

	if (max_width_result != NULL)
		*max_width_result = MAX(max_los_line, max_esp_line);
}

/**
 * Obtener los glifos correctos de los monstruos.
 */
static void monster_list_get_glyphs(monster_list_t *list)
{
	int i;

	/* Recorrer todos los monstruos en la lista. */
	for (i = 0; i < (int)list->entries_size; i++) {
		monster_list_entry_t *entry = &list->entries[i];
		if (entry->race == NULL)
			continue;

		/* Si no hay atributo de monstruo, usar la imagen estándar de la interfaz. */
		if (!entry->attr)
			entry->attr = monster_x_attr[entry->race->ridx];
		/* Si purple_uniques es relevante, aplicarlo. */
		if (!(entry->attr & 0x80) && OPT(player, purple_uniques)
				&& rf_has(entry->race->flags, RF_UNIQUE)) {
			entry->attr = COLOUR_VIOLET;
		}
	}
}

/**
 * Mostrar la lista de monstruos estáticamente. Esto forzará que la lista se
 * muestre con las dimensiones proporcionadas. El contenido se ajustará en consecuencia.
 *
 * Para admitir animaciones de parpadeo de monstruos más eficientes, esta función
 * utiliza un objeto de lista compartido para no estar constantemente asignando y liberando
 * la lista.
 *
 * \param height es la altura de la lista.
 * \param width es el ancho de la lista.
 */
void monster_list_show_subwindow(int height, int width)
{
	textblock *tb;
	monster_list_t *list;
	int i;

	if (height < 1 || width < 1)
		return;

	tb = textblock_new();
	list = monster_list_shared_instance();

	/* Forzar una actualización si se detectan monstruos */
	for (i = 1; i < cave_monster_max(cave); i++) {
		if (mflag_has(cave_monster(cave, i)->mflag, MFLAG_MARK)) {
			list->creation_turn = -1;
			break;
		}
	}

	monster_list_reset(list);
	monster_list_collect(list);
	monster_list_get_glyphs(list);
	monster_list_sort(list, monster_list_standard_compare);

	/* Dibujar la lista para que quepa exactamente en la subventana. */
	monster_list_format_textblock(list, tb, height, width, NULL, NULL);
	textui_textblock_place(tb, SCREEN_REGION, NULL);

	textblock_free(tb);
}

/**
 * Mostrar la lista de monstruos de forma interactiva. Esto dimensionará dinámicamente la lista
 * para obtener la mejor apariencia. Esto solo debe usarse en el terminal principal.
 *
 * \param height es el límite de altura para la lista.
 * \param width es el límite de ancho para la lista.
 */
void monster_list_show_interactive(int height, int width)
{
	textblock *tb;
	monster_list_t *list;
	size_t max_width = 0, max_height = 0;
	int safe_height, safe_width;
	region r;
	int sort_exp = 0;
	struct keypress ch;

	if (height < 1 || width < 1)
		return;

	// Repetir
	do {

		tb = textblock_new();
		list = monster_list_new();

		monster_list_collect(list);
		monster_list_get_glyphs(list);

		monster_list_sort(list, sort_exp ? monster_list_compare_exp : monster_list_standard_compare);

		/* Calcular el rectángulo de visualización óptimo. Se pasan números grandes como límite de altura
		 * y ancho para que podamos calcular el número máximo de filas y columnas para mostrar la lista
		 * adecuadamente. Luego ajustamos esos valores según sea necesario para que quepan en el terminal principal.
		 * La altura se ajusta para tener en cuenta el mensaje del textblock. La lista se posiciona en el lado derecho
		 * del terminal debajo de la línea de estado.
		 */
		monster_list_format_textblock(list, NULL, 1000, 1000, &max_height,
									  &max_width);
		safe_height = MIN(height - 3, (int) max_height + 3);
		safe_width = MIN(width - 40, (int) max_width);
		r.col = -safe_width;
		r.row = 1;
		r.width = safe_width;
		r.page_rows = safe_height;

		/*
		 * Dibujar la lista realmente. Pasamos max_height a la función de formato para
		 * que todas las líneas se añadan al textblock. El textblock en sí mismo
		 * se encargará de encajarlo en la región. Sin embargo, tenemos que pasar
		 * safe_width para que la función de formato pueda rellenar las líneas correctamente
		 * de modo que la cadena de ubicación esté alineada al borde derecho de la lista.
		 */
		monster_list_format_textblock(list, tb, (int) max_height, safe_width, NULL,
									  NULL);
		region_erase_bordered(&r);

		char buf[300];

		if (sort_exp) {
			my_strcpy(buf, "Presiona 'x' para DESACTIVAR 'ordenar por exp'", sizeof(buf));
		}
		else {
			my_strcpy(buf, "Presiona 'x' para ACTIVAR 'ordenar por exp'", sizeof(buf));
		}

		ch = textui_textblock_show(tb, r, buf);

		// Alternar ordenación
		sort_exp = !sort_exp;

		textblock_free(tb);
		monster_list_free(list);
	}
	while (ch.code == 'x');
}

/**
 * Forzar una actualización de la subventana de la lista de monstruos.
 *
 * Hay condiciones que monster_list_reset() no puede detectar, por lo que establecemos el
 * turno en un valor no válido para forzar la actualización de la lista.
 */
void monster_list_force_subwindow_update(void)
{
	monster_list_t *list = monster_list_shared_instance();
	list->creation_turn = -1;
}