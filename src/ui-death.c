/**
 * \file ui-death.c
 * \brief Manejar las partes de la interfaz de usuario que ocurren después de que el personaje muere.
 *
 * Copyright (c) 1987 - 2007 Angband contributors
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
#include "cmds.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-info.h"
#include "savefile.h"
#include "store.h"
#include "ui-death.h"
#include "ui-history.h"
#include "ui-input.h"
#include "ui-knowledge.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-player.h"
#include "ui-score.h"
#include "ui-spoil.h"

/**
 * Escribir cadena formateada `fmt` en la línea `y`, centrada entre los puntos x1 y x2.
 */
static void put_str_centred(int y, int x1, int x2, const char *fmt, ...)
{
	va_list vp;
	char *tmp;
	size_t len;
	int x;

	/* Formatear en el tmp (expandible) */
	va_start(vp, fmt);
	tmp = vformat(fmt, vp);
	va_end(vp);

	/* Centrar ahora; tener en cuenta posibles caracteres multibyte */
	len = utf8_strlen(tmp);
	x = x1 + ((x2-x1)/2 - len/2);

	put_str(tmp, y, x);
}


/**
 * Mostrar la pantalla de lápida/jubilación
 */
static void display_exit_screen(void)
{
	ang_file *fp;
	char buf[1024];
	int line = 0;
	time_t death_time = (time_t)0;
	bool retired = streq(player->died_from, "Retirada");

	Term_clear();
	(void)time(&death_time);

	/* Abrir la imagen de fondo */
	path_build(buf, sizeof(buf), ANGBAND_DIR_SCREENS,
		(retired) ? "retire.txt" : "dead.txt");
	fp = file_open(buf, MODE_READ, FTYPE_TEXT);

	if (fp) {
		while (file_getl(fp, buf, sizeof(buf)))
			put_str(buf, line++, 0);

		file_close(fp);
	}

	line = 7;

	put_str_centred(line++, 8, 8+31, "%s", player->full_name);
	put_str_centred(line++, 8, 8+31, "el");
	if (player->total_winner)
		put_str_centred(line++, 8, 8+31, "Magnífico");
	else
		put_str_centred(line++, 8, 8+31, "%s", player->class->title[(player->lev - 1) / 5]);

	line++;

	put_str_centred(line++, 8, 8+31, "%s", player->class->name);
	put_str_centred(line++, 8, 8+31, "Nivel: %d", (int)player->lev);
	put_str_centred(line++, 8, 8+31, "Exp: %d", (int)player->exp);
	put_str_centred(line++, 8, 8+31, "AU: %d", (int)player->au);
	if (retired) {
		put_str_centred(line++, 8, 8+31, "Retirado en el Nivel %d",
			player->depth);
	} else {
		put_str_centred(line++, 8, 8+31, "Matado en el Nivel %d",
			player->depth);
		put_str_centred(line++, 8, 8+31, "por %s.", player->died_from);
	}

	line++;

	put_str_centred(line, 8, 8+31, "el %-.24s", ctime(&death_time));
}


/**
 * Mostrar la corona del ganador
 */
static void display_winner(void)
{
	char buf[1024];
	ang_file *fp;

	int wid, hgt;
	int i = 2;

	path_build(buf, sizeof(buf), ANGBAND_DIR_SCREENS, "crown.txt");
	fp = file_open(buf, MODE_READ, FTYPE_TEXT);

	Term_clear();
	Term_get_size(&wid, &hgt);

	if (fp) {
		char *pe;
		long lw;
		int width;

		/* Obtener la primera línea del archivo, que nos dice la longitud de la */
		/* línea más larga */
		file_getl(fp, buf, sizeof(buf));
		lw = strtol(buf, &pe, 10);
		width = (pe != buf && lw > 0 && lw < INT_MAX) ? (int)lw : 25;

		/* Volcar el archivo a la pantalla */
		while (file_getl(fp, buf, sizeof(buf))) {
			put_str(buf, i++, (wid / 2) - (width / 2));
		}

		file_close(fp);
	}

	put_str_centred(i, 0, wid, "¡Todos alaben al Poderoso Campeón!");

	event_signal(EVENT_INPUT_FLUSH);
	pause_line(Term);
}


/**
 * Comando del menú: volcar resumen del personaje a un archivo.
 */
static void death_file(const char *title, int row)
{
	char buf[1024];
	char ftmp[80];

	/* Obtener el nombre seguro para el sistema de archivos y añadir .txt */
	player_safe_name(ftmp, sizeof(ftmp), player->full_name, false);
	my_strcat(ftmp, ".txt", sizeof(ftmp));

	if (get_file(ftmp, buf, sizeof buf)) {
		bool success;

		/* Volcar un archivo de personaje */
		screen_save();
		success = dump_save(buf);
		screen_load();

		/* Verificar resultado */
		if (success)
			msg("Volcado de personaje exitoso.");
		else
			msg("¡Volcado de personaje falló!");

		/* Vaciar mensajes */
		event_signal(EVENT_MESSAGE_FLUSH);
	}
}

/**
 * Comando del menú: ver resumen e inventario del personaje.
 */
static void death_info(const char *title, int row)
{
	struct store *home = &stores[f_info[FEAT_HOME].shopnum - 1];

	screen_save();

	/* Mostrar jugador */
	display_player(0);

	/* Solicitar inventario */
	prt("Pulsa cualquier tecla para ver más información: ", 0, 0);

	/* Permitir abortar en este punto */
	(void)anykey();


	/* Mostrar equipo e inventario */

	/* Equipo -- si lo hay */
	if (player->upkeep->equip_cnt) {
		Term_clear();
		show_equip(OLIST_WEIGHT | OLIST_SEMPTY | OLIST_DEATH, NULL);
		prt("Estás usando: -más-", 0, 0);
		(void)anykey();
	}

	/* Inventario -- si lo hay */
	if (player->upkeep->inven_cnt) {
		Term_clear();
		show_inven(OLIST_WEIGHT | OLIST_DEATH, NULL);
		prt("Llevas: -más-", 0, 0);
		(void)anykey();
	}

	/* Carcaj -- si lo hay */
	if (player->upkeep->quiver_cnt) {
		Term_clear();
		show_quiver(OLIST_WEIGHT | OLIST_DEATH, NULL);
		prt("Tu carcaj contiene: -más-", 0, 0);
		(void)anykey();
	}

	/* Hogar -- si hay algo allí */
	if (home->stock) {
		int page;
		struct object *obj = home->stock;

		/* Mostrar contenido del hogar */
		for (page = 1; obj; page++) {
			int line;

			/* Limpiar pantalla */
			Term_clear();

			/* Mostrar 12 objetos */
			for (line = 0; obj && line < 12; obj = obj->next, line++) {
				uint8_t attr;

				char o_name[80];
				char tmp_val[80];

				/* Imprimir encabezado, limpiar línea */
				strnfmt(tmp_val, sizeof(tmp_val), "%c) ", I2A(line));
				prt(tmp_val, line + 2, 4);

				/* Obtener la descripción del objeto */
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_PREFIX | ODESC_FULL, player);

				/* Obtener el color del inventario */
				attr = obj->kind->base->attr;

				/* Mostrar el objeto */
				c_put_str(attr, o_name, line + 2, 7);
			}

			/* Título */
			prt(format("Tu hogar contiene (página %d): -más-", page), 0, 0);

			/* Esperar */
			(void)anykey();
		}
	}

	screen_load();
}

/**
 * Comando del menú: examinar mensajes previos a la muerte.
 */
static void death_messages(const char *title, int row)
{
	screen_save();
	do_cmd_messages();
	screen_load();
}

/**
 * Comando del menú: ver las veinte mejores puntuaciones.
 */
static void death_scores(const char *title, int row)
{
	screen_save();
	show_scores();
	screen_load();
}

/**
 * Comando del menú: examinar objetos en el inventario.
 */
static void death_examine(const char *title, int row)
{
	struct object *obj;
	const char *q, *s;

	/* Obtener un objeto */
	q = "¿Examinar qué objeto? ";
	s = "No tienes nada que examinar.";

	while (get_item(&obj, q, s, 0, NULL, (USE_INVEN | USE_QUIVER | USE_EQUIP | IS_HARMLESS))) {
		char header[120];

		textblock *tb;
		region area = { 0, 0, 0, 0 };

		tb = object_info(obj, OINFO_NONE);
		object_desc(header, sizeof(header), obj,
			ODESC_PREFIX | ODESC_FULL | ODESC_CAPITAL, player);

		textui_textblock_show(tb, area, header);
		textblock_free(tb);
	}
}


/**
 * Comando del menú: ver historial del personaje.
 */
static void death_history(const char *title, int row)
{
	history_display();
}

/**
 * Comando del menú: permitir generación de spoilers (principalmente para randarts).
 */
static void death_spoilers(const char *title, int row)
{
	do_cmd_spoilers();
}

/***
 * Comando del menú: comenzar una nueva partida
 */
static void death_new_game(const char *title, int row)
{
    play_again = get_check("¿Empezar una nueva partida? ");
}

/**
 * Estructuras de menú para el menú de muerte. Nótese que Salir debe ser siempre la
 * última opción, debido a una verificación codificada en death_screen
 */
static menu_action death_actions[] =
{
	{ 0, 'i', "Información",   death_info      },
	{ 0, 'm', "Mensajes",      death_messages  },
	{ 0, 'f', "Volcado a archivo",     death_file      },
	{ 0, 'v', "Ver puntuaciones",   death_scores    },
	{ 0, 'x', "Examinar objetos", death_examine   },
	{ 0, 'h', "Historia",       death_history   },
	{ 0, 's', "Spoilers",      death_spoilers  },
	{ 0, 'n', "Nueva Partida",      death_new_game  },
	{ 0, 'q', "Salir",          NULL            },
};



/**
 * Manejar la muerte del personaje
 */
void death_screen(void)
{
	struct menu *death_menu;
	bool done = false;
	const region area = { 51, 2, 0, N_ELEMENTS(death_actions) };

	/* Ganador */
	if (player->total_winner)
	{
		display_winner();
	}

	/* Lápida/Jubilación */
	display_exit_screen();

	/* Vaciar toda la entrada y salida */
	event_signal(EVENT_INPUT_FLUSH);
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Mostrar y usar el menú de muerte */
	death_menu = menu_new_action(death_actions,
			N_ELEMENTS(death_actions));

	death_menu->flags = MN_CASELESS_TAGS;

	menu_layout(death_menu, &area);

	while (!done && !play_again)
	{
		ui_event e = menu_select(death_menu, EVT_KBRD, false);
		if (e.type == EVT_KBRD)
		{
			if (e.key.code == KTRL('X')) break;
			if (e.key.code == KTRL('N')) play_again = true;
		}
		else if (e.type == EVT_SELECT)
		{
			done = get_check("¿Quieres salir? ");
		}
	}

	menu_free(death_menu);
}