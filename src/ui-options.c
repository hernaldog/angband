/**
 * \file ui-options.c
 * \brief Código de manejo de opciones de la interfaz de texto (todo lo accesible desde '=')
 *
 * Copyright (c) 1997-2000 Robert A. Koeneke, James E. Wilson, Ben Harrison
 * Copyright (c) 2007 Pete Mack
 * Copyright (c) 2010 Andi Sidwell
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
#include "obj-ignore.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-calcs.h"
#include "ui-birth.h"
#include "ui-display.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-knowledge.h"
#include "ui-menu.h"
#include "ui-options.h"
#include "ui-prefs.h"
#include "ui-target.h"


/**
 * Preguntar al usuario por un nombre de archivo para guardar el archivo de preferencias.
 */
static bool get_pref_path(const char *what, int row, char *buf, size_t max)
{
	char ftmp[80];
	bool ok;

	screen_save();

	/* Mensaje */
	if (row > 0) {
		prt("", row - 1, 0);
	}
	prt(format("%s a un archivo de preferencias", what), row, 0);
	prt("", row + 1, 0);
	prt("Archivo: ", row + 2, 0);
	prt("", row + 3, 0);

	/* Obtener el nombre seguro para el sistema de archivos y añadir .prf */
	player_safe_name(ftmp, sizeof(ftmp), player->full_name, true);
	my_strcat(ftmp, ".prf", sizeof(ftmp));

	/* Obtener un nombre de archivo */
	
	if(!arg_force_name)
		ok = askfor_aux(ftmp, sizeof ftmp, NULL);
	
	else
		ok = get_check(format("¿Confirmar escritura en %s? ", ftmp));

	screen_load();

	/* Construir el nombre de archivo */
	if (ok)
		path_build(buf, max, ANGBAND_DIR_USER, ftmp);

	return ok;
}


static void dump_pref_file(void (*dump)(ang_file *), const char *title, int row)
{
	char buf[1024];

	/* Obtener nombre de archivo del usuario */
	if (!get_pref_path(title, row, buf, sizeof(buf)))
		return;

	/* Intentar guardar */
	if (prefs_save(buf, dump, title))
		msg("Guardado %s.", strstr(title, " ") + 1);
	else
		msg("Fallo al guardar %s.", strstr(title, " ") + 1);

	event_signal(EVENT_MESSAGE_FLUSH);

	return;
}

static void do_cmd_pref_file_hack(long row);






/**
 * ------------------------------------------------------------------------
 * Visualización y configuración de opciones
 * ------------------------------------------------------------------------ */


/**
 * Muestra una entrada de opción.
 */
static void option_toggle_display(struct menu *m, int oid, bool cursor,
		int row, int col, int width)
{
	uint8_t attr = curs_attrs[CURS_KNOWN][cursor != 0];
	bool *options = menu_priv(m);
	const char *desc = option_desc(oid);
	size_t u8len = utf8_strlen(desc);

	if (u8len < 45) {
		c_prt(attr, format("%s%*s", desc, (int)(45 - u8len), " "), row,
			col);
	} else {
		char *desc_copy = string_make(desc);

		if (u8len > 45) {
			utf8_clipto(desc_copy, 45);
		}
		c_prt(attr, desc_copy, row, col);
		string_free(desc_copy);
	}
	c_prt(attr, format(": %s  (%s)", options[oid] ? "sí" : "no",
		option_name(oid)), row, col + 45);
}

/**
 * Maneja las pulsaciones de tecla para una entrada de opción.
 */
static bool option_toggle_handle(struct menu *m, const ui_event *event,
		int oid)
{
	bool next = false;
	int page = option_type(oid);

	if (event->type == EVT_SELECT) {
		/* Las opciones de nacimiento no se pueden cambiar después del nacimiento */
		/* Al nacer, m->flags == MN_DBL_TAP. */
		/* Después del nacimiento, m->flags == MN_NO_TAGS */
		if (!((page == OP_BIRTH) && (m->flags == MN_NO_TAGS))) {
			option_set(option_name(oid), !player->opts.opt[oid]);
		}
	} else if (event->type == EVT_KBRD) {
		if (event->key.code == 's' || event->key.code == 'S') {
			option_set(option_name(oid), true);
			next = true;
		} else if (event->key.code == 'n' || event->key.code == 'N') {
			option_set(option_name(oid), false);
			next = true;
		} else if (event->key.code == 't' || event->key.code == 'T') {
			option_set(option_name(oid), !player->opts.opt[oid]);
		} else if (event->key.code == 'g' || event->key.code == 'G') {
			char dummy;

			screen_save();
			if (options_save_custom(&player->opts, page)) {
				get_com("Guardado correctamente. Pulsa cualquier tecla para continuar.", &dummy);
			} else {
				get_com("Fallo al guardar. Pulsa cualquier tecla para continuar.", &dummy);
			}
			screen_load();
		/*
		 * Para opciones de nacimiento, solo permitir restaurar desde
		 * valores personalizados al nacer.
		 */
		} else if ((event->key.code == 'r' || event->key.code == 'R') &&
				(page != OP_BIRTH || m->flags == MN_DBL_TAP)) {
			screen_save();
			if (options_restore_custom(&player->opts, page)) {
				screen_load();
				menu_refresh(m, false);
			} else {
				char dummy;

				get_com("Fallo al restaurar. Pulsa cualquier tecla para continuar.", &dummy);
				screen_load();
			}
		/*
		 * Para opciones de nacimiento, solo permitir restaurar a los valores
		 * por defecto del mantenedor al nacer.
		 */
		} else if ((event->key.code == 'x' || event->key.code == 'X') &&
				(page != OP_BIRTH || m->flags == MN_DBL_TAP)) {
			options_restore_maintainer(&player->opts, page);
			menu_refresh(m, false);
		} else {
			return false;
		}
	} else {
		return false;
	}

	if (next) {
		m->cursor++;
		m->cursor = (m->cursor + m->filter_count) % m->filter_count;
	}

	return true;
}

/**
 * Presentar un menú contextual para las opciones de nacimiento o interfaz para que lo que
 * es accesible mediante el teclado también se pueda hacer si solo se usa el ratón.
 *
 * \param m es la estructura que describe el menú de opciones.
 * \param in es el evento que desencadena el menú contextual. in->type debe ser
 * EVT_MOUSE.
 * \param out es el evento que se pasará hacia arriba (al manejo interno en
 * menu_select() o, potencialmente, al llamador de menu_select()).
 * \return true si el evento fue manejado; en caso contrario, devuelve false.
 *
 * La lógica aquí se superpone con lo que se hace en option_toggle_handle().
 */
static bool use_option_context_menu(struct menu *m, const ui_event *in,
		ui_event *out)
{
	enum {
		ACT_CTX_OPT_SAVE,
		ACT_CTX_OPT_RESTORE,
		ACT_CTX_OPT_RESET
	};
	/*
	 * Como un pequeño truco, obtener el tipo de opciones involucradas de la primera
	 * opción seleccionada por el filtro del menú.
	 */
	int page = option_type(m->filter_list[0]);
	char *labels = string_make(lower_case);
	struct menu *cm = menu_dynamic_new();
	bool refresh = false;
	char save_label[40];
	int selected;
	char dummy;

	cm->selections = labels;
	strnfmt(save_label, sizeof(save_label), "Guardar como opciones %s predeterminadas",
		option_type_name(page));
	menu_dynamic_add_label(cm, save_label, 'g', ACT_CTX_OPT_SAVE, labels);
	if (m->flags == MN_DBL_TAP) {
		menu_dynamic_add_label(cm, "Restaurar desde valores guardados", 'r',
			ACT_CTX_OPT_RESTORE, labels);
		menu_dynamic_add_label(cm, "Restablecer a valores de fábrica", 'x',
			ACT_CTX_OPT_RESET, labels);
	}

	screen_save();

	assert(in->type == EVT_MOUSE);
	menu_dynamic_calc_location(cm, in->mouse.x, in->mouse.y);
	region_erase_bordered(&cm->boundary);

	selected = menu_dynamic_select(cm);

	menu_dynamic_free(cm);
	string_free(labels);

	switch (selected) {
	case ACT_CTX_OPT_SAVE:
		if (options_save_custom(&player->opts, page)) {
			get_com("Guardado correctamente. Pulsa cualquier tecla para "
				"continuar.", &dummy);
		} else {
			get_com("Fallo al guardar. Pulsa cualquier tecla para continuar.",
				&dummy);
		}
		break;

	case ACT_CTX_OPT_RESTORE:
		if (options_restore_custom(&player->opts, page)) {
			refresh = true;
		} else {
			get_com("Fallo al restaurar. Pulsa cualquier tecla para continuar.",
				&dummy);
		}
		break;

	case ACT_CTX_OPT_RESET:
		options_restore_maintainer(&player->opts, page);
		refresh = true;
		break;

	default:
		/* No hay nada que hacer. */
		break;
	}

	screen_load();
	if (refresh) {
		menu_refresh(m, false);
	}

	return true;
}


/**
 * Funciones de visualización y manejo del menú de alternar opciones
 */
static const menu_iter option_toggle_iter = {
	NULL,
	NULL,
	option_toggle_display,
	option_toggle_handle,
	NULL
};


/**
 * Interactuar con algunas opciones
 */
static void option_toggle_menu(const char *name, int page)
{
	static const char selections[] = "abcdefgimopquvwzABCDEFGHIJKLMOPQUVWZ";
	int i;
	
	struct menu *m = menu_new(MN_SKIN_SCROLL, &option_toggle_iter);

	/* para todos los menús */
	m->prompt = "Setear opción (s/n/t), usar teclas de mov o índice";
	m->cmd_keys = "SsNnTt";
	m->selections = selections;
	m->flags = MN_DBL_TAP;

	/* Añadimos 10 a la cantidad de página para indicar que estamos al nacer */
	if (page == OPT_PAGE_BIRTH) {
		m->prompt = "Solo puedes modificar opciones al nacer el personaje.";
		m->cmd_keys = "";
		m->flags = MN_NO_TAGS;
	} else if (page == OPT_PAGE_BIRTH + 10 || page == OP_INTERFACE) {
		m->prompt = "Setear opción (s/n/t), 'g' guardar, 'r' restaurar, 'x' reiniciar";
		m->cmd_keys = "SsNnTtGgRrXx";
		/* Proporcionar un menú contextual para equivalentes a 'g', 'r', ... */
		m->context_hook = use_option_context_menu;
		if (page == OPT_PAGE_BIRTH + 10) {
			page -= 10;
		}
	}

	/* para este menú en particular */
	m->title = name;

	/* Encontrar el número de entradas válidas */
	for (i = 0; option_page[page][i] != OPT_none; ++i) {}

	/* Establecer los datos a las opciones del jugador */
	menu_setpriv(m, OPT_MAX, &player->opts.opt);
	menu_set_filter(m, option_page[page], i);
	menu_layout(m, &SCREEN_REGION);

	/* Ejecutar el menú */
	screen_save();

	clear_from(0);
	menu_select(m, 0, false);

	screen_load();

	mem_free(m);
}

/**
 * Editar opciones de nacimiento.
 */
void do_cmd_options_birth(void)
{
	option_toggle_menu("Opciones de Nacimiento", OPT_PAGE_BIRTH + 10);
}


/**
 * Modificar las opciones de "ventana"
 */
static void do_cmd_options_win(const char *name, int row)
{
	int i, j, d;
	int y = 0;
	int x = 0;
	ui_event ke;
	uint32_t new_flags[ANGBAND_TERM_MAX];

	/* Establecer nuevas banderas a los valores antiguos */
	for (j = 0; j < ANGBAND_TERM_MAX; j++)
		new_flags[j] = window_flag[j];

	/* Limpiar pantalla */
	screen_save();
	clear_from(0);

	/* Interactuar */
	while (1) {
		/* Mensaje */
		prt("Banderas de ventana (<dir> para mover, 't'/Enter para alternar, o ESC)", 0, 0);

		/* Mostrar las ventanas */
		for (j = 0; j < ANGBAND_TERM_MAX; j++) {
			uint8_t a = COLOUR_WHITE;

			const char *s = angband_term_name[j];

			/* Usar color */
			if (j == x) a = COLOUR_L_BLUE;

			/* Nombre de ventana, escalonado, centrado */
			Term_putstr(35 + j * 5 - strlen(s) / 2, 2 + j % 2, -1, a, s);
		}

		/* Mostrar las opciones */
		for (i = 0; i < PW_MAX_FLAGS; i++) {
			uint8_t a = COLOUR_WHITE;

			const char *str = window_flag_desc[i];

			/* Usar color */
			if (i == y) a = COLOUR_L_BLUE;

			/* Opción no usada */
			if (!str) str = "(Opción no usada)";

			/* Nombre de la bandera */
			Term_putstr(0, i + 5, -1, a, str);

			/* Mostrar las ventanas */
			for (j = 0; j < ANGBAND_TERM_MAX; j++) {
				wchar_t c = L'.';

				a = COLOUR_WHITE;

				/* Usar color */
				if ((i == y) && (j == x)) a = COLOUR_L_BLUE;

				/* Bandera activa */
				if (new_flags[j] & ((uint32_t) 1 << i)) c = L'X';

				/* Valor de la bandera */
				Term_putch(35 + j * 5, i + 5, a, c);
			}
		}

		/* Colocar cursor */
		Term_gotoxy(35 + x * 5, y + 5);

		/* Obtener tecla */
		ke = inkey_ex();

		/* Interacción con ratón o teclado */
		if (ke.type == EVT_MOUSE) {
			int choicey = ke.mouse.y - 5;
			int choicex = (ke.mouse.x - 35)/5;

			if (ke.mouse.button == 2)
				break;

			if ((choicey >= 0) && (choicey < PW_MAX_FLAGS)
				&& (choicex > 0) && (choicex < ANGBAND_TERM_MAX)
				&& !(ke.mouse.x % 5)) {
				if ((choicey == y) && (choicex == x)) {
					uint32_t flag = ((uint32_t) 1) << y;

					/* Alternar bandera (desactivar) */
					if (new_flags[x] & flag)
						new_flags[x] &= ~flag;
					/* Alternar bandera (activar) */
					else
						new_flags[x] |= flag;
				} else {
					y = choicey;
					x = (ke.mouse.x - 35)/5;
				}
			}
		} else if (ke.type == EVT_KBRD) {
			if (ke.key.code == ESCAPE || ke.key.code == 'q')
				break;

			/* Alternar */
			else if (ke.key.code == '5' || ke.key.code == 't' ||
					ke.key.code == KC_ENTER) {
				/* Ignorar la ventana principal */
				if (x == 0)
					bell();

				/* Alternar bandera (desactivar) */
				else if (new_flags[x] & (((uint32_t) 1) << y))
					new_flags[x] &= ~(((uint32_t) 1) << y);

				/* Alternar bandera (activar) */
				else
					new_flags[x] |= (((uint32_t) 1) << y);

				/* Continuar */
				continue;
			}

			/* Extraer dirección */
			d = target_dir(ke.key);

			/* Mover */
			if (d != 0) {
				x = (x + ddx[d] + 8) % ANGBAND_TERM_MAX;
				y = (y + ddy[d] + 16) % PW_MAX_FLAGS;
			}
		}
	}

	/* Notar cambios */
	subwindows_set_flags(new_flags, ANGBAND_TERM_MAX);

	screen_load();
}



/**
 * ------------------------------------------------------------------------
 * Interactuar con mapas de teclas
 * ------------------------------------------------------------------------ */

/**
 * Acción de mapa de teclas actual (o reciente)
 */
static struct keypress keymap_buffer[KEYMAP_ACTION_MAX + 1];


/**
 * Preguntar y mostrar un desencadenante de mapa de teclas.
 *
 * Devuelve la entrada desencadenante.
 *
 * Nótese que ambas llamadas a "event_signal(EVENT_INPUT_FLUSH)" son extremadamente
 * importantes. Esto puede
 * ya no ser cierto, ya que "util.c" es ahora mucho más simple. XXX XXX XXX
 */
static struct keypress keymap_get_trigger(void)
{
	char tmp[80];
	struct keypress buf[2] = { KEYPRESS_NULL, KEYPRESS_NULL };

	/* Vaciar */
	event_signal(EVENT_INPUT_FLUSH);

	/* Obtener una tecla */
	buf[0] = inkey();

	/* Convertir a ascii */
	keypress_to_text(tmp, sizeof(tmp), buf, false);

	/* Mostrar el desencadenante */
	Term_addstr(-1, COLOUR_WHITE, tmp);

	/* Vaciar */
	event_signal(EVENT_INPUT_FLUSH);

	/* Devolver desencadenante */
	return buf[0];
}


/**
 * Funciones de acción del menú de mapas de teclas
 */

static void ui_keymap_pref_load(const char *title, int row)
{
	do_cmd_pref_file_hack(16);
}

static void ui_keymap_pref_append(const char *title, int row)
{
	dump_pref_file(keymap_dump, "Guardar mapas de teclas", 13);
}

static void ui_keymap_query(const char *title, int row)
{
	char tmp[1024];
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;
	struct keypress c;
	const struct keypress *act;

	prt(title, 13, 0);
	prt("Tecla: ", 14, 0);
	
	/* Obtener un desencadenante y mapeo de mapa de teclas */
	c = keymap_get_trigger();
	act = keymap_find(mode, c);
	
	/* ¿Se encontró el mapa de teclas? */
	if (!act) {
		/* Mensaje */
		prt("Ningún mapa de teclas con ese desencadenante. Pulsa cualquier tecla para continuar.", 16, 0);
		inkey();
	} else {
		/* Analizar la acción actual */
		keypress_to_text(tmp, sizeof(tmp), act, false);
	
		/* Mostrar la acción actual */
		prt("Encontrado: ", 15, 0);
		Term_addstr(-1, COLOUR_WHITE, tmp);

		prt("Pulsa cualquier tecla para continuar.", 17, 0);
		inkey();
	}
}

static void ui_keymap_create(const char *title, int row)
{
	bool done = false;
	size_t n = 0;

	struct keypress c;
	char tmp[1024];
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	prt(title, 13, 0);
	prt("Tecla: ", 14, 0);

	c = keymap_get_trigger();
	if (c.code == '=') {
		c_prt(COLOUR_L_RED, "La tecla '=' está reservada.", 16, 2);
		prt("Pulsa cualquier tecla para continuar.", 18, 0);
		inkey();
		return;
	}

	/* Obtener una acción codificada, con una respuesta por defecto */
	while (!done) {
		struct keypress kp = {EVT_NONE, 0, 0};

		int color = COLOUR_WHITE;
		if (n == 0) color = COLOUR_YELLOW;
		if (n == KEYMAP_ACTION_MAX) color = COLOUR_L_RED;

		keypress_to_text(tmp, sizeof(tmp), keymap_buffer, false);
		c_prt(color, format("Acción: %s", tmp), 15, 0);

		c_prt(COLOUR_L_BLUE, "  Pulsa '=' cuando termines.", 17, 0);
		c_prt(COLOUR_L_BLUE, "  Usa 'CTRL-u' para reiniciar.", 18, 0);
		c_prt(COLOUR_L_BLUE, format("(La longitud máxima del mapa de teclas es de %d teclas.)",
									KEYMAP_ACTION_MAX), 19, 0);

		kp = inkey();

		if (kp.code == '=') {
			done = true;
			continue;
		}

		switch (kp.code) {
			case KC_DELETE:
			case KC_BACKSPACE: {
				if (n > 0) {
					n -= 1;
				    keymap_buffer[n].type = 0;
					keymap_buffer[n].code = 0;
					keymap_buffer[n].mods = 0;
				}
				break;
			}

			case KTRL('U'): {
				memset(keymap_buffer, 0, sizeof keymap_buffer);
				n = 0;
				break;
			}

			default: {
				if (n == KEYMAP_ACTION_MAX) continue;

				if (n == 0) {
					memset(keymap_buffer, 0, sizeof keymap_buffer);
				}
				keymap_buffer[n++] = kp;
				break;
			}
		}
	}

	if (c.code && get_check("¿Conservar este mapa de teclas? ")) {
		keymap_add(mode, c, keymap_buffer, true);
		prt("Para usarlo en otras sesiones, guarda los mapas de teclas en un archivo. Pulsa una tecla para continuar.", 17, 0);
		inkey();
	}
}

static void ui_keymap_remove(const char *title, int row)
{
	struct keypress c;
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	prt(title, 13, 0);
	prt("Tecla: ", 14, 0);

	c = keymap_get_trigger();

	if (keymap_remove(mode, c))
		prt("Eliminado.", 16, 0);
	else
		prt("¡No hay mapa de teclas que eliminar!", 16, 0);

	/* Mensaje */
	prt("Pulsa cualquier tecla para continuar.", 17, 0);
	inkey();
}

static void keymap_browse_hook(int oid, void *db, const region *loc)
{
	char tmp[1024];

	event_signal(EVENT_MESSAGE_FLUSH);

	clear_from(13);

	/* Mostrar acción actual */
	prt("Acción actual (si la hay) mostrada abajo:", 13, 0);
	keypress_to_text(tmp, sizeof(tmp), keymap_buffer, false);
	prt(tmp, 14, 0);
}

static struct menu *keymap_menu;
static menu_action keymap_actions[] =
{
	{ 0, 0, "Cargar un archivo de preferencias de usuario",    ui_keymap_pref_load },
	{ 0, 0, "Guardar mapas de teclas en archivo",     ui_keymap_pref_append },
	{ 0, 0, "Consultar un mapa de teclas",           ui_keymap_query },
	{ 0, 0, "Crear un mapa de teclas",          ui_keymap_create },
	{ 0, 0, "Eliminar un mapa de teclas",          ui_keymap_remove },
};

static void do_cmd_keymaps(const char *title, int row)
{
	region loc = {0, 0, 0, 12};

	screen_save();
	clear_from(0);

	if (!keymap_menu) {
		keymap_menu = menu_new_action(keymap_actions,
				N_ELEMENTS(keymap_actions));
	
		keymap_menu->title = title;
		keymap_menu->selections = lower_case;
		keymap_menu->browse_hook = keymap_browse_hook;
	}

	menu_layout(keymap_menu, &loc);
	menu_select(keymap_menu, 0, false);

	screen_load();
}



/**
 * ------------------------------------------------------------------------
 * Interactuar con los visuales
 * ------------------------------------------------------------------------ */

static void visuals_pref_load(const char *title, int row)
{
	do_cmd_pref_file_hack(15);
}

static void visuals_dump_monsters(const char *title, int row)
{
	dump_pref_file(dump_monsters, title, 15);
}

static void visuals_dump_objects(const char *title, int row)
{
	dump_pref_file(dump_objects, title, 15);
}

static void visuals_dump_features(const char *title, int row)
{
	dump_pref_file(dump_features, title, 15);
}

static void visuals_dump_flavors(const char *title, int row)
{
	dump_pref_file(dump_flavors, title, 15);
}

static void visuals_reset(const char *title, int row)
{
	/* Reiniciar */
	reset_visuals(true);

	/* Mensaje */
	prt("", 0, 0);
	msg("Tablas de atributos/caracteres visuales reiniciadas.");
	event_signal(EVENT_MESSAGE_FLUSH);
}


static struct menu *visual_menu;
static menu_action visual_menu_items [] =
{
	{ 0, 0, "Cargar un archivo de preferencias de usuario",   visuals_pref_load },
	{ 0, 0, "Guardar atributos/caracteres de monstruos", visuals_dump_monsters },
	{ 0, 0, "Guardar atributos/caracteres de objetos",  visuals_dump_objects },
	{ 0, 0, "Guardar atributos/caracteres de características", visuals_dump_features },
	{ 0, 0, "Guardar atributos/caracteres de sabores",  visuals_dump_flavors },
	{ 0, 0, "Reiniciar visuales",           visuals_reset },
};


static void visuals_browse_hook(int oid, void *db, const region *loc)
{
	event_signal(EVENT_MESSAGE_FLUSH);
	clear_from(1);
}


/**
 * Interactuar con "visuales"
 */
static void do_cmd_visuals(const char *title, int row)
{
	screen_save();
	clear_from(0);

	if (!visual_menu)
	{
		visual_menu = menu_new_action(visual_menu_items,
				N_ELEMENTS(visual_menu_items));

		visual_menu->title = title;
		visual_menu->selections = lower_case;
		visual_menu->browse_hook = visuals_browse_hook;
		visual_menu->header = "Para editar visuales, usa el menú de conocimiento";
	}

	menu_layout(visual_menu, &SCREEN_REGION);
	menu_select(visual_menu, 0, false);

	screen_load();
}


/**
 * ------------------------------------------------------------------------
 * Interactuar con los colores
 * ------------------------------------------------------------------------ */

static void colors_pref_load(const char *title, int row)
{
	/* Preguntar por y cargar un archivo de preferencias de usuario */
	do_cmd_pref_file_hack(8);
	
	/* XXX debería haber una forma más limpia de informar a la UI sobre
	 * cambios de color - ¿qué tal hacer esto también en el código de carga
	 * de archivos de preferencias? */
	Term_xtra(TERM_XTRA_REACT, 0);
	Term_redraw_all();
}

static void colors_pref_dump(const char *title, int row)
{
	dump_pref_file(dump_colors, title, 15);
}

static void colors_modify(const char *title, int row)
{
	int i;

	static uint8_t a = 0;

	/* Mensaje */
	prt("Comando: Modificar colores", 8, 0);

	/* Preguntar hasta terminar */
	while (1) {
		const char *name;
		char index;

		struct keypress cx;

		/* Limpiar */
		clear_from(10);

		/* Exhibir los colores normales */
		for (i = 0; i < BASIC_COLORS; i++) {
			/* Exhibir este color */
			Term_putstr(i*3, 20, -1, a, "##");

			/* Exhibir letra del carácter */
			Term_putstr(i*3, 21, -1, (uint8_t)i,
						format(" %c", color_table[i].index_char));

			/* Exhibir todos los colores */
			Term_putstr(i*3, 22, -1, (uint8_t)i, format("%2d", i));
		}

		/* Describir el color */
		name = ((a < BASIC_COLORS) ? color_table[a].name : "indefinido");
		index = ((a < BASIC_COLORS) ? color_table[a].index_char : '?');

		/* Describir el color */
		Term_putstr(5, 10, -1, COLOUR_WHITE,
					format("Color = %d, Nombre = %s, Índice = %c",
						   a, name, index));

		/* Etiquetar los valores actuales */
		Term_putstr(5, 12, -1, COLOUR_WHITE,
				format("K = 0x%02x / R,V,A = 0x%02x,0x%02x,0x%02x",
				   angband_color_table[a][0],
				   angband_color_table[a][1],
				   angband_color_table[a][2],
				   angband_color_table[a][3]));

		/* Mensaje */
		Term_putstr(0, 14, -1, COLOUR_WHITE,
				"Comando (n/N/k/K/r/R/v/V/a/A): ");

		/* Obtener un comando */
		cx = inkey();

		/* Todo terminado */
		if (cx.code == ESCAPE) break;

		/* Analizar */
		if (cx.code == 'n') {
			a = (uint8_t)(a + 1);
			if (a >= MAX_COLORS) {
				a = 0;
			}
		}
		if (cx.code == 'N') {
			a = (uint8_t)(a - 1);
			if (a >= MAX_COLORS) {
				a = MAX_COLORS - 1;
			}
		}
		if (cx.code == 'k')
			angband_color_table[a][0] =
				(uint8_t)(angband_color_table[a][0] + 1);
		if (cx.code == 'K')
			angband_color_table[a][0] =
				(uint8_t)(angband_color_table[a][0] - 1);
		if (cx.code == 'r')
			angband_color_table[a][1] =
				(uint8_t)(angband_color_table[a][1] + 1);
		if (cx.code == 'R')
			angband_color_table[a][1] =
				(uint8_t)(angband_color_table[a][1] - 1);
		if (cx.code == 'v')
			angband_color_table[a][2] =
				(uint8_t)(angband_color_table[a][2] + 1);
		if (cx.code == 'V')
			angband_color_table[a][2] =
				(uint8_t)(angband_color_table[a][2] - 1);
		if (cx.code == 'a')
			angband_color_table[a][3] =
				(uint8_t)(angband_color_table[a][3] + 1);
		if (cx.code == 'A')
			angband_color_table[a][3] =
				(uint8_t)(angband_color_table[a][3] - 1);

		/* Reaccionar a los cambios */
		Term_xtra(TERM_XTRA_REACT, 0);

		/* Redibujar */
		Term_redraw();
	}
}

static void colors_browse_hook(int oid, void *db, const region *loc)
{
	event_signal(EVENT_MESSAGE_FLUSH);
	clear_from(1);
}


static struct menu *color_menu;
static menu_action color_events [] =
{
	{ 0, 0, "Cargar un archivo de preferencias de usuario", colors_pref_load },
	{ 0, 0, "Guardar colores",           colors_pref_dump },
	{ 0, 0, "Modificar colores",         colors_modify }
};

/**
 * Interactuar con "colores"
 */
static void do_cmd_colors(const char *title, int row)
{
	screen_save();
	clear_from(0);

	if (!color_menu)
	{
		color_menu = menu_new_action(color_events,
			N_ELEMENTS(color_events));

		color_menu->title = title;
		color_menu->selections = lower_case;
		color_menu->browse_hook = colors_browse_hook;
	}

	menu_layout(color_menu, &SCREEN_REGION);
	menu_select(color_menu, 0, false);

	screen_load();
}


/**
 * ------------------------------------------------------------------------
 * Acciones de menú no complejas
 * ------------------------------------------------------------------------ */

static bool askfor_aux_numbers(char *buf, size_t buflen, size_t *curs, size_t *len, struct keypress keypress, bool firsttime)
{
	switch (keypress.code)
	{
		case ESCAPE:
		case KC_ENTER:
		case ARROW_LEFT:
		case ARROW_RIGHT:
		case KC_DELETE:
		case KC_BACKSPACE:
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return askfor_aux_keypress(buf, buflen, curs, len, keypress,
									   firsttime);
	}

	return false;
}


/**
 * Establecer factor de demora base
 */
static void do_cmd_delay(const char *name, int unused)
{
	char tmp[4] = "";
	int msec = player->opts.delay_factor;

	strnfmt(tmp, sizeof(tmp), "%i", player->opts.delay_factor);

	screen_save();

	/* Mensaje */
	prt("", 19, 0);
	prt("Comando: Factor de Demora Base", 20, 0);
	prt("Nuevo factor de demora base (0-255): ", 21, 0);
	prt(format("Factor de demora base actual: %d ms", msec), 22, 0);
	prt("", 23, 0);

	/* Preguntar por un valor numérico */
	if (askfor_aux(tmp, sizeof(tmp), askfor_aux_numbers)) {
		uint16_t val = (uint16_t) strtoul(tmp, NULL, 0);
		player->opts.delay_factor = MIN(val, 255);
	}

	screen_load();
}

/**
 * Establecer modo de barra lateral
 */
static void do_cmd_sidebar_mode(const char *name, int unused)
{
	char tmp[20] = "";	
	const char *names[SIDEBAR_MAX] = {"Izquierda", "Arriba", "Ninguna"};
	struct keypress cx = KEYPRESS_NULL;

	screen_save();

	while (true) {	

		// Obtener el nombre
		my_strcpy(tmp, names[SIDEBAR_MODE % SIDEBAR_MAX], sizeof(tmp));

		/* Mensaje */
		prt("", 19, 0);
		prt("Comando: Modo de Barra Lateral", 20, 0);
		prt(format("Modo actual: %s", tmp), 21, 0);
		prt("ESC: volver, otra tecla: cambiar", 22, 0);
		prt("", 23, 0);

		/* Obtener un comando */
		cx = inkey();

		/* Todo terminado */
		if (cx.code == ESCAPE) break;

		// Cambiar
		SIDEBAR_MODE = (SIDEBAR_MODE + 1) % SIDEBAR_MAX;
	}

	screen_load();
}


/**
 * Establecer nivel de advertencia de puntos de golpe
 */
static void do_cmd_hp_warn(const char *name, int unused)
{
	bool res;
	char tmp[4] = "";
	uint8_t warn;

	strnfmt(tmp, sizeof(tmp), "%i", player->opts.hitpoint_warn);

	screen_save();

	/* Mensaje */
	prt("", 19, 0);
	prt("Comando: Advertencia de Puntos de Golpe", 20, 0);
	prt("Nueva advertencia de puntos de golpe (0-9): ", 21, 0);
	prt(format("Advertencia de puntos de golpe actual: %d (%d%%)",
		player->opts.hitpoint_warn, player->opts.hitpoint_warn * 10),
		22, 0);
	prt("", 23, 0);

	/* Preguntar al usuario por una cadena */
	res = askfor_aux(tmp, sizeof(tmp), askfor_aux_numbers);

	/* Procesar entrada */
	if (res) {
		warn = (uint8_t) strtoul(tmp, NULL, 0);
		
		/* Reiniciar advertencias sin sentido */
		if (warn > 9)
			warn = 0;

		player->opts.hitpoint_warn = warn;
	}

	screen_load();
}


/**
 * Establecer demora de "movimiento perezoso"
 */
static void do_cmd_lazymove_delay(const char *name, int unused)
{
	bool res;
	char tmp[4] = "";

	strnfmt(tmp, sizeof(tmp), "%i", player->opts.lazymove_delay);

	screen_save();

	/* Mensaje */
	prt("", 19, 0);
	prt("Comando: Factor de Demora de Movimiento", 20, 0);
	prt("Nueva demora de movimiento: ", 21, 0);
	prt(format("Demora de movimiento actual: %d (%d ms)",
		player->opts.lazymove_delay, player->opts.lazymove_delay * 10),
		22, 0);
	prt("", 23, 0);

	/* Preguntar al usuario por una cadena */
	res = askfor_aux(tmp, sizeof(tmp), askfor_aux_numbers);

	/* Procesar entrada */
	if (res) {
		unsigned long delay = strtoul(tmp, NULL, 0);
		player->opts.lazymove_delay = (uint8_t) MIN(delay, 255);
	}

	screen_load();
}



/**
 * Preguntar por un "archivo de preferencias de usuario" y procesarlo.
 *
 * Esta función solo debe ser usada por comandos de interacción estándar,
 * en los que un mensaje estándar "Comando:" está presente en la fila dada.
 *
 * ¿Permitir nombres de archivo absolutos?  XXX XXX XXX
 */
static void do_cmd_pref_file_hack(long row)
{
	char ftmp[80];
	bool ok;

	screen_save();

	/* Mensaje */
	if (row > 0) {
		prt("", row - 1, 0);
	}
	prt("Comando: Cargar un archivo de preferencias de usuario", row, 0);
	prt("", row + 1, 0);
	prt("Archivo: ", row + 2, 0);
	prt("", row + 3, 0);

	/* Obtener el nombre seguro para el sistema de archivos y añadir .prf */
	player_safe_name(ftmp, sizeof(ftmp), player->full_name, true);
	my_strcat(ftmp, ".prf", sizeof(ftmp));

	if(!arg_force_name)
		ok = askfor_aux(ftmp, sizeof ftmp, NULL);
	else
		ok = get_check(format("¿Confirmar carga de %s? ", ftmp));
	
	/* Preguntar por un archivo (o cancelar) */
	if(ok) {
		/* Procesar el nombre de archivo dado */
		if (process_pref_file(ftmp, false, true) == false) {
			/* Mencionar fallo */
			prt("", 0, 0);
			msg("¡Fallo al cargar '%s'!", ftmp);
		} else {
			/* Mencionar éxito */
			prt("", 0, 0);
			msg("Cargado '%s'.", ftmp);
		}
	}

	screen_load();
}

 
 
/**
 * Escribir opciones en un archivo.
 */
static void do_dump_options(const char *title, int row) {
	dump_pref_file(option_dump, "Guardar configuración de ventanas", 20);
}

/**
 * Escribir autoinscripciones en un archivo.
 */
static void do_dump_autoinsc(const char *title, int row) {
	dump_pref_file(dump_autoinscriptions, "Guardar autoinscripciones", 20);
}

/**
 * Escribir personalizaciones de la pantalla de personaje en un archivo.
 */
static void do_dump_charscreen_opt(const char *title, int row) {
	dump_pref_file(dump_ui_entry_renderers, "Guardar opciones de pantalla de personaje", 20);
}

/**
 * Cargar un archivo de preferencias.
 */
static void options_load_pref_file(const char *n, int row)
{
	do_cmd_pref_file_hack(20);
}



/**
 * ------------------------------------------------------------------------
 * Menú de ignorar objetos de égida
 * ------------------------------------------------------------------------ */

#define EGO_MENU_HELPTEXT \
"{light green}Teclas de movimiento{/} desplazan la lista\n{light red}ESC{/} vuelve al menú anterior\n{light blue}Enter{/} alterna la configuración actual."

/**
 * Omitir prefijos comunes en nombres de objetos de égida.
 */
static const char *strip_ego_name(const char *name)
{
	if (prefix(name, "of the "))
		return name + 7;
	if (prefix(name, "of "))
		return name + 3;
	return name;
}


/**
 * Mostrar un tipo de objeto de égida en la pantalla.
 */
int ego_item_name(char *buf, size_t buf_size, struct ego_desc *desc)
{
	size_t i;
	int end;
	size_t prefix_size;
	const char *long_name;

	struct ego_item *ego = &e_info[desc->e_idx];

	/* Encontrar el tipo de ignorar */
	for (i = 0; i < N_ELEMENTS(quality_choices); i++)
		if (desc->itype == i) break;

	if (i == N_ELEMENTS(quality_choices)) return 0;

	/* Inicializar el búfer */
	end = my_strcat(buf, "[ ] ", buf_size);

	/* Añadir el nombre */
	end += my_strcat(buf, quality_choices[i].name, buf_size);

	/* Añadir un espacio extra */
	end += my_strcat(buf, " ", buf_size);

	/* Obtener el nombre completo del objeto de égida */
	long_name = ego->name;

	/* Obtener la longitud del prefijo común, si lo hay */
	prefix_size = (desc->short_name - long_name);

	/* ¿Se encontró un prefijo? */
	if (prefix_size > 0) {
		char prefix[100];

		/* Obtener una copia del prefijo */
		my_strcpy(prefix, long_name, prefix_size + 1);

		/* Añadir el prefijo */
		end += my_strcat(buf, prefix, buf_size);
	}
	/* Establecer el nombre a la longitud correcta */
	return end;
}

/**
 * Función de utilidad usada para ordenar una matriz de índices de objetos de égida por
 * nombre de objeto de égida.
 */
static int ego_comp_func(const void *a_ptr, const void *b_ptr)
{
	const struct ego_desc *a = a_ptr;
	const struct ego_desc *b = b_ptr;

	/* Nota la eliminación de prefijos comunes */
	return (strcmp(a->short_name, b->short_name));
}

/**
 * Mostrar una entrada en el menú sval
 */
static void ego_display(struct menu * menu, int oid, bool cursor, int row,
						int col, int width)
{
	char buf[80] = "";
	struct ego_desc *choice = (struct ego_desc *) menu->menu_data;
	bool ignored = ego_is_ignored(choice[oid].e_idx, choice[oid].itype);

	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
	uint8_t sq_attr = (ignored ? COLOUR_L_RED : COLOUR_L_GREEN);

	/* Adquirir el "nombre" del objeto "i" */
	(void) ego_item_name(buf, sizeof(buf), &choice[oid]);

	/* Imprimirlo */
	c_put_str(attr, format("%s", buf), row, col);

	/* Mostrar marca de ignorar, si la hay */
	if (ignored)
		c_put_str(COLOUR_L_RED, "*", row, col + 1);

	/* Mostrar el nombre del objeto de égida sin prefijo usando otro color */
	c_put_str(sq_attr, choice[oid].short_name, row, col + strlen(buf));
}

/**
 * Manejar eventos en el menú sval
 */
static bool ego_action(struct menu * menu, const ui_event * event, int oid)
{
	struct ego_desc *choice = menu->menu_data;

	/* Alternar */
	if (event->type == EVT_SELECT) {
		ego_ignore_toggle(choice[oid].e_idx, choice[oid].itype);

		return true;
	}

	return false;
}

/**
 * Mostrar lista de objetos de égida a ignorar.
 */
static void ego_menu(const char *unused, int also_unused)
{
	int max_num = 0;
	struct ego_item *ego;
	struct ego_desc *choice;

	struct menu menu;
	menu_iter menu_f = { 0, 0, ego_display, ego_action, 0 };
	region area = { 1, 5, -1, -1 };
	int cursor = 0;

	int i;

	/* Crear la matriz */
	choice = mem_zalloc(z_info->e_max * ITYPE_MAX * sizeof(struct ego_desc));

	/* Obtener los objetos de égida válidos */
	for (i = 0; i < z_info->e_max; i++) {
		int itype;
		ego = &e_info[i];

		/* Solo se permiten objetos de égida conocidos válidos */
		if (!ego->name || !ego->everseen)
			continue;

		/* Encontrar tipos de ignorar apropiados */
		for (itype = ITYPE_NONE + 1; itype < ITYPE_MAX; itype++)
			if (ego_has_ignore_type(ego, itype)) {

				/* Rellenar los detalles */
				choice[max_num].e_idx = i;
				choice[max_num].itype = itype;
				choice[max_num].short_name = strip_ego_name(ego->name);

				++max_num;
			}
	}

	/* Ordenar rápidamente la matriz por nombre de objeto de égida */
	qsort(choice, max_num, sizeof(choice[0]), ego_comp_func);

	/* Regresar aquí si no hay objetos */
	if (!max_num) {
		mem_free(choice);
		return;
	}


	/* Guardar la pantalla y limpiarla */
	screen_save();
	clear_from(0);

	/* Texto de ayuda */
	prt("Menú de ignorar objetos de égida", 0, 0);

	/* Salida a la pantalla */
	text_out_hook = text_out_to_screen;

	/* Sangrar salida */
	text_out_indent = 1;
	text_out_wrap = 79;
	Term_gotoxy(1, 1);

	/* Mostrar información útil */
	text_out_e(EGO_MENU_HELPTEXT);

	text_out_indent = 0;

	/* Configurar el menú */
	memset(&menu, 0, sizeof(menu));
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu_setpriv(&menu, max_num, choice);
	menu_layout(&menu, &area);

	/* Seleccionar una entrada */
	(void) menu_select(&menu, cursor, false);

	/* Liberar memoria */
	mem_free(choice);

	/* Cargar pantalla */
	screen_load();

	return;
}


/**
 * ------------------------------------------------------------------------
 * Menú de ignorar por calidad
 * ------------------------------------------------------------------------ */

/**
 * Estructura de menú para diferenciar ignorar por consciente de no consciente
 */
typedef struct
{
	struct object_kind *kind;
	bool aware;
} ignore_choice;

/**
 * Función de ordenamiento para opciones de ignorar.
 * Consciente va antes de no consciente, y luego ordenar alfabéticamente.
 */
static int cmp_ignore(const void *a, const void *b)
{
	char bufa[80];
	char bufb[80];
	const ignore_choice *x = a;
	const ignore_choice *y = b;

	if (!x->aware && y->aware)
		return 1;
	if (x->aware && !y->aware)
		return -1;

	object_kind_name(bufa, sizeof(bufa), x->kind, x->aware);
	object_kind_name(bufb, sizeof(bufb), y->kind, y->aware);

	return strcmp(bufa, bufb);
}

/**
 * Determinar si un objeto es una opción válida
 */
static int quality_validity(struct menu *menu, int oid)
{
	return oid ? 1 : 0;
}

/**
 * Mostrar una entrada en el menú.
 */
static void quality_display(struct menu *menu, int oid, bool cursor, int row,
							int col, int width)
{
	if (oid) {
		/* Nota: el orden de los valores en quality_choices no se
			alinea con el orden de la enumeración ignore_type_t. ¿Arreglar? NRM*/
		const char *name = quality_choices[oid].name;
		uint8_t level = ignore_level[oid];
		const char *level_name = quality_values[level].name;
		uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);
		size_t u8len = utf8_strlen(name);

		if (u8len < 30) {
			c_put_str(attr, format("%s%*s", name, (int)(30 - u8len),
				" "), row, col);
		} else {
			char *name_copy = string_make(name);

			if (u8len > 30) {
				utf8_clipto(name_copy, 30);
			}
			c_put_str(attr, name_copy, row, col);
			string_free(name_copy);
		}
		c_put_str(attr, format(" : %s", level_name), row, col + 30);
	}
}


/**
 * Mostrar los subtipos de ignorar por calidad.
 */
static void quality_subdisplay(struct menu *menu, int oid, bool cursor, int row,
							   int col, int width)
{
	const char *name = quality_values[oid].name;
	uint8_t attr = (cursor ? COLOUR_L_BLUE : COLOUR_WHITE);

	c_put_str(attr, name, row, col);
}


/**
 * Manejar pulsaciones de tecla.
 */
static bool quality_action(struct menu *m, const ui_event *event, int oid)
{
	struct menu menu;
	menu_iter menu_f = { NULL, NULL, quality_subdisplay, NULL, NULL };
	region area = { 37, 2, 29, IGNORE_MAX };
	ui_event evt;
	int count;

	/* Mostrar en el punto correcto */
	area.row += oid;

	/* Guardar */
	screen_save();

	/* Calcular cuántas opciones tenemos */
	count = IGNORE_MAX;
	if ((oid == ITYPE_RING) || (oid == ITYPE_AMULET))
		count = area.page_rows = IGNORE_BAD + 1;

	/* Ejecutar menú */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu_setpriv(&menu, count, quality_values);

	/* Evitar que los menús se salgan de la parte inferior de la pantalla */
	if (area.row + menu.count > Term->hgt - 1)
		area.row += Term->hgt - 1 - area.row - menu.count;

	menu_layout(&menu, &area);

	window_make(area.col - 2, area.row - 1, area.col + area.width + 2,
				area.row + area.page_rows);

	evt = menu_select(&menu, 0, true);

	/* Establecer el nuevo valor apropiadamente */
	if (evt.type == EVT_SELECT)
		ignore_level[oid] = menu.cursor;

	/* Cargar y terminar */
	screen_load();
	return true;
}

/**
 * Mostrar menú de ignorar por calidad.
 */
static void quality_menu(const char *unused, int also_unused)
{
	struct menu menu;
	menu_iter menu_f = { NULL, quality_validity, quality_display,
						 quality_action, NULL };
	region area = { 0, 0, 0, 0 };

	/* Guardar pantalla */
	screen_save();
	clear_from(0);

	/* Configurar el menú */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.title = "Menú de ignorar por calidad";
	menu_setpriv(&menu, ITYPE_MAX, quality_values);
	menu_layout(&menu, &area);

	/* Seleccionar una entrada */
	menu_select(&menu, 0, false);

	/* Cargar pantalla */
	screen_load();
	return;
}



/**
 * ------------------------------------------------------------------------
 * Menú de ignorar por sval
 * ------------------------------------------------------------------------ */

/**
 * Estructura para describir pares tval/descripción.
 */
typedef struct
{
	int tval;
	const char *desc;
} tval_desc;

/**
 * Categorías para ignorar dependiente de sval.
 */
static tval_desc sval_dependent[] =
{
	{ TV_STAFF,			"Báculos" },
	{ TV_WAND,			"Varitas" },
	{ TV_ROD,			"Varas" },
	{ TV_SCROLL,		"Pergaminos" },
	{ TV_POTION,		"Pociones" },
	{ TV_RING,			"Anillos" },
	{ TV_AMULET,		"Amuletos" },
	{ TV_FOOD,			"Comida" },
	{ TV_MUSHROOM,		"Setas" },
	{ TV_MAGIC_BOOK,	"Libros de magia" },
	{ TV_PRAYER_BOOK,	"Libros de plegarias" },
	{ TV_NATURE_BOOK,	"Libros de naturaleza" },
	{ TV_SHADOW_BOOK,	"Libros de sombras" },
	{ TV_OTHER_BOOK,	"Libros de misterio" },
	{ TV_LIGHT,			"Luces" },
	{ TV_FLASK,			"Frascos de aceite" },
	{ TV_GOLD,			"Dinero" },
};


/**
 * Determina si un tval es elegible para ignorar por sval.
 */
bool ignore_tval(int tval)
{
	size_t i;

	/* Solo ignorar si el tval está permitido */
	for (i = 0; i < N_ELEMENTS(sval_dependent); i++) {
		if (kb_info[tval].num_svals == 0) continue;
		if (tval == sval_dependent[i].tval)
			return true;
	}

	return false;
}


/**
 * Mostrar una entrada en el menú sval
 */
static void ignore_sval_menu_display(struct menu *menu, int oid, bool cursor,
									 int row, int col, int width)
{
	char buf[80];
	const ignore_choice *choice = menu_priv(menu);

	struct object_kind *kind = choice[oid].kind;
	bool aware = choice[oid].aware;

	uint8_t attr = curs_attrs[(int)aware][0 != cursor];

	/* Adquirir el "nombre" del objeto "i" */
	object_kind_name(buf, sizeof(buf), kind, aware);

	/* Imprimirlo */
	c_put_str(attr, format("[ ] %s", buf), row, col);
	if ((aware && (kind->ignore & IGNORE_IF_AWARE)) ||
			(!aware && (kind->ignore & IGNORE_IF_UNAWARE)))
		c_put_str(COLOUR_L_RED, "*", row, col + 1);
}


/**
 * Manejar eventos en el menú sval
 */
static bool ignore_sval_menu_action(struct menu *m, const ui_event *event,
									int oid)
{
	const ignore_choice *choice = menu_priv(m);

	if (event->type == EVT_SELECT ||
			(event->type == EVT_KBRD && tolower(event->key.code) == 't')) {
		struct object_kind *kind = choice[oid].kind;

		/* Alternar la bandera apropiada */
		if (choice[oid].aware)
			kind->ignore ^= IGNORE_IF_AWARE;
		else
			kind->ignore ^= IGNORE_IF_UNAWARE;

		player->upkeep->notice |= PN_IGNORE;
		return true;
	}

	return false;
}

static const menu_iter ignore_sval_menu =
{
	NULL,
	NULL,
	ignore_sval_menu_display,
	ignore_sval_menu_action,
	NULL,
};


/**
 * Recopilar todos los tval en la gran matriz ignore_choice
 */
static int ignore_collect_kind(int tval, ignore_choice **ch)
{
	ignore_choice *choice;
	int num = 0;

	int i;

	/* Crear la matriz, con entradas tanto para ignorar consciente como no consciente */
	choice = mem_alloc(2 * z_info->k_max * sizeof *choice);

	for (i = 1; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Saltar objetos vacíos, objetos no vistos y tvals incorrectos */
		if (!kind->name || kind->tval != tval)
			continue;

		if (!kind->aware) {
			/* se puede ignorar cualquier cosa no consciente */
			choice[num].kind = kind;
			choice[num++].aware = false;
		}

		if ((kind->everseen && !kf_has(kind->kind_flags, KF_INSTA_ART)) || 
			tval_is_money_k(kind)) {
			/* No mostrar los tipos base de artefacto en esta lista
			 * ignorar consciente requiere everseen
			 * no requerir consciencia para ignorar consciente, para que la gente pueda establecer
			 * al inicio del juego */
			choice[num].kind = kind;
			choice[num++].aware = true;
		}
	}

	if (num == 0)
		mem_free(choice);
	else
		*ch = choice;

	return num;
}

/**
 * Mostrar lista de svals a ignorar.
 */
static bool sval_menu(int tval, const char *desc)
{
	struct menu *menu;
	region area = { 1, 2, -1, -1 };

	ignore_choice *choices;

	int n_choices = ignore_collect_kind(tval, &choices);
	if (!n_choices)
		return false;

	/* Ordenar por nombre en los menús de ignorar excepto para categorías de objetos que son
	 * conscientes desde el principio */
	switch (tval)
	{
		case TV_LIGHT:
		case TV_MAGIC_BOOK:
		case TV_PRAYER_BOOK:
		case TV_NATURE_BOOK:
		case TV_SHADOW_BOOK:
		case TV_OTHER_BOOK:
		case TV_DRAG_ARMOR:
		case TV_GOLD:
			/* dejar ordenado por sval */
			break;

		default:
			/* ordenar por nombre */
			sort(choices, n_choices, sizeof(*choices), cmp_ignore);
	}


	/* Guardar la pantalla y limpiarla */
	screen_save();
	clear_from(0);

	/* Texto de ayuda */
	prt(format("Ignorar los siguientes %s:", desc), 0, 0);

	/* Ejecutar menú */
	menu = menu_new(MN_SKIN_COLUMNS, &ignore_sval_menu);
	menu_setpriv(menu, n_choices, choices);
	menu->cmd_keys = "Tt";
	menu_layout(menu, &area);
	menu_set_cursor_x_offset(menu, 1); /* Colocar cursor en corchetes. */
	menu_select(menu, 0, false);

	/* Liberar memoria */
	mem_free(menu);
	mem_free(choices);

	/* Cargar pantalla */
	screen_load();
	return true;
}


/**
 * Devuelve true si hay algo que mostrar en un menú
 */
static bool seen_tval(int tval)
{
	int i;

	for (i = 1; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Saltar objetos vacíos, objetos no vistos y tvals incorrectos */
		if (!kind->name) continue;
		if (!kind->everseen) continue;
		if (kind->tval != tval) continue;

		 return true;
	}


	return false;
}


/**
 * Opciones extra en el menú "opciones de objeto"
 */
static struct
{
	char tag;
	const char *name;
	void (*action)(const char*, int);
} extra_item_options[] = {
	{ 'Q', "Opciones de ignorar por calidad", quality_menu },
	{ 'E', "Opciones de ignorar por égida", ego_menu},
	{ '{', "Configuración de autoinscripciones", textui_browse_object_knowledge },
};

static char tag_options_item(struct menu *menu, int oid)
{
	size_t line = (size_t) oid;

	if (line < N_ELEMENTS(sval_dependent))
		return all_letters_nohjkl[oid];

	/* Separador - línea en blanco. */
	if (line == N_ELEMENTS(sval_dependent))
		return 0;

	line = line - N_ELEMENTS(sval_dependent) - 1;

	if (line < N_ELEMENTS(extra_item_options))
		return extra_item_options[line].tag;

	return 0;
}

static int valid_options_item(struct menu *menu, int oid)
{
	size_t line = (size_t) oid;

	if (line < N_ELEMENTS(sval_dependent))
		return 1;

	/* Separador - línea en blanco. */
	if (line == N_ELEMENTS(sval_dependent))
		return 0;

	line = line - N_ELEMENTS(sval_dependent) - 1;

	if (line < N_ELEMENTS(extra_item_options))
		return 1;

	return 0;
}

static void display_options_item(struct menu *menu, int oid, bool cursor,
								 int row, int col, int width)
{
	size_t line = (size_t) oid;

	/* La mayor parte del menú son svals, con una pequeña sección de "opciones extra" abajo */
	if (line < N_ELEMENTS(sval_dependent)) {
		bool known = seen_tval(sval_dependent[line].tval);
		uint8_t attr = curs_attrs[known ? CURS_KNOWN: CURS_UNKNOWN][(int)cursor];

		c_prt(attr, sval_dependent[line].desc, row, col);
	} else {
		uint8_t attr = curs_attrs[CURS_KNOWN][(int)cursor];

		line = line - N_ELEMENTS(sval_dependent) - 1;

		if (line < N_ELEMENTS(extra_item_options))
			c_prt(attr, extra_item_options[line].name, row, col);
	}
}

static bool handle_options_item(struct menu *menu, const ui_event *event,
								int oid)
{
	if (event->type == EVT_SELECT) {
		if ((size_t) oid < N_ELEMENTS(sval_dependent))
		{
			sval_menu(sval_dependent[oid].tval, sval_dependent[oid].desc);
		} else {
			oid = oid - (int)N_ELEMENTS(sval_dependent) - 1;
			assert((size_t) oid < N_ELEMENTS(extra_item_options));
			extra_item_options[oid].action(NULL, 0);
		}

		return true;
	}

	return false;
}


static const menu_iter options_item_iter =
{
	tag_options_item,
	valid_options_item,
	display_options_item,
	handle_options_item,
	NULL
};


/**
 * Mostrar y manejar el menú principal de ignorar.
 */
void do_cmd_options_item(const char *title, int row)
{
	struct menu menu;

	menu_init(&menu, MN_SKIN_SCROLL, &options_item_iter);
	menu_setpriv(&menu, N_ELEMENTS(sval_dependent) +
				 N_ELEMENTS(extra_item_options) + 1, NULL);

	menu.title = title;
	menu_layout(&menu, &SCREEN_REGION);

	screen_save();
	clear_from(0);
	menu_select(&menu, 0, false);
	screen_load();

	player->upkeep->notice |= PN_IGNORE;

	return;
}



/**
 * ------------------------------------------------------------------------
 * Definiciones y visualización del menú principal
 * ------------------------------------------------------------------------ */

static struct menu *option_menu;
static menu_action option_actions[] = 
{
	{ 0, 'a', "Opciones de interfaz de usuario", option_toggle_menu },
	{ 0, 'b', "Opciones de nacimiento (dificultad)", option_toggle_menu },
	{ 0, 'x', "Opciones de trampa", option_toggle_menu },
	{ 0, 'w', "Configuración de subventanas", do_cmd_options_win },
	{ 0, 'i', "Configuración de ignorado de objetos", do_cmd_options_item },
	{ 0, '{', "Configuración de autoinscripciones", textui_browse_object_knowledge },
	{ 0, 0, NULL, NULL },
	{ 0, 'd', "Establecer factor de demora base", do_cmd_delay },
	{ 0, 'h', "Establecer advertencia de puntos de golpe", do_cmd_hp_warn },
	{ 0, 'm', "Establecer demora de movimiento", do_cmd_lazymove_delay },
	{ 0, 'o', "Establecer modo de barra lateral", do_cmd_sidebar_mode },
	{ 0, 0, NULL, NULL },
	{ 0, 's', "Guardar configuración de subventanas en archivo de preferencias", do_dump_options },
	{ 0, 't', "Guardar autoinscripciones en archivo de preferencias", do_dump_autoinsc },
	{ 0, 'u', "Guardar opciones de pantalla de personaje en archivo de preferencias", do_dump_charscreen_opt },
	{ 0, 0, NULL, NULL },
	{ 0, 'p', "Cargar un archivo de preferencias de usuario", options_load_pref_file },
	{ 0, 'e', "Editar mapas de teclas (avanzado)", do_cmd_keymaps },
	{ 0, 'c', "Editar colores (avanzado)", do_cmd_colors },
	{ 0, 'v', "Guardar visuales (avanzado)", do_cmd_visuals },
};


/**
 * Mostrar el menú principal de opciones.
 */
void do_cmd_options(void)
{
	if (!option_menu) {
		/* Menú principal de opciones */
		option_menu = menu_new_action(option_actions,
				N_ELEMENTS(option_actions));

		option_menu->title = "Menú de Opciones";
		option_menu->flags = MN_CASELESS_TAGS;
	}

	screen_save();
	clear_from(0);

	menu_layout(option_menu, &SCREEN_REGION);
	menu_select(option_menu, 0, false);

	screen_load();
}

void cleanup_options(void)
{
	if (keymap_menu) menu_free(keymap_menu);
	if (visual_menu) menu_free(visual_menu);
	if (color_menu) menu_free(color_menu);
	if (option_menu) menu_free(option_menu);
}