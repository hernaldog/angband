/**
 * \archivo obj-power.c
 * \brief cálculo del poder y valor del objeto
 *
 * Copyright (c) 2001 Chris Carr, Chris Robertson
 * Revisado en 2009-11 por Chris Carr, Peter Denison
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
 * Datos de poder de objeto y suposiciones
 * ------------------------------------------------------------------------ */

/**
 * Define un conjunto de constantes para tratar con lanzadores y munición:
 * - el daño promedio asumido de la munición (para calificar lanzadores)
 * (los valores actuales asumen munición normal (no buscadora) encantada a +9)
 * - la bonificación asumida en lanzadores (para calificar munición de ego)
 * - el doble del multiplicador asumido (para calificar cualquier munición)
 * N.B. Se asume que los tvals de munición son consecutivos. Accedemos a este arreglo usando
 * (obj->tval - TV_SHOT) para munición, y
 * (obj->sval / 10) para lanzadores
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
 * Establece las ponderaciones de los tipos de banderas:
 * - factor para el incremento de poder por múltiples banderas
 * - bonificación de poder adicional por un "conjunto completo" de estas banderas
 * - número de estas banderas que constituyen un "conjunto completo"
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
 * Datos similares para elementos
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
 * Datos de poder para elementos
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
 * Valoraciones de mejora para combinaciones de bonificaciones de habilidad
 * Llegamos hasta +24 aquí; cualquier valor más alto se inhibe
 * N.B. No todas las estadísticas cuentan igual para este total
 */
static int16_t ability_power[25] =
	{0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8,
	12, 16, 20, 24, 30, 36, 42, 48, 56, 64,
	74, 84, 96, 110};

/* Archivo de registro declarado aquí por simplicidad */
static ang_file *object_log;

/**
 * Registrar información de progreso en el registro de objetos
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
 * Cálculos de poder de objeto
 * ------------------------------------------------------------------------ */

/**
 * Calcula el multiplicador que obtendremos con un tipo de arco dado.
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
 * Poder por daño adicional
 */
static int to_damage_power(const struct object *obj)
{
	int p;

	p = (obj->to_d * DAMAGE_POWER / 2);
	if (p) log_obj("%d de poder por to_dam\n", p);

	/* Añadir una segunda cantidad de poder de daño para no-armas */
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
 * Poder de los dados de daño o equivalente
 */
static int damage_dice_power(const struct object *obj)
{
	int dice = 0;

	/* Añadir daño de dados para cualquier arma empuñable o munición */
	if (tval_is_melee_weapon(obj) || tval_is_ammo(obj)) {
		dice = ((obj->dd * (obj->ds + 1) * DAMAGE_POWER) / 4);
		log_obj("Añadir %d de poder por dados de daño, ", dice);
	} else if (wield_slot(obj) != slot_by_name(player, "shooting")) {
		/* Añadir aumento de poder para no-armas con banderas de combate */
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
 * Añadir daño de munición para lanzadores, obtener multiplicador y reescalar
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
 * Añadir bonificación de lanzador para munición de ego, multiplicar por lanzador y reescalar
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
 * Añadir poder por golpes extra
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
		/* Añadir aumento por daño fuera del arma asumido */
		p += (NONWEAP_DAMAGE * obj->modifiers[OBJ_MOD_BLOWS]
			  * DAMAGE_POWER / 2);
		log_obj("Añadir %d de poder por golpes extra, total es %d\n",
			p - q, p);
	}
	return p;
}

/**
 * Añadir poder por disparos extra - nota que no podemos manejar disparos negativos
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
		/* Multiplicar por el número efectivo de disparos */
		int q = obj->modifiers[OBJ_MOD_SHOTS];
		p *= (10 + q);
		p /= 10;
		log_obj("Añadiendo %d%% de poder por disparos extra, total es %d\n",
			10 * q, p);
	}
	return p;
}


/**
 * Añadir poder por poderío extra
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
 * Calcular la calificación para una combinación de matanzas dada
 */
static int32_t slay_power(const struct object *obj, int p, int verbose,
					   int dice_pwr)
{
	int i, q, num_brands = 0, num_slays = 0, num_kills = 0;
	int best_power = 1;

	/* Contar las marcas y matanzas */
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

	/* Si no hay matanzas ni marcas, regresar */
	if ((num_slays + num_brands + num_kills) == 0)
		return p;

	/* Escribir el mejor poder */
	if (verbose) {
		/* Escribir información sobre la combinación de matanza y multiplicador */
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

	/* Bonificaciones por múltiples marcas y matanzas */
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
 * Las armas cuerpo a cuerpo asumen MAX_BLOWS por turno, por lo que debemos dividir por MAX_BLOWS
 * para obtener calificaciones iguales para los lanzadores.
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
 * Añadir poder por +to_hit
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
 * Añadir poder por CA base y ajustar por peso
 */
static int ac_power(const struct object *obj, int p)
{
	int q = 0;

	if (obj->ac) {
		int16_t weight = object_weight_one(obj);

		p += BASE_ARMOUR_POWER;
		q += (obj->ac * BASE_AC_POWER / 2);
		log_obj("Añadiendo %d de poder por valor de CA base\n", q);

		/* Añadir poder por CA por unidad de peso */
		if (weight > 0) {
			int i = 750 * (obj->ac + obj->to_a) / weight;

			/* Evitar sobrevalorar Capas Élficas */
			if (i > 450) i = 450;

			q *= i;
			q /= 100;

			/* Los objetos sin peso (etéreos) obtienen aumento fijo */
		} else
			q *= 5;
		p += q;
		log_obj("Añadir %d de poder por CA por unidad de peso, ahora %d\n", q, p);
	}
	return p;
}


/**
 * Añadir poder por +to_ac
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
 * Añadir poder base para joyería
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
 * Añadir poder por modificadores
 */
static int modifier_power(const struct object *obj, int p)
{
	int i, k, extra_stat_bonus = 0, q;

	for (i = 0; i < OBJ_MOD_MAX; i++) {
		/* Obtener los detalles del modificador */
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

	/* Añadir término de poder extra si hay muchas bonificaciones de habilidad */
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
 * Añadir poder por banderas no derivadas (las banderas derivadas tienen flag_power 0)
 */
static int flags_power(const struct object *obj, int p, int verbose,
					   ang_file *log_file)
{
	size_t i, j;
	int q;
	bitflag flags[OF_SIZE];

	/* Extraer las banderas */
	object_flags(obj, flags);

	/* Poner a cero los contadores de banderas */
	for (i = 0; i < N_ELEMENTS(flag_sets); i++)
		flag_sets[i].count = 0;

	for (i = of_next(flags, FLAG_START); i != FLAG_END; 
		 i = of_next(flags, i + 1)) {
		/* Obtener los detalles de la bandera */
		struct obj_property *flag = lookup_obj_property(OBJ_PROPERTY_FLAG, i);
		assert(flag);

		if (flag->power) {
			q = (flag->power * flag->type_mult[obj->tval]);
			p += q;
			log_obj("Añadir %d de poder por %s, total es %d\n",
				q, flag->name, p);
		}

		/* Rastrear combinaciones de tipos de banderas */
		for (j = 0; j < N_ELEMENTS(flag_sets); j++)
			if (flag_sets[j].type == flag->subtype)
				flag_sets[j].count++;
	}

	/* Añadir poder extra por múltiples banderas del mismo tipo */
	for (i = 0; i < N_ELEMENTS(flag_sets); i++) {
		if (flag_sets[i].count > 1) {
			q = (flag_sets[i].factor * flag_sets[i].count * flag_sets[i].count);
			p += q;
			log_obj("Añadir %d de poder por múltiples %s, total es %d\n",
				q, flag_sets[i].desc, p);
		}

		/* Añadir bonificación si el objeto tiene un conjunto completo de estas banderas */
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
 * Añadir poder por propiedades elementales
 */
static int element_power(const struct object *obj, int p)
{
	size_t i, j;
	int q;

	/* Poner a cero los contadores de conjuntos */
	for (i = 0; i < N_ELEMENTS(element_sets); i++)
		element_sets[i].count = 0;

	/* Analizar cada elemento para ignorar, vulnerabilidad, resistencia o inmunidad */
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

		/* Rastrear combinaciones de propiedades elementales */
		for (j = 0; j < N_ELEMENTS(element_sets); j++)
			if ((element_sets[j].type == el_powers[i].type) &&
				(element_sets[j].res_level <= obj->el_info[i].res_level))
				element_sets[j].count++;
	}

	/* Añadir poder extra por múltiples banderas del mismo tipo */
	for (i = 0; i < N_ELEMENTS(element_sets); i++) {
		if (element_sets[i].count > 1) {
			q = (element_sets[i].factor * element_sets[i].count * element_sets[i].count);
			p += q;
			log_obj("Añadir %d de poder por múltiples %s, total es %d\n",
				q, element_sets[i].desc, p);
		}

		/* Añadir bonificación si el objeto tiene un conjunto completo de estas banderas */
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
 * Añadir poder por efecto
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
 * Añadir poder por maldiciones
 */
static int curse_power(const struct object *obj, int p, int verbose,
					   ang_file *log_file)
{
	int i, q = 0;

	if (obj->curses) {
		/*
		 * Tratar las maldiciones que afectan el peso de manera diferente ya que pueden
		 * no modelarse bien con poder(objeto base)
		 * + poder(maldición 1) + .... Podríamos tratar todas las maldiciones de la manera
		 * en que se tratan las que afectan el peso, pero separarlas mantiene
		 * los resultados iguales que los cálculos de 4.2.5 cuando el
		 * objeto no tiene maldiciones que afectan el peso.
		 */
		bool weight_affecting = false;

		/* Obtener el poder del objeto de maldición a menos que afecte el peso. */
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
			 * Obtener el poder para el objeto con todos los atributos de las maldiciones
			 * combinados con los del objeto base.
			 */
			struct object obj_local;
			int p_all_curse;

			memset(&obj_local, 0, sizeof(obj_local));
			object_copy(&obj_local, obj);
			apply_curse_attributes(-1, &obj_local);
			/*
			 * Limpiar maldiciones ya que todas están incluidas por
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
			 * Ahora obtener el poder para el objeto que tiene una de las
			 * maldiciones activas eliminadas. La diferencia entre
			 * ese poder y p_all_curse es el poder de la
			 * maldición. Omitir las maldiciones que no afectan el peso manejadas
			 * en la primera pasada.
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
				 * Limpiar maldiciones ya que todas las de interés están incluidas
				 * por apply_curse_attributes().
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
				 * El efecto de esta maldición en el poder total
				 * es la diferencia entre p_all_curse y
				 * p_all_but_i. Si esa diferencia no es negativa,
				 * usarla tal cual: al menos según el cálculo de poder,
				 * no tiene sentido eliminar esa maldición, por lo que la
				 * resistencia de la maldición a la eliminación no
				 * importa.
				 */
				p_curse = sub_guardi(p_all_curse, p_all_but_i);
				if (p_curse < 0) {
					/*
					 * La maldición reduce el poder del
					 * objeto: escalar la contribución al
					 * poder atribuida a la maldición por
					 * un factor que aumenta con la
					 * resistencia de la maldición a la eliminación.
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
 * Ajustar el poder por un peso no estándar del objeto.
 *
 * Esto actualmente solo considera cambios en el peso de las maldiciones. Podría
 * usar obj->kind->weight como el peso estándar, pero eso:
 *     1) Haría que el poder fuera diferente a los cálculos de 4.2.5 cuando hay
 *        maldiciones que afectan el peso presentes pero obj->weight difiere de
 *        obj->kind->weight.
 *     2) En presencia de maldiciones que afectan el peso, uno tendría que protegerse
 *        contra la realización de estos cálculos en objetos de maldición (es decir,
 *        obj->kind->tval == curse_object_kind->tval
 *        && obj->kind->sval == curse_object_kind->sval) ya que los pesos
 *        en esos son ajustes al peso base del objeto que la maldición
 *        afecta y difieren de obj->kind->weight.
 */
static int nonstandard_weight_power(const struct object *obj, int p)
{
	int16_t std_weight = MAX(obj->weight, 0);
	int16_t nonstd_weight = object_weight_one(obj);
	bitflag flags[OF_SIZE];
	int adj;

	assert(nonstd_weight >= 0);
	if (std_weight == nonstd_weight) {
		/* Sin cambio en el peso, sin cambio en el poder. */
		return p;
	}

	/* Comenzar sin ajuste. */
	adj = 0;

	/*
	 * Para manejar THROWING a continuación, fusionar las banderas del objeto base y cualquier
	 * maldición.
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
	 * ac_power() tuvo en cuenta el peso cuando el objeto proporciona una cantidad
	 * base de armadura, por lo que no ajustar el poder para esos objetos aquí.
	 * Para objetos que no proporcionan una cantidad base de armadura, ajustar
	 * el poder bajo el supuesto de que más ligero de lo normal es beneficioso
	 * (más espacio bajo el límite de peso para otras cosas) y más pesado de
	 * lo normal es perjudicial.
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
	 * Los objetos con la bandera THROWING, ya sea directamente o a través de una
	 * maldición, pueden aumentar el daño con el aumento de peso. Ajustar el poder por eso.
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
	 * El peso también afecta el número de golpes (solo armas cuerpo a cuerpo),
	 * el estado de empuñadura pesada (arma cuerpo a cuerpo o lanzador; dependiente de la fuerza
	 * y normalmente solo relevante para objetos bastante pesados),
	 * los críticos (para cuerpo a cuerpo, proyectil lanzado o proyectil lanzado, pero solo en
	 * cálculos de combate no-O; aumentar el peso puede aumentar la probabilidad de
	 * un crítico y la cantidad de daño del crítico si ocurre),
	 * y los golpes con escudo (más peso es mejor; solo relevante para algunas
	 * clases). Ninguno de estos se tiene en cuenta aquí.
	 */

	if (adj) {
		p = add_guardi(p, adj);
		log_obj("Añadir %d de poder combinado por peso no estándar; "
			"total es %p\n", adj, p);
	}

	return p;
}


/**
 * Evaluar el nivel de poder general del objeto.
 */
int32_t object_power(const struct object* obj, bool verbose, ang_file *log_file)
{
	int32_t p = 0, dice_pwr = 0;
	int mult;

	/* Establecer el archivo de registro */
	object_log = log_file;

	/* Obtener todo el poder de ataque */
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

	/* Poder de clase de armadura */
	p = ac_power(obj, p);
	p = to_ac_power(obj, p);

	/* Bonificación por joyería */
	p = jewelry_power(obj, p);

	/* Otras propiedades del objeto */
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
 * Fijación de precios de objetos
 * ------------------------------------------------------------------------ */
/**
 * Devolver el "valor" de un objeto "desconocido"
 * Hacer una estimación del valor de objetos no conocidos
 */
static int object_value_base(const struct object *obj)
{
	/* Usar el coste de la plantilla para objetos conocidos */
	if (object_flavor_is_aware(obj))
		return obj->kind->cost;

	/* Analizar el tipo */
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
 * Devolver el precio real de un objeto conocido (o parcialmente conocido).
 *
 * Las varitas y bastones obtienen coste por cada carga.
 *
 * Los objetos equipables (armas, lanzadores, joyería, luces, armadura) y munición
 * se valoran según su nivel de poder. Toda la munición y las antorchas normales (sin ego)
 * se reducen por AMMO_RESCALER para reflejar su impermanencia.
 */
int object_value_real(const struct object *obj, int qty)
{
	int value, total_value;

	int power;
	/*
	 * Este es el coeficiente cuadrático para el poder en la expresión para
	 * el valor real. Debe ser no negativo.
	 */
	int a = 1;
	/*
	 * Este es el coeficiente lineal para el poder en la expresión para
	 * el valor real. Debe ser no negativo.
	 */
	int b = 5;

	/* Los equipables y la munición tienen precios que varían según las propiedades individuales del objeto */
	if (tval_has_variable_power(obj)) {
#ifdef PRICE_DEBUG
		char buf[1024];
		ang_file *log_file = NULL;
		static file_mode pricing_mode = MODE_WRITE;

		/* Registro */
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
		/* Proteger contra desbordamiento. */
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

		/* Reescalar para consumibles */
		if ((tval_is_light(obj) && of_has(obj->flags, OF_BURNS_OUT)
			 && !obj->ego) || tval_is_ammo(obj)) {
			value = value / AMMO_RESCALER;
		}

		/* Redondear hacia arriba para asegurar que cosas como capas no sean sin valor */
		if (value == 0) {
			value = 1;
		}

#ifdef PRICE_DEBUG
		/* Más registro */
		file_putf(log_file, "a es %d y b es %d\n", a, b);
		file_putf(log_file, "valor es %d\n", value);

		if (!file_close(log_file)) {
			msg("Error - no se puede cerrar el archivo pricing.log.");
			exit(1);
		}
#endif /* PRICE_DEBUG */

		/* Obtener el valor total */
		total_value = value * qty;
		if (total_value < 0) total_value = 0;
	} else {

		/* Objetos sin valor */
		if (!obj->kind->cost) return (0L);

		/* Coste base */
		value = obj->kind->cost;

		/* Analizar el tipo de objeto y la cantidad */
		if (tval_can_have_charges(obj)) {
			int charges;

			total_value = value * qty;

			/* Calcular el número de cargas, redondeado hacia arriba */
			charges = obj->pval * qty / obj->number;
			if ((obj->pval * qty) % obj->number != 0)
				charges++;

			/* Pagar extra por cargas, dependiendo del número estándar de cargas */
			total_value += value * charges / 20;
		} else {
			total_value = value * qty;
		}

		/* Sin valor negativo */
		if (total_value < 0) total_value = 0;
	}

	/* Devolver el valor */
	return (total_value);
}


/**
 * Devolver el precio de un objeto incluyendo pluses (y cargas).
 *
 * Esta función devuelve el "valor" del objeto dado (cantidad uno).
 *
 * Nunca notificar bonificaciones o propiedades desconocidas, incluyendo maldiciones,
 * ya que eso daría información al jugador que no tenía.
 */
int object_value(const struct object *obj, int qty)
{
	int value;

	/* Los objetos de poder variable se evalúan según lo que se sabe de ellos */
	if (tval_has_variable_power(obj) && obj->known) {
		value = object_value_real(obj->known, qty);
	} else if (tval_can_have_flavor_k(obj->kind) &&
			   object_flavor_is_aware(obj)) {
		value = object_value_real(obj, qty);
	} else {
		/* Los objetos desconocidos de precio constante solo obtienen un valor base */
		value = object_value_base(obj) * qty;
	}

	/* Devolver el valor final */
	return (value);
}