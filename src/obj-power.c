/**
 * \file obj-power.c
 * \brief calculation of object power and value
 *
 * Copyright (c) 2001 Chris Carr, Chris Robertson
 * Revised in 2009-11 by Chris Carr, Peter Denison
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
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "init.h"
#include "effects.h"
#include "monster.h"

/**
 * ------------------------------------------------------------------------
 * Object power data and assumptions
 * ------------------------------------------------------------------------ */

/**
 * Define a set of constants for dealing with launchers and ammo:
 * - the assumed average damage of ammo (for rating launchers)
 * (the current values assume normal (non-seeker) ammo enchanted to +9)
 * - the assumed bonus on launchers (for rating ego ammo)
 * - twice the assumed multiplier (for rating any ammo)
 * N.B. Ammo tvals are assumed to be consecutive! We access this array using
 * (obj->tval - TV_SHOT) for ammo, and
 * (obj->sval / 10) for launchers
 */
static struct archery {
	int ammo_tval;
	int ammo_dam;
	int launch_dam;
	int launch_mult;
} archery[] = {
	{TV_SHOT, 10, 9, 4},
	{TV_ARROW, 12, 9, 5},
	{TV_BOLT, 14, 9, 7}
};

/**
 * Set the weightings of flag types:
 * - factor for power increment for multiple flags
 * - additional power bonus for a "full set" of these flags
 * - number of these flags which constitute a "full set"
 */
static struct flag_set {
	int type;
	int factor;
	int bonus;
	int size;
	int count;
	const char *desc;
} flag_sets[] = {
	{ OFT_SUST, 1, 10, 5, 0, "sostenimientos" },
	{ OFT_PROT, 3, 15, 4, 0, "protecciones" },
	{ OFT_MISC, 1, 25, 8, 0, "habilidades varias" }
};


enum {
	T_LRES,
	T_HRES
};

/**
 * Similar data for elements
 */
static struct element_set {
	int type;
	int res_level;
	int factor;
	int bonus;
	int size;
	int count;
	const char *desc;
} element_sets[] = {
	{ T_LRES, 3, 6, INHIBIT_POWER, 4,    0,     "inmunidades" },
	{ T_LRES, 1, 1, 10,            4,    0,     "resistencias bajas" },
	{ T_HRES, 1, 2, 10,            9,    0,     "resistencias altas" },
};

/**
 * Power data for elements
 */
static struct element_powers {
	const char *name;
	int type;
	int ignore_power;
	int vuln_power;
	int res_power;
	int im_power;
} el_powers[] = {
	{ "ácido",			T_LRES,	3,	-6,	5,	38 },
	{ "electricidad",	T_LRES,	1,	-6,	6,	35 },
	{ "fuego",			T_LRES,	3,	-6,	6,	40 },
	{ "frío",			T_LRES,	1,	-6,	6,	37 },
	{ "veneno",			T_HRES,	0,	0,	28,	0 },
	{ "luz",			T_HRES,	0,	0,	6,	0 },
	{ "oscuridad",		T_HRES,	0,	0,	16,	0 },
	{ "sonido",			T_HRES,	0,	0,	14,	0 },
	{ "fragmentos",		T_HRES,	0,	0,	8,	0 },
	{ "nexo",			T_HRES,	0,	0,	15,	0 },
	{ "más allá",		T_HRES,	0,	0,	20,	0 },
	{ "caos",			T_HRES,	0,	0,	20,	0 },
	{ "desencantamiento",	T_HRES,	0,	0,	20,	0 }
};

/**
 * Boost ratings for combinations of ability bonuses
 * We go up to +24 here - anything higher is inhibited
 * N.B. Not all stats count equally towards this total
 */
static int16_t ability_power[25] =
	{0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8,
	12, 16, 20, 24, 30, 36, 42, 48, 56, 64,
	74, 84, 96, 110};

/* Log file declared here for simplicity */
static ang_file *object_log;

/**
 * Log progress info to the object log
 */
static void log_obj(const char *fmt, ...)
{
	va_list ap;

	if (!object_log) return;

	va_start(ap, fmt);
	file_vputf(object_log, fmt, ap);
	va_end(ap);
}

/**
 * ------------------------------------------------------------------------
 * Object power calculations
 * ------------------------------------------------------------------------ */

/**
 * Calculate the multiplier we'll get with a given bow type.
 */
static int bow_multiplier(const struct object *obj)
{
	int mult = 1;

	if (obj->tval != TV_BOW)
		return mult;
	else
		mult = obj->pval;

	log_obj("El multiplicador base para esta arma es %d\n", mult);
	return mult;
}

/**
 * To damage power
 */
static int to_damage_power(const struct object *obj)
{
	int p;

	p = (obj->to_d * DAMAGE_POWER / 2);
	if (p) log_obj("%d de poder por to_dam\n", p);

	/* Add second lot of damage power for non-weapons */
	if ((wield_slot(obj) != slot_by_name(player, "shooting")) &&
		!tval_is_melee_weapon(obj) &&
		!tval_is_ammo(obj)) {
		int q = (obj->to_d * DAMAGE_POWER);
		p += q;
		if (q) log_obj("Añadir %d de no-arma to_dam, total %d\n",
			q, p);
	}
	return p;
}

/**
 * Damage dice power or equivalent
 */
static int damage_dice_power(const struct object *obj)
{
	int dice = 0;

	/* Add damage from dice for any wieldable weapon or ammo */
	if (tval_is_melee_weapon(obj) || tval_is_ammo(obj)) {
		dice = ((obj->dd * (obj->ds + 1) * DAMAGE_POWER) / 4);
		log_obj("Añadir %d de poder por dados de daño, ", dice);
	} else if (wield_slot(obj) != slot_by_name(player, "shooting")) {
		/* Add power boost for nonweapons with combat flags */
		if (obj->brands || obj->slays ||
			(obj->modifiers[OBJ_MOD_BLOWS] > 0) ||
			(obj->modifiers[OBJ_MOD_SHOTS] > 0) ||
			(obj->modifiers[OBJ_MOD_MIGHT] > 0)) {
			dice = (WEAP_DAMAGE * DAMAGE_POWER);
			log_obj("Añadir %d de poder por bonificaciones de combate en no-armas, ",
				dice);
		}
	}
	return dice;
}

/**
 * Add ammo damage for launchers, get multiplier and rescale
 */
static int ammo_damage_power(const struct object *obj, int p)
{
	int q = 0;
	int launcher = -1;

	if (wield_slot(obj) == slot_by_name(player, "shooting")) {
		if (kf_has(obj->kind->kind_flags, KF_SHOOTS_SHOTS))
			launcher = 0;
		else if (kf_has(obj->kind->kind_flags, KF_SHOOTS_ARROWS))
			launcher = 1; 
		else if (kf_has(obj->kind->kind_flags, KF_SHOOTS_BOLTS))
			launcher = 2;

		if (launcher != -1) {
			q = (archery[launcher].ammo_dam * DAMAGE_POWER / 2);
			log_obj("Añadiendo %d de poder por munición, total es %d\n", q,
				p + q);
		}
	}
	return q;
}

/**
 * Add launcher bonus for ego ammo, multiply for launcher and rescale
 */
static int launcher_ammo_damage_power(const struct object *obj, int p)
{
	int ammo_type = 0;

	if (tval_is_ammo(obj)) {
		if (obj->tval == TV_ARROW) ammo_type = 1;
		if (obj->tval == TV_BOLT) ammo_type = 2;
		if (obj->ego)
			p += (archery[ammo_type].launch_dam * DAMAGE_POWER / 2);
		p = p * archery[ammo_type].launch_mult / (2 * MAX_BLOWS);
		log_obj("Después de multiplicar munición y reescalar, el poder"
			" es %d\n", p);
	}
	return p;
}

/**
 * Add power for extra blows
 */
static int extra_blows_power(const struct object *obj, int p)
{
	int q = p;

	if (obj->modifiers[OBJ_MOD_BLOWS] == 0)
		return p;

	if (obj->modifiers[OBJ_MOD_BLOWS] >= INHIBIT_BLOWS) {
		p += INHIBIT_POWER;
		log_obj("INHIBICIÓN - demasiados golpes extra - abandonando\n");
		return p;
	} else {
		p = p * (MAX_BLOWS + obj->modifiers[OBJ_MOD_BLOWS]) / MAX_BLOWS;
		/* Add boost for assumed off-weapon damage */
		p += (NONWEAP_DAMAGE * obj->modifiers[OBJ_MOD_BLOWS]
			  * DAMAGE_POWER / 2);
		log_obj("Añadir %d de poder por golpes extra, total es %d\n",
			p - q, p);
	}
	return p;
}

/**
 * Add power for extra shots - note that we cannot handle negative shots
 */
static int extra_shots_power(const struct object *obj, int p)
{
	if (obj->modifiers[OBJ_MOD_SHOTS] == 0)
		return p;

	if (obj->modifiers[OBJ_MOD_SHOTS] >= INHIBIT_SHOTS) {
		p += INHIBIT_POWER;
		log_obj("INHIBICIÓN - demasiados disparos extra - abandonando\n");
		return p;
	} else if (obj->modifiers[OBJ_MOD_SHOTS] > 0) {
		/* Multiply by effective number of shots */
		int q = obj->modifiers[OBJ_MOD_SHOTS];
		p *= (10 + q);
		p /= 10;
		log_obj("Añadiendo %d%% de poder por disparos extra, total es %d\n",
			10 * q, p);
	}
	return p;
}


/**
 * Add power for extra might
 */
static int extra_might_power(const struct object *obj, int p, int mult)
{
	if (obj->modifiers[OBJ_MOD_MIGHT] >= INHIBIT_MIGHT) {
		p += INHIBIT_POWER;
		log_obj("INHIBICIÓN - demasiado poderío extra - abandonando\n");
		return p;
	} else {
		mult += obj->modifiers[OBJ_MOD_MIGHT];
	}
	log_obj("Multiplicador después de poderío extra es %d\n", mult);
	p *= mult;
	log_obj("Después de multiplicar poder por poderío, total es %d\n", p);
	return p;
}

/**
 * Calculate the rating for a given slay combination
 */
static int32_t slay_power(const struct object *obj, int p, int verbose,
					   int dice_pwr)
{
	int i, q, num_brands = 0, num_slays = 0, num_kills = 0;
	int best_power = 1;

	/* Count the brands and slays */
	if (obj->brands) {
		for (i = 1; i < z_info->brand_max; i++) {
			if (obj->brands[i]) {
				num_brands++;
				if (brands[i].power > best_power)
					best_power = brands[i].power;
			}
		}
	}
	if (obj->slays) {
		for (i = 1; i < z_info->slay_max; i++) {
			if (obj->slays[i]) {
				if (slays[i].multiplier <= 3) {
					num_slays++;
				} else {
					num_kills++;
				}
				if (slays[i].power > best_power)
					best_power = slays[i].power;
			}
		}
	}

	/* If there are no slays or brands return */
	if ((num_slays + num_brands + num_kills) == 0)
		return p;

	/* Write the best power */
	if (verbose) {
		/* Write info about the slay combination and multiplier */
		log_obj("Matanzas y marcas: ");

		if (obj->brands) {
			for (i = 1; i < z_info->brand_max; i++) {
				if (obj->brands[i]) {
					struct brand *b = &brands[i];
					log_obj("%sx%d ", b->name,
						b->multiplier);
				}
			}
		}
		if (obj->slays) {
			for (i = 1; i < z_info->slay_max; i++) {
				if (obj->slays[i]) {
					struct slay *s = &slays[i];
					log_obj("%sx%d ", s->name,
						s->multiplier);
				}
			}
		}
		log_obj("\nmejor poder es : %d\n", best_power);
	}

	q = (dice_pwr * dice_pwr * (best_power - 100)) / 2500;
	p += q;
	log_obj("Añadir %d por poder de matanza, total es %d\n", q, p);

	/* Bonuses for multiple brands and slays */
	if (num_slays > 1) {
		q = (num_slays * num_slays * dice_pwr) / (DAMAGE_POWER * 5);
		p += q;
		log_obj("Añadir %d de poder por matanzas múltiples, total es %d\n", q, p);
	}
	if (num_brands > 1) {
		q = (2 * num_brands * num_brands * dice_pwr) / (DAMAGE_POWER * 5);
		p += q;
		log_obj("Añadir %d de poder por marcas múltiples, total es %d\n",
			q, p);
	}
	if (num_slays && num_brands) {
		q = (num_slays * num_brands * dice_pwr) / (DAMAGE_POWER * 5);
		p += q;
		log_obj("Añadir %d de poder por matanza y marca, total es %d\n", q, p);
	}
	if (num_kills > 1) {
		q = (3 * num_kills * num_kills * dice_pwr) / (DAMAGE_POWER * 5);
		p += q;
		log_obj("Añadir %d de poder por muertes múltiples, total es %d\n", q, p);
	}
	if (num_slays == 8) {
		p += 10;
		log_obj("Añadir 10 de poder por conjunto completo de matanzas, total es %d\n", p);
	}
	if (num_brands == 5) {
		p += 20;
		log_obj("Añadir 20 de poder por conjunto completo de marcas, total"
			" es %d\n", p);
	}
	if (num_kills == 3) {
		p += 20;
		log_obj("Añadir 20 de poder por conjunto completo de muertes, total es %d\n", p);
	}

	return p;
}

/**
 * Melee weapons assume MAX_BLOWS per turn, so we must divide by MAX_BLOWS
 * to get equal ratings for launchers.
 */
static int rescale_bow_power(const struct object *obj, int p)
{
	if (wield_slot(obj) == slot_by_name(player, "shooting")) {
		p /= MAX_BLOWS;
		log_obj("Reescalando poder de arco, total es %d\n", p);
	}
	return p;
}

/**
 * Add power for +to_hit
 */
static int to_hit_power(const struct object *obj, int p)
{
	int q = (obj->to_h * TO_HIT_POWER / 2);
	p += q;
	if (p) 
		log_obj("Añadir %d de poder por to hit, total es %d\n", q, p);
	return p;
}

/**
 * Add power for base AC and adjust for weight
 */
static int ac_power(const struct object *obj, int p)
{
	int q = 0;

	if (obj->ac) {
		int16_t weight = object_weight_one(obj);

		p += BASE_ARMOUR_POWER;
		q += (obj->ac * BASE_AC_POWER / 2);
		log_obj("Añadiendo %d de poder por valor de CA base\n", q);

		/* Add power for AC per unit weight */
		if (weight > 0) {
			int i = 750 * (obj->ac + obj->to_a) / weight;

			/* Avoid overpricing Elven Cloaks */
			if (i > 450) i = 450;

			q *= i;
			q /= 100;

			/* Weightless (ethereal) armour items get fixed boost */
		} else
			q *= 5;
		p += q;
		log_obj("Añadir %d de poder por CA por unidad de peso, ahora %d\n", q, p);
	}
	return p;
}


/**
 * Add power for +to_ac
 */
static int to_ac_power(const struct object *obj, int p)
{
	int q;

	if (obj->to_a == 0) return p;

	q = (obj->to_a * TO_AC_POWER / 2);
	p += q;
	log_obj("Añadir %d de poder por to_ac de %d, total es %d\n", q, obj->to_a, p);
	if (obj->to_a > HIGH_TO_AC) {
		q = ((obj->to_a - (HIGH_TO_AC - 1)) * TO_AC_POWER);
		p += q;
		log_obj("Añadir %d de poder por to_ac alto, total es %d\n", q, p);
	}
	if (obj->to_a > VERYHIGH_TO_AC) {
		q = ((obj->to_a - (VERYHIGH_TO_AC -1)) * TO_AC_POWER * 2);
		p += q;
		log_obj("Añadir %d de poder por to_ac muy alto, total es %d\n",
			q, p);
	}
	if (obj->to_a >= INHIBIT_AC) {
		p += INHIBIT_POWER;
		log_obj("INHIBICIÓN: Bonificación de CA demasiado alta\n");
	}
	return p;
}

/**
 * Add base power for jewelry
 */
static int jewelry_power(const struct object *obj, int p)
{
	if (tval_is_jewelry(obj)) {
		p += BASE_JEWELRY_POWER;
		log_obj("Añadiendo %d de poder para joyería, total es %d\n",
			BASE_JEWELRY_POWER, p);
	}
	return p;
}

/**
 * Add power for modifiers
 */
static int modifier_power(const struct object *obj, int p)
{
	int i, k, extra_stat_bonus = 0, q;

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		/* Get the modifier details */
		struct obj_property *mod = lookup_obj_property(OBJ_PROPERTY_MOD, i);
		assert(mod);

		k = obj->modifiers[i];
		extra_stat_bonus += (k * mod->mult);

		if (mod->power) {
			q = (k * mod->power * mod->type_mult[obj->tval]);
			p += q;
			if (q) log_obj("Añadir %d de poder por %d %s, total es %d\n",
				q, k, mod->name, p);
		}
	}

	/* Add extra power term if there are a lot of ability bonuses */
	if (extra_stat_bonus > 249) {
		log_obj("Inhibición - Bonificación de habilidad total de %d es demasiado alta\n",
			extra_stat_bonus);
		p += INHIBIT_POWER;
	} else if (extra_stat_bonus > 0) {
		q = ability_power[extra_stat_bonus / 10];
		if (!q) return p;
		p += q;
		log_obj("Añadir %d de poder por total de modificador de %d, total es %d\n",
			q, extra_stat_bonus, p);
	}
	return p;
}

/**
 * Add power for non-derived flags (derived flags have flag_power 0)
 */
static int flags_power(const struct object *obj, int p, int verbose,
					   ang_file *log_file)
{
	size_t i, j;
	int q;
	bitflag flags[OF_SIZE];

	/* Extract the flags */
	object_flags(obj, flags);

	/* Zero the flag counts */
	for (i = 0; i < N_ELEMENTS(flag_sets); i++)
		flag_sets[i].count = 0;

	for (i = of_next(flags, FLAG_START); i != FLAG_END; 
		 i = of_next(flags, i + 1)) {
		/* Get the flag details */
		struct obj_property *flag = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		assert(flag);

		if (flag->power) {
			q = (flag->power * flag->type_mult[obj->tval]);
			p += q;
			log_obj("Añadir %d de poder por %s, total es %d\n",
				q, flag->name, p);
		}

		/* Track combinations of flag types */
		for (j = 0; j < N_ELEMENTS(flag_sets); j++)
			if (flag_sets[j].type == flag->subtype)
				flag_sets[j].count++;
	}

	/* Add extra power for multiple flags of the same type */
	for (i = 0; i < N_ELEMENTS(flag_sets); i++) {
		if (flag_sets[i].count > 1) {
			q = (flag_sets[i].factor * flag_sets[i].count * flag_sets[i].count);
			p += q;
			log_obj("Añadir %d de poder por múltiples %s, total es %d\n",
				q, flag_sets[i].desc, p);
		}

		/* Add bonus if item has a full set of these flags */
		if (flag_sets[i].count == flag_sets[i].size) {
			q = flag_sets[i].bonus;
			p += q;
			log_obj("Añadir %d de poder por conjunto completo de %s,"
				" total es %d\n", q, flag_sets[i].desc, p);
		}
	}

	return p;
}

/**
 * Add power for elemental properties
 */
static int element_power(const struct object *obj, int p)
{
	size_t i, j;
	int q;

	/* Zero the set counts */
	for (i = 0; i < N_ELEMENTS(element_sets); i++)
		element_sets[i].count = 0;

	/* Analyse each element for ignore, vulnerability, resistance or immunity */
	for (i = 0; i < N_ELEMENTS(el_powers); i++) {
		if (obj->el_info[i].flags & EL_INFO_IGNORE) {
			if (el_powers[i].ignore_power != 0) {
				q = (el_powers[i].ignore_power);
				p += q;
				log_obj("Añadir %d de poder por ignorar %s, total"
					" es %d\n", q, el_powers[i].name, p);
			}
		}

		if (obj->el_info[i].res_level == -1) {
			if (el_powers[i].vuln_power != 0) {
				q = (el_powers[i].vuln_power);
				p += q;
				log_obj("Añadir %d de poder por vulnerabilidad a"
					" %s, total es %d\n", q,
					el_powers[i].name, p);
			}
		} else if (obj->el_info[i].res_level == 1) {
			if (el_powers[i].res_power != 0) {
				q = (el_powers[i].res_power);
				p += q;
				log_obj("Añadir %d de poder por resistencia a"
					" %s, total es %d\n", q,
					el_powers[i].name, p);
			}
		} else if (obj->el_info[i].res_level == 3) {
			if (el_powers[i].im_power != 0) {
				q = (el_powers[i].im_power + el_powers[i].res_power);
				p += q;
				log_obj("Añadir %d de poder por inmunidad a"
					" %s, total es %d\n", q,
					el_powers[i].name, p);
			}
		}

		/* Track combinations of element properties */
		for (j = 0; j < N_ELEMENTS(element_sets); j++)
			if ((element_sets[j].type == el_powers[i].type) &&
				(element_sets[j].res_level <= obj->el_info[i].res_level))
				element_sets[j].count++;
	}

	/* Add extra power for multiple flags of the same type */
	for (i = 0; i < N_ELEMENTS(element_sets); i++) {
		if (element_sets[i].count > 1) {
			q = (element_sets[i].factor * element_sets[i].count * element_sets[i].count);
			p += q;
			log_obj("Añadir %d de poder por múltiples %s, total es %d\n",
				q, element_sets[i].desc, p);
		}

		/* Add bonus if item has a full set of these flags */
		if (element_sets[i].count == element_sets[i].size) {
			q = element_sets[i].bonus;
			p += q;
			log_obj("Añadir %d de poder por conjunto completo de %s,"
				" total es %d\n", q, element_sets[i].desc, p);
		}
	}

	return p;
}

/**
 * Add power for effect
 */
static int effects_power(const struct object *obj, int p)
{
	int q = 0;

	if (obj->activation) {
		q = obj->activation->power;
	} else if (obj->kind->power) {
		q = obj->kind->power;
	}

	if (q) {
		p += q;
		log_obj("Añadir %d de poder por activación de objeto, total es %d\n",
			q, p);
	}
	return p;
}

/**
 * Add power for curses
 */
static int curse_power(const struct object *obj, int p, int verbose,
					   ang_file *log_file)
{
	int i, q = 0;

	if (obj->curses) {
		/*
		 * Treat weight-affecting curses differently since those may
		 * not be modeled well with power(base object)
		 * + power(curse 1) + ....  Could treat all curses the way
		 * weight-affecting curses are, but separating them out keeps
		 * the results the same as the 4.2.5 calculations when the
		 * object does not have weight-affecting curses.
		 */
		bool weight_affecting = false;

		/* Get the curse object power unless it affects the weight. */
		for (i = 1; i < z_info->curse_max; i++) {
			int curse_power;

			if (!obj->curses[i].power) {
				continue;
			}
			if (of_has(curses[i].obj->flags, OF_MULTIPLY_WEIGHT)) {
				if (curses[i].obj->weight != 100) {
					weight_affecting = true;
					continue;
				}
			} else {
				if (curses[i].obj->weight != 0) {
					weight_affecting = true;
					continue;
				}
			}

			log_obj("Calculando poder de maldición %s...\n",
				curses[i].name);
			curse_power =
				object_power(curses[i].obj, verbose, log_file);
			curse_power -= obj->curses[i].power / 10;
			log_obj("Ajustado por fuerza de maldición, %d para"
				" poder de maldición %s\n", curse_power,
					curses[i].name);
			q += curse_power;
		}

		if (weight_affecting) {
			/*
			 * Get the power for the object with all the curses'
			 * attributes combined with those for the base object.
			 */
			struct object obj_local;
			int p_all_curse;

			memset(&obj_local, 0, sizeof(obj_local));
			object_copy(&obj_local, obj);
			apply_curse_attributes(-1, &obj_local);
			/*
			 * Clear curses since all included by
			 * apply_curse_attributes().
			 */
			mem_free(obj_local.curses);
			obj_local.curses = NULL;
			p_all_curse = object_power(&obj_local, verbose,
				log_file);
			mem_free(obj_local.brands);
			mem_free(obj_local.slays);
			log_obj("El poder es %d con todas las maldiciones aplicadas\n",
				p_all_curse);

			/*
			 * Now get the power for the object which has one of
			 * the active curses removed.  The difference between
			 * that power and p_all_curse is the power of the
			 * curse.  Skip the non-weight-affecting curses handled
			 * in the first pass.
			 */
			for (i = 1; i < z_info->curse_max; ++i) {
				int p_all_but_i, p_curse;

				if (!obj->curses[i].power) {
					continue;
				}
				if (of_has(curses[i].obj->flags,
						OF_MULTIPLY_WEIGHT)) {
					if (curses[i].obj->weight == 100) {
						continue;
					}
				} else {
					if (curses[i].obj->weight == 0) {
						continue;
					}
				}

				memset(&obj_local, 0, sizeof(obj_local));
				object_copy(&obj_local, obj);
				apply_curse_attributes(i, &obj_local);
				/*
				 * Clear curses since all of interest included
				 * by apply_curse_attributes().
				 */
				mem_free(obj_local.curses);
				obj_local.curses = NULL;
				p_all_but_i = object_power(&obj_local, verbose,
					log_file);
				mem_free(obj_local.brands);
				mem_free(obj_local.slays);
				log_obj("El poder es %d con todas excepto la maldición %s"
					" aplicada\n", p_all_but_i,
					curses[i].name);

				/*
				 * The effect of this curse on the total power
				 * is the difference between p_all_curse and
				 * p_all_but_i.  If that difference is
				 * is not negative, use it as is:  at least
				 * according to the power calculation, it does
				 * not make sense to remove that curse so the
				 * curse's resistance to removal does not
				 * matter.
				 */
				p_curse = sub_guardi(p_all_curse, p_all_but_i);
				if (p_curse < 0) {
					/*
					 * The curse reduces the object's
					 * power: scale the contribution to
					 * power attributed to the curse by
					 * a factor that increases with the
					 * curse's resistance to removal.
					 */
					int resistance = MAX(20, MIN(100,
						obj->curses[i].power));

					p_curse = (p_curse
						>= INT_MIN / resistance) ?
						p_curse * resistance : INT_MIN;
					p_curse /= 100;
				}
				log_obj("El poder ajustado es %d para la maldición %s\n",
					p_curse, curses[i].name);

				q = add_guardi(q, p_curse);
			}
		}
	}

	if (q != 0) {
		p += q;
		log_obj("Total de %d de poder añadido por maldiciones, total es %d\n",
			q, p);
	}
	return p;
}


/**
 * Adjust power for a non-standard weight of the object.
 *
 * This currently only considers changes to the weight from curses.  It could
 * instead use obj->kind->weight as the standard weight but that would:
 *     1) Cause the be power to different than the 4.2.5 calculations when there
 *        are no weight-affecting curses present but obj->weight differs from
 *        obj->kind->weight.
 *     2) In the presense of weight-affecting curses, one would have to guard
 *        against performing these calculations on curse objects (i.e.
 *        obj->kind->tval == curse_object_kind->tval
 *        && obj->kind->sval == curse_object_kind->sval) since the weights
 *        on those are adjustments to the base weight of the object the curse
 *        affects and differ from obj->kind->weight.
 */
static int nonstandard_weight_power(const struct object *obj, int p)
{
	int16_t std_weight = MAX(obj->weight, 0);
	int16_t nonstd_weight = object_weight_one(obj);
	bitflag flags[OF_SIZE];
	int adj;

	assert(nonstd_weight >= 0);
	if (std_weight == nonstd_weight) {
		/* No change to the weight so no change to the power. */
		return p;
	}

	/* Start with no adjustment. */
	adj = 0;

	/*
	 * To handle THROWING below, Merge flags from the base object and any
	 * curses.
	 */
	of_copy(flags, obj->flags);
	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				of_union(flags, curses[i].obj->flags);
			}
		}
	}

	/*
	 * ac_power() accounted for the weight when the object provides a base
	 * amount of armor so do not adjust the power for those objects here.
	 * For objects which do not provide a base amount of armor, adjust
	 * the power under the assumption that lighter than normal is beneficial
	 * (more room under the weight cap for other stuff) and heavier than
	 * normal is harmful.
	 */
	if (!obj->ac) {
		int adj_wc = (std_weight - nonstd_weight)
			/ WGT_POWER_DEN_NOBASEAC;

		if (adj_wc >= 0) {
			adj_wc = (adj_wc < INT_MAX / WGT_POWER_NUM_NOBASEAC) ?
				adj_wc * WGT_POWER_NUM_NOBASEAC : INT_MAX;
		} else {
			adj_wc = (adj_wc > INT_MIN / WGT_POWER_NUM_NOBASEAC) ?
				adj_wc * WGT_POWER_NUM_NOBASEAC : INT_MIN;
		}
		log_obj("Añadir %d de poder por peso no estándar de objeto que no"
			" afecta armadura base.\n", adj_wc);
		adj = add_guardi(adj, adj_wc);
	}

	/*
	 * Objects with the THROWING flag, either directly or via a curse, can
	 * increase damage with increasing weight.  Adjust the power for that.
	 */
	if (of_has(flags, OF_THROWING)) {
		int adj_th = nonstd_weight / WGT_POWER_DEN_THROW
			- std_weight / WGT_POWER_DEN_THROW;

		if (adj_th >= 0) {
			adj_th = (adj_th < INT_MAX / WGT_POWER_NUM_THROW) ?
				adj_th * WGT_POWER_NUM_THROW : INT_MAX;
		} else {
			adj_th = (adj_th > INT_MIN / WGT_POWER_NUM_THROW) ?
				adj_th * WGT_POWER_NUM_THROW : INT_MIN;
		}
		log_obj("Añadir %d de poder por peso no estándar de objeto bueno"
			" para lanzar.\n", adj_th);
		adj = add_guardi(adj, adj_th);
	}

	/*
	 * Weight also affects number of blows (melee weapons only),
	 * heavy wield status (melee weapon or launcher; strength-dependent
	 * and normally only relevant for quite heavy objects), criticals
	 * (for melee, launched missile, or thrown missile but only in non-O
	 * combat calculations; increasing weight can increase the chance of
	 * a critical and the amount of damage from the critical if it occurs),
	 * and shield bashes (more weight is better; only relevant for some
	 * classes).  None of those are accounted for here.
	 */

	if (adj) {
		p = add_guardi(p, adj);
		log_obj("Añadir %d de poder combinado por peso no estándar; "
			"total es %p\n", adj, p);
	}

	return p;
}


/**
 * Evaluate the object's overall power level.
 */
int32_t object_power(const struct object* obj, bool verbose, ang_file *log_file)
{
	int32_t p = 0, dice_pwr = 0;
	int mult;

	/* Set the log file */
	object_log = log_file;

	/* Get all the attack power */
	p = to_damage_power(obj);
	dice_pwr = damage_dice_power(obj);
	p += dice_pwr;
	if (dice_pwr) log_obj("total es %d\n", p);
	p += ammo_damage_power(obj, p);
	mult = bow_multiplier(obj);
	p = launcher_ammo_damage_power(obj, p);
	p = extra_blows_power(obj, p);
	if (p > INHIBIT_POWER) return p;
	p = extra_shots_power(obj, p);
	if (p > INHIBIT_POWER) return p;
	p = extra_might_power(obj, p, mult);
	if (p > INHIBIT_POWER) return p;
	p = slay_power(obj, p, verbose, dice_pwr);
	p = rescale_bow_power(obj, p);
	p = to_hit_power(obj, p);

	/* Armour class power */
	p = ac_power(obj, p);
	p = to_ac_power(obj, p);

	/* Bonus for jewelry */
	p = jewelry_power(obj, p);

	/* Other object properties */
	p = modifier_power(obj, p);
	p = flags_power(obj, p, verbose, object_log);
	p = element_power(obj, p);
	p = effects_power(obj, p);
	p = curse_power(obj, p, verbose, object_log);
	p = nonstandard_weight_power(obj, p);

	log_obj("PODER FINAL ES %d\n", p);

	return p;
}


/**
 * ------------------------------------------------------------------------
 * Object pricing
 * ------------------------------------------------------------------------ */
/**
 * Return the "value" of an "unknown" item
 * Make a guess at the value of non-aware items
 */
static int object_value_base(const struct object *obj)
{
	/* Use template cost for aware objects */
	if (object_flavor_is_aware(obj))
		return obj->kind->cost;

	/* Analyze the type */
	switch (obj->tval)
	{
		case TV_FOOD:
		case TV_MUSHROOM:
			return 5;
		case TV_POTION:
		case TV_SCROLL:
			return 20;
		case TV_RING:
		case TV_AMULET:
			return 45;
		case TV_WAND:
			return 50;
		case TV_STAFF:
			return 70;
		case TV_ROD:
			return 90;
	}

	return 0;
}


/**
 * Return the real price of a known (or partly known) item.
 *
 * Wand and staffs get cost for each charge.
 *
 * Wearable items (weapons, launchers, jewelry, lights, armour) and ammo
 * are priced according to their power rating. All ammo, and normal (non-ego)
 * torches are scaled down by AMMO_RESCALER to reflect their impermanence.
 */
int object_value_real(const struct object *obj, int qty)
{
	int value, total_value;

	int power;
	/*
	 * This is the quadratic coefficient for power in the expression for
	 * the real value.  Must be non-negative.
	 */
	int a = 1;
	/*
	 * This is the linear coefficient for power in the expression for
	 * the real value.  Must be non-negative.
	 */
	int b = 5;

	/* Wearables and ammo have prices that vary by individual item properties */
	if (tval_has_variable_power(obj)) {
#ifdef PRICE_DEBUG
		char buf[1024];
		ang_file *log_file = NULL;
		static file_mode pricing_mode = MODE_WRITE;

		/* Logging */
		path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "pricing.log");
		log_file = file_open(buf, pricing_mode, FTYPE_TEXT);
		if (!log_file) {
			msg("Error - no se puede abrir pricing.log para escribir.");
			exit(1);
		}
		pricing_mode = MODE_APPEND;

		file_putf(log_file, "objeto es %s\n", obj->kind->name);

		power = object_power(obj, true, log_file);
#else /* PRICE_DEBUG */
		power = object_power(obj, false, NULL);
#endif /* PRICE_DEBUG */
		/* Protect against overflow. */
		if (power > 0) {
			if (a > 0) {
				if (power <= (INT_MAX / power - b) / a) {
					value = power * (power * a + b);
				} else {
					value = INT_MAX;
#ifdef PRICE_DEBUG
					file_put(log_file, "Valor limitado para evitar desbordamiento.\n");
#endif
				}
			} else if (b > 0) {
				if (power <= INT_MAX / b) {
					value = power * b;
				} else {
					value = INT_MAX;
#ifdef PRICE_DEBUG
					file_put(log_file, "Valor limitado para evitar desbordamiento.\n");
#endif
				}
			} else {
				value = 0;
			}
		} else if (power < 0) {
			if (a > 0) {
				if (power > INT_MIN && power >= (INT_MIN / (-power) + b) / a) {
					value = -power * (power * a - b);
				} else {
					value = INT_MIN;
#ifdef PRICE_DEBUG
					file_put(log_file, "Valor limitado para evitar desbordamiento.\n");
#endif
				}
			} else if (b > 0) {
				if (power >= INT_MIN / b) {
					value = power * b;
				} else {
					value = INT_MIN;
#ifdef PRICE_DEBUG
					file_put(log_file, "Valor limitado para evitar desbordamiento.\n");
#endif
				}
			} else {
				value = 0;
			}
		} else {
			value = 0;
		}

		/* Rescale for expendables */
		if ((tval_is_light(obj) && of_has(obj->flags, OF_BURNS_OUT)
			 && !obj->ego) || tval_is_ammo(obj)) {
			value = value / AMMO_RESCALER;
		}

		/* Round up to make sure things like cloaks are not worthless */
		if (value == 0) {
			value = 1;
		}

#ifdef PRICE_DEBUG
		/* More logging */
		file_putf(log_file, "a es %d y b es %d\n", a, b);
		file_putf(log_file, "valor es %d\n", value);

		if (!file_close(log_file)) {
			msg("Error - no se puede cerrar el archivo pricing.log.");
			exit(1);
		}
#endif /* PRICE_DEBUG */

		/* Get the total value */
		total_value = value * qty;
		if (total_value < 0) total_value = 0;
	} else {

		/* Worthless items */
		if (!obj->kind->cost) return (0L);

		/* Base cost */
		value = obj->kind->cost;

		/* Analyze the item type and quantity */
		if (tval_can_have_charges(obj)) {
			int charges;

			total_value = value * qty;

			/* Calculate number of charges, rounded up */
			charges = obj->pval * qty / obj->number;
			if ((obj->pval * qty) % obj->number != 0)
				charges++;

			/* Pay extra for charges, depending on standard number of charges */
			total_value += value * charges / 20;
		} else {
			total_value = value * qty;
		}

		/* No negative value */
		if (total_value < 0) total_value = 0;
	}

	/* Return the value */
	return (total_value);
}


/**
 * Return the price of an item including plusses (and charges).
 *
 * This function returns the "value" of the given item (qty one).
 *
 * Never notice unknown bonuses or properties, including curses,
 * since that would give the player information they did not have.
 */
int object_value(const struct object *obj, int qty)
{
	int value;

	/* Variable power items are assessed by what is known about them */
	if (tval_has_variable_power(obj) && obj->known) {
		value = object_value_real(obj->known, qty);
	} else if (tval_can_have_flavor_k(obj->kind) &&
			   object_flavor_is_aware(obj)) {
		value = object_value_real(obj, qty);
	} else {
		/* Unknown constant-price items just get a base value */
		value = object_value_base(obj) * qty;
	}

	/* Return the final value */
	return (value);
}