/**
 * \archivo obj-util.c
 * \brief Utilidades de objetos
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "grafmode.h"
#include "init.h"
#include "mon-make.h"
#include "monster.h"
#include "obj-curse.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-history.h"
#include "player-spell.h"
#include "player-util.h"
#include "randname.h"
#include "z-queue.h"

struct object_base *kb_info;
struct object_kind *k_info;
struct artifact *a_info;
struct artifact_upkeep *aup_info;
struct ego_item *e_info;
struct flavor *flavors;

/**
 * Almacena los títulos de los pergaminos, de 6 a 14 caracteres cada uno, más comillas.
 */
static char scroll_adj[MAX_TITLES][18];

static void flavor_assign_fixed(void)
{
	int i;
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		if (f->sval == SV_UNKNOWN)
			continue;

		for (i = 0; i < z_info->k_max; i++) {
			struct object_kind *k = &k_info[i];
			if (k->tval == f->tval && k->sval == f->sval)
				k->flavor = f;
		}
	}
}


static void flavor_assign_random(uint8_t tval)
{
	int i;
	int flavor_count = 0;
	int choice;
	struct flavor *f;

	/* Contar los sabores aleatorios para el tval dado */
	for (f = flavors; f; f = f->next)
		if (f->tval == tval && f->sval == SV_UNKNOWN)
			flavor_count++;

	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval || k_info[i].flavor)
			continue;

		if (!flavor_count)
			quit_fmt("No hay suficientes sabores para tval %d.", tval);

		choice = randint0(flavor_count);
	
		for (f = flavors; f; f = f->next) {
			if (f->tval != tval || f->sval != SV_UNKNOWN)
				continue;

			if (choice == 0) {
				k_info[i].flavor = f;
				f->sval = k_info[i].sval;
				if (tval == TV_SCROLL)
					f->text = scroll_adj[k_info[i].sval];
				flavor_count--;
				break;
			}

			choice--;
		}
	}
}

/**
 * Restablece los svals en los sabores, eliminando efectivamente cualquier sabor fijo.
 *
 * Principalmente útil para artefactos aleatorios para que los sabores fijos para
 * objetos estándar no sean predecibles. El Anillo Único se mantiene como fijo,
 * ya que perdura a través de los artefactos aleatorios.
 */
static void flavor_reset_fixed(void)
{
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		if (f->tval == TV_RING && strstr(f->text, "Plain Gold"))
			continue;

		f->sval = SV_UNKNOWN;
	}
}

/**
 * Prepara la parte "variable" del array "k_info".
 *
 * El "color"/"metal"/"tipo" de un objeto es su "sabor".
 * En su mayor parte, los sabores se asignan aleatoriamente cada partida.
 *
 * Inicializa las descripciones para los objetos "coloreados", incluyendo:
 * Anillos, Amuletos, Bastones, Varitas, Varas, Setas, Pociones, Pergaminos.
 *
 * Los títulos de los pergaminos siempre tienen entre 6 y 14 letras. Esto está
 * garantizado porque cada título está compuesto de palabras completas, donde cada
 * palabra tiene de 2 a 8 letras, y porque ningún pergamino se termina
 * hasta que intenta crecer más allá de 15 letras. La primera vez que esto
 * puede suceder es cuando el título actual tiene 6 letras y la nueva palabra
 * tiene 8 letras, lo que resultaría en un título de pergamino de 6 letras.
 *
 * Asegura que todo permanezca igual para cada partida guardada.
 * Esto se logra mediante el uso de una "semilla aleatoria" guardada, como en
 * "town_gen()". Dado que no se llaman otras funciones mientras la semilla
 * especial está en efecto, esta función es bastante "segura".
 */
void flavor_init(void)
{
	int i, j;

	/* Usar el RNG "simple" */
	Rand_quick = true;

	/* Inducir sabores consistentes */
	Rand_value = seed_flavor;

	/* Limpiar todos los sabores y volver a analizar para nuevos jugadores */
	if (turn == 1) {
		struct flavor *f;

		for (i = 0; i < z_info->k_max; i++) {
			k_info[i].flavor = NULL;
		}
		for (f = flavors; f; f = f->next) {
			f->sval = SV_UNKNOWN;
		}
		cleanup_parser(&flavor_parser);
		run_parser(&flavor_parser);
	}

	if (OPT(player, birth_randarts))
		flavor_reset_fixed();

	flavor_assign_fixed();

	flavor_assign_random(TV_RING);
	flavor_assign_random(TV_AMULET);
	flavor_assign_random(TV_STAFF);
	flavor_assign_random(TV_WAND);
	flavor_assign_random(TV_ROD);
	flavor_assign_random(TV_MUSHROOM);
	flavor_assign_random(TV_POTION);

	/* Pergaminos (títulos aleatorios, siempre blancos) */
	for (i = 0; i < MAX_TITLES; i++) {
		char buf[26];
		char *end = buf + 1;
		int titlelen = 0;
		int wordlen;
		bool okay = true;

		my_strcpy(buf, "\"", 2);
		wordlen = randname_make(RANDNAME_SCROLL, 2, 8, end, 24, name_sections);
		while (titlelen + wordlen < (int)(sizeof(scroll_adj[0]) - 3)) {
			end[wordlen] = ' ';
			titlelen += wordlen + 1;
			end += wordlen + 1;
			wordlen = randname_make(RANDNAME_SCROLL, 2, 8, end, 24 - titlelen,
									name_sections);
		}
		buf[titlelen] = '"';
		buf[titlelen+1] = '\0';

		/* Comprobar que el nombre del pergamino no se haya generado ya */
		for (j = 0; j < i; j++) {
			if (streq(buf, scroll_adj[j])) {
				okay = false;
				break;
			}
		}

		if (okay)
			my_strcpy(scroll_adj[i], buf, sizeof(scroll_adj[0]));
		else
			/* Intentar hacer un nombre otra vez */
			i--;
	}
	flavor_assign_random(TV_SCROLL);

	/* Usar el RNG "complejo" */
	Rand_quick = false;

	/* Analizar cada objeto */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Saltar objetos "vacíos" */
		if (!kind->name) continue;

		/*
		 * Sin sabor y no es un tipo que solo tiene una instancia,
		 * un artefacto, hace que sea conocido
		 */
		if (!kind->flavor && kind->kidx < z_info->ordinary_kind_max) {
			kind->aware = true;
		}
	}
}

/**
 * Establece todos los sabores como conocidos
 */
void flavor_set_all_aware(void)
{
	int i;

	/* Analizar cada objeto */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Saltar objetos vacíos */
		if (!kind->name) continue;

		/* El sabor hace que sea conocido */
		if (kind->flavor) kind->aware = true;
	}
}

/**
 * Devuelve el peso, en 1/10 de libra e incluyendo maldiciones, de un objeto
 * de una pila.
 *
 * obj->weight es solo el peso base y no incluye maldiciones.
 * Las modificaciones al peso de las maldiciones no harán que el peso
 * caiga fuera del rango [0, 32767].
 */
int16_t object_weight_one(const struct object *obj)
{
	int16_t result = MAX(obj->weight, 0);

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result = modify_weight_for_curse(i, result);
			}
		}
	}

	return result;
}

/**
 * Devuelve la bonificación para golpear de un objeto, incluyendo cualquiera de sus maldiciones.
 */
int object_to_hit(const struct object *obj)
{
	int result = obj->to_h;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_h;
			}
		}
	}
	return result;
}

/**
 * Devuelve la bonificación de daño de un objeto, incluyendo cualquiera de sus maldiciones.
 */
int object_to_dam(const struct object *obj)
{
	int result = obj->to_d;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_d;
			}
		}
	}
	return result;
}

/**
 * Devuelve la bonificación de clase de armadura de un objeto, incluyendo cualquiera de sus maldiciones.
 */
int object_to_ac(const struct object *obj)
{
	int result = obj->to_a;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_a;
			}
		}
	}
	return result;
}

/**
 * Obtiene las banderas de un objeto
 */
void object_flags(const struct object *obj, bitflag flags[OF_SIZE])
{
	of_wipe(flags);
	if (!obj) return;
	of_copy(flags, obj->flags);
}


/**
 * Obtiene las banderas de un objeto que son conocidas por el jugador
 */
void object_flags_known(const struct object *obj, bitflag flags[OF_SIZE])
{
	object_flags(obj, flags);
	of_inter(flags, obj->known->flags);

	if (!obj->kind) {
		return;
	}

	if (object_flavor_is_aware(obj)) {
		of_union(flags, obj->kind->flags);
	}

	if (obj->ego && easy_know(obj)) {
		of_union(flags, obj->ego->flags);
		of_diff(flags, obj->ego->flags_off);
	}
}

/**
 * Aplica una función de prueba, saltándose todos los no-objetos y el oro
 */
bool object_test(item_tester tester, const struct object *obj)
{
	/* Requiere objeto */
	if (!obj) return false;

	/* Ignorar oro */
	if (tval_is_money(obj)) return false;

	/* Pasar sin un probador, o llamada final al probador si existe */
	return !tester || tester(obj);
}


/**
 * Devuelve verdadero si el objeto es desconocido (aún no ha sido visto por el jugador).
 */
bool is_unknown(const struct object *obj)
{
	struct grid_data gd = {
		.m_idx = 0,
		.f_idx = 0,
		.first_kind = NULL,
		.trap = NULL,
		.multiple_objects = false,
		.unseen_object = false,
		.unseen_money = false,
		.lighting = LIGHTING_LOS,
		.in_view = false,
		.is_player = false,
		.hallucinate = false,
	};
	map_info(obj->grid, &gd);
	return gd.unseen_object;
}	


/**
 * Busca si "inscrip" está presente en el objeto dado.
 */
unsigned check_for_inscrip(const struct object *obj, const char *inscrip)
{
	unsigned i = 0;
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Necesitar esto implica que hay instancias defectuosas de obj->note por ahí,
	 * pero no he podido rastrear sus orígenes - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;

		i++;
		s++;
	} while (s);

	return i;
}

/**
 * Busca si "inscrip" seguido inmediatamente de un número entero decimal sin un
 * carácter de signo inicial está presente en el objeto dado. Devuelve el número
 * de veces que ocurre dicha inscripción y, si ese valor es al menos uno,
 * establece *ival al valor del entero que seguía a la primera de esas
 * inscripciones.
 */
unsigned check_for_inscrip_with_int(const struct object *obj, const char *inscrip, int* ival)
{
	unsigned i = 0;
	size_t inlen = strlen(inscrip);
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Necesitar esto implica que hay instancias defectuosas de obj->note por ahí,
	 * pero no he podido rastrear sus orígenes - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;
		if (isdigit(s[inlen])) {
			if (i == 0) {
				long inarg = strtol(s + inlen, 0, 10);

				*ival = (inarg < INT_MAX) ? (int) inarg : INT_MAX;
			}
			i++;
		}
		s++;
	} while (s);

	return i;
}

/*** Funciones de búsqueda de tipos de objeto ***/

/**
 * Devuelve el tipo de objeto con el `tval` y `sval` dados, o NULL.
 */
struct object_kind *lookup_kind(int tval, int sval)
{
	int k;

	/* Buscarlo */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		if (kind->tval == tval && kind->sval == sval)
			return kind;
	}

	/* Fallo */
	msg("No hay objeto: %d:%d (%s)", tval, sval, tval_find_name(tval));
	return NULL;
}

struct object_kind *objkind_byid(int kidx) {
	if (kidx < 0 || kidx >= z_info->k_max)
		return NULL;
	return &k_info[kidx];
}


/*** Conversión texto<->numérico ***/

/**
 * Devuelve el a_idx del artefacto con el nombre dado
 */
const struct artifact *lookup_artifact_name(const char *name)
{
	int i;
	int a_idx = -1;

	/* Buscarlo */
	for (i = 0; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];

		/* Probar igualdad */
		if (art->name && streq(name, art->name))
			return art;
		
		/* Probar coincidencias cercanas */
		if (strlen(name) >= 3 && art->name && my_stristr(art->name, name)
			&& a_idx == -1)
			a_idx = i;
	}

	/* Devolver nuestra mejor coincidencia */
	return a_idx > 0 ? &a_info[a_idx] : NULL;
}

/**
 * \param name nombre del tipo de ego
 * \param tval tval del objeto
 * \param sval sval del objeto
 * \return eidx del tipo de objeto de ego
 */
struct ego_item *lookup_ego_item(const char *name, int tval, int sval)
{
	struct object_kind *kind = lookup_kind(tval, sval);
	int i;

	/* Buscarlo */
	if (!kind) return NULL;
	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];
		struct poss_item *poss_item = ego->poss_items;

		/* Rechazar sin nombre y nombres incorrectos */
		if (!ego->name) continue;
		if (!streq(name, ego->name)) continue;

		/* Comprobar tval y sval */
		while (poss_item) {
			if (kind->kidx == poss_item->kidx) {
				return ego;
			}
			poss_item = poss_item->next;
		}
	}

	return NULL;
}

/**
 * Devuelve el sval numérico del tipo de objeto con el `tval` dado y
 * nombre `name`.
 */
int lookup_sval(int tval, const char *name)
{
	int k;
	char *pe;
	unsigned long r = strtoul(name, &pe, 10);

	if (pe != name) {
		return (contains_only_spaces(pe) && r < INT_MAX) ? (int)r : -1;
	}

	/* Buscarlo */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		char cmp_name[1024];

		if (!kind || !kind->name || kind->tval != tval) continue;

		obj_desc_name_format(cmp_name, sizeof cmp_name, 0, kind->name, 0,
							 false);

		/* Encontró una coincidencia */
		if (!my_stricmp(cmp_name, name)) return kind->sval;
	}

	return -1;
}

void object_short_name(char *buf, size_t max, const char *name)
{
	size_t j, k;
	/* Copiar el nombre, eliminando modificadores & y ~) */
	size_t len = strlen(name);
	for (j = 0, k = 0; j < len && k < max - 1; j++) {
		if (j == 0 && name[0] == '&' && name[1] == ' ')
			j += 2;
		if (name[j] == '~')
			continue;

		buf[k++] = name[j];
	}
	buf[k] = 0;
}

/**
 * Comparador de ordenación para objetos usando solo tval y sval.
 * -1 si o1 debería ir primero
 *  1 si o2 debería ir primero
 *  0 si no importa
 */
static int compare_types(const struct object *o1, const struct object *o2)
{
	if (o1->tval == o2->tval)
		return CMP(o1->sval, o2->sval);
	else
		return CMP(o1->tval, o2->tval);
}	


/**
 * Comparador de ordenación para objetos
 * -1 si o1 debería ir primero
 *  1 si o2 debería ir primero
 *  0 si no importa
 *
 * El orden de clasificación está diseñado pensando en el comando "listar objetos".
 */
int compare_items(const struct object *o1, const struct object *o2)
{
	/* los objetos desconocidos van al final, el orden no importa */
	if (is_unknown(o1)) {
		return (is_unknown(o2)) ? 0 : 1;
	} else if (is_unknown(o2)) {
		return -1;
	}

	/* los artefactos conocidos se ordenarán primero */
	if (object_is_known_artifact(o1) && object_is_known_artifact(o2))
		return compare_types(o1, o2);
	if (object_is_known_artifact(o1)) return -1;
	if (object_is_known_artifact(o2)) return 1;

	/* los objetos desconocidos se ordenarán después */
	if (!object_flavor_is_aware(o1) && !object_flavor_is_aware(o2))
		return compare_types(o1, o2);
	if (!object_flavor_is_aware(o1)) return -1;
	if (!object_flavor_is_aware(o2)) return 1;

	/* si solo uno de ellos no tiene valor, el otro va primero */
	if (o1->kind->cost == 0 && o2->kind->cost != 0) return 1;
	if (o1->kind->cost != 0 && o2->kind->cost == 0) return -1;

	/* de lo contrario, simplemente comparar tvals y svals */
	/* NOTA: podría haber un orden mejor que este */
	return compare_types(o1, o2);
}


/**
 * Convierte una profundidad de un chunk o jugador a un valor apropiado para el
 * origen de un objeto.
 *
 * \param depth es el valor a convertir.
 *
 * Necesario ya que los archivos guardados usan un tipo de 16 bits para registrar la profundidad
 * de un jugador o chunk y uint8_t para registrar la profundidad de origen.
 */
uint8_t convert_depth_to_origin(int depth)
{
	if (depth < 0) return 0;
	if (depth > 255) return 255;
	return (uint8_t) depth;
}


/**
 * Determina si un objeto tiene cargas
 */
bool obj_has_charges(const struct object *obj)
{
	if (!tval_can_have_charges(obj)) return false;

	if (obj->pval <= 0) return false;

	return true;
}

/**
 * Determina si un objeto se puede activar
 */
bool obj_can_zap(const struct object *obj)
{
	/* ¿Alguna vara no está cargando? */
	if (tval_can_have_timeout(obj) && number_charging(obj) < obj->number)
		return true;

	return false;
}

/**
 * Determina si un objeto es activable
 */
bool obj_is_activatable(const struct object *obj)
{
	if (!tval_is_wearable(obj)) return false;
	return object_effect(obj) ? true : false;
}

/**
 * Determina si un objeto se puede activar ahora
 */
bool obj_can_activate(const struct object *obj)
{
	if (obj_is_activatable(obj)) {
		/* Comprobar la recarga */
		if (!obj->timeout) return true;
	}

	return false;
}

/**
 * Comprueba si un objeto se puede usar para reabastecer de combustible a otros objetos.
 */
bool obj_can_refill(const struct object *obj)
{
	const struct object *light = equipped_item_by_slot_name(player, "light");

	/* ¿Necesita combustible? */
	if (of_has(obj->flags, OF_NO_FUEL)) return false;

	/* Un farol puede reabastecerse con una frasco u otro farol */
	if (light && of_has(light->flags, OF_TAKES_FUEL)) {
		if (tval_is_fuel(obj))
			return true;
		else if (tval_is_light(obj) && of_has(obj->flags, OF_TAKES_FUEL) &&
				 obj->timeout > 0) 
			return true;
	}

	return false;
}

bool obj_kind_can_browse(const struct object_kind *kind)
{
	int i;

	for (i = 0; i < player->class->magic.num_books; i++) {
		struct class_book book = player->class->magic.books[i];
		if (kind->tval == book.tval && kind->sval == book.sval)
			return true;
	}

	return false;
}

bool obj_can_browse(const struct object *obj)
{
	return obj_kind_can_browse(obj->kind);
}

bool obj_can_cast_from(const struct object *obj)
{
	return obj_can_browse(obj) &&
		spell_book_count_spells(player, obj, spell_okay_to_cast) > 0;
}

bool obj_can_study(const struct object *obj)
{
	return obj_can_browse(obj) &&
		spell_book_count_spells(player, obj, spell_okay_to_study) > 0;
}


/* Solo se pueden quitar objetos no malditos */
bool obj_can_takeoff(const struct object *obj)
{
	return !obj_has_flag(obj, OF_STICKY);
}

/*
 * Solo se puede lanzar un objeto que no esté equipado o el arma equipada si se
 * puede quitar.
 */
bool obj_can_throw(const struct object *obj)
{
	return !object_is_equipped(player->body, obj)
		|| (tval_is_melee_weapon(obj) && obj_can_takeoff(obj));
}

/* Solo se pueden poner objetos equipables */
bool obj_can_wear(const struct object *obj)
{
	return (wield_slot(obj) >= 0);
}

/* Solo se puede disparar un objeto con el tval correcto */
bool obj_can_fire(const struct object *obj)
{
	return obj->tval == player->state.ammo_tval;
}

/**
 * Determina si un objeto está diseñado para ser lanzado
 */
bool obj_is_throwing(const struct object *obj)
{
	return of_has(obj->flags, OF_THROWING);
}

/**
 * Determina si un objeto es un artefacto conocido
 */
bool obj_is_known_artifact(const struct object *obj)
{
	if (!obj->artifact) return false;
	if (!obj->known) return false;
	if (!obj->known->artifact) return false;
	return true;
}

/* ¿Tiene inscripción, por favor? */
bool obj_has_inscrip(const struct object *obj)
{
	return (obj->note ? true : false);
}

bool obj_has_flag(const struct object *obj, int flag)
{
	struct curse_data *c = obj->curses;

	/* Comprobar las propias banderas del objeto */
	if (of_has(obj->flags, flag)) {
		return true;
	}

	/* Comprobar las banderas de cualquier objeto de maldición */
	if (c) {
		int i;
		for (i = 1; i < z_info->curse_max; i++) {
			if (c[i].power && of_has(curses[i].obj->flags, flag)) {
				return true;
			}
		}
	}
	return false;
}

bool obj_is_useable(const struct object *obj)
{
	if (tval_is_useable(obj))
		return true;

	if (object_effect(obj))
		return true;

	if (tval_is_ammo(obj))
		return obj->tval == player->state.ammo_tval;

	return false;
}

/*** Funciones de utilidad genéricas ***/

/**
 * Devuelve el efecto de un objeto.
 */
struct effect *object_effect(const struct object *obj)
{
	if (obj->activation)
		return obj->activation->effect;
	else if (obj->effect)
		return obj->effect;
	else
		return NULL;
}

/**
 * ¿El objeto dado necesita ser apuntado?
 */ 
bool obj_needs_aim(const struct object *obj)
{
	const struct effect *effect = object_effect(obj);

	/* Si el efecto necesita apuntar, o si el tipo de objeto necesita
	   apuntar, este objeto necesita apuntar. */
	return effect_aim(effect) || tval_is_ammo(obj) ||
			tval_is_wand(obj) ||
			(tval_is_rod(obj) && !object_flavor_is_aware(obj));
}

/**
 * ¿Puede el objeto fallar si se usa?
 */
bool obj_can_fail(const struct object *o)
{
	if (tval_can_have_failure(o))
		return true;

	return wield_slot(o) == -1 ? false : true;
}


/**
 * Tasa de fallo para dispositivos mágicos.
 * Esto ha sido reescrito para 4.2.3 siguiendo las discusiones en el hilo
 * https://angband.live/forums/forum/angband/development/9911-please-help-md-negative-value
 * Utiliza una versión escalada y desplazada de la función sigmoide x/(1+|x|), a saber
 * 380 - 370(x/(5+|x|)), donde x es 2 * (habilidad de dispositivo - nivel del dispositivo) + 1,
 * para dar tasas de fallo sobre 1000.
 */
int get_use_device_chance(const struct object *obj)
{
	int lev, fail, x;
	int skill = player->state.skills[SKILL_DEVICE];

	/* Extraer el nivel del objeto, que es la calificación de dificultad */
	lev = obj->artifact ? obj->artifact->level :
		(obj->activation ? obj->activation->level : obj->kind->level);

	/* Calcular x */
	x = 2 * (skill - lev) + 1;

	/* Ahora calcular la tasa de fallo */
	fail = -370 * x;
	fail /= (5 + ABS(x));
	fail += 380;

	return fail;
}


/**
 * Distribuye las cargas de varas, bastones o varitas.
 *
 * \param source es el objeto fuente
 * \param dest es el objeto destino, debe ser del mismo tipo que source
 * \param amt es el número de objetos que se transfieren
 * \param dest_new si es verdadero, ignorará las cargas o tiempo de espera que
 * dest tenga (es decir, lo tratará como una pila nueva).
 */
void distribute_charges(struct object *source, struct object *dest, int amt,
		bool dest_new)
{
	/*
	 * Si se sueltan varas, bastones o varitas, el tiempo de espera total máximo
	 * o las cargas deben asignarse entre las dos pilas.
	 * Si se están soltando todos los objetos, hace que el mensaje sea más limpio
	 * dejar el pval de la pila original sin cambios. -LM-
	 */
	if (tval_can_have_charges(source)) {
		int change = source->pval * amt / source->number;

		if (dest_new) {
			dest->pval = change;
		} else {
			dest->pval += change;
		}
		if (amt < source->number) {
			source->pval -= change;
		}
	}

	/*
	 * Las varas también necesitan que se distribuyan sus tiempos de espera.
	 *
	 * La pila soltada aceptará todo el tiempo restante para cargar hasta
	 * su máximo.
	 */
	if (tval_can_have_timeout(source)) {
		int charge_time = randcalc(source->time, 0, AVERAGE), max_time;

		max_time = charge_time * amt;
		if (dest_new) {
			dest->timeout = (source->timeout > max_time)
				? max_time : source->timeout;
			if (amt < source->number) {
				source->timeout -= dest->timeout;
			}
		} else {
			int change = (source->timeout > max_time)
				? max_time : source->timeout;

			max_time = charge_time * (dest->number + amt);
			if (dest->timeout < max_time) {
				if (change > max_time - dest->timeout) {
					change = max_time - dest->timeout;
				}
				dest->timeout += change;
				if (amt < source->number) {
					source->timeout -= change;
				}
			}
		}
	}
}


/**
 * Número de objetos (generalmente varas) que se están cargando
 */
int number_charging(const struct object *obj)
{
	int charge_time, num_charging;

	charge_time = randcalc(obj->time, 0, AVERAGE);

	/* El objeto no tiene tiempo de espera */
	if (charge_time <= 0) return 0;

	/* No hay objetos cargando */
	if (obj->timeout <= 0) return 0;

	/* Calcular el número de objetos cargando basado en el tiempo de espera */
	num_charging = (obj->timeout + charge_time - 1) / charge_time;

	/* El número de objetos cargando no puede exceder el tamaño de la pila */
	if (num_charging > obj->number) num_charging = obj->number;

	return num_charging;
}

/**
 * Permite que una pila de objetos que se están cargando se cargue una unidad por objeto cargando
 * Devuelve verdadero si algo se recargó
 */
bool recharge_timeout(struct object *obj)
{
	int charging_before, charging_after;

	/* Encontrar el número de objetos cargando */
	charging_before = number_charging(obj);

	/* Nada que cargar */	
	if (charging_before == 0)
		return false;

	/* Disminuir el tiempo de espera */
	obj->timeout -= MIN(charging_before, obj->timeout);

	/* Encontrar el nuevo número de objetos cargando */
	charging_after = number_charging(obj);

	/* Devolver verdadero si al menos 1 objeto obtuvo una carga */
	if (charging_after < charging_before)
		return true;
	else
		return false;
}

/**
 * Verifica la elección de un objeto.
 *
 * El objeto puede ser negativo para significar "objeto en el suelo".
 */
bool verify_object(const char *prompt, const struct object *obj,
		const struct player *p)
{
	char o_name[80];

	char out_val[160];

	/* Describir */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL, p);

	/* Preguntar */
	strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

	/* Consulta */
	return (get_check(out_val));
}


typedef enum {
	MSG_TAG_NONE,
	MSG_TAG_NAME,
	MSG_TAG_KIND,
	MSG_TAG_VERB,
	MSG_TAG_VERB_IS
} msg_tag_t;

static msg_tag_t msg_tag_lookup(const char *tag)
{
	if (strncmp(tag, "name", 4) == 0) {
		return MSG_TAG_NAME;
	} else if (strncmp(tag, "kind", 4) == 0) {
		return MSG_TAG_KIND;
	} else if (strncmp(tag, "s", 1) == 0) {
		return MSG_TAG_VERB;
	} else if (strncmp(tag, "is", 2) == 0) {
		return MSG_TAG_VERB_IS;
	} else {
		return MSG_TAG_NONE;
	}
}

/**
 * Imprime un mensaje a partir de una cadena, personalizado para incluir detalles sobre un objeto
 */
void print_custom_message(const struct object *obj, const char *string,
		int msg_type, const struct player *p)
{
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	size_t end = 0;

	/* No siempre es una cadena */
	if (!string) return;

	next = strchr(string, '{');
	while (next) {
		/* Copiar el texto que lleva hasta este { */
		strnfcat(buf, 1024, &end, "%.*s", (int) (next - string),
			string);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Etiqueta válida */
		if (*s == '}') {
			/* Comenzar la etiqueta después del { */
			tag = next + 1;
			string = s + 1;

			switch(msg_tag_lookup(tag)) {
			case MSG_TAG_NAME:
				if (obj) {
					end += object_desc(buf, 1024, obj,
						ODESC_PREFIX | ODESC_BASE, p);
				} else {
					strnfcat(buf, 1024, &end, "hands"); // Todo revisar como aplicar Fix traduc
				}
				break;
			case MSG_TAG_KIND:
				if (obj) {
					object_kind_name(&buf[end], 1024 - end, obj->kind, true);
					end += strlen(&buf[end]);
				} else {
					strnfcat(buf, 1024, &end, "hands"); // Todo revisar como aplicar Fix traduc
				}
				break;
			case MSG_TAG_VERB:
				if (obj && obj->number == 1) {
					strnfcat(buf, 1024, &end, "s");
				}
				break;
			case MSG_TAG_VERB_IS:
				if ((!obj) || (obj->number > 1)) {
					strnfcat(buf, 1024, &end, "are"); // Todo revisar como aplicar Fix traduc
				} else {
					strnfcat(buf, 1024, &end, "is"); // Todo revisar como aplicar Fix traduc
				}
			default:
				break;
			}
		} else
			/* Una etiqueta no válida, omitirla */
			string = next + 1;

		next = strchr(string, '{');
	}
	strnfcat(buf, 1024, &end, "%s", string);

	msgt(msg_type, "%s", buf);
}

/**
 * Devuelve si el artefacto dado ha sido creado.
 */
bool is_artifact_created(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].created;
}

/**
 * Devuelve si el artefacto dado ha sido visto.
 */
bool is_artifact_seen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].seen;
}

/**
 * Devuelve si el artefacto dado ha sido visto alguna vez.
 */
bool is_artifact_everseen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].everseen;
}

/**
 * Establece si el artefacto dado ha sido creado o no.
 */
void mark_artifact_created(const struct artifact *art, bool created)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].created = created;
}

/**
 * Establece si el artefacto dado ha sido visto o no.
 */
void mark_artifact_seen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].seen = seen;
}

/**
 * Establece si el artefacto dado ha sido visto o no.
 */
void mark_artifact_everseen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].everseen = seen;
}