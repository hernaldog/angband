/**
 * \file player-attack.c
 * \brief Ataques (tanto lanzamiento como cuerpo a cuerpo) del jugador
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
#include "cmds.h"
#include "effects.h"
#include "game-event.h"
#include "game-input.h"
#include "generate.h"
#include "init.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-predicate.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "target.h"

/**
 * ------------------------------------------------------------------------
 * Cálculos de impacto y rotura
 * ------------------------------------------------------------------------ */
/**
 * Devuelve el porcentaje de probabilidad de que un objeto se rompa después de lanzarlo o dispararlo.
 *
 * Los artefactos nunca se rompen.
 *
 * Más allá de eso, cada tipo de objeto tiene un porcentaje de probabilidad de romperse (0-100). Cuando el
 * objeto impacta en su objetivo se usa esta probabilidad.
 *
 * Cuando un objeto falla también tiene una probabilidad de romperse. Esto se determina mediante
 * elevar al cuadrado la probabilidad de rotura normal. Así, un objeto que se rompe el 100% de
 * las veces al impactar también se romperá el 100% de las veces al fallar, mientras que una probabilidad
 * de rotura al impactar del 50% da una probabilidad de rotura al fallar del 25%, y una probabilidad
 * de rotura al impactar del 10% da una probabilidad de rotura al fallar del 1%.
 */
int breakage_chance(const struct object *obj, bool hit_target) {
	int perc = obj->kind->base->break_perc;

	if (obj->artifact) return 0;
	if (of_has(obj->flags, OF_THROWING) &&
		!of_has(obj->flags, OF_EXPLODE) &&
		!tval_is_ammo(obj)) {
		perc = 1;
	}
	if (!hit_target) return (perc * perc) / 100;
	return perc;
}

/**
 * Calcular el valor base de golpear en cuerpo a cuerpo del jugador sin considerar un monstruo
 * específico.
 * Ver también: chance_of_missile_hit_base
 *
 * \param p El jugador
 * \param weapon El arma del jugador
 */
int chance_of_melee_hit_base(const struct player *p,
		const struct object *weapon)
{
	int bonus = p->state.to_h + (weapon ? object_to_hit(weapon) : 0)
		+ (p->state.bless_wield ? 2 : 0);
	return p->state.skills[SKILL_TO_HIT_MELEE] + bonus * BTH_PLUS_ADJ;
}

/**
 * Calcular el valor de golpear en cuerpo a cuerpo del jugador contra un monstruo específico.
 * Ver también: chance_of_missile_hit
 *
 * \param p El jugador
 * \param weapon El arma del jugador
 * \param mon El monstruo
 */
static int chance_of_melee_hit(const struct player *p,
		const struct object *weapon, const struct monster *mon)
{
	int chance = chance_of_melee_hit_base(p, weapon);
	/* Los objetivos no visibles tienen una penalización al golpear del 50% */
	return monster_is_visible(mon) ? chance : chance / 2;
}

/**
 * Calcular el valor base de golpear con proyectil del jugador sin considerar un monstruo
 * específico.
 * Ver también: chance_of_melee_hit_base
 *
 * \param p El jugador
 * \param missile El proyectil a lanzar
 * \param launcher El lanzador a usar (opcional)
 */
int chance_of_missile_hit_base(const struct player *p,
								 const struct object *missile,
								 const struct object *launcher)
{
	int bonus = object_to_hit(missile);
	int chance;

	if (!launcher) {
		/* Otros objetos lanzados son más fáciles de usar, pero solo las armas arrojadizas
		 * aprovechan las bonificaciones a Habilidad y Letalidad de otros
		 * objetos equipados. */
		if (of_has(missile->flags, OF_THROWING)) {
			bonus += p->state.to_h;
			chance = p->state.skills[SKILL_TO_HIT_THROW] + bonus * BTH_PLUS_ADJ;
		} else {
			chance = 3 * p->state.skills[SKILL_TO_HIT_THROW] / 2
				+ bonus * BTH_PLUS_ADJ;
		}
	} else {
		bonus += p->state.to_h + object_to_hit(launcher);
		chance = p->state.skills[SKILL_TO_HIT_BOW] + bonus * BTH_PLUS_ADJ;
	}

	return chance;
}

/**
 * Calcular el valor de golpear con proyectil del jugador contra un monstruo específico.
 * Ver también: chance_of_melee_hit
 *
 * \param p El jugador
 * \param missile El proyectil a lanzar
 * \param launcher Lanzador opcional a usar (las armas arrojadizas no usan lanzador)
 * \param mon El monstruo
 */
static int chance_of_missile_hit(const struct player *p,
	const struct object *missile, const struct object *launcher,
	const struct monster *mon)
{
	int chance = chance_of_missile_hit_base(p, missile, launcher);
	/* Penalizar por distancia */
	chance -= distance(p->grid, mon->grid);
	/* Los objetivos no visibles tienen una penalización al golpear del 50% */
	return monster_is_obvious(mon) ? chance : chance / 2;
}

/**
 * Determinar si una tirada de impacto tiene éxito contra la CA objetivo.
 * Ver también: hit_chance
 *
 * \param to_hit El valor total de golpear a usar
 * \param ac La CA contra la que tirar
 */
bool test_hit(int to_hit, int ac)
{
	random_chance c;
	hit_chance(&c, to_hit, ac);
	return random_chance_check(c);
}

/**
 * Devolver un random_chance por referencia, que representa la probabilidad de que una
 * tirada de impacto tenga éxito para los valores de to_hit y ac dados. El cálculo de impacto
 *:
 *
 * Siempre impacta el 12% de las veces
 * Siempre falla el 5% de las veces
 * Establece un mínimo de 9 en el valor to_hit
 * Tira entre 0 y el valor to_hit
 * El resultado debe ser >= AC*2/3 para considerarse un impacto
 *
 * \param chance El random_chance a devolver por referencia
 * \param to_hit El valor de golpear a usar
 * \param ac La CA contra la que tirar
 */
void hit_chance(random_chance *chance, int to_hit, int ac)
{
	/* Porcentajes escalados a 10,000 para evitar errores de redondeo */
	const int HUNDRED_PCT = 10000;
	const int ALWAYS_HIT = 1200;
	const int ALWAYS_MISS = 500;

	/* Establecer un mínimo en to_hit */
	to_hit = MAX(9, to_hit);

	/* Calcular el porcentaje de impacto */
	chance->numerator = MAX(0, to_hit - ac * 2 / 3);
	chance->denominator = to_hit;

	/* Convertir la relación a un porcentaje escalado */
	chance->numerator = HUNDRED_PCT * chance->numerator / chance->denominator;
	chance->denominator = HUNDRED_PCT;

	/* La tasa calculada solo se aplica cuando no aplican el impacto/fallo garantizados */
	chance->numerator = chance->numerator *
			(HUNDRED_PCT - ALWAYS_MISS - ALWAYS_HIT) / HUNDRED_PCT;

	/* Añadir el impacto garantizado */
	chance->numerator += ALWAYS_HIT;
}

/**
 * ------------------------------------------------------------------------
 * Cálculos de daño
 * ------------------------------------------------------------------------ */
/**
 * Conversión de pluses a Letalidad en un porcentaje añadido al daño.
 * Gran parte de esta tabla no está pensada para ser utilizada nunca, y se incluye
 * solo para manejar una posible inflación en otras partes. -LM-
 */
uint8_t deadliness_conversion[151] =
  {
    0,
    5,  10,  14,  18,  22,  26,  30,  33,  36,  39,
    42,  45,  48,  51,  54,  57,  60,  63,  66,  69,
    72,  75,  78,  81,  84,  87,  90,  93,  96,  99,
    102, 104, 107, 109, 112, 114, 117, 119, 122, 124,
    127, 129, 132, 134, 137, 139, 142, 144, 147, 149,
    152, 154, 157, 159, 162, 164, 167, 169, 172, 174,
    176, 178, 180, 182, 184, 186, 188, 190, 192, 194,
    196, 198, 200, 202, 204, 206, 208, 210, 212, 214,
    216, 218, 220, 222, 224, 226, 228, 230, 232, 234,
    236, 238, 240, 242, 244, 246, 248, 250, 251, 253,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255
  };

/**
 * La letalidad multiplica el daño infligido por un porcentaje, que varía
 * desde el 0% (no se inflige daño alguno) hasta como máximo el 355% (el daño se multiplica
 * ¡por más de tres veces y media!).
 *
 * Usamos la tabla "deadliness_conversion" para traducir los pluses internos
 * a letalidad en valores porcentuales.
 *
 * Esta función multiplica el daño por 100.
 */
void apply_deadliness(int *die_average, int deadliness)
{
	int i;

	/* Paranoia - asegurar acceso legal a la tabla. */
	if (deadliness > 150)
		deadliness = 150;
	if (deadliness < -150)
		deadliness = -150;

	/* La letalidad es positiva - el daño aumenta */
	if (deadliness >= 0) {
		i = deadliness_conversion[deadliness];

		*die_average *= (100 + i);
	}

	/* La letalidad es negativa - el daño disminuye */
	else {
		i = deadliness_conversion[ABS(deadliness)];

		if (i >= 100)
			*die_average = 0;
		else
			*die_average *= (100 - i);
	}
}

/**
 * Verificar si un monstruo tiene algún estado negativo que haga que un golpe
 * crítico sea más probable.
 */
static bool is_debuffed(const struct monster *monster)
{
	return monster->m_timed[MON_TMD_CONF] > 0 ||
			monster->m_timed[MON_TMD_HOLD] > 0 ||
			monster->m_timed[MON_TMD_FEAR] > 0 ||
			monster->m_timed[MON_TMD_STUN] > 0;
}

/**
 * Determinar el daño para golpes críticos al disparar.
 *
 * Tener en cuenta el peso del objeto, los pluses totales y el nivel del jugador.
 */
static int critical_shot(const struct player *p,
		const struct monster *monster,
		int weight, int plus,
		int dam, bool launched, uint32_t *msg_type)
{
	int to_h = p->state.to_h + plus;
	int chance, new_dam;

	if (is_debuffed(monster)) {
		to_h += z_info->r_crit_debuff_toh;
	}
	chance = z_info->r_crit_chance_weight_scl * weight
		+ z_info->r_crit_chance_toh_scl * to_h
		+ z_info->r_crit_chance_level_scl * p->lev
		+ z_info->r_crit_chance_offset;
	if (launched) {
		chance += z_info->r_crit_chance_launched_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_BOW];
	} else {
		chance += z_info->r_crit_chance_thrown_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_THROW];
	}

	if (randint1(z_info->r_crit_chance_range) > chance
			|| !z_info->r_crit_level_head) {
		*msg_type = MSG_SHOOT_HIT;
		new_dam = dam;
	} else {
		int power = z_info->r_crit_power_weight_scl * weight
			+ randint1(z_info->r_crit_power_random);
		const struct critical_level *this_l = z_info->r_crit_level_head;

		while (power >= this_l->cutoff && this_l->next) {
			this_l = this_l->next;
		}
		*msg_type = this_l->msgt;
		new_dam = this_l->add + this_l->mult * dam;
	}

	return new_dam;
}

/**
 * Determinar el daño para golpes críticos al disparar en combate O.
 */
static int o_critical_shot(const struct player *p,
		const struct monster *monster,
		const struct object *missile,
		const struct object *launcher,
		uint32_t *msg_type)
{
	int power = chance_of_missile_hit_base(p, missile, launcher);
	int chance_num, chance_den, add_dice;

	if (is_debuffed(monster)) {
		power += z_info->o_r_crit_debuff_toh;
	}
	/* Aplicar un factor de escala racional. */
	if (launcher) {
		power = (power * z_info->o_r_crit_power_launched_toh_scl_num)
			/ z_info->o_r_crit_power_launched_toh_scl_den;
	} else {
		power = (power * z_info->o_r_crit_power_thrown_toh_scl_num)
			/ z_info->o_r_crit_power_thrown_toh_scl_den;
	}

	/* Probar para golpe crítico: la probabilidad es a * poder / (b * poder + c) */
	chance_num = power * z_info->o_r_crit_chance_power_scl_num;
	chance_den = power * z_info->o_r_crit_chance_power_scl_den
		+ z_info->o_r_crit_chance_add_den;
	if (randint1(chance_den) <= chance_num && z_info->o_r_crit_level_head) {
		/* Determinar el nivel del golpe crítico. */
		const struct o_critical_level *this_l =
			z_info->o_r_crit_level_head;

		while (this_l->next && !one_in_(this_l->chance)) {
			this_l = this_l->next;
		}
		add_dice = this_l->added_dice;
		*msg_type = this_l->msgt;
	} else {
		add_dice = 0;
		*msg_type = MSG_SHOOT_HIT;
	}

	return add_dice;
}

/**
 * Determinar el daño para golpes críticos en cuerpo a cuerpo.
 *
 * Tener en cuenta el peso del arma, los pluses totales y el nivel del jugador.
 */
static int critical_melee(const struct player *p,
		const struct monster *monster,
		int weight, int plus,
		int dam, uint32_t *msg_type)
{
	int to_h = p->state.to_h + plus;
	int chance, new_dam;

	if (is_debuffed(monster)) {
		to_h += z_info->m_crit_debuff_toh;
	}
	chance = z_info->m_crit_chance_weight_scl * weight
		+ z_info->m_crit_chance_toh_scl * to_h
		+ z_info->m_crit_chance_level_scl * p->lev
		+ z_info->m_crit_chance_toh_skill_scl
			* p->state.skills[SKILL_TO_HIT_MELEE]
		+ z_info->m_crit_chance_offset;

	if (randint1(z_info->m_crit_chance_range) > chance
			|| !z_info->m_crit_level_head) {
		*msg_type = MSG_HIT;
		new_dam = dam;
	} else {
		int power = z_info->m_crit_power_weight_scl * weight
			+ randint1(z_info->m_crit_power_random);
		const struct critical_level *this_l = z_info->m_crit_level_head;

		while (power >= this_l->cutoff && this_l->next) {
			this_l = this_l->next;
		}
		*msg_type = this_l->msgt;
		new_dam = this_l->add + this_l->mult * dam;
	}

	return new_dam;
}

/**
 * Determinar el daño para golpes críticos en cuerpo a cuerpo en combate O.
 */
static int o_critical_melee(const struct player *p,
		const struct monster *monster,
		const struct object *obj, uint32_t *msg_type)
{
	int power = chance_of_melee_hit_base(p, obj);
	int chance_num, chance_den, add_dice;

	if (is_debuffed(monster)) {
		power += z_info->o_m_crit_debuff_toh;
	}
	/* Aplicar un factor de escala racional. */
	power = (power * z_info->o_m_crit_power_toh_scl_num)
		/ z_info->o_m_crit_power_toh_scl_den;

	/* Probar para golpe crítico: la probabilidad es a * poder / (b * poder + c) */
	chance_num = power * z_info->o_m_crit_chance_power_scl_num;
	chance_den = power * z_info->o_m_crit_chance_power_scl_den
		+ z_info->o_m_crit_chance_add_den;
	if (randint1(chance_den) <= chance_num && z_info->o_m_crit_level_head) {
		/* Determinar el nivel del golpe crítico. */
		const struct o_critical_level *this_l =
			z_info->o_m_crit_level_head;

		while (this_l->next && !one_in_(this_l->chance)) {
			this_l = this_l->next;
		}
		add_dice = this_l->added_dice;
		*msg_type = this_l->msgt;
	} else {
		add_dice = 0;
		*msg_type = MSG_SHOOT_HIT;
	}

	return add_dice;
}

/**
 * Determinar el daño estándar en cuerpo a cuerpo.
 *
 * Tener en cuenta los dados de daño, para-dañar y cualquier marca o azote.
 */
static int melee_damage(const struct monster *mon, struct object *obj, int b, int s)
{
	int dmg = (obj) ? damroll(obj->dd, obj->ds) : 1;

	if (s) {
		dmg *= slays[s].multiplier;
	} else if (b) {
		dmg *= get_monster_brand_multiplier(mon, &brands[b], false);
	}

	if (obj) dmg += object_to_dam(obj);

	return dmg;
}

/**
 * Determinar el daño en cuerpo a cuerpo en combate O.
 *
 * La letalidad y cualquier marca o azote añaden caras extra a los dados de daño,
 * los críticos añaden dados extra.
 */
static int o_melee_damage(struct player *p, const struct monster *mon,
		struct object *obj, int b, int s, uint32_t *msg_type)
{
	int dice = (obj) ? obj->dd : 1;
	int sides, deadliness, dmg, add = 0;
	bool extra;

	/* Obtener el valor medio de un dado de daño individual. (x10) */
	int die_average = (10 * (((obj) ? obj->ds : 1) + 1)) / 2;

	/* Ajustar la media para azotes y marcas. (inflación x10) */
	if (s) {
		die_average *= slays[s].o_multiplier;
		add = slays[s].o_multiplier - 10;
	} else if (b) {
		int bmult = get_monster_brand_multiplier(mon, &brands[b], true);

		die_average *= bmult;
		add = bmult - 10;
	} else {
		die_average *= 10;
	}

	/* Aplicar letalidad a la media. (inflación x100) */
	deadliness = p->state.to_d + ((obj) ? object_to_dam(obj) : 0);
	apply_deadliness(&die_average, MIN(deadliness, 150));

	/* Calcular el número real de caras de cada dado. */
	sides = (2 * die_average) - 10000;
	extra = randint0(10000) < (sides % 10000);
	sides /= 10000;
	sides += (extra ? 1 : 0);

	/*
	 * Obtener el número de dados críticos; por ahora, excluyendo críticos para
	 * combate sin armas
	 */
	if (obj) dice += o_critical_melee(p, mon, obj, msg_type);

	/* Tirar el daño. */
	dmg = damroll(dice, sides);

	/* Aplicar cualquier adición especial al daño. */
	dmg += add;

	return dmg;
}

/**
 * Determinar el daño estándar a distancia.
 *
 * Tener en cuenta los dados de daño, para-dañar, multiplicador y cualquier marca o azote.
 */
static int ranged_damage(struct player *p, const struct monster *mon,
						 struct object *missile, struct object *launcher,
						 int b, int s)
{
	int dmg;
	int mult = (launcher ? p->state.ammo_mult : 1);

	/* Si tenemos un azote o una marca, modificar el multiplicador apropiadamente */
	if (b) {
		mult += get_monster_brand_multiplier(mon, &brands[b], false);
	} else if (s) {
		mult += slays[s].multiplier;
	}

	/* Aplicar daño: multiplicador, azotes, bonificaciones */
	dmg = damroll(missile->dd, missile->ds);
	dmg += object_to_dam(missile);
	if (launcher) {
		dmg += object_to_dam(launcher);
	} else if (of_has(missile->flags, OF_THROWING)) {
		/* Ajustar daño para armas arrojadizas.
		 * Esta no es la ecuación más bonita, pero al menos intenta
		 * mantener las armas arrojadizas competitivas. */
		dmg *= 2 + object_weight_one(missile) / 12;
	}
	dmg *= mult;

	return dmg;
}

/**
 * Determinar el daño a distancia en combate O.
 *
 * La letalidad, el multiplicador del lanzador y cualquier marca o azote añaden caras extra a los
 * dados de daño, los críticos añaden dados extra.
 */
static int o_ranged_damage(struct player *p, const struct monster *mon,
		struct object *missile, struct object *launcher,
		int b, int s, uint32_t *msg_type)
{
	int mult = (launcher ? p->state.ammo_mult : 1);
	int dice = missile->dd;
	int sides, deadliness, dmg, add = 0;
	bool extra;

	/* Obtener el valor medio de un dado de daño individual. (x10) */
	int die_average = (10 * (missile->ds + 1)) / 2;

	/* Aplicar el multiplicador del lanzador. */
	die_average *= mult;

	/* Ajustar la media para azotes y marcas. (inflación x10) */
	if (b) {
		int bmult = get_monster_brand_multiplier(mon, &brands[b], true);

		die_average *= bmult;
		add = bmult - 10;
	} else if (s) {
		die_average *= slays[s].o_multiplier;
		add = slays[s].o_multiplier - 10;
	} else {
		die_average *= 10;
	}

	/* Aplicar letalidad a la media. (inflación x100) */
	deadliness = object_to_dam(missile);
	if (launcher) {
		deadliness += object_to_dam(launcher) + p->state.to_d;
	} else if (of_has(missile->flags, OF_THROWING)) {
		deadliness += p->state.to_d;
	}
	apply_deadliness(&die_average, MIN(deadliness, 150));

	/* Calcular el número real de caras de cada dado. */
	sides = (2 * die_average) - 10000;
	extra = randint0(10000) < (sides % 10000);
	sides /= 10000;
	sides += (extra ? 1 : 0);

	/* Obtener el número de dados críticos - solo para objetos adecuados */
	if (launcher) {
		dice += o_critical_shot(p, mon, missile, launcher, msg_type);
	} else if (of_has(missile->flags, OF_THROWING)) {
		dice += o_critical_shot(p, mon, missile, NULL, msg_type);

		/* Multiplicar el número de dados de daño por el multiplicador del arma arrojadiza.
		 * Esta no es la ecuación más bonita,
		 * pero al menos intenta mantener las armas arrojadizas competitivas. */
		dice *= 2 + object_weight_one(missile) / 12;
	}

	/* Tirar el daño. */
	dmg = damroll(dice, sides);

	/* Aplicar cualquier adición especial al daño. */
	dmg += add;

	return dmg;
}

/**
 * Aplicar las bonificaciones de daño del jugador
 */
static int player_damage_bonus(struct player_state *state)
{
	return state->to_d;
}

/**
 * ------------------------------------------------------------------------
 * Efectos secundarios de los golpes de cuerpo a cuerpo (no daño)
 * ------------------------------------------------------------------------ */
/**
 * Aplicar efectos secundarios del golpe
 */
static void blow_side_effects(struct player *p, struct monster *mon)
{
	/* Ataque de confusión */
	if (p->timed[TMD_ATT_CONF]) {
		player_clear_timed(p, TMD_ATT_CONF, true, false);

		mon_inc_timed(mon, MON_TMD_CONF, (10 + randint0(p->lev) / 10),
					  MON_TMD_FLG_NOTIFY);
	}
}

/**
 * Aplicar efectos posteriores al golpe
 */
static bool blow_after_effects(struct loc grid, int dmg, int splash,
							   bool *fear, bool quake)
{
	/* Aplicar marca de terremoto */
	if (quake) {
		effect_simple(EF_EARTHQUAKE, source_player(), "0", 0, 10, 0, 0, 0,
					  NULL);

		/* El monstruo puede estar muerto o haberse movido */
		if (!square_monster(cave, grid))
			return true;
	}

	return false;
}

/**
 * ------------------------------------------------------------------------
 * Ataque cuerpo a cuerpo
 * ------------------------------------------------------------------------ */
/* Tipos de impacto cuerpo a cuerpo y lanzamiento */
static const struct hit_types melee_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_HIT, NULL },
	{ MSG_HIT_GOOD, "¡Fue un buen golpe!" },
	{ MSG_HIT_GREAT, "¡Fue un gran golpe!" },
	{ MSG_HIT_SUPERB, "¡Fue un golpe soberbio!" },
	{ MSG_HIT_HI_GREAT, "¡Fue un *GRAN* golpe!" },
	{ MSG_HIT_HI_SUPERB, "¡Fue un *SOBERBIO* golpe!" },
};

/**
 * Atacar al monstruo en la ubicación dada con un solo golpe.
 */
bool py_attack_real(struct player *p, struct loc grid, bool *fear)
{
	size_t i;

	/* Información sobre el objetivo del ataque */
	struct monster *mon = square_monster(cave, grid);
	char m_name[80];
	bool stop = false;

	/* El arma utilizada */
	struct object *obj = equipped_item_by_slot_name(p, "weapon");

	/* Información sobre el ataque */
	int drain = 0;
	int splash = 0;
	bool do_quake = false;
	bool success = false;

	char verb[20];
	uint32_t msg_type = MSG_HIT;
	int j, b, s, weight, dmg;

	/* Por defecto, puñetazo */
	my_strcpy(verb, "golpeas", sizeof(verb));

	/* Extraer nombre del monstruo (o "eso") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_TARG);

	/* Auto-Recordar y rastrear si es posible y visible */
	if (monster_is_visible(mon)) {
		monster_race_track(p->upkeep, mon->race);
		health_track(p->upkeep, mon);
	}

	/* Manejar miedo del jugador (solo para monstruos invisibles) */
	if (player_of_has(p, OF_AFRAID)) {
		equip_learn_flag(p, OF_AFRAID);
		msgt(MSG_AFRAID, "¡Tienes demasiado miedo para atacar a %s!", m_name);
		return false;
	}

	/* Molestar al monstruo */
	monster_wake(mon, false, 100);
	mon_clear_timed(mon, MON_TMD_HOLD, MON_TMD_FLG_NOTIFY);

	/* Ver si el jugador impactó */
	success = test_hit(chance_of_melee_hit(p, obj, mon), mon->race->ac);

	/* Si es un fallo, saltar este golpe */
	if (!success) {
		msgt(MSG_MISS, "Fallas a %s.", m_name);

		/* Pequeña probabilidad de efectos secundarios de sed de sangre */
		if (p->timed[TMD_BLOODLUST] && one_in_(50)) {
			msg("Te sientes extraño...");
			player_over_exert(p, PY_EXERT_SCRAMBLE, 20, 20);
		}

		return false;
	}

	if (obj) {
		/* Manejar arma normal */
		weight = object_weight_one(obj);
		my_strcpy(verb, "golpeas", sizeof(verb));
	} else {
		weight = 0;
	}

	/* Mejor ataque de todos los azotes o marcas en todo el equipo que no sea lanzador */
	b = 0;
	s = 0;
	for (j = 2; j < p->body.count; j++) {
		struct object *obj_local = slot_object(p, j);
		if (obj_local)
			improve_attack_modifier(p, obj_local, mon, &b, &s,
				verb, false);
	}

	/* Obtener el mejor ataque de todos los azotes o marcas - arma o temporales */
	if (obj) {
		improve_attack_modifier(p, obj, mon, &b, &s, verb, false);
	}
	improve_attack_modifier(p, NULL, mon, &b, &s, verb, false);

	/* Obtener el daño */
	if (!OPT(p, birth_percent_damage)) {
		dmg = melee_damage(mon, obj, b, s);
		/* Por ahora, excluir críticos en combate sin armas */
		if (obj) {
			dmg = critical_melee(p, mon, weight, object_to_hit(obj),
				dmg, &msg_type);
		}
	} else {
		dmg = o_melee_damage(p, mon, obj, b, s, &msg_type);
	}

	/* Daño en área y terremotos */
	splash = (weight * dmg) / 100;
	if (player_of_has(p, OF_IMPACT) && dmg > 50) {
		do_quake = true;
		equip_learn_flag(p, OF_IMPACT);
	}

	/* Aprender mediante el uso */
	equip_learn_on_melee_attack(p);
	learn_brand_slay_from_melee(p, obj, mon);

	/* Aplicar las bonificaciones de daño del jugador */
	if (!OPT(p, birth_percent_damage)) {
		dmg += player_damage_bonus(&p->state);
	}

	/* Sustituir golpes específicos de la forma para jugadores cambiados de forma */
	if (player_is_shapechanged(p)) {
		int choice = randint0(p->shape->num_blows);
		struct player_blow *blow = p->shape->blows;
		while (choice--) {
			blow = blow->next;
		}
		my_strcpy(verb, blow->name, sizeof(verb));
	}

	/* Sin daño negativo; cambiar verbo si no se infligió daño */
	if (dmg <= 0) {
		dmg = 0;
		msg_type = MSG_MISS;
		my_strcpy(verb, "no logras herir", sizeof(verb));
	}

	for (i = 0; i < N_ELEMENTS(melee_hit_types); i++) {
		const char *dmg_text = "";

		if (msg_type != melee_hit_types[i].msg_type)
			continue;

		if (OPT(p, show_damage))
			dmg_text = format(" (%d)", dmg);

		if (melee_hit_types[i].text)
			msgt(msg_type, "%s a %s%s. %s", verb, m_name, dmg_text,
					melee_hit_types[i].text);
		else
			msgt(msg_type, "%s a %s%s.", verb, m_name, dmg_text);
	}

	/* Efectos secundarios previos al daño */
	blow_side_effects(p, mon);

	/* Daño, comprobar drenaje de HP, miedo y muerte */
	drain = MIN(mon->hp, dmg);
	stop = mon_take_hit(mon, p, dmg, fear, NULL);

	/* Pequeña probabilidad de efectos secundarios de sed de sangre */
	if (p->timed[TMD_BLOODLUST] && one_in_(50)) {
		msg("¡Sientes que algo cede!");
		player_over_exert(p, PY_EXERT_CON, 20, 0);
	}

	if (!stop) {
		if (p->timed[TMD_ATT_VAMP] && monster_is_living(mon)) {
			effect_simple(EF_HEAL_HP, source_player(), format("%d", drain),
						  0, 0, 0, 0, 0, NULL);
		}
	}

	if (stop)
		(*fear) = false;

	/* Efectos posteriores al daño */
	if (blow_after_effects(grid, dmg, splash, fear, do_quake))
		stop = true;

	return stop;
}


/**
 * Intentar un golpe con escudo; devuelve true si el monstruo muere
 */
static bool attempt_shield_bash(struct player *p, struct monster *mon, bool *fear)
{
	struct object *weapon = slot_object(p, slot_by_name(p, "weapon"));
	struct object *shield = slot_object(p, slot_by_name(p, "arm"));
	int nblows = p->state.num_blows / 100;
	int bash_quality, bash_dam, energy_lost;

	/* La probabilidad de golpe depende de la habilidad cuerpo a cuerpo, DEX y una bonificación por nivel. */
	int bash_chance = p->state.skills[SKILL_TO_HIT_MELEE] / 8 +
		adj_dex_th[p->state.stat_ind[STAT_DEX]] / 2;

	/* Sin escudo, no hay golpe */
	if (!shield) return false;

	/* El monstruo es demasiado patético, no merece la pena */
	if (mon->race->level < p->lev / 2) return false;

	/* Los jugadores golpean con escudo más a menudo cuando ven una necesidad real: */
	if (!equipped_item_by_slot_name(p, "weapon")) {
		/* Sin armas... */
		bash_chance *= 4;
	} else if (weapon->dd * weapon->ds * nblows < shield->dd * shield->ds * 3) {
		/* ... o armados con un arma insignificante */
		bash_chance *= 2;
	}

	/* Intentar dar un golpe con escudo. */
	if (bash_chance <= randint0(200 + mon->race->level)) {
		return false;
	}

	/* Calcular calidad del ataque, una mezcla de impulso y precisión. */
	bash_quality = p->state.skills[SKILL_TO_HIT_MELEE] / 4 + p->wt / 8 +
		p->upkeep->total_weight / 80 + object_weight_one(shield) / 2;

	/* Calcular daño. Los escudos grandes son letales. */
	bash_dam = damroll(shield->dd, shield->ds);

	/* Multiplicar por factores de calidad y experiencia */
	bash_dam *= bash_quality / 40 + p->lev / 14;

	/* Bonificación por fuerza. */
	bash_dam += adj_str_td[p->state.stat_ind[STAT_STR]];

	/* Paranoia. */
	if (bash_dam <= 0) return false;
	bash_dam = MIN(bash_dam, 125);

	if (OPT(p, show_damage)) {
		msgt(MSG_HIT, "¡Consigues dar un golpe con el escudo! (%d)", bash_dam);
	} else {
		msgt(MSG_HIT, "¡Consigues dar un golpe con el escudo!");
	}

	/* Animar al jugador a seguir llevando ese escudo pesado. */
	if (randint1(bash_dam) > 30 + randint1(bash_dam / 2)) {
		msgt(MSG_HIT_HI_SUPERB, "¡ZAS!");
	}

	/* Daño, comprobar miedo y muerte. */
	if (mon_take_hit(mon, p, bash_dam, fear, NULL)) return true;

	/* Aturdimiento. */
	if (bash_quality + p->lev > randint1(200 + mon->race->level * 8)) {
		mon_inc_timed(mon, MON_TMD_STUN, randint0(p->lev / 5) + 4, 0);
	}

	/* Confusión. */
	if (bash_quality + p->lev > randint1(300 + mon->race->level * 12)) {
		mon_inc_timed(mon, MON_TMD_CONF, randint0(p->lev / 5) + 4, 0);
	}

	/* El jugador a veces tropieza. */
	if (35 + adj_dex_th[p->state.stat_ind[STAT_DEX]] < randint1(60)) {
		energy_lost = randint1(50) + 25;
		/* Perder el 26-75% de un turno debido al tropiezo después del golpe con escudo. */
		msgt(MSG_GENERIC, "¡Tropiezas!");
		p->upkeep->energy_use += energy_lost * z_info->move_energy / 100;
	}

	return false;
}

/**
 * Atacar al monstruo en la ubicación dada
 *
 * Obtenemos golpes hasta que la energía cae por debajo de la requerida para otro golpe, o
 * hasta que el monstruo objetivo muere. Cada golpe es manejado por py_attack_real().
 * No permitimos que @ gaste más de 1 turno de energía,
 * para evitar que monstruos más lentos tengan movimientos dobles.
 */
void py_attack(struct player *p, struct loc grid)
{
	int avail_energy = MIN(p->energy, z_info->move_energy);
	int blow_energy = 100 * z_info->move_energy / p->state.num_blows;
	bool slain = false, fear = false;
	struct monster *mon = square_monster(cave, grid);

	/* Molestar al jugador */
	disturb(p);

	/* Inicializar la energía usada */
	p->upkeep->energy_use = 0;

	/* Recompensar a BG con 5% de los PM máximos, mínimo 1/2 punto */
	if (player_has(p, PF_COMBAT_REGEN)) {
		int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 16384) / 5;
		player_adjust_mana_precise(p, sp_gain);
	}

	/* El jugador intenta un golpe con escudo si puede, y si el monstruo es visible
	 * y no demasiado patético */
	if (player_has(p, PF_SHIELD_BASH) && monster_is_visible(mon)) {
		/* El monstruo puede morir */
		if (attempt_shield_bash(p, mon, &fear)) return;
	}

	/* Atacar hasta que el siguiente ataque exceda la energía disponible o
	 * un turno completo o hasta que el enemigo muera. Limitamos el uso de energía
	 * para evitar dar a los monstruos un posible movimiento doble. */
	while (avail_energy - p->upkeep->energy_use >= blow_energy && !slain) {
		slain = py_attack_real(p, grid, &fear);
		p->upkeep->energy_use += blow_energy;
	}

	/* Truco - retrasar mensajes de miedo */
	if (fear && monster_is_visible(mon)) {
		add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
	}
}

/**
 * ------------------------------------------------------------------------
 * Ataques a distancia
 * ------------------------------------------------------------------------ */
/* Tipos de impacto al disparar */
static const struct hit_types ranged_hit_types[] = {
	{ MSG_MISS, NULL },
	{ MSG_SHOOT_HIT, NULL },
	{ MSG_HIT_GOOD, "¡Fue un buen golpe!" },
	{ MSG_HIT_GREAT, "¡Fue un gran golpe!" },
	{ MSG_HIT_SUPERB, "¡Fue un golpe soberbio!" }
};

/**
 * Esta es una función auxiliar utilizada por do_cmd_throw y do_cmd_fire.
 *
 * Abstrae la ruta del proyectil, el código de visualización, la identificación y la lógica
 * de limpieza, mientras usa el parámetro 'attack' para hacer el trabajo particular de cada
 * tipo de ataque.
 */
static void ranged_helper(struct player *p,	struct object *obj, int dir,
						  int range, int shots, ranged_attack attack,
						  const struct hit_types *hit_types, int num_types)
{
	int i, j;

	int path_n;
	struct loc path_g[256];

	/* Empezar en el jugador */
	struct loc grid = p->grid;

	/* Predecir la ubicación del "objetivo" */
	struct loc target = loc_sum(grid, loc(99 * ddx[dir], 99 * ddy[dir]));

	bool hit_target = false;
	bool none_left = false;

	struct object *missile;
	int pierce = 1;

	/* Comprobar la validez del objetivo */
	if ((dir == DIR_TARGET) && target_okay()) {
		int taim;
		target_get(&target);
		taim = distance(grid, target);
		if (taim > range) {
			char msg[80];
			strnfmt(msg, sizeof(msg),
					"Objetivo fuera de alcance por %d casillas. ¿Disparar de todas formas? ",
				taim - range);
			if (!get_check(msg)) return;
		}
	}

	/* Sonido */
	sound(MSG_SHOOT);

	/* En realidad, "disparar" el objeto -- Tomar un turno parcial */
	p->upkeep->energy_use = (z_info->move_energy * 10 / shots);

	/* Calcular la ruta */
	path_n = project_path(cave, path_g, range, grid, target, 0);

	/* Calcular el potencial de perforación */
	if (p->timed[TMD_POWERSHOT] && tval_is_sharp_missile(obj)) {
		pierce = p->state.ammo_mult;
	}

	/* Manejar eventos */
	handle_stuff(p);

	/* Proyectar a lo largo de la ruta */
	for (i = 0; i < path_n; ++i) {
		struct monster *mon = NULL;
		bool see = square_isseen(cave, path_g[i]);

		/* Detenerse antes de golpear paredes */
		if (!(square_ispassable(cave, path_g[i])) &&
			!(square_isprojectable(cave, path_g[i])))
			break;

		/* Avanzar */
		grid = path_g[i];

		/* Decir a la UI que muestre el proyectil */
		event_signal_missile(EVENT_MISSILE, obj, see, grid.y, grid.x);

		/* Intentar el ataque al monstruo en (x, y) si lo hay */
		mon = square_monster(cave, path_g[i]);
		if (mon) {
			int visible = monster_is_obvious(mon);

			bool fear = false;
			const char *note_dies = monster_is_destroyed(mon) ? 
				" es destruido." : " muere.";

			struct attack_result result = attack(p, obj, grid);
			int dmg = result.dmg;
			uint32_t msg_type = result.msg_type;
			char hit_verb[20];
			my_strcpy(hit_verb, result.hit_verb, sizeof(hit_verb));
			mem_free(result.hit_verb);

			if (result.success) {
				char o_name[80];

				hit_target = true;

				missile_learn_on_ranged_attack(p, obj);

				/* Aprender mediante el uso para otros objetos equipados */
				equip_learn_on_ranged_attack(p);

				/*
				 * Describir el objeto (tener el conocimiento más
				 * actualizado ahora).
				 */
				object_desc(o_name, sizeof(o_name), obj,
					ODESC_FULL | ODESC_SINGULAR, p);

				/* Sin daño negativo; cambiar verbo si no se infligió daño */
				if (dmg <= 0) {
					dmg = 0;
					msg_type = MSG_MISS;
					my_strcpy(hit_verb, "no logra herir", sizeof(hit_verb));
				}

				if (!visible) {
					/* Monstruo invisible */
					msgt(MSG_SHOOT_HIT, "El %s encuentra un blanco.", o_name);
				} else {
					for (j = 0; j < num_types; j++) {
						char m_name[80];
						const char *dmg_text = "";

						if (msg_type != hit_types[j].msg_type) {
							continue;
						}

						if (OPT(p, show_damage)) {
							dmg_text = format(" (%d)", dmg);
						}

						monster_desc(m_name, sizeof(m_name), mon, MDESC_OBJE);

						if (hit_types[j].text) {
							msgt(msg_type, "Tu %s %s a %s%s. %s", o_name, 
								 hit_verb, m_name, dmg_text, hit_types[j].text);
						} else {
							msgt(msg_type, "Tu %s %s a %s%s.", o_name, hit_verb,
								 m_name, dmg_text);
						}
					}

					/* Rastrear este monstruo */
					if (monster_is_obvious(mon)) {
						monster_race_track(p->upkeep, mon->race);
						health_track(p->upkeep, mon);
					}
				}

				/* Golpear al monstruo, comprobar si muere */
				if (!mon_take_hit(mon, p, dmg, &fear, note_dies)) {
					message_pain(mon, dmg);
					if (fear && monster_is_obvious(mon)) {
						add_monster_message(mon, MON_MSG_FLEE_IN_TERROR, true);
					}
				}
			}
			/* Detener el proyectil, o reducir su efecto de perforación */
			pierce--;
			if (pierce) continue;
			else break;
		}

		/* Detenerse si no es proyectable pero es transitable */
		if (!(square_isprojectable(cave, path_g[i]))) 
			break;
	}

	/* Obtener el proyectil */
	if (object_is_carried(p, obj)) {
		missile = gear_object_for_use(p, obj, 1, true, &none_left);
	} else {
		missile = floor_object_for_use(p, obj, 1, true, &none_left);
	}

	/* Terminar perforación */
	if (p->timed[TMD_POWERSHOT]) {
		player_clear_timed(p, TMD_POWERSHOT, true, false);
	}

	/* Soltar (o romper) cerca de esa ubicación */
	drop_near(cave, &missile, breakage_chance(missile, hit_target), grid, true, false);
}


/**
 * Función auxiliar utilizada con ranged_helper por do_cmd_fire.
 */
struct attack_result make_ranged_shot(struct player *p,
		struct object *ammo, struct loc grid)
{
	char *hit_verb = mem_alloc(20 * sizeof(char));
	struct attack_result result = {false, 0, 0, hit_verb};
	struct object *bow = equipped_item_by_slot_name(p, "shooting");
	struct monster *mon = square_monster(cave, grid);
	int b = 0, s = 0;

	my_strcpy(hit_verb, "golpea", 20);

	/* ¿Le dimos? */
	if (!test_hit(chance_of_missile_hit(p, ammo, bow, mon), mon->race->ac))
		return result;

	result.success = true;

	improve_attack_modifier(p, ammo, mon, &b, &s, result.hit_verb, true);
	improve_attack_modifier(p, bow, mon, &b, &s, result.hit_verb, true);

	if (!OPT(p, birth_percent_damage)) {
		result.dmg = ranged_damage(p, mon, ammo, bow, b, s);
		result.dmg = critical_shot(p, mon, object_weight_one(ammo),
			object_to_hit(ammo), result.dmg, true,
			&result.msg_type);
	} else {
		result.dmg = o_ranged_damage(p, mon, ammo, bow, b, s, &result.msg_type);
	}

	missile_learn_on_ranged_attack(p, bow);
	learn_brand_slay_from_launch(p, ammo, bow, mon);

	return result;
}


/**
 * Función auxiliar utilizada con ranged_helper por do_cmd_throw.
 */
struct attack_result make_ranged_throw(struct player *p,
	struct object *obj, struct loc grid)
{
	char *hit_verb = mem_alloc(20 * sizeof(char));
	struct attack_result result = {false, 0, 0, hit_verb};
	struct monster *mon = square_monster(cave, grid);
	int b = 0, s = 0;

	my_strcpy(hit_verb, "golpea", 20);

	/* Si fallamos, hemos terminado */
	if (!test_hit(chance_of_missile_hit(p, obj, NULL, mon), mon->race->ac))
		return result;

	result.success = true;

	improve_attack_modifier(p, obj, mon, &b, &s, result.hit_verb, true);

	if (!OPT(p, birth_percent_damage)) {
		result.dmg = ranged_damage(p, mon, obj, NULL, b, s);
		result.dmg = critical_shot(p, mon, object_weight_one(obj),
			object_to_hit(obj), result.dmg, false,
			&result.msg_type);
	} else {
		result.dmg = o_ranged_damage(p, mon, obj, NULL, b, s, &result.msg_type);
	}

	/* Ajuste directo para cosas explosivas (frascos de aceite) */
	if (of_has(obj->flags, OF_EXPLODE))
		result.dmg *= 3;

	learn_brand_slay_from_throw(p, obj, mon);

	return result;
}


/**
 * Disparar un objeto del carcaj, la mochila o el suelo a un objetivo.
 */
void do_cmd_fire(struct command *cmd) {
	int dir;
	int range = MIN(6 + 2 * player->state.ammo_mult, z_info->max_range);
	int shots = player->state.num_shots;

	ranged_attack attack = make_ranged_shot;

	struct object *bow = equipped_item_by_slot_name(player, "shooting");
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Disparar qué munición?",
			/* Error  */ "No tienes munición adecuada para disparar.",
			/* Filtro */ obj_can_fire,
			/* Elección */ USE_INVEN | USE_QUIVER | USE_FLOOR | QUIVER_TAGS)
		!= CMD_OK)
		return;

	/* Requerir un lanzador utilizable */
	if (!bow || !player->state.ammo_tval) {
		msg("No tienes nada con qué disparar.");
		return;
	}

	/* Comprobar que el objeto a disparar es utilizable por el jugador. */
	if (!item_is_available(obj)) {
		msg("Ese objeto no está a tu alcance.");
		return;
	}

	/* Comprobar que la munición puede usarse con el lanzador */
	if (obj->tval != player->state.ammo_tval) {
		msg("Esa munición no puede ser disparada por tu arma actual.");
		return;
	}

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, false);
	else
		return;

	ranged_helper(player, obj, dir, range, shots, attack, ranged_hit_types,
				  (int) N_ELEMENTS(ranged_hit_types));
}


/**
 * Lanzar un objeto del carcaj, la mochila, el suelo o, en circunstancias limitadas,
 * el equipo.
 */
void do_cmd_throw(struct command *cmd) {
	int dir;
	int shots = 10;
	int str = adj_str_blow[player->state.stat_ind[STAT_STR]];
	ranged_attack attack = make_ranged_throw;

	int weight;
	int range;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/*
	 * Obtener argumentos. Nunca mostrar el equipo por defecto como primera
	 * lista (ya que lanzar el arma equipada deja esa ranura vacía y habrá que
	 * elegir otra fuente de todas formas).
	 */
	if (player->upkeep->command_wrk == USE_EQUIP)
		player->upkeep->command_wrk = USE_INVEN;
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Lanzar qué objeto?",
			/* Error  */ "No tienes nada que lanzar.",
			/* Filtro */ obj_can_throw,
			/* Elección */ USE_EQUIP | USE_QUIVER | USE_INVEN | USE_FLOOR | SHOW_THROWING)
		!= CMD_OK)
		return;

	if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
		player_confuse_dir(player, &dir, false);
	else
		return;

	if (object_is_equipped(player->body, obj)) {
		assert(obj_can_takeoff(obj) && tval_is_melee_weapon(obj));
		inven_takeoff(obj);
	}

	weight = MAX(object_weight_one(obj), 10);
	range = MIN(((str + 20) * 10) / weight, 10);

	ranged_helper(player, obj, dir, range, shots, attack, ranged_hit_types,
				  (int) N_ELEMENTS(ranged_hit_types));
}

/**
 * Comando frontal que dispara al objetivo más cercano con la munición por defecto.
 */
void do_cmd_fire_at_nearest(void) {
	int i, dir = DIR_TARGET;
	struct object *ammo = NULL;
	struct object *bow = equipped_item_by_slot_name(player, "shooting");

	/* Requerir un lanzador utilizable */
	if (!bow || !player->state.ammo_tval) {
		msg("No tienes nada con qué disparar.");
		return;
	}

	/* Encontrar la primera munición elegible en el carcaj */
	for (i = 0; i < z_info->quiver_size; i++) {
		if (!player->upkeep->quiver[i])
			continue;
		if (player->upkeep->quiver[i]->tval != player->state.ammo_tval)
			continue;
		ammo = player->upkeep->quiver[i];
		break;
	}

	/* Requerir munición utilizable */
	if (!ammo) {
		msg("No tienes munición en el carcaj para disparar.");
		return;
	}

	/* Requerir enemigo */
	if (!target_set_closest((TARGET_KILL | TARGET_QUIET), NULL)) return;

	/* ¡Disparar! */
	cmdq_push(CMD_FIRE);
	cmd_set_arg_item(cmdq_peek(), "item", ammo);
	cmd_set_arg_target(cmdq_peek(), "target", dir);
}