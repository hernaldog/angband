/**
 * \file ui-store.c
 * \brief Interfaz de usuario de las tiendas
 *
 * Copyright (c) 1997 Robert A. Koeneke, James E. Wilson, Ben Harrison
 * Copyright (c) 1998-2014 Angband developers
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
#include "hint.h"
#include "init.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-util.h"
#include "store.h"
#include "target.h"
#include "ui-display.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-options.h"
#include "ui-knowledge.h"
#include "ui-object.h"
#include "ui-player.h"
#include "ui-spell.h"
#include "ui-command.h"
#include "ui-store.h"
#include "z-debug.h"


/**
 * Mensajes de bienvenida del tendero.
 *
 * El nombre del tendero debe ir primero, luego el nombre del personaje.
 */
static const char *comment_welcome[] =
{
	"",
	"%s te asiente con la cabeza.",
	"%s te saluda.",
	"%s: \"¿Ves algo que te guste, aventurero?\"",
	"%s: \"¿En qué puedo ayudarte, %s?\"",
	"%s: \"Bienvenido de nuevo, %s.\"",
	"%s: \"Un placer volver a verte, %s.\"",
	"%s: \"¿En qué puedo serte de ayuda, buen %s?\"",
	"%s: \"Honras mi humilde tienda, noble %s.\"",
	"%s: \"Mi familia y yo estamos a tu entera disposición, %s.\""
};

static const char *comment_hint[] =
{
/*	"%s te dice seriamente: \"%s\".", */
/*	"(%s) Hay un dicho por aquí, \"%s\".", */
/*	"%s se ofrece a contarte un secreto la próxima vez que estés cerca." */
	"\"%s\""
};


/**
 * Nombres fáciles para los elementos de las matrices 'scr_places'.
 */
enum
{
	LOC_PRICE = 0,
	LOC_OWNER,
	LOC_HEADER,
	LOC_MORE,
	LOC_HELP_CLEAR,
	LOC_HELP_PROMPT,
	LOC_AU,
	LOC_WEIGHT,

	LOC_MAX
};

/* Banderas de estado */
#define STORE_GOLD_CHANGE      0x01
#define STORE_FRAME_CHANGE     0x02
#define STORE_SHOW_HELP        0x04

/* Bandera compuesta para la visualización inicial de una tienda */
#define STORE_INIT_CHANGE		(STORE_FRAME_CHANGE | STORE_GOLD_CHANGE)

struct store_context {
	struct menu menu;			/* Instancia del menú */
	struct store *store;	/* Puntero a la tienda */
	struct object **list;	/* Lista de objetos (sin usar) */
	int flags;				/* Banderas de visualización */
	bool inspect_only;		/* Solo permitir mirar */

	/* Lugares para las diversas cosas mostradas en pantalla */
	unsigned int scr_places_x[LOC_MAX];
	unsigned int scr_places_y[LOC_MAX];
};

/* Devolver una pista aleatoria de la lista global de pistas */
static const char *random_hint(void)
{
	struct hint *v, *r = hints;
	int n;
	for (v = hints->next, n = 2; v; v = v->next, n++)
		if (one_in_(n))
			r = v;
	return r->hint;
}

/**
 * El saludo que el tendero da al personaje dice mucho sobre su
 * actitud general.
 *
 * Tomado y modificado de Sangband 1.0.
 *
 * Nótese que cada comment_hint debe tener exactamente un %s
 */
static void prt_welcome(const struct owner *proprietor)
{
	char short_name[20];
	const char *owner_name = proprietor->name;

	int j;

	if (one_in_(2))
		return;

	/* Obtener el nombre del tendero (detenerse antes del primer espacio) */
	for (j = 0; owner_name[j] && owner_name[j] != ' '; j++)
		short_name[j] = owner_name[j];

	/* Truncar el nombre */
	short_name[j] = '\0';

	if (hints && one_in_(3)) {
		size_t i = randint0(N_ELEMENTS(comment_hint));
		msg(comment_hint[i], random_hint());
	} else if (player->lev > 5) {
		const char *player_name;

		/* Vamos del nivel 1 al 50 */
		size_t i = ((unsigned)player->lev - 1) / 5;
		i = MIN(i, N_ELEMENTS(comment_welcome) - 1);

		/* Obtener un título para el personaje */
		if ((i % 2) && randint0(2))
			player_name = player->class->title[(player->lev - 1) / 5];
		else if (randint0(2))
			player_name = player->full_name;
		else
			player_name = "valioso cliente";

		/* Balthazar dice "Bienvenido" */
		prt(format(comment_welcome[i], short_name, player_name), 0, 0);
	}
}


/*** Código de visualización ***/


/**
 * Esta función configura las ubicaciones en pantalla basadas en el tamaño actual del terminal.
 *
 * Diseño de pantalla actual:
 *  línea 0: reservada para mensajes
 *  línea 1: tendero y su bolsa / precio de compra del objeto
 *  línea 2: vacía
 *  línea 3: encabezados de tabla
 *
 *  línea 4: Inicio de objetos
 *
 * Si la ayuda está desactivada, el resto de la pantalla es:
 *
 *  línea (altura - 4): fin de objetos
 *  línea (altura - 3): mensaje "más"
 *  línea (altura - 2): vacía
 *  línea (altura - 1): Mensaje de ayuda y oro restante
 *
 * Si la ayuda está activada, el resto de la pantalla es:
 *
 *  línea (altura - 7): fin de objetos
 *  línea (altura - 6): mensaje "más"
 *  línea (altura - 4): oro restante
 *  línea (altura - 3): ayuda de comandos 
 */
static void store_display_recalc(struct store_context *ctx)
{
	int wid, hgt;
	region loc;

	struct menu *m = &ctx->menu;
	struct store *store = ctx->store;

	Term_get_size(&wid, &hgt);

	/* Limitar el ancho a un máximo de 104 (suficiente espacio para un nombre de objeto de 80 caracteres) */
	if (wid > 104) wid = 104;

	/* Limitar la función text_out a dos menos que el ancho de la pantalla */
	text_out_wrap = wid - 2;


	/* Coordenadas X primero */
	ctx->scr_places_x[LOC_PRICE] = wid - 14;
	ctx->scr_places_x[LOC_AU] = wid - 26;
	ctx->scr_places_x[LOC_OWNER] = wid - 2;
	ctx->scr_places_x[LOC_WEIGHT] = wid - 14;

	/* Añadir espacio para precios */
	if (store->feat != FEAT_HOME)
		ctx->scr_places_x[LOC_WEIGHT] -= 10;

	/* Luego Y */
	ctx->scr_places_y[LOC_OWNER] = 1;
	ctx->scr_places_y[LOC_HEADER] = 3;

	/* Si estamos mostrando ayuda, hacer la altura más pequeña */
	if (ctx->flags & (STORE_SHOW_HELP))
		hgt -= 3;

	ctx->scr_places_y[LOC_MORE] = hgt - 3;
	ctx->scr_places_y[LOC_AU] = hgt - 1;

	loc = m->boundary;

	/* Si estamos mostrando la ayuda, ponerla con una línea de relleno */
	if (ctx->flags & (STORE_SHOW_HELP)) {
		ctx->scr_places_y[LOC_HELP_CLEAR] = hgt - 1;
		ctx->scr_places_y[LOC_HELP_PROMPT] = hgt;
		loc.page_rows = -5;
	} else {
		ctx->scr_places_y[LOC_HELP_CLEAR] = hgt - 2;
		ctx->scr_places_y[LOC_HELP_PROMPT] = hgt - 1;
		loc.page_rows = -2;
	}

	menu_layout(m, &loc);
}


/**
 * Redibujar una sola entrada de la tienda
 */
static void store_display_entry(struct menu *menu, int oid, bool cursor, int row,
								int col, int width)
{
	struct object *obj;
	int32_t x;
	uint32_t desc = ODESC_PREFIX;

	char o_name[80];
	char out_val[160];
	uint8_t colour;
	int16_t obj_weight;

	struct store_context *ctx = menu_priv(menu);
	struct store *store = ctx->store;
	assert(store);

	/* Obtener el objeto */
	obj = ctx->list[oid];

	/* Describir el objeto - conservando inscripciones en el hogar */
	if (store->feat == FEAT_HOME) {
		desc |= ODESC_FULL;
	} else {
		desc |= ODESC_FULL | ODESC_STORE;
	}
	object_desc(o_name, sizeof(o_name), obj, desc, player);

	/* Mostrar el objeto */
	c_put_str(obj->kind->base->attr, o_name, row, col);

	/* Mostrar pesos */
	colour = curs_attrs[CURS_KNOWN][(int)cursor];
	obj_weight = object_weight_one(obj);
	strnfmt(out_val, sizeof out_val, "%3d.%d lb", obj_weight / 10,
			obj_weight % 10);
	c_put_str(colour, out_val, row, ctx->scr_places_x[LOC_WEIGHT]);

	/* Describir un objeto (completamente) en una tienda */
	if (store->feat != FEAT_HOME) {
		/* Extraer el precio "mínimo" */
		x = price_item(store, obj, false, 1);

		/* Asegurarse de que el jugador puede pagarlo */
		if (player->au < x)
			colour = curs_attrs[CURS_UNKNOWN][(int)cursor];

		/* Dibujar el precio realmente */
		if (tval_can_have_charges(obj) && (obj->number > 1))
			strnfmt(out_val, sizeof out_val, "%9ld promedio", (long)x);
		else
			strnfmt(out_val, sizeof out_val, "%9ld    ", (long)x);

		c_put_str(colour, out_val, row, ctx->scr_places_x[LOC_PRICE]);
	}
}


/**
 * Mostrar tienda (después de limpiar la pantalla)
 */
static void store_display_frame(struct store_context *ctx)
{
	char buf[80];
	struct store *store = ctx->store;
	struct owner *proprietor = store->owner;

	/* Limpiar pantalla */
	Term_clear();

	/* El "Hogar" es especial */
	if (store->feat == FEAT_HOME) {
		/* Poner el nombre del propietario */
		put_str("Tu Hogar", ctx->scr_places_y[LOC_OWNER], 1);

		/* Etiquetar las descripciones de objetos */
		put_str("Inventario del Hogar", ctx->scr_places_y[LOC_HEADER], 1);

		/* Mostrar encabezado de peso */
		put_str("Peso", ctx->scr_places_y[LOC_HEADER],
				ctx->scr_places_x[LOC_WEIGHT] + 2);
	} else {
		/* Tiendas normales */
		const char *store_name = f_info[store->feat].name;
		const char *owner_name = proprietor->name;

		/* Poner el nombre del propietario */
		put_str(owner_name, ctx->scr_places_y[LOC_OWNER], 1);

		/* Mostrar el precio máximo en la tienda (encima de los precios) */
		strnfmt(buf, sizeof(buf), "%s (%ld)", store_name,
				(long)proprietor->max_cost);
		prt(buf, ctx->scr_places_y[LOC_OWNER],
			ctx->scr_places_x[LOC_OWNER] - strlen(buf));

		/* Etiquetar las descripciones de objetos */
		put_str("Inventario de la Tienda", ctx->scr_places_y[LOC_HEADER], 1);

		/* Mostrar etiqueta de peso */
		put_str("Peso", ctx->scr_places_y[LOC_HEADER],
				ctx->scr_places_x[LOC_WEIGHT] + 2);

		/* Etiquetar el precio de venta (en tiendas) */
		put_str("Precio", ctx->scr_places_y[LOC_HEADER], ctx->scr_places_x[LOC_PRICE] + 4);
	}
}


/**
 * Mostrar ayuda.
 */
static void store_display_help(struct store_context *ctx)
{
	struct store *store = ctx->store;
	int help_loc = ctx->scr_places_y[LOC_HELP_PROMPT];
	bool is_home = (store->feat == FEAT_HOME) ? true : false;

	/* Limpiar */
	clear_from(ctx->scr_places_y[LOC_HELP_CLEAR]);

	/* Preparar ganchos de ayuda */
	text_out_hook = text_out_to_screen;
	text_out_indent = 1;
	Term_gotoxy(1, help_loc);

	if (OPT(player, rogue_like_commands))
		text_out_c(COLOUR_L_GREEN, "x");
	else
		text_out_c(COLOUR_L_GREEN, "l");

	text_out(" examina");
	if (!ctx->inspect_only) {
		text_out(" y ");
		text_out_c(COLOUR_L_GREEN, "p");
		text_out(" (o ");
		text_out_c(COLOUR_L_GREEN, "g");
		text_out(")");

		if (is_home) text_out(" recoge");
		else text_out(" compra");
	}
	text_out(" un objeto. ");

	if (!ctx->inspect_only) {
		if (OPT(player, birth_no_selling) && !is_home) {
			text_out_c(COLOUR_L_GREEN, "d");
			text_out(" (o ");
			text_out_c(COLOUR_L_GREEN, "s");
			text_out(")");
			text_out(" da un objeto a la tienda a cambio de su identificación. Algunas varitas y báculos también se recargarán. ");
		} else {
			text_out_c(COLOUR_L_GREEN, "d");
			text_out(" (o ");
			text_out_c(COLOUR_L_GREEN, "s");
			text_out(")");
			if (is_home) text_out(" deja");
			else text_out(" vende");
			text_out(" un objeto de tu inventario. ");
		}
	}
	text_out_c(COLOUR_L_GREEN, "I");
	text_out(" inspecciona un objeto de tu inventario. ");

	text_out_c(COLOUR_L_GREEN, "ESC");
	if (!ctx->inspect_only)
		text_out(" sale del edificio.");
	else
		text_out(" sale de esta pantalla.");

	text_out_indent = 0;
}

/**
 * Decide qué partes de la visualización de la tienda redibujar. Llamado en
 * cambios de tamaño del terminal y el comando de redibujado.
 */
static void store_redraw(struct store_context *ctx)
{
	if (ctx->flags & (STORE_FRAME_CHANGE)) {
		store_display_frame(ctx);

		if (ctx->flags & STORE_SHOW_HELP)
			store_display_help(ctx);
		else
			prt("Presiona '?' para ayuda.", ctx->scr_places_y[LOC_HELP_PROMPT], 1);

		ctx->flags &= ~(STORE_FRAME_CHANGE);
	}

	if (ctx->flags & (STORE_GOLD_CHANGE)) {
		prt(format("Oro Restante: %9ld", (long)player->au),
				ctx->scr_places_y[LOC_AU], ctx->scr_places_x[LOC_AU]);
		ctx->flags &= ~(STORE_GOLD_CHANGE);
	}
}

static bool store_get_check(const char *prompt)
{
	struct keypress ch;

	/* Preguntar por ello */
	prt(prompt, 0, 0);

	/* Obtener una respuesta */
	ch = inkey();

	/* Borrar el mensaje */
	prt("", 0, 0);

	if (ch.code == ESCAPE) return (false);
	if (strchr("Nn", ch.code)) return (false);

	/* Éxito */
	return (true);
}

/*
 * Vender un objeto, o dejarlo si estamos en el hogar.
 */
static bool store_sell(struct store_context *ctx)
{
	int amt;
	int get_mode = USE_EQUIP | USE_INVEN | USE_FLOOR | USE_QUIVER;

	struct store *store = ctx->store;

	struct object *obj;
	struct object object_type_body = OBJECT_NULL;
	struct object *temp_obj = &object_type_body;

	char o_name[120];

	item_tester tester = NULL;

	const char *reject = "No tienes nada que quiera. ";
	const char *prompt = OPT(player, birth_no_selling) ? "¿Dar qué objeto? " : "¿Vender qué objeto? ";

	assert(store);

	/* Limpiar todos los mensajes actuales */
	msg_flag = false;
	prt("", 0, 0);

	if (store->feat == FEAT_HOME) {
		prompt = "¿Dejar qué objeto? ";
	} else {
		tester = store_will_buy_tester;
		get_mode |= SHOW_PRICES;
	}

	/* Obtener un objeto */
	player->upkeep->command_wrk = USE_INVEN;

	if (!get_item(&obj, prompt, reject, CMD_DROP, tester, get_mode))
		return false;

	/* No se pueden quitar objetos bloqueados */
	if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj)) {
		/* Ups */
		msg("Mmm, parece estar pegado.");

		/* No */
		return false;
	}

	/* Obtener una cantidad */
	amt = get_quantity(NULL, obj->number);

	/* Permitir que el usuario cancele */
	if (amt <= 0) return false;

	/* Obtener una copia del objeto que representa el número que se vende */
	object_copy_amt(temp_obj, obj, amt);

	if (!store_check_num(store, temp_obj)) {
		object_wipe(temp_obj);
		if (store->feat == FEAT_HOME)
			msg("Tu hogar está lleno.");
		else
			msg("No tengo espacio en mi tienda para guardarlo.");

		return false;
	}

	/* Obtener una descripción completa */
	object_desc(o_name, sizeof(o_name), temp_obj,
		ODESC_PREFIX | ODESC_FULL, player);

	/* Tienda real */
	if (store->feat != FEAT_HOME) {
		/* Extraer el valor de los objetos */
		int32_t price = price_item(store, temp_obj, true, amt);

		object_wipe(temp_obj);
		screen_save();

		/* Mostrar precio */
		if (!OPT(player, birth_no_selling))
			prt(format("Precio: %ld", (long)price), 1, 0);

		/* Confirmar venta */
		if (!store_get_check(format("%s %s? [ESC, cualquier otra tecla para aceptar]",
				OPT(player, birth_no_selling) ? "Dar" : "Vender", o_name))) {
			screen_load();
			return false;
		}

		screen_load();

		cmdq_push(CMD_SELL);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", amt);
	} else { /* El jugador está en el hogar */
		object_wipe(temp_obj);
		cmdq_push(CMD_STASH);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", amt);
	}

	/* Actualizar la visualización */
	ctx->flags |= STORE_GOLD_CHANGE;

	return true;
}



/**
 * Comprar un objeto de una tienda
 */
static bool store_purchase(struct store_context *ctx, int item, bool single)
{
	struct store *store = ctx->store;

	struct object *obj = ctx->list[item];
	struct object *dummy = NULL;

	char o_name[80];

	int amt, num;

	int32_t price;

	/* Limpiar todos los mensajes actuales */
	msg_flag = false;
	prt("", 0, 0);


	/*** Verificar si el jugador puede obtener alguno en absoluto ***/

	/* Obtener una cantidad si no se nos dio una */
	if (single) {
		amt = 1;

		/* Verificar si el jugador puede pagar alguno en absoluto */
		if (store->feat != FEAT_HOME &&
				player->au < price_item(store, obj, false, 1)) {
			msg("No tienes suficiente oro para este objeto.");
			return false;
		}
	} else {
		bool flavor_aware;

		if (store->feat == FEAT_HOME) {
			amt = obj->number;
		} else {
			/* Precio de uno */
			price = price_item(store, obj, false, 1);

			/* Verificar si el jugador puede pagar alguno en absoluto */
			if ((uint32_t)player->au < (uint32_t)price) {
				msg("No tienes suficiente oro para este objeto.");
				return false;
			}

			/* Calcular cuántos puede pagar el jugador */
			if (price == 0)
				amt = obj->number; /* Prevenir división por cero */
			else
				amt = player->au / price;

			if (amt > obj->number) amt = obj->number;

			/* Doble verificación para varitas/báculos */
			if ((player->au >= price_item(store, obj, false, amt+1)) &&
				(amt < obj->number))
				amt++;
		}

		/* Limitar al número que se puede llevar */
		amt = MIN(amt, inven_carry_num(player, obj));

		/* Fallar si no hay espacio. No filtrar información sobre
		 * sabores desconocidos para una compra (sacarlo del hogar no
		 * filtra información ya que no muestra el verdadero sabor). */
		flavor_aware = object_flavor_is_aware(obj);
		if (amt <= 0 || (!flavor_aware && store->feat != FEAT_HOME &&
				pack_is_full())) {
			msg("No puedes llevar tantos objetos.");
			return false;
		}

		/* Encontrar el número de este objeto en el inventario. Como arriba,
		 * evitar filtrar información sobre sabores desconocidos. */
		if (!flavor_aware && store->feat != FEAT_HOME)
			num = 0;
		else
			num = find_inven(obj);

		strnfmt(o_name, sizeof o_name, "%s cuántos%s? (máx %d) ",
				(store->feat == FEAT_HOME) ? "Coger" : "Comprar",
				num ? format(" (tienes %d)", num) : "", amt);

		/* Obtener una cantidad */
		amt = get_quantity(o_name, amt);

		/* Permitir que el usuario cancele */
		if (amt <= 0) return false;
	}

	/* Obtener el objeto deseado */
	dummy = object_new();
	object_copy_amt(dummy, obj, amt);

	/* Asegurarse de que tenemos espacio */
	if (!inven_carry_okay(dummy)) {
		msg("No puedes llevar tantos objetos.");
		object_delete(NULL, NULL, &dummy);
		return false;
	}

	/* Intentar comprarlo */
	if (store->feat != FEAT_HOME) {
		bool response;

		bool obj_is_book = tval_is_book_k(obj->kind);
		bool obj_can_use = !obj_is_book || obj_can_browse(obj);

		/* Describir el objeto (completamente) */
		object_desc(o_name, sizeof(o_name), dummy,
			ODESC_PREFIX | ODESC_FULL | ODESC_STORE, player);

		/* Extraer el precio para todo el montón */
		price = price_item(store, dummy, false, dummy->number);

		screen_save();

		/* Mostrar precio */
		prt(format("Precio: %ld", (long)price), 1, 0);

		/* Confirmar compra */
		response = store_get_check(format("¿Comprar %s?%s %s",
					o_name,
					obj_can_use ? "" : " (¡No puedes usar!)",
					"[ESC, cualquier otra tecla para aceptar]"));

		screen_load();

		/* Respuesta negativa, así que rendirse */
		if (!response) return false;

		cmdq_push(CMD_BUY);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", amt);
	} else {
		/* El hogar es mucho más fácil */
		cmdq_push(CMD_RETRIEVE);
		cmd_set_arg_item(cmdq_peek(), "item", obj);
		cmd_set_arg_number(cmdq_peek(), "quantity", amt);
	}

	/* Actualizar la visualización */
	ctx->flags |= STORE_GOLD_CHANGE;

	object_delete(NULL, NULL, &dummy);

	/* No expulsado */
	return true;
}


/**
 * Examinar un objeto en una tienda
 */
static void store_examine(struct store_context *ctx, int item)
{
	struct object *obj;
	char header[120];
	textblock *tb;
	region area = { 0, 0, 0, 0 };
	uint32_t odesc_flags = ODESC_PREFIX | ODESC_FULL;

	if (item < 0) return;

	/* Obtener el objeto real */
	obj = ctx->list[item];

	/* Los objetos en el hogar obtienen menos descripción */
	if (ctx->store->feat == FEAT_HOME) {
		odesc_flags |= ODESC_CAPITAL;
	} else {
		odesc_flags |= ODESC_STORE;
	}

	/* No se necesita vaciado */
	msg_flag = false;

	/* Mostrar información completa en la mayoría de las tiendas, pero información normal en el hogar */
	tb = object_info(obj, OINFO_NONE);
	object_desc(header, sizeof(header), obj, odesc_flags, player);

	textui_textblock_show(tb, area, header);
	textblock_free(tb);

	/* Examinar libro, luego preguntar por un comando */
	if (obj_can_browse(obj))
		textui_book_browse(obj);
}


static void store_menu_set_selections(struct menu *menu, bool knowledge_menu)
{
	if (knowledge_menu) {
		if (OPT(player, rogue_like_commands)) {
			/* ¡Estos dos no pueden intersecar! */
			menu->cmd_keys = "?|Ieilx";
			menu->selections = "abcdfghmnopqrstuvwyzABCDEFGHJKLMNOPQRSTUVWXYZ";
		} else {
			/* ¡Estos dos no pueden intersecar! */
			menu->cmd_keys = "?|Ieil";
			menu->selections = "abcdfghjkmnopqrstuvwxyzABCDEFGHJKLMNOPQRSTUVWXYZ";
		}
	} else {
		if (OPT(player, rogue_like_commands)) {
			/* ¡Estos dos no pueden intersecar! */
			menu->cmd_keys = "\x04\x05\x10?={|}~CEIPTdegilpswx"; /* \x10 = ^p , \x04 = ^D, \x05 = ^E */
			menu->selections = "abcfmnoqrtuvyzABDFGHJKLMNOQRSUVWXYZ";
		} else {
			/* ¡Estos dos no pueden intersecar! */
			menu->cmd_keys = "\x05\x010?={|}~CEIbdegiklpstwx"; /* \x05 = ^E, \x10 = ^p */
			menu->selections = "acfhjmnoqruvyzABDFGHJKLMNOPQRSTUVWXYZ";
		}
	}
}

static void store_menu_recalc(struct menu *m)
{
	struct store_context *ctx = menu_priv(m);
	menu_setpriv(m, ctx->store->stock_num, ctx);
}

/**
 * Procesar un comando en una tienda
 *
 * Nótese que debemos permitir el uso de algunos comandos "especiales" en las tiendas
 * que no están permitidos en la mazmorra, y debemos deshabilitar algunos comandos
 * que están permitidos en la mazmorra pero no en las tiendas, para evitar el caos.
 */
static bool store_process_command_key(struct keypress kp)
{
	int cmd = 0;

	/* No se necesita vaciado */
	prt("", 0, 0);
	msg_flag = false;

	/* Procesar el código de tecla */
	switch (kp.code) {
		case 'T': /* roguelike */
		case 't': cmd = CMD_TAKEOFF; break;

		case KTRL('D'): /* roguelike */
		case 'k': textui_cmd_ignore(); break;

		case 'P': /* roguelike */
		case 'b': textui_spell_browse(); break;

		case '~': textui_browse_knowledge(); break;
		case 'I': textui_obj_examine(); break;
		case 'w': cmd = CMD_WIELD; break;
		case '{': cmd = CMD_INSCRIBE; break;
		case '}': cmd = CMD_UNINSCRIBE; break;

		case 'e': do_cmd_equip(); break;
		case 'i': do_cmd_inven(); break;
		case '|': do_cmd_quiver(); break;
		case KTRL('E'): toggle_inven_equip(); break;
		case 'C': do_cmd_change_name(); break;
		case KTRL('P'): do_cmd_messages(); break;
		case ')': do_cmd_save_screen(); break;

		default: return false;
	}

	if (cmd)
		cmdq_push_repeat(cmd, 0);

	return true;
}

/**
 * Seleccionar un objeto del inventario de la tienda y devolver el índice del inventario
 */
static int store_get_stock(struct menu *m, int oid)
{
	ui_event e;
	int no_act = m->flags & MN_NO_ACTION;

	/* Establecer una bandera para asegurarnos de que obtenemos la selección o escape
	 * sin ejecutar el manejador del menú */
	m->flags |= MN_NO_ACTION;
	e = menu_select(m, 0, true);
	if (!no_act) {
		m->flags &= ~MN_NO_ACTION;
	}

	if (e.type == EVT_SELECT) {
		return m->cursor;
	} else if (e.type == EVT_ESCAPE) {
		return -1;
	}

	/* si no tenemos una nueva selección, simplemente devolver el objeto original */
	return oid;
}

/** Enum para entradas del menú contextual */
enum {
	ACT_INSPECT_INVEN,
	ACT_SELL,
	ACT_EXAMINE,
	ACT_BUY,
	ACT_BUY_ONE,
	ACT_EXIT
};

/* Elegir las opciones del menú contextual apropiadas para una tienda */
static int context_menu_store(struct store_context *ctx, const int oid, int mx, int my)
{
	struct store *store = ctx->store;
	bool home = (store->feat == FEAT_HOME) ? true : false;

	struct menu *m = menu_dynamic_new();

	int selected;
	char *labels = string_make(lower_case);
	m->selections = labels;

	menu_dynamic_add_label(m, "Inspeccionar inventario", 'I', ACT_INSPECT_INVEN, labels);
	if (!ctx->inspect_only) {
		menu_dynamic_add_label(m, home ? "Guardar" : "Vender", 'd',
			ACT_SELL, labels);
	}
	menu_dynamic_add_label(m, "Salir", '`', ACT_EXIT, labels);

	/* No se necesita vaciado */
	msg_flag = false;
	screen_save();

	menu_dynamic_calc_location(m, mx, my);
	region_erase_bordered(&m->boundary);

	prt("(Enter seleccionar, ESC) Comando:", 0, 0);
	selected = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	switch (selected) {
		case ACT_SELL:
			store_sell(ctx);
			break;
		case ACT_INSPECT_INVEN:
			textui_obj_examine();
			break;
		case ACT_EXIT:
			return false;
	}

	return true;
}

/* Hacer que 'g' sea un sinónimo de 'p' para el menú contextual de un objeto en la tienda. */
static bool handle_g_context_store_item(struct menu *menu,
		const ui_event *event, int oid)
{
	if (event->type == EVT_KBRD && event->key.code == 'g') {
		ui_event mod_event, out_event;

		mod_event = *event;
		mod_event.key.code = 'p';
		return menu_handle_keypress(menu, &mod_event, &out_event);
	}
	return false;
}

/* Elegir las opciones del menú contextual apropiadas para un objeto disponible en una tienda */
static bool context_menu_store_item(struct store_context *ctx, const int oid, int mx, int my)
{
	struct store *store = ctx->store;
	bool home = (store->feat == FEAT_HOME) ? true : false;

	struct menu *m = menu_dynamic_new();
	struct object *obj = ctx->list[oid];
	menu_iter mod_iter;
	int selected;
	char *labels;
	char header[120];

	object_desc(header, sizeof(header), obj,
		ODESC_PREFIX | ODESC_FULL | ((home) ? 0 : ODESC_STORE), player);

	labels = string_make(lower_case);
	m->selections = labels;

	menu_dynamic_add_label(m, "Examinar", (OPT(player, rogue_like_commands))
		? 'x' : 'l', ACT_EXAMINE, labels);
	if (!ctx->inspect_only) {
		menu_dynamic_add_label(m, home ? "Coger" : "Comprar", 'p',
			ACT_BUY, labels);
		if (obj->number > 1) {
			menu_dynamic_add_label(m, home ? "Coger uno" : "Comprar uno",
				'o', ACT_BUY_ONE, labels);
		}
		/*
		 * Esto es un pequeño truco para que 'g' actúe como 'p' (como lo hace cuando
		 * no hay un objeto seleccionado). Debe hacerse después de que todas las
		 * etiquetas hayan sido añadidas para evitar fallos de aserción.
		 */
		mod_iter = *m->row_funcs;
		mod_iter.row_handler = handle_g_context_store_item;
		m->row_funcs = &mod_iter;
		m->switch_keys = "g";
	}

	/* No se necesita vaciado */
	msg_flag = false;
	screen_save();

	menu_dynamic_calc_location(m, mx, my);
	region_erase_bordered(&m->boundary);

	prt(format("(Enter seleccionar, ESC) Comando para %s:", header), 0, 0); /* tienda piso 1 */
	selected = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	switch (selected) {
		case ACT_EXAMINE:
			store_examine(ctx, oid);
			return false;
		case ACT_BUY:
			return store_purchase(ctx, oid, false);
		case ACT_BUY_ONE:
			return store_purchase(ctx, oid, true);
	}
	return false;
}

/**
 * Manejar la entrada del menú de la tienda
 */
static bool store_menu_handle(struct menu *m, const ui_event *event, int oid)
{
	bool processed = true;
	struct store_context *ctx = menu_priv(m);
	struct store *store = ctx->store;
	
	if (event->type == EVT_SELECT) {
		/* TRUCO: no hay coordenadas de evento de ratón para usar en */
		/* menu_store_item, así que fingir una como si el ratón hubiera hecho clic en la letra */
		bool purchased = context_menu_store_item(ctx, oid, 1, m->active.row + oid);
		ctx->flags |= (STORE_FRAME_CHANGE | STORE_GOLD_CHANGE);

		/* Dejar que el juego maneje cualquier comando central (equipar, etc.) */
		cmdq_pop(CTX_STORE);

		/* Notar y manejar cosas */
		notice_stuff(player);
		handle_stuff(player);

		if (purchased) {
			/* Mostrar la tienda */
			store_display_recalc(ctx);
			store_menu_recalc(m);
			store_redraw(ctx);
		}

		return true;
	} else if (event->type == EVT_MOUSE) {
		if (event->mouse.button == 2) {
			/* ¿salir de la tienda? ¿qué hace esto ya? menu_handle_mouse
			 * así que salir de esto para que menu_handle_mouse sea llamado */
			return false;
		} else if (event->mouse.button == 1) {
			bool action = false;
			if ((event->mouse.y == 0) || (event->mouse.y == 1)) {
				/* mostrar el menú contextual de la tienda */
				if (context_menu_store(ctx, oid, event->mouse.x, event->mouse.y) == false)
					return false;

				action = true;
			} else if ((oid >= 0) && (event->mouse.y == m->active.row + oid)) {
				/* si la pulsación está en un elemento de la lista, así que contexto del objeto de la tienda */
				context_menu_store_item(ctx, oid, event->mouse.x,
										event->mouse.y);
				action = true;
			}

			if (action) {
				ctx->flags |= (STORE_FRAME_CHANGE | STORE_GOLD_CHANGE);

				/* Dejar que el juego maneje cualquier comando central (equipar, etc.) */
				cmdq_pop(CTX_STORE);

				/* Notar y manejar cosas */
				notice_stuff(player);
				handle_stuff(player);

				/* Mostrar la tienda */
				store_display_recalc(ctx);
				store_menu_recalc(m);
				store_redraw(ctx);

				return true;
			}
		}
	} else if (event->type == EVT_KBRD) {
		switch (event->key.code) {
			case 's':
			case 'd': store_sell(ctx); break;

			case 'p':
			case 'g':
				/* usar la forma antigua de comprar objetos */
				msg_flag = false;
				if (store->feat != FEAT_HOME) {
					prt("¿Comprar qué objeto? (ESC para cancelar, Enter seleccionar)",
						0, 0);
				} else {
					prt("¿Coger qué objeto? (ESC cancelar, Enter seleccionar)",
						0, 0);
				}
				oid = store_get_stock(m, oid);
				prt("", 0, 0);
				if (oid >= 0) {
					store_purchase(ctx, oid, false);
				}
				break;
			case 'l':
			case 'x':
				/* usar la forma antigua de examinar objetos */
				msg_flag = false;
				prt("¿Examinar qué objeto? (ESC cancelar, Enter seleccionar)",
					0, 0);
				oid = store_get_stock(m, oid);
				prt("", 0, 0);
				if (oid >= 0) {
					store_examine(ctx, oid);
				}
				break;

			case '?': {
				/* Alternar ayuda */
				if (ctx->flags & STORE_SHOW_HELP)
					ctx->flags &= ~(STORE_SHOW_HELP);
				else
					ctx->flags |= STORE_SHOW_HELP;

				/* Redibujar */
				ctx->flags |= STORE_INIT_CHANGE;

				store_display_recalc(ctx);
				store_redraw(ctx);

				break;
			}

			case '=': {
				do_cmd_options();
				store_menu_set_selections(m, false);
				break;
			}

			default:
				processed = store_process_command_key(event->key);
		}

		/* Dejar que el juego maneje cualquier comando central (equipar, etc.) */
		cmdq_pop(CTX_STORE);

		if (processed) {
			event_signal(EVENT_INVENTORY);
			event_signal(EVENT_EQUIPMENT);
		}

		/* Notar y manejar cosas */
		notice_stuff(player);
		handle_stuff(player);

		return processed;
	}

	return false;
}

static region store_menu_region = { 1, 4, -1, -2 };
static const menu_iter store_menu =
{
	NULL,
	NULL,
	store_display_entry,
	store_menu_handle,
	NULL
};

/**
 * Iniciar el menú de la tienda
 */
static void store_menu_init(struct store_context *ctx, struct store *store, bool inspect_only)
{
	struct menu *menu = &ctx->menu;

	ctx->store = store;
	ctx->flags = STORE_INIT_CHANGE;
	ctx->inspect_only = inspect_only;
	ctx->list = mem_zalloc(sizeof(struct object *) * z_info->store_inven_max);

	store_stock_list(ctx->store, ctx->list, z_info->store_inven_max);

	/* Iniciar la estructura del menú */
	menu_init(menu, MN_SKIN_SCROLL, &store_menu);
	menu_setpriv(menu, 0, ctx);

	/* Calcular las posiciones de las cosas y dibujar */
	menu_layout(menu, &store_menu_region);
	store_menu_set_selections(menu, inspect_only);
	store_display_recalc(ctx);
	store_menu_recalc(menu);
	store_redraw(ctx);
}

/**
 * Mostrar contenido de una tienda desde el menú de conocimiento
 *
 * Las únicas acciones permitidas son 'I' para inspeccionar un objeto
 */
void textui_store_knowledge(int n)
{
	struct store_context ctx;

	screen_save();
	clear_from(0);

	store_menu_init(&ctx, &stores[n], true);
	menu_select(&ctx.menu, 0, false);

	/* Vaciar mensajes XXX XXX XXX */
	event_signal(EVENT_MESSAGE_FLUSH);

	screen_load();

	mem_free(ctx.list);
}


/**
 * Manejar cambio de inventario.
 */
static void refresh_stock(game_event_type type, game_event_data *unused, void *user)
{
	struct store_context *ctx = user;
	struct menu *menu = &ctx->menu;

	store_stock_list(ctx->store, ctx->list, z_info->store_inven_max);

	/* Mostrar la tienda */
	store_display_recalc(ctx);
	store_menu_recalc(menu);
	store_redraw(ctx);
}

/**
 * Entrar a una tienda.
 */
void enter_store(game_event_type type, game_event_data *data, void *user)
{
	struct store *store = store_at(cave, player->grid);

	/* Verificar que estamos en una tienda */
	if (!store) {
		msg("No ves ninguna tienda aquí.");
		return;
	}

	sound((store->feat == FEAT_HOME) ? MSG_STORE_HOME : MSG_STORE_ENTER);

	/* Apagar la vista normal del juego */
	event_signal(EVENT_LEAVE_WORLD);
}

/**
 * Interactuar con una tienda.
 */
void use_store(game_event_type type, game_event_data *data, void *user)
{
	struct store *store = store_at(cave, player->grid);
	struct store_context ctx;

	/* Verificar que estamos en una tienda */
	if (!store) return;

	/*** Visualización ***/

	/* Guardar pantalla actual (ej. mazmorra) */
	screen_save();
	msg_flag = false;

	/* Obtener una versión matricial del inventario de la tienda, registrar manejador para cambios */
	event_add_handler(EVENT_STORECHANGED, refresh_stock, &ctx);
	store_menu_init(&ctx, store, false);

	/* Decir un saludo amistoso. */
	if (store->feat != FEAT_HOME)
		prt_welcome(store->owner);

	/* Compras */
	menu_select(&ctx.menu, 0, false);

	/* Compras terminadas */
	event_remove_handler(EVENT_STORECHANGED, refresh_stock, &ctx);
	msg_flag = false;
	mem_free(ctx.list);

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Vaciar mensajes */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Cargar la pantalla */
	screen_load();
}

void leave_store(game_event_type type, game_event_data *data, void *user)
{
	/* Deshabilitar repeticiones */
	cmd_disable_repeat();

	sound(MSG_STORE_LEAVE);

	/* Volver a la vista normal del juego. */
	event_signal(EVENT_ENTER_WORLD);

	/* Actualizar los visuales */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Redibujar toda la pantalla */
	player->upkeep->redraw |= (PR_BASIC | PR_EXTRA);

	/* Redibujar mapa */
	player->upkeep->redraw |= (PR_MAP);
}