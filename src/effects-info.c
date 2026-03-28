/**
 * \file effects-info.c
 * \brief Implementa interfaces para mostrar información sobre efectos
 *
 * Copyright (c) 2020 Eric Branlund
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2004 Robert Ruehlmann
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

#include "effects-info.h"
#include "effects.h"
#include "init.h"
#include "message.h"
#include "mon-summon.h"
#include "obj-info.h"
#include "player-timed.h"
#include "project.h"
#include "z-color.h"
#include "z-form.h"
#include "z-util.h"


static struct {
        int index;
        int args;
        int efinfo_flag;
        const char *desc;
	const char *menu_name;
} base_descs[] = {
        { EF_NONE, 0, EFINFO_NONE, "", "" },
        #define EFFECT(x, a, b, c, d, e, f) { EF_##x, c, d, e, f },
        #include "list-effects.h"
        #undef EFFECT
};


/**
 * Obtiene las cadenas de dados posibles.
 */
static void format_dice_string(const random_value *v, int multiplier,
	size_t len, char* dice_string)
{
	if (v->dice && v->base) {
		if (multiplier == 1) {
			strnfmt(dice_string, len, "%d+%dd%d", v->base, v->dice,
				v->sides);
		} else {
			strnfmt(dice_string, len, "%d+%d*(%dd%d)",
				multiplier * v->base, multiplier, v->dice,
				v->sides);
		}
	} else if (v->dice) {
		if (multiplier == 1) {
			strnfmt(dice_string, len, "%dd%d", v->dice, v->sides);
		} else {
			strnfmt(dice_string, len, "%d*(%dd%d)", multiplier,
				v->dice, v->sides);
		}
	} else {
		strnfmt(dice_string, len, "%d", multiplier * v->base);
	}
}


/**
 * Añade un mensaje describiendo la bonificación de habilidad de dispositivo mágico
 * y el daño medio. El daño medio solo se muestra si hay variación o una bonificación
 * de dispositivo mágico.
 */
static void append_damage(char *buffer, size_t buffer_size, random_value value,
	int dev_skill_boost)
{
	if (dev_skill_boost != 0) {
		my_strcat(buffer, format(", que tu habilidad con dispositivos aumenta en un %d%%",
			dev_skill_boost), buffer_size);
	}

	if (randcalc_varies(value) || dev_skill_boost > 0) {
		// Diez veces el daño medio, para 1 dígito de precisión
		int dam = (100 + dev_skill_boost) * randcalc(value, 0, AVERAGE) / 10;
		my_strcat(buffer, format(" para un promedio de %d.%d de daño", dam / 10,
			dam % 10), buffer_size);
	}
}

//fix traduc para soportar UTF-8 cuando se describe un objeto
//Se usa cuando se quiere descripción de un objeto del inventario que tiene efectos mágicos
static void copy_to_textblock_with_coloring(textblock *tb, const char *s)
{
	const char *start = s;
	while (*s) {
		if (isdigit((unsigned char)*s)) {
			/* Si veníamos acumulando texto no-numérico, lo añadimos ahora */
			if (s > start) {
				textblock_append(tb, "%.*s", (int)(s - start), start);
			}
			/* Añadimos el número en verde */
			textblock_append_c(tb, COLOUR_L_GREEN, "%c", *s);
			s++;
			start = s;
		} else {
			s++;
		}
	}
	/* Añadir el resto de la cadena si queda algo */
	if (s > start) {
		textblock_append(tb, "%s", start);
	}
}


/**
 * Crea una descripción del efecto aleatorio o select que elige entre los
 * siguientes count efectos en la lista enlazada que comienza con e. La
 * descripción tiene como prefijo el contenido de *prefix si prefix no es NULL.
 * También tiene como prefijo el contenido de *type_prefix que normalmente sería
 * "aleatoriamente " para un efecto aleatorio y NULL para un efecto select.
 * dev_skill_boost es el aumento porcentual de daño a reportar por la habilidad
 * de dispositivo. Establece *nexte para que apunte al elemento en la lista
 * enlazada o NULL que está inmediatamente después de los count efectos.
 * Devuelve un valor no NULL si hubo al menos un efecto que pudo ser descrito.
 * De lo contrario, devuelve NULL.
 */
static textblock *create_nested_effect_description(const struct effect *e,
	int count, const char *prefix, const char *type_prefix,
	int dev_skill_boost, const struct effect **nexte)
{
	/*
	 * Hacer una pasada a través de los efectos para determinar si son todos
	 * del mismo tipo básico. Esto se usa para condensar la descripción en
	 * el caso de que todos sean alientos. Ignorar efectos anidados aleatorios
	 * ya que no harán nada cuando el efecto aleatorio externo se procese con
	 * effect_do().
	 */
	textblock *res = NULL;
	const struct effect *efirst;
	const dice_t *first_dice;
	int first_ind, first_other;
	random_value first_rv = { 0, 0, 0, 0 };
	bool same_ind, same_other, same_dice;
	int irand, jrand;
	int nvalid;

	/* Encontrar el primer efecto que sea válido y no aleatorio ni select. */
	irand = 0;
	while (1) {
		if (!e || irand >= count) {
			/*
			 * No hay efectos válidos o no aleatorios; no hacer nada.
			 */
			*nexte = e;
			return false;
		}
		if (effect_desc(e) && e->index != EF_RANDOM &&
				e->index != EF_SELECT) {
			break;
		}
		e = e->next;
		++irand;
	}

	efirst = e;
	first_ind = e->index;
	first_other = e->other;
	first_dice = e->dice;
	if (e->dice) {
		dice_random_value(e->dice, &first_rv);
	}

	nvalid = 1;
	same_ind = true;
	same_other = true;
	same_dice = true;
	for (e = efirst->next, jrand = irand + 1;
		e && jrand < count;
		e = e->next, ++jrand) {
		if (!effect_desc(e) || e->index == EF_RANDOM ||
				e->index == EF_SELECT) {
			continue;
		}
		++nvalid;
		if (e->index != first_ind) {
			same_ind = false;
		}
		if (e->other != first_other) {
			same_other = false;
		}
		if (e->dice) {
			if (first_dice) {
				random_value this_rv;

				dice_random_value(e->dice, &this_rv);
				if (this_rv.base != first_rv.base ||
					this_rv.dice != first_rv.dice ||
					this_rv.sides != first_rv.sides ||
					this_rv.m_bonus != first_rv.m_bonus) {
					same_dice = false;
				}
			} else {
				same_dice = false;
			}
		} else if (first_dice) {
			same_dice = false;
		}
	}
	*nexte = e;

	if (same_ind && base_descs[first_ind].efinfo_flag == EFINFO_BREATH &&
		same_dice && same_other) {
		/* Concatenar la lista de elementos posibles. */
		char breaths[120], dice_string[20], desc[200];
		int ivalid;

		strnfmt(breaths, sizeof(breaths), "%s",
			projections[efirst->subtype].player_desc);
		ivalid = 1;
		for (e = efirst->next, jrand = irand + 1;
			e && jrand < count;
			e = e->next, ++jrand) {
			if (!effect_desc(e) || e->index == EF_RANDOM ||
					e->index == EF_SELECT) {
				continue;
			}
			if (ivalid == nvalid - 1) {
				my_strcat(breaths,
					(nvalid > 2) ? ", o " : " o ",
					sizeof(breaths));
			} else {
				my_strcat(breaths, ", ", sizeof(breaths));
			}
			my_strcat(breaths, projections[e->subtype].player_desc,
				sizeof(breaths));
			++ivalid;
		}

		/* Luego usar eso en la descripción del efecto. */
		format_dice_string(&first_rv, 1, sizeof(dice_string),
			dice_string);
		strnfmt(desc, sizeof(desc), effect_desc(efirst), breaths,
			efirst->other, dice_string);
		append_damage(desc, sizeof(desc), first_rv,
			efirst->index == EF_BREATH ? 0 : dev_skill_boost);

		res = textblock_new();
		if (prefix) {
			textblock_append(res, "%s", prefix);
		}
		if (type_prefix) {
			textblock_append(res, "%s", type_prefix);
		}
		copy_to_textblock_with_coloring(res, desc);
	} else {
		/* Concatenar las descripciones de los efectos. */
		textblock *tb;
		int ivalid;
		
		tb = effect_describe(efirst, type_prefix, dev_skill_boost,
			true);
		if (tb) {
			ivalid = 1;
			if (prefix) {
				res = textblock_new();
				textblock_append(res, "%s", prefix);
				textblock_append_textblock(res, tb);
				textblock_free(tb);
			} else {
				res = tb;
			}
		} else {
			ivalid = 0;
			--nvalid;
		}
		for (e = efirst->next, jrand = irand + 1;
			e && jrand < count;
			e = e->next, ++jrand) {
			if (!effect_desc(e) || e->index == EF_RANDOM ||
					e->index == EF_SELECT) {
				continue;
			}
			tb = effect_describe(e,
				(ivalid == 0) ? type_prefix : NULL,
				dev_skill_boost, true);
			if (!tb) {
				--nvalid;
				continue;
			}
			if (prefix && ! res) {
				assert(ivalid == 0);
				res = textblock_new();
				textblock_append(res, "%s", prefix);
			}
			if (res) {
				if (ivalid > 0) {
					textblock_append(res,
						(ivalid == nvalid - 1) ?
						" o " : ", ");
				}
				textblock_append_textblock(res, tb);
				textblock_free(tb);
			} else {
				res = tb;
			}
			++ivalid;
		}
	}

	return res;
}


/**
 * Crea un nuevo textblock que tiene una descripción del efecto en *e (y
 * cualquier efecto enlazado a él porque e->index == EF_RANDOM o
 * e->index == EF_SELECT) si only_first es true, o tiene una descripción de
 * *e y todos los efectos subsiguientes si only_first es false. Si ninguno de
 * los efectos tiene una descripción, devolverá NULL. Si hay al menos un efecto
 * con una descripción y prefix no es NULL, la cadena apuntada por prefix se
 * añadirá al textblock antes de las descripciones. dev_skill_boost es el
 * aumento porcentual de la habilidad de dispositivo para mostrar en las
 * descripciones.
 */
textblock *effect_describe(const struct effect *e, const char *prefix,
	int dev_skill_boost, bool only_first)
{
	textblock *tb = NULL;
	int nadded = 0;
	char desc[250];
	random_value value = { 0, 0, 0, 0 };
	bool value_set = false;

	while (e) {
		const char* edesc = effect_desc(e);
		int roll = 0;
		char dice_string[20];

		/* Manejar el efecto especial de borrar valor. */
		if (e->index == EF_CLEAR_VALUE) {
			assert(value_set);
			value_set = false;
			e = e->next;
			continue;
		}

		/* Manejar el efecto especial de establecer valor. */
		if (e->index == EF_SET_VALUE) {
			assert(e->dice != NULL);
			roll = dice_roll(e->dice, &value);
			value_set = true;
			e = e->next;
			continue;
		}

		if ((e->dice != NULL) && !value_set) {
			roll = dice_roll(e->dice, &value);
		}

		/* Manejar efectos especiales aleatorios o select. */
		if (e->index == EF_RANDOM || e->index == EF_SELECT) {
			const struct effect *nexte;
			textblock *tbe = create_nested_effect_description(
				e->next, roll, (nadded == 0) ? prefix : NULL,
				(e->index == EF_RANDOM) ? "aleatoriamente " : NULL,
				dev_skill_boost, &nexte);

			e = (only_first) ? NULL : nexte;
			if (tbe) {
				if (tb) {
					textblock_append(tb,
						e ? ", " : " y ");
					textblock_append_textblock(tb, tbe);
					textblock_free(tbe);
				} else {
					tb = tbe;
				}
				++nadded;
			}
			continue;
		}

		if (!edesc) {
			e = (only_first) ? NULL : e->next;
			continue;
		}

		format_dice_string(&value, 1, sizeof(dice_string), dice_string);

		/* Verificar todos los tipos posibles de formato de descripción. */
		switch (base_descs[e->index].efinfo_flag) {
		case EFINFO_DICE:
			strnfmt(desc, sizeof(desc), edesc, dice_string);
			break;

		case EFINFO_HEAL:
			/* La curación a veces tiene un porcentaje mínimo. */
			{
				char min_string[50];

				if (value.m_bonus) {
					strnfmt(min_string, sizeof(min_string),
						" (o %d%%, lo que sea mayor)",
						value.m_bonus);
				} else {
					strnfmt(min_string, sizeof(min_string),
						"%s", "");
				}
				strnfmt(desc, sizeof(desc), edesc, dice_string,
					min_string);
			}
			break;

		case EFINFO_CONST:
			strnfmt(desc, sizeof(desc), edesc, value.base / 2);
			break;

		case EFINFO_FOOD:
			{
				const char *fed = e->subtype ?
					(e->subtype == 1 ? "usa suficiente valor alimenticio" : 
					 "te deja nutrido") : "te alimenta";
				char turn_dice_string[20];

				format_dice_string(&value, z_info->food_value,
					sizeof(turn_dice_string),
					turn_dice_string);

				strnfmt(desc, sizeof(desc), edesc, fed,
					turn_dice_string, dice_string);
			}
			break;

		case EFINFO_CURE:
			strnfmt(desc, sizeof(desc), edesc,
				timed_effects[e->subtype].desc);
			break;

		case EFINFO_TIMED:
			strnfmt(desc, sizeof(desc), edesc,
				timed_effects[e->subtype].desc,
				dice_string);
			break;

		case EFINFO_STAT:
			{
				int stat = e->subtype;

				strnfmt(desc, sizeof(desc), edesc,
					lookup_obj_property(OBJ_PROPERTY_STAT, stat)->name);
			}
			break;

		case EFINFO_SEEN:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].desc);
			break;

		case EFINFO_SUMM:
			strnfmt(desc, sizeof(desc), edesc,
				summon_desc(e->subtype));
			break;

		case EFINFO_TELE:
			/*
			 * Actualmente solo se usa para el jugador, pero puede
			 * manejar monstruos.
			 */
			{
				char dist[32];

				if (value.m_bonus) {
					strnfmt(dist, sizeof(dist),
						"una distancia dependiente del nivel");
				} else {
					strnfmt(dist, sizeof(dist),
						"%d casillas", value.base);
				}
				strnfmt(desc, sizeof(desc), edesc,
					(e->subtype) ? "un monstruo" : "a ti",
					dist);
			}
			break;

		case EFINFO_QUAKE:
			strnfmt(desc, sizeof(desc), edesc, e->radius);
			break;

		case EFINFO_BALL:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].player_desc,
				e->radius, dice_string);
			append_damage(desc, sizeof(desc), value, dev_skill_boost);
			break;

		case EFINFO_SPOT:
			{
				int i_radius = e->other ? e->other : e->radius;

				strnfmt(desc, sizeof(desc), edesc,
					projections[e->subtype].player_desc,
					e->radius, i_radius, dice_string);
				append_damage(desc, sizeof(desc), value, dev_skill_boost);
			}
			break;

		case EFINFO_BREATH:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].player_desc, e->other,
				dice_string);
			append_damage(desc, sizeof(desc), value,
				e->index == EF_BREATH ? 0 : dev_skill_boost);
			break;

		case EFINFO_SHORT:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].player_desc,
				e->radius +
					(e->other ? player->lev / e->other : 0),
				dice_string);
			break;

		case EFINFO_LASH:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].lash_desc, e->subtype);
			break;

		case EFINFO_BOLT:
			/* Proyectil que inflige estado */
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].desc);
			break;

		case EFINFO_BOLTD:
			/* Proyectiles y rayos que dañan */
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].desc, dice_string);
			append_damage(desc, sizeof(desc), value, dev_skill_boost);
			break;

		case EFINFO_TOUCH:
			strnfmt(desc, sizeof(desc), edesc,
				projections[e->subtype].desc);
			break;

		case EFINFO_NONE:
			strnfmt(desc, sizeof(desc), "%s", edesc);
			break;

		default:
			strnfmt(desc, sizeof(desc), "%s", "");
			msg("Se pasó una descripción de efecto incorrecta a effect_info(). Por favor, informa de este error.");
			break;
		}

		e = (only_first) ? NULL : e->next;

		if (desc[0] != '\0') {
			if (tb) {
				if (e) {
					textblock_append(tb, ", ");
				} else {
					textblock_append(tb, " y ");
				}
			} else {
				tb = textblock_new();
				if (prefix) {
					textblock_append(tb, "%s", prefix);
				}
			}
			copy_to_textblock_with_coloring(tb, desc);

			++nadded;
		}
	}

	return tb;
}

/**
 * Llena un búfer con una descripción corta, adecuada para usar como entrada
 * de menú, de un efecto.
 * \param buf es el búfer a llenar.
 * \param max es el número máximo de caracteres que puede contener el búfer.
 * \param e es el efecto a describir.
 * \return el número de caracteres escritos en el búfer; será cero si el
 * efecto no es válido
 */
size_t effect_get_menu_name(char *buf, size_t max, const struct effect *e)
{
	const char *fmt;
	size_t len;

	if (!e || e->index <= EF_NONE || e->index >= EF_MAX) {
		return 0;
	}

	fmt = base_descs[e->index].menu_name;
	switch (base_descs[e->index].efinfo_flag) {
	case EFINFO_DICE:
	case EFINFO_HEAL:
	case EFINFO_CONST:
	case EFINFO_QUAKE:
	case EFINFO_NONE:
		len = strnfmt(buf, max, "%s", fmt);
		break;

	case EFINFO_FOOD:
		{
			const char *actstr;
			const char *actarg;
			int avg;

			switch (e->subtype) {
			case 0: /* INC_BY */
				actstr = "alimentar";
				actarg = "a ti mismo";
				break;
			case 1: /* DEC_BY */
				actstr = "aumentar";
				actarg = "hambre";
				break;
			case 2: /* SET_TO */
				avg = (e->dice) ?
					dice_evaluate(e->dice, 1, AVERAGE, NULL) : 0;
				actstr = "convertirte en";
				if (avg > PY_FOOD_FULL) {
					actarg = "hinchado";
				} else if (avg > PY_FOOD_HUNGRY) {
					actarg = "satisfecho";
				} else {
					actarg = "hambriento";
				}
				break;
			case 3: /* INC_TO */
				avg = (e->dice) ?
					dice_evaluate(e->dice, 1, AVERAGE, NULL): 0;
				actstr = "dejarte";
				if (avg > PY_FOOD_FULL) {
					actarg = "hinchado";
				} else if (avg > PY_FOOD_HUNGRY) {
					actarg = "nutrido";
				} else {
					actarg = "hambriento";
				}
				break;
			default:
				actstr = NULL;
				actarg = NULL;
				break;
			}
			if (actstr && actarg) {
				len = strnfmt(buf, max, fmt, actstr, actarg);
			} else {
				len = strnfmt(buf, max, "%s", "");
			}
		}
		break;

	case EFINFO_CURE:
	case EFINFO_TIMED:
		len = strnfmt(buf, max, fmt, timed_effects[e->subtype].desc);
		break;

	case EFINFO_STAT:
		len = strnfmt(buf, max, fmt,
			lookup_obj_property(OBJ_PROPERTY_STAT,
			e->subtype)->name);
		break;

	case EFINFO_SEEN:
	case EFINFO_BOLT:
	case EFINFO_BOLTD:
	case EFINFO_TOUCH:
		len = strnfmt(buf, max, fmt, projections[e->subtype].desc);
		break;

	case EFINFO_SUMM:
		len = strnfmt(buf, max, fmt, summon_desc(e->subtype));
		break;

	case EFINFO_TELE:
		{
			random_value value = { 0, 0, 0, 0 };
			char dist[32];
			int avg = 0;

			if (e->dice) {
				avg = dice_evaluate(e->dice, 1, AVERAGE,
					&value);
			}
			if (value.m_bonus) {
				strnfmt(dist, sizeof(dist), "cierta distancia");
			} else {
				strnfmt(dist, sizeof(dist), "%d casillas", avg);
			}
			len = strnfmt(buf, max, fmt,
				(e->subtype) ? "a otro" : "a ti", dist);
		}
		break;

	case EFINFO_BALL:
	case EFINFO_SPOT:
	case EFINFO_BREATH:
	case EFINFO_SHORT:
		len = strnfmt(buf, max, fmt,
			projections[e->subtype].player_desc);
		break;

	case EFINFO_LASH:
		len = strnfmt(buf, max, fmt, projections[e->subtype].lash_desc);
		break;

	default:
		len = strnfmt(buf, max, "%s", "");
		msg("Se pasó una descripción de efecto incorrecta a effect_get_menu_name(). Por favor, informa de este error.");
		break;
	}

	return len;
}

/**
 * Devuelve un puntero al siguiente efecto en la pila de efectos, saltando
 * todos los subefectos de efectos aleatorios o select.
 */
struct effect *effect_next(struct effect *effect)
{
	if (effect->index == EF_RANDOM || effect->index == EF_SELECT) {
		struct effect *e = effect;
		int num_subeffects = MAX(0,
			dice_evaluate(effect->dice, 0, AVERAGE, NULL));
		// Saltar todos los subefectos, más uno para avanzar más allá del actual
		for (int i = 0; e != NULL && i < num_subeffects + 1; i++) {
			e = e->next;
		}
		return e;
	} else {
		return effect->next;
	}
}

/**
 * Comprueba si el efecto inflige daño, verificando la cadena de información del efecto.
 * Los efectos aleatorios o select se consideran que infligen daño si algún subefecto
 * inflige daño.
 */
bool effect_damages(const struct effect *effect)
{
	if (effect->index == EF_RANDOM || effect->index == EF_SELECT) {
		// Efecto aleatorio o select
		struct effect *e = effect->next;
		int num_subeffects = dice_evaluate(effect->dice, 0, AVERAGE, NULL);

		// Verificar si alguno de los subefectos hace daño
		for (int i = 0; e != NULL && i < num_subeffects; i++) {
			if (effect_damages(e)) {
				return true;
			}
			e = e->next;
		}
		return false;
	} else {
		// No es un efecto aleatorio o select, verificar la cadena de información
		// para daño
		return effect_info(effect) != NULL &&
			streq(effect_info(effect), "dam");
	}
}

/**
 * Calcula el daño medio del efecto. Los efectos aleatorios y select devuelven
 * un promedio de todos los promedios de los subefectos.
 *
 * \param effect es el efecto a evaluar.
 * \param shared_dice son los dados establecidos por un efecto SET_VALUE previo.
 * Usar NULL si no hubo un efecto SET_VALUE previo para establecer los dados.
 */
int effect_avg_damage(const struct effect *effect, const dice_t *shared_dice)
{
	if (effect->index == EF_RANDOM || effect->index == EF_SELECT) {
		// Efecto aleatorio o select, verificar los subefectos para
		// acumular daño
		int total = 0;
		struct effect *e = effect->next;
		int n_stated = dice_evaluate((shared_dice) ?
			shared_dice : effect->dice, 0, AVERAGE, NULL);
		int n_actual = 0;

		for (int i = 0; e != NULL && i < n_stated; i++) {
			total += effect_avg_damage(e, shared_dice);
			++n_actual;
			e = e->next;
		}
		// Devolver un promedio de los daños medios de los subefectos
		return (n_actual > 0) ? total / n_actual : 0;
	} else if (effect_damages(effect)) {
		// Efecto no aleatorio, calcular el daño medio
		return dice_evaluate((shared_dice) ?
			shared_dice : effect->dice, 0, AVERAGE, NULL);
	}
	return 0;
}

/**
 * Devuelve la proyección del efecto, o una cadena vacía si no tiene ninguna.
 * Los efectos aleatorios o select solo devuelven una proyección si todos los
 * subefectos tienen la misma proyección.
 */
const char *effect_projection(const struct effect *effect)
{
	if (effect->index == EF_RANDOM || effect->index == EF_SELECT) {
		// Efecto aleatorio o select
		int num_subeffects = dice_evaluate(effect->dice, 0, AVERAGE, NULL);
		struct effect *e;
		const char *subeffect_proj;

		// Verificar si todos los subefectos tienen la misma proyección,
		// y si no, simplemente renunciar a ello
		if (num_subeffects <= 0 || !effect->next) {
			return "";
		}

		e = effect->next;
		subeffect_proj = effect_projection(e);
		for (int i = 0; e != NULL && i < num_subeffects; i++) {
			if (!streq(subeffect_proj, effect_projection(e))) {
				return "";
			}
			e = e->next;
		}

		return subeffect_proj;
	} else if (projections[effect->subtype].player_desc != NULL) {
		// Efecto no aleatorio, extraer la proyección si la hay
		switch (base_descs[effect->index].efinfo_flag) {
			case EFINFO_BALL:
			case EFINFO_BOLTD:
			case EFINFO_BREATH:
			case EFINFO_SHORT:
			case EFINFO_SPOT:
				return projections[effect->subtype].player_desc;
		}
	}

	return "";
}

/**
 * Ayuda a effect_summarize_properties() y summarize_cure(): añadir un elemento
 * a la lista enlazada de propiedades de objeto.
 */
static void add_to_summaries(struct effect_object_property **summaries,
		int idx, int reslevel_min, int reslevel_max,
		enum effect_object_property_kind kind)
{
	struct effect_object_property *prop = mem_alloc(sizeof(*prop));

	prop->next = *summaries;
	prop->idx = idx;
	prop->reslevel_min = reslevel_min;
	prop->reslevel_max = reslevel_max;
	prop->kind = kind;
	*summaries = prop;
}

/**
 * Ayuda a effect_summarize_properties(): actualizar los resúmenes para un
 * efecto que actúa como una cura.
 * \param tmd Es el índice TMD_* para el efecto temporal que se está curando.
 * \param summaries Es el puntero a la lista enlazada de resúmenes.
 * \param unsummarized_count Es el recuento de efectos no resumidos.
 */
static void summarize_cure(int tmd, struct effect_object_property **summaries,
		int *unsummarized_count)
{
	const struct timed_failure *f = timed_effects[tmd].fail;

	while (f) {
		if (f->code == TMD_FAIL_FLAG_OBJECT) {
			add_to_summaries(summaries, f->idx, 0, 0,
				EFPROP_CURE_FLAG);
		} else if (f->code == TMD_FAIL_FLAG_RESIST) {
			add_to_summaries(summaries, f->idx, -1, 0,
				EFPROP_CURE_RESIST);
		} else {
			++*unsummarized_count;
		}
		f = f->next;
	}
}

/**
 * Devuelve un resumen de las propiedades de objeto que coinciden con los
 * efectos en una cadena de efectos.
 * \param ef Es el puntero al primer efecto en la cadena.
 * \param unsummarized_count Si no es NULL, *unsummarized_count se establecerá
 * al recuento de efectos en la cadena que hacen algo que no se puede resumir
 * mediante una propiedad de objeto.
 * \return Devuelve un puntero a una lista enlazada de las propiedades de objeto
 * implícitas por la cadena de efectos. Cuando ya no se necesite, cada elemento
 * de esa lista enlazada debe liberarse con mem_free().
 */
struct effect_object_property *effect_summarize_properties(
		const struct effect *ef, int *unsummarized_count)
{
	int unsummarized = 0;
	struct effect_object_property *summaries = NULL;
	dice_t *remembered_dice = NULL;

	for (; ef; ef = ef->next) {
		int value_this;

		switch (ef->index) {
		case EF_RANDOM:
		case EF_SELECT:
			/*
			 * Para efectos aleatorios o select, resumir todos los
			 * subefectos ya que cualquiera de ellos es posible.
			 * Eso es equivalente a simplemente saltarse el efecto
			 * aleatorio o select y avanzar uno por uno a través de
			 * lo que sigue.
			 */
			break;

		case EF_SET_VALUE:
			/*
			 * Recordar el valor. No hace nada que deba ser
			 * recordado en los resúmenes o el recuento de no resumidos.
			 */
			remembered_dice = ef->dice;
			break;

		case EF_CLEAR_VALUE:
			/*
			 * Olvidar el valor. No hace nada que deba ser
			 * recordado en los resúmenes o el recuento de no resumidos.
			 */
			remembered_dice = NULL;
			break;

		case EF_CURE:
			if (ef->subtype >= 0 && ef->subtype < TMD_MAX) {
				summarize_cure(ef->subtype, &summaries,
					&unsummarized);
			}
			break;

		case EF_TIMED_SET:
			value_this = (remembered_dice) ?
				dice_evaluate(remembered_dice, 0, MAXIMISE, NULL) :
				((ef->dice) ?
				dice_evaluate(ef->dice, 0, MAXIMISE, NULL) : 0);
			if (value_this <= 0 && ef->subtype >= 0 &&
					ef->subtype < TMD_MAX) {
				/* Es equivalente a una cura. */
				summarize_cure(ef->subtype, &summaries,
					&unsummarized);
				break;
			}
			/* Fall through. */

		case EF_TIMED_INC:
		case EF_TIMED_INC_NO_RES:
			value_this = (remembered_dice) ?
				dice_evaluate(remembered_dice, 0, MAXIMISE, NULL) :
				((ef->dice) ?
				dice_evaluate(ef->dice, 0, MAXIMISE, NULL) : 0);
			if (value_this > 0 && ef->subtype >= 0 &&
					ef->subtype < TMD_MAX) {
				bool summarized = false;
				const struct timed_failure *f;

				if (timed_effects[ef->subtype].oflag_dup !=
						OF_NONE) {
					add_to_summaries(&summaries,
						timed_effects[ef->subtype].oflag_dup,
						0, 0,
						timed_effects[ef->subtype].oflag_syn ?
						EFPROP_OBJECT_FLAG_EXACT : EFPROP_OBJECT_FLAG);
					summarized = true;
				}
				if (timed_effects[ef->subtype].temp_resist >= 0) {
					int rmin = -1, rmax = 1;

					f = timed_effects[ef->subtype].fail;
					while (f) {
						if (f->idx == timed_effects[ef->subtype].temp_resist) {
							if (f->code == TMD_FAIL_FLAG_RESIST) {
								rmax = MIN(rmax, 0);
							} else if (f->code == TMD_FAIL_FLAG_VULN) {
								rmin = MAX(rmin, 0);
							}
						}
						f = f->next;
					}
					add_to_summaries(&summaries,
						timed_effects[ef->subtype].temp_resist,
						rmin, rmax, EFPROP_RESIST);
					summarized = true;
				}
				f = timed_effects[ef->subtype].fail;
				while (f) {
					switch (f->code) {
					case TMD_FAIL_FLAG_OBJECT:
						add_to_summaries(&summaries,
							f->idx, 0, 0,
							EFPROP_CONFLICT_FLAG);
						summarized = true;
						break;

					case TMD_FAIL_FLAG_RESIST:
						if (f->idx != timed_effects[ef->subtype].temp_resist) {
							add_to_summaries(
								&summaries,
								f->idx, -1, 0,
								EFPROP_CONFLICT_RESIST);
							summarized = true;
						}
						break;

					case TMD_FAIL_FLAG_VULN:
						if (f->idx != timed_effects[ef->subtype].temp_resist) {
							add_to_summaries(
								&summaries,
								f->idx, 0, 3,
								EFPROP_CONFLICT_VULN);
							summarized = true;
						}
						break;

					default:
						/* No se necesita nada especial. */
						break;
					}
					f = f->next;
				}
				if (timed_effects[ef->subtype].temp_brand >= 0) {
					add_to_summaries(&summaries,
						timed_effects[ef->subtype].temp_brand,
						0, 0, EFPROP_BRAND);
					summarized = true;
				}
				if (timed_effects[ef->subtype].temp_slay >= 0) {
					add_to_summaries(&summaries,
						timed_effects[ef->subtype].temp_slay,
						0, 0, EFPROP_SLAY);
					summarized = true;
				}
				if (!summarized) ++unsummarized;
			}
			break;

		case EF_TIMED_DEC:
			value_this = (remembered_dice) ?
				dice_evaluate(remembered_dice, 0, MAXIMISE, NULL) :
				((ef->dice) ?
				dice_evaluate(ef->dice, 0, MAXIMISE, NULL) : 0);
			/* Si disminuye la duración, es una cura parcial. */
			if (value_this > 0) {
				summarize_cure(ef->subtype, &summaries,
					&unsummarized);
			}
			break;

		case EF_TELEPORT:
		case EF_TELEPORT_TO:
		case EF_TELEPORT_LEVEL:
			add_to_summaries(&summaries, OF_NO_TELEPORT,
				0, 0, EFPROP_CONFLICT_FLAG);
			break;

		/*
		 * Hay otros efectos que tienen utilidad limitada cuando el
		 * objeto ya tiene algunos indicadores:
		 * DISABLE_TRAPS con OF_TRAP_IMMUNE solo es bueno para desbloquear
		 * DETECT_INVISIBLE con OF_SEE_INVISIBLE o OF_TELEPATHY
		 * RESTORE_x con OF_SUST_x
		 * RESTORE_EXP con OF_HOLD_LIFE
		 * Por ahora, no intentar marcar esos.
		 */
		default:
			/*
			 * Todo lo demás no está relacionado con una propiedad de objeto.
			 */
			++unsummarized;
			break;
		}
	}

	if (unsummarized_count) *unsummarized_count = unsummarized;
	return summaries;
}