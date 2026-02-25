/**
 * \file cmd-obj.c
 * \brief Manejar objetos de varias maneras
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
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
#include "cmd-core.h"
#include "cmds.h"
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "target.h"
#include "trap.h"

/**
 * ------------------------------------------------------------------------
 * Pequeñas utilidades
 * ------------------------------------------------------------------------
 */

/**
 * Verificar si el jugador puede usar una vara/varita/báculo/objeto activable.
 *
 * \return un valor positivo si el objeto dado puede ser usado; devuelve cero si
 * el objeto no puede ser usado pero podría tener éxito en la repetición (es decir, la comprobación
 * de fallo del dispositivo no pasó pero la tasa de fallo es menor al 100%); devuelve
 * un valor negativo si el objeto no puede ser usado y la repetición no ayudará
 * (sin cargas, necesita recarga, o la tasa de fallo es del 100% o más).
 */
static int check_devices(struct object *obj)
{
	int fail;
	const char *action;
	const char *what = NULL;
	bool activated = false;

	/* Obtener la cadena correcta */
	if (tval_is_rod(obj)) {
		action = "usar la vara";
	} else if (tval_is_wand(obj)) {
		action = "usar la varita";
		what = "varita";
	} else if (tval_is_staff(obj)) {
		action = "usar el báculo";
		what = "báculo";
	} else {
		action = "activarlo";
		activated = true;
	}

	/* Notar báculos vacíos */
	if (what && obj->pval <= 0) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("El %s no tiene cargas restantes.", what);
		return -1;
	}

	/* Calcular la dificultad de uso del objeto */
	fail = get_use_device_chance(obj);

	/* Tirar para el uso */
	if (randint1(1000) < fail) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("No has podido %s correctamente.", action);
		return (fail < 1001) ? 0 : -1;
	}

	/* Notar activaciones */
	if (activated) {
		if (obj->effect)
			obj->known->effect = obj->effect;
		else if (obj->activation)
			obj->known->activation = obj->activation;
	}

	return 1;
}


/**
 * Devolver la probabilidad de que un efecto sea direccional, dado un tval.
 */
static int beam_chance(int tval)
{
	switch (tval)
	{
		case TV_WAND: return 20;
		case TV_ROD:  return 10;
	}

	return 0;
}


/**
 * Imprimir un mensaje de activación de artefacto.
 */
static void activation_message(struct object *obj, const struct player *p)
{
	const char *message;

	/* Ver si tenemos un mensaje, luego imprimirlo */
	if (!obj->activation) return;
	if (!obj->activation->message) return;
	if (obj->artifact && obj->artifact->alt_msg) {
		message = obj->artifact->alt_msg;
	} else {
		message = obj->activation->message;
	}
	print_custom_message(obj, message, MSG_GENERIC, p);
}



/**
 * ------------------------------------------------------------------------
 * Inscripciones
 * ------------------------------------------------------------------------
 */

/**
 * Eliminar inscripción
 */
void do_cmd_uninscribe(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Desinscribir qué objeto?",
			/* Error  */ "No tienes nada que puedas desinscribir.",
			/* Filtro */ obj_has_inscrip,
			/* Elección */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR) != CMD_OK)
		return;

	obj->note = 0;
	msg("Inscripción eliminada.");

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}

/**
 * Añadir inscripción
 */
void do_cmd_inscribe(struct command *cmd)
{
	struct object *obj;
	const char *str;

	char prompt[1024];
	char o_name[80];

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Inscribir qué objeto?",
			/* Error  */ "No tienes nada que inscribir.",
			/* Filtro */ NULL,
			/* Elección */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | IS_HARMLESS) != CMD_OK)
		return;

	/* Formar mensaje */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);
	strnfmt(prompt, sizeof prompt, "Inscribiendo %s.", o_name);

	if (cmd_get_string(cmd, "inscription", &str,
			quark_str(obj->note) /* Por defecto */,
			prompt, "¿Inscribir con qué? ") != CMD_OK)
		return;

	obj->note = quark_add(str);

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * Autoinscribir todos los objetos apropiados
 */
void do_cmd_autoinscribe(struct command *cmd)
{
	if (player_is_shapechanged(player)) return;

	autoinscribe_ground(player);
	autoinscribe_pack(player);

	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * ------------------------------------------------------------------------
 * Quitar/Poner
 * ------------------------------------------------------------------------
 */

/**
 * Quitar un objeto
 */
void do_cmd_takeoff(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Quitarte o soltar qué objeto?",
			/* Error  */ "No tienes nada que quitarte o soltar.",
			/* Filtro */ obj_can_takeoff,
			/* Elección */ USE_EQUIP) != CMD_OK)
		return;

	inven_takeoff(obj);
	combine_pack(player);
	pack_overflow(obj);
	player->upkeep->energy_use = z_info->move_energy / 2;
}


/**
 * Empuñar o usar un objeto
 */
void do_cmd_wield(struct command *cmd)
{
	struct object *equip_obj;
	char o_name[80];
	const char *act;

	unsigned n;

	int slot;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Usar o empuñar qué objeto?",
			/* Error  */ "No tienes nada que usar o empuñar.",
			/* Filtro */ obj_can_wear,
			/* Elección */ USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK)
		return;

	/* Obtener la ranura en la que el objeto quiere ir, y el objeto actualmente ahí */
	slot = wield_slot(obj);
	equip_obj = slot_object(player, slot);

	/* Si la ranura está vacía, empuñar y terminar */
	if (!equip_obj) {
		inven_wield(obj, slot);
		return;
	}

	/* Normalmente si la ranura está ocupada simplemente reemplazaremos el objeto en la ranura,
	 * pero para los anillos necesitamos preguntar al usuario qué ranura realmente
	 * quiere reemplazar */
	if (tval_is_ring(obj)) {
		if (cmd_get_item(cmd, "replace", &equip_obj,
						 /* Mensaje */ "¿Reemplazar qué anillo? ",
						 /* Error  */ "Error en do_cmd_wield(), por favor informa.",
						 /* Filtro */ tval_is_ring,
						 /* Elección */ USE_EQUIP) != CMD_OK)
			return;

		/* Cambiar ranura si es necesario */
		slot = equipped_item_slot(player->body, equip_obj);
	}

	/* Prevenir empuñar en una ranura bloqueada */
	if (!obj_can_takeoff(equip_obj)) {
		object_desc(o_name, sizeof(o_name), equip_obj, ODESC_BASE,
			player);
		msg("No puedes quitarte %s que estás %s.", o_name,
			equip_describe(player, slot));
		return;
	}

	/* "!t" comprueba para quitar */
	n = check_for_inscrip(equip_obj, "!t");
	while (n--) {
		/* Mensaje */
		object_desc(o_name, sizeof(o_name), equip_obj,
			ODESC_PREFIX | ODESC_FULL, player);
		
		/* Olvidarlo */
		if (!get_check(format("¿Realmente quitarte %s? ", o_name))) return;
	}

	/* Describir el objeto */
	object_desc(o_name, sizeof(o_name), equip_obj,
		ODESC_PREFIX | ODESC_FULL, player);

	/* Se quitó el arma */
	if (slot_type_is(player, slot, EQUIP_WEAPON))
		act = "Estabas empuñando";
	/* Se quitó el arco */
	else if (slot_type_is(player, slot, EQUIP_BOW))
		act = "Estabas sujetando";
	/* Se quitó la luz */
	else if (slot_type_is(player, slot, EQUIP_LIGHT))
		act = "Estabas sujetando";
	/* Se quitó otra cosa */
	else
		act = "Llevabas puesto";

	inven_wield(obj, slot);

	/* Mensaje */
	msgt(MSG_WIELD, "%s %s (%c).", act, o_name,
		gear_to_label(player, equip_obj));
}

/**
 * Soltar un objeto
 */
void do_cmd_drop(struct command *cmd)
{
	int amt;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener argumentos */
	if (cmd_get_item(cmd, "item", &obj,
			/* Mensaje */ "¿Soltar qué objeto?",
			/* Error  */ "No tienes nada que soltar.",
			/* Filtro */ NULL,
			/* Elección */ USE_EQUIP | USE_INVEN | USE_QUIVER) != CMD_OK)
		return;

	/* No se pueden quitar objetos bloqueados */
	if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj)) {
		msg("Mmm, parece estar pegado.");
		return;
	}

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	inven_drop(obj, amt);
	player->upkeep->energy_use = z_info->move_energy / 2;
}

/**
 * ------------------------------------------------------------------------
 * Usar objetos de la manera tradicional
 * ------------------------------------------------------------------------
 */

enum use {
	USE_TIMEOUT,
	USE_CHARGE,
	USE_SINGLE
};

/**
 * Usar un objeto de la manera correcta.
 *
 * Devuelve true si los comandos repetidos pueden continuar.
 */
static bool use_aux(struct command *cmd, struct object *obj, enum use use,
					int snd)
{
	struct effect *effect = object_effect(obj);
	bool from_floor = !object_is_carried(player, obj);
	int can_use = 1;
	bool was_aware;
	bool known_aim = false;
	bool none_left = false;
	int dir = 5;
	struct trap_kind *rune = lookup_trap("glyph of warding");

	/* Obtener argumentos */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK) assert(0);

	was_aware = object_flavor_is_aware(obj);

	/* Determinar si sabemos que un objeto necesita ser apuntado */
	if (tval_is_wand(obj) || tval_is_rod(obj) || was_aware ||
		(obj->effect && (obj->known->effect == obj->effect)) ||
		(obj->activation && (obj->known->activation == obj->activation))) {
		known_aim = true;
	}

	if (obj_needs_aim(obj)) {
		/* Las cosas desconocidas sin objetivo obvio obtienen una dirección aleatoria */
		if (!known_aim) {
			dir = ddd[randint0(8)];
		} else if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
			return false;
		}

		/* La confusión estropea la puntería */
		player_confuse_dir(player, &dir, false);
	}

	/* rastrear el objeto usado */
	track_object(player->upkeep, obj);

	/* Verificar efecto */
	assert(effect);

	/* Comprobar uso si es necesario */
	if ((use == USE_CHARGE) || (use == USE_TIMEOUT)) {
		can_use = check_devices(obj);
	}

	/* Ejecutar el efecto */
	if (can_use > 0) {
		int beam = beam_chance(obj->tval);
		int boost, level, charges = 0;
		uint16_t number;
		bool ident = false, describe = false, deduct_before, used;
		struct object *work_obj;
		struct object *first_remainder = NULL;
		char label = '\0';

		if (from_floor) {
			number = obj->number;
		} else {
			label = gear_to_label(player, obj);
			/*
			 * Mostrar un total agregado si la descripción no
			 * tiene un aviso de carga/recarga específico para el
			 * montón.
			 */
			if (use != USE_CHARGE && use != USE_TIMEOUT) {
				number = object_pack_total(player, obj, false,
					&first_remainder);
				if (first_remainder && first_remainder->number
						== number) {
					first_remainder = NULL;
				}
			} else {
				number = obj->number;
			}
		}

		/* Obtener el nivel */
		if (obj->artifact)
			level = obj->artifact->level;
		else if (obj->activation)
			level = obj->activation->level;
		else
			level = obj->kind->level;

		/* Sonido y/o mensaje */
		if (obj->activation) {
			msgt(snd, "Lo activas.");
			activation_message(obj, player);
		} else if (obj->kind->effect_msg) {
			msgt(snd, "%s", obj->kind->effect_msg);
		} else if (obj->kind->vis_msg && !player->timed[TMD_BLIND]) {
			msgt(snd, "%s", obj->kind->vis_msg);
		} else {
			/* ¡Hacer ruido! */
			sound(snd);
		}

		/* Aumentar efectos de daño si la habilidad > dificultad */
		boost = MAX((player->state.skills[SKILL_DEVICE] - level) / 2, 0);

		/*
		 * Si el objeto está en el suelo, deducir tentativamente la
		 * cantidad usada - el efecto podría dejar el objeto inaccesible
		 * dificultando hacerlo después de un uso exitoso. Por la
		 * misma razón, obtener una copia del objeto para usar para propagar
		 * conocimiento y mensajes (también hacerlo para objetos en la mochila
		 * para mantener la lógica posterior más simple). No hacer la deducción para
		 * un objeto en la mochila porque la reorganización de la
		 * mochila, si se usa un montón de un solo objeto de un solo uso, puede distraer
		 * al jugador, ver
		 * https://github.com/angband/angband/issues/5543 .
		 * Si los efectos cambian para que el objeto de origen pueda ser
		 * destruido incluso si está en la mochila, la deducción tendría que
		 * hacerse aquí también si el objeto está en la mochila.
		 */
		if (from_floor) {
			if (use == USE_SINGLE) {
				deduct_before = true;
				work_obj = floor_object_for_use(player, obj, 1,
					false, &none_left);
			} else {
				if (use == USE_CHARGE) {
					deduct_before = true;
					charges = obj->pval;
					/* Usar una sola carga */
					obj->pval--;
				} else if (use == USE_TIMEOUT) {
					deduct_before = true;
					charges = obj->timeout;
					obj->timeout += randcalc(obj->time, 0,
						RANDOMISE);
				} else {
					deduct_before = false;
				}
				work_obj = object_new();
				object_copy(work_obj, obj);
				work_obj->oidx = 0;
				if (obj->known) {
					work_obj->known = object_new();
					object_copy(work_obj->known,
						obj->known);
					work_obj->known->oidx = 0;
				}
			}
		} else {
			deduct_before = false;
			work_obj = object_new();
			object_copy(work_obj, obj);
			work_obj->oidx = 0;
			if (obj->known) {
				work_obj->known = object_new();
				object_copy(work_obj->known, obj->known);
				work_obj->known->oidx = 0;
			}
		}

		/* Hacer efecto; usar original no copia (manejo de efecto de proy) */
		target_fix();
		used = effect_do(effect,
							source_player(),
							obj,
							&ident,
							was_aware,
							dir,
							beam,
							boost,
							cmd);
		target_release();

		if (!used) {
			if (deduct_before) {
				/* Restaurar la deducción tentativa. */
				if (use == USE_SINGLE) {
					/*
					 * Soltar/guardar copia para simplificar
					 * la lógica posterior.
					 */
					struct object *wcopy = object_new();

					object_copy(wcopy, work_obj);
					if (work_obj->known) {
						wcopy->known = object_new();
						object_copy(wcopy->known,
							work_obj->known);
					}
					if (from_floor) {
						drop_near(cave, &wcopy, 0,
							player->grid, false,
							true);
					} else {
						inven_carry(player, wcopy,
							true, false);
					}
				} else if (use == USE_CHARGE) {
					obj->pval = charges;
				} else if (use == USE_TIMEOUT) {
					obj->timeout = charges;
				}
			}

			/*
			 * Salir si el objeto no se usó y no se ganó conocimiento
			 */
			if (was_aware || !ident) {
				if (work_obj->known) {
					object_delete(player->cave, NULL, &work_obj->known);
				}
				object_delete(cave, player->cave, &work_obj);
				/*
				 * La selección del objetivo del efecto puede haber
				 * desencadenado una actualización de ventanas mientras la
				 * deducción tentativa estaba en efecto; señalar
				 * otra actualización para remediar eso.
				 */
				if (deduct_before) {
					assert(from_floor);
					player->upkeep->redraw |= (PR_OBJECT);
				}
				return false;
			}
		}

		/* Aumentar conocimiento */
		if (use == USE_SINGLE) {
			/* Los objetos de un solo uso se aprenden automáticamente */
			if (!was_aware) {
				object_learn_on_use(player, work_obj);
			}
			describe = true;
		} else {
			/* Los objetos usables pueden necesitar actualización, otras cosas se vuelven conocidas o probadas */
			if (tval_is_wearable(work_obj)) {
				update_player_object_knowledge(player);
			} else if (!was_aware && ident) {
				object_learn_on_use(player, work_obj);
				describe = true;
			} else {
				object_flavor_tried(work_obj);
			}
		}

		/*
		 * Usar, deducir carga, o aplicar tiempo de espera si no se
		 * hizo antes. Para cargas o tiempos de espera, también hay que cambiar
		 * work_obj ya que se usa para mensajes (para objetos de un solo
		 * uso, ODESC_ALTNUM significa que el número de work_obj no
		 * necesita ser ajustado).
		 */
		if (used && !deduct_before) {
			assert(!from_floor);
			if (use == USE_CHARGE) {
				obj->pval--;
				work_obj->pval--;
			} else if (use == USE_TIMEOUT) {
				int adj = randcalc(obj->time, 0, RANDOMISE);

				obj->timeout += adj;
				work_obj->timeout += adj;
			} else if (use == USE_SINGLE) {
				struct object *used_obj = gear_object_for_use(
					player, obj, 1, false, &none_left);

				if (used_obj->known) {
					object_delete(cave, player->cave,
						&used_obj->known);
				}
				object_delete(cave, player->cave, &used_obj);
			}
		}

		if (describe) {
			/*
			 * Describir lo que queda de objetos de un solo uso o
			 * objetos recién identificados de todo tipo.
			 */
			char name[80];

			object_desc(name, sizeof(name), work_obj,
				ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
				((number + ((used && use == USE_SINGLE) ?
				-1 : 0)) << 16), player);
			if (from_floor) {
				/* Imprimir un mensaje */
				msg("Ves %s.", name);
			} else if (first_remainder) {
				label = gear_to_label(player, first_remainder);
				msg("Tienes %s (1er %c).", name, label);
			} else {
				msg("Tienes %s (%c).", name, label);
			}
		} else if (used && use == USE_CHARGE) {
			/* Describir cargas */
			if (from_floor)
				floor_item_charges(work_obj);
			else
				inven_item_charges(work_obj);
		}

		/* Limpiar copia creada. */
		if (work_obj->known)
			object_delete(player->cave, NULL, &work_obj->known);
		object_delete(cave, player->cave, &work_obj);
	}

	/* Usar el turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Autoinscribir si estamos garantizados de que todavía tenemos alguno */
	if (!none_left && !from_floor)
		apply_autoinscription(player, obj);

	/* Marcar como probado y redibujar */
	player->upkeep->notice |= (PN_COMBINE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_OBJECT);

	/* Truco para hacer que el Glifo de Protección funcione correctamente */
	if (square_trap_specific(cave, player->grid, rune->tidx)) {
		/* Empujar objetos fuera de la casilla */
		if (square_object(cave, player->grid))
			push_object(player->grid);
	}

	return can_use == 0;
}


/**
 * Leer un pergamino
 */
void do_cmd_read_scroll(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Comprobar que el jugador puede usar pergaminos */
	if (!player_can_read(player, true))
		return;

	/* Obtener el pergamino */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Leer qué pergamino? ",
			"No tienes pergaminos para leer.",
			tval_is_scroll,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_GENERIC);
}

/**
 * Usar un báculo
 */
void do_cmd_use_staff(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Usar qué báculo? ",
			"No tienes báculos para usar.",
			tval_is_staff,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_has_charges(obj)) {
		msg("Ese báculo no tiene cargas.");
		cmd_set_repeat(0);
		return;
	}

	/* Deshabilitar autorepetición cuando tiene éxito. */
	if (!use_aux(cmd, obj, USE_CHARGE, MSG_USE_STAFF)) {
		cmd_set_repeat(0);
	}
}

/**
 * Apuntar una varita
 */
void do_cmd_aim_wand(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Apuntar qué varita? ",
			"No tienes varitas para apuntar.",
			tval_is_wand,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_has_charges(obj)) {
		msg("Esa varita no tiene cargas.");
		cmd_set_repeat(0);
		return;
	}

	/* Deshabilitar autorepetición cuando tiene éxito. */
	if (!use_aux(cmd, obj, USE_CHARGE, MSG_ZAP_ROD)) {
		cmd_set_repeat(0);
	}
}

/**
 * Activar una vara
 */
void do_cmd_zap_rod(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Activar qué vara? ",
			"No tienes varas para activar.",
			tval_is_rod,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_can_zap(obj)) {
		msg("Esa vara aún se está recargando.");
		cmd_set_repeat(0);
		return;
	}

	/* Deshabilitar autorepetición cuando tiene éxito. */
	if (!use_aux(cmd, obj, USE_TIMEOUT, MSG_ZAP_ROD)) {
		cmd_set_repeat(0);
	}
}

/**
 * Activar un objeto
 */
void do_cmd_activate(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Activar qué objeto? ",
			"No tienes objetos para activar.",
			obj_is_activatable,
			USE_EQUIP | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_can_activate(obj)) {
		msg("Ese objeto aún se está recargando.");
		cmd_set_repeat(0);
		return;
	}

	/* Deshabilitar autorepetición cuando tiene éxito. */
	if (!use_aux(cmd, obj, USE_TIMEOUT, MSG_ACT_ARTIFACT)) {
		cmd_set_repeat(0);
	}
}

/**
 * Comer algo
 */
void do_cmd_eat_food(struct command *cmd)
{
	struct object *obj;

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Comer qué alimento? ",
			"No tienes alimento para comer.",
			tval_is_edible,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_EAT);
}

/**
 * Beber una poción
 */
void do_cmd_quaff_potion(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Beber qué poción? ",
			"No tienes pociones para beber.",
			tval_is_potion,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_QUAFF);
}

/**
 * Usar cualquier objeto usable
 */
void do_cmd_use(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Usar qué objeto? ",
			"No tienes objetos para usar.",
			obj_is_useable,
			USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | SHOW_FAIL | QUIVER_TAGS | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	/*
	 * Para báculos, varas, varitas, o activación de objeto equipado, actuar como si el
	 * comando específico del objeto se hubiera invocado directamente: cmd-core.c
	 * habilita automáticamente la repetición para esos comandos si no se estableció manualmente
	 * un contador de repetición.
	 */
	if (tval_is_ammo(obj)) {
		do_cmd_fire(cmd);
	} else if (tval_is_potion(obj)) {
		do_cmd_quaff_potion(cmd);
	} else if (tval_is_edible(obj)) {
		do_cmd_eat_food(cmd);
	} else if (tval_is_rod(obj)) {
		if (cmd->nrepeats == 0) {
			cmd->nrepeats = 99;
		}
		do_cmd_zap_rod(cmd);
	} else if (tval_is_wand(obj)) {
		if (cmd->nrepeats == 0) {
			cmd->nrepeats = 99;
		}
		do_cmd_aim_wand(cmd);
	} else if (tval_is_staff(obj)) {
		if (cmd->nrepeats == 0) {
			cmd->nrepeats = 99;
		}
		do_cmd_use_staff(cmd);
	} else if (tval_is_scroll(obj)) {
		do_cmd_read_scroll(cmd);
	} else if (obj_can_refill(obj)) {
		do_cmd_refill(cmd);
	} else if (obj_is_activatable(obj)) {
		if (object_is_equipped(player->body, obj)) {
			if (cmd->nrepeats == 0) {
				cmd->nrepeats = 99;
			}
			do_cmd_activate(cmd);
		} else {
			msg("Equipa el objeto para usarlo.");
		}
	} else {
		msg("El objeto no se puede usar en este momento");
	}
}


/**
 * ------------------------------------------------------------------------
 * Recarga
 * ------------------------------------------------------------------------
 */

static void refill_lamp(struct object *lamp, struct object *obj)
{
	/* Recargar */
	lamp->timeout += obj->timeout ? obj->timeout : obj->pval;

	/* Mensaje */
	msg("Recargas tu lámpara.");

	/* Comentario */
	if (lamp->timeout >= z_info->fuel_lamp) {
		lamp->timeout = z_info->fuel_lamp;
		msg("Tu lámpara está llena.");
	}

	/* Recargado desde una linterna */
	if (of_has(obj->flags, OF_TAKES_FUEL)) {
		/* Desapilar si es necesario */
		if (obj->number > 1) {
			/* Obtener un objeto local, dividir */
			struct object *used = object_split(obj, 1);

			/* Eliminar combustible */
			used->timeout = 0;

			/* Llevar o soltar */
			if (object_is_carried(player, obj) && inven_carry_okay(used))
				inven_carry(player, used, true, true);
			else
				drop_near(cave, &used, 0, player->grid, false, true);
		} else
			/* Vaciar una linterna individual */
			obj->timeout = 0;

		/* Combinar la mochila (más tarde) */
		player->upkeep->notice |= (PN_COMBINE);

		/* Redibujar cosas */
		player->upkeep->redraw |= (PR_INVEN);
	} else { /* Recargado desde un frasco */
		struct object *used;
		bool none_left = false;

		/* Disminuir el objeto de la mochila o del suelo */
		if (object_is_carried(player, obj)) {
			used = gear_object_for_use(player, obj, 1, true,
				&none_left);
		} else {
			used = floor_object_for_use(player, obj, 1, true,
				&none_left);
		}
		if (used->known)
			object_delete(player->cave, NULL, &used->known);
		object_delete(cave, player->cave, &used);
	}

	/* Recalcular antorcha */
	player->upkeep->update |= (PU_TORCH);

	/* Redibujar cosas */
	player->upkeep->redraw |= (PR_EQUIP);
}


void do_cmd_refill(struct command *cmd)
{
	struct object *light = equipped_item_by_slot_name(player, "light");
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Comprobar lo que estamos empuñando. */
	if (!light || !tval_is_light(light)) {
		msg("No estás empuñando una luz.");
		return;
	} else if (of_has(light->flags, OF_NO_FUEL)
			|| !of_has(light->flags, OF_TAKES_FUEL)) {
		msg("Tu luz no se puede recargar.");
		return;
	}

	/* Obtener un objeto */
	if (cmd_get_item(cmd, "item", &obj,
			"¿Recargar con qué fuente de combustible? ",
			"No tienes nada con lo que recargar.",
			obj_can_refill,
			USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK) return;

	refill_lamp(light, obj);

	player->upkeep->energy_use = z_info->move_energy / 2;
}



/**
 * ------------------------------------------------------------------------
 * Lanzar hechizos
 * ------------------------------------------------------------------------
 */

/**
 * Lanzar un hechizo de un libro
 */
void do_cmd_cast(struct command *cmd)
{
	int spell_index, dir = 0;
	const struct class_spell *spell;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Comprobar que el jugador puede lanzar hechizos en absoluto */
	if (!player_can_cast(player, true))
		return;

	/* Obtener argumentos */
	if (cmd_get_spell(cmd, "spell", player, &spell_index,
			/* Verbo */ "lanzar",
			/* Libro */ obj_can_cast_from,
			/* Error de libro */ "No hay hechizos que puedas lanzar.",
			/* Filtro */ spell_okay_to_cast,
			/* Error de hechizo */ "Ese libro no tiene hechizos que puedas lanzar.") != CMD_OK) {
		return;
	}

	/* Obtener el hechizo */
	spell = spell_by_index(player, spell_index);

	/* Verificar hechizos "peligrosos" */
	if (spell->smana > player->csp) {
		const char *verb = spell->realm->verb;
		const char *noun = spell->realm->spell_noun;

		/* Advertencia */
		msg("No tienes suficiente maná para %s este %s.", verb, noun);

		/* Vaciar entrada */
		event_signal(EVENT_INPUT_FLUSH);

		/* Verificar */
		if (!get_check("¿Intentarlo de todas formas? ")) return;
	}

	if (spell_needs_aim(spell_index)) {
		if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
			player_confuse_dir(player, &dir, false);
		else
			return;
	}

	/* Lanzar un hechizo */
	target_fix();
	if (spell_cast(spell_index, dir, cmd)) {
		if (player->timed[TMD_FASTCAST]) {
			player->upkeep->energy_use = (z_info->move_energy * 3) / 4;
		} else {
			player->upkeep->energy_use = z_info->move_energy;
		}
	}
	target_release();
}


/**
 * Aprender un hechizo específico, especificado por número de hechizo (para magos).
 */
void do_cmd_study_spell(struct command *cmd)
{
	int spell_index;

	/* Comprobar que el jugador puede estudiar ahora mismo */
	if (!player_can_study(player, true))
		return;

	if (cmd_get_spell(cmd, "spell", player, &spell_index,
			/* Verbo */ "estudiar",
			/* Libro */ obj_can_study,
			/* Error de libro */ "No puedes aprender nuevos hechizos de los libros que tienes.",
			/* Filtro */ spell_okay_to_study,
			/* Error de hechizo */ "Ese libro no tiene hechizos que puedas aprender.") != CMD_OK)
		return;

	spell_learn(spell_index);
	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * Aprender un hechizo aleatorio del libro dado (para sacerdotes)
 */
void do_cmd_study_book(struct command *cmd)
{
	struct object *book_obj;
	const struct class_book *book;
	int spell_index = -1;
	struct class_spell *spell;
	int i, k = 0;

	/* Comprobar que el jugador puede estudiar ahora mismo */
	if (!player_can_study(player, true))
		return;

	if (cmd_get_item(cmd, "item", &book_obj,
			/* Mensaje */ "¿Estudiar qué libro? ",
			/* Error  */ "No puedes aprender nuevos hechizos de los libros que tienes.",
			/* Filtro */ obj_can_study,
			/* Elección */ USE_INVEN | USE_FLOOR) != CMD_OK)
		return;

	book = player_object_to_book(player, book_obj);
	track_object(player->upkeep, book_obj);
	handle_stuff(player);

	for (i = 0; i < book->num_spells; i++) {
		spell = &book->spells[i];
		if (!spell_okay_to_study(player, spell->sidx))
			continue;
		if ((++k > 1) && (randint0(k) != 0))
			continue;
		spell_index = spell->sidx;
	}

	if (spell_index < 0) {
		msg("No puedes aprender ningún %s en ese libro.", book->realm->spell_noun);
	} else {
		spell_learn(spell_index);
		player->upkeep->energy_use = z_info->move_energy;
	}
}

/**
 * Elegir la forma de estudiar. Elegir vida. Elegir una carrera. Elegir familia.
 * Elegir un puto monstruo enorme, elegir chamanes orcos, kobolds, druidas elfos oscuros,
 * y Mim, el Traidor de Túrin.
 */
void do_cmd_study(struct command *cmd)
{
	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	if (player_has(player, PF_CHOOSE_SPELLS))
		do_cmd_study_spell(cmd);
	else
		do_cmd_study_book(cmd);
}