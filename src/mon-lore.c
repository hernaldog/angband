/**
 * \file mon-lore.c
 * \brief Código de memoria de monstruos.
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
#include "effects.h"
#include "game-world.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-init.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-util.h"
#include "obj-gear.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "project.h"
#include "z-textblock.h"

/**
 * Géneros de monstruos
 */
enum monster_sex {
	MON_SEX_NEUTER = 0,
	MON_SEX_MALE,
	MON_SEX_FEMALE,
	MON_SEX_MAX,
};

typedef enum monster_sex monster_sex_t;

/**
 * Determinar el color para codificar un hechizo de monstruo
 *
 * Esta función asigna un color a cada hechizo de monstruo, dependiendo de lo
 * peligroso que sea el ataque para el jugador dado el estado actual. Los hechizos pueden
 * ser coloreados verde (menos peligroso), amarillo, naranja o rojo (más peligroso).
 */
static int spell_color(struct player *p, const struct monster_race *race,
					   int spell_index)
{
	const struct monster_spell *spell = monster_spell_by_index(spell_index);
	struct monster_spell_level *level = spell->level;
	struct effect *eff = spell ? spell->effect : NULL;

	/* Sin hechizo */
	if (!spell) return COLOUR_DARK;

	/* Obtener el nivel correcto */
	while (level->next && race->spell_power >= level->next->power) {
		level = level->next;
	}

	/* Los hechizos irresistibles solo usan el color por defecto */
	if (!level->lore_attr_resist && !level->lore_attr_immune) {
		return level->lore_attr;
	}

	/* Hechizos con tirada de salvación */
	if (level->save_message) {
		/* Resultados mixtos si la salvación puede fallar, resultado perfecto si no puede */
		if (p->known_state.skills[SKILL_SAVE] < 100) {
			if (eff->index == EF_TELEPORT_LEVEL) {
				/* Caso especial - teletransporte de nivel */
				if (p->known_state.el_info[ELEM_NEXUS].res_level > 0) {
					return level->lore_attr_resist;
				} else {
					return level->lore_attr;
				}
			} else if (eff->index == EF_TIMED_INC) {
				/* Efectos temporales simples */
				if (player_inc_check(p, eff->subtype, true)) {
					return level->lore_attr;
				} else {
					return level->lore_attr_resist;
				}
			} else if (level->lore_attr_immune) {
				/* Múltiples efectos temporales más daño */
				for (; eff; eff = eff->next) {
					if (eff->index != EF_TIMED_INC) continue;
					if (player_inc_check(p, eff->subtype, true)) {
						return level->lore_attr;
					}
				}
				return level->lore_attr_resist;
			} else {
				/* Daño directo */
				return level->lore_attr;
			}
		} else if (level->lore_attr_immune) {
			return level->lore_attr_immune;
		} else {
			return level->lore_attr_resist;
		}
	}

	/* Proyectiles, bolas y alientos */
	if ((eff->index == EF_BOLT) || (eff->index == EF_BALL) ||
		(eff->index == EF_BREATH)) {
		/* Tratar por elemento */
		switch (eff->subtype) {
			/* Caso especial - sonido */
			case ELEM_SOUND:
				if (p->known_state.el_info[ELEM_SOUND].res_level > 0) {
					return level->lore_attr_immune;
				} else if (of_has(p->known_state.flags, OF_PROT_STUN)) {
					return level->lore_attr_resist;
				} else {
					return level->lore_attr;
				}
				break;
			/* Caso especial - nexo */
			case ELEM_NEXUS:
				if (p->known_state.el_info[ELEM_NEXUS].res_level > 0) {
					return level->lore_attr_immune;
				} else if (p->known_state.skills[SKILL_SAVE] >= 100) {
					return level->lore_attr_resist;
				} else {
					return level->lore_attr;
				}
				break;
			/* Elementos que aturden o confunden */
			case ELEM_FORCE:
			case ELEM_ICE:
			case ELEM_PLASMA:
			case ELEM_WATER:
				if (!of_has(p->known_state.flags, OF_PROT_STUN)) {
					return level->lore_attr;
				} else if (!of_has(p->known_state.flags, OF_PROT_CONF) &&
						   (eff->subtype == ELEM_WATER)){
					return level->lore_attr;
				} else {
					return level->lore_attr_resist;
				}
				break;
			/* Todos los demás elementos */
			default:
				if (p->known_state.el_info[eff->subtype].res_level == 3) {
					return level->lore_attr_immune;
				} else if (p->known_state.el_info[eff->subtype].res_level > 0) {
					return level->lore_attr_resist;
				} else {
					return level->lore_attr;
				}
		}
	}

	return level->lore_attr;
}

/**
 * Determinar el color para codificar un efecto de golpe cuerpo a cuerpo de monstruo
 *
 * Esta función asigna un color a cada efecto de golpe de monstruo, dependiendo de lo
 * peligroso que sea el ataque para el jugador dado el estado actual. Los golpes pueden
 * ser coloreados verde (menos peligroso), amarillo, naranja o rojo (más peligroso).
 */
static int blow_color(struct player *p, int blow_idx)
{
	const struct blow_effect *blow = &blow_effects[blow_idx];

	/* Algunos golpes solo usan el color por defecto */
	if (!blow->lore_attr_resist && !blow->lore_attr_immune) {
		return blow->lore_attr;
	}

	/* Los efectos con inmunidades son sencillos */
	if (blow->lore_attr_immune) {
		int i;

		for (i = ELEM_ACID; i < ELEM_POIS; i++) {
			if (proj_name_to_idx(blow->name) == i) {
				break;
			}
		}

		if (p->known_state.el_info[i].res_level == 3) {
			return blow->lore_attr_immune;
		} else if (p->known_state.el_info[i].res_level > 0) {
			return blow->lore_attr_resist;
		} else {
			return blow->lore_attr;
		}
	}

	/* Ahora ver qué atributos del jugador pueden proteger de los efectos */
	if (streq(blow->effect_type, "theft")) {
		if (p->lev + adj_dex_safe[p->known_state.stat_ind[STAT_DEX]] >= 100) {
			return blow->lore_attr_resist;
		} else {
			return blow->lore_attr;
		}
	} else if (streq(blow->effect_type, "drain")) {
		int i;
		bool found = false;
		for (i = 0; i < z_info->pack_size; i++) {
			struct object *obj = p->upkeep->inven[i];
			if (obj && tval_can_have_charges(obj) && obj->pval) {
				found = true;
				break;
			}
		}
		if (found) {
			return blow->lore_attr;
		} else {
			return blow->lore_attr_resist;
		}
	} else if (streq(blow->effect_type, "eat-food")) {
		int i;
		bool found = false;
		for (i = 0; i < z_info->pack_size; i++) {
			struct object *obj = p->upkeep->inven[i];
			if (obj && tval_is_edible(obj)) {
				found = true;
				break;
			}
		}
		if (found) {
			return blow->lore_attr;
		} else {
			return blow->lore_attr_resist;
		}
	} else if (streq(blow->effect_type, "eat-light")) {
		int light_slot = slot_by_name(p, "light");
		struct object *obj = slot_object(p, light_slot);
		if (obj && obj->timeout && !of_has(obj->flags, OF_NO_FUEL)) {
			return blow->lore_attr;
		} else {
			return blow->lore_attr_resist;
		}
	} else if (streq(blow->effect_type, "element")) {
		if (p->known_state.el_info[blow->resist].res_level > 0) {
			return blow->lore_attr_resist;
		} else {
			return blow->lore_attr;
		}
	} else if (streq(blow->effect_type, "flag")) {
		if (of_has(p->known_state.flags, blow->resist)) {
			return blow->lore_attr_resist;
		} else {
			return blow->lore_attr;
		}
	} else if (streq(blow->effect_type, "all_sustains")) {
		if (of_has(p->known_state.flags, OF_SUST_STR) &&
			of_has(p->known_state.flags, OF_SUST_INT) &&
			of_has(p->known_state.flags, OF_SUST_WIS) &&
			of_has(p->known_state.flags, OF_SUST_DEX) &&
			of_has(p->known_state.flags, OF_SUST_CON)) {
			return blow->lore_attr_resist;
		} else {
			return blow->lore_attr;
		}
	}

	return blow->lore_attr;
}

void lore_learn_spell_if_has(struct monster_lore *lore, const struct monster_race *race, int flag)
{
	if (rsf_has(race->spell_flags, flag)) {
		rsf_on(lore->spell_flags, flag);
	}
}

void lore_learn_spell_if_visible(struct monster_lore *lore, const struct monster *mon, int flag)
{
	if (monster_is_visible(mon)) {
		rsf_on(lore->spell_flags, flag);
	}
}

void lore_learn_flag_if_visible(struct monster_lore *lore, const struct monster *mon, int flag)
{
	if (monster_is_visible(mon)) {
		rf_on(lore->flags, flag);
	}
}


/**
 * Actualizar qué partes del saber se conocen
 */
void lore_update(const struct monster_race *race, struct monster_lore *lore)
{
	int i;
	bitflag mask[RF_SIZE];

	if (!race || !lore) return;

	/* Asumir algunas banderas "obvias" */
	create_mon_flag_mask(mask, RFT_OBV, RFT_MAX);
	rf_union(lore->flags, mask);

	/* Golpes */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		if (!race->blow) break;
		if (lore->blow_known[i] || lore->blows[i].times_seen ||
			lore->all_known) {
			lore->blow_known[i] = true;
			lore->blows[i].method = race->blow[i].method;
			lore->blows[i].effect = race->blow[i].effect;
			lore->blows[i].dice = race->blow[i].dice;
		}
	}

	/* Matar a un monstruo revela algunas propiedades */
	if ((lore->tkills > 0) || lore->all_known) {
		lore->armour_known = true;
		lore->drop_known = true;
		create_mon_flag_mask(mask, RFT_RACE_A, RFT_RACE_N, RFT_DROP, RFT_MAX);
		rf_union(lore->flags, mask);
		rf_on(lore->flags, RF_FORCE_DEPTH);
	}

	/* Conciencia */
	if ((((int)lore->wake * (int)lore->wake) > race->sleep) ||
	    (lore->ignore == UCHAR_MAX) || lore->all_known ||
	    ((race->sleep == 0) && (lore->tkills >= 10)))
		lore->sleep_known = true;

	/* Frecuencia de lanzamiento de hechizos */
	if (lore->cast_innate > 50 || lore->all_known) {
		lore->innate_freq_known = true;
	}
	if (lore->cast_spell > 50 || lore->all_known) {
		lore->spell_freq_known = true;
	}

	/* Banderas para sondear y hacer trampa */
	if (lore->all_known) {
		rf_setall(lore->flags);
		rsf_copy(lore->spell_flags, race->spell_flags);
	}
}

/**
 * Aprender todo sobre un monstruo.
 *
 * Establece la variable all_known, todas las banderas y todas las banderas de hechizo relevantes.
 */
void cheat_monster_lore(const struct monster_race *race, struct monster_lore *lore)
{
	assert(race);
	assert(lore);

	/* Conocimiento completo */
	lore->all_known = true;
	lore_update(race, lore);
}

/**
 * Olvidar todo sobre un monstruo.
 */
void wipe_monster_lore(const struct monster_race *race, struct monster_lore *lore)
{
	struct monster_blow *blows;
	bool *blow_known;
	struct monster_drop *d;
	struct monster_friends *f;
	struct monster_friends_base *fb;
	struct monster_mimic *mk;

	assert(race);
	assert(lore);

	d = lore->drops;
	while (d) {
		struct monster_drop *dn = d->next;
		mem_free(d);
		d = dn;
	}
	f = lore->friends;
	while (f) {
		struct monster_friends *fn = f->next;
		mem_free(f);
		f = fn;
	}
	fb = lore->friends_base;
	while (fb) {
		struct monster_friends_base *fbn = fb->next;
		mem_free(fb);
		fb = fbn;
	}
	mk = lore->mimic_kinds;
	while (mk) {
		struct monster_mimic *mkn = mk->next;
		mem_free(mk);
		mk = mkn;
	}
	/*
	 * Mantener los punteros blows y blow_known - otro código asume que
	 * no son NULL. Limpiar la memoria a la que apuntan.
	 */
	blows = lore->blows;
	memset(blows, 0, z_info->mon_blows_max * sizeof(*blows));
	blow_known = lore->blow_known;
	memset(blow_known, 0, z_info->mon_blows_max * sizeof(*blow_known));
	memset(lore, 0, sizeof(*lore));
	lore->blows = blows;
	lore->blow_known = blow_known;
}

/**
 * Aprender sobre un monstruo (mediante "sonda")
 */
void lore_do_probe(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	
	lore->all_known = true;
	lore_update(mon->race, lore);

	/* Actualizar la ventana de recuerdo de monstruos */
	if (player->upkeep->monster_race == mon->race)
		player->upkeep->redraw |= (PR_MONSTER);
}

/**
 * Determinar si el monstruo es completamente conocido
 */
bool lore_is_fully_known(const struct monster_race *race)
{
	unsigned i;
	struct monster_lore *lore = get_lore(race);

	/* Comprobar si ya es conocido */
	if (lore->all_known)
		return true;
		
	if (!lore->armour_known)
		return false;
	/* Solo comprobar hechizos si el monstruo puede lanzarlos */
	if (!lore->spell_freq_known && race->freq_innate + race->freq_spell)
		return false;
	if (!lore->drop_known)
		return false;
	if (!lore->sleep_known)
		return false;
		
	/* Comprobar si los golpes son conocidos */
	for (i = 0; i < z_info->mon_blows_max; i++){
		/* Solo comprobar si el golpe existe */
		if (!race->blow[i].method)
			break;
		if (!lore->blow_known[i])
			return false;
		
	}
		
	/* Comprobar todas las banderas */
	for (i = 0; i < RF_SIZE; i++)
		if (!lore->flags[i])
			return false;
		
		
	/* Comprobar banderas de hechizo */
	for (i = 0; i < RSF_SIZE; i++)
		if (lore->spell_flags[i] != race->spell_flags[i])			
			return false;
	
	/* El jugador lo sabe todo */
	lore->all_known = true;
	lore_update(race, lore);
	return true;
}
	
	
/**
 * Tomar nota de que el monstruo dado acaba de soltar algún tesoro
 *
 * Nótese que aprender las banderas "BUENO"/"EXCELENTE" da información
 * sobre el tesoro (incluso cuando el monstruo es asesinado por primera
 * vez, como los únicos, y el tesoro aún no ha sido examinado).
 *
 * Este método "indirecto" se utilizó para evitar que el jugador aprendiera
 * exactamente cuánto tesoro puede soltar un monstruo con solo observar
 * un solo ejemplo de una caída. Este método realmente observa cuánto
 * oro y objetos se dejan caer, y recuerda esa información para ser
 * descrita más tarde por el código de recuerdo de monstruos. Da al jugador la oportunidad
 * de aprender si un monstruo solo deja caer objetos o solo oro.
 */
void lore_treasure(struct monster *mon, int num_item, int num_gold)
{
	struct monster_lore *lore = get_lore(mon->race);

	assert(num_item >= 0);
	assert(num_gold >= 0);

	/* Anotar el número de cosas dejadas */
	if (num_item > lore->drop_item) {
		lore->drop_item = num_item;
	}
	if (num_gold > lore->drop_gold) {
		lore->drop_gold = num_gold;
	}

	/* Aprender sobre la calidad de la caída */
	rf_on(lore->flags, RF_DROP_GOOD);
	rf_on(lore->flags, RF_DROP_GREAT);

	/* Tener la oportunidad de aprender SOLO_OBJETO y SOLO_ORO */
	if (num_item && (lore->drop_gold == 0) && one_in_(4)) {
		rf_on(lore->flags, RF_ONLY_ITEM);
	}
	if (num_gold && (lore->drop_item == 0) && one_in_(4)) {
		rf_on(lore->flags, RF_ONLY_GOLD);
	}

	/* Actualizar la ventana de recuerdo de monstruos */
	if (player->upkeep->monster_race == mon->race) {
		player->upkeep->redraw |= (PR_MONSTER);
	}
}

/**
 * Copia en `flags` las banderas de la raza de monstruo dada que son conocidas
 * por la estructura lore dada (normalmente el conocimiento del jugador).
 *
 * Las banderas conocidas serán 1 si están presentes, o 0 si no lo están. Las banderas
 * desconocidas siempre serán 0.
 */
void monster_flags_known(const struct monster_race *race,
						 const struct monster_lore *lore,
						 bitflag flags[RF_SIZE])
{
	rf_copy(flags, race->flags);
	rf_inter(flags, lore->flags);
}

/**
 * Devolver una descripción para el valor de conciencia de la raza de monstruo dada.
 *
 * Las descripciones están en una tabla dentro de la función. Devuelve una cadena sensata
 * para valores no incluidos en la tabla.
 *
 * \param awareness es el contador de inactividad de la raza (monster_race.sleep).
 */
static const char *lore_describe_awareness(int16_t awareness)
{
	/* Tabla de valores ordenada descendente, por prioridad. El terminador es
	 * {SHRT_MAX, NULL}. */
	static const struct lore_awareness {
		int16_t threshold;
		const char *description;
	} lore_awareness_description[] = {
		{200,	"prefiere ignorar"},
		{95,	"presta muy poca atención a"},
		{75,	"presta poca atención a"},
		{45,	"suele pasar por alto"},
		{25,	"tarda bastante en ver"},
		{10,	"tarda un tiempo en ver"},
		{5,		"es bastante observador de"},
		{3,		"es observador de"},
		{1,		"es muy observador de"},
		{0,		"está vigilante por"},
		{SHRT_MAX,	NULL},
	};
	const struct lore_awareness *current = lore_awareness_description;

	while (current->threshold != SHRT_MAX && current->description != NULL) {
		if (awareness > current->threshold)
			return current->description;

		current++;
	}

	/* Los valores cero y menos son los más vigilantes */
	return "está siempre vigilante por";
}

/**
 * Devolver una descripción para el valor de velocidad de la raza de monstruo dada.
 *
 * Las descripciones están en una tabla dentro de la función. Devuelve una cadena sensata
 * para valores no incluidos en la tabla.
 *
 * \param speed es la velocidad de la raza (monster_race.speed).
 */
static const char *lore_describe_speed(uint8_t speed)
{
	/* Tabla de valores ordenada descendente, por prioridad. El terminador es
	 * {UCHAR_MAX, NULL}. */
	static const struct lore_speed {
		uint8_t threshold;
		const char *description;
	} lore_speed_description[] = {
		{130,	"increíblemente rápido"},
		{120,	"muy rápido"},
		{115,	"rápido"},
		{110,	"bastante rápido"},
		{109,	"a velocidad normal"}, /* 110 es velocidad normal */
		{99,	"lento"},
		{89,	"muy lento"},
		{0,		"increíblemente lento"},
		{UCHAR_MAX,	NULL},
	};
	const struct lore_speed *current = lore_speed_description;

	while (current->threshold != UCHAR_MAX && current->description != NULL) {
		if (speed > current->threshold)
			return current->description;

		current++;
	}

	/* Devolver una descripción extraña, ya que el valor no se encontró en la tabla */
	return "erróneamente";
}

/**
 * Añadir la velocidad del monstruo, en palabras, a un textblock.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 */
static void lore_adjective_speed(textblock *tb, const struct monster_race *race)
{
	/* "a" está separado de la descripción normal de velocidad para usar el
	 * color de texto normal */
	if (race->speed == 110)
		textblock_append(tb, "a ");

	textblock_append_c(tb, COLOUR_GREEN, "%s", lore_describe_speed(race->speed));
}

/**
 * Añadir la velocidad del monstruo, en multiplicadores, a un textblock.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 */
static void lore_multiplier_speed(textblock *tb, const struct monster_race *race)
{
	// se mueve a 2.3x la velocidad normal (0.9x tu velocidad actual)
	textblock_append(tb, "a ");

	char buf[8] = "";
	int multiplier = 10 * extract_energy[race->speed] / extract_energy[110];
	uint8_t int_mul = multiplier / 10;
	uint8_t dec_mul = multiplier % 10;
	uint8_t attr = COLOUR_ORANGE;

	strnfmt(buf, sizeof(buf), "%d.%dx", int_mul, dec_mul);
	textblock_append_c(tb, COLOUR_L_BLUE, "%s", buf);

	textblock_append(tb, " la velocidad normal, que es ");
	multiplier = 100 * extract_energy[race->speed]
		/ extract_energy[player->state.speed];
	int_mul = multiplier / 100;
	dec_mul = multiplier % 100;
	if (!dec_mul) {
		strnfmt(buf, sizeof(buf), "%dx", int_mul);
	} else if (!(dec_mul % 10)) {
		strnfmt(buf, sizeof(buf), "%d.%dx", int_mul, dec_mul / 10);
	} else {
		strnfmt(buf, sizeof(buf), "%d.%02dx", int_mul, dec_mul);
	}

	if (player->state.speed > race->speed) {
		attr = COLOUR_L_GREEN;
	} else if (player->state.speed < race->speed) {
		attr = COLOUR_RED;
	}
	if (player->state.speed == race->speed) {
		textblock_append(tb, "la misma que la tuya");
	} else {
		textblock_append_c(tb, attr, "%s", buf);
		textblock_append(tb, " tu velocidad");
	}
}

/**
 * Devolver un valor que describe el sexo de la raza de monstruo proporcionada.
 */
static monster_sex_t lore_monster_sex(const struct monster_race *race)
{
	if (rf_has(race->flags, RF_FEMALE))
		return MON_SEX_FEMALE;
	else if (rf_has(race->flags, RF_MALE))
		return MON_SEX_MALE;

	return MON_SEX_NEUTER;
}

/**
 * Devolver un pronombre para un monstruo; usado como sujeto de una oración.
 *
 * Las descripciones están en una tabla dentro de la función. La tabla debe coincidir
 * con los valores de monster_sex_t.
 *
 * \param sex es el valor de género (como lo proporciona `lore_monster_sex()`.
 * \param title_case indica si la letra inicial debe ir en mayúscula;
 * `true` es mayúscula, `false` no lo es.
 */
static const char *lore_pronoun_nominative(monster_sex_t sex, bool title_case)
{
	static const char *lore_pronouns[MON_SEX_MAX][2] = {
		{"ello", "Ello"},
		{"él", "Él"},
		{"ella", "Ella"},
	};

	int pronoun_index = MON_SEX_NEUTER, case_index = 0;

	if (sex < MON_SEX_MAX)
		pronoun_index = sex;

	if (title_case)
		case_index = 1;

	return lore_pronouns[pronoun_index][case_index];
}

/**
 * Devolver un pronombre posesivo para un monstruo.
 *
 * Las descripciones están en una tabla dentro de la función. La tabla debe coincidir
 * con los valores de monster_sex_t.
 *
 * \param sex es el valor de género (como lo proporciona `lore_monster_sex()`.
 * \param title_case indica si la letra inicial debe ir en mayúscula;
 * `true` es mayúscula, `false` no lo es.
 */
static const char *lore_pronoun_possessive(monster_sex_t sex, bool title_case)
{
	static const char *lore_pronouns[MON_SEX_MAX][2] = {
		{"su", "Su"},
		{"su", "Su"},
		{"su", "Su"},
	};

	int pronoun_index = MON_SEX_NEUTER, case_index = 0;

	if (sex < MON_SEX_MAX)
		pronoun_index = sex;

	if (title_case)
		case_index = 1;

	return lore_pronouns[pronoun_index][case_index];
}

/**
 * Añadir una cláusula que contiene una lista de descripciones de banderas de monstruo de
 * list-mon-race-flags.h a un textblock.
 *
 * El texto que une la lista se dibuja con los atributos por defecto. La lista
 * utiliza una coma serial ("a, b, c, y d").
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param f es el conjunto de banderas a describir.
 * \param attr es el atributo con el que se dibujará cada elemento de la lista.
 * \param start es una cadena para comenzar la cláusula.
 * \param conjunction es una cadena que se añade antes del último elemento.
 * \param end es una cadena que se añade después del último elemento.
 */
static void lore_append_clause(textblock *tb, bitflag *f, uint8_t attr,
							   const char *start, const char *conjunction,
							   const char *end)
{
	int count = rf_count(f);
	bool comma = count > 2;

	if (count) {
		int flag;
		textblock_append(tb, "%s", start);
		for (flag = rf_next(f, FLAG_START); flag; flag = rf_next(f, flag + 1)) {
			/* La primera entrada comienza inmediatamente */
			if (flag != rf_next(f, FLAG_START)) {
				if (comma) {
					textblock_append(tb, ",");
				}
				/* Última entrada */
				if (rf_next(f, flag + 1) == FLAG_END) {
					textblock_append(tb, " ");
					textblock_append(tb, "%s", conjunction);
				}
				textblock_append(tb, " ");
			}
			textblock_append_c(tb, attr, "%s", describe_race_flag(flag));
		}
		textblock_append(tb, "%s", end);
	}
}


/**
 * Añadir una lista de descripciones de hechizos.
 *
 * Esta es una versión modificada de `lore_append_clause()` para formatear hechizos.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param f es el conjunto de banderas a describir.
 * \param know_hp es si el jugador conoce la CA del monstruo.
 * \param race es la raza de monstruo.
 * \param conjunction es una cadena que se añade antes del último elemento.
 * \param end es una cadena que se añade después del último elemento.
 */
static void lore_append_spell_clause(textblock *tb, bitflag *f, bool know_hp,
									 const struct monster_race *race,
									 const char *conjunction,
									 const char *end)
{
	int count = rsf_count(f);
	bool comma = count > 2;

	if (count) {
		int spell;
		for (spell = rsf_next(f, FLAG_START); spell;
			 spell = rsf_next(f, spell + 1)) {
			int color = spell_color(player, race, spell);
			int damage = mon_spell_lore_damage(spell, race, know_hp);

			/* La primera entrada comienza inmediatamente */
			if (spell != rsf_next(f, FLAG_START)) {
				if (comma) {
					textblock_append(tb, ",");
				}
				/* Última entrada */
				if (rsf_next(f, spell + 1) == FLAG_END) {
					textblock_append(tb, " ");
					textblock_append(tb, "%s", conjunction);
				}
				textblock_append(tb, " ");
			}
			textblock_append_c(tb, color, "%s",
							   mon_spell_lore_description(spell, race));
			if (damage > 0) {
				textblock_append_c(tb, color, " (%d)", damage);
			}
		}
		textblock_append(tb, "%s", end);
	}
}

/**
 * Añadir el historial de muertes a un textblock para una raza de monstruo dada.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_kills(textblock *tb, const struct monster_race *race,
					   const struct monster_lore *lore,
					   const bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;
	bool out = true;

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* Tratar según si es único, luego según si tienen muertes de jugador */
	if (rf_has(known_flags, RF_UNIQUE)) {
		/* Determinar si el único está "muerto" */
		bool dead = (race->max_num == 0) ? true : false;

		/* Hemos sido asesinados... */
		if (lore->deaths) {
			/* Antepasados asesinados */
			textblock_append(tb, "%s ha matado a %d de tus antepasados",
							 lore_pronoun_nominative(msex, true), lore->deaths);

			/* Pero también lo hemos matado */
			if (dead)
				textblock_append(tb, ", ¡pero te has vengado!  ");

			/* Sin vengar (nunca) */
			else
				textblock_append(tb, ", que %s sin vengar.  ",
								 VERB_AGREEMENT(lore->deaths, "permanece",
												"permanecen"));
		} else if (dead) { /* Único muerto que nunca nos hizo daño */
			textblock_append(tb, "Has matado a este enemigo.  ");
		} else {
			/* Vivo y nunca nos mató */
			out = false;
		}
	} else if (lore->deaths) {
		/* Antepasados muertos */
		textblock_append(tb, "%d de tus antepasados %s sido asesinados por esta criatura, ", lore->deaths, VERB_AGREEMENT(lore->deaths, "ha", "han"));

		if (lore->pkills) {
			/* Algunas muertes en esta vida */
			textblock_append(tb, "y has exterminado al menos %d de las criaturas.  ", lore->pkills);
		} else if (lore->tkills) {
			/* Algunas muertes en vidas pasadas */
			textblock_append(tb, "y tus antepasados han exterminado al menos %d de las criaturas.  ", lore->tkills);
		} else {
			/* Sin muertes */
			textblock_append_c(tb, COLOUR_RED, "y %s no se sabe que haya sido derrotado nunca.  ", lore_pronoun_nominative(msex, false));
		}
	} else {
		if (lore->pkills) {
			/* Mató algunos en esta vida */
			textblock_append(tb, "Has matado al menos %d de estas criaturas.  ", lore->pkills);
		} else if (lore->tkills) {
			/* Mató algunos en vidas pasadas */
			textblock_append(tb, "Tus antepasados han matado al menos %d de estas criaturas.  ", lore->tkills);
		} else {
			/* No mató ninguno */
			textblock_append(tb, "No se recuerdan batallas a muerte.  ");
		}
	}

	/* Separar */
	if (out)
		textblock_append(tb, "\n");
}

/**
 * Añadir la descripción de la raza de monstruo a un textblock.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 */
void lore_append_flavor(textblock *tb, const struct monster_race *race)
{
	assert(tb && race);

	textblock_append(tb, "%s\n", race->text);
}

/**
 * Añadir el tipo de monstruo, ubicación y patrones de movimiento a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_movement(textblock *tb, const struct monster_race *race,
						  const struct monster_lore *lore,
						  bitflag known_flags[RF_SIZE])
{
	int f;
	bitflag flags[RF_SIZE];

	assert(tb && race && lore);

	textblock_append(tb, "Esta");

	/* Obtener adjetivos */
	create_mon_flag_mask(flags, RFT_RACE_A, RFT_MAX);
	rf_inter(flags, race->flags);
	for (f = rf_next(flags, FLAG_START); f; f = rf_next(flags, f + 1)) {
		textblock_append_c(tb, COLOUR_L_BLUE, " %s", describe_race_flag(f));
	}

	/* Obtener sustantivo */
	create_mon_flag_mask(flags, RFT_RACE_N, RFT_MAX);
	rf_inter(flags, race->flags);
	f = rf_next(flags, FLAG_START);
	if (f) {
		textblock_append_c(tb, COLOUR_L_BLUE, " %s", describe_race_flag(f));
	} else {
		textblock_append_c(tb, COLOUR_L_BLUE, " criatura");
	}

	/* Describir ubicación */
	if (race->level == 0) {
		textblock_append(tb, " vive en la ciudad");
	} else {
		uint8_t colour = (race->level > player->max_depth) ?
			COLOUR_RED : COLOUR_L_BLUE;

		if (rf_has(known_flags, RF_FORCE_DEPTH))
			textblock_append(tb, " se encuentra ");
		else
			textblock_append(tb, " normalmente se encuentra ");

		textblock_append(tb, "a profundidades de ");
		textblock_append_c(tb, colour, "%d", race->level * 50);
		textblock_append(tb, " pies (nivel ");
		textblock_append_c(tb, colour, "%d", race->level);
		textblock_append(tb, ")");
	}

	textblock_append(tb, ", y se mueve");

	/* Aleatoriedad */
	if (flags_test(known_flags, RF_SIZE, RF_RAND_50, RF_RAND_25, FLAG_END)) {
		/* Adverbio */
		if (rf_has(known_flags, RF_RAND_50) && rf_has(known_flags, RF_RAND_25))
			textblock_append(tb, " extremadamente");
		else if (rf_has(known_flags, RF_RAND_50))
			textblock_append(tb, " algo");
		else if (rf_has(known_flags, RF_RAND_25))
			textblock_append(tb, " un poco");

		/* Adjetivo */
		textblock_append(tb, " erráticamente");

		/* Conjunción ocasional */
		if (race->speed != 110) textblock_append(tb, ", y");
	}

	/* Velocidad */
	textblock_append(tb, " ");

	if (OPT(player, effective_speed))
		lore_multiplier_speed(tb, race);
	else
		lore_adjective_speed(tb, race);

	/* La descripción de la velocidad también describe la "velocidad de ataque" */
	if (rf_has(known_flags, RF_NEVER_MOVE)) {
		textblock_append(tb, ", pero ");
		textblock_append_c(tb, COLOUR_L_GREEN,
						   "no se digna a perseguir a los intrusos");
	}

	/* Terminar esta oración */
	textblock_append(tb, ".  ");
}

/**
 * Añadir la CA, PG y probabilidad de impacto del monstruo a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_toughness(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;
	struct object *weapon = equipped_item_by_slot_name(player, "weapon");

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* Describir la "resistencia" del monstruo */
	if (lore->armour_known) {
		/* Puntos de golpe */
		textblock_append(tb, "%s tiene una", lore_pronoun_nominative(msex, true));

		if (!rf_has(known_flags, RF_UNIQUE))
			textblock_append(tb, " media");

		textblock_append(tb, " valoración de vida de ");
		textblock_append_c(tb, COLOUR_L_BLUE, "%d", race->avg_hp);

		/* Armadura */
		textblock_append(tb, ", y una valoración de armadura de ");
		textblock_append_c(tb, COLOUR_L_BLUE, "%d", race->ac);
		textblock_append(tb, ".  ");

		/* Probabilidad base del jugador de golpear */
		random_chance c;
		hit_chance(&c, chance_of_melee_hit_base(player, weapon), race->ac);
		int percent = random_chance_scaled(c, 100);

		textblock_append(tb, "Tienes una probabilidad del");
		textblock_append_c(tb, COLOUR_L_BLUE, " %d", percent);
		textblock_append(tb, "%% de golpear a tal criatura en combate cuerpo a cuerpo (si puedes verla).  ");
	}
}

/**
 * Añadir la descripción del valor de experiencia a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_exp(textblock *tb, const struct monster_race *race,
					 const struct monster_lore *lore,
					 bitflag known_flags[RF_SIZE])
{
	const char *ordinal, *article;
	char buf[20] = "";
	long exp_integer, exp_fraction;
	int16_t level;

	/* Verificar legalidad y que este es un monstruo colocable */
	assert(tb && race && lore);
	if (!race->rarity) return;

	/* Introducción */
	if (rf_has(known_flags, RF_UNIQUE))
		textblock_append(tb, "Matar");
	else
		textblock_append(tb, "Una muerte de");

	textblock_append(tb, " esta criatura");

	/* calcular la parte entera de la exp */
	exp_integer = (long)race->mexp * race->level / player->lev;

	/* calcular la parte fraccionaria de la exp escalada por 100, debe usar aritmética
	 * larga para evitar desbordamiento */
	exp_fraction = ((((long)race->mexp * race->level % player->lev) *
					 (long)1000 / player->lev + 5) / 10);

	/* Calcular representación textual */
	strnfmt(buf, sizeof(buf), "%ld", exp_integer);
	if (exp_fraction)
		my_strcat(buf, format(".%02ld", exp_fraction), sizeof(buf));

	/* Mencionar la experiencia */
	textblock_append(tb, " vale ");
	textblock_append_c(tb, COLOUR_BLUE, "%s punto%s", buf,
		PLURAL((exp_integer == 1) && (exp_fraction == 0)));

	/* Tener en cuenta el molesto inglés */
	ordinal = "º";
	level = player->lev % 10;
	if ((player->lev / 10) == 1) /* nada */;
	else if (level == 1) ordinal = "er";
	else if (level == 2) ordinal = "do";
	else if (level == 3) ordinal = "er";
	else if (level == 7) ordinal = "mo";

	/* Tener en cuenta las "vocales iniciales" en los números */
	article = "un";
	level = player->lev;
	if ((level == 8) || (level == 11) || (level == 18)) article = "un";

	/* Mencionar la dependencia del nivel del jugador */
	textblock_append(tb, " para %s personaje de nivel %u%s.  ", article,
					 level, ordinal);
}

/**
 * Añadir la descripción de la caída del monstruo a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_drop(textblock *tb, const struct monster_race *race,
					  const struct monster_lore *lore,
					  bitflag known_flags[RF_SIZE])
{
	int n = 0, nspec = 0;
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);
	if (!lore->drop_known) return;

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* Contar el máximo de caída */
	n = mon_create_drop_count(race, true, false, &nspec);

	/* Suelta oro y/u objetos */
	if (n > 0 || nspec > 0) {
		textblock_append(tb, "%s puede llevar",
			lore_pronoun_nominative(msex, true));

		/* Informar de caídas generales */
		if (n > 0) {
			bool only_item = rf_has(known_flags, RF_ONLY_ITEM);
			bool only_gold = rf_has(known_flags, RF_ONLY_GOLD);

			if (n == 1) {
				textblock_append_c(tb, COLOUR_BLUE,
					" un único ");
			} else if (n == 2) {
				textblock_append_c(tb, COLOUR_BLUE,
					" uno o dos ");
			} else {
				textblock_append(tb, " hasta ");
				textblock_append_c(tb, COLOUR_BLUE, "%d ", n);
			}

			/* Calidad */
			if (rf_has(known_flags, RF_DROP_GREAT)) {
				textblock_append_c(tb, COLOUR_BLUE,
					"excepcional ");
			} else if (rf_has(known_flags, RF_DROP_GOOD)) {
				textblock_append_c(tb, COLOUR_BLUE, "buen ");
			}

			/* Objetos o tesoros */
			if (only_item && only_gold) {
				textblock_append_c(tb, COLOUR_BLUE,
					"error%s", PLURAL(n));
			} else if (only_item && !only_gold) {
				textblock_append_c(tb, COLOUR_BLUE,
					"objeto%s", PLURAL(n));
			} else if (!only_item && only_gold) {
				textblock_append_c(tb, COLOUR_BLUE,
					"tesoro%s", PLURAL(n));
			} else if (!only_item && !only_gold) {
				textblock_append_c(tb, COLOUR_BLUE,
					"objeto%s o tesoro%s",
					PLURAL(n), PLURAL(n));
			}
		}

		/*
		 * Informar de caídas específicas (solo número máximo, sin tipos,
		 * no incluye artefactos de misión).
		 */
		if (nspec > 0) {
			if (n > 0) {
				textblock_append(tb, " y");
			}
			if (nspec == 1) {
				textblock_append(tb, " un único");
			} else if (nspec == 2) {
				textblock_append(tb, " uno o dos");
			} else {
				textblock_append(tb, " hasta");
				textblock_append_c(tb, COLOUR_BLUE, " %d",
					nspec);
			}
			textblock_append(tb, " objetos específicos");
		}

		textblock_append(tb, ".  ");
	}
}

/**
 * Añadir las habilidades del monstruo (resistencias, debilidades, otros rasgos) a un
 * textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia. Nótese las macros
 * que se utilizan para simplificar el código.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_abilities(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	int flag;
	char start[40];
	const char *initial_pronoun;
	bool prev = false;
	bitflag current_flags[RF_SIZE], test_flags[RF_SIZE];
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) y obtener un pronombre para el inicio de
	 * las oraciones */
	msex = lore_monster_sex(race);
	initial_pronoun = lore_pronoun_nominative(msex, true);

	/* Describir habilidades que moldean el entorno. */
	create_mon_flag_mask(current_flags, RFT_ALTER, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s puede ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_WHITE, start, "y", ".  ");

	/* Describir rasgos de detección */
	create_mon_flag_mask(current_flags, RFT_DET, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s es ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_WHITE, start, "y", ".  ");

	/* Describir cosas especiales */
	if (rf_has(known_flags, RF_UNAWARE))
		textblock_append(tb, "%s se disfraza de otra cosa.  ",
						 initial_pronoun);
	if (rf_has(known_flags, RF_MULTIPLY))
		textblock_append_c(tb, COLOUR_ORANGE, "%s se reproduce explosivamente.  ",
						   initial_pronoun);
	if (rf_has(known_flags, RF_REGENERATE))
		textblock_append(tb, "%s se regenera rápidamente.  ", initial_pronoun);

	/* Describir luz */
	if (race->light > 1) {
		textblock_append(tb, "%s ilumina %s alrededores.  ",
						 initial_pronoun, lore_pronoun_possessive(msex, false));
	} else if (race->light == 1) {
		textblock_append(tb, "%s está iluminado.  ", initial_pronoun);
	} else if (race->light == -1) {
		textblock_append(tb, "%s está oscurecido.  ", initial_pronoun);
	} else if (race->light < -1) {
		textblock_append(tb, "%s envuelve %s alrededores en oscuridad.  ",
						 initial_pronoun, lore_pronoun_possessive(msex, false));
	}

	/* Recoger susceptibilidades */
	create_mon_flag_mask(current_flags, RFT_VULN, RFT_VULN_I, RFT_MAX);
	rf_inter(current_flags, known_flags);
	strnfmt(start, sizeof(start), "%s es herido por ", initial_pronoun);
	lore_append_clause(tb, current_flags, COLOUR_VIOLET, start, "y", "");
	if (!rf_is_empty(current_flags)) {
		prev = true;
	}

	/* Recoger inmunidades y resistencias */
	create_mon_flag_mask(current_flags, RFT_RES, RFT_MAX);
	rf_inter(current_flags, known_flags);

	/* Notar la falta de vulnerabilidad como una resistencia */
	create_mon_flag_mask(test_flags, RFT_VULN, RFT_MAX);
	for (flag = rf_next(test_flags, FLAG_START); flag;
		 flag = rf_next(test_flags, flag + 1)) {
		if (rf_has(lore->flags, flag) && !rf_has(known_flags, flag)) {
			rf_on(current_flags, flag);
		}
	}
	if (prev) {
		my_strcpy(start, ", pero resiste ", sizeof(start));
	} else {
		strnfmt(start, sizeof(start), "%s resiste ", initial_pronoun);
	}
	lore_append_clause(tb, current_flags, COLOUR_L_UMBER, start, "y", "");
	if (!rf_is_empty(current_flags)) {
		prev = true;
	}

	/* Recoger susceptibilidades conocidas pero promedio */
	rf_wipe(current_flags);
	create_mon_flag_mask(test_flags, RFT_RES, RFT_MAX);
	for (flag = rf_next(test_flags, FLAG_START); flag;
		 flag = rf_next(test_flags, flag + 1)) {
		if (rf_has(lore->flags, flag) && !rf_has(known_flags, flag)) {
			rf_on(current_flags, flag);
		}
	}

	/* Las vulnerabilidades deben eliminarse específicamente */
	create_mon_flag_mask(test_flags, RFT_VULN_I, RFT_MAX);
	rf_inter(test_flags, known_flags);
	for (flag = rf_next(test_flags, FLAG_START); flag;
		 flag = rf_next(test_flags, flag + 1)) {
		int susc_flag;
		for (susc_flag = rf_next(current_flags, FLAG_START); susc_flag;
			 susc_flag = rf_next(current_flags, susc_flag + 1)) {
			if (streq(describe_race_flag(flag), describe_race_flag(susc_flag)))
				rf_off(current_flags, susc_flag);
		}
	}
	if (prev) {
		my_strcpy(start, ", y no resiste ", sizeof(start));
	} else {
		strnfmt(start, sizeof(start), "%s no resiste ",
			initial_pronoun);
	}

	/* Caso especial para no muertos */
	if (rf_has(known_flags, RF_UNDEAD)) {
		rf_off(current_flags, RF_IM_NETHER);
	}

	lore_append_clause(tb, current_flags, COLOUR_L_UMBER, start, "o", "");
	if (!rf_is_empty(current_flags)) {
		prev = true;
	}

	/* Recoger no-efectos */
	create_mon_flag_mask(current_flags, RFT_PROT, RFT_MAX);
	rf_inter(current_flags, known_flags);
	if (prev) {
		my_strcpy(start, ", y no puede ser ", sizeof(start));
	} else {
		strnfmt(start, sizeof(start), "%s no puede ser ", initial_pronoun);
	}
	lore_append_clause(tb, current_flags, COLOUR_L_UMBER, start, "o", "");
	if (!rf_is_empty(current_flags)) {
		prev = true;
	}

	if (prev)
		textblock_append(tb, ".  ");
}

/**
 * Añadir cómo reacciona el monstruo a los intrusos y a qué distancia lo hace.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_awareness(textblock *tb, const struct monster_race *race,
						   const struct monster_lore *lore,
						   bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* ¿Sabemos lo consciente que es? */
	if (lore->sleep_known)
	{
		const char *aware = lore_describe_awareness(race->sleep);
		textblock_append(tb, "%s %s a los intrusos, y puede notarlos desde ",
						 lore_pronoun_nominative(msex, true), aware);
		textblock_append_c(tb, COLOUR_L_BLUE, "%d", 10 * race->hearing);
		textblock_append(tb, " pies.  ");
	}
}

/**
 * Añadir información sobre con qué otras razas aparece el monstruo y si
 * trabajan juntos.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_friends(textblock *tb, const struct monster_race *race,
						 const struct monster_lore *lore,
						 bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* Describir compañeros */
	if (race->friends || race->friends_base) {
		textblock_append(tb, "%s puede aparecer con otros monstruos",
						 lore_pronoun_nominative(msex, true));
		if (rf_has(known_flags, RF_GROUP_AI))
			textblock_append(tb, " y caza en manada");
		textblock_append(tb, ".  ");
	}
}

/**
 * Añadir los hechizos de ataque del monstruo a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia. Nótese las macros
 * que se utilizan para simplificar el código.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_spells(textblock *tb, const struct monster_race *race,
						const struct monster_lore *lore,
						bitflag known_flags[RF_SIZE])
{
	monster_sex_t msex = MON_SEX_NEUTER;
	bool innate = false;
	bool breath = false;
	const char *initial_pronoun;
	bool know_hp;
	bitflag current_flags[RSF_SIZE], test_flags[RSF_SIZE];
	const struct monster_race *old_ref;

	assert(tb && race && lore);

	/* Establecer la raza para expresiones en los hechizos. */
	old_ref = ref_race;
	ref_race = race;

	know_hp = lore->armour_known;

	/* Extraer un género (si corresponde) y obtener un pronombre para el inicio de
	 * las oraciones */
	msex = lore_monster_sex(race);
	initial_pronoun = lore_pronoun_nominative(msex, true);

	/* Recoger ataques innatos (no aliento) */
	create_mon_spell_mask(current_flags, RST_INNATE, RST_NONE);
	rsf_inter(current_flags, lore->spell_flags);
	create_mon_spell_mask(test_flags, RST_BREATH, RST_NONE);
	rsf_diff(current_flags, test_flags);
	if (!rsf_is_empty(current_flags)) {
		textblock_append(tb, "%s puede ", initial_pronoun);
		lore_append_spell_clause(tb, current_flags, know_hp, race, "o", "");
		innate = true;
	}

	/* Recoger alientos */
	create_mon_spell_mask(current_flags, RST_BREATH, RST_NONE);
	rsf_inter(current_flags, lore->spell_flags);
	if (!rsf_is_empty(current_flags)) {
		if (innate) {
			textblock_append(tb, ", y puede ");
		} else {
			textblock_append(tb, "%s puede ", initial_pronoun);
		}
		textblock_append_c(tb, COLOUR_L_RED, "exhalar ");
		lore_append_spell_clause(tb, current_flags, know_hp, race, "o", "");
		breath = true;
	}

	/* Terminar la oración sobre hechizos innatos y alientos */
	if ((innate || breath) && race->freq_innate) {
		if (lore->innate_freq_known) {
			/* Describir la frecuencia de hechizos */
			textblock_append(tb, "; ");
			textblock_append_c(tb, COLOUR_L_GREEN, "1");
			textblock_append(tb, " vez cada ");
			textblock_append_c(tb, COLOUR_L_GREEN, "%d",
							   100 / race->freq_innate);
		} else if (lore->cast_innate) {
			/* Suponer la frecuencia */
			int approx_frequency = MAX(((race->freq_innate + 9) / 10) * 10, 1);
			textblock_append(tb, "; aproximadamente ");
			textblock_append_c(tb, COLOUR_L_GREEN, "1");
			textblock_append(tb, " vez cada ");
			textblock_append_c(tb, COLOUR_L_GREEN, "%d",
							   100 / approx_frequency);
		}

		textblock_append(tb, ".  ");
	}

	/* Recoger información de hechizos */
	rsf_copy(current_flags, lore->spell_flags);
	create_mon_spell_mask(test_flags, RST_BREATH, RST_INNATE, RST_NONE);
	rsf_diff(current_flags, test_flags);
	if (!rsf_is_empty(current_flags)) {
		/* Introducción */
		textblock_append(tb, "%s puede ", initial_pronoun);

		/* Frase verbal */
		textblock_append_c(tb, COLOUR_L_RED, "lanzar hechizos");

		/* Adverbio */
		if (rf_has(known_flags, RF_SMART))
			textblock_append(tb, " inteligentemente");

		/* Lista */
		textblock_append(tb, " que ");
		lore_append_spell_clause(tb, current_flags, know_hp, race, "o", "");

		/* Terminar la oración sobre hechizos innatos/otros */
		if (race->freq_spell) {
			if (lore->spell_freq_known) {
				/* Describir la frecuencia de hechizos */
				textblock_append(tb, "; ");
				textblock_append_c(tb, COLOUR_L_GREEN, "1");
				textblock_append(tb, " vez cada ");
				textblock_append_c(tb, COLOUR_L_GREEN, "%d",
								   100 / race->freq_spell);
			} else if (lore->cast_spell) {
				/* Suponer la frecuencia */
				int approx_frequency = MAX(((race->freq_spell + 9) / 10) * 10,
										   1);
				textblock_append(tb, "; aproximadamente ");
				textblock_append_c(tb, COLOUR_L_GREEN, "1");
				textblock_append(tb, " vez cada ");
				textblock_append_c(tb, COLOUR_L_GREEN, "%d",
								   100 / approx_frequency);
			}
		}

		textblock_append(tb, ".  ");
	}

	/* Restaurar la referencia anterior. */
	ref_race = old_ref;
}

/**
 * Añadir los ataques cuerpo a cuerpo del monstruo a un textblock.
 *
 * Las banderas de raza conocidas se pasan por simplicidad/eficiencia.
 *
 * \param tb es el textblock al que estamos añadiendo.
 * \param race es la raza de monstruo que estamos describiendo.
 * \param lore es la información conocida sobre la raza de monstruo.
 * \param known_flags es el campo de bits preprocesado de las banderas de raza conocidas por el
 *        jugador.
 */
void lore_append_attack(textblock *tb, const struct monster_race *race,
						const struct monster_lore *lore,
						bitflag known_flags[RF_SIZE])
{
	int i, known_attacks, total_attacks, described_count, total_centidamage;
	monster_sex_t msex = MON_SEX_NEUTER;

	assert(tb && race && lore);

	/* Extraer un género (si corresponde) */
	msex = lore_monster_sex(race);

	/* Notar la falta de ataques */
	if (rf_has(known_flags, RF_NEVER_BLOW)) {
		textblock_append(tb, "%s no tiene ataques físicos.  ",
						 lore_pronoun_nominative(msex, true));
		return;
	}

	total_attacks = 0;
	known_attacks = 0;

	/* Contar el número de ataques definidos y conocidos */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		/* Saltar no-ataques */
		if (!race->blow[i].method) continue;

		total_attacks++;
		if (lore->blow_known[i])
			known_attacks++;
	}

	/* Describir la falta de conocimiento */
	if (known_attacks == 0) {
		textblock_append_c(tb, COLOUR_ORANGE, "No se sabe nada sobre el ataque de %s.  ",
						 lore_pronoun_possessive(msex, false));
		return;
	}

	described_count = 0;
	total_centidamage = 99; // redondear el resultado final al punto entero superior

	/* Describir cada ataque cuerpo a cuerpo */
	for (i = 0; i < z_info->mon_blows_max; i++) {
		random_value dice;
		const char *effect_str = NULL;

		/* Saltar ataques desconocidos e indefinidos */
		if (!race->blow[i].method || !lore->blow_known[i]) continue;

		/* Extraer la información del ataque */
		dice = race->blow[i].dice;
		effect_str = race->blow[i].effect->desc;

		/* Introducir la descripción del ataque */
		if (described_count == 0)
			textblock_append(tb, "%s puede ",
							 lore_pronoun_nominative(msex, true));
		else if (described_count < known_attacks - 1)
			textblock_append(tb, ", ");
		else
			textblock_append(tb, ", y ");

		/* Describir el método */
		textblock_append(tb, "%s", race->blow[i].method->desc);

		/* Describir el efecto (si lo hay) */
		if (effect_str && strlen(effect_str) > 0) {
			int index = blow_index(race->blow[i].effect->name);
			/* Describir el tipo de ataque */
			textblock_append(tb, " para ");
			textblock_append_c(tb, blow_color(player, index), "%s", effect_str);

			textblock_append(tb, " (");
			/* Describir el daño (si se conoce) */
			if (dice.base || (dice.dice && dice.sides) || dice.m_bonus) {
				if (dice.base)
					textblock_append_c(tb, COLOUR_L_GREEN, "%d", dice.base);

				if (dice.dice && dice.sides)
					textblock_append_c(tb, COLOUR_L_GREEN, "%dd%d", dice.dice, dice.sides);

				if (dice.m_bonus)
					textblock_append_c(tb, COLOUR_L_GREEN, "M%d", dice.m_bonus);

				textblock_append(tb, ", ");
			}

			/* Describir las probabilidades de golpear */
			random_chance c;
			hit_chance(&c, chance_of_monster_hit_base(race, race->blow[i].effect),
				player->state.ac + player->state.to_a);
			int percent = random_chance_scaled(c, 100);
			textblock_append_c(tb, COLOUR_L_BLUE, "%d", percent);
			textblock_append(tb, "%%)");

			total_centidamage += (percent * randcalc(dice, 0, AVERAGE));
		}

		described_count++;
	}

	textblock_append(tb, ", con un promedio de");
	if (known_attacks < total_attacks) {
		textblock_append_c(tb, COLOUR_ORANGE, " al menos");
	}
	textblock_append_c(tb, COLOUR_L_GREEN, " %d", total_centidamage/100);
	textblock_append(tb, " de daño en cada uno de los turnos de %s.  ",
					 lore_pronoun_possessive(msex, false));
}

/**
 * Obtener el registro de saber para esta raza de monstruo.
 */
struct monster_lore *get_lore(const struct monster_race *race)
{
	assert(race);
	return &l_list[race->ridx];
}


/**
 * Escribir las entradas de saber de monstruos
 */
static void write_lore_entries(ang_file *fff)
{
	int i, n;

	for (i = 0; i < z_info->r_max; i++) {
		/* Entrada actual */
		struct monster_race *race = &r_info[i];
		struct monster_lore *lore = &l_list[i];

		/* Ignorar monstruos inexistentes o no vistos */
		if (!race->name) continue;
		if (!lore->sights && !lore->all_known) continue;

		/* Salida 'name' */
		file_putf(fff, "name:%s\n", race->name);

		/* Salida base si estamos recordando todo */
		if (lore->all_known)
			file_putf(fff, "base:%s\n", race->base->name);

		/* Salida de recuentos */
		file_putf(fff, "counts:%d:%d:%d:%d:%d:%d:%d\n", lore->sights,
				  lore->deaths, lore->tkills, lore->wake, lore->ignore,
				  lore->cast_innate, lore->cast_spell);

		/* Salida de golpes (hasta el máximo de golpes) */
		for (n = 0; n < z_info->mon_blows_max; n++) {
			/* Fin de los golpes */
			if (!lore->blow_known[n] && !lore->all_known) continue;
			if (!lore->blows[n].method) continue;

			/* Salida del método del golpe */
			file_putf(fff, "blow:%s", lore->blows[n].method->name);

			/* Salida del efecto del golpe (puede ser ninguno) */
			file_putf(fff, ":%s", lore->blows[n].effect->name);

			/* Salida del daño del golpe (puede ser 0) */
			file_putf(fff, ":%d+%dd%dM%d", lore->blows[n].dice.base,
					lore->blows[n].dice.dice,
					lore->blows[n].dice.sides,
					lore->blows[n].dice.m_bonus);

			/* Salida del número de veces que se ha visto ese golpe */
			file_putf(fff, ":%d", lore->blows[n].times_seen);

			/* Salida del índice del golpe */
			file_putf(fff, ":%d", n);

			/* Fin de línea */
			file_putf(fff, "\n");
		}

		/* Salida de banderas */
		write_flags(fff, "flags:", lore->flags, RF_SIZE, r_info_flags);

		/* Salida de banderas de hechizo (múltiples líneas) */
		rsf_inter(lore->spell_flags, race->spell_flags);
		write_flags(fff, "spells:", lore->spell_flags, RSF_SIZE,
					r_info_spell_flags);

		/* Salida de 'drop' */
		if (lore->drops) {
			struct monster_drop *drop = lore->drops;
			char name[120] = "";

			while (drop) {
				struct object_kind *kind = drop->kind;

				if (kind) {
					object_short_name(name, sizeof name, kind->name);
					file_putf(fff, "drop:%s:%s:%d:%d:%d\n",
							  tval_find_name(kind->tval), name,
							  drop->percent_chance, drop->min, drop->max);
					drop = drop->next;
				} else {
					file_putf(fff, "drop-base:%s:%d:%d:%d\n",
							  tval_find_name(drop->tval), drop->percent_chance,
							  drop->min, drop->max);
					drop = drop->next;
				}
			}
		}

		/* Salida de 'friends' */
		if (lore->friends) {
			struct monster_friends *f = lore->friends;

			while (f) {
				if (f->role == MON_GROUP_MEMBER) {
					file_putf(fff, "friends:%d:%dd%d:%s\n", f->percent_chance,
							  f->number_dice, f->number_side, f->race->name);
				} else {
					char *role_name = NULL;
					if (f->role == MON_GROUP_SERVANT) {
						role_name = string_make("sirviente");
					} else if (f->role == MON_GROUP_BODYGUARD) {
						role_name = string_make("guardaespaldas");
					}
					file_putf(fff, "friends:%d:%dd%d:%s:%s\n",
							  f->percent_chance, f->number_dice,
							  f->number_side, f->race->name, role_name);
					string_free(role_name);
				}
				f = f->next;
			}
		}

		/* Salida de 'friends-base' */
		if (lore->friends_base) {
			struct monster_friends_base *b = lore->friends_base;

			while (b) {
				if (b->role == MON_GROUP_MEMBER) {
					file_putf(fff, "friends-base:%d:%dd%d:%s\n",
							  b->percent_chance, b->number_dice,
							  b->number_side, b->base->name);
				} else {
					char *role_name = NULL;
					if (b->role == MON_GROUP_SERVANT) {
						role_name = string_make("sirviente");
					} else if (b->role == MON_GROUP_BODYGUARD) {
						role_name = string_make("guardaespaldas");
					}
					file_putf(fff, "friends-base:%d:%dd%d:%s:%s\n",
							  b->percent_chance, b->number_dice,
							  b->number_side, b->base->name, role_name);
					string_free(role_name);
				}
				b = b->next;
			}
		}

		/* Salida de 'mimic' */
		if (lore->mimic_kinds) {
			struct monster_mimic *m = lore->mimic_kinds;
			struct object_kind *kind = m->kind;
			char name[120] = "";

			while (m) {
				object_short_name(name, sizeof name, kind->name);
				file_putf(fff, "mimic:%s:%s\n",
						  tval_find_name(kind->tval), name);
				m = m->next;
			}
		}

		file_putf(fff, "\n");
	}
}


/**
 * Guardar el saber en un archivo en el directorio de usuario.
 *
 * \param name es el nombre del archivo
 *
 * \returns true en caso de éxito, false en caso contrario.
 */
bool lore_save(const char *name)
{
	char path[1024];

	/* Escribir en el directorio de usuario */
	path_build(path, sizeof(path), ANGBAND_DIR_USER, name);

	if (text_lines_to_file(path, write_lore_entries)) {
		msg("Fallo al crear el archivo %s.new", path);
		return false;
	}

	return true;
}