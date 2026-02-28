/**
 * \file ui-target.c
 * \brief Interfaz de usuario para el código de apuntado
 *
 * Copyright (c) 1997-2014 Angband contributors
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
#include "game-input.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-path.h"
#include "player-timed.h"
#include "project.h"
#include "target.h"
#include "trap.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-map.h"
#include "ui-mon-lore.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-target.h"
#include "ui-term.h"

/*
 * Almacena el estado pasado entre target_set_interactive_aux() y los manejadores
 * que ayudan a manejar diferentes tipos de casillas o situaciones. En general,
 * los manejadores solo deben modificar press (devuelto desde
 * target_set_interactive_aux() a target_set_interactive()) y boring
 * (modula cómo actúan los manejadores posteriores).
 */
struct target_aux_state {
	char coord_desc[20];
	const char *phrase1;
	const char *phrase2;
	struct loc grid;
	ui_event press;
	int mode;
	bool boring;
};

typedef bool (*target_aux_handler)(struct chunk *c, struct player *p,
	struct target_aux_state *auxst);

/**
 * Extraer una dirección (o cero) de un carácter
 */
int target_dir(struct keypress ch)
{
	return target_dir_allow(ch, false, false);
}

/**
 * Extraer, con control más fino, una dirección (o cero) de un carácter.
 *
 * \param ch es la pulsación de tecla a examinar.
 * \param allow_5, si es true, permitirá que se devuelva 5 como dirección.
 * Si es false, se devolverá cero cuando se extraiga 5.
 * \param allow_esc, si es true, probará si ch es el desencadenante de un mapa de teclas
 * cuyo primer carácter en la acción es ESCAPE y, cuando eso ocurra,
 * devolverá ESCAPE.
 * \return un entero que está entre 0 y 4, inclusive, o entre 6 y 9, inclusive,
 * indicando la dirección extraída. Si no fue posible extraer una
 * dirección, devuelve 0. Si allow_5 es true, el valor devuelto también puede ser 5
 * cuando la dirección extraída es 5. Si allow_esc es true, el valor devuelto
 * también puede ser ESCAPE si ch es el desencadenante de un mapa de teclas cuyo primer
 * carácter en la acción es ESCAPE.
 *
 * Al examinar un mapa de teclas, ¿deberían omitirse los '(' o ')' ya que no
 * hacen nada más que alternar cómo se manejan los mensajes?
 */
int target_dir_allow(struct keypress ch, bool allow_5, bool allow_esc)
{
	int d = 0;

	/* ¿Ya es una dirección? */
	if (isdigit((unsigned char)ch.code)) {
		d = D2I(ch.code);
	} else if (isarrow(ch.code)) {
		switch (ch.code) {
			case ARROW_DOWN:  d = 2; break;
			case ARROW_LEFT:  d = 4; break;
			case ARROW_RIGHT: d = 6; break;
			case ARROW_UP:    d = 8; break;
		}
	} else {
		int mode;
		const struct keypress *act;

		if (OPT(player, rogue_like_commands))
			mode = KEYMAP_MODE_ROGUE;
		else
			mode = KEYMAP_MODE_ORIG;

		act = keymap_find(mode, ch);
		if (act && act->type == EVT_KBRD) {
			if (allow_esc && act->code == ESCAPE) {
				/*
				 * Permitir al jugador salir del apuntado
				 * con un mapa de teclas cuya acción comience con
				 * escape. Sugerido por
				 * https://github.com/angband/angband/issues/6297 .
				 * Para ahorrar pulsaciones de teclas adicionales al jugador,
				 * es tentador, si no hay un mapa de teclas activo
				 * o el mapa de teclas actual está al final,
				 * insertar el mapa de teclas desencadenado por ch en
				 * la cola de comandos, pero no sabemos si el
				 * ESCAPE pasado aquí terminará el procesamiento del
				 * último comando.
				 */
				d = ESCAPE;
			} else if (((unsigned char)act->code
					== cmd_lookup_key(CMD_WALK, mode)
					|| (unsigned char)act->code
					== cmd_lookup_key(CMD_RUN, mode))) {
				/*
				 * Permitir al jugador usar un mapa de teclas de movimiento
				 * de una sola acción para especificar la dirección.
				 */
				++act;
				if (act->type == EVT_KBRD
						&& isdigit((unsigned char)act->code)
						&& (act + 1)->type == EVT_NONE) {
					d = D2I(act->code);
				}
			}
		}
	}

	/* Paranoia */
	if (d == 5 && !allow_5) d = 0;

	/* Devolver dirección */
	return (d);
}

/**
 * Altura de la pantalla de ayuda; cualquier valor superior a 4 superpondrá la barra
 * de salud que queremos mantener en modo apuntado.
 */
#define HELP_HEIGHT 3

/**
 * Mostrar ayuda de apuntado en la parte inferior de la pantalla.
 */
static void target_display_help(bool monster, bool object, bool free,
		bool allow_pathfinding)
{
	/* Determinar ubicación de la ayuda */
	int wid, hgt, help_loc;
	Term_get_size(&wid, &hgt);
	help_loc = hgt - HELP_HEIGHT;
	
	/* Limpiar */
	clear_from(help_loc);

	/* Preparar ganchos de ayuda */
	text_out_hook = text_out_to_screen;
	text_out_indent = 1;
	Term_gotoxy(1, help_loc);

	/* Mostrar ayuda */
	text_out_c(COLOUR_L_GREEN, "<dir>");
	text_out(" y ");
	text_out_c(COLOUR_L_GREEN, "<clic>");
	text_out(" miran alrededor. '");
	if (allow_pathfinding) {
		text_out_c(COLOUR_L_GREEN, "g");
		text_out("' se mueve a la selección. '");
	}
	text_out_c(COLOUR_L_GREEN, "p");
	text_out("' selecciona al jugador. '");
	text_out_c(COLOUR_L_GREEN, "q");
	text_out("' sale. '");
	text_out_c(COLOUR_L_GREEN, "r");
	text_out("' muestra detalles. '");

	if (free) {
		text_out_c(COLOUR_L_GREEN, "m");
		text_out("' restringe a lugares interesantes.");
	} else {
		text_out_c(COLOUR_L_GREEN, "+");
		text_out("' y '");
		text_out_c(COLOUR_L_GREEN, "-");
		text_out("' recorren lugares. '");
		text_out_c(COLOUR_L_GREEN, "o");
		text_out("' permite selección libre.");
	}
	
	if (monster || free) {
		text_out(" '");
		text_out_c(COLOUR_L_GREEN, "t");
		text_out("' apunta a la selección.");
	}

	if (object) {
		unsigned char key = cmd_lookup_key(CMD_IGNORE,
			(OPT(player, rogue_like_commands)) ?
			KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG);
		char label[3];

		if (KTRL(key) == key) {
			label[0] = '^';
			label[1] = UN_KTRL(key);
			label[2] = '\0';
		} else {
			label[0] = key;
			label[1] = '\0';
		}
		text_out(" '");
		text_out_c(COLOUR_L_GREEN, "%s", label);
		text_out("' ignora la selección.");
	}

	text_out(" '");
	text_out_c(COLOUR_L_GREEN, ">");
	text_out("', '");
	text_out_c(COLOUR_L_GREEN, "<");
	text_out("', y '");
	text_out_c(COLOUR_L_GREEN, "x");
	text_out("' seleccionan las escaleras más cercanas o área inexplorada.");

	/* Reiniciar */
	text_out_indent = 0;
}


/**
 * Devolver si una tecla desencadena un mapa de teclas cuya única acción es correr.
 */
static bool is_running_keymap(struct keypress ch)
{
	int mode = (OPT(player, rogue_like_commands)) ?
		KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
	const struct keypress *act = keymap_find(mode, ch);

	if (act && act->type == EVT_KBRD && (unsigned char)act->code
			== cmd_lookup_key(CMD_RUN, mode)) {
		++act;
		if (act->type == EVT_NONE || (act->type == EVT_KBRD
				&& isdigit((unsigned char)act->code)
				&& (act + 1)->type == EVT_NONE)) {
			return true;
		}
	}
	return false;
}


/**
 * Realizar el ajuste mínimo de "panel completo" para asegurar que la ubicación
 * dada esté contenida dentro del panel actual. Opcionalmente tiene en cuenta
 * la ventana de ayuda de apuntado. Si targets no es NULL y el panel cambia,
 * reiniciar la lista de objetivos interesantes. Si show_interesting
 * y target_index no son NULL, reiniciar si está en modo de apuntado libre o no
 * dependiendo de si las nuevas coordenadas están en la lista de
 * objetivos interesantes.
 */
static void adjust_panel_help(int y, int x, bool help,
		struct player *p, int mode, struct point_set **targets,
		bool *show_interesting, int *target_index)
{
	bool changed = false;

	int j;

	int screen_hgt_main = help ? (Term->hgt - ROW_MAP - ROW_BOTTOM_MAP - 2)
			 : (Term->hgt - ROW_MAP - ROW_BOTTOM_MAP);

	/* Escanear ventanas */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
	{
		int wx, wy;
		int screen_hgt, screen_wid;

		term *t = angband_term[j];

		/* Sin ventana */
		if (!t) continue;

		/* Sin banderas relevantes */
		if ((j > 0) && !(window_flag[j] & PW_OVERHEAD)) continue;

		wy = t->offset_y;
		wx = t->offset_x;

		screen_hgt = (j == 0) ? screen_hgt_main : t->hgt;
		screen_wid = (j == 0) ? (Term->wid - COL_MAP - 1) : t->wid;

		/* Los paneles con mosaicos grandes necesitan ajuste */
		screen_wid = screen_wid / tile_width;
		screen_hgt = screen_hgt / tile_height;

		/* Ajustar según sea necesario */
		while (y >= wy + screen_hgt) wy += screen_hgt / 2;
		while (y < wy) wy -= screen_hgt / 2;

		/* Ajustar según sea necesario */
		while (x >= wx + screen_wid) wx += screen_wid / 2;
		while (x < wx) wx -= screen_wid / 2;

		/* Usar "modify_panel" */
		if (modify_panel(t, wy, wx)) changed = true;
	}

	if (changed) {
		handle_stuff(p);
		if (targets) {
			/* Recalcular casillas interesantes */
			point_set_dispose(*targets);
			*targets = target_get_monsters(mode, NULL, true);
		}
	}

	if (show_interesting && target_index) {
		/* Desactivar modo interesante si hicieron clic en un lugar aburrido... */
		*show_interesting = false;

		/* ...pero activarlo si hicieron clic en un lugar interesante */
		for (j = 0; j < point_set_size(*targets); j++) {
			if (y == (*targets)->pts[j].y
					&& x == (*targets)->pts[j].x) {
				*target_index = j;
				*show_interesting = true;
				break;
			}
		}
	}
}


/**
 * Mostrar el nombre del objeto seleccionado y permitir el recuerdo completo del objeto.
 *
 * Esto solo funcionará para un solo objeto en el suelo y no para un montón. Este
 * bucle es similar al bucle de recuerdo de monstruos en target_set_interactive_aux().
 * El tamaño de la matriz out_val debe coincidir con el tamaño que se pasa (ya que
 * este código se extrajo de allí).
 *
 * \param obj es el objeto a describir.
 * \param y es la fila de la cueva del objeto.
 * \param x es la columna de la cueva del objeto.
 * \param out_val es la cadena que contiene el nombre del objeto y se
 * devuelve a la función llamadora.
 * \param s1 es parte de la cadena de salida.
 * \param s2 es parte de la cadena de salida.
 * \param s3 es parte de la cadena de salida.
 * \param coords es parte de la cadena de salida.
 * \param p es el jugador que realiza el apuntado.
 * \return el último evento que ocurrió durante la visualización.
 */
static ui_event target_recall_loop_object(struct object *obj, int y, int x,
		char out_val[TARGET_OUT_VAL_SIZE],
		const char *s1,
		const char *s2,
		const char *s3,
		const char *coords,
		const struct player *p)
{
	bool recall = false;
	ui_event press;

	while (1) {
		if (recall) {
			display_object_recall_interactive(cave->objects[obj->oidx]);
			press = inkey_m();
		} else {
			char o_name[80];

			/* Obtener una descripción del objeto */
			object_desc(o_name, sizeof(o_name),
				cave->objects[obj->oidx],
				ODESC_PREFIX | ODESC_FULL, p);

			/* Describir el objeto */
			if (p->wizard) {
				strnfmt(out_val, TARGET_OUT_VAL_SIZE,
						"%s%s%s%s, %s (%d:%d, ruido=%d, olor=%d).", s1, s2, s3,
						o_name, coords, y, x, (int)cave->noise.grids[y][x],
						(int)cave->scent.grids[y][x]);
			} else {
				strnfmt(out_val, TARGET_OUT_VAL_SIZE,
						"%s%s%s%s, %s.", s1, s2, s3, o_name, coords);
			}

			prt(out_val, 0, 0);
			move_cursor_relative(y, x);
			press = inkey_m();
		}

		if ((press.type == EVT_MOUSE) && (press.mouse.button == 1) &&
			(KEY_GRID_X(press) == x) && (KEY_GRID_Y(press) == y))
			recall = !recall;
		else if ((press.type == EVT_KBRD) && (press.key.code == 'r'))
			recall = !recall;
		else
			break;
	}

	return press;
}

/**
 * Ayuda a target_set_interactive_aux(): reiniciar el estado para otra pasada
 * a través de los manejadores.
 */
static bool aux_reinit(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	struct monster *mon;

	/* Establecer el evento por defecto para enfocarse en el jugador. */
	auxst->press.type = EVT_KBRD;
	auxst->press.key.code = 'p';
	auxst->press.key.mods = 0;

	/* Salir si se mira una casilla prohibida. No ejecutar más manejadores. */
	if (!square_in_bounds(c, auxst->grid)) return true;

	/* Asumir aburrido. */
	auxst->boring = true;

	if (square(c, auxst->grid)->mon < 0) {
		/* Mirando la casilla del jugador */
		auxst->phrase1 = "Estás ";
		auxst->phrase2 = "en ";
	} else {
		/* Por defecto */
		if (square_isseen(c, auxst->grid)) {
			auxst->phrase1 = "Ves ";
		} else {
			mon = square_monster(c, auxst->grid);
			if (mon && monster_is_obvious(mon)) {
				/* El monstruo es visible gracias a detección o telepatía */
				auxst->phrase1 = "Sientes ";
			} else {
				auxst->phrase1 = "Recuerdas ";
			}
		}
		auxst->phrase2 = "";
	}

	return false;
}

/**
 * Ayuda a target_set_interactive_aux(): manejar alucinación.
 */
static bool aux_hallucinate(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	const char *name_strange = "algo extraño";
	char out_val[TARGET_OUT_VAL_SIZE];

	if (!p->timed[TMD_IMAGE]) return false;

	/* La alucinación lo estropea todo */
	/* Mostrar un mensaje */
	if (p->wizard) {
		strnfmt(out_val, sizeof(out_val),
			"%s%s%s, %s (%d:%d, ruido=%d, olor=%d).",
			auxst->phrase1,
			auxst->phrase2,
			name_strange,
			auxst->coord_desc,
			auxst->grid.y,
			auxst->grid.x,
			(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
			(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);
	} else {
		strnfmt(out_val, sizeof(out_val), "%s%s%s, %s.",
			auxst->phrase1,
			auxst->phrase2,
			name_strange,
			auxst->coord_desc);
	}
	prt(out_val, 0, 0);
	move_cursor_relative(auxst->grid.y, auxst->grid.x);

	auxst->press.key = inkey();

	/* Parar en todo menos "retorno" */
	return auxst->press.key.code != KC_ENTER;
}

/**
 * Ayuda a target_set_interactive_aux(): manejar monstruos.
 *
 * Nótese que si hay un monstruo en la casilla, actualizamos tanto la información
 * de recuerdo del monstruo como la barra de salud para rastrear ese monstruo.
 */
static bool aux_monster(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	struct monster *mon;
	const struct monster_lore *lore;
	char m_name[80];
	char out_val[TARGET_OUT_VAL_SIZE];
	bool recall;

	if (square(c, auxst->grid)->mon <= 0) return false;

	mon = square_monster(c, auxst->grid);
	if (!monster_is_obvious(mon)) return false;

	/* Monstruos visibles reales */
	lore = get_lore(mon->race);

	/* No aburrido */
	auxst->boring = false;

	/* Obtener el nombre del monstruo ("un kobold") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_IND_VIS);

	/* Rastrear la raza y salud de este monstruo */
	monster_race_track(p->upkeep, mon->race);
	health_track(p->upkeep, mon);
	handle_stuff(p);

	/* Interactuar */
	recall = false;
	while (1) {
		/* Recordar o apuntar */
		if (recall) {
			lore_show_interactive(mon->race, lore);
			auxst->press = inkey_m();
		} else {
			char buf[80];

			/* Describir el monstruo */
			look_mon_desc(buf, sizeof(buf),
				square(c, auxst->grid)->mon);

			/* Describir, y solicitar recuerdo */
			if (p->wizard) {
				strnfmt(out_val, sizeof(out_val),
					"%s%s%s (%s), %s (%d:%d, ruido=%d, olor=%d).",
					auxst->phrase1,
					auxst->phrase2,
					m_name,
					buf,
					auxst->coord_desc,
					auxst->grid.y,
					auxst->grid.x,
					(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
					(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);
			} else {
				strnfmt(out_val, sizeof(out_val),
					"%s%s%s (%s), %s.",
					auxst->phrase1,
					auxst->phrase2,
					m_name,
					buf,
					auxst->coord_desc);
			}

			prt(out_val, 0, 0);

			/* Colocar cursor */
			move_cursor_relative(auxst->grid.y, auxst->grid.x);

			/* Comando */
			auxst->press = inkey_m();
		}

		/* Comandos normales */
		if (auxst->press.type == EVT_MOUSE
				&& auxst->press.mouse.button == 1
				&& KEY_GRID_X(auxst->press) == auxst->grid.x
				&& KEY_GRID_Y(auxst->press) == auxst->grid.y) {
			recall = !recall;
		} else if (auxst->press.type == EVT_KBRD
				&& auxst->press.key.code == 'r') {
			recall = !recall;
		} else {
			break;
		}
	}

	if (auxst->press.type == EVT_MOUSE) {
		/* Parar en clic derecho */
		if (auxst->press.mouse.button == 2) return true;

		/* A veces parar en tecla "espacio" */
		if (auxst->press.mouse.button
				&& !(auxst->mode & (TARGET_LOOK))) return true;
	} else {
		/* Parar en todo menos "retorno"/"espacio" */
		if (auxst->press.key.code != KC_ENTER
				&& auxst->press.key.code != ' ') return true;

		/* A veces parar en tecla "espacio" */
		if (auxst->press.key.code == ' '
				&& !(auxst->mode & (TARGET_LOOK))) return true;
	}

	/* Describir objetos llevados (solo magos) */
	if (p->wizard) {
		const char *lphrase1;
		const char *lphrase2;
		struct object *obj;

		/* Tener en cuenta el género */
		if (rf_has(mon->race->flags, RF_FEMALE)) {
			lphrase1 = "Ella está ";
		} else if (rf_has(mon->race->flags, RF_MALE)) {
			lphrase1 = "Él está ";
		} else {
			lphrase1 = "Está ";
		}

		/* Usar un verbo */
		lphrase2 = "llevando ";

		/* Escanear todos los objetos llevados */
		for (obj = mon->held_obj; obj; obj = obj->next) {
			char o_name[80];

			/* Obtener una descripción del objeto */
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, p);

			strnfmt(out_val, sizeof(out_val),
				"%s%s%s, %s (%d:%d, ruido=%d, olor=%d).",
				lphrase1,
				lphrase2,
				o_name,
				auxst->coord_desc,
				auxst->grid.y,
				auxst->grid.x,
				(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
				(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);

			prt(out_val, 0, 0);
			move_cursor_relative(auxst->grid.y, auxst->grid.x);
			auxst->press = inkey_m();

			if (auxst->press.type == EVT_MOUSE) {
				/* Parar en clic derecho */
				if (auxst->press.mouse.button == 2) break;

				/* A veces parar en tecla "espacio" */
				if (auxst->press.mouse.button
						&& !(auxst->mode & (TARGET_LOOK)))
					break;
			} else {
				/* Parar en todo menos "retorno"/"espacio" */
				if (auxst->press.key.code != KC_ENTER
						&& auxst->press.key.code != ' ')
					break;

				/* A veces parar en tecla "espacio" */
				if (auxst->press.key.code == ' '
						&& !(auxst->mode & (TARGET_LOOK)))
					break;
			}

			/* Cambiar la introducción */
			lphrase2 = "también llevando ";
		}

		/* Doble rotura */
		if (obj) return true;
	}

	return false;
}

/**
 * Ayuda a target_set_interactive_aux(): manejar trampas visibles.
 */
static bool aux_trap(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	struct trap *trap;
	char out_val[TARGET_OUT_VAL_SIZE];
	const char *lphrase3;

	if (!square_isvisibletrap(p->cave, auxst->grid)) return false;

	/* Una trampa */
	trap = square(p->cave, auxst->grid)->trap;

	/* No aburrido */
	auxst->boring = false;

	/* Elegir el artículo indefinido apropiado */
	lphrase3 = (is_a_vowel(trap->kind->desc[0])) ? "una " : "un ";

	/* Interactuar */
	while (1) {
		/* Describir, y solicitar recuerdo */
		if (p->wizard) {
			strnfmt(out_val, sizeof(out_val),
				"%s%s%s%s, %s (%d:%d, ruido=%d, olor=%d).",
				auxst->phrase1,
				auxst->phrase2,
				lphrase3,
				trap->kind->name,
				auxst->coord_desc,
				auxst->grid.y,
				auxst->grid.x,
				(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
				(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);
		} else {
			strnfmt(out_val, sizeof(out_val), "%s%s%s%s, %s.",
				auxst->phrase1,
				auxst->phrase2,
				lphrase3,
				trap->kind->desc,
				auxst->coord_desc);
		}

		prt(out_val, 0, 0);

		/* Colocar cursor */
		move_cursor_relative(auxst->grid.y, auxst->grid.x);

		/* Comando */
		auxst->press = inkey_m();

		/* Parar en todo menos "retorno"/"espacio" */
		if (auxst->press.key.code != KC_ENTER
				&& auxst->press.key.code != ' ')
			break;

		/* A veces parar en tecla "espacio" */
		if (auxst->press.key.code == ' '
				&& !(auxst->mode & (TARGET_LOOK)))
			break;
	}

	return true;
}

/**
 * Ayuda a target_set_interactive_aux(): manejar objetos.
 */
static bool aux_object(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	int floor_max = z_info->floor_size;
	struct object **floor_list =
		mem_zalloc(floor_max * sizeof(*floor_list));
	bool result = false;
	char out_val[TARGET_OUT_VAL_SIZE];
	int floor_num;

	/* Escanear todos los objetos detectados en la casilla */
	floor_num = scan_distant_floor(floor_list, floor_max, p, auxst->grid);
	if (floor_num <= 0) {
		mem_free(floor_list);
		return result;
	}

	/* No aburrido */
	auxst->boring = false;

	track_object(p->upkeep, floor_list[0]);
	handle_stuff(p);

	/* Si hay más de un objeto... */
	if (floor_num > 1) {
		while (1) {
			/* Describir el montón */
			if (p->wizard) {
				strnfmt(out_val, sizeof(out_val),
					"%s%sun montón de %d objetos, %s (%d:%d, ruido=%d, olor=%d).",
					auxst->phrase1,
					auxst->phrase2,
					floor_num,
					auxst->coord_desc,
					auxst->grid.y,
					auxst->grid.x,
					(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
					(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);
			} else {
				strnfmt(out_val, sizeof(out_val),
					"%s%sun montón de %d objetos, %s.",
					auxst->phrase1,
					auxst->phrase2,
					floor_num,
					auxst->coord_desc);
			}

			prt(out_val, 0, 0);
			move_cursor_relative(auxst->grid.y, auxst->grid.x);
			auxst->press = inkey_m();

			/* Mostrar objetos */
			if ((auxst->press.type == EVT_MOUSE
					&& auxst->press.mouse.button == 1
					&& KEY_GRID_X(auxst->press) ==
					auxst->grid.x
					&& KEY_GRID_Y(auxst->press) ==
					auxst->grid.y)
					|| (auxst->press.type == EVT_KBRD
					&& auxst->press.key.code == 'r')) {
				int pos;
				while (1) {
					/* Guardar pantalla */
					screen_save();

					/*
					 * Usar OLIST_DEATH para mostrar etiquetas de objeto
					 */
					show_floor(floor_list, floor_num,
						(OLIST_DEATH | OLIST_WEIGHT
						| OLIST_GOLD), NULL);

					/* Describir el montón */
					prt(out_val, 0, 0);
					auxst->press = inkey_m();

					/* Cargar pantalla */
					screen_load();

					if (auxst->press.type == EVT_MOUSE) {
						pos = auxst->press.mouse.y - 1;
					} else {
						pos = auxst->press.key.code -
							'a';
					}
					if (0 <= pos && pos < floor_num) {
						track_object(p->upkeep,
							floor_list[pos]);
						handle_stuff(p);
						continue;
					}
					break;
				}

				/*
				 * Ahora que el usuario ha terminado con el bucle
				 * de visualización, repitamos el bucle exterior de nuevo.
				 */
				continue;
			}

			/* Hecho */
			break;
		}
	} else {
		/* Solo un objeto para mostrar */
		/* Obtener el único objeto en la lista */
		struct object *obj_local = floor_list[0];

		/* Permitir al usuario recordar un objeto */
		auxst->press = target_recall_loop_object(obj_local,
			auxst->grid.y, auxst->grid.x, out_val, auxst->phrase1,
			auxst->phrase2, "", auxst->coord_desc, p);

		/* Parar en todo menos "retorno"/"espacio" */
		if (auxst->press.key.code != KC_ENTER
				&& auxst->press.key.code != ' ') result = true;

		/* A veces parar en tecla "espacio" */
		if (auxst->press.key.code == ' '
				&& !(auxst->mode & (TARGET_LOOK))) result = true;
	}

	mem_free(floor_list);
	return result;
}

/**
 * Ayuda a target_set_interactive_aux(): manejar terreno.
 */
static bool aux_terrain(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	const char *name, *lphrase2, *lphrase3;
	char out_val[TARGET_OUT_VAL_SIZE];

	if (!auxst->boring && !square_isinteresting(p->cave, auxst->grid))
		return false;

	/* Característica del terreno si es necesario */
	name = square_apparent_name(p->cave, auxst->grid);

	/* Manejar casillas desconocidas */

	/* Elegir una preposición si es necesario */
	lphrase2 = (*auxst->phrase2) ?
		square_apparent_look_in_preposition(p->cave, auxst->grid) : "";

	/* Elegir prefijo para el nombre */
	lphrase3 = square_apparent_look_prefix(p->cave, auxst->grid);

	/* Mostrar un mensaje */
	if (p->wizard) {
		strnfmt(out_val, sizeof(out_val),
			"%s%s%s%s, %s (%d:%d, ruido=%d, olor=%d).",
			auxst->phrase1,
			lphrase2,
			lphrase3,
			name,
			auxst->coord_desc,
			auxst->grid.y,
			auxst->grid.x,
			(int)c->noise.grids[auxst->grid.y][auxst->grid.x],
			(int)c->scent.grids[auxst->grid.y][auxst->grid.x]);
	} else {
		strnfmt(out_val, sizeof(out_val),
			"%s%s%s%s, %s.",
			auxst->phrase1,
			lphrase2,
			lphrase3,
			name,
			auxst->coord_desc);
	}

	prt(out_val, 0, 0);
	move_cursor_relative(auxst->grid.y, auxst->grid.x);
	auxst->press = inkey_m();

	/*
	 * Parar en clic derecho del ratón o en todo menos "retorno"/"espacio" para
	 * una tecla.
	 */
	return (auxst->press.type == EVT_MOUSE
			&& auxst->press.mouse.button == 2)
		|| (auxst->press.type != EVT_MOUSE
			&& auxst->press.key.code != KC_ENTER
			&& auxst->press.key.code != ' ');
}

/**
 * Ayuda a target_set_interactive_aux(): comprobar qué hay en press para decidir si
 * hacer otra pasada a través de los manejadores.
 */
static bool aux_wrapup(struct chunk *c, struct player *p,
		struct target_aux_state *auxst)
{
	if (auxst->press.type == EVT_MOUSE) {
		/* Parar en clic derecho. */
		return auxst->press.mouse.button != 2;
	}
	/* Parar en todo menos "retorno". */
	return auxst->press.key.code != KC_ENTER;
}

/**
 * Examinar una casilla, devolver una pulsación de tecla.
 *
 * El argumento "mode" contiene la bandera "TARGET_LOOK", que
 * indica que la tecla "espacio" debería recorrer el contenido
 * de la casilla, en lugar de simplemente regresar inmediatamente. Esto
 * permite que el comando "mirar" obtenga información completa, sin hacer
 * que el comando "apuntar" sea molesto.
 *
 * Esta función maneja correctamente múltiples objetos por casilla, y objetos
 * y características del terreno en la misma casilla, aunque esto último nunca
 * sucede.
 *
 * Esta función debe manejar ceguera/alucinación.
 */
static ui_event target_set_interactive_aux(int y, int x, int mode)
{
	/*
	 * Si hay otros tipos que manejar, insertar una función para hacerlo
	 * entre aux_hallucinate y aux_wrapup. Debido a que cada manejador
	 * puede señalar que la secuencia se detenga, estos están ordenados en
	 * orden decreciente de precedencia.
	 */
	target_aux_handler handlers[] = {
		aux_reinit,
		aux_hallucinate,
		aux_monster,
		aux_trap,
		aux_object,
		aux_terrain,
		aux_wrapup
	};
	struct target_aux_state auxst;
	int ihandler;

	auxst.mode = mode;

	/* Describir la ubicación de la casilla */
	auxst.grid.x = x;
	auxst.grid.y = y;
	coords_desc(auxst.coord_desc, sizeof(auxst.coord_desc), y, x);

	/* Aplicar los manejadores en orden hasta terminar */
	ihandler = 0;
	while (1) {
		if ((*handlers[ihandler])(cave, player, &auxst)) break;
		++ihandler;
		if (ihandler >= (int) N_ELEMENTS(handlers)) ihandler = 0;
	}

	/* Seguir adelante */
	return auxst.press;
}

/**
 * Comando de apuntado
 */
void textui_target(void)
{
	if (target_set_interactive(TARGET_KILL, -1, -1, true))
		msg("Objetivo Seleccionado.");
	else
		msg("Apuntado Cancelado.");
}

/**
 * Apuntar al monstruo más cercano.
 *
 * XXX: Mover para usar CMD_TARGET_CLOSEST en algún momento en lugar de invocar
 * target_set_closest() directamente.
 */
void textui_target_closest(void)
{
	if (target_set_closest(TARGET_KILL, NULL)) {
		bool visibility;
		struct loc target;

		target_get(&target);

		/* Señal visual */
		Term_fresh();
		Term_get_cursor(&visibility);
		(void)Term_set_cursor(true);
		move_cursor_relative(target.y, target.x);
		/* TODO: ¿cuánto tiempo es apropiado para resaltar? */
		Term_xtra(TERM_XTRA_DELAY, 150);
		(void)Term_set_cursor(visibility);
	}
}


/**
 * Dibujar una ruta visible sobre las casillas entre (x1,y1) y (x2,y2).
 *
 * La ruta consiste en "*", que son blancos excepto donde hay un
 * monstruo, objeto o característica en la casilla.
 *
 * Esta rutina tiene (al menos) tres debilidades:
 * - los objetos/paredes recordados que ya no están presentes no se muestran,
 * - las casillas que (ej.) el jugador ha atravesado en la oscuridad se
 *   tratan como espacio desconocido.
 * - las paredes que parecen extrañas debido a la alucinación no se tratan correctamente.
 *
 * Las dos primeras resultan de que la información se pierde de las matrices de la mazmorra,
 * lo que requiere cambios en otras partes.
 */
static int draw_path(uint16_t path_n, struct loc *path_g, wchar_t *c, int *a,
					 int y1, int x1)
{
	int i;
	bool on_screen;
	bool pastknown = false;

	/* Sin ruta, así que no hacer nada. */
	if (path_n < 1) return 0;

	/* La casilla de inicio nunca se dibuja, pero notar si se está
     * mostrando. En teoría, podría ser la última casilla de este tipo.
     */
	on_screen = panel_contains(y1, x1);

	/* Dibujar la ruta. */
	for (i = 0; i < path_n; i++) {
		uint8_t colour;

		/* Encontrar las coordenadas en el nivel. */
		struct loc grid = path_g[i];
		struct monster *mon = square_monster(cave, grid);
		struct object *obj = square_object(player->cave, grid);

		/*
		 * Como path[] es una línea recta y la pantalla es rectangular,
		 * solo hay una sección de path[] en pantalla.
		 * Si la casilla que se está dibujando es visible, esta es parte de ella.
		 * Si no se ha dibujado nada de ella, continuar hasta que se encuentre alguna
		 * parte o se alcance la última casilla.
		 * Si ya se ha dibujado algo de ella, terminar ahora ya que no hay
		 * más casillas visibles para dibujar.
		 */
		 if (panel_contains(grid.y, grid.x)) on_screen = true;
		 else if (on_screen) break;
		 else continue;

	 	/* Encontrar la posición en pantalla */
		move_cursor_relative(grid.y, grid.x);

		/* Esta casilla está siendo sobrescrita, así que guardar la original. */
		Term_what(Term->scr->cx, Term->scr->cy, a + i, c + i);

		/* Elegir un color. */
		if (pastknown) {
			/* Una vez que pasamos una casilla desconocida, ya no sabemos
			 * si llegaremos a casillas posteriores */
			colour = COLOUR_L_DARK;
		} else if (mon && monster_is_visible(mon)) {
			/* Los imitadores actúan como objetos */
			if (monster_is_mimicking(mon)) {
				colour = COLOUR_YELLOW;
			} else if (!monster_is_camouflaged(mon)) {
				/* Los monstruos visibles son rojos. */
				colour = COLOUR_L_RED;
			} else if (obj) {
				/*
				 * El monstruo camuflado está en una casilla con
				 * un objeto; hacer que actúe como un objeto.
				 */
				colour = COLOUR_YELLOW;
			} else if (square_isknown(cave, grid)
					&& !square_isprojectable(player->cave,
					grid)) {
				/* El monstruo camuflado parece una pared. */
				colour = COLOUR_BLUE;
			} else {
				/*
				 * El monstruo camuflado parece una
				 * casilla desocupada.
				 */
				colour = COLOUR_WHITE;
			}
		} else if (obj)
			/* Los objetos conocidos son amarillos. */
			colour = COLOUR_YELLOW;

		else if (square_isknown(cave, grid)
				&& !square_isprojectable(player->cave, grid)) {
			/* Las paredes conocidas son azules. */
			colour = COLOUR_BLUE;

		} else if (!square_isknown(cave, grid)) {
			/* Las casillas desconocidas son grises. */
			pastknown = true;
			colour = COLOUR_L_DARK;

		} else
			/* Las casillas desocupadas son blancas. */
			colour = COLOUR_WHITE;

		/* Dibujar el segmento de ruta */
		(void)Term_addch(colour, L'*');
	}
	return i;
}


/**
 * Cargar el atributo/carácter en cada punto a lo largo de "path" que está en pantalla desde
 * "a" y "c". Esto se guardó en draw_path().
 */
static void load_path(uint16_t path_n, struct loc *path_g, wchar_t *c, int *a)
{
	int i;
	for (i = 0; i < path_n; i++) {
		int y = path_g[i].y;
		int x = path_g[i].x;

		if (!panel_contains(y, x)) continue;
		move_cursor_relative(y, x);
		Term_addch(a[i], c[i]);
	}

	Term_fresh();
}

/**
 * Devolver true si el montón de objetos contiene el objeto rastreado del jugador
 */
static bool pile_is_tracked(const struct object *obj) {
	for (const struct object *o = obj; o != NULL; o = o->next) {
		if (player->upkeep->object == o) {
			return true;
		}
	}
	return false;
}

/**
 * Devolver true si el montón de objetos contiene al menos 1 objeto conocido
 */
static bool pile_has_known(const struct object *obj) {
	for (const struct object *o = obj; o != NULL; o = o->next) {
		struct object *base_obj = cave->objects[o->oidx];
		if (!is_unknown(base_obj)) {
			return true;
		}
	}
	return false;
}

/**
 * Manejar "apuntar" y "mirar". Puede ser llamado desde comandos o "get_aim_dir()".
 *
 * \param mode es TARGET_LOOK (la lista de objetivos interesantes puede
 * incluir al jugador, monstruos, objetos, trampas y terreno interesante) o
 * TARGET_KILL (la lista de objetivos interesantes solo incluye monstruos
 * que pueden ser apuntados).
 * \param x es la posición x inicial del cursor de apuntado. Usar -1 para
 * que esta función determine la posición inicial.
 * \param y es la posición y inicial del cursor de apuntado. Usar -1 para
 * que esta función determine la posición inicial.
 * \param allow_pathfinding, si es true, permitirá al jugador iniciar
 * la búsqueda de ruta hacia una ubicación.
 * \return true si se ha establecido un objetivo correctamente, false en caso contrario.
 *
 * Actualmente, cuando se usan casillas "interesantes", y se presiona una tecla de
 * dirección, solo nos desplazamos por un único panel, en la dirección solicitada, y
 * comprobamos si hay casillas interesantes en ese panel. La solución "correcta"
 * implicaría escanear un conjunto más grande de casillas, incluyendo aquellas en paneles
 * que son adyacentes al que se está escaneando actualmente, pero esto es excesivo para
 * esta función.
 *
 * Apuntar/observar una "casilla del borde exterior" puede inducir problemas, por lo que esto
 * no está permitido actualmente.
 *
 * El jugador puede usar las teclas de dirección para moverse entre casillas
 * "interesantes" de manera heurística, o las teclas "espacio", "+", y "-" para
 * moverse a través de las casillas "interesantes" de manera secuencial, o
 * puede entrar en modo "ubicación", y usar las teclas de dirección para mover una
 * casilla a la vez en cualquier dirección. El comando "t" (establecer objetivo) solo
 * apuntará a un monstruo (en lugar de a una ubicación) si el monstruo es
 * target_able y se está usando el modo "interesante".
 *
 * La casilla actual se describe usando el método "mirar" anterior, y
 * se puede ingresar un nuevo comando en cualquier momento, pero nótese que si
 * la bandera "TARGET_LOOK" está establecida (o si estamos en modo "ubicación",
 * donde "espacio" no tiene un significado obvio) entonces "espacio" recorrerá
 * la descripción de la casilla actual hasta terminar, en lugar
 * de saltar inmediatamente a la siguiente casilla "interesante". Esto
 * permite que el comando "apuntar" conserve su semántica antigua.
 *
 * Las teclas "*", "+", y "-" siempre se pueden usar para saltar inmediatamente
 * al siguiente (o anterior) casilla interesante, en el modo apropiado.
 *
 * La tecla "retorno" siempre se puede usar para recorrer una descripción
 * completa de la casilla (para siempre).
 *
 * Este comando cancelará cualquier objetivo antiguo, incluso si se usa desde
 * dentro del comando "mirar".
 */
bool target_set_interactive(int mode, int x, int y, bool allow_pathfinding)
{
	int path_n;
	struct loc path_g[256];

	int wid, hgt, help_prompt_loc;

	bool done = false;
	bool show_interesting = true;
	bool help = false;
	keycode_t ignore_key = cmd_lookup_key(CMD_IGNORE,
		(OPT(player, rogue_like_commands)) ?
		KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG);

	/* Estos se usan para mostrar la ruta al objetivo */
	wchar_t *path_char = mem_zalloc(z_info->max_range * sizeof(wchar_t));
	int *path_attr = mem_zalloc(z_info->max_range * sizeof(int));

	/* Si no se nos ha dado una ubicación inicial, empezar en el
	   jugador, de lo contrario respetarla entrando en "modo de apuntado libre". */
	if (x == -1 || y == -1 || !square_in_bounds_fully(cave, loc(x, y))) {
		x = player->grid.x;
		y = player->grid.y;
	} else {
		show_interesting = false;
	}

	/* Cancelar objetivo */
	target_set_monster(0);

	/* Prevenir animaciones */
	disallow_animations();

	/* Calcular la ubicación de la ventana para el mensaje de ayuda */
	Term_get_size(&wid, &hgt);
	help_prompt_loc = hgt - 1;
	
	/* Mostrar el mensaje de ayuda */
	prt("Presiona '?' para ayuda.", help_prompt_loc, 0);

	/* Preparar el conjunto de objetivos */
	struct point_set *targets = target_get_monsters(mode, NULL, true);
	int target_index = 0;

	/* Interactuar */
	while (!done) {
		bool path_drawn = false;
		bool use_interesting_mode = show_interesting && point_set_size(targets);
		bool use_free_mode = !use_interesting_mode;

		/* Usar una casilla interesante si se solicita y las hay */
		if (use_interesting_mode) {
			y = targets->pts[target_index].y;
			x = targets->pts[target_index].x;

			/* Ajustar panel si es necesario */
			adjust_panel_help(y, x, help, player, mode, NULL,
				NULL, NULL);
		}

		/* Actualizar ayuda */
		if (help) {
			bool has_target = target_able(square_monster(cave, loc(x, y)));
			bool has_object = !(mode & TARGET_KILL)
					&& pile_has_known(square_object(cave, loc(x, y)));
			target_display_help(has_target, has_object,
				use_free_mode, allow_pathfinding);
		}

		/* Encontrar la ruta. */
		path_n = project_path(cave, path_g, z_info->max_range,
			loc(player->grid.x, player->grid.y), loc(x, y),
			PROJECT_THRU | PROJECT_INFO);

		/* Dibujar la ruta en modo "apuntar". Si hay una */
		if (mode & (TARGET_KILL))
			path_drawn = draw_path(path_n, path_g, path_char, path_attr,
					player->grid.y, player->grid.x);

		/* Describir y Preguntar */
		ui_event press = target_set_interactive_aux(y, x,
				mode | (use_free_mode ? TARGET_LOOK : 0));

		/* Eliminar la ruta */
		if (path_drawn) load_path(path_n, path_g, path_char, path_attr);

		/* Manejar un evento de entrada */
		if (event_is_mouse_m(press, 2, KC_MOD_CONTROL) || event_is_mouse(press, 3)) {
			/* Establecer un objetivo y terminar */
			y = KEY_GRID_Y(press);
			x = KEY_GRID_X(press);
			if (use_free_mode) {
				/* Modo libre: Apuntar a una ubicación */
				target_set_location(y, x);
				done = true;
			} else {
				/* Modo interesante: Intentar apuntar a un monstruo y terminar, o hacer sonar campana */
				struct monster *m_local = square_monster(cave, loc(x, y));

				if (target_able(m_local)) {
					/* Raza y salud del monstruo rastreadas por target_set_interactive_aux() */
					target_set_monster(m_local);
					done = true;
				} else {
					bell();
					if (!square_in_bounds(cave, loc(x, y))) {
						x = player->grid.x;
						y = player->grid.y;
					}
				}
			}

		} else if (allow_pathfinding
				&& event_is_mouse_m(press, 2, KC_MOD_ALT)) {
			/* Navegar a la ubicación y terminar */
			y = KEY_GRID_Y(press);
			x = KEY_GRID_X(press);
			cmdq_push(CMD_PATHFIND);
			cmd_set_arg_point(cmdq_peek(), "point", loc(x, y));
			done = true;

		} else if (event_is_mouse(press, 2)) {
			/* Cancelar y terminar */
			if (use_free_mode && (mode & TARGET_KILL)
					&& y == KEY_GRID_Y(press) && x == KEY_GRID_X(press)) {
				/* Modo libre/apuntar: Hizo clic en la ubicación actual, establecer objetivo */
				target_set_location(y, x);
			}
			done = true;

		} else if (event_is_mouse(press, 1)) {
			/* Reubicar cursor */
			y = KEY_GRID_Y(press);
			x = KEY_GRID_X(press);

			/* Si hicieron clic en un borde del mapa, arrastrar el cursor más lejos
			   para desencadenar un desplazamiento del panel */
			if (press.mouse.y <= 1) {
				y--;
			} else if (press.mouse.y >= Term->hgt - 2) {
				y++;
			} else if (press.mouse.x <= COL_MAP) {
				x--;
			} else if (press.mouse.x >= Term->wid - 2) {
				x++;
			}

			/* Restringir cursor a dentro de los límites */
			x = MAX(0, MIN(x, cave->width - 1));
			y = MAX(0, MIN(y, cave->height - 1));

			/*
			 * Ajustar panel y lista de objetivos si es necesario; también
			 * ajustar modo interesante
			 */
			adjust_panel_help(y, x, help, player, mode, &targets,
				&show_interesting, &target_index);

		} else if (event_is_key(press, ESCAPE) || event_is_key(press, 'q')) {
			/* Cancelar */
			done = true;

		} else if (event_is_key(press, ' ') || event_is_key(press, '*')
				|| event_is_key(press, '+')) {
			/* Recorrer objetivo interesante hacia adelante */
			if (use_interesting_mode && ++target_index == point_set_size(targets)) {
				target_index = 0;
			}

		} else if (event_is_key(press, '-')) {
			/* Recorrer objetivo interesante hacia atrás */
			if (use_interesting_mode && target_index-- == 0) {
				target_index = point_set_size(targets) - 1;
			}

		} else if (event_is_key(press, 'p')) {
			/* Enfocar al jugador y cambiar a modo libre */
			y = player->grid.y;
			x = player->grid.x;
			show_interesting = false;

			/* Recentrar alrededor del jugador */
			verify_panel();
			handle_stuff(player);

		} else if (event_is_key(press, 'o')) {
			/* Cambiar a modo libre */
			show_interesting = false;

		} else if (event_is_key(press, 'm')) {
			/* Cambiar a modo interesante */
			if (use_free_mode && point_set_size(targets) > 0) {
				show_interesting = true;
				target_index = 0;
				int min_dist = 999;

				/* Elegir el objetivo interesante más cercano */
				for (int i = 0; i < point_set_size(targets); i++) {
					int dist = distance(loc(x, y), targets->pts[i]);
					if (dist < min_dist) {
						target_index = i;
						min_dist = dist;
					}
				}
			}

		} else if (event_is_key(press, 't') || event_is_key(press, '5')
				|| event_is_key(press, '0') || event_is_key(press, '.')) {
			/* Establecer un objetivo y terminar */
			if (use_interesting_mode) {
				struct monster *m_local = square_monster(cave, loc(x, y));

				if (target_able(m_local)) {
					/* Raza y salud del monstruo rastreadas por target_set_interactive_aux() */
					target_set_monster(m_local);
					done = true;
				} else {
					bell();
				}
			} else {
				target_set_location(y, x);
				done = true;
			}

		} else if (allow_pathfinding && event_is_key(press, 'g')) {
			/* Navegar a una ubicación y terminar */
			cmdq_push(CMD_PATHFIND);
			cmd_set_arg_point(cmdq_peek(), "point", loc(x, y));
			done = true;

		} else if (event_is_key(press, ignore_key)) {
			/* Ignorar el objeto rastreado, establecido por target_set_interactive_aux() */
			if (!(mode & TARGET_KILL)
					&& pile_is_tracked(square_object(cave, loc(x, y)))) {
				textui_cmd_ignore_menu(player->upkeep->object);
				handle_stuff(player);

				/* Recalcular casillas interesantes */
				point_set_dispose(targets);
				targets = target_get_monsters(mode, NULL, true);
			}

		} else if (event_is_key(press, '>')) {
			struct loc new_grid;

			if (path_nearest_known(player, loc(x, y),
					square_isdownstairs, &new_grid, NULL)
					> 0) {
				x = new_grid.x;
				y = new_grid.y;
				/*
				 * Ajustar panel y lista de objetivos si es necesario; también
				 * ajustar modo interesante
				 */
				adjust_panel_help(y, x, help, player, mode,
					&targets, &show_interesting,
					&target_index);
			} else {
				bell();
			}

		} else if (event_is_key(press, '<')) {
			struct loc new_grid;

			if (path_nearest_known(player, loc(x, y),
					square_isupstairs, &new_grid, NULL)
					> 0) {
				x = new_grid.x;
				y = new_grid.y;
				/*
				 * Ajustar panel y lista de objetivos si es necesario; también
				 * ajustar modo interesante
				 */
				adjust_panel_help(y, x, help, player, mode,
					&targets, &show_interesting,
					&target_index);
			} else {
				bell();
			}

		} else if (event_is_key(press, 'x')) {
			struct loc new_grid;

			if (path_nearest_unknown(player, loc(x, y), &new_grid,
					NULL) > 0) {
				x = new_grid.x;
				y = new_grid.y;
				/*
				 * Ajustar panel y lista de objetivos si es necesario; también
				 * ajustar modo interesante
				 */
				adjust_panel_help(y, x, help, player, mode,
					&targets, &show_interesting,
					&target_index);
			} else {
				bell();
			}

		} else if (event_is_key(press, '?')) {
			/* Alternar texto de ayuda */
			help = !help;

			/* Redibujar ventana principal */
			player->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIP);
			Term_clear();
			handle_stuff(player);
			if (!help)
				prt("Presiona '?' para ayuda.", help_prompt_loc, 0);

		} else {
			/* Intentar extraer una dirección de la pulsación de tecla */
			int dir = target_dir_allow(press.key, false, true);

			if (!dir) {
				bell();
			} else if (dir == ESCAPE) {
				done = true;
			} else if (use_interesting_mode) {
				/* Dirección en modo interesante: Elegir nueva casilla interesante */
				int old_y = targets->pts[target_index].y;
				int old_x = targets->pts[target_index].x;
				int new_index;

				/* Buscar una nueva casilla interesante */
				new_index = target_pick(old_y, old_x, ddy[dir], ddx[dir], targets);

				/* Si no se encuentra ninguna, probar en el siguiente panel */
				if (new_index < 0) {
					int old_wy = Term->offset_y;
					int old_wx = Term->offset_x;

					if (change_panel(dir)) {
						/* Recalcular casillas interesantes */
						point_set_dispose(targets);
						targets = target_get_monsters(mode, NULL, true);

						/* Buscar una nueva casilla interesante de nuevo */
						new_index = target_pick(old_y, old_x, ddy[dir], ddx[dir], targets);

						/* Si no se encuentra ninguna de nuevo, reiniciar el panel y no hacer nada */
						if (new_index < 0 && modify_panel(Term, old_wy, old_wx)) {
							/* Recalcular casillas interesantes */
							point_set_dispose(targets);
							targets = target_get_monsters(mode, NULL, true);
						}

						handle_stuff(player);
					}
				}

				/* Usar la casilla interesante si se encuentra */
				if (new_index >= 0) target_index = new_index;
			} else {
				int step = (is_running_keymap(press.key)) ?
					10 : 1;

				/* Dirección en modo libre: Mover cursor */
				x += step * ddx[dir];
				y += step * ddy[dir];

				/* Mantener a 1 del borde */
				x = MAX(1, MIN(x, cave->width - 2));
				y = MAX(1, MIN(y, cave->height - 2));

				/* Ajustar panel y lista de objetivos si es necesario */
				adjust_panel_help(y, x, help, player, mode,
					&targets, NULL, NULL);
			}
		}
		/* Fin del while finalmente */
	}

	/* Olvidar */
	point_set_dispose(targets);

	/* Redibujar según sea necesario */
	if (help) {
		player->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIP);
		Term_clear();
	} else {
		prt("", 0, 0);
		prt("", help_prompt_loc, 0);
		player->upkeep->redraw |= (PR_DEPTH | PR_STATUS);
	}

	/* Recentrar alrededor del jugador */
	verify_panel();

	handle_stuff(player);

	mem_free(path_attr);
	mem_free(path_char);

	/* Permitir animaciones de nuevo */
	allow_animations();

	return target_is_set();
}