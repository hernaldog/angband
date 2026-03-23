/**
 * \archivo store.c
 * \brief Abastecimiento de tiendas
 *
 * Copyright (c) 1997 Robert A. Koeneke, James E. Wilson, Ben Harrison
 * Copyright (c) 2007 Andi Sidwell
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
#include "game-world.h"
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
#include "obj-power.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-history.h"
#include "player-spell.h"
#include "store.h"
#include "target.h"
#include "debug.h"


static void store_maint(struct store *s);

/**
 * ------------------------------------------------------------------------
 * Constantes y definiciones
 * ------------------------------------------------------------------------ */


/**
 * Array[z_info->store_max] de tiendas
 */
struct store *stores;

/**
 * El array de pistas
 */
struct hint *hints;


static const char *obj_flags[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

/**
 * Devuelve la instancia de la tienda en la ubicación dada
 */
struct store *store_at(struct chunk *c, struct loc grid)
{
	if (square_isshop(c, grid))
		return &stores[square_shopnum(c, grid)];

	return NULL;
}


/**
 * Elimina las tiendas al limpiar. Elimina todo.
 */
static void cleanup_stores(void)
{
	struct owner *o, *o_next;
	struct object_buy *buy, *buy_next;
	int i;

	if (!stores)
		return;

	/* Liberar los inventarios de las tiendas */
	for (i = 0; i < z_info->store_max; i++) {
		/* Obtener la tienda */
		struct store *store = &stores[i];

		/* Liberar el inventario de la tienda */
		object_pile_free(NULL, NULL, store->stock_k);
		object_pile_free(NULL, NULL, store->stock);
		mem_free(store->always_table);
		mem_free(store->normal_table);

		for (o = store->owners; o; o = o_next) {
			o_next = o->next;
			string_free(o->name);
			mem_free(o);
		}

		for (buy = store->buy; buy; buy = buy_next) {
			buy_next = buy->next;
			mem_free(buy);
		}
	}
	mem_free(stores);
}


/**
 * ------------------------------------------------------------------------
 * Análisis del archivo de edición
 * ------------------------------------------------------------------------ */


/** store.txt **/

static enum parser_error parse_store(struct parser *p) {
	int feat = lookup_feat_code(parser_getstr(p, "feat"));
	struct store *s;

	if (feat < 0 || !tf_has(f_info[feat].flags, TF_SHOP)) {
		return PARSE_ERROR_INVALID_VALUE;
	}

	assert(f_info[feat].shopnum >= 1
		&& f_info[feat].shopnum <= z_info->store_max);
	s = &stores[f_info[feat].shopnum - 1];
	s->feat = feat;
	s->stock_size = z_info->store_inven_max;
	parser_setpriv(p, s);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_slots(struct parser *p) {
	struct store *s = parser_priv(p);
	s->normal_stock_min = parser_getuint(p, "min");
	s->normal_stock_max = parser_getuint(p, "max");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_turnover(struct parser *p) {
	struct store *s = parser_priv(p);
	s->turnover = parser_getuint(p, "turnover");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_normal(struct parser *p) {
	struct store *s = parser_priv(p);
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	int sval = lookup_sval(tval, parser_getsym(p, "sval"));

	struct object_kind *kind = lookup_kind(tval, sval);
	if (!kind)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	/* Expandir si es necesario */
	if (!s->normal_num) {
		s->normal_size = 16;
		s->normal_table = mem_zalloc(s->normal_size * sizeof *s->normal_table);
	} else if (s->normal_num >= s->normal_size) {
		s->normal_size += 8; 
		s->normal_table = mem_realloc(s->normal_table, s->normal_size * sizeof *s->normal_table);
	}

	s->normal_table[s->normal_num++] = kind;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_always(struct parser *p) {
	struct store *s = parser_priv(p);
	int tval = tval_find_idx(parser_getsym(p, "tval"));
	struct object_kind *kind = NULL;

	/* Mayormente se dan svals, pero se necesita un manejo especial para los libros */
	if (parser_hasval(p, "sval")) {
		int sval = lookup_sval(tval, parser_getsym(p, "sval"));
		kind = lookup_kind(tval, sval);
		if (!kind) {
			return PARSE_ERROR_UNRECOGNISED_SVAL;
		}

		/* Expandir si es necesario */
		if (!s->always_num) {
			s->always_size = 8;
			s->always_table = mem_zalloc(s->always_size * sizeof *s->always_table);
		} else if (s->always_num >= s->always_size) {
			s->always_size += 8;
			s->always_table = mem_realloc(s->always_table, s->always_size * sizeof *s->always_table);
		}

		s->always_table[s->always_num++] = kind;
	} else {
		/* Libros */
		struct object_base *book_base = &kb_info[tval];
		int i;

		/* Recorrer todos los libros para este tipo, añadir los libros de la ciudad */
		for (i = 1; i <= book_base->num_svals; i++) {
			const struct class_book *book = NULL;
			kind = lookup_kind(tval, i);
			book = object_kind_to_book(kind);
			if (!book->dungeon) {
				/* Expandir si es necesario */
				if (!s->always_num) {
					s->always_size = 8;
					s->always_table = mem_zalloc(s->always_size * sizeof *s->always_table);
				} else if (s->always_num >= s->always_size) {
					s->always_size += 8;
					s->always_table = mem_realloc(s->always_table, s->always_size * sizeof *s->always_table);
				}

				s->always_table[s->always_num++] = kind;
			}
		}
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_owner(struct parser *p) {
	struct store *s = parser_priv(p);
	unsigned int maxcost = parser_getuint(p, "purse");
	char *name = string_make(parser_getstr(p, "name"));
	struct owner *o;

	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	o = mem_zalloc(sizeof *o);
	o->oidx = (s->owners ? s->owners->oidx + 1 : 0);
	o->next = s->owners;
	o->name = name;
	o->max_cost = maxcost;
	s->owners = o;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_buy(struct parser *p) {
	struct store *s = parser_priv(p);
	struct object_buy *buy;

	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	buy = mem_zalloc(sizeof(*buy));
	buy->tval = tval_find_idx(parser_getstr(p, "base"));
	buy->next = s->buy;
	s->buy = buy;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_buy_flag(struct parser *p) {
	struct store *s = parser_priv(p);
	int flag;

	if (!s)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	flag = lookup_flag(obj_flags, parser_getsym(p, "flag"));

	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	} else {
		struct object_buy *buy = mem_zalloc(sizeof(*buy));

		buy->flag = flag;
		buy->tval = tval_find_idx(parser_getstr(p, "base"));
		buy->next = s->buy;
		s->buy = buy;

		return PARSE_ERROR_NONE;
	}
}

struct parser *init_parse_stores(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "store str feat", parse_store);
	parser_reg(p, "owner uint purse str name", parse_owner);
	parser_reg(p, "slots uint min uint max", parse_slots);
	parser_reg(p, "turnover uint turnover", parse_turnover);
	parser_reg(p, "normal sym tval sym sval", parse_normal);
	parser_reg(p, "always sym tval ?sym sval", parse_always);
	parser_reg(p, "buy str base", parse_buy);
	parser_reg(p, "buy-flag sym flag str base", parse_buy_flag);
	/*
	 * El número de tiendas se conoce desde terrain.txt, así que se asigna el
	 * array de tiendas aquí y se completan los detalles al analizar.
	 */
	stores = mem_zalloc(z_info->store_max * sizeof(*stores));
	return p;
}

static errr run_parse_stores(struct parser *p) {
	return parse_file_quit_not_found(p, "store");
}

static errr finish_parse_stores(struct parser *p) {
	parser_destroy(p);
	return 0;
}

static struct file_parser store_parser = {
	"store",
	init_parse_stores,
	run_parse_stores,
	finish_parse_stores,
	NULL
};


/**
 * ------------------------------------------------------------------------
 * Otras cosas de inicialización
 * ------------------------------------------------------------------------ */


void store_init(void)
{
	event_signal_message(EVENT_INITSTATUS, 0, "Inicializando tiendas...");
	if (run_parser(&store_parser)) quit("No se pueden inicializar las tiendas");
}

void store_reset(void) {
	int i, j;
	struct store *s;

	for (i = 0; i < z_info->store_max; i++) {
		s = &stores[i];
		s->stock_num = 0;
		store_shuffle(s);
		object_pile_free(NULL, NULL, s->stock_k);
		object_pile_free(NULL, NULL, s->stock);
		s->stock_k = NULL;
		s->stock = NULL;
		if (s->feat == FEAT_HOME)
			continue;
		for (j = 0; j < 10; j++)
			store_maint(s);
	}
}


struct init_module store_module = {
	.name = "store",
	.init = store_init,
	.cleanup = cleanup_stores
};





/**
 * Comprueba si un tipo de objeto dado es un artículo siempre disponible.
 */
static bool store_is_staple(struct store *s, struct object_kind *k) {
	size_t i;

	assert(s);
	assert(k);

	for (i = 0; i < s->always_num; i++) {
		struct object_kind *l = s->always_table[i];
		if (k == l)
			return true;
	}

	return false;
}

/**
 * Comprueba si un tipo de objeto dado es un artículo siempre disponible o a veces disponible.
 */
static bool store_can_carry(struct store *store, struct object_kind *kind) {
	size_t i;

	for (i = 0; i < store->normal_num; i++) {
		if (store->normal_table[i] == kind)
			return true;
	}

	return store_is_staple(store, kind);
}

/**
 * Comprueba si la venta de un objeto debe reducir el stock.
 */
static bool store_sale_should_reduce_stock(struct store *store,
		struct object *obj)
{
	if (obj->artifact || obj->ego) return true;
	if (tval_is_weapon(obj) && (obj->to_h || obj->to_d))
		return true;
	if (tval_is_armor(obj) && obj->to_a) return true;
	return !store_is_staple(store, obj->kind);
}


/**
 * ------------------------------------------------------------------------
 * Utilidades
 * ------------------------------------------------------------------------ */


/* Seleccionar aleatoriamente una de las entradas de un array */
#define ONE_OF(x)	x[randint0(N_ELEMENTS(x))]


/**
 * ------------------------------------------------------------------------
 * Texto de sabor
 * ------------------------------------------------------------------------ */


/**
 * Mensajes para reaccionar a los precios de compra.
 */
static const char *comment_worthless[] =
{
	"¡Arrgghh!",
	"¡Bastardo!",
	"Oyes a alguien sollozando...",
	"¡El tendero aúlla de agonía!",
	"¡El tendero se lamenta con angustia!",
	"El tendero golpea su cabeza contra el mostrador."
};

static const char *comment_bad[] =
{
	"¡Maldición!",
	"¡Qué demonio!",
	"El tendero te maldice.",
	"El tendero te fulmina con la mirada."
};

static const char *comment_accept[] =
{
	"De acuerdo.",
	"Bien.",
	"¡Aceptado!",
	"¡Acordado!",
	"¡Hecho!",
	"¡Tomado!"
};

static const char *comment_good[] =
{
	"¡Genial!",
	"¡Has hecho mi día!",
	"El tendero ríe entre dientes.",
	"El tendero ríe tontamente.",
	"El tendero ríe a carcajadas."
};

static const char *comment_great[] =
{
	"¡Yupi!",
	"¡Creo que me voy a jubilar!",
	"El tendero salta de alegría.",
	"El tendero sonríe alegremente.",
	"Vaya. Voy a ponerle tu nombre a mi nueva villa en tu honor."
};





/**
 * Permitir que un tendero reaccione a una compra
 *
 * Pagamos "price", valía "value", y pensamos que valía "guess"
 */
static void purchase_analyze(int price, int value, int guess)
{
	/* El objeto no valía nada, pero lo compramos */
	if ((value <= 0) && (price > value))
		msgt(MSG_STORE1, "%s", ONE_OF(comment_worthless));

	/* El objeto era más barato de lo que pensábamos, y pagamos más de lo necesario */
	else if ((value < guess) && (price > value))
		msgt(MSG_STORE2, "%s", ONE_OF(comment_bad));

	/* El objeto era una buena ganga, y nos salimos con la nuestra */
	else if ((value > guess) && (value < (4 * guess)) && (price < value))
		msgt(MSG_STORE3, "%s", ONE_OF(comment_good));

	/* El objeto era una gran ganga, y nos salimos con la nuestra */
	else if ((value > guess) && (price < value))
		msgt(MSG_STORE4, "%s", ONE_OF(comment_great));
}




/**
 * ------------------------------------------------------------------------
 * Comprobar si una tienda comprará un objeto
 * ------------------------------------------------------------------------ */


/**
 * Determina si la tienda actual comprará el objeto dado
 *
 * Nótese que un tendero debe negarse a comprar objetos "sin valor"
 */
static bool store_will_buy(struct store *store, const struct object *obj)
{
	struct object_buy *buy;

	/* El hogar acepta cualquier cosa */
	if (store->feat == FEAT_HOME) return true;

	/* Ignorar objetos aparentemente sin valor, excepto objetos sin venta {??} */
	if (object_value(obj, 1) <= 0 && !(OPT(player, birth_no_selling) &&
									   tval_has_variable_power(obj) &&
									   !object_runes_known(obj))) {
		return false;
	}

	/* No hay lista de compra significa que compramos cualquier cosa */
	if (!store->buy) return true;

	/* Recorrer la lista de compra */
	for (buy = store->buy; buy; buy = buy->next) {
		/* Tval incorrecto */
		if (buy->tval != obj->tval) continue;

		/* Sin bandera significa que está bien */
		if (!buy->flag) return true;

		/* Está bien si se sabe que el objeto tiene la bandera */
		if (of_has(obj->flags, buy->flag) &&
			object_flag_is_known(player, obj, buy->flag))
			return true;
	}

	/* No está en la lista */
	return false;
}


/**
 * ------------------------------------------------------------------------
 * Conceptos básicos: precios, generación, etc.
 * ------------------------------------------------------------------------ */


/**
 * Determina el precio de un objeto (cantidad uno) en una tienda.
 *
 *  store_buying == true  significa que la tienda está comprando, el jugador vendiendo
 *               == false significa que la tienda está vendiendo, el jugador comprando
 *
 * Esta función nunca permite que un tendero pierda dinero en una transacción.
 *
 * El valor "greed" debe ser superior a 100 cuando el jugador está "comprando" el
 * objeto, y debe ser inferior a 100 cuando el jugador lo está "vendiendo".
 *
 * El mercado negro siempre cobra el doble de lo que debería.
 */
int price_item(struct store *store, const struct object *obj,
			   bool store_buying, int qty)
{
	int adjust = 100;
	int price;
	struct owner *proprietor;

	if (!store) {
		return 0;
	}

	proprietor = store->owner;

	/* Obtener el valor de la pila de varitas, o de un solo objeto */
	if (tval_can_have_charges(obj)) {
		if (store_buying) {
			price = MIN(object_value_real(obj, qty),
				object_value(obj, qty));
		} else {
			price = MAX(object_value_real(obj, qty),
				object_value(obj, qty));
		}
	} else {
		if (store_buying) {
			price = MIN(object_value_real(obj, 1),
				object_value(obj, 1));
		} else {
			price = MAX(object_value_real(obj, 1),
				object_value(obj, 1));
		}
	}

	/* Objetos sin valor */
	if (price <= 0) {
		return (store_buying) ? 0 : qty;
	}

	/* El mercado negro siempre es un peor negocio */
	if (store->feat == FEAT_STORE_BLACK)
		adjust = 150;

	/* La tienda está comprando */
	if (store_buying) {
		/* Establecer el factor */
		adjust = 100 + (100 - adjust);
		if (adjust > 100) {
			adjust = 100;
		}

		/* Las tiendas ahora pagan 2/3 del valor real */
		price = price * 2 / 3;

		/* El mercado negro apesta */
		if (store->feat == FEAT_STORE_BLACK) {
			price = price / 2;
		}

		/* Comprobar la opción de no venta */
		if (OPT(player, birth_no_selling)) {
			return 0;
		}
	} else {
		/* Reevaluar si estamos vendiendo */
		if (tval_can_have_charges(obj)) {
			price = object_value_real(obj, qty);
		} else {
			price = object_value_real(obj, 1);
		}

		/* El mercado negro apesta */
		if (store->feat == FEAT_STORE_BLACK) {
			price = price * 2;
		}
	}

	/* Calcular el precio final (con redondeo) */
	price = (price * adjust + 50L) / 100L;

	/* Ahora convertir el precio a precio total para objetos que no son varitas */
	if (!tval_can_have_charges(obj)) {
		price *= qty;
	}

	/* Ahora limitar el precio al límite de la bolsa */
	if (store_buying && (price > proprietor->max_cost * qty)) {
		price = proprietor->max_cost * qty;
	}

	/* Nota -- Nunca llegar a ser "gratis" */
	if (price <= 0) {
		return qty;
	}

	/* Devolver el precio */
	return price;
}


/**
 * Cálculo especial de "producción en masa".
 */
static int mass_roll(int times, int max)
{
	int i, t = 0;

	assert(max > 1);

	for (i = 0; i < times; i++)
		t += randint0(max);

	return (t);
}


/**
 * Algunos objetos baratos deben crearse en pilas.
 */
static void mass_produce(struct object *obj)
{
	int size = 1;
	int cost = object_value_real(obj, 1);

	/* Analizar el tipo */
	switch (obj->tval)
	{
		/* Comida, Frascos y Luces */
		case TV_FOOD:
		case TV_MUSHROOM:
		case TV_FLASK:
		case TV_LIGHT:
		{
			if (cost <= 5L) size += mass_roll(3, 5);
			if (cost <= 20L) size += mass_roll(3, 5);
			break;
		}

		case TV_POTION:
		case TV_SCROLL:
		{
			if (cost <= 60L) size += mass_roll(3, 5);
			if (cost <= 240L) size += mass_roll(1, 5);
			break;
		}

		case TV_MAGIC_BOOK:
		case TV_PRAYER_BOOK:
		case TV_NATURE_BOOK:
		case TV_SHADOW_BOOK:
		case TV_OTHER_BOOK:
		{
			if (cost <= 50L) size += mass_roll(2, 3);
			if (cost <= 500L) size += mass_roll(1, 3);
			break;
		}

		case TV_SOFT_ARMOR:
		case TV_HARD_ARMOR:
		case TV_SHIELD:
		case TV_GLOVES:
		case TV_BOOTS:
		case TV_CLOAK:
		case TV_HELM:
		case TV_CROWN:
		case TV_SWORD:
		case TV_POLEARM:
		case TV_HAFTED:
		case TV_DIGGING:
		case TV_BOW:
		{
			if (obj->ego) break;
			if (cost <= 10L) size += mass_roll(3, 5);
			if (cost <= 100L) size += mass_roll(3, 5);
			break;
		}

		case TV_SHOT:
		case TV_ARROW:
		case TV_BOLT:
		{
			if (cost <= 5L)
				size = randint1(2) * 20;         /* 20-40 en 20s */
			else if (cost > 5L && cost <= 50L)
				size = randint1(4) * 10;         /* 10-40 en 10s */
			else if (cost > 50 && cost <= 500L)
				size = randint1(4) * 5;          /* 5-20 en 5s */
			else
				size = 1;

			break;
		}
	}

	/* Guardar el tamaño total de la pila */
	obj->number = MIN(size, obj->kind->base->max_stack);
}


/**
 * Ordenar el inventario de la tienda en un array ordenado.
 */
void store_stock_list(struct store *store, struct object **list, int n)
{
	bool home = (store->feat != FEAT_HOME);
	int list_num;
	int num = 0;

	for (list_num = 0; list_num < n; list_num++) {
		struct object *current, *first = NULL;
		for (current = store->stock; current; current = current->next) {
			int i;
			bool possible = true;

			/* Saltar objetos ya asignados */
			for (i = 0; i < num; i++)
				if (list[i] == current)
					possible = false;

			/* Si sigue siendo posible, elegir el primero en orden */
			if (!possible)
				continue;
			else if (earlier_object(first, current, home))
				first = current;
		}

		/* Asignar y contar el stock */
		list[list_num] = first;
		if (first)
			num++;
	}
}

/**
 * Permitir que un objeto de la tienda absorba otro objeto
 */
static void store_object_absorb(struct object *old, struct object *new)
{
	/* Combinar cantidad, perder objetos en exceso */
	int change = (old->number < old->kind->base->max_stack) ?
		MIN(new->number, old->kind->base->max_stack - old->number) : 0;

	distribute_charges(new, old, change, false);
	old->number += change;

	object_origin_combine(old, new);

	/* Absorbido completamente */
	object_delete(NULL, NULL, &new);
}


/**
 * Comprobar si la tienda estará llevando demasiados objetos
 *
 * Nótese que la tienda, al igual que un jugador, no aceptará cosas
 * que no puede contener. Antes, uno podía "eliminar" objetos de esta manera,
 * añadiéndolos a una pila que ya estaba llena.
 */
bool store_check_num(struct store *store, const struct object *obj)
{
	struct object *stock_obj;

	/* El espacio libre siempre es utilizable */
	if (store->stock_num < store->stock_size) return true;

	/* El "hogar" actúa como el jugador */
	if (store->feat == FEAT_HOME) {
		for (stock_obj = store->stock; stock_obj; stock_obj = stock_obj->next) {
			/* ¿Se puede combinar el nuevo objeto con el antiguo? */
			if (object_mergeable(stock_obj, obj, OSTACK_PACK))
				return true;
		}
	} else {
		/* Las tiendas normales hacen cosas especiales */
		for (stock_obj = store->stock; stock_obj; stock_obj = stock_obj->next) {
			/* ¿Se puede combinar el nuevo objeto con el antiguo? */
			if (object_mergeable(stock_obj, obj, OSTACK_STORE))
				return true;
		}
	}

	/* Pero no había lugar en la posada... */
	return false;
}


/**
 * Añadir un objeto al inventario del Hogar.
 *
 * También nótese que puede no "adaptarse" correctamente al "conocimiento" que se
 * vuelve conocido: el jugador puede tener que recoger objetos y soltarlos de nuevo.
 */
void home_carry(struct object *obj)
{
	struct object *temp_obj;
	struct store *store = &stores[f_info[FEAT_HOME].shopnum - 1];

	/* Comprobar cada objeto existente (intentar combinar) */
	for (temp_obj = store->stock; temp_obj; temp_obj = temp_obj->next) {
		/* El hogar actúa igual que el jugador */
		if (object_mergeable(temp_obj, obj, OSTACK_PACK)) {
			/* Guardar el nuevo número de objetos */
			object_absorb(temp_obj->known, obj->known);
			obj->known = NULL;
			object_absorb(temp_obj, obj);
			return;
		}
	}

	/* ¿Sin espacio? */
	if (store->stock_num >= store->stock_size) return;

	/* Insertar el nuevo objeto */
	pile_insert(&store->stock, obj);
	pile_insert(&store->stock_k, obj->known);
	store->stock_num++;
}


/**
 * Añadir un objeto al inventario de una tienda real.
 *
 * Si el objeto es "sin valor", se desecha (excepto en el hogar).
 *
 * Si el objeto no se puede combinar con un objeto ya en el inventario,
 * se crea un nuevo espacio para él y se calcula su precio "por objeto". Nótese que
 * este precio será negativo, ya que el precio no estará "fijado" todavía.
 * Añadir un objeto a una pila de precio "fijo" no cambiará el precio fijo.
 *
 * Devuelve el objeto insertado (para facilitar su uso) o NULL si desaparece
 */
struct object *store_carry(struct store *store, struct object *obj)
{
	unsigned int i;
	uint32_t value;
	struct object *temp_obj, *known_obj = obj->known;

	struct object_kind *kind = obj->kind;

	/* Evaluar el objeto */
	if (object_is_carried(player, obj))
		value = object_value(obj, 1);
	else
		value = object_value_real(obj, 1);

	/* Los objetos malditos/sin valor "desaparecen" al ser vendidos */
	if (value <= 0)
		return NULL;

	/* Borrar la inscripción */
	obj->note = 0;
	known_obj->note = 0;

	/* Algunos tipos de objetos requieren mantenimiento */
	if (tval_is_light(obj)) {
		if (!of_has(obj->flags, OF_NO_FUEL)) {
			if (of_has(obj->flags, OF_BURNS_OUT))
				obj->timeout = z_info->fuel_torch;

			else if (of_has(obj->flags, OF_TAKES_FUEL))
				obj->timeout = z_info->default_lamp;
		}
	} else if (tval_can_have_timeout(obj)) {
		obj->timeout = 0;
	} else if (tval_is_launcher(obj)) {
		obj->known->pval = obj->pval;
	} else if (tval_can_have_charges(obj)) {
		/* Si la tienda puede almacenar este tipo de objeto, lo recargamos */
		if (store_can_carry(store, obj->kind)) {
			int charges = 0;

			/* Calcular el número de cargas recargadas */
			for (i = 0; i < obj->number; i++)
				charges += randcalc(kind->charge, 0, RANDOMISE);

			/* Usar el valor recargado solo si es mayor */
			if (charges > obj->pval)
				obj->pval = charges;
		}
	}

	for (temp_obj = store->stock; temp_obj; temp_obj = temp_obj->next) {
		/* ¿Se pueden incrementar los objetos existentes? */
		if (object_mergeable(temp_obj, obj, OSTACK_STORE)) {
			/* Absorber (parte de) el objeto */
			store_object_absorb(temp_obj->known, known_obj);
			obj->known = NULL;
			store_object_absorb(temp_obj, obj);

			/* Todo listo */
			return temp_obj;
		}
	}

	/* ¿Sin espacio? */
	if (store->stock_num >= store->stock_size)
		return NULL;

	/* Insertar el nuevo objeto */
	pile_insert(&store->stock, obj);
	pile_insert(&store->stock_k, known_obj);
	store->stock_num++;

	return obj;
}


static void store_delete(struct store *s, struct object *obj, int amt)
{
	struct object *known_obj = obj->known;

	if (obj->number > amt) {
		obj->number -= amt;
		known_obj->number -= amt;
	} else {
		pile_excise(&s->stock, obj);
		object_delete(NULL, NULL, &obj);
		pile_excise(&s->stock_k, known_obj);
		object_delete(NULL, NULL, &known_obj);
		assert(s->stock_num);
		s->stock_num--;
	}
}


/**
 * Encontrar un tipo de objeto dado en la tienda. Si fexclude no es NULL, excluir
 * cualquier objeto, o, para el cual (*fexclude)(s, o) sea verdadero.
 */
static struct object *store_find_kind(struct store *s, struct object_kind *k,
		bool (*fexclude)(struct store *, struct object *)) {
	struct object *obj;

	assert(s);
	assert(k);

	/* Comprobar si ya está en stock */
	for (obj = s->stock; obj; obj = obj->next) {
		if (obj->kind == k && (fexclude == NULL ||
			!((*fexclude)(s, obj)))) return obj;
	}

	return NULL;
}


/**
 * Eliminar un objeto de la tienda 'store', o, si es una pila, quizás solo
 * eliminarlo parcialmente.
 *
 * Esta función se usa cuando ocurre el mantenimiento de la tienda y está diseñada para
 * imitar a compradores no-PC que hacen compras en la tienda.
 *
 * La razón por la que esto no comprueba los artículos "básicos" y se niega a
 * eliminarlos es que una tienda podría tener dos pilas de un
 * solo artículo básico, en cuyo caso, se podría tener una tienda que
 * tuviera más pilas que artículos básicos, pero todas las pilas son artículos básicos.
 */
static void store_delete_random(struct store *store)
{
	int what;
	int num;
	struct object *obj;

	assert(store->stock_num > 0);

	/* Elegir un espacio aleatorio */
	what = randint0(store->stock_num);

	/* Recorrer la lista hasta encontrar nuestro objeto */
	obj = store->stock;
	while (what--) {
		assert(obj);
		obj = obj->next;
	}

	/* Determinar cuántos objetos hay en el espacio */
	num = obj->number;

	/* Tratar con pilas */
	if (num > 1) {
		/* Comportamiento especial para flechas, pernos, etc. */
		if (tval_is_ammo(obj)) {
			/* 50% de probabilidad de destruir toda la pila */
			if (randint0(100) < 50 || num < 10)
				num = obj->number;

			/* 50% de probabilidad de reducir el tamaño a un múltiplo de 5 */
			else
				num = randint1(num / 5) * 5 + (num % 5);
		} else {
			/* 50% de probabilidad de destruir un solo objeto */
			if (randint0(100) < 50) num = 1;

			/* 25% de probabilidad de destruir la mitad de los objetos */
			else if (randint0(100) < 50) num = (num + 1) / 2;

			/* 25% de probabilidad de destruir todos los objetos */
			else num = obj->number;

			/* Disminuir las cargas totales de bastones y varitas. */
			if (tval_can_have_charges(obj))
				obj->pval -= num * obj->pval / obj->number;
		}
	}

	assert (num <= obj->number);

	if (obj->artifact) {
		history_lose_artifact(player, obj->artifact);
	}

	/* Eliminar el objeto, total o parcialmente */
	store_delete(store, obj, num);
}


/**
 * Esto asegura que el mercado negro no tenga ningún objeto que otras
 * tiendas tengan, a menos que sea un objeto de ego o tenga varias bonificaciones.
 *
 * Basado en una sugerencia de Lee Vogt <lvogt@cig.mcel.mot.com>.
 */
static bool black_market_ok(const struct object *obj)
{
	int i;

	/* Los objetos de ego siempre están bien */
	if (obj->ego) return true;

	/* Los objetos buenos normalmente están bien */
	if (obj->to_a > 2) return true;
	if (obj->to_h > 1) return true;
	if (obj->to_d > 2) return true;

	/* Sin objetos baratos */
	if (object_value_real(obj, 1) < 10) return (false);

	/* Comprobar las otras tiendas */
	for (i = 0; i < z_info->store_max; i++) {
		struct object *stock_obj;

		/* Saltar el hogar y el mercado negro */
		if (stores[i].feat == FEAT_STORE_BLACK
				|| stores[i].feat == FEAT_HOME)
			continue;

		/* Comprobar cada objeto en la tienda */
		for (stock_obj = stores[i].stock; stock_obj; stock_obj = stock_obj->next) {
			/* Comparar tipos de objeto */
			if (obj->kind == stock_obj->kind)
				return false;
		}
	}

	/* De lo contrario, está bien */
	return true;
}



/**
 * Obtener una opción de la tabla de asignación de la tienda, en tables.c
 */
static struct object_kind *store_get_choice(struct store *store)
{
	/* Elegir una entrada aleatoria de la tabla de la tienda */
	return store->normal_table[randint0(store->normal_num)];
}


/**
 * Crea un objeto aleatorio y se lo da a la tienda 'store'
 */
static bool store_create_random(struct store *store)
{
	int tries, level;

	int min_level, max_level;

	/* Decidir niveles mínimos y máximos */
	if (store->feat == FEAT_STORE_BLACK) {
		min_level = player->max_depth + 5;
		max_level = player->max_depth + 20;
	} else {
		min_level = 1;
		max_level = z_info->store_magic_level + MAX(player->max_depth - 20, 0);
	}

	if (min_level > 55) min_level = 55;
	if (max_level > 70) max_level = 70;

	/* Considerar hasta seis objetos */
	for (tries = 0; tries < 6; tries++) {
		struct object_kind *kind;
		struct object *obj, *known_obj;

		/* Calcular el nivel para los objetos a generar */
		level = rand_range(min_level, max_level);

		/* Los Mercados Negros tienen un objeto aleatorio, de un nivel dado */
		if (store->feat == FEAT_STORE_BLACK)
			kind = get_obj_num(level, false, 0);
		else
			kind = store_get_choice(store);

		/*** Filtros de pre-generación ***/

		/* Sin cofres en las tiendas XXX */
		if (kind->tval == TV_CHEST) continue;

		/*** Generar el objeto ***/

		/* Crear un nuevo objeto del tipo elegido */
		obj = object_new();
		object_prep(obj, kind, level, RANDOMISE);

		/* Aplicar algo de magia de "bajo nivel" (sin artefactos) */
		apply_magic(obj, level, false, false, false, false);
		assert(!obj->artifact);

		/* Rechazar si el objeto está 'dañado' (modificadores de combate negativos, maldiciones) */
		if ((tval_is_weapon(obj) && ((obj->to_h < 0) || (obj->to_d < 0)))
			|| (tval_is_armor(obj) && (obj->to_a < 0)) || (obj->curses)) {
			object_delete(NULL, NULL, &obj);
			continue;
		}

		/*** Filtros de post-generación ***/

		/* Crear un objeto conocido */
		known_obj = object_new();
		obj->known = known_obj;

		/* Saber todo lo que el jugador sabe, sin origen todavía */
		obj->known->notice |= OBJ_NOTICE_ASSESSED;
		object_set_base_known(player, obj);
		obj->known->notice |= OBJ_NOTICE_ASSESSED;
		player_know_object(player, obj);
		obj->origin = ORIGIN_NONE;

		/* Los mercados negros tienen gustos caros */
		if ((store->feat == FEAT_STORE_BLACK) && !black_market_ok(obj)) {
			object_delete(NULL, NULL, &known_obj);
			obj->known = NULL;
			object_delete(NULL, NULL, &obj);
			continue;
		}

		/* No hay objetos "sin valor" */
		if (object_value_real(obj, 1) < 1)  {
			object_delete(NULL, NULL, &known_obj);
			obj->known = NULL;
			object_delete(NULL, NULL, &obj);
			continue;
		}

		/* Producción en masa y/o aplicar descuento */
		mass_produce(obj);

		/* Intentar llevar el objeto */
		if (!store_carry(store, obj)) {
			object_delete(NULL, NULL, &known_obj);
			obj->known = NULL;
			object_delete(NULL, NULL, &obj);
			continue;
		}

		/* Definitivamente listo */
		return true;
	}

	return false;
}


/**
 * Función auxiliar: crear un objeto con el par tval,sval dado, añadirlo a la
 * tienda st. Devolver el objeto en el inventario.
 */
static struct object *store_create_item(struct store *store,
										struct object_kind *kind)
{
	struct object *obj = object_new();
	struct object *known_obj = object_new();
	struct object *carried;

	/* Crear un nuevo objeto del tipo elegido */
	object_prep(obj, kind, 0, RANDOMISE);
	assert(!obj->artifact);

	/* Saber todo lo que el jugador sabe, sin origen todavía */
	obj->known = known_obj;
	obj->known->notice |= OBJ_NOTICE_ASSESSED;
	object_set_base_known(player, obj);
	obj->known->notice |= OBJ_NOTICE_ASSESSED;
	player_know_object(player, obj);
	obj->origin = ORIGIN_NONE;

	/* Intentar llevar el objeto */
	carried = store_carry(store, obj);
	if (!carried) {
		object_delete(NULL, NULL, &known_obj);
		obj->known = NULL;
		object_delete(NULL, NULL, &obj);
	}
	return carried;
}

/**
 * Mantener el inventario de las tiendas.
 */
static void store_maint(struct store *s)
{
	/* Ignorar el hogar */
	if (s->feat == FEAT_HOME)
		return;

	/* Destruir objetos del mercado negro de baja calidad */
	if (s->feat == FEAT_STORE_BLACK) {
		struct object *obj = s->stock;
		while (obj) {
			struct object *next = obj->next;
			if (!black_market_ok(obj)) {
				if (obj->artifact) {
					history_lose_artifact(player,
						obj->artifact);
				}
				store_delete(s, obj, obj->number);
			}
			obj = next;
		}
	}

	/* Queremos asegurarnos de que las tiendas tengan artículos básicos. Si hay
	 * rotación, también queremos eliminar algunos objetos y añadir algunos
	 * objetos.
	 *
	 * Si creamos artículos básicos, luego eliminamos objetos, luego creamos nuevos
	 * objetos, nos quedamos con una de tres opciones:
	 * 1. Podemos arriesgarnos a eliminar artículos básicos y no tener ninguno.
	 * 2. Podemos negarnos a eliminar artículos básicos y arriesgarnos a que eso
	 * se convierta en un bucle infinito.
	 * 3. Podemos hacer un montón de contabilidad adicional para asegurarnos de eliminar
	 * artículos básicos solo si hay duplicados de ellos.
	 *
	 * ¿Y si cambiamos el orden? Primero vender un puñado de objetos aleatorios,
	 * luego crear los artículos básicos que falten, luego crear nuevos objetos. Esto
	 * tiene dos pruebas para s->turnover, pero simplifica todo lo demás
	 * drásticamente.
	 */

	if (s->turnover) {
		int restock_attempts = 100000;
		int stock = s->stock_num - randint1(s->turnover);

		/* Terminaremos añadiendo artículos básicos con seguridad, quizás más otros
		 * objetos. Está bien si nos quedamos sin existencias por completo, sin embargo,
		 * si la rotación es alta. El límite no incluye always_num,
		 * porque de lo contrario la adición de artículos básicos que faltan podría
		 * ponernos por encima (si la tienda estaba llena de botín vendido por el jugador).
		 */
		int min = 0;
		int max = s->normal_stock_max;

		if (stock < min) stock = min;
		if (stock > max) stock = max;

		/* Destruir objetos aleatorios hasta que solo queden espacios "stock" */
		while (s->stock_num > stock && --restock_attempts)
			store_delete_random(s);

		if (!restock_attempts)
			quit_fmt("No se puede (des-)abastecer %s. Por favor, reporta este error",
				(f_info[s->feat].name) ? f_info[s->feat].name :
				format("tienda %d", f_info[s->feat].shopnum));
	} else {
		/* Para el Librero, ocasionalmente vender un libro */
		if (s->always_num && s->stock_num) {
			int sales = randint1(s->stock_num);
			while (sales--) {
				store_delete_random(s);
			}
		}
	}

	/* Asegurar que se creen los artículos básicos */
	if (s->always_num) {
		size_t i;
		for (i = 0; i < s->always_num; i++) {
			struct object_kind *kind = s->always_table[i];
			struct object *obj = store_find_kind(s, kind,
				store_sale_should_reduce_stock);

			/* Crear el objeto si no existe */
			if (!obj) {
				obj = store_create_item(s, kind);
				if (!obj) continue;
			}

			/* Asegurar una pila completa */
			obj->number = obj->kind->base->max_stack;
			obj->known->number = obj->kind->base->max_stack;
		}
	}

	if (s->turnover) {
		int restock_attempts = 100000;
		int stock = s->stock_num + randint1(s->turnover);

		/* Ahora que existen los artículos básicos, queremos añadir más
		 * objetos, al menos suficientes para llegar a normal_stock_min
		 * objetos que no son necesariamente artículos básicos.
		 */

		int min = s->normal_stock_min + s->always_num;
		int max = s->normal_stock_max + s->always_num;

		/* Comprar algunos objetos */

		/* Mantener el stock entre los espacios mínimos y máximos especificados */
		if (stock > max) stock = max;
		if (stock < min) stock = min;

		/* Para el resto, simplemente elegimos objetos de forma aleatoria */
		/* Los (enormes) restock_attempts solo llegarán a cero (de lo contrario
		 * bucle infinito) si las tiendas no tienen suficientes objetos que puedan almacenar. */
		while (s->stock_num < stock && --restock_attempts)
			store_create_random(s);

		if (!restock_attempts)
			quit_fmt("No se puede (re-)abastecer %s. Por favor, reporta este error",
				(f_info[s->feat].name) ? f_info[s->feat].name :
				format("tienda %d", f_info[s->feat].shopnum));
	}
}

/**
 * Actualizar las tiendas al regresar a la ciudad.
 */
void store_update(void)
{
	if (OPT(player, cheat_xtra)) msg("Actualizando tiendas...");
	while (daycount--) {
		int n;

		/* Mantener cada tienda (excepto el hogar) */
		for (n = 0; n < z_info->store_max; n++) {
			/* Saltar el hogar */
			if (stores[n].feat == FEAT_HOME) continue;

			/* Mantener */
			store_maint(&stores[n]);
		}

		/* A veces, cambiar a los dueños de las tiendas */
		if (one_in_(z_info->store_shuffle)) {
			int *non_home_inds = mem_zalloc(z_info->store_max
				* sizeof(*non_home_inds));
			int n_without_home = 0;

			/* Mensaje */
			if (OPT(player, cheat_xtra)) msg("Cambiando a un tendero...");

			/* Elegir una tienda aleatoria (excepto el hogar) */
			for (n = 0; n < z_info->store_max; n++) {
				if (stores[n].feat != FEAT_HOME) {
					non_home_inds[n_without_home] = n;
					++n_without_home;
				}
			}
			if (n_without_home > 0) {
				n = randint0(n_without_home);
				/* Luego cambiarlo. */
				store_shuffle(&stores[non_home_inds[n]]);
			}
			mem_free(non_home_inds);
		}
	}
	daycount = 0;
	if (OPT(player, cheat_xtra)) msg("Listo.");
}

/** Cosas del dueño **/

struct owner *store_ownerbyidx(struct store *s, unsigned int idx) {
	struct owner *o;
	for (o = s->owners; o; o = o->next) {
		if (o->oidx == idx)
			return o;
	}

	quit_fmt("Llamada incorrecta a store_ownerbyidx: idx es %d\n", idx);
	return 0; /* Necesario para evitar advertencia del compilador de Windows */
}

static struct owner *store_choose_owner(struct store *s) {
	struct owner *o;
	unsigned int n = 0;

	for (o = s->owners; o; o = o->next) {
		n++;
	}

	n = randint0(n);
	return store_ownerbyidx(s, n);
}

/**
 * Cambiar al dueño de una de las tiendas.
 */
void store_shuffle(struct store *store)
{
	struct owner *o = store->owner;

	while (o == store->owner)
	    o = store_choose_owner(store);

	store->owner = o;
}




/**
 * ------------------------------------------------------------------------
 * Código de nivel superior
 * ------------------------------------------------------------------------ */


/**
 * Devolver la cantidad de un objeto dado en la mochila (incluyendo la aljaba).
 */
int find_inven(const struct object *obj)
{
	int i;
	struct object *gear_obj;
	int num = 0;

	/* ¿Espacio similar? */
	for (gear_obj = player->gear; gear_obj; gear_obj = gear_obj->next) {
		/* Comprobar solo el inventario y la aljaba */
		if (object_is_equipped(player->body, gear_obj))
			continue;

		/* Requerir tipos de objeto idénticos */
		if (obj->kind != gear_obj->kind)
			continue;

		/* Analizar los objetos */
		switch (obj->tval)
		{
			/* Cofres */
			case TV_CHEST:
			{
				/* Nunca está bien */
				return 0;
			}

			/* Comida, Pociones y Pergaminos */
			case TV_FOOD:
			case TV_MUSHROOM:
			case TV_POTION:
			case TV_SCROLL:
			{
				/* Asumir que está bien */
				break;
			}

			/* Bastones y Varitas */
			case TV_STAFF:
			case TV_WAND:
			{
				/* Asumir que está bien */
				break;
			}

			/* Varas */
			case TV_ROD:
			{
				/* Asumir que está bien */
				break;
			}

			/* Equipables */
			case TV_BOW:
			case TV_DIGGING:
			case TV_HAFTED:
			case TV_POLEARM:
			case TV_SWORD:
			case TV_BOOTS:
			case TV_GLOVES:
			case TV_HELM:
			case TV_CROWN:
			case TV_SHIELD:
			case TV_CLOAK:
			case TV_SOFT_ARMOR:
			case TV_HARD_ARMOR:
			case TV_DRAG_ARMOR:
			case TV_RING:
			case TV_AMULET:
			case TV_LIGHT:
			case TV_BOLT:
			case TV_ARROW:
			case TV_SHOT:
			{
				/* Requerir "bonificaciones" idénticas */
				if (obj->to_h != gear_obj->to_h)
					continue;
				if (obj->to_d != gear_obj->to_d)
					continue;
				if (obj->to_a != gear_obj->to_a)
					continue;

				/* Requerir modificadores idénticos */
				for (i = 0; i < OBJ_MOD_MAX; i++)
					if (obj->modifiers[i] != gear_obj->modifiers[i])
						continue;

				/* Requerir nombres de "artefacto" idénticos */
				if (obj->artifact != gear_obj->artifact)
					continue;

				/* Requerir nombres de "objeto-ego" idénticos */
				if (obj->ego != gear_obj->ego)
					continue;

				/* Las luces deben tener la misma cantidad de combustible */
				else if (obj->timeout != gear_obj->timeout &&
						 tval_is_light(obj))
					continue;

				/* Requerir "valores" idénticos */
				if (obj->ac != gear_obj->ac)
					continue;
				if (obj->dd != gear_obj->dd)
					continue;
				if (obj->ds != gear_obj->ds)
					continue;

				/* Probablemente está bien */
				break;
			}

			/* Varios */
			default:
			{
				/* Probablemente está bien */
				break;
			}
		}


		/* Banderas diferentes */
		if (!of_is_equal(obj->flags, gear_obj->flags))
			continue;

		/* Coinciden, así que sumar */
		num += gear_obj->number;
	}

	return num;
}


/**
 * Comprar el objeto con el índice dado del inventario de la tienda actual.
 */
void do_cmd_buy(struct command *cmd)
{
	int amt;

	struct object *obj, *bought, *known_obj;

	char o_name[80];
	char o_name_final[80];  //fix traduc
	int price;

	struct store *store = store_at(cave, player->grid);

	if (!store) {
		msg("No puedes comprar objetos cuando no estás en una tienda.");
		return;
	}

	/* Obtener argumentos */
	/* XXX-AS completar esto, dividir en cmd-store.c */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK)
		return;

	if (!pile_contains(store->stock, obj)) {
		msg("No puedes comprar ese objeto porque no está en la tienda.");
		return;
	}

	if (cmd_get_arg_number(cmd, "quantity", &amt) != CMD_OK)
		return;

	/* Obtener el objeto deseado */
	bought = object_new();
	object_copy_amt(bought, obj, amt);

	/* Asegurar que tenemos espacio */
	if (bought->number > inven_carry_num(player, bought)) {
		msg("No puedes llevar tantos objetos.");
		object_delete(NULL, NULL, &bought);
		return;
	}

	/* fix traduc Describir el objeto (completamente) */	
	object_desc(o_name, sizeof(o_name), bought, ODESC_FULL, player);
	// fix traduc
	if (bought->number > 1) {		
    	strnfmt(o_name_final, sizeof(o_name_final), "%d %s", bought->number, o_name);
    } else {
    	my_strcpy(o_name_final, o_name, sizeof(o_name_final));
      }    

	/* Extraer el precio para la pila completa */
	price = price_item(store, bought, false, bought->number);

	if (price > player->au) {
		msg("No puedes permitirte esa compra.");
		object_delete(NULL, NULL, &bought);
		return;
	}

	/* Gastar el dinero */
	player->au -= price;

	/* Actualizar el equipo */
	player->upkeep->update |= (PU_INVEN);

	/* Combinar la mochila (después) */
	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);

	/* fix traduc Describir el objeto (completamente) de nuevo para el mensaje*/
	object_desc(o_name, sizeof(o_name), bought, ODESC_FULL, player);
	//fix traduc
	if (bought->number > 1) {		
    	strnfmt(o_name_final, sizeof(o_name_final), "%d %s", bought->number, o_name);
    } else {    	
    	my_strcpy(o_name_final, o_name, sizeof(o_name_final));
      }

	/* Mensaje */
	if (one_in_(3)) msgt(MSG_STORE5, "%s", ONE_OF(comment_accept));	
	//fix traduc
	msg("Compraste %s por %d de oro.", o_name_final, price);

	/* Borrar la inscripción */
	bought->note = 0;

	/* Darle un origen si no tiene uno */
	if (bought->origin == ORIGIN_NONE)
		bought->origin = ORIGIN_STORE;

	/* Truco - Reducir el número de cargas en la pila original */
	if (tval_can_have_charges(obj))
		obj->pval -= bought->pval;

	/* Crear un objeto conocido */
	known_obj = object_new();
	object_copy(known_obj, obj->known);
	bought->known = known_obj;

	/* Aprender el sabor, cualquier efecto y todas las runas */
	object_flavor_aware(player, bought);
	bought->known->effect = bought->effect;
	while (!object_fully_known(bought)) {
		object_learn_unknown_rune(player, bought);
		player_know_object(player, bought);
	}

	/* Darlo al jugador */
	inven_carry(player, bought, true, true);

	/* Manejar cosas */
	handle_stuff(player);

	/* Eliminar los objetos comprados de la tienda si no es un artículo básico
	 * que se repone fácilmente */
	if (store_sale_should_reduce_stock(store, obj)) {
		/* Reducir o eliminar el objeto */
		store_delete(store, obj, amt);

		/* La tienda está vacía */
		if (store->stock_num == 0) {
			int i;

			/* A veces cambiar al tendero */
			if (one_in_(z_info->store_shuffle)) {
				/* Cambiar */
				msg("El tendero se jubila.");
				store_shuffle(store);
			} else
				/* Mantener */
				msg("El tendero saca algunas existencias nuevas.");

			/* Nuevo inventario */
			for (i = 0; i < 10; ++i)
				store_maint(store);
		}
	}

	event_signal(EVENT_STORECHANGED);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}

/**
 * Recuperar el objeto con el índice dado del inventario del hogar.
 */
void do_cmd_retrieve(struct command *cmd)
{
	int amt;

	struct object *obj, *known_obj, *picked_item;

	struct store *store = store_at(cave, player->grid);
	if (!store) return;

	if (store->feat != FEAT_HOME) {
		msg("No estás actualmente en tu hogar.");
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK)
		return;

	if (!pile_contains(store->stock, obj)) {
		msg("No puedes recuperar ese objeto porque no está en el hogar.");
		return;
	}

	if (cmd_get_arg_number(cmd, "quantity", &amt) != CMD_OK)
		return;

	/* Obtener el objeto deseado */
	picked_item = object_new();
	object_copy_amt(picked_item, obj, amt);

	/* Asegurar que tenemos espacio */
	if (picked_item->number > inven_carry_num(player, picked_item)) {
		msg("No puedes llevar tantos objetos.");
		object_delete(NULL, NULL, &picked_item);
		return;
	}

	/* Distribuir cargas de varitas, bastones o varas */
	distribute_charges(obj, picked_item, amt, true);

	/* Crear un objeto conocido */
	known_obj = object_new();
	/*
	 * Tener al menos un guardado,
	 * https://github.com/angband/angband/issues/6362 , donde
	 * obj->known->number no coincide con obj->number. Forzar
	 * obj->known->number para que sea utilizable en object_copy_amt() y
	 * distribute_charges(). Puede ser posible eliminar esa coerción si
	 * la fuente de los números desalineados se soluciona y se requiere compatibilidad
	 * con guardados antiguos que pueden tener números desalineados.
	 */
	obj->known->number = obj->number;
	object_copy_amt(known_obj, obj->known, amt);
	picked_item->known = known_obj;
	distribute_charges(obj->known, picked_item->known, amt, true);

	/* Darlo al jugador */
	inven_carry(player, picked_item, true, true);

	/* Manejar cosas */
	handle_stuff(player);
	
	/* Reducir o eliminar el objeto */
	store_delete(store, obj, amt);

	event_signal(EVENT_STORECHANGED);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}


/**
 * Determinar si la tienda actual comprará el objeto dado
 */
bool store_will_buy_tester(const struct object *obj)
{
	struct store *store = store_at(cave, player->grid);
	if (!store) return false;

	return store_will_buy(store, obj);
}

/**
 * Vender un objeto a la tienda actual.
 */
void do_cmd_sell(struct command *cmd)
{
	int amt;
	struct object dummy_item;
	struct store *store = store_at(cave, player->grid);
	int price, dummy, value;
	char o_name[120];
	char o_name_final[120];  //fix traduc
	char label;

	struct object *obj, *sold_item;
	bool none_left = false;

	/* Obtener argumentos */
	/* XXX-AS completar esto, dividir en cmd-store.c */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK)
		return;

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	/* No se pueden quitar objetos pegajosos */
	if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj)) {
		msg("Mmm, parece estar pegado.");
		return;
	}

	/* Comprobar que estamos en un lugar donde podemos vender los objetos. */
	if (!store) {
		msg("No puedes vender objetos cuando no estás en una tienda.");
		return;
	}

	/* Comprobar si la tienda quiere los objetos que se venden */
	if (!store_will_buy(store, obj)) {
		msg("No deseo comprar este objeto.");
		return;
	}

	/* Obtener una copia del objeto que representa el número que se vende */
	object_copy_amt(&dummy_item, obj, amt);

	/* Comprobar si la tienda tiene espacio para los objetos */
	if (!store_check_num(store, &dummy_item)) {
		object_wipe(&dummy_item);
		msg("No tengo espacio en mi tienda para guardarlo.");
		return;
	}

	/* Obtener la etiqueta */
	label = gear_to_label(player, obj);

	price = price_item(store, &dummy_item, true, amt);

	/* Obtener algo de dinero */
	player->au += price;

	/* Actualizar el auto-historial si se vende un artefacto que anteriormente
	 * no estaba identificado. (¡Ay!) */
	if (obj->artifact)
		history_find_artifact(player, obj->artifact);

	/* Actualizar el equipo */
	player->upkeep->update |= (PU_INVEN);

	/* Combinar la mochila (después) */
	player->upkeep->notice |= (PN_COMBINE);

	/* Redibujar cosas */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);

	/* Obtener el valor "aparente" */
	dummy = object_value(&dummy_item, amt);
	/*
	 * Ya no necesitamos el dummy, así que liberamos la memoria asignada
	 * dentro de él.
	 */
	object_wipe(&dummy_item);

	/* Conocer el sabor de los consumibles */
	object_flavor_aware(player, obj);
	obj->known->effect = obj->effect;
	while (!object_fully_known(obj)) {
		object_learn_unknown_rune(player, obj);
		player_know_object(player, obj);
	}

	/* Tomar una copia adecuada del objeto ahora conocido. */
	sold_item = gear_object_for_use(player, obj, amt, false, &none_left);

	/* Obtener el valor "real" */
	value = object_value_real(sold_item, amt);

	/* Fix traduc Obtener la descripción de nuevo*/
	object_desc(o_name, sizeof(o_name), sold_item, ODESC_FULL, player);
	//fix traduc
	if (sold_item->number > 1) {
    	strnfmt(o_name_final, sizeof(o_name_final), "%d %s", sold_item->number, o_name);
    } else {    	
    	my_strcpy(o_name_final, o_name, sizeof(o_name_final));
      }

	/* Describir el resultado (en el búfer de mensajes) */
	if (OPT(player, birth_no_selling)) {
		//fix traduc
		msg("Tenías %s (%c).", o_name_final, label);
	} else {
		//fix traduc
		msg("Vendiste %s (%c) por %d de oro.", o_name_final, label, price);
		/* Analizar los precios (y comentar verbalmente) */
		purchase_analyze(price, value, dummy);	    
	}

	/* Autoinscribir si todavía tenemos alguno */
	if (!none_left)
		apply_autoinscription(player, obj);

	/* Establecer la bandera de ignorar */
	player->upkeep->notice |= PN_IGNORE;

	/* Notificar si los objetos de la mochila necesitan combinarse o reordenarse */
	notice_stuff(player);

	/* Manejar cosas */
	handle_stuff(player);

	/* La tienda obtiene ese objeto (conocido) */
	if (!store_carry(store, sold_item)) {
		/* La tienda lo rechazó; eliminar. */
		if (sold_item->artifact) {
			history_lose_artifact(player, sold_item->artifact);
		}
		if (sold_item->known) {
			object_delete(NULL, NULL, &sold_item->known);
			sold_item->known = NULL;
		}
		object_delete(NULL, NULL, &sold_item);
	}

	event_signal(EVENT_STORECHANGED);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}

/**
 * Guardar un objeto en el hogar.
 */
void do_cmd_stash(struct command *cmd)
{
	int amt;
	struct object dummy;
	struct store *store = store_at(cave, player->grid);
	char o_name[120];
	char o_name_final[120]; //fix traduc
	char label;

	struct object *obj, *dropped;
	bool none_left = false;
	bool no_room;

	if (cmd_get_arg_item(cmd, "item", &obj))
		return;

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	/* Comprobar que estamos en un lugar donde podemos guardar objetos. */
	if (!store || store->feat != FEAT_HOME) {
		msg("No estás en tu hogar.");
		return;
	}

	/* No se pueden quitar objetos pegajosos */
	if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj)) {
		msg("Mmm, parece estar pegado.");
		return;
	}	

	/* Obtener una copia del objeto que representa el número que se vende */
	object_copy_amt(&dummy, obj, amt);

	no_room = !store_check_num(store, &dummy);
	/*
	 * Ya no necesitamos el dummy, así que liberamos la memoria asignada
	 * dentro de él.
	 */
	object_wipe(&dummy);
	if (no_room) {
		msg("Tu hogar está lleno.");
		return;
	}

	/* Obtener dónde está el objeto ahora */
	label = gear_to_label(player, obj);

	/* Ahora obtener el objeto real */
	dropped = gear_object_for_use(player, obj, amt, false, &none_left);

	/* fix traduc Describir*/
	object_desc(o_name, sizeof(o_name), dropped, ODESC_FULL, player);
	// fix traduc
	if (dropped->number > 1) {		
    	strnfmt(o_name_final, sizeof(o_name_final), "%d %s", dropped->number, o_name);
    } else {    	
    	my_strcpy(o_name_final, o_name, sizeof(o_name_final));
      }

	/* Mensaje */
	//fix traduc
	msg("Soltaste %s (%c).", o_name_final, label);

	/* Manejar cosas */
	handle_stuff(player);

	/* Permitir que el hogar lo lleve */
	home_carry(dropped);

	event_signal(EVENT_STORECHANGED);
	event_signal(EVENT_INVENTORY);
	event_signal(EVENT_EQUIPMENT);
}