/**
 * \file ui-output.c
 * \brief Colocar texto en la pantalla, guardar y cargar la pantalla, manejo de paneles 
 *
 * Copyright (c) 2007 Pete Mack and others.
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
#include "player-calcs.h"
#include "ui-input.h"
#include "ui-output.h"
#include "z-textblock.h"

/**
 * ------------------------------------------------------------------------
 * Regiones
 * ------------------------------------------------------------------------
 */

/**
 * Estas funciones se utilizan para manipular regiones en la pantalla, utilizadas
 * principalmente (pero no exclusivamente) por las funciones de menú.
 */

region region_calculate(region loc)
{
	int w, h;
	Term_get_size(&w, &h);

	if (loc.col < 0)
		loc.col += w;
	if (loc.row < 0)
		loc.row += h;
	if (loc.width <= 0)
		loc.width += w - loc.col;
	if (loc.page_rows <= 0)
		loc.page_rows += h - loc.row;

	return loc;
}

void region_erase_bordered(const region *loc)
{
	region calc = region_calculate(*loc);
	int i = 0;

	calc.col = MAX(calc.col - 1, 0);
	calc.row = MAX(calc.row - 1, 0);
	calc.width += 2;
	calc.page_rows += 2;

	for (i = 0; i < calc.page_rows; i++)
		Term_erase(calc.col, calc.row + i, calc.width);
}

void region_erase(const region *loc)
{
	region calc = region_calculate(*loc);
	int i = 0;

	for (i = 0; i < calc.page_rows; i++)
		Term_erase(calc.col, calc.row + i, calc.width);
}

bool region_inside(const region *loc, const ui_event *key)
{
	if ((loc->col > key->mouse.x) || (loc->col + loc->width <= key->mouse.x))
		return false;

	if ((loc->row > key->mouse.y) ||
		(loc->row + loc->page_rows <= key->mouse.y))
		return false;

	return true;
}


/**
 * ------------------------------------------------------------------------
 * Visualización de texto
 * ------------------------------------------------------------------------
 */

/**
 * Estas funciones están diseñadas para mostrar grandes bloques de texto en la pantalla
 * de una sola vez. Son la capa específica de la interfaz de usuario (ui-term) sobre las
 * funciones de z-textblock.c.
 */

/**
 * Función utilitaria
 */
static void display_area(const wchar_t *text, const uint8_t *attrs,
		size_t *line_starts, size_t *line_lengths,
		size_t n_lines,
		region area, size_t line_from)
{
	size_t i, j;

	n_lines = MIN(n_lines, (size_t) area.page_rows);

	for (i = 0; i < n_lines; i++) {
		Term_erase(area.col, area.row + i, area.width);
		for (j = 0; j < line_lengths[line_from + i]; j++) {
			Term_putch(area.col + j, area.row + i,
					attrs[line_starts[line_from + i] + j],
					text[line_starts[line_from + i] + j]);
		}
	}
}

/**
 * Coloca un textblock en la pantalla dentro de un cuadro delimitador específico.
 */
void textui_textblock_place(textblock *tb, region orig_area, const char *header)
{
	/* xxx al redimensionar esto debería recalcularse */
	region area = region_calculate(orig_area);

	size_t *line_starts = NULL, *line_lengths = NULL;
	size_t n_lines;

	n_lines = textblock_calculate_lines(tb,
			&line_starts, &line_lengths, area.width);

	if (header != NULL) {
		area.page_rows--;
		Term_erase(area.col, area.row, area.width);
		c_put_str(COLOUR_L_BLUE, header, area.row, area.col);
		area.row++;
	}

	if (n_lines > (size_t) area.page_rows)
		n_lines = area.page_rows;

	display_area(textblock_text(tb), textblock_attrs(tb), line_starts,
	             line_lengths, n_lines, area, 0);

	mem_free(line_starts);
	mem_free(line_lengths);
}

/**
 * Muestra un textblock de forma interactiva
 */
struct keypress textui_textblock_show(textblock *tb, region orig_area, const char *header)
{
	/* xxx al redimensionar esto debería recalcularse */
	region area = region_calculate(orig_area);

	size_t *line_starts = NULL, *line_lengths = NULL;
	size_t n_lines;

	struct keypress ch = KEYPRESS_NULL;

	n_lines = textblock_calculate_lines(tb,
			&line_starts, &line_lengths, area.width);

	screen_save();

	/* hacer espacio para el pie de página */
	area.page_rows -= 2;

	if (header != NULL) {
		area.page_rows--;
		Term_erase(area.col, area.row, area.width);
		c_put_str(COLOUR_L_BLUE, header, area.row, area.col);
		area.row++;
	}

	if (n_lines > (size_t) area.page_rows) {
		int start_line = 0;

		Term_erase(area.col, area.row + area.page_rows, area.width);
		Term_erase(area.col, area.row + area.page_rows + 1, area.width);
		c_put_str(COLOUR_L_BLUE, "(Arriba/Abajo o ESCAPE para salir.)",
				area.row + area.page_rows + 1, area.col);

		/* Modo paginador */
		while (1) {			

			display_area(textblock_text(tb), textblock_attrs(tb), line_starts,
					line_lengths, n_lines, area, start_line);

			ch = inkey();
			if (ch.code == ARROW_UP)
				start_line--;
			else if (ch.code == ESCAPE || ch.code == 'q' || ch.code == 'x')
				break;
			else if (ch.code == ']' || ch.code == '[')
				/* Caso especial para manejar listas de monstruos y objetos -
				 * ver bug #2120 */
				break;
			else if (ch.code == ARROW_DOWN)
				start_line++;
			else if (ch.code == ' ')
				start_line += area.page_rows;

			if (start_line < 0)
				start_line = 0;
			if (start_line + (size_t) area.page_rows > n_lines)
				start_line = n_lines - area.page_rows;
		}
	} else {
		display_area(textblock_text(tb), textblock_attrs(tb), line_starts,
				line_lengths, n_lines, area, 0);

		Term_erase(area.col, area.row + n_lines, area.width);
		Term_erase(area.col, area.row + n_lines + 1, area.width);
		c_put_str(COLOUR_L_BLUE, "(Presiona cualquier tecla para continuar.)",
				area.row + n_lines + 1, area.col);
		ch = inkey();
	}

	mem_free(line_starts);
	mem_free(line_lengths);

	screen_load();

	return (ch);
}


/**
 * ------------------------------------------------------------------------
 * Hook de text_out para la visualización en pantalla
 * ------------------------------------------------------------------------
 */

/**
 * Imprime algo de texto (coloreado) en la pantalla en la posición actual del cursor,
 * automáticamente "ajustando" el texto existente (en los espacios) cuando sea necesario para
 * evitar colocar cualquier texto en la última columna, y limpiando cada línea
 * antes de colocar texto en esa línea. Además, permite que un "newline" fuerce
 * un "ajuste" a la siguiente línea. Avanza el cursor según sea necesario para que las
 * llamadas secuenciales a esta función funcionen correctamente.
 *
 * Una vez que se ha llamado a esta función, el cursor no debe moverse
 * hasta que todas las llamadas relacionadas "text_out()" a la ventana estén completas.
 *
 * Esta función manejará correctamente cualquier ancho hasta el valor máximo legal
 * de 256, aunque funciona mejor para un ancho estándar de 80 caracteres.
 */
void text_out_to_screen(uint8_t a, const char *str)
{
	int x, y;

	int wid, h;

	int wrap;

	const wchar_t *s;
	wchar_t buf[1024];

	/* Obtener el tamaño */
	(void)Term_get_size(&wid, &h);

	/* Obtener el cursor */
	(void)Term_locate(&x, &y);

	/* Copiar a una cadena reescribible */
	text_mbstowcs(buf, str, 1024);
	
	/* ¿Usar un límite de ajuste especial? */
	if ((text_out_wrap > 0) && (text_out_wrap < wid))
		wrap = text_out_wrap;
	else
		wrap = wid;

	/* Procesar la cadena */
	for (s = buf; *s; s++) {
		wchar_t ch;

		/* Forzar ajuste */
		if (*s == L'\n') {
			/* Ajustar */
			x = text_out_indent;
			y++;

			/* Limpiar línea, mover cursor */
			Term_erase(x, y, 255);

			x += text_out_pad;
			Term_gotoxy(x, y);

			continue;
		}

		/* Limpiar el carácter */
		ch = (text_iswprint(*s) ? *s : L' ');

		/* Ajustar palabras según sea necesario */
		if ((x >= wrap - 1) && (ch != L' ')) {
			int i, n = 0;

			int av[256];
			wchar_t cv[256];

			/* Ajustar palabra */
			if (x < wrap) {
				/* Escanear texto existente */
				for (i = wrap - 2; i >= 0; i--) {
					/* Obtener atributo/carácter existente */
					Term_what(i, y, &av[i], &cv[i]);

					/* Romper en espacio */
					if (cv[i] == L' ') break;

					/* Rastrear palabra actual */
					n = i;
				}
			}

			/* Caso especial */
			if (n == 0) n = wrap;

			/* Limpiar línea */
			Term_erase(n, y, 255);

			/* Ajustar */
			x = text_out_indent;
			y++;

			/* Limpiar línea, mover cursor */
			Term_erase(x, y, 255);

			x += text_out_pad;
			Term_gotoxy(x, y);

			/* Ajustar la palabra (si existe) */
			for (i = n; i < wrap - 1; i++) {
				/* Volcar */
				Term_addch(av[i], cv[i]);

				/* Avanzar (sin ajuste) */
				if (++x > wrap) x = wrap;
			}
		}

		/* Volcar */
		Term_addch(a, ch);

		/* Avanzar */
		if (++x > wrap) x = wrap;
	}
}


/**
 * ------------------------------------------------------------------------
 * Visualización de texto simple
 * ------------------------------------------------------------------------
 */

/**
 * Muestra una cadena en la pantalla usando un atributo.
 *
 * En la ubicación dada, usando el atributo dado, si está permitido,
 * añade la cadena dada. No limpia la línea.
 */
void c_put_str(uint8_t attr, const char *str, int row, int col) {
	/* Posicionar cursor, Volcar el atributo/texto */
	Term_putstr(col, row, -1, attr, str);
}


/**
 * Como arriba, pero en blanco
 */
void put_str(const char *str, int row, int col) {
	c_put_str(COLOUR_WHITE, str, row, col);
}

/**
 * Muestra una cadena en la pantalla usando un atributo, y limpia hasta el
 * final de la línea.
 */
void c_prt(uint8_t attr, const char *str, int row, int col) {
	/* Limpiar línea, posicionar cursor */
	Term_erase(col, row, 255);

	/* Volcar el atributo/texto */
	Term_addstr(-1, attr, str);
}

/**
 * Como arriba, pero en blanco
 */
void prt(const char *str, int row, int col) {
	c_prt(COLOUR_WHITE, str, row, col);
}



/**
 * ------------------------------------------------------------------------
 * Guardar/Cargar pantalla
 * ------------------------------------------------------------------------
 */

/**
 * Guardar y cargar la pantalla se puede hacer a una profundidad arbitraria, pero es
 * importante que cada llamada a screen_save() esté equilibrada por una llamada a
 * screen_load() o screen_load_all() más tarde. 'screen_save_depth' es utilizado
 * por el juego para llevar la cuenta de si debe intentar actualizar el mapa y la
 * barra lateral o no, por lo que si te saltas un screen_load o screen_load_all no
 * obtendrás las actualizaciones adecuadas del juego.
 *
 * Term_save() / Term_load() / Term_load_all() hacen todo el trabajo pesado aquí.
 */

/**
 * Profundidad de la pila de screen_save()
 */
int16_t screen_save_depth;

/**
 * Guarda la pantalla y aumenta la profundidad "icky".
 */
void screen_save(void)
{
	player->upkeep->redraw |= PR_MAP;
	redraw_stuff(player);
	event_signal(EVENT_MESSAGE_FLUSH);
	Term_save();
	screen_save_depth++;
}

/**
 * Carga la pantalla y disminuye la profundidad "icky".
 */
void screen_load(void)
{
	event_signal(EVENT_MESSAGE_FLUSH);
	Term_load();
	screen_save_depth--;
}

/**
 * Carga la pantalla reproduciendo todos los guardados en orden inverso con un redibujado
 * para cada uno y disminuye la profundidad "icky".
 */
void screen_load_all(void)
{
	event_signal(EVENT_MESSAGE_FLUSH);
	Term_load_all();
	screen_save_depth--;
}

bool textui_map_is_visible(void)
{
	return (screen_save_depth == 0);
}

/**
 * ------------------------------------------------------------------------
 * Cosas varias
 * ------------------------------------------------------------------------
 */

/**
 * Una función de 'ventana' al estilo Hengband, que dibuja un cuadro circundante en arte ASCII.
 */
void window_make(int origin_x, int origin_y, int end_x, int end_y)
{
	int n;
	region to_clear;

	to_clear.col = origin_x;
	to_clear.row = origin_y;
	to_clear.width = end_x - origin_x;
	to_clear.page_rows = end_y - origin_y;

	region_erase(&to_clear);

	Term_putch(origin_x, origin_y, COLOUR_WHITE, L'+');
	Term_putch(end_x, origin_y, COLOUR_WHITE, L'+');
	Term_putch(origin_x, end_y, COLOUR_WHITE, L'+');
	Term_putch(end_x, end_y, COLOUR_WHITE, L'+');

	for (n = 1; n < (end_x - origin_x); n++) {
		Term_putch(origin_x + n, origin_y, COLOUR_WHITE, L'-');
		Term_putch(origin_x + n, end_y, COLOUR_WHITE, L'-');
	}

	for (n = 1; n < (end_y - origin_y); n++) {
		Term_putch(origin_x, origin_y + n, COLOUR_WHITE, L'|');
		Term_putch(end_x, origin_y + n, COLOUR_WHITE, L'|');
	}
}

bool panel_should_modify(term *t, int wy, int wx)
{
	int dungeon_hgt = cave->height;
	int dungeon_wid = cave->width;
	int screen_hgt = (t == angband_term[0]) ?
		SCREEN_HGT : t->hgt / tile_height;
	int screen_wid = (t == angband_term[0]) ?
		SCREEN_WID : t->wid / tile_width;

	/* Verificar wy, ajustar si es necesario */
	if (wy > dungeon_hgt - screen_hgt) wy = dungeon_hgt - screen_hgt;
	if (wy < 0) wy = 0;

	/* Verificar wx, ajustar si es necesario */
	if (wx > dungeon_wid - screen_wid) wx = dungeon_wid - screen_wid;
	if (wx < 0) wx = 0;

	/* ¿Necesita cambios? */
	return ((t->offset_y != wy) || (t->offset_x != wx));
}

/**
 * Modifica el panel actual a las coordenadas dadas, ajustando solo para
 * asegurar que las coordenadas sean legales, y devuelve verdadero si se hizo algo.
 *
 * La ciudad nunca debe desplazarse.
 *
 * Nota: los monstruos ya no se ven afectados de ninguna manera por los cambios de panel.
 *
 * Como un truco total, cada vez que cambia el panel actual, asumimos que
 * la ventana de "vista general" debe actualizarse.
 */
bool modify_panel(term *t, int wy, int wx)
{
	int dungeon_hgt = cave->height;
	int dungeon_wid = cave->width;
	int screen_hgt = (t == angband_term[0]) ?
		SCREEN_HGT : t->hgt / tile_height;
	int screen_wid = (t == angband_term[0]) ?
		SCREEN_WID : t->wid / tile_width;

	/* Verificar wy, ajustar si es necesario */
	if (wy > dungeon_hgt - screen_hgt) wy = dungeon_hgt - screen_hgt;
	if (wy < 0) wy = 0;

	/* Verificar wx, ajustar si es necesario */
	if (wx > dungeon_wid - screen_wid) wx = dungeon_wid - screen_wid;
	if (wx < 0) wx = 0;

	/* Reaccionar a los cambios */
	if (panel_should_modify(t, wy, wx)) {
		/* Guardar wy, wx */
		t->offset_y = wy;
		t->offset_x = wx;

		/* Redibujar mapa */
		player->upkeep->redraw |= (PR_MAP);

		/* Cambiado */
		return (true);
	}

	/* Sin cambios */
	return (false);
}

static void verify_panel_int(bool centered)
{
	int wy, wx;
	int screen_hgt, screen_wid;

	int panel_wid, panel_hgt;

	int py = player->grid.y;
	int px = player->grid.x;

	int j;

	/* Escanear ventanas */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		term *t = angband_term[j];

		/* Sin ventana */
		if (!t) continue;

		/* Sin banderas relevantes */
		if ((j > 0) && !(window_flag[j] & (PW_OVERHEAD))) continue;

		wy = t->offset_y;
		wx = t->offset_x;

		screen_hgt = (j == 0) ? SCREEN_HGT : t->hgt / tile_height;
		screen_wid = (j == 0) ? SCREEN_WID : t->wid / tile_width;

		panel_wid = screen_wid / 2;
		panel_hgt = screen_hgt / 2;


		/* Desplazar pantalla verticalmente cuando está descentrada */
		if (centered && !player->upkeep->running && (py != wy + panel_hgt))
			wy = py - panel_hgt;

		/* Desplazar pantalla verticalmente cuando está a 3 cuadros del borde superior/inferior */
		else if ((py < wy + 3) || (py >= wy + screen_hgt - 3))
			wy = py - panel_hgt;


		/* Desplazar pantalla horizontalmente cuando está descentrada */
		if (centered && !player->upkeep->running && (px != wx + panel_wid))
			wx = px - panel_wid;

		/* Desplazar pantalla horizontalmente cuando está a 3 cuadros del borde izquierdo/derecho */
		else if ((px < wx + 3) || (px >= wx + screen_wid - 3))
			wx = px - panel_wid;


		/* Desplazar si es necesario */
		modify_panel(t, wy, wx);
	}
}

/**
 * Cambia el panel actual al panel que se encuentra en la dirección dada.
 *
 * Devuelve verdadero si el panel fue cambiado.
 */
bool change_panel(int dir)
{
	bool changed = false;
	int j;

	/* Escanear ventanas */
	for (j = 0; j < ANGBAND_TERM_MAX; j++) {
		int screen_hgt, screen_wid;
		int wx, wy;

		term *t = angband_term[j];

		/* Sin ventana */
		if (!t) continue;

		/* Sin banderas relevantes */
		if ((j > 0) && !(window_flag[j] & PW_OVERHEAD)) continue;

		screen_hgt = (j == 0) ? SCREEN_HGT : t->hgt / tile_height;
		screen_wid = (j == 0) ? SCREEN_WID : t->wid / tile_width;

		/* Desplazar medio panel */
		wy = t->offset_y + ddy[dir] * screen_hgt / 2;
		wx = t->offset_x + ddx[dir] * screen_wid / 2;

		/* Usar "modify_panel" */
		if (modify_panel(t, wy, wx)) changed = true;
	}

	return (changed);
}


/**
 * Verifica el panel actual (en relación con la ubicación del jugador).
 *
 * Por defecto, cuando el jugador se acerca "demasiado" al borde del panel
 * actual, el mapa se desplaza un panel en esa dirección para que el jugador
 * ya no esté tan cerca del borde.
 *
 * La opción "OPT(player, center_player)" permite que el panel actual esté siempre
 * centrado alrededor del jugador, lo cual es muy costoso, y también tiene algunas
 * ramificaciones interesantes en la jugabilidad.
 */
void verify_panel(void)
{
	verify_panel_int(OPT(player, center_player));
}

void center_panel(void)
{
	verify_panel_int(true);
}

void textui_get_panel(int *min_y, int *min_x, int *max_y, int *max_x)
{
	term *t = term_screen;

	if (!t) return;

	*min_y = t->offset_y;
	*min_x = t->offset_x;
	*max_y = t->offset_y + SCREEN_HGT;
	*max_x = t->offset_x + SCREEN_WID;
}

bool textui_panel_contains(unsigned int y, unsigned int x)
{
	unsigned int hgt;
	unsigned int wid;
	if (!Term)
		return true;
	if (Term == term_screen) {
		hgt = SCREEN_HGT;
		wid = SCREEN_WID;
	} else {
		hgt = Term->hgt / tile_height;
		wid = Term->wid / tile_width;
	}
	return (y - Term->offset_y) < hgt && (x - Term->offset_x) < wid;
}