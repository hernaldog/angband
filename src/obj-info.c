/**
 * \file obj-info.c
 * \brief Código de descripción de objetos.
 *
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

#include "angband.h"
#include "cmds.h"
#include "effects.h"
#include "effects-info.h"
#include "game-world.h"
#include "init.h"
#include "monster.h"
#include "mon-util.h"
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "project.h"
#include "z-textblock.h"

/**
 * Describe el número de golpes posibles para determinados bonos de estadísticas
 */
struct blow_info {
	int str_plus;
	int dex_plus;  
	int centiblows;
};

/**
 * ------------------------------------------------------------------------
 * Tablas de datos
 * ------------------------------------------------------------------------ */

static const struct origin_type {
	int type;
	int args;
	const char *desc;
} origins[] = {
	#define ORIGIN(a, b, c) { ORIGIN_##a, b, c },
	#include "list-origins.h"
	#undef ORIGIN
};


/**
 * ------------------------------------------------------------------------
 * Código auxiliar para escribir listas
 * ------------------------------------------------------------------------ */

/**
 * Dado un array de cadenas, como así:
 *  { "inteligencia", "pez", "lente", "primo", "número" },
 *
 * ... genera una lista como "inteligencia, pez, lente, primo, número.\n".
 */
static void info_out_list(textblock *tb, const char *list[], size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		textblock_append(tb, "%s", list[i]);
		if (i != (count - 1)) textblock_append(tb, ", ");
	}

	textblock_append(tb, ".\n");
}


/**
 * Llena el receptáculo con todos los elementos que corresponden a la `lista` dada.
 */
static size_t element_info_collect(const bool list[], const char *recepticle[])
{
	int i, count = 0;

	for (i = 0; i < ELEM_MAX; i++) {
		if (list[i])
			recepticle[count++] = projections[i].name;
	}

	return count;
}


/**
 * ------------------------------------------------------------------------
 * Código que utiliza las tablas de datos para describir aspectos de la
 * información de un objeto
 * ------------------------------------------------------------------------ */

/**
 * Describe las maldiciones de un objeto.
 */
static bool describe_curses(textblock *tb, const struct object *obj,
		const bitflag flags[OF_SIZE])
{
	int i;
	struct curse_data *c = obj->known->curses;

	if (!c)
		return false;
	for (i = 1; i < z_info->curse_max; i++) {
		if (c[i].power) {
			textblock_append(tb, "Hace que ");
			textblock_append_c(tb, COLOUR_L_RED, "%s", curses[i].desc);
			if (c[i].power == 100) {
				textblock_append(tb, "; esta maldición no se puede eliminar");
			}
			textblock_append(tb, ".\n");
		}
	}

	return true;
}


/**
 * Describe las modificaciones a las estadísticas.
 */
static bool describe_stats(textblock *tb, const struct object *obj,
						   oinfo_detail_t mode)
{
	size_t count = 0, i;
	bool detail = false;

	/* No dar los valores exactos para objetos de ego falsos, ya que cada
	 * objeto real será diferente */
	bool suppress_details = mode & (OINFO_EGO | OINFO_FAKE) ? true : false;

	/* Se conoce el hecho, pero no la magnitud, para los egos y objetos con
	 * sabor de los que el jugador tiene conocimiento */
	bool known_effect = false;
	if (obj->known->ego)
		known_effect = true;
	if (tval_can_have_flavor_k(obj->kind) && object_flavor_is_aware(obj))
		known_effect = true;

	/* Ver qué tenemos */
	for (i = 0; i < OBJ_MOD_MAX; i++)
		if (obj->known->modifiers[i]) {
			count++;
			detail = true;
		}

	if (!count)
		return false;

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		const char *desc = lookup_obj_property(OBJ_PROPERTY_MOD, i)->name;
		int val = obj->known->modifiers[i];
		if (!val) continue;

		/* Objeto real */
		if (detail && !suppress_details) {
			int attr = (val > 0) ? COLOUR_L_GREEN : COLOUR_RED;
			textblock_append_c(tb, attr, "%+i %s.\n", val, desc);
		} else if (known_effect)
			/* Tipo de ego o descripción de joyería */
			textblock_append(tb, "Afecta a tu %s\n", desc);
	}

	return true;
}


/**
 * Describe inmunidades, resistencias y vulnerabilidades otorgadas por un objeto.
 */
static bool describe_elements(textblock *tb,
							  const struct element_info el_info[])
{
	const char *i_descs[ELEM_MAX];
	const char *r_descs[ELEM_MAX];
	const char *v_descs[ELEM_MAX];
	size_t i, count;

	bool list[ELEM_MAX], prev = false;

	/* Inmunidades */
	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].res_level == 3);
	count = element_info_collect(list, i_descs);
	if (count) {
		textblock_append(tb, "Otorga inmunidad a ");
		info_out_list(tb, i_descs, count);
		prev = true;
	}

	/* Resistencias */
	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].res_level == 1);
	count = element_info_collect(list, r_descs);
	if (count) {
		textblock_append(tb, "Otorga resistencia a ");
		info_out_list(tb, r_descs, count);
		prev = true;
	}

	/* Vulnerabilidades */
	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].res_level == -1);
	count = element_info_collect(list, v_descs);
	if (count) {
		textblock_append(tb, "Te hace vulnerable a ");
		info_out_list(tb, v_descs, count);
		prev = true;
	}

	return prev;
}


/**
 * Describe protecciones otorgadas por un objeto.
 */
static bool describe_protects(textblock *tb, const bitflag flags[OF_SIZE])
{
	const char *p_descs[OF_MAX];
	int i, count = 0;

	/* Protecciones */
	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if (prop->subtype != OFT_PROT) continue;
		if (of_has(flags, prop->index)) {
			p_descs[count++] = prop->desc;
		}
	}

	if (!count)
		return false;

	textblock_append(tb, "Otorga protección contra ");
	info_out_list(tb, p_descs, count);

	return  true;
}

/**
 * Describe elementos que el objeto ignora.
 */
static bool describe_ignores(textblock *tb, const struct element_info el_info[])
{
	const char *descs[ELEM_MAX];
	size_t i, count;
	bool list[ELEM_MAX];

	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].flags & EL_INFO_IGNORE);
	count = element_info_collect(list, descs);

	if (!count)
		return false;

	textblock_append(tb, "No puede ser dañado por ");
	info_out_list(tb, descs, count);

	return true;
}

/**
 * Describe elementos que dañan o destruyen un objeto.
 */
static bool describe_hates(textblock *tb, const struct element_info el_info[])
{
	const char *descs[ELEM_MAX];
	size_t i, count = 0;
	bool list[ELEM_MAX];

	for (i = 0; i < ELEM_MAX; i++)
		list[i] = (el_info[i].flags & EL_INFO_HATES);
	count = element_info_collect(list, descs);

	if (!count)
		return false;

	textblock_append(tb, "Puede ser destruido por ");
	info_out_list(tb, descs, count);

	return true;
}


/**
 * Describe las sustentaciones de estadísticas.
 */
static bool describe_sustains(textblock *tb, const bitflag flags[OF_SIZE])
{
	const char *descs[STAT_MAX];
	int i, count = 0;

	for (i = 0; i < STAT_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_STAT, i);
		if (of_has(flags, sustain_flag(prop->index)))
			descs[count++] = prop->name;
	}

	if (!count)
		return false;

	textblock_append(tb, "Sustenta ");
	info_out_list(tb, descs, count);

	return true;
}


/**
 * Describe poderes diversos.
 */
static bool describe_misc_magic(textblock *tb, const bitflag flags[OF_SIZE])
{
	int i;
	bool printed = false;

	for (i = 1; i < OF_MAX; i++) {
		struct obj_property *prop = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		if ((prop->subtype != OFT_MISC)  && (prop->subtype != OFT_MELEE) &&
			(prop->subtype != OFT_BAD)) continue;
		if (of_has(flags, prop->index)) {
			textblock_append(tb, "%s.  ", prop->desc);
			printed = true;
		}
	}

	if (printed)
		textblock_append(tb, "\n");

	return printed;
}


/**
 * Describe los ataques especiales (slays) en armas
 */
static bool describe_slays(textblock *tb, const struct object *obj)
{
	int i, count = 0;
	bool *s = obj->known->slays;

	if (!s) return false;

	if (tval_is_weapon(obj) || tval_is_fuel(obj))
		textblock_append(tb, "Aniquila a ");
	else
		textblock_append(tb, "Hace que tus ataques cuerpo a cuerpo aniquilen a ");

	for (i = 1; i < z_info->slay_max; i++) {
		if (s[i]) {
			count++;
		}
	}

	assert(count >= 1);
	for (i = 1; i < z_info->slay_max; i++) {
		if (!s[i]) continue;

		textblock_append(tb, "%s", slays[i].name);
		if (slays[i].multiplier > 3)
			textblock_append(tb, " (poderosamente)");
		if (count > 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ".\n");
		count--;
	}

	return true;
}

/**
 * Describe las marcas elementales (brands) en armas
 */
static bool describe_brands(textblock *tb, const struct object *obj)
{
	int i, count = 0;
	bool *b = obj->known->brands;

	if (!b) return false;

	if (tval_is_weapon(obj) || tval_is_fuel(obj))
		textblock_append(tb, "Marca tus golpes con ");
	else
		textblock_append(tb, "Hace que tus ataques cuerpo a cuerpo se marquen con ");

	for (i = 1; i < z_info->brand_max; i++) {
		if (b[i]) {
			count++;
		}
	}

	assert(count >= 1);
	for (i = 1; i < z_info->brand_max; i++) {
		if (!b[i]) continue;

		if (brands[i].multiplier < 3)
			textblock_append(tb, "débil ");
		textblock_append(tb, "%s", brands[i].name);
		if (count > 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ".\n");
		count--;
	}

	return true;
}

/**
 * Suma los niveles críticos de O-combat para obtener el número esperado de
 * dados añadidos cuando ocurre un crítico.
 */
static struct my_rational sum_o_criticals(const struct o_critical_level *head)
{
	struct my_rational remaining_chance = my_rational_construct(1, 1);
	struct my_rational added_dice = my_rational_construct(0, 1);

	while (head) {
		/* El último nivel de críticos toma el resto. */
		struct my_rational level_added_dice = my_rational_construct(
			head->added_dice, (head->next) ? head->chance : 1);

		level_added_dice = my_rational_product(&level_added_dice,
			&remaining_chance);
		added_dice = my_rational_sum(&added_dice, &level_added_dice);
		if (head->next) {
			struct my_rational pr_not_this = my_rational_construct(
				head->chance - 1, head->chance);

			remaining_chance = my_rational_product(
				&remaining_chance, &pr_not_this);
		}
		head = head->next;
	}

	return added_dice;
}

/**
 * Considera los críticos en el cálculo de la habilidad de combate cuerpo a cuerpo.
 *
 * Nota: Esto asume que los críticos son una función afín del daño previo,
 * ya que se utiliza para transformar la media de una tirada.
 */
static void calculate_melee_crits(struct player_state *state, int weight,
		int plus, int *mult, int *add, int *div, int *mult_round,
		int *add_round, int *scl_round)
{
	/*
	 * Asumir pesimistamente que el objetivo no tiene desventajas;
	 * de lo contrario, esto debe coincidir con los cálculos en
	 * player-attack.c's critical_melee().
	 */
	int crit_chance = z_info->m_crit_chance_weight_scl * weight
		+ z_info->m_crit_chance_toh_scl * (state->to_h + plus)
		+ z_info->m_crit_chance_level_scl * player->lev
		+ z_info->m_crit_chance_toh_skill_scl
			* state->skills[SKILL_TO_HIT_MELEE]
		+ z_info->m_crit_chance_offset;
	crit_chance = MIN(z_info->m_crit_chance_range, MAX(0, crit_chance));

	/* Los resultados informados (*mult y *add) están escalados por 100. */
	*div = 100;

	if (crit_chance > 0 && z_info->m_crit_level_head) {
		/*
		 * Ahora sumar sobre los posibles valores del poder crítico.
		 */
		const struct critical_level *this_l = z_info->m_crit_level_head;
		int min_power = z_info->m_crit_power_weight_scl * weight + 1;
		int max_power = min_power - 1 + z_info->m_crit_power_random;
		int mult_sum = 0;
		int add_sum = 0;
		int scale;

		while (min_power <= max_power) {
			int w;

			if (max_power < this_l->cutoff || !this_l->next) {
				/*
				 * Todos los poderes críticos posibles restantes
				 * caen en esta banda.
				 */
				w = max_power - min_power + 1;
				min_power = max_power + 1;
			} else  {
				if (min_power >= this_l->cutoff) {
					/*
					 * Esta banda no se superpone con los
					 * poderes posibles.
					 */
					this_l = this_l->next;
					continue;
				}
				/*
				 * Esta banda está completamente cubierta o su
				 * parte superior está cubierta por los poderes
				 * posibles.
				 */
				w = this_l->cutoff - min_power;
				min_power = this_l->cutoff;
			}
			mult_sum += w * (this_l->mult - 1);
			add_sum += w * this_l->add;
			this_l = this_l->next;
		}
		/*
		 * En otras palabras, el resultado de sin crítico (multiplicador 1
		 * y sin término aditivo) más el resultado escalado de sumar sobre
		 * los críticos posibles truncado al entero más cercano.
		 */
		scale = (z_info->m_crit_chance_range / *div)
			* z_info->m_crit_power_random;
		*mult = *div + (crit_chance * mult_sum) / scale;
		*add = (crit_chance * add_sum) / scale;
		*mult_round = (crit_chance * mult_sum) % scale;
		*add_round = (crit_chance * add_sum) % scale;
		*scl_round = scale;
	} else {
		*mult = 100;
		*add = 0;
		*mult_round = 0;
		*add_round = 0;
		*scl_round = 1;
	}
}

/**
 * Considera los críticos en el cálculo de la habilidad de combate cuerpo a
 * cuerpo para O-combat; probabilidad de crítico * número medio de dados añadidos.
 *
 * \param state apunta al estado del jugador de interés.
 * \param obj es el arma cuerpo a cuerpo de interés.
 * \param dice se desreferencia y se establece a 100 * probabilidad de crítico *
 * número medio de dados añadidos.
 * \param frac_dice se desreferencia y se establece a la parte fraccionaria
 * truncada de *dice cuando se convierte a entero.
 */
static void o_calculate_melee_crits(struct player_state *state,
		const struct object *obj, unsigned int *dice,
		struct my_rational *frac_dice)
{
	if (z_info->o_m_crit_level_head) {
		/*
		 * Asumir pesimistamente que el objetivo no tiene desventajas.
		 * De lo contrario, estos cálculos deben coincidir con los de
		 * player-attack.c's o_critical_melee().
		 */
		struct player_state old_state = player->state;
		int power, chance_num, chance_den;

		if (z_info->o_m_max_added.n == 0) {
			z_info->o_m_max_added =
				sum_o_criticals(z_info->o_m_crit_level_head);
		}

		player->state = *state;
		power = chance_of_melee_hit_base(player, obj);
		player->state = old_state;
		power = (power * z_info->o_m_crit_power_toh_scl_num)
			/ z_info->o_m_crit_power_toh_scl_den;
		chance_num = power * z_info->o_m_crit_chance_power_scl_num;
		chance_den = power * z_info->o_m_crit_chance_power_scl_den
			+ z_info->o_m_crit_chance_add_den;
		if (chance_den > 0 && chance_num > 0) {
			unsigned int tr;

			if (chance_num < chance_den) {
				/*
				 * El crítico solo ocurre parte del tiempo.
				 * Escalar por la probabilidad y 100.
				 */
				struct my_rational t = my_rational_construct(
					chance_num, chance_den);

				t = my_rational_product(&t,
					&z_info->o_m_max_added);
				*dice = my_rational_to_uint(&t, 100, &tr);
				*frac_dice = my_rational_construct(tr, t.d);
			} else {
				/* El crítico siempre ocurre. Escalar por 100. */
				*dice = my_rational_to_uint(
					&z_info->o_m_max_added, 100, &tr);
				*frac_dice = my_rational_construct(tr,
					z_info->o_m_max_added.d);
			}
		} else {
			/* Sin probabilidad de ocurrir, sin daño adicional. */
			*dice = 0;
			*frac_dice = my_rational_construct(0, 1);
		}
	} else {
		/* Sin niveles críticos definidos, sin daño adicional. */
		*dice = 0;
		*frac_dice = my_rational_construct(0, 1);
	}
}

/**
 * Los críticos de proyectil siguen el mismo enfoque que los críticos cuerpo a
 * cuerpo.
 */
static void calculate_missile_crits(struct player_state *state, int weight,
		int plus, bool launched, int *mult, int *add, int *div,
		int *mult_round, int *add_round, int *scl_round)
{
	/*
	 * Asumir pesimistamente que el objetivo no tiene desventajas;
	 * de lo contrario, esto debe coincidir con los cálculos en
	 * player-attack.c's critical_shot().
	 */
	int crit_chance = z_info->r_crit_chance_weight_scl * weight
		+ z_info->r_crit_chance_toh_scl * (state->to_h + plus)
		+ z_info->r_crit_chance_level_scl * player->lev
		+ z_info->r_crit_chance_offset;

	if (launched) {
		crit_chance += z_info->r_crit_chance_launched_toh_skill_scl
			* player->state.skills[SKILL_TO_HIT_BOW];
	} else {
		crit_chance += z_info->r_crit_chance_thrown_toh_skill_scl
			* player->state.skills[SKILL_TO_HIT_THROW];
	}
	crit_chance = MIN(z_info->r_crit_chance_range, MAX(0, crit_chance));

	/* Los resultados informados (*mult y *add) están escalados por 100. */
	*div = 100;

	if (crit_chance > 0 && z_info->r_crit_level_head) {
		/*
		 * Ahora sumar sobre los posibles valores del poder crítico.
		 */
		const struct critical_level *this_l = z_info->r_crit_level_head;
		int min_power = z_info->r_crit_power_weight_scl * weight + 1;
		int max_power = min_power - 1 + z_info->r_crit_power_random;
		int mult_sum = 0;
		int add_sum = 0;
		int scale;

		while (min_power <= max_power) {
			int w;

			if (max_power < this_l->cutoff || !this_l->next) {
				/*
				 * Todos los poderes críticos posibles restantes
				 * caen en esta banda.
				 */
				w = max_power - min_power + 1;
				min_power = max_power + 1;
			} else  {
				if (min_power >= this_l->cutoff) {
					/*
					 * Esta banda no se superpone con los
					 * poderes posibles.
					 */
					this_l = this_l->next;
					continue;
				}
				/*
				 * Esta banda está completamente cubierta o su
				 * parte superior está cubierta por los poderes
				 * posibles.
				 */
				w = this_l->cutoff - min_power;
				min_power = this_l->cutoff;
			}
			mult_sum += w * (this_l->mult - 1);
			add_sum += w * this_l->add;
			this_l = this_l->next;
		}
		/*
		 * En otras palabras, el resultado de sin crítico (multiplicador 1
		 * y sin término aditivo) más el resultado escalado de sumar sobre
		 * los críticos posibles truncado al entero más cercano.
		 */
		scale = (z_info->r_crit_chance_range / *div)
			* z_info->r_crit_power_random;
		*mult = *div + (crit_chance * mult_sum) / scale;
		*add = (crit_chance * add_sum) / scale;
		*mult_round = (crit_chance * mult_sum) % scale;
		*add_round = (crit_chance * add_sum) % scale;
		*scl_round = scale;
	} else {
		*mult = 100;
		*add = 0;
		*mult_round = 0;
		*add_round = 0;
		*scl_round = 1;
	}
}

/**
 * Los críticos de proyectil siguen el mismo enfoque que los críticos cuerpo a
 * cuerpo.
 *
 * \param state apunta al estado del jugador de interés.
 * \param obj es el proyectil de interés.
 * \param launcher es el lanzador de interés o NULL para un proyectil arrojado.
 * \param dice se desreferencia y se establece a 100 * probabilidad de crítico *
 * número medio de dados añadidos.
 * \param frac_dice se desreferencia y se establece a la parte fraccionaria
 * truncada de *dice cuando se convierte a entero.
 */
static void o_calculate_missile_crits(struct player_state *state,
		const struct object *obj, const struct object *launcher,
		unsigned int *dice, struct my_rational *frac_dice)
{
	if (z_info->o_r_crit_level_head) {
		/*
		 * Asumir pesimistamente que el objetivo no tiene desventajas.
		 * De lo contrario, estos cálculos deben coincidir con los de
		 * player-attack.c's o_critical_shot().
		 */
		struct player_state old_state = player->state;
		int power, chance_num, chance_den;

		if (z_info->o_r_max_added.n == 0) {
			z_info->o_r_max_added =
				sum_o_criticals(z_info->o_r_crit_level_head);
		}

		player->state = *state;
		power = chance_of_missile_hit_base(player, obj, launcher);
		player->state = old_state;
		if (launcher) {
			power = (power
				* z_info->o_r_crit_power_launched_toh_scl_num)
				/ z_info->o_r_crit_power_launched_toh_scl_den;
		} else {
			power = (power
				* z_info->o_r_crit_power_thrown_toh_scl_num)
				/ z_info->o_r_crit_power_thrown_toh_scl_den;
		}
		chance_num = power * z_info->o_r_crit_chance_power_scl_num;
		chance_den = power * z_info->o_r_crit_chance_power_scl_den
			+ z_info->o_r_crit_chance_add_den;
		if (chance_den > 0 && chance_num > 0) {
			unsigned int tr;

			if (chance_num < chance_den) {
				/*
				 * El crítico solo ocurre parte del tiempo.
				 * Escalar por la probabilidad y 100.
				 */
				struct my_rational t = my_rational_construct(
					chance_num, chance_den);

				t = my_rational_product(&t,
					&z_info->o_r_max_added);
				*dice = my_rational_to_uint(&t, 100, &tr);
				*frac_dice = my_rational_construct(tr, t.d);
			} else {
				/* El crítico siempre ocurre. Escalar por 100. */
				*dice = my_rational_to_uint(
					&z_info->o_r_max_added, 100,
					&tr);
				*frac_dice = my_rational_construct(tr,
					z_info->o_r_max_added.d);
			}
		} else {
			/* Sin probabilidad de ocurrir, sin daño adicional. */
			*dice = 0;
			*frac_dice = my_rational_construct(0, 1);
		}
	} else {
		/* Sin niveles críticos definidos, sin daño adicional. */
		*dice = 0;
		*frac_dice = my_rational_construct(0, 1);
	}
}

/**
 * Obtiene los indicadores (flags) del objeto que el jugador debería conocer
 * para la combinación dada de objeto/modo de visualización.
 */
static void get_known_flags(const struct object *obj, const oinfo_detail_t mode,
							bitflag flags[OF_SIZE])
{
	/* Obtener los indicadores del objeto */
	if (mode & OINFO_EGO) {
			object_flags(obj, flags);
	} else {
		object_flags_known(obj, flags);

		/* No incluir indicadores base cuando es escueto */
		if (mode & OINFO_TERSE)
			of_diff(flags, obj->kind->base->flags);
	}
}

/**
 * Obtiene la información de elementos del objeto que el jugador debería
 * conocer para la combinación dada de objeto/modo de visualización.
 */
static void get_known_elements(const struct object *obj,
							   const oinfo_detail_t mode,
							   struct element_info el_info[])
{
	size_t i;

	/* Obtener la información de elementos */
	for (i = 0; i < ELEM_MAX; i++) {
		/* Informar sobre egos falsos o información conocida de elementos */
		if (player->obj_k->el_info[i].res_level || (mode & OINFO_SPOIL))
			el_info[i].res_level = obj->known->el_info[i].res_level;
		else
			el_info[i].res_level = 0;
		el_info[i].flags = obj->known->el_info[i].flags;

		/* Ignorar un elemento: */
		if (obj->el_info[i].flags & EL_INFO_IGNORE) {
			/* Si el objeto normalmente se destruye, mencionar la ignorancia; */
			if (obj->el_info[i].flags & EL_INFO_HATES)
				el_info[i].flags &= ~(EL_INFO_HATES);
			/* De lo contrario, no decir nada */
			else
				el_info[i].flags &= ~(EL_INFO_IGNORE);
		}

		/* No incluir el indicador de odio cuando es escueto */
		if (mode & OINFO_TERSE)
			el_info[i].flags &= ~(EL_INFO_HATES);
	}
}

/**
 * Obtiene información sobre el número de golpes posibles para el jugador
 * con el objeto dado.
 *
 * Rellena si el objeto es demasiado pesado para ser empuñado eficazmente,
 * y la información de possible_blows[] de .str_plus y .dex_plus necesarios
 * para lograr el número aproximado de golpes en centiblows.
 *
 * `max_blows` debe ser al menos 1 para contener el número actual de golpes.
 * `possible_blows` debe tener al menos el tamaño de [`max_blows`] y se limitará
 * a ese número de entradas. El máximo teórico es STAT_RANGE * 2 si se diera
 * un golpe extra/mejora de velocidad para cada combinación de FUE y DES.
 *
 * Devuelve el número de entradas realizadas en la tabla possible_blows[], o 0
 * si el objeto no es un arma.
 *
 * Nótese que los resultados no tienen sentido si se llama a un objeto de ego
 * falso, ya que el ego real puede tener propiedades diferentes.
 */
static int obj_known_blows(const struct object *obj, int max_num,
						   struct blow_info possible_blows[])
{
	int str_plus, dex_plus, old_blows = 0;
	int str_faster = -1, str_done = -1;
	int dex_plus_bound;
	int str_plus_bound;

	struct player_state state;

	int weapon_slot = slot_by_name(player, "weapon");
	struct object *current_weapon = slot_object(player, weapon_slot);
	int num = 0;

	/* No es un arma - ¡sin golpes! */
	if (!tval_is_melee_weapon(obj)) return 0;

	/* Fingir que estamos empuñando el objeto */
	player->body.slots[weapon_slot].obj = (struct object *) obj;

	/* Calcular el estado hipotético del jugador */
	memcpy(&state, &player->state, sizeof(state));
	state.stat_ind[STAT_STR] = 0; //Hack - NRM
	state.stat_ind[STAT_DEX] = 0; //Hack - NRM
	calc_bonuses(player, &state, true, false);

	/* La primera entrada es siempre el número actual de golpes. */
	possible_blows[num].str_plus = 0;
	possible_blows[num].dex_plus = 0;
	possible_blows[num].centiblows = state.num_blows;
	num++;

	/* Verificar si FUE o DES extra darían golpes adicionales */
	old_blows = state.num_blows;
	dex_plus_bound = STAT_RANGE - state.stat_ind[STAT_DEX];
	str_plus_bound = STAT_RANGE - state.stat_ind[STAT_STR];

	/* Recalcular con estadísticas aumentadas */
	for (dex_plus = 0; dex_plus < dex_plus_bound; dex_plus++) {
		for (str_plus = 0; str_plus < str_plus_bound; str_plus++) {
			int new_blows = 0;

			/* Improbable */
			if (num == max_num) {
				player->body.slots[weapon_slot].obj = current_weapon;
				return num;
			}

			state.stat_ind[STAT_STR] = str_plus; //Hack - NRM
			state.stat_ind[STAT_DEX] = dex_plus; //Hack - NRM
			calc_bonuses(player, &state, true, false);
			new_blows = state.num_blows;

			/* Probar que este golpe extra es una combinación nueva
			 * de fue/des, no una repetición */
			if (((new_blows - new_blows % 10) > (old_blows - old_blows % 10)) &&
				(str_plus < str_done || str_done == -1)) {
				possible_blows[num].str_plus = str_plus;
				possible_blows[num].dex_plus = dex_plus;
				possible_blows[num].centiblows = new_blows / 10;
				possible_blows[num].centiblows *= 10;
				num++;

				str_done = str_plus;
				break;
			}

			/* Si la combinación no incrementa el número de golpes
			 * mostrado, podría consumir un poco menos de energía */
			if ((new_blows > old_blows) &&
				(str_plus < str_faster || str_faster == -1) &&
				(str_plus < str_done || str_done == -1)) {
				possible_blows[num].str_plus = str_plus;
				possible_blows[num].dex_plus = dex_plus;
				possible_blows[num].centiblows = new_blows;
				num++;

				str_faster = str_plus;
			}
		}
	}

	/* Dejar de fingir */
	player->body.slots[weapon_slot].obj = current_weapon;

	return num;
}


/**
 * Describe los golpes.
 */
static bool describe_blows(textblock *tb, const struct object *obj)
{
	int i;
	struct blow_info blow_info[STAT_RANGE * 2]; /* Máximo (muy) teórico */
	int num_entries = 0;

	num_entries = obj_known_blows(obj, STAT_RANGE * 2, blow_info);
	if (num_entries == 0) return false;

	/* La primera entrada es siempre los golpes actuales (+0, +0) */
	textblock_append_c(tb, COLOUR_L_GREEN, "%d.%d ",
			blow_info[0].centiblows / 100, 
			(blow_info[0].centiblows / 10) % 10);
	textblock_append(tb, "golpe%s/ronda.\n",
			(blow_info[0].centiblows > 100) ? "s" : "");

	/* Luego listar combinaciones que dan más golpes / mejora de velocidad */
	for (i = 1; i < num_entries; i++) {
		struct blow_info entry = blow_info[i];

		if (entry.centiblows % 10 == 0) {
			textblock_append(tb, 
				"Con +%d FUE y +%d DES obtendrías %d.%d golpes\n",
				entry.str_plus, entry.dex_plus, 
				(entry.centiblows / 100),
				(entry.centiblows / 10) % 10);
		} else {
			textblock_append(tb, 
				"Con +%d FUE y +%d DES atacarías un poco más rápido\n",
				entry.str_plus, entry.dex_plus);
		}
	}

	return true;
}


/**
 * Obtiene información sobre el daño medio por turno que se puede infligir si
 * el jugador usa el arma dada. Utiliza los cálculos de daño estándar (no O).
 *
 * \param obj es el arma cuerpo a cuerpo o proyectil lanzado/arrojado a evaluar.
 * \param normal_damage se desreferencia y se establece al daño medio por
 * turno multiplicado por diez si no hay marcas o ataques especiales efectivos.
 * \param brand_damage debe apuntar a z_info->brand_max ints. brand_damage[i]
 * se establece al daño medio por turno multiplicado por diez con la i-ésima
 * marca del array global brands si esa marca está presente y no es anulada por
 * una marca más potente también presente para el mismo elemento; en caso
 * contrario, brand_damage[i] no se modifica.
 * \param slay_damage debe apuntar a z_info->slay_max ints. slay_damage[i]
 * se establece al daño medio por turno multiplicado por diez con el i-ésimo
 * ataque especial del array global slays si ese ataque está presente y no es
 * anulado por un ataque más potente también presente para los mismos monstruos;
 * en caso contrario, slay_damage[i] no se modifica.
 * \param nonweap_slay se desreferencia y se establece a true si un ataque
 * especial o marca fuera del arma afecta al daño, o a false si no.
 * \param throw hace que, si es true, el daño se calcule como si obj fuera
 * arrojado.
 * \return true si hay al menos una marca o ataque especial conocido que pueda
 * afectar al daño; en caso contrario, devuelve false.
 *
 * Nótese que los resultados no tienen sentido si se llama a un objeto de ego
 * falso, ya que el ego real puede tener propiedades diferentes.
 */
bool obj_known_damage(const struct object *obj, int *normal_damage,
							 int *brand_damage, int *slay_damage,
							 bool *nonweap_slay, bool throw)
{
	int i;
	int dice, sides, dam, total_dam, plus = 0;
	int xtra_postcrit = 0, xtra_precrit = 0;
	int crit_mult, crit_div, crit_add;
	int crit_round_mult, crit_round_add, crit_scl_round;
	int temp0, temp1, round;
	int old_blows = 0;
	bool *total_brands;
	bool *total_slays;
	bool has_brands_or_slays = false;

	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	bool weapon = tval_is_melee_weapon(obj) && !throw;
	bool ammo   = (player->state.ammo_tval == obj->tval) && (bow) && !throw;
	int melee_adj_mult = (ammo || throw) ? 0 : 1;
	int multiplier = 1;

	struct player_state state;
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *current_weapon = slot_object(player, weapon_slot);

	/* Fingir que estamos empuñando el objeto si es un arma */
	if (weapon)
		player->body.slots[weapon_slot].obj = (struct object *) obj;

	/* Calcular el estado hipotético del jugador */
	memcpy(&state, &player->state, sizeof(state));
	state.stat_ind[STAT_STR] = 0; //Hack - NRM
	state.stat_ind[STAT_DEX] = 0; //Hack - NRM
	calc_bonuses(player, &state, true, false);

	/* Dejar de fingir */
	player->body.slots[weapon_slot].obj = current_weapon;

	/* Terminar si no se conocen los dados */
	dice = obj->known->dd;
	sides = obj->known->ds;
	if (!dice || !sides) return false;

	/* Calcular daño */
	dam = ((sides + 1) * dice * 5);

	plus += object_to_hit(obj->known);
	if (weapon)	{
		xtra_postcrit = state.to_d * 10;
		xtra_precrit += object_to_dam(obj->known) * 10;

		calculate_melee_crits(&state, object_weight_one(obj), plus,
			&crit_mult, &crit_add, &crit_div,
			&crit_round_mult, &crit_round_add, &crit_scl_round);

		old_blows = state.num_blows;
	} else if (ammo) {
		calculate_missile_crits(&player->state, object_weight_one(obj),
			plus, true, &crit_mult, &crit_add, &crit_div,
			&crit_round_mult, &crit_round_add, &crit_scl_round);

		dam += (object_to_dam(obj->known) * 10);
		dam += (object_to_dam(bow->known) * 10);
	} else {
		calculate_missile_crits(&player->state, object_weight_one(obj),
			plus, false, &crit_mult, &crit_add, &crit_div,
			&crit_round_mult, &crit_round_add, &crit_scl_round);

		dam += (object_to_dam(obj->known) * 10);
		dam *= 2 + object_weight_one(obj) / 12;
	}

	if (ammo) multiplier = player->state.ammo_mult;

	/* Obtener las marcas */
	total_brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	copy_brands(&total_brands, obj->known->brands);
	if (ammo && bow->known)
		copy_brands(&total_brands, bow->known->brands);

	/* Obtener los ataques especiales */
	total_slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	copy_slays(&total_slays, obj->known->slays);
	if (ammo && bow->known)
		copy_slays(&total_slays, bow->known->slays);

	/*
	 * Las armas cuerpo a cuerpo pueden obtener ataques especiales y marcas
	 * de otros objetos o de efectos temporales.
	 */
	*nonweap_slay = false;
	if (weapon)	{
		for (i = 2; i < player->body.count; i++) {
			struct object *slot_obj = slot_object(player, i);
			if (!slot_obj)
				continue;

			if (slot_obj->known->brands || slot_obj->known->slays)
				*nonweap_slay = true;
			else
				continue;

			/* Reemplazar las listas antiguas con las nuevas */
			copy_brands(&total_brands, slot_obj->known->brands);
			copy_slays(&total_slays, slot_obj->known->slays);
		}

		for (i = 1; i < z_info->brand_max; i++) {
			if (player_has_temporary_brand(player, i)
					&& append_brand(&total_brands, i)) {
				*nonweap_slay = true;
			}
		}

		for (i = 1; i < z_info->slay_max; i++) {
			if (player_has_temporary_slay(player, i)
					&& append_slay(&total_slays, i)) {
				*nonweap_slay = true;
			}
		}
	}

	/* Obtener daño para cada marca activa */
	for (i = 1; i < z_info->brand_max; i++) {
		if (!total_brands[i]) {
			continue;
		}
		has_brands_or_slays = true;

		/* Incluir daño extra y marca en el promedio indicado */
		temp0 = dam * (multiplier + brands[i].multiplier
			- melee_adj_mult) + xtra_precrit;
		temp1 = temp0 * crit_mult + 10 * crit_add
			+ (temp0 * crit_round_mult + 10 * crit_round_add)
			/ crit_scl_round;
		total_dam = temp1 / crit_div + xtra_postcrit;
		round = temp1 % crit_div;

		if (weapon) {
			temp0 = total_dam * old_blows
				+ (round * old_blows) / crit_div;
			total_dam = temp0 / 100 + ((temp0 % 100 >= 50) ? 1 : 0);
		} else if (ammo) {
			temp0 = total_dam * player->state.num_shots
				+ (round * player->state.num_shots) / crit_div;
			total_dam = temp0 / 10 + ((temp0 % 10 >= 5) ? 1 : 0);
		} else {
			total_dam += (round > (crit_div + 1) / 2) ? 1 : 0;
		}

		brand_damage[i] = total_dam;
	}

	/* Obtener daño para cada ataque especial activo */
	for (i = 1; i < z_info->slay_max; i++) {
		if (!total_slays[i]) {
			continue;
		}
		has_brands_or_slays = true;

		/* Incluir daño extra y ataque especial en el promedio indicado */
		temp0 = dam * (multiplier + slays[i].multiplier
			- melee_adj_mult) + xtra_precrit;
		temp1 = temp0 * crit_mult + 10 * crit_add
			+ (temp0 * crit_round_mult + 10 * crit_round_add)
			/ crit_scl_round;
		total_dam = temp1 / crit_div + xtra_postcrit;
		round = temp1 % crit_div;

		if (weapon) {
			temp0 = total_dam * old_blows
				+ (round * old_blows) / crit_div;
			total_dam = temp0 / 100 + ((temp0 % 100 >= 50) ? 1 : 0);
		} else if (ammo) {
			temp0 = total_dam * player->state.num_shots
				+ (round * player->state.num_shots) / crit_div;
			total_dam = temp0 / 10 + ((temp0 % 10 >= 5) ? 1 : 0);
		} else {
			total_dam += (round >= (crit_div + 1) / 2) ? 1 : 0;
		}

		slay_damage[i] = total_dam;
	}

	/* Incluir daño extra en el promedio indicado */
	temp0 = dam * multiplier + xtra_precrit;
	temp1 = temp0 * crit_mult + 10 * crit_add
		+ (temp0 * crit_round_mult + 10 * crit_round_add)
		/ crit_scl_round;
	total_dam = temp1 / crit_div + xtra_postcrit;
	round = temp1 % crit_div;

	/* Daño normal, sin considerar marcas o ataques especiales */
	if (weapon) {
		temp0 = total_dam * old_blows
			+ (round * old_blows) / crit_div;
		total_dam = temp0 / 100 + ((temp0 % 100 >= 50) ? 1 : 0);
	} else if (ammo) {
		temp0 = total_dam * player->state.num_shots
			+ (round * player->state.num_shots) / crit_div;
		total_dam = temp0 / 10 + ((temp0 % 10 >= 5) ? 1 : 0);
	} else {
		total_dam += (round > (crit_div + 1) / 2) ? 1 : 0;
	}

	*normal_damage = total_dam;

	mem_free(total_brands);
	mem_free(total_slays);
	return has_brands_or_slays;
}


/**
 * Obtiene información sobre el daño medio por turno que se puede infligir si
 * el jugador usa el arma dada. Utiliza los cálculos de daño de OAngband.
 *
 * \param obj es el arma cuerpo a cuerpo o proyectil lanzado/arrojado a evaluar.
 * \param normal_damage se desreferencia y se establece al daño medio por
 * turno multiplicado por diez si no hay marcas o ataques especiales efectivos.
 * \param brand_damage debe apuntar a z_info->brand_max ints. brand_damage[i]
 * se establece al daño medio por turno multiplicado por diez con la i-ésima
 * marca del array global brands si esa marca está presente y no es anulada por
 * una marca más potente también presente para el mismo elemento; en caso
 * contrario, brand_damage[i] no se modifica.
 * \param slay_damage debe apuntar a z_info->slay_max ints. slay_damage[i]
 * se establece al daño medio por turno multiplicado por diez con el i-ésimo
 * ataque especial del array global slays si ese ataque está presente y no es
 * anulado por un ataque más potente también presente para los mismos monstruos;
 * en caso contrario, slay_damage[i] no se modifica.
 * \param nonweap_slay se desreferencia y se establece a true si un ataque
 * especial o marca fuera del arma afecta al daño, o a false si no.
 * \param throw hace que, si es true, el daño se calcule como si obj fuera
 * arrojado.
 * \return true si hay al menos una marca o ataque especial conocido que pueda
 * afectar al daño; en caso contrario, devuelve false.
 *
 * Nótese que los resultados no tienen sentido si se llama a un objeto de ego
 * falso, ya que el ego real puede tener propiedades diferentes.
 */
bool o_obj_known_damage(const struct object *obj, int *normal_damage,
								 int *brand_damage, int *slay_damage,
							   bool *nonweap_slay, bool throw)
{
	int i;
	int dice, sides, die_average, total_dam;
	unsigned int added_dice, remainder;
	struct my_rational frac_dice, frac_temp;
	int temp0, round;
	int deadliness = object_to_dam(obj->known);
	int old_blows = 0;
	bool *total_brands;
	bool *total_slays;
	bool has_brands_or_slays = false;

	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	bool weapon = tval_is_melee_weapon(obj) && !throw;
	bool ammo   = (player->state.ammo_tval == obj->tval) && (bow) && !throw;
	int multiplier = 1;

	struct player_state state;
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *current_weapon = slot_object(player, weapon_slot);

	/* Fingir que estamos empuñando el objeto si es un arma */
	if (weapon)
		player->body.slots[weapon_slot].obj = (struct object *) obj;

	/* Calcular el estado hipotético del jugador */
	memcpy(&state, &player->state, sizeof(state));
	state.stat_ind[STAT_STR] = 0; //Hack - NRM
	state.stat_ind[STAT_DEX] = 0; //Hack - NRM
	calc_bonuses(player, &state, true, false);

	/* Dejar de fingir */
	player->body.slots[weapon_slot].obj = current_weapon;

	/* Terminar si no se conocen los dados */
	dice = obj->known->dd * 100;
	sides = obj->known->ds;
	if (!dice || !sides) return false;

	/* Obtener el número de dados adicionales por críticos (x100) */
	if (weapon)	{
		o_calculate_melee_crits(&state, obj, &added_dice, &frac_dice);
		dice += added_dice;
		old_blows = state.num_blows;
	} else if (ammo) {
		o_calculate_missile_crits(&player->state, obj, bow,
			&added_dice, &frac_dice);
		dice += added_dice;
	} else {
		unsigned int thrown_scl = 2 + object_weight_one(obj) / 12;

		o_calculate_missile_crits(&player->state, obj, NULL,
			&added_dice, &frac_dice);
		dice += added_dice;
		dice *= thrown_scl;
		dice += my_rational_to_uint(&frac_dice, thrown_scl, &remainder);
		frac_dice = my_rational_construct(remainder, frac_dice.d);
	}

	if (ammo) multiplier = player->state.ammo_mult;

	/* Obtener el valor medio de un dado de daño. (x10) */
	die_average = 5 * (sides + 1);

	/* Aplicar el multiplicador del lanzador. */
	die_average *= multiplier;

	/* Aplicar letalidad al promedio. (inflación x100) */
	if (ammo) {
		deadliness += object_to_dam(bow->known) + state.to_d;
	} else {
		deadliness += state.to_d;
	}
	apply_deadliness(&die_average, MIN(deadliness, 150));

	/* Obtener las marcas */
	total_brands = mem_zalloc(z_info->brand_max * sizeof(bool));
	copy_brands(&total_brands, obj->known->brands);
	if (ammo && bow->known)
		copy_brands(&total_brands, bow->known->brands);

	/* Obtener los ataques especiales */
	total_slays = mem_zalloc(z_info->slay_max * sizeof(bool));
	copy_slays(&total_slays, obj->known->slays);
	if (ammo && bow->known)
		copy_slays(&total_slays, bow->known->slays);

	/*
	 * Las armas cuerpo a cuerpo pueden obtener ataques especiales y marcas
	 * de otros objetos o de efectos temporales.
	 */
	*nonweap_slay = false;
	if (weapon)	{
		for (i = 2; i < player->body.count; i++) {
			struct object *slot_obj = slot_object(player, i);
			if (!slot_obj)
				continue;

			if (slot_obj->known->brands || slot_obj->known->slays)
				*nonweap_slay = true;
			else
				continue;

			/* Reemplazar las listas antiguas con las nuevas */
			copy_brands(&total_brands, slot_obj->known->brands);
			copy_slays(&total_slays, slot_obj->known->slays);
		}

		for (i = 1; i < z_info->brand_max; i++) {
			if (player_has_temporary_brand(player, i)
					&& append_brand(&total_brands, i)) {
				*nonweap_slay = true;
			}
		}

		for (i = 1; i < z_info->slay_max; i++) {
			if (player_has_temporary_slay(player, i)
					&& append_slay(&total_slays, i)) {
				*nonweap_slay = true;
			}
		}
	}

	/* Aumentar el promedio del dado por cada marca activa */
	for (i = 1; i < z_info->brand_max; i++) {
		int brand_average, add = brands[i].o_multiplier - 10;

		if (!total_brands[i]) {
			continue;
		}
		has_brands_or_slays = true;

		/* Incluir marca en el promedio indicado (x10), deflactar (/1000) */
		brand_average = die_average * brands[i].o_multiplier;
		round = brand_average % 1000;
		brand_average /= 1000;

		/* El daño por golpe es ahora dados * promedio del dado, (aún x1000) */
		temp0 = dice * brand_average + (dice * round) / 1000
			+ my_rational_to_uint(&frac_dice, brand_average,
			&remainder);
		frac_temp = my_rational_construct(remainder, frac_dice.d);
		round = (dice * round) % 1000
			+ my_rational_to_uint(&frac_temp, 1000, &remainder);
		if (remainder >= (frac_temp.d + 1) / 2) {
			++round;
		}

		/* Ahora ajustar por golpes y disparos y deflactar de nuevo */
		if (weapon) {
			total_dam = old_blows * temp0
				+ (old_blows * round) / 1000;
			round = total_dam % 10000;
			total_dam /= 10000;
			total_dam += (add * old_blows) / 10
				+ ((round >= 5000) ? 1 : 0);
		} else if (ammo) {
			total_dam = player->state.num_shots * temp0
				+ (player->state.num_shots * round) / 1000;
			round = total_dam % 1000;
			total_dam /= 1000;
			total_dam += add * player->state.num_shots
				+ ((round >= 500) ? 1 : 0);
		} else {
			total_dam = temp0 / 100 + add * 10
				+ ((temp0 % 100 >= 50) ? 1 : 0);
		}

		brand_damage[i] = total_dam;
	}

	/* Obtener daño para cada ataque especial activo */
	for (i = 1; i < z_info->slay_max; i++) {
		int slay_average, add = slays[i].o_multiplier - 10;

		if (!total_slays[i]) {
			continue;
		}
		has_brands_or_slays = true;

		/* Incluir ataque especial en el promedio indicado (x10), deflactar (/1000) */
		slay_average = die_average * slays[i].o_multiplier;
		round = slay_average % 1000;
		slay_average /= 1000;

		/* El daño por golpe es ahora dados * promedio del dado, (aún x1000) */
		temp0 = dice * slay_average + (dice * round) / 1000
			+ my_rational_to_uint(&frac_dice, slay_average,
			&remainder);
		frac_temp = my_rational_construct(remainder, frac_dice.d);
		round = (dice * round) % 1000
			+ my_rational_to_uint(&frac_temp, 1000, &remainder);
		if (remainder >= (frac_temp.d + 1) / 2) {
			++round;
		}

		/* Ahora ajustar por golpes y disparos y deflactar de nuevo */
		if (weapon) {
			total_dam = old_blows * temp0
				+ (old_blows * round) / 1000;
			round = total_dam % 10000;
			total_dam /= 10000;
			total_dam += (add * old_blows) / 10
				+ ((round >= 5000) ? 1 : 0);
		} else if (ammo) {
			total_dam = player->state.num_shots * temp0
				+ (player->state.num_shots * round) / 1000;
			round = total_dam % 1000;
			total_dam /= 1000;
			total_dam += add * player->state.num_shots
				+ ((round >= 500) ? 1 : 0);
		} else {
			total_dam = temp0 / 100 + add * 10
				+ ((temp0 % 100 >= 50) ? 1 : 0);
		}

		slay_damage[i] = total_dam;
	}

	/* Daño normal, sin considerar marcas o ataques especiales */
	temp0 = dice * die_average +
		my_rational_to_uint(&frac_dice, die_average, &remainder);
	if (remainder >= (frac_dice.d + 1) / 2) {
		++temp0;
	}
	round = temp0 % 1000;
	temp0 /= 1000;
	if (weapon) {
		total_dam = old_blows * temp0 + (old_blows * round) / 1000;
		round = total_dam % 1000;
		total_dam /= 1000;
		total_dam += (round >= 500) ? 1 : 0;
	} else if (ammo) {
		total_dam = player->state.num_shots * temp0
			+ (player->state.num_shots * round) / 1000;
		round = total_dam % 100;
		total_dam /= 100;
		total_dam += (round >= 50) ? 1 : 0;
	} else {
		total_dam = temp0 / 10 + ((temp0 % 10 >= 5) ? 1 : 0);
	}
	*normal_damage = total_dam;

	mem_free(total_brands);
	mem_free(total_slays);
	return has_brands_or_slays;
}


/**
 * Describe el daño.
 */
static bool describe_damage(textblock *tb, const struct object *obj, bool throw)
{
	int i;
	bool nonweap_slay = false;
	int normal_damage = 0;
	int *brand_damage = mem_zalloc(z_info->brand_max * sizeof(int));
	int *slay_damage = mem_zalloc(z_info->slay_max * sizeof(int));

	/* Recolectar marcas y ataques especiales */
	bool has_brands_or_slays = OPT(player, birth_percent_damage) ?
		o_obj_known_damage(obj, &normal_damage, brand_damage, slay_damage,
						   &nonweap_slay, throw) :
		obj_known_damage(obj, &normal_damage, brand_damage, slay_damage,
						 &nonweap_slay, throw);

	/* Mencionar ataques especiales y marcas de otros objetos */
	if (nonweap_slay)
		textblock_append(tb, "Esta arma puede beneficiarse de una o más marcas o ataques especiales fuera del arma.\n");

	if (throw) {
		textblock_append(tb, "Daño medio al arrojar: ");
	} else {
		textblock_append(tb, "Daño medio/ronda: ");
	}

	if (has_brands_or_slays) {
		/*
		 * Ordenar por daño decreciente para que las entradas con el mismo
		 * daño se puedan imprimir juntas.
		 */
		int *sortind = mem_alloc(
			(z_info->brand_max + z_info->slay_max) *
			sizeof(*sortind));
		int nsort = 0;
		const char *lastnm;
		int lastdam, groupn;
		bool last_is_brand;

		/*
		 * Ensamblar los índices. Hacer los ataques especiales primero
		 * para que, si hay empate en daño, aparezcan antes. Es más fácil de leer.
		 */
		for (i = 0; i < z_info->slay_max; i++) {
			if (slay_damage[i] > 0) {
				sortind[nsort] = i + z_info->brand_max;
				++nsort;
			}
		}
		for (i = 0; i < z_info->brand_max; i++) {
			if (brand_damage[i] > 0) {
				sortind[nsort] = i;
				++nsort;
			}
		}
		/* Ordenar. Como el número es pequeño, la ordenación por inserción es suficiente. */
		for (i = 0; i < nsort - 1; i++) {
			int maxdam = (sortind[i] < z_info->brand_max) ?
				brand_damage[sortind[i]] :
				slay_damage[sortind[i] - z_info->brand_max];
			int maxind = i;
			int j;

			for (j = i + 1; j < nsort; j++) {
				int dam = (sortind[j] < z_info->brand_max) ?
					brand_damage[sortind[j]] :
					slay_damage[sortind[j] -
						z_info->brand_max];

				if (maxdam < dam) {
					maxdam = dam;
					maxind = j;
				}
			}
			if (maxind != i) {
				int tmp = sortind[maxind];

				sortind[maxind] = sortind[i];
				sortind[i] = tmp;
			}
		}

		/* Salida. */
		lastdam = 0;
		groupn = 0;
		lastnm = NULL;
		last_is_brand = false;
		for (i = 0; i < nsort; i++) {
			const char *tgt;
			int dam;
			bool is_brand;

			if (sortind[i] < z_info->brand_max) {
				is_brand = true;
				tgt = brands[sortind[i]].name;
				dam = brand_damage[sortind[i]];
			} else {
				is_brand = false;
				tgt = slays[sortind[i] -
					z_info->brand_max].name;
				dam = slay_damage[sortind[i] -
					z_info->brand_max];
			}

			if (groupn > 0) {
				if (dam != lastdam) {
					if (groupn > 2) {
						textblock_append(tb, " y");
					} else if (groupn == 2) {
						textblock_append(tb, " y");
					}
				} else if (groupn > 1) {
					textblock_append(tb, ",");
				}
				if (last_is_brand) {
					textblock_append(tb,
						" criaturas no resistentes a");
				}
				textblock_append(tb, " %s", lastnm);
			}
			if (dam != lastdam) {
				if (i != 0) {
					textblock_append(tb, ", ");
				}
				if (dam % 10) {
					textblock_append_c(tb, COLOUR_L_GREEN,
						"%d.%d contra", dam / 10, dam % 10);
				} else {
					textblock_append_c(tb, COLOUR_L_GREEN,
						"%d contra", dam / 10);
				}
				groupn = 1;
				lastdam = dam;
			} else {
				assert(groupn > 0);
				++groupn;
			}
			lastnm = tgt;
			last_is_brand = is_brand;
		}
		if (groupn > 0) {
			if (groupn > 2) {
				textblock_append(tb, " y");
			} else if (groupn == 2) {
				textblock_append(tb, " y");
			}
			if (last_is_brand) {
				textblock_append(tb,
					" criaturas no resistentes a");
			}
			textblock_append(tb, " %s", lastnm);
		}

		if (nsort == 0) {
			has_brands_or_slays = false;
		} else {
			textblock_append(tb, (nsort == 1) ? " y " : ", y ");
		}
		mem_free(sortind);
	}

	if (normal_damage <= 0)
		textblock_append_c(tb, COLOUR_L_RED, "%d", 0);
	else if (normal_damage % 10)
		textblock_append_c(tb, COLOUR_L_GREEN, "%d.%d",
			   normal_damage / 10, normal_damage % 10);
	else
		textblock_append_c(tb, COLOUR_L_GREEN, "%d", normal_damage / 10);

	if (has_brands_or_slays) textblock_append(tb, " contra otros");
	textblock_append(tb, ".\n");

	mem_free(brand_damage);
	mem_free(slay_damage);
	return true;
}

/**
 * Obtiene información diversa de combate sobre el objeto dado.
 *
 * Rellena si hay un efecto especial al arrojar en `thrown effect`,
 * el `alcance` en pies (o cero si no es munición), el porcentaje de rotura
 * y si es demasiado pesado para ser empuñado eficazmente en el momento.
 */
static void obj_known_misc_combat(const struct object *obj, bool *thrown_effect,
								  int *range, int *break_chance, bool *heavy)
{
	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	bool weapon = tval_is_melee_weapon(obj);
	bool ammo   = (player->state.ammo_tval == obj->tval) && (bow);

	*thrown_effect = *heavy = false;
	*range = *break_chance = 0;

	if (!weapon && !ammo) {
		/* Las pociones pueden tener texto especial */
		if (tval_is_potion(obj) && obj->dd != 0 && obj->ds != 0 &&
			object_flavor_is_aware(obj))
			*thrown_effect = true;
	}

	if (ammo)
		*range = 10 * MIN(6 + 2 * player->state.ammo_mult, z_info->max_range);

	/* Añadir probabilidad de rotura */
	*break_chance = breakage_chance(obj, true);

	/* ¿Es el arma demasiado pesada? */
	if (weapon) {
		struct player_state state;
		int weapon_slot = slot_by_name(player, "weapon");
		struct object *current = equipped_item_by_slot_name(player, "weapon");

		/* Fingir que estamos empuñando el objeto */
		player->body.slots[weapon_slot].obj = (struct object *) obj;

		/* Calcular el estado hipotético del jugador */
		memcpy(&state, &player->state, sizeof(state));
		state.stat_ind[STAT_STR] = 0; //Hack - NRM
		state.stat_ind[STAT_DEX] = 0; //Hack - NRM
		calc_bonuses(player, &state, true, false);

		/* Dejar de fingir */
		player->body.slots[weapon_slot].obj = current;

		/* Advertir sobre armas pesadas */
		*heavy = state.heavy_wield;
	}
}


/**
 * Describe las ventajas de combate.
 */
static bool describe_combat(textblock *tb, const struct object *obj)
{
	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	bool weapon = tval_is_melee_weapon(obj);
	bool ammo   = (player->state.ammo_tval == obj->tval) && (bow);
	bool throwing_weapon = weapon && of_has(obj->flags, OF_THROWING);
	bool rock = tval_is_ammo(obj) && of_has(obj->flags, OF_THROWING);

	int range, break_chance;
	bool thrown_effect, heavy;

	obj_known_misc_combat(obj, &thrown_effect, &range, &break_chance, &heavy);

	if (!weapon && !ammo && !rock) {
		if (thrown_effect) {
			textblock_append(tb, "Puede ser arrojado a criaturas con efecto dañino.\n");
			return true;
		} else
			return false;
	}

	textblock_append_c(tb, COLOUR_L_WHITE, "Información de combate:\n");

	if (heavy)
		textblock_append_c(tb, COLOUR_L_RED, "Eres demasiado débil para usar esta arma.\n");

	describe_blows(tb, obj);

	if (ammo) {
		textblock_append(tb, "Al disparar, alcanza objetivos hasta ");
		textblock_append_c(tb, COLOUR_L_GREEN, "%d", range);
		textblock_append(tb, " pies de distancia.\n");
	}

	if (weapon || ammo) {
		describe_damage(tb, obj, false);
	}
	if (throwing_weapon || rock) {
		describe_damage(tb, obj, true);
	}

	if (ammo) {
		textblock_append_c(tb, COLOUR_L_GREEN, "%d%%", break_chance);
		textblock_append(tb, " de probabilidad de romperse al contacto.\n");
	}

	/* Se ha dicho algo */
	return true;
}


/**
 * Devuelve información sobre objetos que se pueden usar para cavar.
 *
 * `deciturns` se llenará con el número medio de deciturnos que se tardará
 * en cavar cada tipo de terreno excavable, y debe tener al menos
 * [DIGGING_MAX].
 *
 * Devuelve false si el objeto no tiene efecto en la excavación, o si los
 * detalles no tienen sentido (es decir, el objeto es una plantilla de ego,
 * no un objeto real).
 */
static bool obj_known_digging(struct object *obj, int deciturns[])
{
	struct player_state state;
	int i;
	int chances[DIGGING_MAX];
	int slot;
	struct object *current;

	/* No se parece ni remotamente a un cavador */
	if (!tval_is_wearable(obj) ||
		(!tval_is_melee_weapon(obj) && (obj->modifiers[OBJ_MOD_TUNNEL] <= 0)))
		return false;

	/* El jugador no tiene información de excavación */
	if (!tval_is_melee_weapon(obj) && !obj->known->modifiers[OBJ_MOD_TUNNEL])
		return false;

	/* Fingir que estamos empuñando el objeto */
	slot = wield_slot(obj);
	current = slot_object(player, slot);
	player->body.slots[slot].obj = obj;

	/* Calcular el estado hipotético del jugador */
	memcpy(&state, &player->state, sizeof(state));
	state.stat_ind[STAT_STR] = 0; //Hack - NRM
	state.stat_ind[STAT_DEX] = 0; //Hack - NRM
	calc_bonuses(player, &state, true, false);

	/* Dejar de fingir */
	player->body.slots[slot].obj = current;

	calc_digging_chances(&state, chances);

	/* La probabilidad de excavar es de 1600 */
	for (i = DIGGING_RUBBLE; i < DIGGING_MAX; i++) {
		int chance = MIN(1600, chances[i]);
		deciturns[i] = chance ? (16000 / chance) : 0;
	}

	return true;
}

/**
 * Describe objetos que se pueden usar para cavar.
 */
static bool describe_digger(textblock *tb, const struct object *obj)
{
	int i;
	int deciturns[DIGGING_MAX];
	struct object *obj1 = (struct object *) obj;
	static const char *names[4] = { "escombros", "vetas de magma", "vetas de cuarzo",
									"granito" };

	/* Obtener información útil o no imprimir nada */
	if (!obj_known_digging(obj1, deciturns)) return false;

	for (i = DIGGING_RUBBLE; i < DIGGING_DOORS; i++) {
		if (i == 0 && deciturns[0] > 0) {
			if (tval_is_melee_weapon(obj))
				textblock_append(tb, "Limpia ");
			else
				textblock_append(tb, "Con este objeto, tu arma actual limpia ");
		}

		if (i == 3 || (i != 0 && deciturns[i] == 0))
			textblock_append(tb, "y ");

		if (deciturns[i] == 0) {
			textblock_append_c(tb, COLOUR_L_RED, "no afecta a ");
			textblock_append(tb, "%s.\n", names[i]);
			break;
		}

		textblock_append(tb, "%s en ", names[i]);

		if (deciturns[i] == 10) {
			textblock_append_c(tb, COLOUR_L_GREEN, "1 ");
		} else if (deciturns[i] < 100) {
			textblock_append_c(tb, COLOUR_GREEN, "%d.%d ", deciturns[i]/10,
							   deciturns[i]%10);
		} else {
			textblock_append_c(tb, (deciturns[i] < 1000) ? COLOUR_YELLOW :
							   COLOUR_RED, "%d ", (deciturns[i]+5)/10);
		}

		textblock_append(tb, "turno%s%s", deciturns[i] == 10 ? "" : "s",
				(i == 3) ? ".\n" : ", ");
	}

	return true;
}

/**
 * Proporciona las características conocidas de fuente de luz del objeto dado.
 *
 * Rellena la intensidad de la luz en `intensity`, si usa combustible y
 * cuántos turnos de luz puede recargar en objetos similares.
 *
 * Devuelve false si no se sabe que el objeto sea una fuente de luz (lo que
 * incluye que realmente no lo sea).
 */
static bool obj_known_light(const struct object *obj, oinfo_detail_t mode,
							int *intensity, bool *uses_fuel, int *refuel_turns)
{
	bool no_fuel;
	bool is_light = tval_is_light(obj);

	if (!is_light && (obj->modifiers[OBJ_MOD_LIGHT] <= 0))
		return false;

	/* Calcular intensidad */
	if (of_has(obj->flags, OF_LIGHT_2))
		*intensity = 2;
	else if (of_has(obj->flags, OF_LIGHT_3))
		*intensity = 3;
	*intensity += obj->known->modifiers[OBJ_MOD_LIGHT];

	/* Evitar que objetos no identificados (especialmente luces de artefacto)
	 * muestren intensidad incorrecta e información de recarga. */
	if (*intensity == 0)
		return false;

	no_fuel = of_has(obj->known->flags, OF_NO_FUEL) ? true : false;

	if (no_fuel || obj->known->artifact) {
		*uses_fuel = false;
	} else {
		*uses_fuel = true;
	}

	if (is_light && of_has(obj->known->flags, OF_TAKES_FUEL)) {
		*refuel_turns = z_info->fuel_lamp;
	} else {
		*refuel_turns = 0;
	}

	return true;
}

/**
 * Describe objetos que parecen fuentes de luz.
 */
static bool describe_light(textblock *tb, const struct object *obj,
						   oinfo_detail_t mode)
{
	int intensity = 0;
	bool uses_fuel = false;
	int refuel_turns = 0;
	bool terse = mode & OINFO_TERSE ? true : false;

	if (!obj_known_light(obj, mode, &intensity, &uses_fuel, &refuel_turns))
		return false;

	if (tval_is_light(obj)) {
		textblock_append(tb, "Luz de intensidad ");
		textblock_append_c(tb, COLOUR_L_GREEN, "%d", intensity);
		textblock_append(tb, ".");

		if (!obj->artifact && !uses_fuel)
			textblock_append(tb, "  No requiere combustible.");

		if (!terse) {
			if (refuel_turns)
				textblock_append(tb, "  Recarga otras linternas hasta %d turnos de combustible.", refuel_turns);
			else
				textblock_append(tb, "  No se puede recargar.");
		}
		textblock_append(tb, "\n");
	}

	return true;
}


/**
 * Describe libros legibles.
 */
static bool describe_book(textblock *tb, const struct object *obj,
						   oinfo_detail_t mode)
{
	if (!obj_can_browse(obj)) return false;

	textblock_append(tb, "\nPuedes leer este libro.\n");

	return true;
}


/**
 * Proporciona los efectos conocidos de usar el objeto dado.
 *
 * Rellena:
 *  - el efecto
 *  - si el efecto puede ser dirigido
 *  - el tiempo mínimo y máximo en turnos de juego para que el objeto se
 *    recargue (o cero si no se recarga)
 *  - el porcentaje de probabilidad de que el efecto falle al usarse
 *
 * Devuelve false si el objeto no tiene efecto.
 */
static bool obj_known_effect(const struct object *obj, struct effect **effect,
								 bool *aimed, int *min_recharge,
								 int *max_recharge, int *failure_chance)
{
	random_value timeout = {0, 0, 0, 0};
	bool store_consumable = object_is_in_store(obj) && tval_is_useable(obj);

	*effect = NULL;
	*min_recharge = 0;
	*max_recharge = 0;
	*failure_chance = 0;
	*aimed = false;

	if (object_effect_is_known(obj) || store_consumable) {
		*effect = object_effect(obj);
		timeout = obj->time;
		if (effect_aim(*effect))
			*aimed = true;;
	} else if (object_effect(obj)) {
		/* No se sabe mucho - ser vago */
		*effect = NULL;
		if (tval_is_wand(obj) || tval_is_rod(obj)) {
			*aimed = true;
		}
		return true;
	} else {
		/* Sin efecto - sin información */
		return false;
	}

	if (randcalc(timeout, 0, MAXIMISE) > 0)	{
		*min_recharge = randcalc(timeout, 0, MINIMISE);
		*max_recharge = randcalc(timeout, 0, MAXIMISE);
	}

	if (tval_is_edible(obj) || tval_is_potion(obj) || tval_is_scroll(obj)) {
		*failure_chance = 0;
	} else {
		*failure_chance = get_use_device_chance(obj);
	}

	return true;
}

/**
 * Describe el efecto de un objeto, si lo hay.
 */
static bool describe_effect(textblock *tb, const struct object *obj,
		bool only_artifacts, bool subjective)
{
	struct effect *effect = NULL;
	bool aimed = false;
	int min_time, max_time, failure_chance;

	/* A veces solo imprimimos información de activación de artefactos */
	if (only_artifacts && !obj->artifact) {
		return false;
	}

	if (obj_known_effect(obj, &effect, &aimed, &min_time, &max_time,
						 &failure_chance) == false) {
		return false;
	}

	/* Efecto no conocido, decir generalidades */
	if (!effect && object_effect(obj)) {
		if (tval_is_edible(obj)) {
			textblock_append(tb, "Puede ser comido.\n");
		} else if (tval_is_potion(obj)) {
			textblock_append(tb, "Puede ser bebido.\n");
		} else if (tval_is_scroll(obj)) {
			textblock_append(tb, "Puede ser leído.\n");
		} else if (aimed) {
			textblock_append(tb, "Puede ser dirigido.\n");
		} else {
			textblock_append(tb, "Puede ser activado.\n");
		}

		return true;
	}

	/* Las activaciones tienen un mensaje especial */
	if (obj->activation && obj->activation->desc) {
		textblock_append(tb, "Cuando se activa, ");
		textblock_append(tb, "%s", obj->activation->desc);
	} else {
		int level = obj->artifact ?
			obj->artifact->level : (obj->activation ?
			obj->activation->level : obj->kind->level);
		int boost = MAX((player->state.skills[SKILL_DEVICE] - level) / 2, 0);
		const char *prefix;
		textblock *tbe;

		if (obj->activation)
			prefix = "Cuando se activa, ";
		else if (aimed)
			prefix = "Cuando se dirige, ";
		else if (tval_is_edible(obj))
			prefix = "Cuando se come, ";
		else if (tval_is_potion(obj))
			prefix = "Cuando se bebe, ";
		else if (tval_is_scroll(obj))
			prefix = "Cuando se lee, ";
		else
			prefix = "Cuando se activa, ";

		tbe = effect_describe(effect, prefix, boost, false);
		if (! tbe) {
			return false;
		}
		textblock_append_textblock(tb, tbe);
		textblock_free(tbe);
	}

	textblock_append(tb, ".\n");

	if (min_time || max_time) {
		/* A veces ajustar por la velocidad del jugador */
		int multiplier = turn_energy(player->state.speed);
		if (!subjective) multiplier = 10;

		textblock_append(tb, "Tarda ");

		/* Corregir por la velocidad del jugador */
		min_time = (min_time * multiplier) / 10;
		max_time = (max_time * multiplier) / 10;

		textblock_append_c(tb, COLOUR_L_GREEN, "%d", min_time);

		if (min_time != max_time) {
			textblock_append(tb, " a ");
			textblock_append_c(tb, COLOUR_L_GREEN, "%d", max_time);
		}

		textblock_append(tb, " turnos en recargarse");
		if (subjective && player->state.speed != 110)
			textblock_append(tb, " a tu velocidad actual");

		textblock_append(tb, ".\n");
	}

	if (failure_chance > 0) {
		textblock_append(tb, "Tu probabilidad de éxito es %d.%d%%\n", 
			(1000 - failure_chance) / 10, (1000 - failure_chance) % 10);
	}

	return true;
}

/**
 * Describe el origen de un objeto
 */
static bool describe_origin(textblock *tb, const struct object *obj, bool terse)
{
	char loot_spot[80];
	char name[80];
	int origin;
	const char *dropper = NULL;
	const char *article;
	bool unique = false;
	bool comma = false;

	/* Solo dar esta información en volcados de personaje si se puede equipar */
	if (terse && !obj_can_wear(obj))
		return false;

	/* Establecer el origen - cuidado con los imitadores */
	if ((obj->origin == ORIGIN_DROP_MIMIC) && (obj->mimicking_m_idx != 0))
		origin = ORIGIN_FLOOR;
	else
		origin = obj->origin;

	/* Nombrar el lugar de origen */
	if (obj->origin_depth)
		strnfmt(loot_spot, sizeof(loot_spot), "a %d pies (nivel %d)",
		        obj->origin_depth * 50, obj->origin_depth);
	else
		my_strcpy(loot_spot, "en la ciudad", sizeof(loot_spot));

	/* Nombrar el monstruo de origen */
	if (obj->origin_race) {
		dropper = obj->origin_race->name;
		if (rf_has(obj->origin_race->flags, RF_UNIQUE)) {
			unique = true;
		}
		if (rf_has(obj->origin_race->flags, RF_NAME_COMMA)) {
			comma = true;
		}
	} else {
		dropper = "monstruo perdido en la historia";
	}
	article = is_a_vowel(dropper[0]) ? "un " : "un ";
	if (unique)
		my_strcpy(name, dropper, sizeof(name));
	else {
		my_strcpy(name, article, sizeof(name));
		my_strcat(name, dropper, sizeof(name));
	}
	if (comma) {
		my_strcat(name, ",", sizeof(name));
	}

	/* Imprimir una descripción apropiada */
	switch (origins[origin].args)
	{
		case -1: return false;
		case 0: textblock_append(tb, "%s", origins[origin].desc); break;
		case 1: textblock_append(tb, origins[origin].desc, loot_spot);
				break;
		case 2:
			textblock_append(tb, origins[origin].desc, name, loot_spot);
			break;
	}

	textblock_append(tb, "\n\n");

	return true;
}

/**
 * Imprime el texto de sabor de un objeto.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param obj es el objeto que estamos describiendo.
 * \param ego indica si estamos describiendo una plantilla de ego (en oposición a
 * un objeto real)
 */
static void describe_flavor_text(textblock *tb, const struct object *obj,
								 bool ego)
{
	/* Mostrar la descripción conocida del artefacto u objeto */
	if (!OPT(player, birth_randarts) && obj->artifact &&
		obj->known->artifact && obj->artifact->text) {
		textblock_append(tb, "%s\n\n", obj->artifact->text);

	} else if (object_flavor_is_aware(obj) || ego) {
		bool did_desc = false;

		if (!ego && obj->kind->text) {
			textblock_append(tb, "%s", obj->kind->text);
			did_desc = true;
		}

		/* Mostrar una descripción adicional de objeto de ego */
		if ((ego || (obj->known->ego != NULL)) && obj->ego->text) {
			if (did_desc) textblock_append(tb, "  ");
			textblock_append(tb, "%s\n\n", obj->ego->text);
		} else if (did_desc) {
			textblock_append(tb, "\n\n");
		}
	}
}

/**
 * Describe las propiedades aleatorias que puede tener un objeto de ego
 */
static bool describe_ego(textblock *tb, const struct ego_item *ego)
{
	bool something = false;

	if (kf_has(ego->kind_flags, KF_RAND_HI_RES)) {
		something = true;
		textblock_append(tb, "Proporciona una resistencia superior aleatoria.  ");
	} else if (kf_has(ego->kind_flags, KF_RAND_SUSTAIN)) {
		something = true;
		textblock_append(tb, "Proporciona una sustentación aleatoria.  ");
	} else if (kf_has(ego->kind_flags, KF_RAND_POWER)) {
		something = true;
		textblock_append(tb, "Proporciona una habilidad aleatoria.  ");
	} else if (kf_has(ego->kind_flags, KF_RAND_RES_POWER)) {
		something = true;
		textblock_append(tb, "Proporciona una habilidad aleatoria o resistencia base.  ");
	}
	if (of_has(ego->flags, OF_NO_FUEL)
			&& of_has(ego->flags_off, OF_TAKES_FUEL)) {
		something = true;
		textblock_append(tb, "Arde eternamente sin combustible.  ");
	}

	return something;
}


/**
 * ------------------------------------------------------------------------
 * Código de salida
 * ------------------------------------------------------------------------ */
/**
 * Información de salida del objeto
 */
static textblock *object_info_out(const struct object *obj, int mode)
{
	bitflag flags[OF_SIZE];
	struct element_info el_info[ELEM_MAX];
	bool something = false;

	bool terse = mode & OINFO_TERSE ? true : false;
	bool subjective = mode & OINFO_SUBJ ? true : false;
	bool ego = mode & OINFO_EGO ? true : false;
	textblock *tb = textblock_new();

	assert(obj->known);

	/* Los objetos no conocidos obtienen descripciones simples */
	if (obj->kind != obj->known->kind) {
		textblock_append(tb, "\n\nNo sabes qué es esto.\n");
		return tb;
	}

	/* Obtener los indicadores del objeto */
	get_known_flags(obj, mode, flags);

	/* Obtener la información de elementos */
	get_known_elements(obj, mode, el_info);

	if (subjective) describe_origin(tb, obj, terse);
	if (!terse) describe_flavor_text(tb, obj, ego);

	if (!object_fully_known(obj) &&	(obj->known->notice & OBJ_NOTICE_ASSESSED) && !tval_is_useable(obj)) {
		textblock_append(tb, "No conoces toda la extensión de los poderes de este objeto.\n");
		something = true;
	}

	if (describe_curses(tb, obj, flags)) something = true;
	if (describe_stats(tb, obj, mode)) something = true;
	if (describe_slays(tb, obj)) something = true;
	if (describe_brands(tb, obj)) something = true;
	if (describe_elements(tb, el_info)) something = true;
	if (describe_protects(tb, flags)) something = true;
	if (describe_ignores(tb, el_info)) something = true;
	if (describe_hates(tb, el_info)) something = true;
	if (describe_sustains(tb, flags)) something = true;
	if (describe_misc_magic(tb, flags)) something = true;
	if (describe_light(tb, obj, mode)) something = true;
	if (describe_book(tb, obj, mode)) something = true;
	if (ego && describe_ego(tb, obj->ego)) something = true;
	if (something) textblock_append(tb, "\n");

	/* Omitir toda la información muy específica cuando estamos dando
	   conocimiento general de ego en lugar de para un objeto individual
	   - las habilidades pueden variar */
	if (!ego) {
		if (describe_effect(tb, obj, terse, subjective)) {
			something = true;
			textblock_append(tb, "\n");
		}

		if (subjective && describe_combat(tb, obj)) {
			something = true;
			textblock_append(tb, "\n");
		}

		if (!terse && subjective && describe_digger(tb, obj)) something = true;
	}

	/* No añadir nada en modo escueto (para volcado de personaje) */
	if (!something && !terse)
		textblock_append(tb, "\n\nEste objeto no parece poseer ninguna habilidad especial.");

	return tb;
}


/**
 * Proporciona información sobre un objeto, incluyendo cómo afectaría al estado
 * actual del jugador.
 *
 * devuelve true si se imprime algo.
 */
textblock *object_info(const struct object *obj, oinfo_detail_t mode)
{
	mode |= OINFO_SUBJ;
	return object_info_out(obj, mode);
}

/**
 * Proporciona información sobre un tipo de objeto de ego
 */
textblock *object_info_ego(struct ego_item *ego)
{
	struct object_kind *kind = NULL;
	struct object obj = OBJECT_NULL, known_obj = OBJECT_NULL;
	textblock *result;

	if (ego->poss_items) {
		size_t i;

		for (i = 0; i < z_info->k_max; i++) {
			kind = &k_info[i];
			if (!kind->name)
				continue;
			if (i == ego->poss_items->kidx)
				break;
		}
	}
	if (!kind) {
		result = textblock_new();
		if (ego->poss_items) {
			textblock_append(result, "Error: el array de tipos de "
				"objetos ya no contiene el primer tipo "
				"que puede tener este ego.");
		} else {
			textblock_append(result,
				"Este ego no aparece en ningún objeto.");
		}
		return result;
	}

	obj.kind = kind;
	obj.tval = kind->tval;
	obj.sval = kind->sval;
	obj.ego = ego;
	ego_apply_magic(&obj, 0);

	object_copy(&known_obj, &obj);
	obj.known = &known_obj;

	result = object_info_out(&obj, OINFO_NONE | OINFO_EGO);
	object_wipe(&known_obj);
	object_wipe(&obj);
	return result;
}



/**
 * Proporciona información sobre un objeto adecuada para escribir en el volcado
 * de personaje - mantenerlo breve.
 */
void object_info_chardump(ang_file *f, const struct object *obj, int indent,
						  int wrap)
{
	textblock *tb = object_info_out(obj, OINFO_TERSE | OINFO_SUBJ);
	textblock_to_file(tb, f, indent, wrap);
	textblock_free(tb);
}


/**
 * Proporciona información de spoiler sobre un objeto.
 *
 * Prácticamente, esto significa que no debemos imprimir nada que dependa del
 * estado actual del jugador, ya que eso no es adecuado para material de spoiler.
 */
void object_info_spoil(ang_file *f, const struct object *obj, int wrap)
{
	textblock *tb = object_info_out(obj, OINFO_SPOIL);
	textblock_to_file(tb, f, 0, wrap);
	textblock_free(tb);
}