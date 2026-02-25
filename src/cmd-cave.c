/**
 * \file cmd-cave.c
 * \brief Apertura/cierre de cofres y puertas, desarme, correr, descansar, etc.
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
#include "cmd-core.h"
#include "cmds.h"
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "init.h"
#include "mon-attack.h"
#include "mon-desc.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-spell.h"
#include "mon-timed.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-path.h"
#include "player-quest.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "store.h"
#include "trap.h"

/**
 * Subir un nivel
 */
void do_cmd_go_up(struct command *cmd)
{
	int ascend_to;

	/* Verificar escaleras */
	if (!square_isupstairs(cave, player->grid)) {
		if (OPT(player, autoexplore_commands)) {
			do_cmd_navigate_up(cmd);
		} else {
			msg("No veo una escalera para subir aquí.");
		}
		return;
	}

	/* Forzar descenso */
	if (OPT(player, birth_force_descend)) {
		msg("¡No pasa nada!");
		return;
	}
	
	ascend_to = dungeon_get_next_level(player, player->depth, -1);
	
	if (ascend_to == player->depth) {
		msg("¡No puedes subir desde aquí!");
		return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Éxito */
	msgt(MSG_STAIRS_UP, "Entras en un laberinto de escaleras que suben.");

	/* Crear un camino de vuelta */
	player->upkeep->create_up_stair = false;
	player->upkeep->create_down_stair = true;
	
	/* Cambiar de nivel */
	dungeon_change_level(player, ascend_to);
}


/**
 * Bajar un nivel
 */
void do_cmd_go_down(struct command *cmd)
{
	int descend_to = dungeon_get_next_level(player, player->depth, 1);

	/* Verificar escaleras */
	if (!square_isdownstairs(cave, player->grid)) {
		if (OPT(player, autoexplore_commands)) {
			do_cmd_navigate_down(cmd);
		} else {
			msg("No veo una escalera para bajar aquí.");
		}
		return;
	}

	/* Paranoia, no se puede bajar de z_info->max_depth - 1 */
	if (player->depth == z_info->max_depth - 1) {
		msg("La mazmorra no parece extenderse más profundo");
		return;
	}

	/* Advertir a un jugador con force_descend si va a un nivel de misión */
	if (OPT(player, birth_force_descend)) {
		descend_to = dungeon_get_next_level(player,
			player->max_depth, 1);
		if (is_quest(player, descend_to) &&
			!get_check("¿Estás seguro de que quieres bajar? "))
			return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Éxito */
	msgt(MSG_STAIRS_DOWN, "Entras en un laberinto de escaleras que bajan.");

	/* Crear un camino de vuelta */
	player->upkeep->create_up_stair = true;
	player->upkeep->create_down_stair = false;

	/* Cambiar de nivel */
	dungeon_change_level(player, descend_to);
}



/**
 * Determina si una casilla dada puede ser "abierta"
 */
static bool do_cmd_open_test(struct player *p, struct loc grid)
{
	/* Debe ser conocida */
	if (!square_isknown(cave, grid)) {
		msg("No ves nada ahí.");
		return false;
	}

	/* Debe ser una puerta cerrada */
	if (!square_iscloseddoor(cave, grid)) {
		msgt(MSG_NOTHING_TO_OPEN, "No ves nada que abrir ahí.");
		if (square_iscloseddoor(p->cave, grid)) {
			square_forget(cave, grid);
			square_light_spot(cave, grid);
		}
		return false;
	}

	return (true);
}


/**
 * Realiza la acción básica de "abrir" en puertas
 *
 * Asume que no hay ningún monstruo bloqueando el destino
 *
 * Devuelve true si los comandos repetidos pueden continuar
 */
static bool do_cmd_open_aux(struct loc grid)
{
	bool more = false;

	/* Verificar legalidad */
	if (!do_cmd_open_test(player, grid)) return (false);

	/* Puerta cerrada con llave */
	if (square_islockeddoor(cave, grid)) {
		int chance = calc_unlocking_chance(player,
			square_door_power(cave, grid), no_light(player));

		if (randint0(100) < chance) {
			/* Mensaje */
			msgt(MSG_LOCKPICK, "Has forzado la cerradura.");

			/* Abrir la puerta */
			square_open_door(cave, grid);

			/* Actualizar lo visual */
			square_memorize(cave, grid);
			square_light_spot(cave, grid);
			player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

			/* Experiencia */
			/* Eliminado para evitar explotación al cerrar y abrir repetidamente */
			/* player_exp_gain(player, 1); */
		} else {
			event_signal(EVENT_INPUT_FLUSH);

			/* Mensaje */
			msgt(MSG_LOCKPICK_FAIL, "No has podido forzar la cerradura.");

			/* Podemos seguir intentando */
			more = true;
		}
	} else {
		/* Puerta cerrada */
		square_open_door(cave, grid);
		square_memorize(cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_OPENDOOR);
	}

	/* Resultado */
	return (more);
}



/**
 * Abrir una puerta cerrada/con llave/atascada o un cofre cerrado/con llave.
 *
 * Abrir un cofre con llave vale un punto de experiencia; como las puertas
 * pueden ser cerradas con llave por el jugador, no hay experiencia por abrir puertas.
 */
void do_cmd_open(struct command *cmd)
{
	struct loc grid;
	int dir;
	struct object *obj;
	bool more = false;
	int err;
	struct monster *mon;

	/* Obtener argumentos */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_closed_doors, n_locked_chests;

		n_closed_doors = count_feats(&grid1, square_iscloseddoor, false);
		n_locked_chests = count_chests(&grid1, CHEST_OPENABLE);

		/*
		 * Si se pide una dirección, permitir la casilla del jugador como
		 * opción si hay un cofre cerca.
		 */
		if (n_closed_doors + n_locked_chests == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, n_locked_chests > 0)) {
			return;
		}
	}

	/* Obtener ubicación */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Buscar cofre */
	obj = chest_check(player, grid, CHEST_OPENABLE);

	/* Buscar puerta */
	if (!obj && !do_cmd_open_test(player, grid)) {
		/* Cancelar repetición */
		disturb(player);
		return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);

		/* Buscar cofre */
		obj = chest_check(player, grid, CHEST_OPENABLE);
	}

	/* Monstruo */
	mon = square_monster(cave, grid);
	if (mon) {
		/* Los monstruos camuflados sorprenden al jugador */
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			/* El monstruo camuflado se despierta y se da cuenta */
			monster_wake(mon, false, 100);
		} else {
			/* Mensaje */
			msg("¡Hay un monstruo en medio!");

			/* Atacar */
			py_attack(player, grid);
		}
	} else if (obj) {
		/* Cofre */
		more = do_cmd_open_chest(grid, obj);
	} else {
		/* Puerta */
		more = do_cmd_open_aux(grid);
	}

	/* Cancelar repetición a menos que podamos continuar */
	if (!more) disturb(player);
}


/**
 * Determina si una casilla dada puede ser "cerrada"
 */
static bool do_cmd_close_test(struct player *p, struct loc grid)
{
	/* Debe ser conocida */
	if (!square_isknown(cave, grid)) {
		/* Mensaje */
		msg("No ves nada ahí.");

		/* No */
		return (false);
	}

 	/* Requiere puerta abierta/rota */
	if (!square_isopendoor(cave, grid) && !square_isbrokendoor(cave, grid)) {
		/* Mensaje */
		msg("No ves nada que cerrar ahí.");
		if (square_isopendoor(p->cave, grid)
				|| square_isbrokendoor(p->cave, grid)) {
			square_forget(cave, grid);
			square_light_spot(cave, grid);
		}

		/* No */
		return (false);
	}

	/* No permitir si el jugador está en medio. */
	if (square(cave, grid)->mon < 0) {
		/* Mensaje */
		msg("Estás parado en esa puerta.");

		/* No */
		return (false);
	}

	/* Ok */
	return (true);
}


/**
 * Realiza la acción básica de "cerrar"
 *
 * Asume que no hay ningún monstruo bloqueando el destino
 *
 * Devuelve true si los comandos repetidos pueden continuar
 */
static bool do_cmd_close_aux(struct loc grid)
{
	bool more = false;

	/* Verificar legalidad */
	if (!do_cmd_close_test(player, grid)) return (false);

	/* Puerta rota */
	if (square_isbrokendoor(cave, grid)) {
		msg("La puerta parece estar rota.");
	} else {
		/* Cerrar puerta */
		square_close_door(cave, grid);
		square_memorize(cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
		sound(MSG_SHUTDOOR);
	}

	/* Resultado */
	return (more);
}


/**
 * Cerrar una puerta abierta.
 */
void do_cmd_close(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	bool more = false;

	/* Obtener argumentos */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;

		/* Contar puertas abiertas */
		if (count_feats(&grid1, square_isopendoor, false) == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, false)) {
			return;
		}
	}

	/* Obtener ubicación */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Verificar legalidad */
	if (!do_cmd_close_test(player, grid)) {
		/* Cancelar repetición */
		disturb(player);
		return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Monstruo - alertar, luego atacar */
	if (square(cave, grid)->mon > 0) {
		msg("¡Hay un monstruo en medio!");
		py_attack(player, grid);
	} else
		/* Puerta - cerrarla */
		more = do_cmd_close_aux(grid);

	/* Cancelar repetición a menos que se indique lo contrario */
	if (!more) disturb(player);
}


/**
 * Determina si una casilla dada puede ser "excavada"
 */
static bool do_cmd_tunnel_test(struct player *p, struct loc grid)
{

	/* Debe ser conocida */
	if (!square_isknown(cave, grid)) {
		msg("No ves nada ahí.");
		return (false);
	}

	/* Titanio */
	if (square_isperm(cave, grid)) {
		msg("Esto parece ser roca permanente.");
		if (!square_isperm(p->cave, grid)) {
			square_memorize(cave, grid);
			square_light_spot(cave, grid);
		}
		return (false);
	}

	/* Debe ser una pared/puerta/etc */
	if (!(square_isdiggable(cave, grid) || square_iscloseddoor(cave, grid))) {
		msg("No ves nada que excavar ahí.");
		if (square_isdiggable(p->cave, grid)
				|| square_iscloseddoor(p->cave, grid)) {
			square_forget(cave, grid);
			square_light_spot(cave, grid);
		}
		return (false);
	}

	/* Ok */
	return (true);
}


/**
 * Excavar a través de una pared. Asume ubicación válida.
 *
 * Nótese que es imposible "extender" habitaciones más allá de sus
 * muros exteriores (que en realidad son parte de la habitación).
 *
 * Intentar hacerlo producirá casillas de suelo que no son parte
 * de la habitación, y cuyo estado de "iluminación" no cambia con
 * el resto de la habitación.
 */
static bool twall(struct loc grid)
{
	/* Paranoia -- Requiere una pared o puerta o algo similar */
	if (!(square_isdiggable(cave, grid) || square_iscloseddoor(cave, grid)))
		return (false);

	/* Sonido */
	sound(MSG_DIG);

	/* Olvidar la pared */
	square_forget(cave, grid);

	/* Eliminar la característica */
	square_tunnel_wall(cave, grid);

	/* Actualizar lo visual */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

	/* Resultado */
	return (true);
}


/**
 * Realiza la acción básica de "excavar"
 *
 * Asume que ningún monstruo bloquea el destino.
 * Usa twall() (arriba) para hacer toda la "modificación del terreno".
 * Devuelve true si los comandos repetidos pueden continuar.
 */
static bool do_cmd_tunnel_aux(struct loc grid)
{
	bool more = false;
	int digging_chances[DIGGING_MAX], chance;
	bool okay = false;
	bool gold = square_hasgoldvein(cave, grid);
	bool rubble = square_isrubble(cave, grid);
	bool digger_swapped = false;
	int weapon_slot = slot_by_name(player, "weapon");
	struct object *current_weapon = slot_object(player, weapon_slot);
	struct object *best_digger = NULL;
	struct player_state local_state;
	struct player_state *used_state = &player->state;
	int oldn = 1, dig_idx;
	const char *with_clause = current_weapon == NULL ? "con las manos" : "con tu arma";

	/* Verificar legalidad */
	if (!do_cmd_tunnel_test(player, grid)) return (false);

	/* Encontrar con qué estamos excavando y nuestra probabilidad de éxito */
	best_digger = player_best_digger(player, false);
	if (best_digger != current_weapon &&
			(!current_weapon || obj_can_takeoff(current_weapon))) {
		digger_swapped = true;
		with_clause = "con tu pico de intercambio";
		/* Usar solo uno sin la sobrecarga de gear_obj_for_use(). */
		if (best_digger) {
			oldn = best_digger->number;
			best_digger->number = 1;
		}
		player->body.slots[weapon_slot].obj = best_digger;
		memcpy(&local_state, &player->state, sizeof(local_state));
		/*
		 * Evitar efectos secundarios de usar update establecido a false con
		 * calc_bonuses().
		 */
		local_state.stat_ind[STAT_STR] = 0;
		local_state.stat_ind[STAT_DEX] = 0;
		calc_bonuses(player, &local_state, false, false);
		used_state = &local_state;
	}
	calc_digging_chances(used_state, digging_chances);

	/* ¿Tenemos éxito? */
	dig_idx = square_digging(cave, grid);
	if (dig_idx < 1 || dig_idx > DIGGING_MAX) {
		msg("%s tiene la probabilidad de excavar mal configurada; por favor, informa de este error.",
			(square_feat(cave, grid)->name) ?
			square_feat(cave, grid)->name :
			format("Terrain index %d", square_feat(cave, grid)->fidx));
		dig_idx = DIGGING_GRANITE + 1;
	}
	chance = digging_chances[dig_idx - 1];
	okay = (chance > randint0(1600));

	/* Intercambiar de vuelta */
	if (digger_swapped) {
		if (best_digger) {
			best_digger->number = oldn;
		}
		player->body.slots[weapon_slot].obj = current_weapon;
	}

	/* Éxito */
	if (okay && twall(grid)) {
		/* Los escombros son un caso especial - podría manejarse de forma más general NRM */
		if (rubble) {
			/* Mensaje */
			msg("Has quitado los escombros %s.", with_clause);

			/* Colocar un objeto (excepto en la ciudad) */
			if ((randint0(100) < 10) && player->depth) {
				/* Crear un objeto simple */
				place_object(cave, grid, player->depth, false, false,
							 ORIGIN_RUBBLE, 0);

				/* Observar el nuevo objeto */
				if (square_object(cave, grid)
						&& !ignore_item_ok(player,
						square_object(cave, grid))
						&& square_isseen(cave, grid)) {
					msg("¡Has encontrado algo!");
				}
			} 
		} else if (gold) {
			/* Encontró tesoro */
			place_gold(cave, grid, player->depth, ORIGIN_FLOOR);
			msg("¡Has encontrado algo excavando %s!", with_clause);
		} else {
			msg("Has terminado el túnel %s.", with_clause);
		}
		/* En la superficie, el nuevo terreno puede quedar expuesto al sol. */
		if (cave->depth == 0) expose_to_sun(cave, grid, is_daytime());
		/* Actualizar lo visual. */
		square_memorize(cave, grid);
		square_light_spot(cave, grid);
		player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	} else if (chance > 0) {
		/* Fracaso, continuar excavando */
		if (rubble)
			msg("Excavas entre los escombros %s.", with_clause);
		else
			msg("Excavas en %s %s.",
				square_apparent_name(player->cave, grid), with_clause);
		more = true;
	} else {
		/* No repetir automáticamente si no hay esperanza. */
		if (rubble) {
			msg("Excavas entre los escombros %s con poco efecto.", with_clause);
		} else {
			msg("Martilleas sin resultado %s contra %s.", with_clause,
				square_apparent_name(player->cave, grid));
		}
	}

	/* Resultado */
	return (more);
}


/**
 * Excavar a través de "paredes" (incluyendo escombros y puertas, secretas o no)
 *
 * Excavar es muy difícil sin un arma "excavadora", pero puede ser
 * realizado por jugadores fuertes usando armas pesadas.
 */
void do_cmd_tunnel(struct command *cmd)
{
	struct loc grid;
	int dir;
	bool more = false;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false))
		return;

	/* Obtener ubicación */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Ups */
	if (!do_cmd_tunnel_test(player, grid)) {
		/* Cancelar repetición */
		disturb(player);
		return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Atacar cualquier monstruo con el que nos topemos */
	if (square(cave, grid)->mon > 0) {
		msg("¡Hay un monstruo en medio!");
		py_attack(player, grid);
	} else {
		/* Excavar a través de paredes */
		more = do_cmd_tunnel_aux(grid);
	}

	/* Cancelar repetición a menos que podamos continuar */
	if (!more) disturb(player);
}

/**
 * Determina si una casilla dada puede ser "desarmada"
 */
static bool do_cmd_disarm_test(struct player *p, struct loc grid)
{
	/* Debe ser conocida */
	if (!square_isknown(cave, grid)) {
		msg("No ves nada ahí.");
		return false;
	}

	/* Buscar una puerta cerrada sin llave para cerrar con llave */
	if (square_iscloseddoor(cave, grid) && !square_islockeddoor(cave, grid))
		return true;

	/* Buscar una trampa */
	if (!square_isdisarmabletrap(cave, grid)) {
		msg("No ves nada que desarmar ahí.");
		if (square_isdisarmabletrap(p->cave, grid)) {
			square_memorize_traps(cave, grid);
			square_light_spot(cave, grid);
		}
		return false;
	}

	/* Ok */
	return true;
}


/**
 * Realiza el comando "cerrar con llave"
 *
 * Asume que no hay ningún monstruo bloqueando el destino
 *
 * Devuelve true si los comandos repetidos pueden continuar
 */
static bool do_cmd_lock_door(struct loc grid)
{
	int i, j, power;
	bool more = false;

	/* Verificar legalidad */
	if (!do_cmd_disarm_test(player, grid)) return false;

	/* Obtener el factor de "desarme" */
	i = player->state.skills[SKILL_DISARM_PHYS];

	/* Penalizar algunas condiciones */
	if (player->timed[TMD_BLIND] || no_light(player))
		i = i / 10;
	if (player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE])
		i = i / 10;

	/* Calcular "poder" de la cerradura */
	power = m_bonus(7, player->depth);

	/* Extraer la dificultad */
	j = i - power;

	/* Tener siempre una pequeña probabilidad de éxito */
	if (j < 2) j = 2;

	/* Éxito */
	if (randint0(100) < j) {
		msg("Cierras la puerta con llave.");
		square_set_door_lock(cave, grid, power);
	}

	/* Fracaso -- Seguir intentando */
	else if ((i > 5) && (randint1(i) > 5)) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("No has podido cerrar la puerta con llave.");

		/* Podemos seguir intentando */
		more = true;
	}
	/* Fracaso */
	else
		msg("No has podido cerrar la puerta con llave.");

	/* Resultado */
	return more;
}


/**
 * Realiza la acción básica de "desarmar"
 *
 * Asume que no hay ningún monstruo bloqueando el destino
 *
 * Devuelve true si los comandos repetidos pueden continuar
 */
static bool do_cmd_disarm_aux(struct loc grid)
{
	int skill, power, chance;
    struct trap *trap = square(cave, grid)->trap;
	bool more = false;

	/* Verificar legalidad */
	if (!do_cmd_disarm_test(player, grid)) return (false);

    /* Elegir la primera trampa de jugador */
	while (trap) {
		if (trf_has(trap->flags, TRF_TRAP))
			break;
		trap = trap->next;
	}
	if (!trap)
		return false;

	/* Obtener la habilidad base de desarme */
	if (trf_has(trap->flags, TRF_MAGICAL))
		skill = player->state.skills[SKILL_DISARM_MAGIC];
	else
		skill = player->state.skills[SKILL_DISARM_PHYS];

	/* Penalizar algunas condiciones */
	if (player->timed[TMD_BLIND] ||
			no_light(player) ||
			player->timed[TMD_CONFUSED] ||
			player->timed[TMD_IMAGE])
		skill = skill / 10;

	/* Extraer poder de la trampa */
	power = cave->depth / 5;

	/* Extraer el porcentaje de éxito */
	chance = skill - power;

	/* Tener siempre una pequeña probabilidad de éxito */
	if (chance < 2) chance = 2;

	/* Dos oportunidades - una para desarmar, otra para no activar la trampa */
	if (randint0(100) < chance) {
		msgt(MSG_DISARM, "Has desarmado %s.", trap->kind->name);
		player_exp_gain(player, 1 + power);

		/* La trampa ha desaparecido */
		if (!square_remove_trap(cave, grid, trap, true)) {
			assert(0);
		}
	} else if (randint0(100) < chance) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("No has podido desarmar %s.", trap->kind->name);

		/* El jugador puede intentarlo de nuevo */
		more = true;
	} else {
		msg("¡Has activado %s!", trap->kind->name);
		hit_trap(grid, -1);
	}

	/* Resultado */
	return (more);
}


/**
 * Desarma una trampa, o un cofre
 *
 * Las trampas deben ser visibles, los cofres deben saberse que están atrapados
 */
void do_cmd_disarm(struct command *cmd)
{
	struct loc grid;
	int dir;
	int err;

	struct object *obj;
	bool more = false;
	struct monster *mon;

	/* Obtener argumentos */
	err = cmd_get_arg_direction(cmd, "direction", &dir);
	if (err || dir == DIR_UNKNOWN) {
		struct loc grid1;
		int n_traps, n_chests, n_unldoor;

		n_traps = count_feats(&grid1, square_isdisarmabletrap, false);
		n_chests = count_chests(&grid1, CHEST_TRAPPED);
		n_unldoor = count_feats(&grid1, square_isunlockeddoor, false);

		if (n_traps + n_chests + n_unldoor == 1) {
			dir = motion_dir(player->grid, grid1);
			cmd_set_arg_direction(cmd, "direction", dir);
		} else if (cmd_get_direction(cmd, "direction", &dir, n_chests > 0)) {
			/* Si hay cofres que desarmar, se permite el 5 como dirección */
			return;
		}
	}

	/* Obtener ubicación */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Buscar cofres */
	obj = chest_check(player, grid, CHEST_TRAPPED);

	/* Verificar legalidad */
	if (!obj && !do_cmd_disarm_test(player, grid)) {
		/* Cancelar repetición */
		disturb(player);
		return;
	}

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);

		/* Buscar cofres */
		obj = chest_check(player, grid, CHEST_TRAPPED);
	}


	/* Monstruo */
	mon = square_monster(cave, grid);
	if (mon) {
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			monster_wake(mon, false, 100);
		} else {
			msg("¡Hay un monstruo en medio!");
			py_attack(player, grid);
		}
	} else if (obj)
		/* Cofre */
		more = do_cmd_disarm_chest(obj);
	else if (square_iscloseddoor(cave, grid) &&
			 !square_islockeddoor(cave, grid))
		/* Puerta para cerrar con llave */
		more = do_cmd_lock_door(grid);
	else
		/* Desarmar trampa */
		more = do_cmd_disarm_aux(grid);

	/* Cancelar repetición a menos que se indique lo contrario */
	if (!more) disturb(player);
}

/**
 * Manipular una casilla adyacente de alguna manera
 *
 * Atacar monstruos, excavar paredes, desarmar trampas, abrir puertas.
 *
 * Este comando siempre debe gastar energía, para prevenir detección gratuita
 * de monstruos invisibles.
 *
 * La "semántica" de este comando debe ser elegida antes de que el jugador
 * esté confundido, y debe ser verificada contra la nueva casilla.
 */
static void do_cmd_alter_aux(int dir)
{
	struct loc grid;
	bool more = false;
	struct object *o_chest_closed;
	struct object *o_chest_trapped;

	/* Obtener ubicación */
	grid = loc_sum(player->grid, ddgrid[dir]);

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Buscar cofre cerrado */
	o_chest_closed = chest_check(player, grid, CHEST_OPENABLE);
	/* Buscar cofre atrapado */
	o_chest_trapped = chest_check(player, grid, CHEST_TRAPPED);

	/* La acción depende de lo que haya */
	if (square(cave, grid)->mon > 0) {
		/* Atacar monstruo */
		py_attack(player, grid);
	} else if (square_isdiggable(cave, grid)) {
		/* Excavar paredes y escombros */
		more = do_cmd_tunnel_aux(grid);
	} else if (square_iscloseddoor(cave, grid)) {
		/* Abrir puertas cerradas */
		more = do_cmd_open_aux(grid);
	} else if (square_isdisarmabletrap(cave, grid)) {
		/* Desarmar trampas */
		more = do_cmd_disarm_aux(grid);
	} else if (o_chest_trapped) {
        	/* Cofre atrapado */
        	more = do_cmd_disarm_chest(o_chest_trapped);
    	} else if (o_chest_closed) {
        	/* Abrir cofre */
        	more = do_cmd_open_chest(grid, o_chest_closed);
	} else if (square_isopendoor(cave, grid)) {
		/* Cerrar puerta */
        	more = do_cmd_close_aux(grid);
	} else {
		/* Ups */
		msg("Das una vuelta sobre ti mismo.");
	}

	/* Cancelar repetición a menos que podamos continuar */
	if (!more) disturb(player);
}

void do_cmd_alter(struct command *cmd)
{
	int dir;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	do_cmd_alter_aux(dir);
}

static void do_cmd_steal_aux(int dir)
{
	/* Obtener ubicación */
	struct loc grid = loc_sum(player->grid, ddgrid[dir]);

	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Aplicar confusión */
	if (player_confuse_dir(player, &dir, false)) {
		/* Obtener ubicación */
		grid = loc_sum(player->grid, ddgrid[dir]);
	}

	/* Atacar o robar de monstruos */
	if ((square(cave, grid)->mon > 0) && player_has(player, PF_STEAL)) {
		steal_monster_item(square_monster(cave, grid), -1);
	} else {
		/* Ups */
		msg("Das una vuelta sobre ti mismo.");
	}
}

void do_cmd_steal(struct command *cmd)
{
	int dir;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	do_cmd_steal_aux(dir);
}

/**
 * Mover al jugador en la dirección dada.
 *
 * Esta rutina solo debe ser llamada cuando se ha gastado energía.
 *
 * Nótese que esta rutina maneja monstruos en la casilla de destino,
 * y también maneja intentos de moverse a paredes/puertas/escombros/etc.
 */
void move_player(int dir, bool disarm)
{
	struct loc grid = loc_sum(player->grid, ddgrid[dir]);

	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);
	bool trapsafe = player_is_trapsafe(player);
	bool trap = square_isdisarmabletrap(cave, grid);
	bool door = square_iscloseddoor(cave, grid);

	/* Pueden pasar muchas cosas al moverse */
	if (m_idx > 0) {
		/* Atacar monstruos */
		if (monster_is_camouflaged(mon)) {
			become_aware(cave, mon);

			/* El monstruo camuflado se despierta y se da cuenta */
			monster_wake(mon, false, 100);
		} else {
			py_attack(player, grid);
		}
	} else if (((trap && disarm) || door) && square_isknown(cave, grid)) {
		/* Auto-repetir si aún no se está repitiendo */
		if (cmd_get_nrepeats() == 0)
			cmd_set_repeat(99);
		do_cmd_alter_aux(dir);
	} else if (trap && player->upkeep->running && !trapsafe) {
		/* Dejar de correr antes de trampas conocidas */
		disturb(player);
		/* No se ha movido, así que no se gasta energía. */
		player->upkeep->energy_use = 0;
	} else if (!square_ispassable(cave, grid)) {
		disturb(player);

		/* Notificar obstáculos desconocidos, mencionar obstáculos conocidos */
		if (!square_isknown(cave, grid)) {
			if (square_isrubble(cave, grid)) {
				msgt(MSG_HITWALL,
					 "Sientes un montón de escombros bloqueando tu camino.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			} else if (square_iscloseddoor(cave, grid)) {
				msgt(MSG_HITWALL, "Sientes una puerta bloqueando tu camino.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			} else {
				msgt(MSG_HITWALL, "Sientes una pared bloqueando tu camino.");
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			}
		} else {
			if (square_isrubble(cave, grid)) {
				msgt(MSG_HITWALL,
					 "Hay un montón de escombros bloqueando tu camino.");
				if (!square_isrubble(player->cave, grid)) {
					square_memorize(cave, grid);
					square_light_spot(cave, grid);
				}
			} else if (square_iscloseddoor(cave, grid)) {
				msgt(MSG_HITWALL, "Hay una puerta bloqueando tu camino.");
				if (!square_iscloseddoor(player->cave, grid)) {
					square_memorize(cave, grid);
					square_light_spot(cave, grid);
				}
			} else {
				msgt(MSG_HITWALL, "Hay una pared bloqueando tu camino.");
				if (square_ispassable(player->cave, grid)
						|| square_isrubble(player->cave, grid)
						|| square_iscloseddoor(player->cave, grid)) {
					square_forget(cave, grid);
					square_light_spot(cave, grid);
				}
			}
		}
		/*
		 * No hay movimiento pero no se reembolsa la energía: principalmente para que
		 * los movimientos confundidos mientras se está ciego o sin luz gasten energía.
		 */
	} else {
		/* Ver si el estado de detección de trampas va a cambiar */
		bool old_dtrap = square_isdtrap(cave, player->grid);
		bool new_dtrap = square_isdtrap(cave, grid);
		bool step = true;

		/* Notar el cambio en el estado de detección */
		if (old_dtrap != new_dtrap)
			player->upkeep->redraw |= (PR_DTRAP);

		/* Molestar al jugador si está a punto de dejar el área */
		if (player->upkeep->running
				&& !player->upkeep->running_firststep
				&& old_dtrap && !new_dtrap) {
			disturb(player);
			/* No se ha movido, así que no se gasta energía. */
			player->upkeep->energy_use = 0;
			return;
		}

		/*
		 * Si no está confundido, permitir verificar antes de moverse a terreno
		 * dañino.
		 */
		if (square_isdamaging(cave, grid)
				&& !player->timed[TMD_CONFUSED]) {
			struct feature *feat = square_feat(cave, grid);
			int dam_taken = player_check_terrain_damage(player,
				grid, false);

			/*
			 * Verificar si está corriendo, o si va a costar más de un
			 * tercio de los ph.
			 */
			if (player->upkeep->running && dam_taken) {
				if (!get_check(feat->run_msg)) {
					player->upkeep->running = 0;
					step = false;
				}
			} else {
				if (dam_taken > player->chp / 3) {
					step = get_check(feat->walk_msg);
				}
			}
		}

		if (step) {
			/* Mover jugador */
			monster_swap(player->grid, grid);
			player_handle_post_move(player, true, false);
			cmdq_push(CMD_AUTOPICKUP);
			/*
			 * La recogida automática es un efecto secundario del movimiento:
			 * cualquier comando que desencadenara el movimiento será el
			 * objetivo de CMD_REPEAT en lugar de repetir la recogida automática,
			 * y la recogida automática no activará la sed de sangre.
			 */
			cmdq_peek()->background_command = 2;
		} else {
			/* No se ha movido, así que no se gasta energía. */
			player->upkeep->energy_use = 0;
		}
	}

	player->upkeep->running_firststep = false;
}

/**
 * Determina si una casilla dada puede ser "caminada"
 */
static bool do_cmd_walk_test(struct player *p, struct loc grid)
{
	int m_idx = square(cave, grid)->mon;
	struct monster *mon = cave_monster(cave, m_idx);

	/* Permitir atacar monstruos obvios si no se tiene miedo */
	if (m_idx > 0 && monster_is_obvious(mon)) {
		/* Manejar el miedo del jugador */
		if (player_of_has(p, OF_AFRAID)) {
			/* Extraer nombre del monstruo (o "eso") */
			char m_name[80];
			monster_desc(m_name, sizeof(m_name), mon, MDESC_DEFAULT);

			/* Mensaje */
			msgt(MSG_AFRAID, "¡Tienes demasiado miedo para atacar a %s!", m_name);
			equip_learn_flag(p, OF_AFRAID);

			/* No */
			return (false);
		}

		return (true);
	}

	/* Si no conocemos la casilla, permitir intentos de caminar hacia ella */
	if (!square_isknown(cave, grid))
		return true;

	/*
	 * Requiere espacio abierto; si el mensaje indica lo que hay y eso
	 * no coincide con la memoria del jugador, entonces actualizar la
	 * memoria del jugador
	 */
	if (!square_ispassable(cave, grid)) {
		if (square_isrubble(cave, grid)) {
			/* Escombros */
			msgt(MSG_HITWALL, "¡Hay un montón de escombros en el camino!");
			if (!square_isrubble(p->cave, grid)) {
				square_memorize(cave, grid);
				square_light_spot(cave, grid);
			}
		} else if (square_iscloseddoor(cave, grid)) {
			/* Puerta */
			return true;
		} else {
			/* Pared */
			msgt(MSG_HITWALL, "¡Hay una pared en el camino!");
			if (square_ispassable(p->cave, grid)
					|| square_isrubble(p->cave, grid)
					|| square_iscloseddoor(p->cave, grid)) {
				square_forget(cave, grid);
				square_light_spot(cave, grid);
			}
		}

		/* Cancelar repetición */
		disturb(p);

		/* No */
		return (false);
	}

	/* Ok */
	return (true);
}


/**
 * Caminar en la dirección dada.
 */
void do_cmd_walk(struct command *cmd)
{
	struct loc grid;
	int dir;
	bool trapsafe = player_is_trapsafe(player) ? true : false;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		struct trap_kind *web = lookup_trap("web");

		msg("Limpias la telaraña.");
		assert(web);
		square_remove_all_traps_of_type(cave, player->grid, web->tidx);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}

	/* Aplicar confusión si es necesario */
	/* Los movimientos confundidos usan energía pase lo que pase */
	if (player_confuse_dir(player, &dir, false))
		player->upkeep->energy_use = z_info->move_energy;
	
	/* Verificar si se puede caminar */
	grid = loc_sum(player->grid, ddgrid[dir]);
	if (!do_cmd_walk_test(player, grid))
		return;

	player->upkeep->energy_use = energy_per_move(player);

	/* Intentar desarmar a menos que sea una trampa y estemos a salvo de trampas */
	move_player(dir, !(square_isdisarmabletrap(cave, grid) && trapsafe));
}


/**
 * Caminar hacia una trampa.
 */
void do_cmd_jump(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		struct trap_kind *web = lookup_trap("web");

		msg("Limpias la telaraña.");
		assert(web);
		square_remove_all_traps_of_type(cave, player->grid, web->tidx);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}

	/* Aplicar confusión si es necesario */
	if (player_confuse_dir(player, &dir, false))
		player->upkeep->energy_use = z_info->move_energy;

	/* Verificar si se puede caminar */
	grid = loc_sum(player->grid, ddgrid[dir]);
	if (!do_cmd_walk_test(player, grid))
		return;

	player->upkeep->energy_use = energy_per_move(player);

	move_player(dir, false);
}

/**
 * Empezar a correr.
 *
 * Nótese que no se permite correr mientras se está confundido.
 */
void do_cmd_run(struct command *cmd)
{
	struct loc grid;
	int dir;

	/* Obtener argumentos */
	if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
		return;

	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		struct trap_kind *web = lookup_trap("web");

		msg("Limpias la telaraña.");
		assert(web);
		square_remove_all_traps_of_type(cave, player->grid, web->tidx);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}

	if (player_confuse_dir(player, &dir, true))
		return;

	/* Obtener ubicación */
	if (dir) {
		grid = loc_sum(player->grid, ddgrid[dir]);
		if (!do_cmd_walk_test(player, grid))
			return;
			
		/* Hack: convertir el contador de repeticiones en contador de carrera */
		if (cmd->nrepeats > 0) {
			player->upkeep->running = cmd->nrepeats;
			cmd->nrepeats = 0;
		}
		else {
			player->upkeep->running = 0;
		}
	}

	/* Empezar a correr */
	run_step(dir);
}

/**
 * Navegar automáticamente a la ubicación de escaleras abajo más cercana.
 *
 * Nótese que no se permite navegar mientras se está confundido.
 */
void do_cmd_navigate_down(struct command *cmd)
{
	/* cancelar si está confundido */
	if (player->timed[TMD_CONFUSED]) {
		msg("No puedes explorar mientras estás confundido.");
		return;
	}


	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		msg("Limpias la telaraña.");
		square_destroy_trap(cave, player->grid);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}


	/* Filtrar monstruos visibles */
	if (player_has_monster_in_view(player)) {
		msg("Algo está aquí.");
		return;
	}

	assert(!player->upkeep->steps);
	player->upkeep->step_count = path_nearest_known(player, player->grid,
		square_isdownstairs, &player->upkeep->path_dest,
		&player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->running_firststep = true;
		player->upkeep->running = player->upkeep->step_count;
		/* Calcular radio de la antorcha */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	msg("No hay camino conocido a escaleras abajo.");
}

/**
 * Navegar automáticamente a la ubicación de escaleras arriba más cercana.
 *
 * Nótese que no se permite navegar mientras se está confundido.
 */
void do_cmd_navigate_up(struct command *cmd)
{
	/* cancelar si está confundido */
	if (player->timed[TMD_CONFUSED]) {
		msg("No puedes explorar mientras estás confundido.");
		return;
	}


	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		msg("Limpias la telaraña.");
		square_destroy_trap(cave, player->grid);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}


	/* Filtrar monstruos visibles */
	if (player_has_monster_in_view(player)) {
		msg("Algo está aquí.");
		return;
	}

	assert(!player->upkeep->steps);
	player->upkeep->step_count = path_nearest_known(player, player->grid,
		square_isupstairs, &player->upkeep->path_dest,
		&player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->running_firststep = true;
		player->upkeep->running = player->upkeep->step_count;
		/* Calcular radio de la antorcha */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	msg("No hay camino conocido a escaleras arriba.");
}

/**
 * Empezar a explorar.
 *
 * Nótese que no se permite explorar mientras se está confundido.
 */
void do_cmd_explore(struct command *cmd)
{
	/* No hacer nada si los comandos de autoexploración están deshabilitados. */
	if (!OPT(player, autoexplore_commands)) {
		return;
	}

	/* cancelar si está confundido */
	if (player->timed[TMD_CONFUSED]) {
		msg("No puedes explorar mientras estás confundido.");
		return;
	}


	/* Si estamos en una telaraña, lidiar con eso */
	if (square_iswebbed(cave, player->grid)) {
		/* Limpiar la telaraña, terminar turno */
		msg("Limpias la telaraña.");
		square_destroy_trap(cave, player->grid);
		player->upkeep->energy_use = z_info->move_energy;
		return;
	}


	/* Filtrar monstruos visibles */
	if (player_has_monster_in_view(player)) {
		msg("Algo está aquí.");
		return;
	}

	assert(!player->upkeep->steps);
	player->upkeep->step_count = path_nearest_unknown(player, player->grid,
		&player->upkeep->path_dest, &player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->running_firststep = true;
		player->upkeep->running = player->upkeep->step_count;
		/* Calcular radio de la antorcha */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
		return;
	}

	msg("No hay camino aparente para explorar.");
}


/**
 * Empezar a correr con el buscador de caminos.
 *
 * Nótese que no se permite correr mientras se está confundido.
 */
void do_cmd_pathfind(struct command *cmd)
{
	struct loc grid;

	/* XXX-AS Añadir mejor comprobación de argumentos */
	cmd_get_arg_point(cmd, "point", &grid);

	if (player->timed[TMD_CONFUSED])
		return;

	assert(!player->upkeep->steps);
	player->upkeep->step_count =
		find_path(player, player->grid, grid, &player->upkeep->steps);
	if (player->upkeep->step_count > 0) {
		player->upkeep->path_dest = grid;
		player->upkeep->running_firststep = true;
		player->upkeep->running = player->upkeep->step_count;
		/* Calcular radio de la antorcha */
		player->upkeep->update |= (PU_TORCH);
		run_step(0);
	}
}



/**
 * Quedarse quieto. Buscar. Entrar a tiendas.
 * Recoger tesoro si "pickup" es true.
 */
void do_cmd_hold(struct command *cmd)
{
	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;

	/* Buscar (probablemente no necesario - NRM)*/
	search(player);

	/* Recoger cosas, sin usar energía extra */
	do_autopickup(player);

	/* Entrar a una tienda si estamos en una, si no mirar al suelo */
	if (square_isshop(cave, player->grid)) {
		if (player_is_shapechanged(player)) {
			if (square(cave, player->grid)->feat != FEAT_HOME) {
				msg("¡Se oye un grito y la puerta se cierra de golpe!");
			}
			return;
		}
		disturb(player);
		event_signal(EVENT_ENTER_STORE);
		event_remove_handler_type(EVENT_ENTER_STORE);
		event_signal(EVENT_USE_STORE);
		event_remove_handler_type(EVENT_USE_STORE);
		event_signal(EVENT_LEAVE_STORE);
		event_remove_handler_type(EVENT_LEAVE_STORE);

		/* Se gastará un turno al salir de la tienda */
		player->upkeep->energy_use = 0;
	} else {
		event_signal(EVENT_SEEFLOOR);
		square_know_pile(cave, player->grid, NULL);
	}
}


/**
 * Descansar (restaura puntos de golpe y maná y tal)
 */
void do_cmd_rest(struct command *cmd)
{
	int n;

	/* XXX-AS necesito insertar UI aquí */
	if (cmd_get_arg_choice(cmd, "choice", &n) != CMD_OK)
		return;

	/* 
	 * Un poco de verificación de cordura en la entrada - solo los valores
	 * negativos especificados son válidos. 
	 */
	if (n < 0 && !player_resting_is_special(n))
		return;

	/* Hacer algo de mantenimiento en el primer turno de descanso */
	if (!player_is_resting(player)) {
		player->upkeep->update |= (PU_BONUS);

		/* Si se ingresó un número de turnos, recordarlo */
		if (n > 1)
			player_set_resting_repeat_count(player, n);
		else if (n == 1)
			/* Si estamos repitiendo el comando, usar el mismo contador */
			n = player_get_resting_repeat_count(player);
	}

	/* Establecer el contador, y parar si se indica */
	player_resting_set_count(player, n);
	if (!player_is_resting(player))
		return;

	/* Gastar un turno */
	player_resting_step_turn(player);

	/* Redibujar el estado si se solicita */
	handle_stuff(player);

	/* Prepararse para continuar, o cancelar y limpiar */
	if (player_resting_count(player) > 0) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n - 1);
	} else if (player_resting_is_special(n)) {
		cmdq_push(CMD_REST);
		cmd_set_arg_choice(cmdq_peek(), "choice", n);
		player_set_resting_repeat_count(player, 0);
	} else {
		player_resting_cancel(player, false);
	}

}


/**
 * Pasar un turno sin hacer nada
 */
void do_cmd_sleep(struct command *cmd)
{
	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;
}


/**
 * Matriz de cadenas de sensaciones para sensaciones de objetos.
 * Mantener las cadenas en 36 caracteres o menos para mantener la
 * sensación combinada en una sola línea.
 */
static const char *obj_feeling_text[] =
{
	"Parece un nivel cualquiera.",
	"¡sientes un objeto de poder maravilloso!",
	"hay tesoros soberbios aquí.",
	"hay tesoros excelentes aquí.",
	"hay tesoros muy buenos aquí.",
	"hay tesoros buenos aquí.",
	"puede haber algo que valga la pena aquí.",
	"puede que no haya mucho interesante aquí.",
	"no hay muchos tesoros aquí.",
	"solo hay restos de basura aquí.",
	"no hay más que telarañas aquí."
};

/**
 * Matriz de cadenas de sensaciones para sensaciones de monstruos.
 * Mantener las cadenas en 36 caracteres o menos para mantener la
 * sensación combinada en una sola línea.
 */
static const char *mon_feeling_text[] =
{
	/* la primera cadena es solo un marcador de posición para
	 * mantener la simetría con obj_feeling.
	 */
	"Aún no estás seguro sobre este lugar",
	"Augurios de muerte acechan este lugar",
	"Este lugar parece asesino",
	"Este lugar parece terriblemente peligroso",
	"Te sientes ansioso sobre este lugar",
	"Te sientes nervioso sobre este lugar",
	"Este lugar no parece demasiado arriesgado",
	"Este lugar parece razonablemente seguro",
	"Este parece un lugar manso y resguardado",
	"Este parece un lugar tranquilo y pacífico"
};

/**
 * Mostrar la sensación. Los jugadores siempre reciben una sensación de monstruos.
 * Las sensaciones de objetos se retrasan hasta que el jugador haya explorado algo
 * del nivel.
 */
void display_feeling(bool obj_only)
{
	uint16_t obj_feeling = cave->feeling / 10;
	uint16_t mon_feeling = cave->feeling - (10 * obj_feeling);
	const char *join;

	/* No mostrar sensaciones para personajes de corazón frío */
	if (!OPT(player, birth_feelings)) return;

	/* Sin sensación útil en la ciudad */
	if (!player->depth) {
		msg("Parece una ciudad típica.");
		return;
	}

	/* Mostrar solo la sensación de objetos cuando se descubre por primera vez. */
	if (obj_only) {
		disturb(player);
		msg("Sientes que %s", obj_feeling_text[obj_feeling]);
		return;
	}

	/* Los jugadores obtienen automáticamente una sensación de monstruos. */
	if (cave->feeling_squares < z_info->feeling_need) {
		msg("%s.", mon_feeling_text[mon_feeling]);
		return;
	}

	/* Verificar las sensaciones */
	if (obj_feeling >= N_ELEMENTS(obj_feeling_text))
		obj_feeling = N_ELEMENTS(obj_feeling_text) - 1;

	if (mon_feeling >= N_ELEMENTS(mon_feeling_text))
		mon_feeling = N_ELEMENTS(mon_feeling_text) - 1;

	/* Decidir la conjunción */
	if ((mon_feeling <= 5 && obj_feeling > 6) ||
			(mon_feeling > 5 && obj_feeling <= 6))
		join = ", sin embargo";
	else
		join = ", y";

	/* Mostrar la sensación */
	msg("%s%s %s", mon_feeling_text[mon_feeling], join,
		obj_feeling_text[obj_feeling]);
}


void do_cmd_feeling(void)
{
	display_feeling(false);
}

/**
 * Hacer que un monstruo realice una acción.
 *
 * Actualmente las acciones posibles son lanzar un hechizo aleatorio, soltar un objeto aleatorio,
 * quedarse quieto, o moverse (atacando a cualquier monstruo que intervenga).
 */
void do_cmd_mon_command(struct command *cmd)
{
	struct monster *mon = get_commanded_monster();
	struct monster_lore *lore = NULL;
	char m_name[80];

	assert(mon);
	lore = get_lore(mon->race);

	/* Obtener el nombre del monstruo */
	monster_desc(m_name, sizeof(m_name), mon,
		MDESC_CAPITAL | MDESC_IND_HID | MDESC_COMMA);

	switch (cmd->code) {
		case CMD_READ_SCROLL: {
			/* En realidad 'l'iberar monstruo */
			mon_clear_timed(mon, MON_TMD_COMMAND, MON_TMD_FLG_NOTIFY);
			player_clear_timed(player, TMD_COMMAND, true, false);
			break;
		}
		case CMD_CAST: {
			int dir = DIR_UNKNOWN;
			struct monster *t_mon = NULL;
			bitflag f[RSF_SIZE];
			bool seen = player->timed[TMD_BLIND] ? false : true;
			int spell_index;

			/* Elegir un monstruo objetivo */
			target_set_monster(NULL);
			get_aim_dir(&dir);
			t_mon = target_get_monster();
			if (!t_mon) {
				msg("¡No se ha seleccionado ningún monstruo objetivo!");
				return;
			}
			mon->target.midx = t_mon->midx;

			/* Elegir un hechizo aleatorio y lanzarlo */
			rsf_copy(f, mon->race->spell_flags);
			spell_index = choose_attack_spell(f, true, true);
			if (!spell_index) {
				msg("¡Este monstruo no tiene hechizos!");
				return;
			}
			do_mon_spell(spell_index, mon, seen);

			/* Recordar lo que hizo el monstruo */
			if (seen) {
				rsf_on(lore->spell_flags, spell_index);
				if (mon_spell_is_innate(spell_index)) {
					/* Hechizo innato */
					if (lore->cast_innate < UCHAR_MAX)
						lore->cast_innate++;
				} else {
					/* Hechizo de proyectil o de área, o especial */
					if (lore->cast_spell < UCHAR_MAX)
						lore->cast_spell++;
				}
			}
			if (player->is_dead && (lore->deaths < SHRT_MAX)) {
				lore->deaths++;
			}
			lore_update(mon->race, lore);

			break;
		}
		case CMD_DROP: {
			char o_name[80];
			struct object *obj = get_random_monster_object(mon);
			if (!obj) break;
			obj->held_m_idx = 0;
			pile_excise(&mon->held_obj, obj);
			drop_near(cave, &obj, 0, mon->grid, true, false);
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, player);
			if (!ignore_item_ok(player, obj)) {
				msg("%s suelta %s.", m_name, o_name);
			}

			break;
		}
		case CMD_HOLD: {
			/* No hacer nada */
			break;
		}
		case CMD_WALK: {
			int dir;
			struct loc grid;
			bool can_move = false;
			bool has_hit = false;
			struct monster *t_mon = NULL;

			/* Obtener argumentos */
			if (cmd_get_direction(cmd, "direction", &dir, false) != CMD_OK)
				return;
			grid = loc_sum(mon->grid, ddgrid[dir]);

			/* No permitir que monstruos inmóviles se muevan */
			if (rf_has(mon->race->flags, RF_NEVER_MOVE)) {
				msg("El monstruo no puede moverse.");
				return;
			}

			/* Hay monstruo - atacar */
			t_mon = square_monster(cave, grid);
			if (t_mon) {
				/* Atacar al monstruo */
				if (monster_attack_monster(mon, t_mon)) {
					has_hit = true;
				} else {
					can_move = false;
				}
			} else if (square_ispassable(cave, grid)) {
				/* ¿El suelo está despejado? */
				can_move = true;
			} else if (square_isperm(cave, grid)) {
				/* Pared permanente en el camino */
				can_move = false;
			} else {
				/* Hay algún tipo de característica en el camino, así que aprender sobre
				 * kill-wall y pass-wall ahora */
				if (monster_is_visible(mon)) {
					rf_on(lore->flags, RF_PASS_WALL);
					rf_on(lore->flags, RF_KILL_WALL);
					rf_on(lore->flags, RF_SMASH_WALL);
				}

				/* El monstruo puede ser capaz de lidiar con paredes y puertas */
				if (rf_has(mon->race->flags, RF_PASS_WALL)) {
					can_move = true;
				} else if (rf_has(mon->race->flags, RF_KILL_WALL)) {
					/* Eliminar la pared */
					square_destroy_wall(cave, grid);
					can_move = true;
				} else if (rf_has(mon->race->flags, RF_SMASH_WALL)) {
					/* Eliminar todo */
					square_smash_wall(cave, grid);
					can_move = true;
				} else if (square_iscloseddoor(cave, grid) ||
						   square_issecretdoor(cave, grid)) {
					bool can_open = rf_has(mon->race->flags, RF_OPEN_DOOR);
					bool can_bash = rf_has(mon->race->flags, RF_BASH_DOOR);

					/* Aprender sobre habilidades con puertas */
					if (monster_is_visible(mon)) {
						rf_on(lore->flags, RF_OPEN_DOOR);
						rf_on(lore->flags, RF_BASH_DOOR);
					}

					/* Si el monstruo puede lidiar con puertas, preferir derribar */
					if (can_bash || can_open) {
						/* El resultado depende del tipo de puerta */
						if (square_islockeddoor(cave, grid)) {
							/* Probar fuerza contra resistencia de la puerta */
							int k = square_door_power(cave, grid);
							if (randint0(mon->hp / 10) > k) {
								if (can_bash) {
									msg("%s se estrella contra la puerta.", m_name);
								} else {
									msg("%s manipula la cerradura.", m_name);
								}

								/* Reducir la resistencia de la puerta en uno */
								square_set_door_lock(cave, grid, k - 1);
							}
						} else {
							/* Puerta cerrada o secreta -- siempre abrir o derribar */
							if (can_bash) {
								square_smash_door(cave, grid);

								msg("¡Escuchas una puerta abrirse de golpe!");

								/* Caer en la puerta */
								can_move = true;
							} else {
								square_open_door(cave, grid);
								can_move = true;
							}
						}
					}
				}
			}

			if (has_hit) {
				break;
			} else if (can_move) {
				monster_swap(mon->grid, grid);
				player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
			} else {
				msg("El camino está bloqueado.");
			}
			break;
		}
		default: {
			msg("Comandos válidos: mover, quedarse quieto, 's'oltar, 'm'agia, o 'l'iberar.");
			return;
		}
	}


	/* Gastar un turno */
	player->upkeep->energy_use = z_info->move_energy;
}