/**
 * \file obj-desc.c
 * \brief Crear descripciones de nombres de objetos
 *
 * Copyright (c) 1997 - 2007 Angband contributors
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
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-tval.h"
#include "obj-util.h"

/**
 * Pone el nombre del tipo base del objeto en buf.
 */
void object_base_name(char *buf, size_t max, int tval, bool plural)
{
	struct object_base *kb = &kb_info[tval];
	size_t end = 0;

	if (kb->name && kb->name[0]) 
		(void) obj_desc_name_format(buf, max, end, kb->name, NULL, plural);
}


/**
 * Pone una versión muy simplificada del nombre de un objeto en buf.
 * Si easy_know es true, se usan los nombres identificados; de lo contrario,
 * se usarán sabores, nombres de pergaminos, etc.
 *
 * Simplemente trunca si el búfer no es lo suficientemente grande.
 */
void object_kind_name(char *buf, size_t max, const struct object_kind *kind,
					  bool easy_know)
{
	/* Si no es consciente, el sabor simple (ej. Cobre) servirá. */
	if (!easy_know && !kind->aware && kind->flavor)
		my_strcpy(buf, kind->flavor->text, max);

	/* Usar nombre propio (Curación, o lo que sea) */
	else
		(void) obj_desc_name_format(buf, max, 0, kind->name, NULL, false);
}


/**
 * Una cadena modificadora, colocada donde va '#' en el nombre base a continuación.
 * Los extraños juegos con los nombres de los libros son para permitir que la parte
 * no esencial del nombre se pueda abreviar cuando no hay mucho espacio para mostrar.
 */
static const char *obj_desc_get_modstr(const struct object_kind *kind)
{
	if (tval_can_have_flavor_k(kind))
		return kind->flavor ? kind->flavor->text : "";

	if (tval_is_book_k(kind))
		return kind->name;

	return "";
}

/**
 * El nombre básico de un objeto - un nombre genérico para objetos con sabor
 * (con el nombre real añadido después dependiendo del conocimiento), el nombre
 * de object.txt para casi todo lo demás, y un poco extra para los libros.
 */
static const char *obj_desc_get_basename(const struct object *obj, bool aware,
		bool terse, uint32_t mode, const struct player *p)
{
	bool show_flavor = !terse && obj->kind->flavor;

	if (mode & ODESC_STORE)
		show_flavor = false;
	if (aware && p && !OPT(p, show_flavors)) show_flavor = false;

	/* Los artefactos son especiales */
	if (obj->artifact && (aware || object_is_known_artifact(obj) || terse ||
						  !obj->kind->flavor))
		return obj->kind->name;

	/* Analizar el objeto */
	switch (obj->tval)
	{
		case TV_FLASK:
		case TV_CHEST:
		case TV_SHOT:
		case TV_BOLT:
		case TV_ARROW:
		case TV_BOW:
		case TV_HAFTED:
		case TV_POLEARM:
		case TV_SWORD:
		case TV_DIGGING:
		case TV_BOOTS:
		case TV_GLOVES:
		case TV_CLOAK:
		case TV_CROWN:
		case TV_HELM:
		case TV_SHIELD:
		case TV_SOFT_ARMOR:
		case TV_HARD_ARMOR:
		case TV_DRAG_ARMOR:
		case TV_LIGHT:
		case TV_FOOD:
			return obj->kind->name;

		case TV_AMULET:
			return (show_flavor ? "& # Amuleto~" : "& Amuleto~");

		case TV_RING:
			return (show_flavor ? "& # Anillo~" : "& Anillo~");

		case TV_STAFF:
			return (show_flavor ? "& # Báculo~" : "& Báculo~");

		case TV_WAND:
			return (show_flavor ? "& # Varita~" : "& Varita~");

		case TV_ROD:
			return (show_flavor ? "& # Vara~" : "& Vara~");

		case TV_POTION:
			return (show_flavor ? "& # Poción~" : "& Poción~");

		case TV_SCROLL:
			return (show_flavor ? "& Pergamino~ titulado #" : "& Pergamino~");

		case TV_MAGIC_BOOK:
			if (terse)
				return "& Libro~ #";
			else
				return "& Libro~ de Hechizos Mágicos #";

		case TV_PRAYER_BOOK:
			if (terse)
				return "& Libro~ #";
			else
				return "& Libro Sagrado~ de Plegarias #";

		case TV_NATURE_BOOK:
			if (terse)
				return "& Libro~ #";
			else
				return "& Libro~ de Magias Naturales #";

		case TV_SHADOW_BOOK:
			if (terse)
				return "& T o m o~ #";
			else
				return "& T o m o~ Nigromántico #";

		case TV_OTHER_BOOK:
			if (terse)
				return "& Libro~ #";
			else
				return "& Libro de Misterios~ #";

		case TV_MUSHROOM:
			return (show_flavor ? "& # Seta~" : "& Seta~");
	}

	return "(nada)";
}


/**
 * Comienzo de la descripción, indicando número/unicidad (un, el, no más, 7, etc.)
 */
static size_t obj_desc_name_prefix(char *buf, size_t max, size_t end,
		const struct object *obj, const char *basename,
		const char *modstr, bool terse, uint16_t number)
{
	if (number == 0) {
		strnfcat(buf, max, &end, "no más ");
	} else if (number > 1) {
		strnfcat(buf, max, &end, "%u ", number);
	} else if (object_is_known_artifact(obj)) {
		strnfcat(buf, max, &end, "el ");
	} else if (*basename == '&') {
		bool an = false;
		const char *lookahead = basename + 1;

		while (*lookahead == ' ') lookahead++;

		if (*lookahead == '#') {
			if (modstr && is_a_vowel(*modstr))
				an = true;
		} else if (is_a_vowel(*lookahead)) {
			an = true;
		}

		if (!terse) {
			if (an)
				strnfcat(buf, max, &end, "un ");
			else
				strnfcat(buf, max, &end, "un ");			
		}
	}

	return end;
}



/**
 * Formatea 'fmt' en 'buf', con los siguientes caracteres de formato:
 *
 * '~' al final de una palabra (ej. "frigorífico~") pluralizará
 *
 * '|x|y|' se mostrará como 'x' si es singular o 'y' si es plural
 *    (ej. "cuchi|llo|llos|")
 *
 * '#' será reemplazado por 'modstr' (que puede contener los formatos de pluralización
 * dados arriba).
 */
size_t obj_desc_name_format(char *buf, size_t max, size_t end,
		const char *fmt, const char *modstr, bool pluralise)
{
	/* Copiar la cadena */
	while (*fmt) {
		/* Saltar */
		if (*fmt == '&') {
			while (*fmt == ' ' || *fmt == '&')
				fmt++;
			continue;
		} else if (*fmt == '~') {
			/* Pluralizador (plurales regulares en inglés/español) */
			char prev = *(fmt - 1);

			if (!pluralise)	{
				fmt++;
				continue;
			}

			/* En español, normalmente solo se añade 's' o 'es' */
			/* Nota: Esto es una simplificación; el juego usa reglas de pluralización en inglés,
			   pero adaptamos a español de forma básica. */
			if (prev == 's' || prev == 'h' || prev == 'x' || prev == 'z')
				strnfcat(buf, max, &end, "es");
			else
				strnfcat(buf, max, &end, "s");
		} else if (*fmt == '|') {
			/* Plurales especiales 
			* ej. cuchi|llo|llos|
			*          ^   ^    ^ */
			const char *singular = fmt + 1;
			const char *plural   = strchr(singular, '|');
			const char *endmark  = NULL;

			if (plural) {
				plural++;
				endmark = strchr(plural, '|');
			}

			if (!singular || !plural || !endmark) return end;

			if (!pluralise)
				strnfcat(buf, max, &end, "%.*s",
					(int) (plural - singular) - 1,
					singular);
			else
				strnfcat(buf, max, &end, "%.*s",
					(int) (endmark - plural), plural);

			fmt = endmark;
		} else if (*fmt == '#' && modstr) {
			/* Añadir modstr, con pluralización si es relevante */
			end = obj_desc_name_format(buf, max, end, modstr, NULL,	pluralise);
		}

		else
			buf[end++] = *fmt;

		fmt++;
	}

	buf[end] = 0;

	return end;
}


/**
 * Formatear el nombre del objeto obj en 'buf'.
 */
static size_t obj_desc_name(char *buf, size_t max, size_t end,
		const struct object *obj, bool prefix, uint32_t mode,
		bool terse, const struct player *p)
{
	bool store = mode & ODESC_STORE ? true : false;
	bool spoil = mode & ODESC_SPOIL ? true : false;
	uint16_t number = (mode & ODESC_ALTNUM) ?
		(mode & 0xFFFF0000) >> 16 : obj->number;
	
	/* Nombre real para objetos con sabor si se es consciente, o en tienda, o en spoiler */
	bool aware = object_flavor_is_aware(obj) || store || spoil;
	/* Pluralizar si (no forzado a singular) y
	 * (no es un artefacto conocido/visible) y
	 * (no es uno en el montón o forzado a plural) */
	bool plural = !(mode & ODESC_SINGULAR) &&
		!obj->artifact &&
		(number != 1 || (mode & ODESC_PLURAL));
	const char *basename = obj_desc_get_basename(obj, aware, terse,
		mode, p);
	const char *modstr = obj_desc_get_modstr(obj->kind);

	/* Prefijo de cantidad */
	if (prefix)
		end = obj_desc_name_prefix(buf, max, end, obj, basename,
			modstr, terse, number);

	/* Nombre base */
	end = obj_desc_name_format(buf, max, end, basename, modstr, plural);

	/* Añadir nombres extra de varios tipos */
	if (object_is_known_artifact(obj))
		strnfcat(buf, max, &end, " %s", obj->artifact->name);
	else if ((obj->known->ego && !(mode & ODESC_NOEGO)) || (obj->ego && store))
		strnfcat(buf, max, &end, " %s", obj->ego->name);
	else if (aware && !obj->artifact &&
			 (obj->kind->flavor || obj->kind->tval == TV_SCROLL)) {
		if (terse)
			strnfcat(buf, max, &end, " '%s'", obj->kind->name);
		else
			strnfcat(buf, max, &end, " de %s", obj->kind->name);
	}

	return end;
}

/**
 * ¿Es obj una armadura?
 */
static bool obj_desc_show_armor(const struct object *obj,
		const struct player *p)
{
	return (!p || p->obj_k->ac) && (obj->ac || tval_is_armor(obj));
}

/**
 * Descripciones especiales para tipos de trampas de cofre
 */
static size_t obj_desc_chest(const struct object *obj, char *buf, size_t max,
							 size_t end)
{
	if (!tval_is_chest(obj)) return end;

	/* El cofre no está abierto, pero no sabemos nada sobre su trampa/cerradura */
	if (obj->pval && !obj->known->pval) return end;

	/* Describir las trampas */
	strnfcat(buf, max, &end, " (%s)", chest_trap_name(obj));

	return end;
}

/**
 * Describir propiedades de combate de un objeto - dados de daño, para-golpear, para-dañar, clase de armadura, multiplicador de proyectiles
 */
static size_t obj_desc_combat(const struct object *obj, char *buf, size_t max, 
		size_t end, uint32_t mode, const struct player *p)
{
	bool spoil = mode & ODESC_SPOIL ? true : false;
	int to_h, to_d, to_a;

	/* Mostrar dados de daño si se conocen */
	if (kf_has(obj->kind->kind_flags, KF_SHOW_DICE) &&
		(!p || (p->obj_k->dd && p->obj_k->ds))) {
		strnfcat(buf, max, &end, " (%dd%d)", obj->dd, obj->ds);
	}

	/* Mostrar poder de disparo como parte del multiplicador */
	if (kf_has(obj->kind->kind_flags, KF_SHOW_MULT)) {
		strnfcat(buf, max, &end, " (x%d)",
				 obj->pval + obj->modifiers[OBJ_MOD_MIGHT]);
	}

	/* No más si el objeto no ha sido evaluado */
	if (!((obj->notice & OBJ_NOTICE_ASSESSED) || spoil)) return end;

	to_h = object_to_hit(obj);
	to_d = object_to_dam(obj);
	to_a = object_to_ac(obj);

	/* Mostrar bonificaciones de arma si conocemos alguna */
	if ((!p || (p->obj_k->to_h && p->obj_k->to_d))
			&& (tval_is_weapon(obj) || to_d
			|| (to_h && !tval_is_body_armor(obj))
			|| ((!object_has_standard_to_h(obj)
			|| obj->to_h != to_h)
			&& !obj->artifact && !obj->ego))) {
		/* En general mostrar bonificaciones completas de combate */
		strnfcat(buf, max, &end, " (%+d,%+d)", to_h, to_d);
	} else if (obj->to_h < 0 && object_has_standard_to_h(obj)) {
		/* Tratamiento especial para armaduras corporales con solo penalización para-golpear */
		strnfcat(buf, max, &end, " (%+d)", obj->to_h);
	} else if (to_d != 0 && (!p || p->obj_k->to_d)) {
		/* Solo runa para-dañar conocida */
		strnfcat(buf, max, &end, " (%+d)", to_d);
	} else if (to_h != 0 && (!p || p->obj_k->to_h)) {
		/* Solo runa para-golpear conocida */
		strnfcat(buf, max, &end, " (%+d)", to_h);
	}

	/* Mostrar bonificaciones de armadura */
	if (!p || p->obj_k->to_a) {
		if (obj_desc_show_armor(obj, p))
			strnfcat(buf, max, &end, " [%d,%+d]", obj->ac, to_a);
		else if (to_a)
			strnfcat(buf, max, &end, " [%+d]", to_a);
	} else if (obj_desc_show_armor(obj, p)) {
		strnfcat(buf, max, &end, " [%d]", obj->ac);
	}

	return end;
}

/**
 * Describir luz restante para fuentes de luz recargables
 */
static size_t obj_desc_light(const struct object *obj, char *buf, size_t max,
							 size_t end)
{
	/* Las fuentes de luz con combustible tienen el número de turnos restantes añadido */
	if (tval_is_light(obj) && !of_has(obj->flags, OF_NO_FUEL))
		strnfcat(buf, max, &end, " (%d turnos)", obj->timeout);

	return end;
}

/**
 * Describir modificadores numéricos a estadísticas y otras cualidades del jugador que
 * permiten bonificaciones numéricas - velocidad, sigilo, etc.
 */
static size_t obj_desc_mods(const struct object *obj, char *buf, size_t max,
							size_t end)
{
	int i, j, num_mods = 0;
	int mods[OBJ_MOD_MAX] = { 0 };

	/* Recorrer posibles modificadores y almacenar los distintos */
	for (i = 0; i < OBJ_MOD_MAX; i++) {
		/* Verificar modificadores conocidos no nulos */
		if (obj->modifiers[i] != 0) {
			/* Si aún no hay modificadores almacenados, almacenar y continuar */
			if (!num_mods) {
				mods[num_mods++] = obj->modifiers[i];
				continue;
			}

			/* Recorrer los modificadores existentes, salir si hay duplicados */
			for (j = 0; j < num_mods; j++)
				if (mods[j] == obj->modifiers[i]) break;

			/* Añadir otro modificador si es necesario */
			if (j == num_mods)
				mods[num_mods++] = obj->modifiers[i];
		}
	}

	if (!num_mods) return end;

	/* Imprimir los modificadores */
	strnfcat(buf, max, &end, " <");
	for (j = 0; j < num_mods; j++) {
		if (j) strnfcat(buf, max, &end, ", ");
		strnfcat(buf, max, &end, "%+d", mods[j]);
	}
	strnfcat(buf, max, &end, ">");

	return end;
}

/**
 * Describir cargas o estado de recarga para objetos reutilizables con efectos mágicos
 */
static size_t obj_desc_charges(const struct object *obj, char *buf, size_t max,
		size_t end, uint32_t mode)
{
	bool aware = object_flavor_is_aware(obj) || (mode & ODESC_STORE);

	/* Las varitas y báculos tienen cargas, otros pueden estar recargándose */
	if (aware && tval_can_have_charges(obj)) {
		strnfcat(buf, max, &end, " (%d carga%s)", obj->pval,
				 PLURAL(obj->pval));
	} else if (obj->timeout > 0) {
		if (tval_is_rod(obj) && obj->number > 1)
			strnfcat(buf, max, &end, " (%d recargándose)", number_charging(obj));
		else if (tval_is_rod(obj) || obj->activation || obj->effect)
			/* Artefactos, varas individuales */
			strnfcat(buf, max, &end, " (recargándose)");
	}

	return end;
}

/**
 * Añadir inscripciones definidas por el jugador o descripciones definidas por el juego
 */
static size_t obj_desc_inscrip(const struct object *obj, char *buf,
		size_t max, size_t end, const struct player *p)
{
	const char *u[6] = { 0, 0, 0, 0, 0, 0 };
	int n = 0;

	/* Obtener inscripción */
	if (obj->note)
		u[n++] = quark_str(obj->note);

	/* Usar inscripción especial, si la hay */
	if (!object_flavor_is_aware(obj)) {
		if (tval_can_have_charges(obj) && (obj->pval == 0))
			u[n++] = "vacío";
		if (object_flavor_was_tried(obj))
			u[n++] = "probado";
	}

	/* Notar maldiciones */
	if (obj->known->curses)
		u[n++] = "maldito";

	/* Notar ignorar */
	if (p && ignore_item_ok(p, obj))
		u[n++] = "ignorar";

	/* Notar propiedades desconocidas */
	if (!object_runes_known(obj) && (obj->known->notice & OBJ_NOTICE_ASSESSED))
		u[n++] = "??";

	if (n) {
		int i;
		for (i = 0; i < n; i++) {
			if (i == 0)
				strnfcat(buf, max, &end, " {");
			strnfcat(buf, max, &end, "%s", u[i]);
			if (i < n - 1)
				strnfcat(buf, max, &end, ", ");
		}

		strnfcat(buf, max, &end, "}");
	}

	return end;
}


/**
 * Añadir "no visto" al final de objetos no identificados en tiendas,
 * y "??" a objetos sin sabor no completamente conocidos
 */
static size_t obj_desc_aware(const struct object *obj, char *buf, size_t max,
							 size_t end)
{
	if (!object_flavor_is_aware(obj)) {
		strnfcat(buf, max, &end, " {no visto}");
	} else if (!object_runes_known(obj)) {
		strnfcat(buf, max, &end, " {??}");
	} else if (obj->known->curses) {
		strnfcat(buf, max, &end, " {maldito}");
	}

	return end;
}


/**
 * Describe el objeto `obj` en el búfer `buf` de tamaño `max`.
 *
 * \param buf es el búfer para la descripción. Debe tener espacio para al menos
 * max bytes.
 * \param max es el tamaño del búfer, en bytes.
 * \param obj es el objeto a describir.
 * \param mode debe ser una combinación bitwise de cero o uno más de los siguientes:
 * ODESC_PREFIX antepone un 'el', 'un' o número
 * ODESC_BASE resulta en una descripción base.
 * ODESC_COMBAT añadirá información de para-golpear, para-dañar y CA.
 * ODESC_EXTRA añadirá información de pval/carga/inscripción/ignorar.
 * ODESC_PLURAL pluralizará independientemente del número en el montón.
 * ODESC_STORE desactiva marcadores de ignorar, para visualización en tienda.
 * ODESC_SPOIL trata el objeto como completamente identificado.
 * ODESC_CAPITAL pone en mayúscula el nombre del objeto.
 * ODESC_TERSE hace que se use un nombre conciso.
 * ODESC_NOEGO omite los nombres de égida.
 * ODESC_ALTNUM hace que los 16 bits superiores de mode se usen como el número
 * de objetos en lugar de usar obj->number. Nótese que usar ODESC_ALTNUM
 * no es completamente compatible con ODESC_EXTRA: la visualización del número de varas
 * recargándose no tiene en cuenta el número alternativo.
 * \param p es el jugador cuyo conocimiento se tiene en cuenta en la descripción.
 * Si p es NULL, la descripción es para un observador omnisciente.
 *
 * \returns El número de bytes usados del búfer.
 */
size_t object_desc(char *buf, size_t max, const struct object *obj,
		uint32_t mode, const struct player *p)
{
	bool prefix = mode & ODESC_PREFIX ? true : false;
	bool spoil = mode & ODESC_SPOIL ? true : false;
	bool terse = mode & ODESC_TERSE ? true : false;

	size_t end = 0;

	/* Descripción simple para objeto nulo */
	if (!obj || !obj->known)
		return strnfmt(buf, max, "(nada)");

	/* Los objetos desconocidos y el dinero obtienen descripciones directas */
	if (obj->known && obj->kind != obj->known->kind) {
		if (prefix)
			return strnfmt(buf, max, "un objeto desconocido");
		return strnfmt(buf, max, "objeto desconocido");
	}

	if (tval_is_money(obj))
		return strnfmt(buf, max, "%d piezas de oro en %s%s",
				obj->pval, obj->kind->name,
				ignore_item_ok(p, obj) ? " {ignorar}" : "");

	/* Las égidas y tipos cuyo nombre conocemos se ven */
	if (obj->known->ego && !spoil)
		obj->ego->everseen = true;

	if (object_flavor_is_aware(obj) && !spoil)
		obj->kind->everseen = true;

	/** Construir el nombre **/

	/* Copiar el nombre base al búfer */
	end = obj_desc_name(buf, max, end, obj, prefix, mode, terse, p);

	/* Propiedades de combate */
	if (mode & ODESC_COMBAT) {
		if (tval_is_chest(obj))
			end = obj_desc_chest(obj, buf, max, end);
		else if (tval_is_light(obj))
			end = obj_desc_light(obj, buf, max, end);

		end = obj_desc_combat(obj->known, buf, max, end, mode, p);
	}

	/* Modificadores, cargas, detalles de sabor, inscripciones */
	if (mode & ODESC_EXTRA) {
		end = obj_desc_mods(obj->known, buf, max, end);

		end = obj_desc_charges(obj, buf, max, end, mode);

		if (mode & ODESC_STORE)
			end = obj_desc_aware(obj, buf, max, end);
		else
			end = obj_desc_inscrip(obj, buf, max, end, p);
	}

	return end;
}