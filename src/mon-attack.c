/**
 * \file mon-attack.c
 * \brief Ataques de monstruos
 *
 * Ataques a distancia de monstruos - elegir un hechizo de ataque o disparo y realizarlo.
 * Ataques cuerpo a cuerpo de monstruos - golpes críticos de monstruos, si un ataque
 * de monstruo golpea, qué sucede cuando un monstruo ataca a un jugador adyacente.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
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
#include "effects.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "obj-knowledge.h"
#include "player-attack.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

/**
 * Este archivo trata sobre los ataques de monstruos (incluyendo hechizos) de la siguiente manera:
 *
 * Dar a los monstruos una selección de ataques/hechizos más inteligente basada en
 * observaciones de ataques previos al jugador, y/o permitiendo
 * que el monstruo "haga trampa" y conozca el estado del jugador.
 *
 * Mantener una idea del estado del jugador, y usar esa información
 * para eliminar ocasionalmente ataques de hechizos "ineficaces". Podríamos
 * también eliminar ataques normales ineficaces, pero no hay razón
 * para que el monstruo haga esto, ya que no obtiene ningún beneficio.
 * Nótese que los monstruos MINDLESS no pueden usar este código.
 * Y los monstruos no-INTELIGENTES solo lo usan parcialmente de forma efectiva.
 *
 * Aprender realmente lo que el jugador resiste, y usar esa información
 * para eliminar ataques o hechizos antes de usarlos.
 */

/**
 * Dados el monstruo, *mon, y la cueva *c, establecer *dist a la distancia al
 * objetivo del monstruo y *grid a la ubicación del objetivo. Tiene en cuenta un señuelo
 * del jugador, si está presente. Tanto dist como grid pueden ser NULL si ese valor no es
 * necesario.
 */
static void monster_get_target_dist_grid(struct monster *mon, int *dist,
										 struct loc *grid)
{
	if (monster_is_decoyed(mon)) {
		struct loc decoy = cave_find_decoy(cave);
		if (dist) {
			*dist = distance(mon->grid, decoy);
		}
		if (grid) {
			*grid = decoy;
		}
	} else {
		if (dist) {
			*dist = mon->cdis;
		}
		if (grid) {
			*grid = player->grid;
		}
	}
}

/**
 * Verificar si un monstruo tiene posibilidad de lanzar un hechizo este turno
 */
static bool monster_can_cast(struct monster *mon, bool innate)
{
	int chance = innate ? mon->race->freq_innate : mon->race->freq_spell;
	int tdist;
	struct loc tgrid;

	monster_get_target_dist_grid(mon, &tdist, &tgrid);

	/* No puede lanzar hechizos cuando es amable */
	if (mflag_has(mon->mflag, MFLAG_NICE)) return false;

	/* No se le permite lanzar hechizos */
	if (!chance) return false;

	/* Es más probable que los monstruos provocados solo ataquen */
	if (player->timed[TMD_TAUNT]) {
		chance /= 2;
	}

	/* Los monstruos en su rango preferido tienen más probabilidades de lanzar */
	if (tdist == mon->best_range) {
		chance *= 2;
	}

	/* Solo hacer hechizos ocasionalmente */
	if (randint0(100) >= chance) return false;

	/* Verificar rango */
	if (tdist > z_info->max_range) return false;

	/* Verificar trayectoria */
	if (!projectable(cave, mon->grid, tgrid, PROJECT_SHORT))
		return false;

	/* Si el objetivo no es el jugador, solo lanzar si el jugador puede presenciarlo */
	if ((tgrid.x != player->grid.x || tgrid.y != player->grid.y) &&
		!square_isview(cave, mon->grid) &&
		!square_isview(cave, tgrid)) {
		struct loc *path = mem_alloc(z_info->max_range * sizeof(*path));
		int npath, ipath;

		npath = project_path(cave, path, z_info->max_range, mon->grid,
			tgrid, PROJECT_SHORT);
		ipath = 0;
		while (1) {
			if (ipath >= npath) {
				/* Ningún punto en la trayectoria visible. No lanzar. */
				mem_free(path);
				return false;
			}
			if (square_isview(cave, path[ipath])) {
				break;
			}
			++ipath;
		}
		mem_free(path);
	}

	return true;
}

/**
 * Eliminar los hechizos "malos" de una lista de hechizos
 */
static void remove_bad_spells(struct monster *mon, bitflag f[RSF_SIZE])
{
	bitflag f2[RSF_SIZE];
	int tdist;

	monster_get_target_dist_grid(mon, &tdist, NULL);

	/* Tomar copia de trabajo de las banderas de hechizo */
	rsf_copy(f2, f);

	/* No curarse si está lleno */
	if (mon->hp >= mon->maxhp) {
		rsf_off(f2, RSF_HEAL);
	}

	/* No curar a otros si no hay heridos */
	if (rsf_has(f2, RSF_HEAL_KIN) && !find_any_nearby_injured_kin(cave, mon)) {
		rsf_off(f2, RSF_HEAL_KIN);
	}

	/* No apresurarse si ya está apresurado con tiempo restante */
	if (mon->m_timed[MON_TMD_FAST] > 10) {
		rsf_off(f2, RSF_HASTE);
	}

	/* No teletransportarse hacia si el jugador ya está al lado */
	if (tdist == 1) {
		rsf_off(f2, RSF_TELE_TO);
		rsf_off(f2, RSF_TELE_SELF_TO);
	}

	/* No usar el efecto de látigo si el jugador está demasiado lejos */
	if (tdist > 2) {
		rsf_off(f2, RSF_WHIP);
	}
	if (tdist > 3) {
		rsf_off(f2, RSF_SPIT);
	}

	/* Actualizar el conocimiento adquirido */
	if (OPT(player, birth_ai_learn)) {
		size_t i;
		bitflag ai_flags[OF_SIZE], ai_pflags[PF_SIZE];
		struct element_info el[ELEM_MAX];
		bool know_something = false;

		/* Olvidar el estado del jugador ocasionalmente */
		if (one_in_(20)) {
			of_wipe(mon->known_pstate.flags);
			pf_wipe(mon->known_pstate.pflags);
			for (i = 0; i < ELEM_MAX; i++)
				mon->known_pstate.el_info[i].res_level = 0;
		}

		/* Usar la información memorizada */
		of_wipe(ai_flags);
		pf_wipe(ai_pflags);
		of_copy(ai_flags, mon->known_pstate.flags);
		pf_copy(ai_pflags, mon->known_pstate.pflags);
		if (!of_is_empty(ai_flags) || !pf_is_empty(ai_pflags)) {
			know_something = true;
		}

		for (i = 0; i < ELEM_MAX; i++) {
			el[i].res_level = mon->known_pstate.el_info[i].res_level;
			if (el[i].res_level != 0) {
				know_something = true;
			}
		}

		/* Cancelar ciertas banderas basadas en el conocimiento */
		if (know_something) {
			unset_spells(f2, ai_flags, ai_pflags, el, mon);
		}
	}

	/* Usar copia de trabajo de las banderas de hechizo */
	rsf_copy(f, f2);
}


/**
 * Determinar si hay un espacio cerca del lugar seleccionado en el que
 * pueda aparecer una criatura invocada
 */
static bool summon_possible(struct loc grid)
{
	int y, x;

	/* Sin invocaciones en niveles de arena */
	if (player->upkeep->arena_level) return false;

	/* Comenzar en la ubicación y verificar 2 casillas en cada dirección */
	for (y = grid.y - 2; y <= grid.y + 2; y++) {
		for (x = grid.x - 2; x <= grid.x + 2; x++) {
			struct loc near = loc(x, y);

			/* Ignorar ubicaciones ilegales */
			if (!square_in_bounds(cave, near)) continue;

			/* Solo verificar un área circular */
			if (distance(grid, near) > 2) continue;

			/* Truco: no invocar sobre glifo de protección */
			if (square_iswarded(cave, near)) continue;

			/* Si es una casilla de suelo vacía en la línea de visión, estamos bien */
			if (square_isempty(cave, near) && los(cave, grid, near))
				return (true);
		}
	}

	return false;
}


/**
 * Hacer que un monstruo elija un hechizo para lanzar.
 *
 * Nótese que la lista de hechizos del monstruo ya ha tenido los hechizos "inútiles"
 * (rayos que no golpearán al jugador, invocaciones sin espacio, etc.) eliminados.
 * Quizás eso debería hacerlo esta función.
 *
 * Los monstruos estúpidos simplemente elegirán un hechizo al azar. Los monstruos inteligentes
 * elegirán de manera más "inteligente".
 *
 * Esta función podría ser un cuello de botella de eficiencia.
 */
int choose_attack_spell(bitflag *f, bool innate, bool non_innate)
{
	int num = 0;
	uint8_t spells[RSF_MAX];

	int i;

	/* Inicialización paranoica */
	for (i = 0; i < RSF_MAX; i++) {
		spells[i] = 0;
	}

	/* Extraer hechizos, filtrando según sea necesario */
	for (i = FLAG_START, num = 0; i < RSF_MAX; i++) {
		if (!innate && mon_spell_is_innate(i)) continue;
		if (!non_innate && !mon_spell_is_innate(i)) continue;
		if (rsf_has(f, i)) spells[num++] = i;
	}

	/* Elegir al azar */
	return (spells[randint0(num)]);
}

/**
 * Tasa de fallo del hechizo de un monstruo, basada en el poder del hechizo y el estado actual
 */
static int monster_spell_failrate(struct monster *mon)
{
	int power = MIN(mon->race->spell_power, 1);
	int failrate = 0;

	/* Los monstruos estúpidos nunca fallarán (para gelatinas y similares) */
	if (!monster_is_stupid(mon)) {
		/* Tasa de fallo base */
		failrate = 25 - (power + 3) / 4;

		/* El miedo añade 20% */
		if (mon->m_timed[MON_TMD_FEAR])
			failrate += 20;

		/* La confusión y el desencantamiento añaden 50% */
		if (mon->m_timed[MON_TMD_CONF] || mon->m_timed[MON_TMD_DISEN])
			failrate += 50;
	}

	return failrate;
}

/**
 * Calcular el valor base de probabilidad de golpe para un ataque de monstruo basado solo en la raza.
 * Ver también: chance_of_spell_hit_base
 *
 * \param race La raza del monstruo
 * \param effect El ataque
 */
int chance_of_monster_hit_base(const struct monster_race *race,
	const struct blow_effect *effect)
{
	return MAX(race->level, 1) * 3 + effect->power;
}

/**
 * Calcular el valor de probabilidad de golpe de un ataque de monstruo para un monstruo específico
 *
 * \param mon El monstruo
 * \param effect El ataque
 */
static int chance_of_monster_hit(const struct monster *mon,
	const struct blow_effect *effect)
{
	int to_hit = chance_of_monster_hit_base(mon->race, effect);

	/* Aplicar reducción de golpe por aturdimiento si corresponde */
	if (mon->m_timed[MON_TMD_STUN]) {
		to_hit = to_hit * (100 - STUN_HIT_REDUCTION) / 100;
	}

	return to_hit;
}

/**
 * Las criaturas pueden lanzar hechizos, disparar proyectiles y respirar.
 *
 * Devuelve "true" si se lanzó un hechizo (o lo que sea) (con éxito).
 *
 * Quizás los monstruos deberían respirar en ubicaciones *cercanas* al jugador,
 * ya que esto les permitiría infligir daño "parcial".
 *
 * No será posible manejar "correctamente" el caso en el que un
 * monstruo intenta atacar una ubicación que se cree que contiene
 * al jugador, pero que de hecho no está cerca del jugador, ya que esto
 * podría inducir todo tipo de mensajes sobre el ataque en sí mismo, y sobre
 * los efectos del ataque, que el jugador podría o no estar en
 * posición de observar. Por lo tanto, para simplificar, probablemente sea mejor
 * solo permitir ataques "erróneos" por parte de un monstruo si una de las casillas importantes
 (probablemente la casilla inicial o final) está de hecho a la vista del jugador.
 * Puede ser necesario evitar realmente los ataques de hechizos excepto cuando el
 * monstruo realmente tiene línea de visión hacia el jugador. Nótese que un monstruo
 * podría quedar en una situación extraña después de que el jugador se agazapara detrás de un
 * pilar y luego se teletransportara, por ejemplo.
 *
 * Nótese que esta función intenta optimizar el uso de hechizos para los
 * casos en los que el monstruo no tiene hechizos, o tiene hechizos pero no puede usarlos,
 * o tiene hechizos pero no tendrán ningún efecto "útil". Nótese que
 * esta función ha sido un cuello de botella de eficiencia en el pasado.
 *
 * Nótese la bandera especial "MFLAG_NICE", que evita que un monstruo use
 * cualquier ataque de hechizo hasta que el jugador haya tenido una sola oportunidad de moverse.
 *
 * Nótese la interacción entre ataques innatos y ataques no innatos (hechizos
 * verdaderos). Debido a que la verificación de hechizos se realiza primero, las frecuencias
 * de ataque innato reales se ven afectadas por la frecuencia de hechizos.
 */
bool make_ranged_attack(struct monster *mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int thrown_spell, failrate;
	bitflag f[RSF_SIZE];
	char m_name[80];
	bool seen = (player->timed[TMD_BLIND] == 0) && monster_is_visible(mon);
	bool innate = false;

	/* Verificar si puede lanzar este turno, primero no innato y luego innato */
	if (!monster_can_cast(mon, false)) {
		if (!monster_can_cast(mon, true)) {
			return false;
		} else {
			/* Lanzaremos un "hechizo" innato */
			innate = true;
		}
	}

	/* Extraer las banderas de hechizo raciales */
	rsf_copy(f, mon->race->spell_flags);

	/* Los monstruos inteligentes pueden usar hechizos "desesperados" */
	if (monster_is_smart(mon) && mon->hp < mon->maxhp / 10 && one_in_(2)) {
		ignore_spells(f, RST_DAMAGE);
	}

	/* Los monstruos no estúpidos hacen algo de filtrado */
	if (!monster_is_stupid(mon)) {
		struct loc tgrid;

		/* Eliminar los hechizos "ineficaces" */
		remove_bad_spells(mon, f);

		/* Verificar si hay un disparo de rayo limpio */
		monster_get_target_dist_grid(mon, NULL, &tgrid);
		if (test_spells(f, RST_BOLT) &&
			!projectable(cave, mon->grid, tgrid, PROJECT_STOP)) {
			ignore_spells(f, RST_BOLT);
		}

		/* Verificar si hay una invocación posible */
		if (!summon_possible(mon->grid)) {
			ignore_spells(f, RST_SUMMON);
		}
	}

	/* No quedan hechizos */
	if (rsf_is_empty(f)) return false;

	/* Elegir un hechizo para lanzar */
	thrown_spell = choose_attack_spell(f, innate, !innate);

	/* Abortar si no se eligió ningún hechizo */
	if (!thrown_spell) return false;

	/* Ahora habrá al menos un intento, así que obtener el nombre del monstruo */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

	/* Si vemos a un monstruo oculto intentar lanzar un hechizo, tomar conciencia de él */
	if (monster_is_camouflaged(mon))
		become_aware(cave, mon);

	/* Verificar fallo de hechizo (los ataques innatos nunca fallan) */
	failrate = monster_spell_failrate(mon);
	if (!mon_spell_is_innate(thrown_spell) && (randint0(100) < failrate)) {
		msg("%s intenta lanzar un hechizo, pero falla.", m_name);
		return true;
	}

	/* Lanzar el hechizo. */
	disturb(player);
	do_mon_spell(thrown_spell, mon, seen);

	/* Recordar lo que hizo el monstruo */
	if (seen) {
		rsf_on(lore->spell_flags, thrown_spell);
		if (mon_spell_is_innate(thrown_spell)) {
			/* Hechizo innato */
			if (lore->cast_innate < UCHAR_MAX)
				lore->cast_innate++;
		} else {
			/* Hechizo de rayo o bola, o especial */
			if (lore->cast_spell < UCHAR_MAX)
				lore->cast_spell++;
		}
	}
	if (player->is_dead && (lore->deaths < SHRT_MAX)) {
		lore->deaths++;
	}
	lore_update(mon->race, lore);

	/* Se lanzó un hechizo */
	return true;
}



/**
 * Golpe crítico. Todos los golpes que hacen el 95% del daño total posible,
 * y que además hacen al menos 20 de daño, o, a veces, N de daño.
 * Esto se usa solo para determinar "cortes" y "aturdimientos".
 */
static int monster_critical(random_value dice, int rlev, int dam)
{
	int max = 0;
	int total = randcalc(dice, rlev, MAXIMISE);

	/* Debe hacer al menos el 95% del perfecto */
	if (dam < total * 19 / 20) return (0);

	/* Los golpes débiles rara vez funcionan */
	if ((dam < 20) && (randint0(100) >= dam)) return (0);

	/* Daño perfecto */
	if (dam == total) max++;

	/* Sobre-carga */
	if (dam >= 20)
		while (randint0(100) < 2) max++;

	/* Daño crítico */
	if (dam > 45) return (6 + max);
	if (dam > 33) return (5 + max);
	if (dam > 25) return (4 + max);
	if (dam > 18) return (3 + max);
	if (dam > 11) return (2 + max);
	return (1 + max);
}

/**
 * Determinar si un ataque contra el jugador tiene éxito.
 */
bool check_hit(struct player *p, int to_hit)
{
	/* Si algo verifica contra la CA, el jugador aprende bonificaciones de CA */
	equip_learn_on_defend(p);

	/* Verificar si el jugador fue golpeado */
	return test_hit(to_hit, p->state.ac + p->state.to_a);
}

/**
 * Calcular cuánto daño queda después de tener en cuenta la armadura
 * (hace para un ataque físico lo que adjust_dam hace para un ataque elemental).
 */
int adjust_dam_armor(int damage, int ac)
{
	return damage - (damage * ((ac < 240) ? ac : 240) / 400);
}

/**
 * Atacar al jugador mediante ataques físicos.
 */
bool make_attack_normal(struct monster *mon, struct player *p)
{
	struct monster_lore *lore = get_lore(mon->race);
	int rlev = ((mon->race->level >= 1) ? mon->race->level : 1);
	int ap_cnt;
	char m_name[80];
	char ddesc[80];
	bool blinked = false;

	/* No se le permite atacar */
	if (rf_has(mon->race->flags, RF_NEVER_BLOW)) return (false);

	/* Obtener el nombre del monstruo (o "eso") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);

	/* Obtener la información de "murió por" (ej. "un kobold") */
	monster_desc(ddesc, sizeof(ddesc), mon, MDESC_SHOW | MDESC_IND_VIS);

	/* Escanear todos los golpes */
	for (ap_cnt = 0; ap_cnt < z_info->mon_blows_max; ap_cnt++) {
		struct loc pgrid = p->grid;
		bool visible = monster_is_visible(mon) || (mon->race->light > 0);
		bool obvious = false;

		int damage = 0;
		bool do_cut = false;
		bool do_stun = false;

		/* Extraer la información del ataque */
		struct blow_effect *effect = mon->race->blow[ap_cnt].effect;
		struct blow_method *method = mon->race->blow[ap_cnt].method;
		random_value dice = mon->race->blow[ap_cnt].dice;

		/* No hay más ataques */
		if (!method) break;

		/* Manejar "salida" */
		if (p->is_dead || p->upkeep->generate_level) break;

		/* El monstruo golpea al jugador */
		assert(effect);
		if (streq(effect->name, "NONE") ||
			check_hit(p, chance_of_monster_hit(mon, effect))) {
			melee_effect_handler_f effect_handler;

			/* Siempre molesto */
			disturb(p);

			/* Aplicar "protección contra el mal" */
			if (p->timed[TMD_PROTEVIL] > 0) {
				/* Aprender sobre la bandera de maldad */
				if (monster_is_visible(mon))
					rf_on(lore->flags, RF_EVIL);

				if (monster_is_evil(mon) && p->lev >= rlev &&
				    randint0(100) + p->lev > 50) {
					/* Mensaje */
					msg("%s es repelido.", m_name);

					/* Siguiente ataque */
					continue;
				}
			}

			do_cut = method->cut;
			do_stun = method->stun;

			/* Asumir que todos los ataques son obvios */
			obvious = true;

			/* Tirar dados */
			damage = randcalc(dice, rlev, RANDOMISE);

			/* Reducir daño cuando está aturdido */
			if (mon->m_timed[MON_TMD_STUN]) {
				damage = (damage * (100 - STUN_DAM_REDUCTION)) / 100;
			}

			/* Realizar el efecto real. */
			effect_handler = melee_handler_for_blow_effect(effect->name);
			if (effect_handler != NULL) {
				melee_effect_handler_context_t context = {
					p,
					mon,
					NULL,
					rlev,
					method,
					p->state.ac + p->state.to_a,
					ddesc,
					obvious,
					blinked,
					damage,
					m_name
				};

				effect_handler(&context);

				/* Guardar cualquier cambio realizado en el manejador para uso posterior. */
				obvious = context.obvious;
				blinked = context.blinked;
				damage = context.damage;
			} else {
				msg("ERROR: Manejador de efecto no encontrado para %s.", effect->name);
			}

			/* No cortar o aturdir si el jugador está muerto */
			if (p->is_dead) {
				do_cut = false;
				do_stun = false;
			}

			/* Solo uno de corte o aturdimiento */
			if (do_cut && do_stun) {
				/* Cancelar corte */
				if (randint0(100) < 50)
					do_cut = false;

				/* Cancelar aturdimiento */
				else
					do_stun = false;
			}

			/* Manejar corte */
			if (do_cut) {
				/* Golpe crítico (cero si no es crítico) */
				int amt, tmp = monster_critical(dice, rlev, damage);

				/* Tirar para daño */
				switch (tmp) {
					case 0: amt = 0; break;
					case 1: amt = randint1(5); break;
					case 2: amt = randint1(5) + 5; break;
					case 3: amt = randint1(20) + 20; break;
					case 4: amt = randint1(50) + 50; break;
					case 5: amt = randint1(100) + 100; break;
					case 6: amt = 300; break;
					default: amt = 500; break;
				}

				/* Aplicar el corte */
				if (amt) {
					(void)player_inc_timed(p, TMD_CUT, amt,
						true, true, true);
				}
			}

			/* Manejar aturdimiento */
			if (do_stun) {
				/* Golpe crítico (cero si no es crítico) */
				int amt, tmp = monster_critical(dice, rlev, damage);

				/* Tirar para daño */
				switch (tmp) {
					case 0: amt = 0; break;
					case 1: amt = randint1(5); break;
					case 2: amt = randint1(10) + 10; break;
					case 3: amt = randint1(20) + 20; break;
					case 4: amt = randint1(30) + 30; break;
					case 5: amt = randint1(40) + 40; break;
					case 6: amt = 100; break;
					default: amt = 200; break;
				}

				/* Aplicar el aturdimiento */
				if (amt) {
					(void)player_inc_timed(p, TMD_STUN, amt,
						true, true, true);
				}
			}
		} else {
			/* El monstruo visible falló al jugador, así que notificar si corresponde. */
			if (monster_is_visible(mon) &&	method->miss) {
				/* Molesto */
				disturb(p);
				msg("%s falla contra ti.", m_name);
			}
		}

		/* Analizar solo monstruos "visibles" */
		if (visible) {
			/* Contar ataques "obvios" (y aquellos que causan daño) */
			if (obvious || damage || (lore->blows[ap_cnt].times_seen > 10)) {
				/* Contar ataques de este tipo */
				if (lore->blows[ap_cnt].times_seen < UCHAR_MAX)
					lore->blows[ap_cnt].times_seen++;
			}
		}

		/* Saltar los otros golpes si el jugador se ha movido */
		if (!loc_eq(p->grid, pgrid)) break;
	}

	/* Parpadear */
	if (blinked) {
		char dice[5];

		if (!p->is_dead && square_isseen(cave, mon->grid)) {
			add_monster_message(mon, MON_MSG_HIT_AND_RUN, true);
		}
		strnfmt(dice, sizeof(dice), "%d", z_info->max_sight * 2 + 5);
		effect_simple(EF_TELEPORT, source_monster(mon->midx), dice, 0, 0, 0, 0, 0, NULL);
	}

	/* Siempre notificar la causa de la muerte */
	if (p->is_dead && (lore->deaths < SHRT_MAX))
		lore->deaths++;

	/* Aprender conocimiento */
	lore_update(mon->race, lore);

	/* Asumir que atacamos */
	return (true);
}

/**
 * Atacar a otro monstruo mediante ataques físicos.
 */
bool monster_attack_monster(struct monster *mon, struct monster *t_mon)
{
	struct monster_lore *lore = get_lore(mon->race);
	int rlev = ((mon->race->level >= 1) ? mon->race->level : 1);
	int ap_cnt;
	char m_name[80];
	char t_name[80];
	bool blinked = false;

	/* No se le permite atacar */
	if (rf_has(mon->race->flags, RF_NEVER_BLOW)) return (false);

	/* Obtener los nombres de los monstruos (o "eso") */
	monster_desc(m_name, sizeof(m_name), mon, MDESC_STANDARD);
	monster_desc(t_name, sizeof(t_name), t_mon, MDESC_TARG);

	/* Escanear todos los golpes */
	for (ap_cnt = 0; ap_cnt < z_info->mon_blows_max; ap_cnt++) {
		struct loc grid = t_mon->grid;
		bool visible = monster_is_visible(mon) || (mon->race->light > 0);
		bool obvious = false;

		int damage = 0;
		bool do_stun = false;

		/* Extraer la información del ataque */
		struct blow_effect *effect = mon->race->blow[ap_cnt].effect;
		struct blow_method *method = mon->race->blow[ap_cnt].method;
		random_value dice = mon->race->blow[ap_cnt].dice;

		/* No hay más ataques */
		if (!method) break;

		/* El monstruo golpea al monstruo */
		assert(effect);
		if (streq(effect->name, "NONE") ||
			test_hit(chance_of_monster_hit(mon, effect), t_mon->race->ac)) {
			melee_effect_handler_f effect_handler;

			do_stun = method->stun;

			/* Asumir que todos los ataques son obvios */
			obvious = true;

			/* Tirar dados */
			damage = randcalc(dice, rlev, RANDOMISE);

			/* Reducir daño cuando está aturdido */
			if (mon->m_timed[MON_TMD_STUN]) {
				damage = (damage * (100 - STUN_DAM_REDUCTION)) / 100;
			}

			/* Realizar el efecto real. */
			effect_handler = melee_handler_for_blow_effect(effect->name);
			if (effect_handler != NULL) {
				melee_effect_handler_context_t context = {
					NULL,
					mon,
					t_mon,
					rlev,
					method,
					t_mon->race->ac,
					NULL,
					obvious,
					blinked,
					damage,
					m_name
				};

				effect_handler(&context);

				/* Guardar cualquier cambio realizado en el manejador para uso posterior. */
				obvious = context.obvious;
				blinked = context.blinked;
				damage = context.damage;
			} else {
				msg("ERROR: Manejador de efecto no encontrado para %s.", effect->name);
			}

			/* Manejar aturdimiento */
			if (do_stun && square_monster(cave, grid)) {
				/* Golpe crítico (cero si no es crítico) */
				int amt, tmp = monster_critical(dice, rlev, damage);

				/* Tirar para daño */
				switch (tmp) {
					case 0: amt = 0; break;
					case 1: amt = randint1(5); break;
					case 2: amt = randint1(10) + 10; break;
					case 3: amt = randint1(20) + 20; break;
					case 4: amt = randint1(30) + 30; break;
					case 5: amt = randint1(40) + 40; break;
					case 6: amt = 100; break;
					default: amt = 200; break;
				}

				/* Aplicar el aturdimiento */
				if (amt)
					(void)mon_inc_timed(t_mon, MON_TMD_STUN, amt, 0);
			}
		} else {
			/* El monstruo visible falló al monstruo, así que notificar si corresponde. */
			if (monster_is_visible(mon) && method->miss) {
				msg("%s falla contra %s.", m_name, t_name);
			}
		}

		/* Analizar solo monstruos "visibles" */
		if (visible) {
			/* Contar ataques "obvios" (y aquellos que causan daño) */
			if (obvious || damage || (lore->blows[ap_cnt].times_seen > 10)) {
				/* Contar ataques de este tipo */
				if (lore->blows[ap_cnt].times_seen < UCHAR_MAX)
					lore->blows[ap_cnt].times_seen++;
			}
		}

		/* Saltar los otros golpes si el objetivo se ha movido o muerto */
		if (!square_monster(cave, grid)) break;
	}

	/* Parpadear */
	if (blinked) {
		char dice[5];

		if (square_isseen(cave, mon->grid)) {
			add_monster_message(mon, MON_MSG_HIT_AND_RUN, true);
		}
		strnfmt(dice, sizeof(dice), "%d", z_info->max_sight * 2 + 5);
		effect_simple(EF_TELEPORT, source_monster(mon->midx), dice, 0, 0, 0, 0, 0, NULL);
	}

	/* Aprender conocimiento */
	lore_update(mon->race, lore);

	/* Asumir que atacamos */
	return (true);
}