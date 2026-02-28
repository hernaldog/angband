/**
 * \file mon-desc.c
 * \brief Descripción de monstruos
 *
 * Copyright (c) 1997-2007 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "game-input.h"
#include "mon-desc.h"
#include "mon-predicate.h"

/**
 * Realizar pluralización simple en inglés en un nombre de monstruo.
 */
void plural_aux(char *name, size_t max)
{
	size_t name_len = strlen(name);
	assert(name_len != 0);

	if (name[name_len - 1] == 's')
		my_strcat(name, "es", max);
	else
		my_strcat(name, "s", max);
}


/**
 * Función auxiliar para mostrar la lista de monstruos. Imprime el número de criaturas,
 * seguido de una versión singular o plural del nombre de la raza según
 * corresponda.
 */
void get_mon_name(char *buf, size_t buflen,
				  const struct monster_race *race, int num)
{
	assert(race != NULL);

    /* Los nombres únicos no tienen un número */
	if (rf_has(race->flags, RF_UNIQUE)) {
		strnfmt(buf, buflen, "[U] %s", race->name);
    } else {
	    strnfmt(buf, buflen, "%3d ", num);

	    if (num == 1) {
	        my_strcat(buf, race->name, buflen);
	    } else if (race->plural != NULL) {
	        my_strcat(buf, race->plural, buflen);
	    } else {
	        my_strcat(buf, race->name, buflen);
	        plural_aux(buf, buflen);
	    }
    }
}

/**
 * Construye una cadena describiendo un monstruo de alguna manera.
 *
 * Podemos describir correctamente monstruos basándonos en su visibilidad.
 * Podemos forzar que todos los monstruos sean tratados como visibles o invisibles.
 * Podemos construir nominativos, objetivos, posesivos o reflexivos.
 * Podemos pronominalizar selectivamente monstruos ocultos, visibles o todos.
 * Podemos usar descripciones definidas o indefinidas para monstruos ocultos.
 * Podemos usar descripciones definidas o indefinidas para monstruos visibles.
 *
 * La pronominalización implica el género siempre que sea posible y esté permitido,
 * de modo que solicitando ingeniosamente pronominalización / visibilidad, puedes
 * obtener mensajes como "Golpeas a alguien. ¡Ella grita de agonía!".
 *
 * Los reflexivos se obtienen solicitando Objetivo más Posesivo.
 *
 * Nótese que los monstruos "fuera de pantalla" obtendrán una notación especial
 * "(fuera de pantalla)" en su nombre si son visibles pero están fuera de pantalla.
 * Esto puede verse tonto con posesivos, como en "la rata's (fuera de pantalla)".
 * Quizás el descriptor "fuera de pantalla" debería ser abreviado.
 *
 * Banderas de Modo:
 *   0x01 --> Objetivo (o Reflexivo)
 *   0x02 --> Posesivo (o Reflexivo)
 *   0x04 --> Usar indefinidos para monstruos ocultos ("algo")
 *   0x08 --> Usar indefinidos para monstruos visibles ("un kobold")
 *   0x10 --> Pronominalizar monstruos ocultos
 *   0x20 --> Pronominalizar monstruos visibles
 *   0x40 --> Asumir que el monstruo está oculto
 *   0x80 --> Asumir que el monstruo es visible
 *  0x100 --> Poner en mayúscula el nombre del monstruo
 *  0x200 --> Añadir una coma si el nombre incluye una frase no terminada,
 *            "Lengua de Serpiente, Agente de Saruman" es un ejemplo
 *
 * Modos Útiles:
 *   0x00 --> Nombre nominativo completo ("el kobold") o "ello"
 *   0x04 --> Nombre nominativo completo ("el kobold") o "algo"
 *   0x80 --> Nombre de resistencia al destierro ("el kobold")
 *   0x88 --> Nombre de muerte ("un kobold")
 *   0x22 --> Posesivo, con género si es visible ("su") o "su"
 *   0x23 --> Reflexivo, con género si es visible ("sí mismo") o "sí mismo"
 */
void monster_desc(char *desc, size_t max, const struct monster *mon, int mode)
{
	assert(mon != NULL);

	/* ¿Podemos verlo? (forzado, o no oculto + visible) */
	bool seen = (mode & MDESC_SHOW) ||
		(!(mode & MDESC_HIDE) && monster_is_visible(mon));

	/* Pronombres con género (visto y forzado, o no visto y permitido) */
	bool use_pronoun = (seen && (mode & MDESC_PRO_VIS)) ||
			(!seen && (mode & MDESC_PRO_HID));

	/* Primero, intentar usar pronombres, o describir monstruos ocultos */
	if (!seen || use_pronoun) {
		const char *choice = "ello";

		/* una codificación del "sexo" del monstruo */
		int msex = 0x00;

		/* Extraer el género (si corresponde) */
		if (use_pronoun) {
			if (rf_has(mon->race->flags, RF_FEMALE)) {
				msex = 0x20;
			} else if (rf_has(mon->race->flags, RF_MALE)) {
				msex = 0x10;
			}
		}

		/* Fuerza bruta: dividir en las posibilidades */
		switch (msex + (mode & 0x07)) {
			/* Neutro */
			case 0x00: choice = "ello"; break;
			case 0x01: choice = "ello"; break;
			case 0x02: choice = "su"; break;
			case 0x03: choice = "sí mismo"; break;
			case 0x04: choice = "algo"; break;
			case 0x05: choice = "algo"; break;
			case 0x06: choice = "de algo"; break;
			case 0x07: choice = "sí mismo"; break;

			/* Masculino */
			case 0x10: choice = "él"; break;
			case 0x11: choice = "él"; break;
			case 0x12: choice = "su"; break;
			case 0x13: choice = "sí mismo"; break;
			case 0x14: choice = "alguien"; break;
			case 0x15: choice = "alguien"; break;
			case 0x16: choice = "de alguien"; break;
			case 0x17: choice = "sí mismo"; break;

			/* Femenino */
			case 0x20: choice = "ella"; break;
			case 0x21: choice = "ella"; break;
			case 0x22: choice = "su"; break;
			case 0x23: choice = "sí misma"; break;
			case 0x24: choice = "alguien"; break;
			case 0x25: choice = "alguien"; break;
			case 0x26: choice = "de alguien"; break;
			case 0x27: choice = "sí misma"; break;
		}

		my_strcpy(desc, choice, max);
	} else if ((mode & MDESC_POSS) && (mode & MDESC_OBJE)) {
		/* El monstruo es visible, así que usar su género */
		if (rf_has(mon->race->flags, RF_FEMALE))
			my_strcpy(desc, "sí misma", max);
		else if (rf_has(mon->race->flags, RF_MALE))
			my_strcpy(desc, "sí mismo", max);
		else
			my_strcpy(desc, "sí mismo", max);
	} else {
		const char *comma_pos;

		/* Único, indefinido o definido */
		if (monster_is_shape_unique(mon)) {
			/* Comenzar con el nombre (así nominativo y objetivo) */
			/*
			 * Eliminar la frase descriptiva si se añadirá un posesivo.
			 */
			if ((mode & MDESC_POSS)
					&& rf_has(mon->race->flags, RF_NAME_COMMA)
					&& (comma_pos = strchr(mon->race->name, ','))
					&& comma_pos - mon->race->name < 1024) {
				strnfmt(desc, max, "%.*s",
					(int) (comma_pos - mon->race->name),
					mon->race->name);
			} else {
				my_strcpy(desc, mon->race->name, max);
			}
		} else {
			if (mode & MDESC_IND_VIS) {
				/* XXX Verificar pluralidad para "algunos" */
				/* Los monstruos indefinidos necesitan un artículo indefinido */
				my_strcpy(desc, is_a_vowel(mon->race->name[0]) ? "un " : "un ", max);
			} else {
				/* Los monstruos definidos necesitan un artículo definido */
				my_strcpy(desc, "el ", max);
			}

			/*
			 * Como con los únicos, eliminar la frase si se añadirá un posesivo.
			 */
			if ((mode & MDESC_POSS)
					&& rf_has(mon->race->flags, RF_NAME_COMMA)
					&& (comma_pos = strchr(mon->race->name, ','))
					&& comma_pos - mon->race->name < 1024) {
				my_strcat(desc, format("%.*s",
					(int) (comma_pos - mon->race->name),
					mon->race->name), max);
			} else {
				my_strcat(desc, mon->race->name, max);
			}
		}

		if ((mode & MDESC_COMMA)
				&& rf_has(mon->race->flags, RF_NAME_COMMA)) {
			my_strcat(desc, ",", max);
		}

		/* Manejar el posesivo */
		/* XXX Verificar si termina en "s" */
		if (mode & MDESC_POSS) {
			my_strcat(desc, "'s", max);
		}

		/* Mencionar monstruos "fuera de pantalla" */
		if (!panel_contains(mon->grid.y, mon->grid.x)) {
			my_strcat(desc, " (fuera de pantalla)", max);
		}
	}

	if (mode & MDESC_CAPITAL) {
		my_strcap(desc);
	}
}