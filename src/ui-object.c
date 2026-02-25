/**
 * \file ui-object.c
 * \brief Listas de objetos y selección, y otras funciones de interfaz relacionadas con objetos
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 * Copyright (c) 2015 Nick McConnell
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
#include "cmd-core.h"
#include "cmds.h"
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "store.h"
#include "ui-command.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-options.h"
#include "ui-output.h"
#include "ui-prefs.h"

/**
 * ------------------------------------------------------------------------
 * Variables para la visualización y selección de objetos
 * ------------------------------------------------------------------------ */
#define MAX_ITEMS 50

/**
 * Información sobre un objeto particular
 */
struct object_menu_data {
	char label[80];
	char equip_label[80];
	struct object *object;
	char o_name[80];
	char key;
};

static struct object_menu_data items[MAX_ITEMS];
static int num_obj;
static int num_head;
static size_t max_len;
static int ex_width;
static int ex_offset;

/**
 * ------------------------------------------------------------------------
 * Visualización de objetos individuales en listas o para selección
 * ------------------------------------------------------------------------ */
/**
 * Determinar si el atributo y carácter deben considerar el sabor del objeto
 *
 * Los pergaminos identificados deben usar su propio mosaico.
 */
static bool use_flavor_glyph(const struct object_kind *kind)
{
	return kind->flavor && !(kind->tval == TV_SCROLL && kind->aware);
}

/**
 * Devolver el "atributo" para un tipo de objeto dado.
 * Usar "sabor" si está disponible.
 * Por defecto, usar las definiciones del usuario.
 */
uint8_t object_kind_attr(const struct object_kind *kind)
{
	return use_flavor_glyph(kind) ? flavor_x_attr[kind->flavor->fidx] :
		kind_x_attr[kind->kidx];
}

/**
 * Devolver el "carácter" para un tipo de objeto dado.
 * Usar "sabor" si está disponible.
 * Por defecto, usar las definiciones del usuario.
 */
wchar_t object_kind_char(const struct object_kind *kind)
{
	return use_flavor_glyph(kind) ? flavor_x_char[kind->flavor->fidx] :
		kind_x_char[kind->kidx];
}

/**
 * Devolver el "atributo" para un objeto dado.
 * Usar "sabor" si está disponible.
 * Por defecto, usar las definiciones del usuario.
 */
uint8_t object_attr(const struct object *obj)
{
	return object_kind_attr(obj->kind);
}

/**
 * Devolver el "carácter" para un objeto dado.
 * Usar "sabor" si está disponible.
 * Por defecto, usar las definiciones del usuario.
 */
wchar_t object_char(const struct object *obj)
{
	return object_kind_char(obj->kind);
}

/**
 * Mostrar un objeto. Cada objeto puede tener un prefijo con una etiqueta.
 * Usado por show_inven(), show_equip(), show_quiver() y show_floor().
 * Las banderas de modo están documentadas en object.h
 */
static void show_obj(int obj_num, int row, int col, bool cursor,
					 olist_detail_t mode)
{
	int attr;
	int label_attr = cursor ? COLOUR_L_BLUE : COLOUR_WHITE;
	int ex_offset_ctr;
	char buf[80];
	struct object *obj = items[obj_num].object;
	bool show_label = mode & (OLIST_WINDOW | OLIST_DEATH) ? true : false;
	int label_size = show_label ? strlen(items[obj_num].label) : 0;
	int equip_label_size = strlen(items[obj_num].equip_label);

	/* Limpiar la línea */
	prt("", row + obj_num, MAX(col - 1, 0));

	/* Si no tenemos etiqueta, no mostraremos nada */
	if (!strlen(items[obj_num].label)) return;

	/* Imprimir la etiqueta */
	if (show_label)
		c_put_str(label_attr, items[obj_num].label, row + obj_num, col);

	/* Imprimir la etiqueta de equipo */
	c_put_str(label_attr, items[obj_num].equip_label, row + obj_num,
			  col + label_size);

	/* Limitar el nombre del objeto */
	if (label_size + equip_label_size + strlen(items[obj_num].o_name) >
		(size_t)ex_offset) {
		int truncate = ex_offset - label_size - equip_label_size;

		if (truncate < 0) truncate = 0;
		if ((size_t)truncate > sizeof(items[obj_num].o_name) - 1)
			truncate = sizeof(items[obj_num].o_name) - 1;

		items[obj_num].o_name[truncate] = '\0';
	}

	/* El tipo de objeto determina el color de la salida */
	if (obj) {
		attr = obj->kind->base->attr;

		/* Los libros ilegibles son un caso especial */
		if (tval_is_book_k(obj->kind) &&
			(player_object_to_book(player, obj) == NULL)) {
			attr = COLOUR_SLATE;
		}
	} else {
		attr = COLOUR_SLATE;
	}

	/* Nombre del objeto */
	c_put_str(attr, items[obj_num].o_name, row + obj_num,
			  col + label_size + equip_label_size);

	/* Si no tenemos un objeto, podemos saltarnos el resto de la salida */
	if (!obj) return;

	/* Campos extra */
	ex_offset_ctr = ex_offset;

	/* Precio */
	if (mode & OLIST_PRICE) {
		struct store *store = store_at(cave, player->grid);
		if (store) {
			int price = price_item(store, obj, true, obj->number);

			strnfmt(buf, sizeof(buf), "%6d po", price);
			put_str(buf, row + obj_num, col + ex_offset_ctr);
			ex_offset_ctr += 9;
		}
	}

	/* Probabilidad de fallo para dispositivos mágicos y activaciones */
	if (mode & OLIST_FAIL && obj_can_fail(obj)) {
		int fail = (9 + get_use_device_chance(obj)) / 10;
		if (object_effect_is_known(obj))
			strnfmt(buf, sizeof(buf), "%4d%% fallo", fail);
		else
			my_strcpy(buf, "    ? fallo", sizeof(buf));
		put_str(buf, row + obj_num, col + ex_offset_ctr);
		ex_offset_ctr += 10;
	}

	/* Probabilidades de fallo para recargar un objeto; ver effect_handler_RECHARGE */
	if (mode & OLIST_RECHARGE) {
		int fail = 1000 / recharge_failure_chance(obj, player->upkeep->recharge_pow);
		if (object_effect_is_known(obj))
			strnfmt(buf, sizeof(buf), "%2d.%1d%% fallo", fail / 10, fail % 10);
		else
			my_strcpy(buf, "    ? fallo", sizeof(buf));
		put_str(buf, row + obj_num, col + ex_offset_ctr);
		ex_offset_ctr += 10;
	}

	/* Peso */
	if (mode & OLIST_WEIGHT) {
		int weight = obj->number * object_weight_one(obj);
		strnfmt(buf, sizeof(buf), "%4d.%1d lb", weight / 10, weight % 10);
		put_str(buf, row + obj_num, col + ex_offset_ctr);
	}
}

/**
 * ------------------------------------------------------------------------
 * Visualización de listas de objetos
 * ------------------------------------------------------------------------ */
/**
 * Limpiar la lista de objetos.
 */
static void wipe_obj_list(void)
{
	int i;

	/* Poner a cero las constantes */
	num_obj = 0;
	num_head = 0;
	max_len = 0;
	ex_width = 0;
	ex_offset = 0;

	/* Limpiar el contenido existente */
	for (i = 0; i < MAX_ITEMS; i++) {
		my_strcpy(items[i].label, "", sizeof(items[i].label));
		my_strcpy(items[i].equip_label, "", sizeof(items[i].equip_label));
		items[i].object = NULL;
		my_strcpy(items[i].o_name, "", sizeof(items[i].o_name));
		items[i].key = '\0';
	}
}

/**
 * Construir la lista de objetos.
 */
static void build_obj_list(int last, struct object **list, item_tester tester,
						   olist_detail_t mode)
{
	int i;
	bool gold_ok = (mode & OLIST_GOLD) ? true : false;
	bool in_term = (mode & OLIST_WINDOW) ? true : false;
	bool dead = (mode & OLIST_DEATH) ? true : false;
	bool show_empty = (mode & OLIST_SEMPTY) ? true : false;
	bool equip = list ? false : true;
	bool quiver = list == player->upkeep->quiver ? true : false;

	/* Construir la lista de objetos */
	for (i = 0; i <= last; i++) {
		char buf[80];
		struct object *obj = equip ? slot_object(player, i) : list[i];

		/* Los objetos aceptables obtienen una etiqueta */
		if (object_test(tester, obj) ||	(obj && tval_is_money(obj) && gold_ok))
			strnfmt(items[num_obj].label, sizeof(items[num_obj].label), "%c) ",
				quiver ? I2D(i) : all_letters_nohjkl[i]);

		/* Los objetos no aceptables a veces se muestran */
		else if ((!obj && show_empty) || in_term)
			my_strcpy(items[num_obj].label, "   ",
					  sizeof(items[num_obj].label));

		/* Los objetos no aceptables se omiten en la ventana principal */
		else continue;

		/* Mostrar etiquetas completas de ranura para equipo (o carcaj en subventana) */
		if (equip) {
			const char *mention = equip_mention(player, i);
			size_t u8len = utf8_strlen(mention);

			if (u8len < 14) {
				strnfmt(buf, sizeof(buf), "%s%*s", mention,
					(int)(14 - u8len), " ");
			} else {
				char *mention_copy = string_make(mention);

				if (u8len > 14) {
					utf8_clipto(mention_copy, 14);
				}
				strnfmt(buf, sizeof(buf), "%s", mention_copy);
				string_free(mention_copy);
			}
			my_strcpy(items[num_obj].equip_label, buf,
					  sizeof(items[num_obj].equip_label));
		} else if ((in_term || dead) && quiver) {
			strnfmt(buf, sizeof(buf), "Ranura %-9d: ", i);
			my_strcpy(items[num_obj].equip_label, buf,
					  sizeof(items[num_obj].equip_label));
		} else {
			strnfmt(items[num_obj].equip_label,
				sizeof(items[num_obj].equip_label), "%s", "");
		}

		/* Guardar el objeto */
		items[num_obj].object = obj;
		items[num_obj].key = (items[num_obj].label)[0];
		num_obj++;
	}
}

/**
 * Establecer nombres de objetos y obtener su longitud máxima.
 * Solo tiene sentido después de construir la lista de objetos.
 */
static void set_obj_names(bool terse, const struct player *p)
{
	int i;
	struct object *obj;

	/* Calcular desplazamiento del nombre y longitud máxima del nombre */
	for (i = 0; i < num_obj; i++) {
		obj = items[i].object;

		/* Los objetos nulos se usan para saltar líneas, o mostrar solo una etiqueta */		
		if (!obj) {
			if ((i < num_head) || streq(items[i].label, "In quiver"))
				strnfmt(items[i].o_name, sizeof(items[i].o_name), "%s", "");
			else
				strnfmt(items[i].o_name, sizeof(items[i].o_name), "(nada)");
		} else {
			if (terse) {
				object_desc(items[i].o_name,
					sizeof(items[i].o_name), obj,
					ODESC_PREFIX | ODESC_FULL | ODESC_TERSE,
					p);
			} else {
				object_desc(items[i].o_name,
					sizeof(items[i].o_name), obj,
					ODESC_PREFIX | ODESC_FULL, p);
			}
		}

		/* Longitud máxima de etiqueta + nombre del objeto */
		max_len = MAX(max_len,
					  strlen(items[i].label) + strlen(items[i].equip_label) +
					  strlen(items[i].o_name));
	}
}

/**
 * Mostrar una lista de objetos. Cada objeto puede tener un prefijo con una etiqueta.
 * Usado por show_inven(), show_equip(), y show_floor(). Las banderas de modo están
 * documentadas en object.h
 */
static void show_obj_list(olist_detail_t mode)
{
	int i, row = 0, col = 0;
	char tmp_val[80];

	bool in_term = (mode & OLIST_WINDOW) ? true : false;
	bool terse = false;

	/* Inicializar */
	max_len = 0;
	ex_width = 0;
	ex_offset = 0;

	if (in_term) max_len = 40;
	if (in_term && Term->wid < 40) mode &= ~(OLIST_WEIGHT);

	if (Term->wid < 50) terse = true;

	/* Establecer los nombres y obtener la longitud máxima */
	set_obj_names(terse, player);

	/* Tener en cuenta el mensaje del carcaj */
	if (mode & OLIST_QUIVER && player->upkeep->quiver[0] != NULL)
		max_len = MAX(max_len, 24);

	/* Ancho de los campos extra */
	if (mode & OLIST_WEIGHT) ex_width += 9;
	if (mode & OLIST_PRICE) ex_width += 9;
	if (mode & OLIST_FAIL) ex_width += 10;

	/* Determinar fila y columna de inicio */
	if (in_term) {
		/* Ventana de terminal */
		row = 0;
		col = 0;
	} else {
		/* Ventana principal */
		row = 1;
		col = Term->wid - 1 - max_len - ex_width;

		if (col < 3) col = 0;
	}

	/* Desplazamiento de columna del primer campo extra */
	ex_offset = MIN(max_len, (size_t)(Term->wid - 1 - ex_width - col));

	/* Salida de la lista */
	for (i = 0; i < num_obj; i++)
		show_obj(i, row, col, false, mode);

	/* Para el inventario: imprimir el recuento del carcaj */
	if (mode & OLIST_QUIVER) {
		int count, j;
		int quiver_slots = (player->upkeep->quiver_cnt + z_info->quiver_slot_size - 1) / z_info->quiver_slot_size;

		/* El carcaj puede ocupar varias líneas */
		for (j = 0; j < quiver_slots; j++, i++) {
			const char *fmt = "en Carcaj: %d proyectil%s";
			char letter = all_letters_nohjkl[in_term ? i - 1 : i];

			/* Número de proyectiles en esta "ranura" */
			if (j == quiver_slots - 1)
				count = player->upkeep->quiver_cnt - (z_info->quiver_slot_size * (quiver_slots - 1));
			else
				count = z_info->quiver_slot_size;

			/* Limpiar la línea */
			prt("", row + i, MAX(col - 2, 0));

			/* Imprimir la etiqueta (desactivada) */
			strnfmt(tmp_val, sizeof(tmp_val), "%c) ", letter);
			c_put_str(COLOUR_SLATE, tmp_val, row + i, col);

			/* Imprimir el recuento */
			strnfmt(tmp_val, sizeof(tmp_val), fmt, count,
					count == 1 ? "" : "s");
			c_put_str(COLOUR_L_UMBER, tmp_val, row + i, col + 3);
		}
	}

	/* Limpiar ventanas de terminal */
	if (in_term) {
		for (; i < Term->hgt; i++)
			prt("", row + i, MAX(col - 2, 0));
	} else if (i > 0 && row + i < 24) {
		/* Imprimir una sombra para la ventana principal si es necesario */
		prt("", row + i, MAX(col - 2, 0));
	}
}

/**
 * Mostrar el inventario. Construye una lista de objetos y los pasa
 * a show_obj_list() para su visualización. Las banderas de modo están
 * documentadas en object.h
 */
void show_inven(int mode, item_tester tester)
{
	int i, last_slot = -1;
	int diff = weight_remaining(player);

	bool in_term = (mode & OLIST_WINDOW) ? true : false;

	/* Inicializar */
	wipe_obj_list();

	/* Incluir carga para ventanas de terminal */
	if (in_term) {
		strnfmt(items[num_obj].label, sizeof(items[num_obj].label),
		        "Carga %d.%d lb (%d.%d lb %s) ",
		        player->upkeep->total_weight / 10,
				player->upkeep->total_weight % 10,
		        abs(diff) / 10, abs(diff) % 10,
		        (diff < 0 ? "sobrecargado" : "restante"));

		items[num_obj].object = NULL;
		num_obj++;
	}

	/* Encontrar la última ranura de inventario ocupada */
	for (i = 0; i < z_info->pack_size; i++)
		if (player->upkeep->inven[i] != NULL) last_slot = i;

	/* Construir la lista de objetos */
	build_obj_list(last_slot, player->upkeep->inven, tester, mode);

	/* La ventana de terminal comienza con un encabezado de carga */
	num_head = in_term ? 1 : 0;

	/* Mostrar la lista de objetos */
	show_obj_list(mode);
}


/**
 * Mostrar el carcaj. Construye una lista de objetos y los pasa
 * a show_obj_list() para su visualización. Las banderas de modo están
 * documentadas en object.h
 */
void show_quiver(int mode, item_tester tester)
{
	int i, last_slot = -1;

	/* Inicializar */
	wipe_obj_list();

	/* Encontrar la última ranura de carcaj ocupada */
	for (i = 0; i < z_info->quiver_size; i++)
		if (player->upkeep->quiver[i] != NULL) last_slot = i;

	/* Construir la lista de objetos */
	build_obj_list(last_slot, player->upkeep->quiver, tester, mode);

	/* Mostrar la lista de objetos */
	num_head = 0;
	show_obj_list(mode);
}


/**
 * Mostrar el equipo. Construye una lista de objetos y los pasa
 * a show_obj_list() para su visualización. Las banderas de modo están
 * documentadas en object.h
 */
void show_equip(int mode, item_tester tester)
{
	int i;
	bool in_term = (mode & OLIST_WINDOW) ? true : false;

	/* Inicializar */
	wipe_obj_list();

	/* Construir la lista de objetos */
	build_obj_list(player->body.count - 1, NULL, tester, mode);

	/* Mostrar el carcaj en subventanas */
	if (in_term) {
		int last_slot = -1;

		strnfmt(items[num_obj].label, sizeof(items[num_obj].label),
				"En carcaj");
		items[num_obj].object = NULL;
		num_obj++;

		/* Encontrar la última ranura de carcaj ocupada */
		for (i = 0; i < z_info->quiver_size; i++)
			if (player->upkeep->quiver[i] != NULL) last_slot = i;

		/* Extender la lista de objetos */
		build_obj_list(last_slot, player->upkeep->quiver, tester, mode);
	}

	/* Mostrar la lista de objetos */
	num_head = 0;
	show_obj_list(mode);
}


/**
 * Mostrar el suelo. Construye una lista de objetos y los pasa
 * a show_obj_list() para su visualización. Las banderas de modo están
 * documentadas en object.h
 */
void show_floor(struct object **floor_list, int floor_num, int mode,
				item_tester tester)
{
	/* Inicializar */
	wipe_obj_list();

	if (floor_num > z_info->floor_size)
		floor_num = z_info->floor_size;

	/* Construir la lista de objetos */
	build_obj_list(floor_num - 1, floor_list, tester, mode);

	/* Mostrar la lista de objetos */
	num_head = 0;
	show_obj_list(mode);
}


/**
 * ------------------------------------------------------------------------
 * Variables para la selección de objetos
 * ------------------------------------------------------------------------ */

static item_tester tester_m;
static region area = { 20, 1, -1, -2 };
static struct object *selection;
static const char *prompt;
static char header[80];
static int i1, i2;
static int e1, e2;
static int q1, q2;
static int f1, f2;
static int throwing_num;
static struct object **floor_list;
static struct object **throwing_list;
static olist_detail_t olist_mode = 0;
static int item_mode;
static cmd_code item_cmd;
static bool newmenu = false;
static bool allow_all = false;

/**
 * ------------------------------------------------------------------------
 * Utilidades de selección de objetos
 * ------------------------------------------------------------------------ */

/**
 * Prevenir ciertas elecciones dependiendo de las inscripciones en el objeto.
 *
 * El objeto puede ser negativo para significar "objeto en el suelo".
 */
bool get_item_allow(const struct object *obj, unsigned char ch, cmd_code cmd,
					bool is_harmless)
{
	char verify_inscrip[] = "!*";

	unsigned n;

	/*
	 * Truco - Solo cambiar la tecla de comando si realmente necesita ser cambiada.
	 * Porque UN_KTRL('ctrl-d') (es decir, comando de ignorar en roguelike) da 'd'
	 * que es el comando de soltar en ambos conjuntos de teclas, usar UN_KTRL_CAP().
	 */
	if (ch < 0x20)
		ch = UN_KTRL_CAP(ch);

	/* La inscripción a buscar */
	verify_inscrip[1] = ch;

	/* Buscar la inscripción */
	n = check_for_inscrip(obj, verify_inscrip);

	/* También buscar la inscripción '!*' */
	if (!is_harmless)
		n += check_for_inscrip(obj, "!*");

	/* Elegir cadena para el mensaje */
	if (n) {
		char prompt_buf[1024];

		const char *verb = cmd_verb(cmd);
		if (!verb)
			verb = "hacer eso con";

		strnfmt(prompt_buf, sizeof(prompt_buf), "¿Realmente %s", verb);

		/* Preguntar para confirmar n veces */
		while (n--) {
			if (!verify_object(prompt_buf, obj, player)) {
				return false;
			}
		}
	}

	/* Permitirlo */
	return (true);
}



/**
 * Encontrar el primer objeto en la lista de objetos con la "etiqueta" dada. La lista
 * de objetos debe construirse antes de llamar a esta función.
 *
 * Una "etiqueta" es un carácter "n" que aparece como "@n" en cualquier parte de la
 * inscripción de un objeto.
 *
 * También, la etiqueta "@xn" funcionará igualmente, donde "n" es un carácter de etiqueta,
 * y "x" es la acción para la que funcionará esa etiqueta.
 */
static bool get_tag(struct object **tagged_obj, char tag, cmd_code cmd,
				   bool quiver_tags)
{
	int i;
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	/* (f)uego se maneja de manera diferente a todos los demás, debido al carcaj */
	if (quiver_tags) {
		i = tag - '0';
		if (player->upkeep->quiver[i]) {
			*tagged_obj = player->upkeep->quiver[i];
			return true;
		}
	}

	/* Verificar cada objeto en la lista de objetos */
	for (i = 0; i < num_obj; i++) {
		const char *s;
		struct object *obj = items[i].object;

		/* Saltar no-objetos */
		if (!obj) continue;

		/* Saltar inscripciones vacías */
		if (!obj->note) continue;

		/* Encontrar un '@' */
		s = strchr(quark_str(obj->note), '@');

		/* Procesar todas las etiquetas */
		while (s) {
			unsigned char cmdkey;

			/* Verificar las etiquetas normales */
			if (s[1] == tag) {
				/* Guardar el objeto actual */
				*tagged_obj = obj;

				/* Éxito */
				return true;
			}

			cmdkey = cmd_lookup_key_unktrl(cmd, mode);

			/* Verificar las etiquetas especiales */
			if ((s[1] == cmdkey) && (s[2] == tag)) {
				/* Guardar el ID de inventario actual */
				*tagged_obj = obj;

				/* Éxito */
				return true;
			}

			/* Encontrar otro '@' */
			s = strchr(s + 1, '@');
		}
	}

	/* No existe tal etiqueta */
	return false;
}


/**
 * ------------------------------------------------------------------------
 * Menú de selección de objetos
 * ------------------------------------------------------------------------ */

/**
 * Hacer el encabezado correcto para el menú de selección
 */
static void menu_header(void)
{
	char tmp_val[75];
	char out_val[75];

	bool use_inven = ((item_mode & USE_INVEN) ? true : false);
	bool use_equip = ((item_mode & USE_EQUIP) ? true : false);
	bool use_quiver = ((item_mode & USE_QUIVER) ? true : false);
	bool allow_floor = ((f1 <= f2) || allow_all);

	/* Viendo inventario */
	if (player->upkeep->command_wrk == USE_INVEN) {
		/* Comenzar el encabezado */
		strnfmt(out_val, sizeof(out_val), "Inven:");

		/* Listar opciones */
		if (i1 <= i2) {
			/* Construir el encabezado */
			strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				all_letters_nohjkl[i1], all_letters_nohjkl[i2]);

			/* Añadir */
			my_strcat(out_val, tmp_val, sizeof(out_val));
		}

		/* Indicar legalidad del equipo */
		if (use_equip)
			my_strcat(out_val, " / para Equip,", sizeof(out_val));

		/* Indicar legalidad del carcaj */
		if (use_quiver)
			my_strcat(out_val, " | para Carcaj,", sizeof(out_val));

		/* Indicar legalidad del "suelo" */
		if (allow_floor)
			my_strcat(out_val, " - para suelo,", sizeof(out_val));
	}

	/* Viendo equipo */
	else if (player->upkeep->command_wrk == USE_EQUIP) {
		/* Comenzar el encabezado */
		strnfmt(out_val, sizeof(out_val), "Equip:");

		/* Listar opciones */
		if (e1 <= e2) {
			/* Construir el encabezado */
			strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				all_letters_nohjkl[e1], all_letters_nohjkl[e2]);

			/* Añadir */
			my_strcat(out_val, tmp_val, sizeof(out_val));
		}

		/* Indicar legalidad del inventario */
		if (use_inven)
			my_strcat(out_val, " / para Inven,", sizeof(out_val));

		/* Indicar legalidad del carcaj */
		if (use_quiver)
			my_strcat(out_val, " | para Carcaj,", sizeof(out_val));

		/* Indicar legalidad del "suelo" */
		if (allow_floor)
			my_strcat(out_val, " - para suelo,", sizeof(out_val));
	}

	/* Viendo carcaj */
	else if (player->upkeep->command_wrk == USE_QUIVER) {
		/* Comenzar el encabezado */
		strnfmt(out_val, sizeof(out_val), "Carcaj:");

		/* Listar opciones */
		if (q1 <= q2) {
			/* Construir el encabezado */
			strnfmt(tmp_val, sizeof(tmp_val), " %d-%d,", q1, q2);

			/* Añadir */
			my_strcat(out_val, tmp_val, sizeof(out_val));
		}

		/* Indicar legalidad del inventario o equipo */
		if (use_inven)
			my_strcat(out_val, " / para Inven,", sizeof(out_val));
		else if (use_equip)
			my_strcat(out_val, " / para Equip,", sizeof(out_val));

		/* Indicar legalidad del "suelo" */
		if (allow_floor)
			my_strcat(out_val, " - para suelo,", sizeof(out_val));
	}

	/* Viendo lanzamiento */
	else if (player->upkeep->command_wrk == SHOW_THROWING) {
		/* Comenzar el encabezado */
		strnfmt(out_val, sizeof(out_val), "Objetos para lanzar:");

		/* Listar opciones */
		if (throwing_num) {
			/* Construir el encabezado */
			strnfmt(tmp_val, sizeof(tmp_val),  " a-%c,",
				all_letters_nohjkl[throwing_num - 1]);

			/* Añadir */
			my_strcat(out_val, tmp_val, sizeof(out_val));
		}

		/* Indicar legalidad del inventario */
		if (use_inven)
			my_strcat(out_val, " / para Inven,", sizeof(out_val));

		/* Indicar legalidad del carcaj */
		if (use_quiver)
			my_strcat(out_val, " | para Carcaj,", sizeof(out_val));

		/* Indicar legalidad del "suelo" */
		if (allow_floor)
			my_strcat(out_val, " - para suelo,", sizeof(out_val));
	}

	/* Viendo suelo */
	else {
		/* Comenzar el encabezado */
		strnfmt(out_val, sizeof(out_val), "Suelo:");

		/* Listar opciones */
		if (f1 <= f2) {
			/* Construir el encabezado */
			strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,",
				all_letters_nohjkl[f1], all_letters_nohjkl[f2]);

			/* Añadir */
			my_strcat(out_val, tmp_val, sizeof(out_val));
		}

		/* Indicar legalidad del inventario o equipo */
		if (use_inven)
			my_strcat(out_val, " / para Inven,", sizeof(out_val));
		else if (use_equip)
			my_strcat(out_val, " / para Equip,", sizeof(out_val));

		/* Indicar legalidad del carcaj */
		if (use_quiver)
			my_strcat(out_val, " | para Carcaj,", sizeof(out_val));
	}

	/* Terminar el encabezado */
	my_strcat(out_val, " ESC", sizeof(out_val));

	/* Construir el encabezado */
	strnfmt(header, sizeof(header), "(%s)", out_val);
}


/**
 * Obtener una etiqueta de objeto
 */
static char get_item_tag(struct menu *menu, int oid)
{
	struct object_menu_data *choice = menu_priv(menu);

	return choice[oid].key;
}

/**
 * Determinar si un objeto es una opción válida
 */
static int get_item_validity(struct menu *menu, int oid)
{
	struct object_menu_data *choice = menu_priv(menu);

	return (choice[oid].object != NULL) ? 1 : 0;
}

/**
 * Mostrar una entrada en el menú de objetos
 */
static void get_item_display(struct menu *menu, int oid, bool cursor, int row,
					  int col, int width)
{
	/* Imprimirlo */
	show_obj(oid, row - oid, col, cursor, olist_mode);
}

/**
 * Manejar eventos en el menú get_item
 */
static bool get_item_action(struct menu *menu, const ui_event *event, int oid)
{
	struct object_menu_data *choice = menu_priv(menu);
	char key = event->key.code;
	bool is_harmless = item_mode & IS_HARMLESS ? true : false;
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	if (event->type == EVT_SELECT) {
		if (choice[oid].object && get_item_allow(choice[oid].object, cmd_lookup_key(item_cmd, mode),
						   item_cmd, is_harmless))
			selection = choice[oid].object;
	}

	if (event->type == EVT_KBRD) {
		if (key == '/') {
			/* Alternar si está permitido */
			if (((item_mode & USE_INVEN) || allow_all)
				&& (player->upkeep->command_wrk != USE_INVEN)) {
				player->upkeep->command_wrk = USE_INVEN;
				newmenu = true;
			} else if (((item_mode & USE_EQUIP) || allow_all) &&
					   (player->upkeep->command_wrk != USE_EQUIP)) {
				player->upkeep->command_wrk = USE_EQUIP;
				newmenu = true;
			} else {
				bell();
			}
		}

		else if (key == '|') {
			/* No se permite alternar */
			if ((q1 > q2) && !allow_all){
				bell();
			} else {
				/* Alternar a carcaj */
				player->upkeep->command_wrk = (USE_QUIVER);
				newmenu = true;
			}
		}

		else if (key == '-') {
			/* No se permite alternar */
			if ((f1 > f2) && !allow_all) {
				bell();
			} else {
				/* Alternar a suelo */
				player->upkeep->command_wrk = (USE_FLOOR);
				newmenu = true;
			}
		}
	}

	return false;
}

/**
 * Mostrar proyectiles del carcaj en el inventario completo
 */
static void item_menu_browser(int oid, void *data, const region *local_area)
{
	char tmp_val[80];
	int count, j, i = num_obj;
	int quiver_slots = (player->upkeep->quiver_cnt + z_info->quiver_slot_size - 1)
		/ z_info->quiver_slot_size;

	/* Configurar para salida debajo del menú */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 0;
	text_out_indent = local_area->col - 1;
	text_out_pad = 1;
	prt("", local_area->row + local_area->page_rows, MAX(0, local_area->col - 1));
	Term_gotoxy(local_area->col, local_area->row + local_area->page_rows);

	/* Si estamos imprimiendo las ranuras de mochila que ocupa el carcaj */
	if (olist_mode & OLIST_QUIVER && player->upkeep->command_wrk == USE_INVEN) {
		/* El carcaj puede ocupar varias líneas */
		for (j = 0; j < quiver_slots; j++, i++) {
			const char *fmt = "en Carcaj: %d proyectil%s\n";
			char letter = all_letters_nohjkl[i];

			/* Número de proyectiles en esta "ranura" */
			if (j == quiver_slots - 1)
				count = player->upkeep->quiver_cnt - (z_info->quiver_slot_size *
													  (quiver_slots - 1));
			else
				count = z_info->quiver_slot_size;

			/* Imprimir la etiqueta (desactivada) */
			strnfmt(tmp_val, sizeof(tmp_val), "%c) ", letter);
			text_out_c(COLOUR_SLATE, tmp_val, local_area->row + i, local_area->col);

			/* Imprimir el recuento */
			strnfmt(tmp_val, sizeof(tmp_val), fmt, count,
					count == 1 ? "" : "s");
			text_out_c(COLOUR_L_UMBER, tmp_val, local_area->row + i, local_area->col + 3);
		}
	}

	/* Siempre imprimir una línea en blanco */
	prt("", local_area->row + i, MAX(0, local_area->col - 1));

	/* Limpiar mosaicos completos */
	while ((tile_height > 1) && ((local_area->row + i) % tile_height != 0)) {
		i++;
		prt("", local_area->row + i, MAX(0, local_area->col - 1));
	}

	text_out_pad = 0;
	text_out_indent = 0;
}

/**
 * Mostrar y manejar la interacción del usuario con un menú contextual para cambiar la
 * lista de objetos.
 *
 * \param current_menu es el menú estándar (no contextual) que muestra la
 * lista de objetos.
 * \param in es el evento que desencadena el menú contextual. in->type debe ser
 * EVT_MOUSE.
 * \param out es el evento que se pasará hacia arriba (al manejo interno en
 * menu_select() o, potencialmente, al llamador de menu_select()).
 * \return true si el evento fue manejado; en caso contrario, devuelve false.
 */
static bool use_context_menu_list_switcher(struct menu *current_menu,
		const ui_event *in, ui_event *out)
{
	struct menu *m;
	char *labels;
	bool allows_inven;
	int selected;

	assert(in->type == EVT_MOUSE);
	if (in->mouse.y != 0) {
		return false;
	}

	m = menu_dynamic_new();
	if (!m) {
		return false;
	}
	labels = string_make(lower_case);

	m->selections = labels;
	if (((item_mode & USE_INVEN) || allow_all)
			&& player->upkeep->command_wrk != USE_INVEN) {
		menu_dynamic_add_label(m, "Inventario", '/', USE_INVEN, labels);
		allows_inven = true;
	} else {
		allows_inven = false;
	}
	if (((item_mode & USE_EQUIP) || allow_all)
			&& player->upkeep->command_wrk != USE_EQUIP) {
		menu_dynamic_add_label(m, "Equipo",
			(allows_inven) ? 'e' : '/', USE_EQUIP, labels);
	}
	if ((q1 <= q2 || allow_all)
			&& player->upkeep->command_wrk != USE_QUIVER) {
		menu_dynamic_add_label(m, "Carcaj", '|', USE_QUIVER, labels);
	}
	if ((f1 <= f2 || allow_all)
			&& player->upkeep->command_wrk != USE_FLOOR) {
		menu_dynamic_add_label(m, "Suelo", '-', USE_FLOOR, labels);
	}
	menu_dynamic_add_label(m, "Salir", 'q', 0, labels);

	screen_save();

	menu_dynamic_calc_location(m, in->mouse.x, in->mouse.y);
	region_erase_bordered(&m->boundary);

	selected = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	if (selected == USE_INVEN || selected == USE_EQUIP
			|| selected == USE_QUIVER || selected == USE_FLOOR) {
		player->upkeep->command_wrk = selected;
		newmenu = true;
		out->type = EVT_SWITCH;
	} else if (selected == 0) {
		out->type = EVT_ESCAPE;
	}

	return true;
}

/**
 * Mostrar elementos de la lista para elegir
 */
static struct object *item_menu(cmd_code cmd, int prompt_size, int mode)
{
	menu_iter menu_f = { get_item_tag, get_item_validity, get_item_display,
						 get_item_action, 0 };
	struct menu *m = menu_new(MN_SKIN_OBJECT, &menu_f);
	ui_event evt = { 0 };
	int ex_offset_ctr = 0;
	int row, inscrip;
	struct object *obj = NULL;

	/* Configurar el menú */
	menu_setpriv(m, num_obj, items);
	if (player->upkeep->command_wrk == USE_QUIVER)
		m->selections = "0123456789";
	else
		m->selections = all_letters_nohjkl;
	m->switch_keys = "/|-";
	m->context_hook = use_context_menu_list_switcher;
	m->flags = (MN_PVT_TAGS | MN_INSCRIP_TAGS | MN_KEYMAP_ESC);
	m->browse_hook = item_menu_browser;

	/* Obtener inscripciones */
	m->inscriptions = mem_zalloc(10 * sizeof(char));
	for (inscrip = 0; inscrip < 10; inscrip++) {
		/* Buscar la etiqueta */
		if (get_tag(&obj, (char)inscrip + '0', item_cmd,
					item_mode & QUIVER_TAGS)) {
			int i;
			for (i = 0; i < num_obj; i++)
				if (items[i].object == obj)
						break;

			if (i < num_obj)
				m->inscriptions[inscrip] = get_item_tag(m, i);
		}
	}

	/* Configurar las variables de la lista de objetos */
	selection = NULL;
	set_obj_names(false, player);

	if (mode & OLIST_QUIVER && player->upkeep->quiver[0] != NULL)
		max_len = MAX(max_len, 24);

	if (olist_mode & OLIST_WEIGHT) {
		ex_width += 9;
		ex_offset_ctr += 9;
	}
	if (olist_mode & OLIST_PRICE) {
		ex_width += 9;
		ex_offset_ctr += 9;
	}
	if (olist_mode & OLIST_FAIL) {
		ex_width += 10;
		ex_offset_ctr += 10;
	}

	/* Configurar la región del menú */
	area.page_rows = m->count;
	area.row = 1;
	area.col = MIN(Term->wid - 1 - (int) max_len - ex_width, prompt_size - 2);
	if (area.col <= 3)
		area.col = 0;
	ex_offset = MIN(max_len, (size_t)(Term->wid - 1 - ex_width - area.col));
	while (strlen(header) < max_len + ex_width + ex_offset_ctr) {
		my_strcat(header, " ", sizeof(header));
		if (strlen(header) > sizeof(header) - 2) break;
	}
	area.width = MAX(max_len, strlen(header));

	for (row = area.row; row < area.row + area.page_rows; row++)
		prt("", row, MAX(0, area.col - 1));

	menu_layout(m, &area);

	/* Elegir */
	evt = menu_select(m, 0, true);

	/* Limpiar */
	mem_free(m->inscriptions);
	mem_free(m);

	/* Manejar cambio de menú */
	if (evt.type == EVT_SWITCH && !newmenu) {
		bool left = evt.key.code == ARROW_LEFT;

		if (player->upkeep->command_wrk == USE_EQUIP) {
			if (left) {
				if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
				else if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
			} else {
				if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
				else if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
			}
		} else if (player->upkeep->command_wrk == USE_INVEN) {
			if (left) {
				if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
				else if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
				else if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
			} else {
				if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
				else if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
				else if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
			}
		} else if (player->upkeep->command_wrk == USE_QUIVER) {
			if (left) {
				if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
				else if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
			} else {
				if (f1 <= f2) player->upkeep->command_wrk = USE_FLOOR;
				else if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
			}
		} else if (player->upkeep->command_wrk == USE_FLOOR) {
			if (left) {
				if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
			} else {
				if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
			}
		} else if (player->upkeep->command_wrk == SHOW_THROWING) {
			if (left) {
				if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
			} else {
				if (e1 <= e2) player->upkeep->command_wrk = USE_EQUIP;
				else if (i1 <= i2) player->upkeep->command_wrk = USE_INVEN;
				else if (q1 <= q2) player->upkeep->command_wrk = USE_QUIVER;
			}
		}

		newmenu = true;
	}

	/* Resultado */
	return selection;
}



/**
 * Dejar que el usuario seleccione un objeto, guardar su dirección
 *
 * Devuelve true solo si el usuario eligió un objeto aceptable.
 *
 * Se permite al usuario elegir objetos aceptables del equipo,
 * inventario, carcaj o suelo, respectivamente, si se dio la bandera adecuada,
 * y hay objetos aceptables en esa ubicación.
 *
 * El equipo, inventario o carcaj se muestran (incluso si no hay objetos
 * aceptables en esa ubicación) si se dio la bandera adecuada.
 *
 * Si no hay objetos aceptables disponibles en ninguna parte, y "str" no
 * es NULL, se usará como texto de un mensaje de advertencia
 * antes de que la función regrese.
 *
 * Si se selecciona un objeto legal, lo guardamos en "choice" y devolvemos true.
 *
 * Si no hay ningún objeto disponible, no hacemos nada con "choice" y mostramos un
 * mensaje de advertencia, usando "str" si está disponible, y devolvemos false.
 *
 * Si no se selecciona ningún objeto, no hacemos nada con "choice" y devolvemos false.
 *
 * El global "player->upkeep->command_wrk" se usa para elegir entre
 * listados de equip/inven/quiver/floor. Es igual a USE_INVEN o USE_EQUIP o
 * USE_QUIVER o USE_FLOOR, excepto cuando se llama a esta función por primera vez, cuando
 * es igual a cero, lo que hará que se establezca en USE_INVEN.
 *
 * Siempre borramos el mensaje cuando terminamos, dejando una línea en blanco,
 * o un mensaje de advertencia, si corresponde, si no hay objetos disponibles.
 *
 * Nótese que solo los objetos de suelo "aceptables" obtienen índices, por lo que entre dos
 * comandos, los índices de los objetos del suelo pueden cambiar. XXX XXX XXX
 */
bool textui_get_item(struct object **choice, const char *pmt, const char *str,
					 cmd_code cmd, item_tester tester, int mode)
{
	bool use_inven = ((mode & USE_INVEN) ? true : false);
	bool use_equip = ((mode & USE_EQUIP) ? true : false);
	bool use_quiver = ((mode & USE_QUIVER) ? true : false);
	bool use_floor = ((mode & USE_FLOOR) ? true : false);
	bool quiver_tags = ((mode & QUIVER_TAGS) ? true : false);
	bool show_throwing = ((mode & SHOW_THROWING) ? true : false);

	bool allow_inven = false;
	bool allow_equip = false;
	bool allow_quiver = false;
	bool allow_floor = false;

	bool toggle = false;

	int floor_max = z_info->floor_size;
	int floor_num;

	int throwing_max = z_info->pack_size + z_info->quiver_size +
		z_info->floor_size;

	floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
	throwing_list = mem_zalloc(throwing_max * sizeof(*throwing_list));
	olist_mode = 0;
	item_mode = mode;
	item_cmd = cmd;
	tester_m = tester;
	prompt = pmt;
	allow_all = str ? false : true;

	/* Modos de visualización de la lista de objetos */
	if (mode & SHOW_FAIL)
		olist_mode |= OLIST_FAIL;
	else
		olist_mode |= OLIST_WEIGHT;

	if (mode & SHOW_PRICES)
		olist_mode |= OLIST_PRICE;

	if (mode & SHOW_EMPTY)
		olist_mode |= OLIST_SEMPTY;

	if (mode & SHOW_QUIVER)
		olist_mode |= OLIST_QUIVER;

	if (mode & SHOW_RECHARGE)
		olist_mode |= OLIST_RECHARGE;

	/* Paranoia XXX XXX XXX */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Inventario completo */
	i1 = 0;
	i2 = z_info->pack_size - 1;

	/* Prohibir inventario */
	if (!use_inven) i2 = -1;

	/* Restringir índices de inventario */
	while ((i1 <= i2) && (!object_test(tester, player->upkeep->inven[i1])))
		i1++;
	while ((i1 <= i2) && (!object_test(tester, player->upkeep->inven[i2])))
		i2--;

	/* Aceptar inventario */
	if ((i1 <= i2) || allow_all)
		allow_inven = true;
	else if (item_mode & USE_INVEN)
		item_mode -= USE_INVEN;

	/* Equipo completo */
	e1 = 0;
	e2 = player->body.count - 1;

	/* Prohibir equipo */
	if (!use_equip) e2 = -1;

	/* Restringir índices de equipo a menos que comience sin comando */
	if ((cmd != CMD_NULL) || (tester != NULL)) {
		while ((e1 <= e2) && (!object_test(tester, slot_object(player, e1))))
			e1++;
		while ((e1 <= e2) && (!object_test(tester, slot_object(player, e2))))
			e2--;
	}

	/* Aceptar equipo */
	if ((e1 <= e2) || allow_all)
		allow_equip = true;
	else if (item_mode & USE_EQUIP)
		item_mode -= USE_EQUIP;

	/* Restringir índices de carcaj */
	q1 = 0;
	q2 = z_info->quiver_size - 1;

	/* Prohibir carcaj */
	if (!use_quiver) q2 = -1;

	/* Restringir índices de carcaj */
	while ((q1 <= q2) && (!object_test(tester, player->upkeep->quiver[q1])))
		q1++;
	while ((q1 <= q2) && (!object_test(tester, player->upkeep->quiver[q2])))
		q2--;

	/* Aceptar carcaj */
	if ((q1 <= q2) || allow_all)
		allow_quiver = true;
	else if (item_mode & USE_QUIVER)
		item_mode -= USE_QUIVER;

	/* Escanear todos los objetos no monetarios en la casilla */
	floor_num = scan_floor(floor_list, floor_max, player,
		OFLOOR_TEST | OFLOOR_SENSE | OFLOOR_VISIBLE, tester);

	/* Suelo completo */
	f1 = 0;
	f2 = floor_num - 1;

	/* Prohibir suelo */
	if (!use_floor) f2 = -1;

	/* Restringir índices de suelo */
	while ((f1 <= f2) && (!object_test(tester, floor_list[f1]))) f1++;
	while ((f1 <= f2) && (!object_test(tester, floor_list[f2]))) f2--;

	/* Aceptar suelo */
	if ((f1 <= f2) || allow_all)
		allow_floor = true;
	else if (item_mode & USE_FLOOR)
		item_mode -= USE_FLOOR;

	/* Escanear todos los objetos para lanzar al alcance */
	throwing_num = scan_items(throwing_list, throwing_max, player,
		USE_INVEN | USE_QUIVER | USE_FLOOR, obj_is_throwing);

	/* Requerir al menos una opción legal */
	if (allow_inven || allow_equip || allow_quiver || allow_floor) {
		/* Usar menú de lanzamiento si es posible */
		if (show_throwing && throwing_num) {
			player->upkeep->command_wrk = SHOW_THROWING;

			/* Comenzar donde se solicitó si es posible */
		} else if ((player->upkeep->command_wrk == USE_EQUIP) && allow_equip)
			player->upkeep->command_wrk = USE_EQUIP;
		else if ((player->upkeep->command_wrk == USE_INVEN) && allow_inven)
			player->upkeep->command_wrk = USE_INVEN;
		else if ((player->upkeep->command_wrk == USE_QUIVER) && allow_quiver)
			player->upkeep->command_wrk = USE_QUIVER;
		else if ((player->upkeep->command_wrk == USE_FLOOR) && allow_floor)
			player->upkeep->command_wrk = USE_FLOOR;

		/* Si obviamente estamos usando el carcaj, entonces empezar en carcaj */
		else if (quiver_tags && allow_quiver && (cmd != CMD_USE))
			player->upkeep->command_wrk = USE_QUIVER;

		/* De lo contrario, elegir lo que esté permitido */
		else if (use_inven && allow_inven)
			player->upkeep->command_wrk = USE_INVEN;
		else if (use_equip && allow_equip)
			player->upkeep->command_wrk = USE_EQUIP;
		else if (use_quiver && allow_quiver)
			player->upkeep->command_wrk = USE_QUIVER;
		else if (use_floor && allow_floor)
			player->upkeep->command_wrk = USE_FLOOR;

		/* Si no hay nada que elegir, usar inventario (vacío) */
		else
			player->upkeep->command_wrk = USE_INVEN;

		while (true) {
			int j;
			int ni = 0;
			int ne = 0;

			/* Si inven o equip está en la pantalla principal, y solo uno de ellos
			 * está destinado a una subventana, deberíamos mostrar el opuesto allí */
			for (j = 0; j < ANGBAND_TERM_MAX; j++) {
				/* No usado */
				if (!angband_term[j]) continue;

				/* Contar ventanas que muestran inven */
				if (window_flag[j] & (PW_INVEN)) ni++;

				/* Contar ventanas que muestran equip */
				if (window_flag[j] & (PW_EQUIP)) ne++;
			}

			/* ¿Estamos en la situación en la que tiene sentido alternar? */
			if ((ni && !ne) || (!ni && ne)) {
				if (player->upkeep->command_wrk == USE_EQUIP) {
					if ((ne && !toggle) || (ni && toggle)) {
						/* La pantalla principal es equipo, también la subventana */
						toggle_inven_equip();
						toggle = !toggle;
					}
				} else if (player->upkeep->command_wrk == USE_INVEN) {
					if ((ni && !toggle) || (ne && toggle)) {
						/* La pantalla principal es inventario, también la subventana */
						toggle_inven_equip();
						toggle = !toggle;
					}
				} else {
					/* Carcaj o suelo, volver al original */
					if (toggle) {
						toggle_inven_equip();
						toggle = !toggle;
					}
				}
			}

			/* Redibujar */
			player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

			/* Redibujar ventanas */
			redraw_stuff(player);

			/* Guardar pantalla */
			screen_save();

			/* Construir lista de objetos */
			wipe_obj_list();
			if (player->upkeep->command_wrk == USE_INVEN)
				build_obj_list(i2, player->upkeep->inven, tester_m, olist_mode);
			else if (player->upkeep->command_wrk == USE_EQUIP)
				build_obj_list(e2, NULL, tester_m, olist_mode);
			else if (player->upkeep->command_wrk == USE_QUIVER)
				build_obj_list(q2, player->upkeep->quiver, tester_m,olist_mode);
			else if (player->upkeep->command_wrk == USE_FLOOR)
				build_obj_list(f2, floor_list, tester_m, olist_mode);
			else if (player->upkeep->command_wrk == SHOW_THROWING)
				build_obj_list(throwing_num, throwing_list, tester_m,
							   olist_mode);

			/* Mostrar el mensaje */
			menu_header();
			if (pmt) {
				prt(pmt, 0, 0);
				prt(header, 0, strlen(pmt) + 1);
			}

			/* No hay solicitud de cambio de menú */
			newmenu = false;

			/* Obtener una elección de objeto */
			*choice = item_menu(cmd, MAX(pmt ? strlen(pmt) : 0, 15), mode);

			/* Arreglar la pantalla */
			screen_load();

			/* Actualizar */
			player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
			redraw_stuff(player);

			/* Limpiar la línea de mensaje */
			prt("", 0, 0);

			/* Tenemos una selección, o estamos retrocediendo */
			if (*choice || !newmenu) {
				if (toggle) toggle_inven_equip();
				break;
			}
		}
	} else {
		/* Advertencia si es necesario */
		if (str) msg("%s", str);
		*choice = NULL;
	}

	/* Limpiar */
	player->upkeep->command_wrk = 0;
	mem_free(throwing_list);
	mem_free(floor_list);

	/* Resultado */
	return (*choice != NULL) ? true : false;
}


/**
 * ------------------------------------------------------------------------
 * Recuerdo de objetos
 * ------------------------------------------------------------------------ */


/**
 * Esto dibuja la subventana de Recuerdo de Objetos cuando se muestra un objeto particular
 * (ej. un casco en la mochila, o un pergamino en el suelo)
 */
void display_object_recall(struct object *obj)
{
	char header_buf[120];

	textblock *tb = object_info(obj, OINFO_NONE);
	object_desc(header_buf, sizeof(header_buf), obj,
		ODESC_PREFIX | ODESC_FULL, player);

	clear_from(0);
	textui_textblock_place(tb, SCREEN_REGION, header_buf);
	textblock_free(tb);
}


/**
 * Esto dibuja la subventana de Recuerdo de Objetos cuando se muestra un tipo de objeto recordado
 * (ej. un anillo de ácido genérico o una hoja del caos genérica)
 */
void display_object_kind_recall(struct object_kind *kind)
{
	struct object object = OBJECT_NULL, known_obj = OBJECT_NULL;
	object_prep(&object, kind, 0, EXTREMIFY);
	if (kind->aware || !kind->flavor) {
		object_copy(&known_obj, &object);
	}
	object.known = &known_obj;

	display_object_recall(&object);
	object_wipe(&known_obj);
	object_wipe(&object);
}

/**
 * Mostrar el recuerdo de objeto modalmente y esperar una pulsación de tecla.
 *
 * Esto está configurado para su uso en modo mirar (ver target_set_interactive_aux()).
 *
 * \param obj es el objeto a describir.
 */
void display_object_recall_interactive(struct object *obj)
{
	char header_buf[120];
	textblock *tb;

	event_signal(EVENT_MESSAGE_FLUSH);

	tb = object_info(obj, OINFO_NONE);
	object_desc(header_buf, sizeof(header_buf), obj,
		ODESC_PREFIX | ODESC_FULL, player);
	textui_textblock_show(tb, SCREEN_REGION, header_buf);
	textblock_free(tb);
}

/**
 * Examinar un objeto
 */
void textui_obj_examine(void)
{
	char header_buf[120];

	textblock *tb;
	region local_area = { 0, 0, 0, 0 };

	struct object *obj;

	/* Seleccionar objeto */
	if (!get_item(&obj, "¿Examinar qué objeto?", "No tienes nada que examinar.",
			CMD_NULL, NULL, (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | IS_HARMLESS)))
		return;

	/* Rastrear objeto para el recuerdo de objetos */
	track_object(player->upkeep, obj);

	/* Mostrar información */
	tb = object_info(obj, OINFO_NONE);
	object_desc(header_buf, sizeof(header_buf), obj,
		ODESC_PREFIX | ODESC_FULL | ODESC_CAPITAL, player);

	textui_textblock_show(tb, local_area, header_buf);
	textblock_free(tb);
}


/**
 * ------------------------------------------------------------------------
 * Interfaz de ignorado de objetos
 * ------------------------------------------------------------------------ */

enum {
	IGNORE_THIS_ITEM,
	UNIGNORE_THIS_ITEM,
	IGNORE_THIS_FLAVOR,
	UNIGNORE_THIS_FLAVOR,
	IGNORE_THIS_EGO,
	UNIGNORE_THIS_EGO,
	IGNORE_THIS_QUALITY
};

void textui_cmd_ignore_menu(struct object *obj)
{
	char out_val[160];

	struct menu *m;
	region r;
	int selected;
	uint8_t value;
	int type;

	if (!obj)
		return;

	m = menu_dynamic_new();
	m->selections = all_letters_nohjkl;

	/* Opción básica de ignorar */
	if (!(obj->known->notice & OBJ_NOTICE_IGNORE)) {
		menu_dynamic_add(m, "Solo este objeto", IGNORE_THIS_ITEM);
	} else {
		menu_dynamic_add(m, "Dejar de ignorar este objeto", UNIGNORE_THIS_ITEM);
	}

	/* Ignorar por sabor */
	if (ignore_tval(obj->tval) &&
			(!obj->artifact || !object_flavor_is_aware(obj))) {
		bool ignored = kind_is_ignored_aware(obj->kind) ||
				kind_is_ignored_unaware(obj->kind);

		char tmp[70];
		object_desc(tmp, sizeof(tmp), obj,
			ODESC_NOEGO | ODESC_BASE | ODESC_PLURAL, player);
		if (!ignored) {
			strnfmt(out_val, sizeof out_val, "Todos los %s", tmp);
			menu_dynamic_add(m, out_val, IGNORE_THIS_FLAVOR);
		} else {
			strnfmt(out_val, sizeof out_val, "Dejar de ignorar todos los %s", tmp);
			menu_dynamic_add(m, out_val, UNIGNORE_THIS_FLAVOR);
		}
	}

	type = ignore_type_of(obj);

	/* Ignorar por égida */
	if (obj->known->ego && type != ITYPE_MAX) {
		struct ego_desc choice;
		struct ego_item *ego = obj->ego;
		char tmp[80] = "";

		choice.e_idx = ego->eidx;
		choice.itype = type;
		choice.short_name = "";
		(void) ego_item_name(tmp, sizeof(tmp), &choice);
		if (!ego_is_ignored(choice.e_idx, choice.itype)) {
			strnfmt(out_val, sizeof out_val, "Todos %s", tmp + 4);
			menu_dynamic_add(m, out_val, IGNORE_THIS_EGO);
		} else {
			strnfmt(out_val, sizeof out_val, "Dejar de ignorar todos %s", tmp + 4);
			menu_dynamic_add(m, out_val, UNIGNORE_THIS_EGO);
		}
	}

	/* Ignorar por calidad */
	value = ignore_level_of(obj);

	if (tval_is_jewelry(obj) &&	ignore_level_of(obj) != IGNORE_BAD)
		value = IGNORE_MAX;

	if (value != IGNORE_MAX && type != ITYPE_MAX) {
		strnfmt(out_val, sizeof out_val, "Todos los %s %s",
				quality_values[value].name, ignore_name_for_type(type));

		menu_dynamic_add(m, out_val, IGNORE_THIS_QUALITY);
	}

	/* Calcular región de visualización */
	r.width = menu_dynamic_longest_entry(m) + 3 + 2; /* +3 para etiqueta, +2 para relleno */
	r.col = 80 - r.width;
	r.row = 1;
	r.page_rows = m->count;

	screen_save();
	menu_layout(m, &r);
	region_erase_bordered(&r);

	prt("(Enter para seleccionar, ESC) Ignorar:", 0, 0);
	selected = menu_dynamic_select(m);

	screen_load();

	if (selected == IGNORE_THIS_ITEM) {
		obj->known->notice |= OBJ_NOTICE_IGNORE;
	} else if (selected == UNIGNORE_THIS_ITEM) {
		obj->known->notice &= ~(OBJ_NOTICE_IGNORE);
	} else if (selected == IGNORE_THIS_FLAVOR) {
		object_ignore_flavor_of(obj);
	} else if (selected == UNIGNORE_THIS_FLAVOR) {
		kind_ignore_clear(obj->kind);
	} else if (selected == IGNORE_THIS_EGO) {
		ego_ignore(obj);
	} else if (selected == UNIGNORE_THIS_EGO) {
		ego_ignore_clear(obj);
	} else if (selected == IGNORE_THIS_QUALITY) {
		uint8_t ignore_value = ignore_level_of(obj);
		int ignore_type = ignore_type_of(obj);

		ignore_level[ignore_type] = ignore_value;
	}

	player->upkeep->notice |= PN_IGNORE;

	menu_dynamic_free(m);
}

void textui_cmd_ignore(void)
{
	struct object *obj;

	/* Obtener un objeto */
	const char *q = "¿Ignorar qué objeto? ";
	const char *s = "No tienes nada que ignorar.";
	if (!get_item(&obj, q, s, CMD_IGNORE, NULL,
				  USE_INVEN | USE_QUIVER | USE_EQUIP | USE_FLOOR))
		return;

	textui_cmd_ignore_menu(obj);
}

void textui_cmd_toggle_ignore(void)
{
	player->unignoring = !player->unignoring;
	player->upkeep->notice |= PN_IGNORE;
	do_cmd_redraw();
}