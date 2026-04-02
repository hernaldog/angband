/**
 * \file mon-blows.c
 * \brief Módulo de combate cuerpo a cuerpo de monstruos.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
 *               2013 Ben Semmler
 *               2016 Nick McConnell
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
#include "init.h"
#include "monster.h"
#include "mon-attack.h"
#include "mon-blows.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-util.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"

/**
 * ------------------------------------------------------------------------
 * Métodos de golpe de monstruo
 * ------------------------------------------------------------------------ */

typedef enum {
	BLOW_TAG_NONE,
	BLOW_TAG_TARGET,
	BLOW_TAG_OF_TARGET,
	BLOW_TAG_HAS
} blow_tag_t;

static blow_tag_t blow_tag_lookup(const char *tag)
{
	if (strncmp(tag, "target", 6) == 0)
		return BLOW_TAG_TARGET;
	else if (strncmp(tag, "oftarget", 8) == 0)
		return BLOW_TAG_OF_TARGET;
	else if (strncmp(tag, "has", 3) == 0)
		return BLOW_TAG_HAS;
	else
		return BLOW_TAG_NONE;
}

/**
 * Imprime un mensaje de golpe de monstruo.
 *
 * Rellenamos el nombre del monstruo y/o pronombre donde sea necesario en
 * el mensaje para reemplazar las instancias de {name} o {pronoun}.
 */
char *monster_blow_method_action(const struct blow_method *method, int midx)
{
	const char punct[] = ".!?;:,'";
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	const char *in_cursor;
	size_t end = 0;
	struct monster *t_mon = NULL;

	int choice = randint0(method->num_messages);
	const struct blow_message *msg = method->messages;

	/* Obtener el monstruo objetivo, si lo hay */
	if (midx > 0) {
		t_mon = cave_monster(cave, midx);
	}

	/* Elegir un mensaje */
	while (choice--) {
		msg = msg->next;
	}
	in_cursor = msg->act_msg;

	/* Añadir información al mensaje */
	next = strchr(in_cursor, '{');
	while (next) {
		/* Copiar el texto que lleva hasta este { */
		strnfcat(buf, 1024, &end, "%.*s", (int) (next - in_cursor),
			in_cursor);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Etiqueta válida */
		if (*s == '}') {
			/* Comenzar la etiqueta después del { */
			tag = next + 1;
			in_cursor = s + 1;

			switch (blow_tag_lookup(tag)) {
				case BLOW_TAG_TARGET: {
					char m_name[80];
					if (midx > 0) {
						int mdesc_mode = MDESC_TARG;

						if (!strchr(punct, *in_cursor)) {
							mdesc_mode |= MDESC_COMMA;
						}
						monster_desc(m_name,
							sizeof(m_name), t_mon,
							mdesc_mode);
						strnfcat(buf, sizeof(buf),
							&end, "%s", m_name);
					} else {
						//strnfcat(buf, sizeof(buf), &end, "you");
						strnfcat(buf, sizeof(buf), &end, "te"); //fix traduc
					}
					break;
				}
				case BLOW_TAG_OF_TARGET: {
					char m_name[80];
					if (midx > 0) {
						monster_desc(m_name,
							sizeof(m_name), t_mon,
							MDESC_TARG | MDESC_POSS);
						strnfcat(buf, sizeof(buf), &end, "%s", m_name);
					} else {
						//strnfcat(buf, sizeof(buf), &end, "your");
						strnfcat(buf, sizeof(buf), &end, "tu"); //fix traduc
					}
					break;
				}
				case BLOW_TAG_HAS: {
					if (midx > 0) {
						//strnfcat(buf, sizeof(buf), &end, "has");
						strnfcat(buf, sizeof(buf), &end, "tiene"); //fix traduc
					} else {
						//strnfcat(buf, sizeof(buf), &end, "have");
						strnfcat(buf, sizeof(buf), &end, "tienes"); //fix traduc
					}
					break;
				}

				default: {
					break;
				}
			}
		} else {
			/* Una etiqueta no válida, omitirla */
			in_cursor = next + 1;
		}

		next = strchr(in_cursor, '{');
	}
	strnfcat(buf, 1024, &end, "%s", in_cursor);
	return string_make(buf);
}

/**
 * ------------------------------------------------------------------------
 * Funciones auxiliares para efectos de golpe de monstruo
 * ------------------------------------------------------------------------ */
int blow_index(const char *name)
{
	int i;

	for (i = 1; i < z_info->blow_effects_max; i++) {
		struct blow_effect *effect = &blow_effects[i];
		if (my_stricmp(name, effect->name) == 0)
			return i;
	}
	return 0;
}

/**
 * Muestra el mensaje para un golpe contra un jugador.
 *
 * \param method es la estructura que describe el tipo de golpe.
 * \param m_name es el nombre formateado del monstruo atacante.
 * \param p es el jugador que está siendo atacado.
 * \param damage es la cantidad de daño del golpe.
 */
static void display_blow_message_vs_player(const struct blow_method *method,
		const char *m_name, struct player *p, int damage)
{
	char *act = monster_blow_method_action(method, -1);

	if (act) {
		const char *fullstop = ".";

		if (suffix(act, "'") || suffix(act, "!")) {
			fullstop = "";
		}
		if (damage > 0 && OPT(p, show_damage)) {
			msgt(method->msgt, "%s %s%s (%d)", m_name, act,
				fullstop, damage);
		} else {
			msgt(method->msgt, "%s %s%s", m_name, act, fullstop);
		}
		string_free(act);
	} else if (damage > 0 && OPT(p, show_damage)) {
		msgt(method->msgt, "Recibes %d de daño.", damage);
	}
}

/**
 * Muestra el mensaje para un golpe contra otro monstruo.
 *
 * \param method es la estructura que describe el tipo de golpe.
 * \param m_name es el nombre formateado del monstruo atacante.
 * \param t_idx es el índice del monstruo objetivo (es decir, si mon es la
 * estructura que representa al objetivo, es mon->midx).
 */
static void display_blow_message_vs_monster(const struct blow_method *method,
		const char *m_name, int t_idx)
{
	char *act = monster_blow_method_action(method, t_idx);

	if (act) {
		const char *fullstop = ".";

		if (suffix(act, "'") || suffix(act, "!")) {
			fullstop = "";
		}
		msgt(method->msgt, "%s %s%s", m_name, act, fullstop);
		string_free(act);
	}
}

/**
 * El monstruo roba un objeto del jugador
 */
static void steal_player_item(melee_effect_handler_context_t *context)
{
	int tries;

    /* Encontrar un objeto */
    for (tries = 0; tries < 10; tries++) {
		struct object *obj, *stolen;
		char o_name[80];
		bool split = false;
		bool none_left = false;

        /* Elegir un objeto */
		int index = randint0(z_info->pack_size);

        /* Obtener el objeto */
        obj = context->p->upkeep->inven[index];

		/* Omitir no-objetos */
		if (obj == NULL) continue;

        /* Omitir artefactos */
        if (obj->artifact) continue;

        /* Obtener una descripción */
        object_desc(o_name, sizeof(o_name), obj, ODESC_FULL, context->p);

		/* ¿Es parte de un montón lo que se roba? */
		if (obj->number > 1)
			split = true;

		/* Intentar robar */
		if (react_to_slay(obj, context->mon)) {
			/* Reaccionar a objetos que dañan al monstruo */
			char m_name[80];

			/* Obtener los nombres del monstruo (o "eso") */
			monster_desc(m_name, sizeof(m_name), context->mon, MDESC_STANDARD);

			/* Fallo al robar */
			msg("%s intenta robarte %s %s, pero falla.", m_name,
				(split ? "uno de tus" : "tu"), o_name);
		} else {
			/* Mensaje */
			msg("¡Te han robado %s %s (%c)!",
				(split ? "uno de tus" : "tu"), o_name,
				gear_to_label(context->p, obj));

			/* Robar y llevar */
			stolen = gear_object_for_use(context->p, obj, 1,
				false, &none_left);
			(void)monster_carry(cave, context->mon, stolen);
		}

        /* Obvio */
        context->obvious = true;

        /* Desaparecer parpadeando */
        context->blinked = true;

        /* Terminado */
        break;
    }
}

/**
 * Obtiene el daño elemental recibido por un monstruo del combate cuerpo a
 * cuerpo de otro monstruo
 */
static int monster_elemental_damage(melee_effect_handler_context_t *context,
									int type, enum mon_messages *hurt_msg,
									enum mon_messages *die_msg)
{
	struct monster_lore *lore = get_lore(context->t_mon->race);
	int hurt_flag = RF_NONE;
	int imm_flag = RF_NONE;
	int damage = 0;

	/* Tratar con tipos elementales */
	switch (type) {
		case PROJ_ACID: {
			imm_flag = RF_IM_ACID;
			break;
		}
		case PROJ_ELEC: {
			imm_flag = RF_IM_ELEC;
			break;
		}
		case PROJ_FIRE: {
			imm_flag = RF_IM_FIRE;
			hurt_flag = RF_HURT_FIRE;
			*hurt_msg = MON_MSG_CATCH_FIRE;
			*die_msg = MON_MSG_DISINTEGRATES;
			break;
		}
		case PROJ_COLD: {
			imm_flag = RF_IM_COLD;
			hurt_flag = RF_HURT_COLD;
			*hurt_msg = MON_MSG_BADLY_FROZEN;
			*die_msg = MON_MSG_FREEZE_SHATTER;
			break;
		}
		case PROJ_POIS: {
			imm_flag = RF_IM_POIS;
			break;
		}
		default: return 0;
	}

	rf_on(lore->flags, imm_flag);
	if (hurt_flag) {
		rf_on(lore->flags, hurt_flag);
	}

	if (rf_has(context->t_mon->race->flags, imm_flag)) {
		*hurt_msg = MON_MSG_RESIST_A_LOT;
		*die_msg = MON_MSG_DIE;
		damage = context->damage / 9;
	} else if (rf_has(context->t_mon->race->flags, hurt_flag)) {
		damage = context->damage * 2;
	} else {
		*hurt_msg = MON_MSG_NONE;
		*die_msg = MON_MSG_DIE;
	}

	return damage;
}

/**
 * Inflige el daño cuerpo a cuerpo real de un monstruo a un jugador o monstruo objetivo
 *
 * Esta función se usa en manejadores donde no hay más procesamiento de
 * un monstruo después del daño, por lo que siempre devolvemos true para
 * objetivos monstruo
 */
static bool monster_damage_target(melee_effect_handler_context_t *context,
								  bool no_further_monster_effect)
{
	/* Recibir daño */
	if (context->p) {
		/*
		 * La reducción de daño del jugador no afecta al daño usado para
		 * los cálculos de efectos secundarios, así que dejamos context->damage
		 * como está.
		 */
		int reduced = player_apply_damage_reduction(context->p,
			context->damage);

		display_blow_message_vs_player(context->method, context->m_name,
			context->p, reduced);
		take_hit(context->p, reduced, context->ddesc);
		if (context->p->is_dead) return true;
	} else {
		bool dead;

		display_blow_message_vs_monster(context->method,
			context->m_name, context->t_mon->midx);
		dead = mon_take_nonplayer_hit(context->damage, context->t_mon,
			MON_MSG_NONE, MON_MSG_DIE);
		return (dead || no_further_monster_effect);
	}
	return false;
}

/**
 * ------------------------------------------------------------------------
 * Manejadores de efectos múltiples de golpe de monstruo
 * Estos son llamados por varios manejadores de efectos individuales
 * ------------------------------------------------------------------------ */
/**
 * Inflige daño como resultado de un ataque cuerpo a cuerpo que tiene un
 * aspecto elemental.
 *
 * \param context es la información para el ataque actual.
 * \param type es la constante PROJ_ para el elemento.
 * \param pure_element debería ser true si no hay efectos secundarios
 * (principalmente un truco para el veneno).
 */
static void melee_effect_elemental(melee_effect_handler_context_t *context,
								   int type, bool pure_element)
{
	int physical_dam, elemental_dam;
	enum mon_messages hurt_msg = MON_MSG_NONE;
	enum mon_messages die_msg = MON_MSG_DIE;

	if (pure_element)
		/* Obvio */
		context->obvious = true;

	if (context->p) {
		switch (type) {
			case PROJ_ACID: msg("¡Estás cubierto de ácido!");
				break;
			case PROJ_ELEC: msg("¡Te golpea un relámpago!");
				break;
			case PROJ_FIRE: msg("¡Estás envuelto en llamas!");
				break;
			case PROJ_COLD: msg("¡Estás cubierto de escarcha!");
				break;
		}
	}

	/* Dar una pequeña bonificación a la CA para ataques elementales */
	physical_dam = adjust_dam_armor(context->damage, context->ac + 50);

	/* Algunos ataques no hacen daño físico */
	if (!context->method->phys)
		physical_dam = 0;

	if (context->p) {
		elemental_dam = adjust_dam(context->p, type, context->damage,
								   RANDOMISE, 0, true);
	} else {
		assert(context->t_mon);
		elemental_dam = monster_elemental_damage(context, type, &hurt_msg,
												 &die_msg);
	}

	/* Tomar el mayor del daño físico o elemental */
	context->damage = (physical_dam > elemental_dam) ?
		physical_dam : elemental_dam;

	if (context->p && elemental_dam > 0)
		inven_damage(context->p, type, MIN(elemental_dam * 5, 300));
	if (context->damage > 0) {
		if (context->p) {
			/*
			 * La reducción de daño del jugador no afecta al daño
			 * usado para los cálculos de efectos secundarios, así que dejamos
			 * context->damage como está.
			 */
			int reduced = player_apply_damage_reduction(context->p,
				context->damage);

			display_blow_message_vs_player(context->method,
				context->m_name, context->p, reduced);
			take_hit(context->p, reduced, context->ddesc);
		} else {
			assert(context->t_mon);
			display_blow_message_vs_monster(context->method,
				context->m_name, context->t_mon->midx);
			(void) mon_take_nonplayer_hit(context->damage,
				context->t_mon, hurt_msg, die_msg);
		}
	}

	/* Aprender sobre el jugador */
	if (pure_element && context->p) {
		update_smart_learn(context->mon, context->p, 0, 0, type);
	}
}

/**
 * Inflige daño como resultado de un ataque cuerpo a cuerpo que tiene un
 * efecto de estado.
 *
 * \param context es la información para el ataque actual.
 * \param type es la constante TMD_ para el efecto.
 * \param amount es la cantidad que debe aumentar el temporizador.
 * \param of_flag es la bandera OF_ que se pasa al aprendizaje del monstruo
 * para este efecto.
 * \param save indica si se debe intentar una tirada de salvación para este efecto.
 * \param save_msg es el mensaje que se muestra si la tirada de salvación tiene éxito.
 */
static void melee_effect_timed(melee_effect_handler_context_t *context,
							   int type, int amount, int of_flag, bool save,
							   const char *save_msg)
{
	/* Recibir daño */
	if (monster_damage_target(context, false)) return;

	/* Manejar estado */
	if (context->t_mon) {
		/* Traducir a efecto temporizado de monstruo */
		int mon_tmd_effect = -1;

		/* Servirá hasta que se fusionen los efectos temporizados de monstruo y jugador */
		switch (type) {
			case TMD_CONFUSED: {
				mon_tmd_effect = MON_TMD_CONF;
				break;
			}
			case TMD_PARALYZED: {
				mon_tmd_effect = MON_TMD_HOLD;
				break;
			}
			case TMD_BLIND: {
				mon_tmd_effect = MON_TMD_STUN;
				break;
			}
			case TMD_AFRAID: {
				mon_tmd_effect = MON_TMD_FEAR;
				break;
			}
			default: {
				break;
			}
		}
		if (mon_tmd_effect >= 0) {
			mon_inc_timed(context->t_mon, mon_tmd_effect, amount, 0);
			context->obvious = true;
		}
	} else if (save && randint0(100) < context->p->state.skills[SKILL_SAVE]) {
		/* Intentar una tirada de salvación si se desea. */
		if (save_msg != NULL) {
			msg("%s", save_msg);
		}
		context->obvious = true;
	} else {
		/* Aumentar el temporizador para el tipo. */
		if (player_inc_timed(context->p, type, amount, true, true,
				true)) {
			context->obvious = true;
		}

		/* Aprender sobre el jugador */
		update_smart_learn(context->mon, context->p, of_flag, 0, -1);
	}
}

/**
 * Inflige daño como resultado de un ataque cuerpo a cuerpo que drena una
 * estadística.
 *
 * \param context es la información para el ataque actual.
 * \param stat es la constante STAT_ para la estadística deseada.
 */
static void melee_effect_stat(melee_effect_handler_context_t *context, int stat)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Dañar (estadística) */
	effect_simple(EF_DRAIN_STAT,
			source_monster(context->mon->midx),
			"0",
			stat,
			0,
			0,
			0,
			0,
			&context->obvious);
}

/**
 * Inflige daño como resultado de un ataque cuerpo a cuerpo que drena experiencia.
 *
 * \param context es la información para el ataque actual.
 * \param chance es la probabilidad del jugador de resistir el drenaje si tiene
 * OF_HOLD_LIFE.
 * \param drain_amount es la cantidad base de experiencia a drenar.
 */
static void melee_effect_experience(melee_effect_handler_context_t *context,
									int chance, int drain_amount)
{
	/* Recibir daño */
	if (context->p) {
		/*
		 * La reducción de daño del jugador no afecta al daño usado para
		 * los cálculos de efectos secundarios, así que dejamos context->damage
		 * como está.
		 */
		int reduced = player_apply_damage_reduction(context->p,
			context->damage);

		display_blow_message_vs_player(context->method,
			context->m_name, context->p, reduced);
		take_hit(context->p, reduced, context->ddesc);
		context->obvious = true;
		update_smart_learn(context->mon, context->p, OF_HOLD_LIFE, 0, -1);
		if (context->p->is_dead) return;
	} else {
		assert(context->t_mon);
		display_blow_message_vs_monster(context->method,
			context->m_name, context->t_mon->midx);
		(void) mon_take_nonplayer_hit(context->damage, context->t_mon,
									  MON_MSG_NONE, MON_MSG_DIE);
		return;
	}

	if (player_of_has(context->p, OF_HOLD_LIFE) && (randint0(100) < chance)) {
		msg("¡Conservas tu fuerza vital!");
	} else {
		int32_t d = drain_amount +
			(context->p->exp/100) * z_info->life_drain_percent;
		if (player_of_has(context->p, OF_HOLD_LIFE)) {
			msg("Sientes que tu vida se escapa.");
			player_exp_lose(context->p, d / 10, false);
		} else {
			msg("Sientes que tu vida se drena.");
			player_exp_lose(context->p, d, false);
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Manejadores de efectos de golpe de monstruo
 * ------------------------------------------------------------------------ */
/**
 * Manejador de efecto cuerpo a cuerpo: Golpear al jugador, pero no hacer ningún daño.
 */
static void melee_effect_handler_NONE(melee_effect_handler_context_t *context)
{
	if (context->p) {
		display_blow_message_vs_player(context->method,
			context->m_name, context->p, 0);
	} else {
		assert(context->t_mon);
		display_blow_message_vs_monster(context->method,
			context->m_name, context->t_mon->midx);
	}
	context->obvious = true;
	context->damage = 0;
}

/**
 * Manejador de efecto cuerpo a cuerpo: Herir al jugador sin efectos secundarios.
 */
static void melee_effect_handler_HURT(melee_effect_handler_context_t *context)
{
	/* Obvio */
	context->obvious = true;

	/* La armadura reduce el daño total */
	context->damage = adjust_dam_armor(context->damage, context->ac);

	/* Recibir daño */
	(void) monster_damage_target(context, true);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Envenenar al jugador.
 *
 * No podemos usar melee_effect_timed(), porque esto es tanto un ataque elemental
 * como un ataque de estado. Nótese el valor false para pure_element en
 * melee_effect_elemental().
 */
static void melee_effect_handler_POISON(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_POIS, false);

	/* El jugador está muerto o no atacado */
	if (!context->p || context->p->is_dead)
		return;

	/* Aplicar efecto de "veneno" */
	if (player_inc_timed(context->p, TMD_POISONED,
			5 + randint1(context->rlev), true, true, true)) {
		context->obvious = true;
	}

	/* Aprender sobre el jugador */
	update_smart_learn(context->mon, context->p, 0, 0, ELEM_POIS);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Desencantar al jugador.
 */
static void melee_effect_handler_DISENCHANT(melee_effect_handler_context_t *context)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Aplicar desencantamiento si no hay resistencia */
	if (!player_resists(context->p, ELEM_DISEN))
		effect_simple(EF_DISENCHANT, source_monster(context->mon->midx), "0", 0, 0, 0, 0, 0, &context->obvious);

	/* Aprender sobre el jugador */
	update_smart_learn(context->mon, context->p, 0, 0, ELEM_DISEN);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar cargas del inventario del jugador.
 */
static void melee_effect_handler_DRAIN_CHARGES(melee_effect_handler_context_t *context)
{
	struct object *obj;
	struct monster *monster = context->mon;
	struct player *current_player = context->p;
	int tries;
	int unpower = 0, newcharge;

	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Encontrar un objeto */
	for (tries = 0; tries < 10; tries++) {
		/* Elegir un objeto */
		obj = context->p->upkeep->inven[randint0(z_info->pack_size)];

		/* Omitir no-objetos */
		if (obj == NULL) continue;

		/* Drenar varitas/varales cargados */
		if (tval_can_have_charges(obj)) {
			/* ¿Cargado? */
			if (obj->pval) {
				/* Obtener número de cargas a drenar */
				unpower = (context->rlev / (obj->kind->level + 2)) + 1;

				/* Obtener nuevo valor de carga, no permitir negativo */
				newcharge = MAX((obj->pval - unpower),0);

				/* Eliminar las cargas */
				obj->pval = newcharge;
			}
		}

		if (unpower) {
			int heal = context->rlev * unpower;

			msg("¡La energía se drena de tu mochila!");

			context->obvious = true;

			/* No curar más del máximo de PV */
			heal = MIN(heal, monster->maxhp - monster->hp);

			/* Curar */
			monster->hp += heal;

			/* Redibujar (después) si es necesario */
			if (current_player->upkeep->health_who == monster)
				current_player->upkeep->redraw |= (PR_HEALTH);

			/* Combinar la mochila */
			current_player->upkeep->notice |= (PN_COMBINE);

			/* Redibujar cosas */
			current_player->upkeep->redraw |= (PR_INVEN);

			/* Afectar solo un espacio de inventario */
			break;
		}
	}
}

/**
 * Manejador de efecto cuerpo a cuerpo: Tomar el oro del jugador.
 */
static void melee_effect_handler_EAT_GOLD(melee_effect_handler_context_t *context)
{
	struct player *current_player = context->p;

	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

    /* Obvio */
    context->obvious = true;

    /* Intentar tirada de salvación (a menos que esté paralizado) basada en destreza y nivel */
    if (!current_player->timed[TMD_PARALYZED] &&
        (randint0(100) < (adj_dex_safe[current_player->state.stat_ind[STAT_DEX]]
						  + current_player->lev))) {
        /* Mensaje de tirada de salvación */
        msg("¡Proteges rápidamente tu bolsa de dinero!");

        /* Parpadeo ocasional de todos modos */
        if (randint0(3)) context->blinked = true;
    } else {
        int32_t gold = (current_player->au / 10) + randint1(25);
        if (gold < 2) gold = 2;
        if (gold > 5000) gold = (current_player->au / 20) + randint1(3000);
        if (gold > current_player->au) gold = current_player->au;
        current_player->au -= gold;
        if (gold <= 0) {
            msg("No te robaron nada.");
            return;
        }

        /* Informar al jugador de que le robaron */
        msg("Tu bolsa se siente más ligera.");
        if (current_player->au)
            msg("¡Te robaron %d monedas!", gold);
        else
            msg("¡Te robaron todas tus monedas!");

        /* Mientras tengamos oro, ponerlo en objetos */
        while (gold > 0) {
            int amt;

            /* Crear un nuevo objeto temporal */
            struct object *obj = object_new();
            object_prep(obj, money_kind("gold", gold), 0, MINIMISE);

            /* Cantidad de oro a poner en este objeto */
            amt = gold > MAX_PVAL ? MAX_PVAL : gold;
            obj->pval = amt;
            gold -= amt;

            /* Establecer origen a robado, para que no se confunda con
             * tesoro soltado en monster_death */
            obj->origin = ORIGIN_STOLEN;
            obj->origin_depth = convert_depth_to_origin(current_player->depth);

            /* Dar el oro al monstruo */
            monster_carry(cave, context->mon, obj);
        }

        /* Redibujar oro */
        current_player->upkeep->redraw |= (PR_GOLD);

        /* Desaparecer parpadeando */
        context->blinked = true;
    }
}

/**
 * Manejador de efecto cuerpo a cuerpo: Tomar algo del inventario del jugador.
 */
static void melee_effect_handler_EAT_ITEM(melee_effect_handler_context_t *context)
{
    /* Recibir daño */
	if (monster_damage_target(context, false)) return;

	/* Robar del jugador o monstruo */
	if (context->p) {
		int chance = adj_dex_safe[context->p->state.stat_ind[STAT_DEX]] +
			context->p->lev;

		/* Tirada de salvación (a menos que esté paralizado) basada en destreza y nivel */
		if (!context->p->timed[TMD_PARALYZED] && (randint0(100) < chance)) {
			/* Mensaje de tirada de salvación */
			msg("¡Te agarras a tu mochila!");

			/* "Parpadeo" ocasional de todos modos */
			context->blinked = true;

			/* Obvio */
			context->obvious = true;

			/* Terminado */
			return;
		}

		/* Intentar robar un objeto */
		steal_player_item(context);
	} else {
		assert(context->t_mon);
		steal_monster_item(context->t_mon, context->mon->midx);
		context->obvious = true;
	}
}

/**
 * Manejador de efecto cuerpo a cuerpo: Comer la comida del jugador.
 */
static void melee_effect_handler_EAT_FOOD(melee_effect_handler_context_t *context)
{
	int tries;

	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Robar algo de comida */
	for (tries = 0; tries < 10; tries++) {
		/* Elegir un objeto de la mochila */
		int index = randint0(z_info->pack_size);
		struct object *obj, *eaten;
		char o_name[80];
		bool none_left = false;

		/* Obtener el objeto */
		obj = context->p->upkeep->inven[index];

		/* Omitir no-objetos */
		if (obj == NULL) continue;

		/* Omitir objetos no comestibles */
		if (!tval_is_edible(obj)) continue;

		if (obj->number == 1) {
			object_desc(o_name, sizeof(o_name), obj, ODESC_BASE,
				context->p);
			msg("¡Se comieron tu %s (%c)!", o_name,
				gear_to_label(context->p, obj));
		} else {
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_BASE, context->p);
			msg("¡Se comieron uno de tus %s (%c)!", o_name,
				gear_to_label(context->p, obj));
		}

		/* Robar y comer */
		eaten = gear_object_for_use(context->p, obj, 1, false,
			&none_left);
		if (eaten->known)
			object_delete(player->cave, NULL, &eaten->known);
		object_delete(cave, player->cave, &eaten);

		/* Obvio */
		context->obvious = true;

		/* Terminado */
		break;
	}
}

/**
 * Manejador de efecto cuerpo a cuerpo: Absorber la luz del jugador.
 */
static void melee_effect_handler_EAT_LIGHT(melee_effect_handler_context_t *context)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Drenar la fuente de luz */
	effect_simple(EF_DRAIN_LIGHT,
			source_monster(context->mon->midx),
			"250+1d250",
			0,
			0,
			0,
			0,
			0,
			&context->obvious);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Atacar al jugador con ácido.
 */
static void melee_effect_handler_ACID(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_ACID, true);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Atacar al jugador con electricidad.
 */
static void melee_effect_handler_ELEC(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_ELEC, true);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Atacar al jugador con fuego.
 */
static void melee_effect_handler_FIRE(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_FIRE, true);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Atacar al jugador con frío.
 */
static void melee_effect_handler_COLD(melee_effect_handler_context_t *context)
{
	melee_effect_elemental(context, PROJ_COLD, true);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Cegar al jugador.
 */
static void melee_effect_handler_BLIND(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_BLIND, 10 + randint1(context->rlev),
					   OF_PROT_BLIND, false, NULL);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Confundir al jugador.
 */
static void melee_effect_handler_CONFUSE(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_CONFUSED, 3 + randint1(context->rlev),
					   OF_PROT_CONF, false, NULL);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Aterrorizar al jugador.
 */
static void melee_effect_handler_TERRIFY(melee_effect_handler_context_t *context)
{
	melee_effect_timed(context, TMD_AFRAID, 3 + randint1(context->rlev),
					   OF_PROT_FEAR, true, "¡Te mantienes firme!");
}

/**
 * Manejador de efecto cuerpo a cuerpo: Paralizar al jugador.
 */
static void melee_effect_handler_PARALYZE(melee_effect_handler_context_t *context)
{
	/* Prevenir parálisis permanente mediante daño */
	if (context->p && context->p->timed[TMD_PARALYZED] && (context->damage < 1))
		context->damage = 1;

	melee_effect_timed(context, TMD_PARALYZED, 3 + randint1(context->rlev),
					   OF_FREE_ACT, true, "¡Resistes los efectos!");
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la fuerza del jugador.
 */
static void melee_effect_handler_LOSE_STR(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_STR);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la inteligencia del jugador.
 */
static void melee_effect_handler_LOSE_INT(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_INT);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la sabiduría del jugador.
 */
static void melee_effect_handler_LOSE_WIS(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_WIS);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la destreza del jugador.
 */
static void melee_effect_handler_LOSE_DEX(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_DEX);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la constitución del jugador.
 */
static void melee_effect_handler_LOSE_CON(melee_effect_handler_context_t *context)
{
	melee_effect_stat(context, STAT_CON);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar todas las estadísticas del jugador.
 */
static void melee_effect_handler_LOSE_ALL(melee_effect_handler_context_t *context)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Dañar (estadísticas) */
	effect_simple(EF_DRAIN_STAT, source_monster(context->mon->midx), "0", STAT_STR, 0, 0, 0, 0, &context->obvious);
	effect_simple(EF_DRAIN_STAT, source_monster(context->mon->midx), "0", STAT_DEX, 0, 0, 0, 0, &context->obvious);
	effect_simple(EF_DRAIN_STAT, source_monster(context->mon->midx), "0", STAT_CON, 0, 0, 0, 0, &context->obvious);
	effect_simple(EF_DRAIN_STAT, source_monster(context->mon->midx), "0", STAT_INT, 0, 0, 0, 0, &context->obvious);
	effect_simple(EF_DRAIN_STAT, source_monster(context->mon->midx), "0", STAT_WIS, 0, 0, 0, 0, &context->obvious);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Causar un terremoto alrededor del jugador.
 */
static void melee_effect_handler_SHATTER(melee_effect_handler_context_t *context)
{
	/* Obvio */
	context->obvious = true;

	/* Reducir daño basado en la clase de armadura del jugador */
	context->damage = adjust_dam_armor(context->damage, context->ac);

	/* Recibir daño */
	if (monster_damage_target(context, false)) return;

	/* Terremoto centrado en el monstruo, radio determinado por el daño */
	if (context->damage > 23) {
		int radius = context->damage / 12;
		effect_simple(EF_EARTHQUAKE, source_monster(context->mon->midx), "0",
					  0, radius, 0, 0, 0, NULL);
	}

	/* Probabilidad de ser empujado */
	if ((context->damage > 100)) {
		int value = context->damage - 100;
		if (randint1(value) > 40) {
			int dist = 1 + value / 40;
			if (context->p) {
				thrust_away(context->mon->grid, context->p->grid, dist);
			} else {
				thrust_away(context->mon->grid, context->t_mon->grid, dist);
			}
		}
	}
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la experiencia del jugador.
 */
static void melee_effect_handler_EXP_10(melee_effect_handler_context_t *context)
{
	melee_effect_experience(context, 95, damroll(10, 6));
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la experiencia del jugador.
 */
static void melee_effect_handler_EXP_20(melee_effect_handler_context_t *context)
{
	melee_effect_experience(context, 90, damroll(20, 6));
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la experiencia del jugador.
 */
static void melee_effect_handler_EXP_40(melee_effect_handler_context_t *context)
{
	melee_effect_experience(context, 75, damroll(40, 6));
}

/**
 * Manejador de efecto cuerpo a cuerpo: Drenar la experiencia del jugador.
 */
static void melee_effect_handler_EXP_80(melee_effect_handler_context_t *context)
{
	melee_effect_experience(context, 50, damroll(80, 6));
}

/**
 * Manejador de efecto cuerpo a cuerpo: Hacer alucinar al jugador.
 *
 * Nótese que no usamos melee_effect_timed(), debido a la diferente función
 * de aprendizaje del monstruo.
 */
static void melee_effect_handler_HALLU(melee_effect_handler_context_t *context)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Aumentar "imagen" */
	if (player_inc_timed(context->p, TMD_IMAGE,
			3 + randint1(context->rlev / 2), true, true, true))
		context->obvious = true;

	/* Aprender sobre el jugador */
	update_smart_learn(context->mon, context->p, 0, 0, ELEM_CHAOS);
}

/**
 * Manejador de efecto cuerpo a cuerpo: Dar al jugador el Aliento Negro.
 *
 * Nótese que no usamos melee_effect_timed(), ya que esto es irresistible.
 */
static void melee_effect_handler_BLACK_BREATH(melee_effect_handler_context_t *context)
{
	/* Recibir daño */
	if (monster_damage_target(context, true)) return;

	/* Aumentar el contador de Aliento Negro una cantidad *pequeña*, tal vez */
	if (one_in_(5) && player_inc_timed(context->p, TMD_BLACKBREATH,
			context->damage / 10, true, true, false)) {
		context->obvious = true;
	}
}

/**
 * ------------------------------------------------------------------------
 * Selección del manejador de combate cuerpo a cuerpo de golpe de monstruo
 * ------------------------------------------------------------------------ */
melee_effect_handler_f melee_handler_for_blow_effect(const char *name)
{
	static const struct effect_handler_s {
		const char *name;
		melee_effect_handler_f function;
	} effect_handlers[] = {
		{ "NONE", melee_effect_handler_NONE },
		{ "HURT", melee_effect_handler_HURT },
		{ "POISON", melee_effect_handler_POISON },
		{ "DISENCHANT", melee_effect_handler_DISENCHANT },
		{ "DRAIN_CHARGES", melee_effect_handler_DRAIN_CHARGES },
		{ "EAT_GOLD", melee_effect_handler_EAT_GOLD },
		{ "EAT_ITEM", melee_effect_handler_EAT_ITEM },
		{ "EAT_FOOD", melee_effect_handler_EAT_FOOD },
		{ "EAT_LIGHT", melee_effect_handler_EAT_LIGHT },
		{ "ACID", melee_effect_handler_ACID },
		{ "ELEC", melee_effect_handler_ELEC },
		{ "FIRE", melee_effect_handler_FIRE },
		{ "COLD", melee_effect_handler_COLD },
		{ "BLIND", melee_effect_handler_BLIND },
		{ "CONFUSE", melee_effect_handler_CONFUSE },
		{ "TERRIFY", melee_effect_handler_TERRIFY },
		{ "PARALYZE", melee_effect_handler_PARALYZE },
		{ "LOSE_STR", melee_effect_handler_LOSE_STR },
		{ "LOSE_INT", melee_effect_handler_LOSE_INT },
		{ "LOSE_WIS", melee_effect_handler_LOSE_WIS },
		{ "LOSE_DEX", melee_effect_handler_LOSE_DEX },
		{ "LOSE_CON", melee_effect_handler_LOSE_CON },
		{ "LOSE_ALL", melee_effect_handler_LOSE_ALL },
		{ "SHATTER", melee_effect_handler_SHATTER },
		{ "EXP_10", melee_effect_handler_EXP_10 },
		{ "EXP_20", melee_effect_handler_EXP_20 },
		{ "EXP_40", melee_effect_handler_EXP_40 },
		{ "EXP_80", melee_effect_handler_EXP_80 },
		{ "HALLU", melee_effect_handler_HALLU },
		{ "BLACK_BREATH", melee_effect_handler_BLACK_BREATH },
		{ NULL, NULL },
	};
	const struct effect_handler_s *current = effect_handlers;

	while (current->name != NULL && current->function != NULL) {
		if (my_stricmp(name, current->name) == 0)
			return current->function;

		current++;
	}

	return NULL;
}