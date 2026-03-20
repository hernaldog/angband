/**
 * \archivo obj-chest.c
 * \brief Encapsulación de funciones relacionadas con cofres
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2012 Peter Denison
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
#include "mon-lore.h"
#include "obj-chest.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"

/**
 * Las trampas de cofres se especifican en el archivo chest_trap.txt.
 *
 * Los cofres se describen mediante su pval de 16 bits de la siguiente manera:
 * - pval de 0 es un cofre vacío
 * - pval de 1 es un cofre cerrado con llave sin trampas
 * - pval > 1 es un cofre con trampas, donde cada bit del pval aparte del
 *             más bajo y el más alto (potencialmente) representa una trampa diferente
 * - pval < 1 es un cofre desarmado/abierto; el proceso de desarme es simplemente
 *             negar el pval
 *
 * El pval del cofre también determina la dificultad de desarmar el cofre.
 * Actualmente la dificultad máxima es 60 (32 + 16 + 8 + 4); si se añaden más trampas
 * a chest_trap.txt, el cálculo de desarme necesitará ajustes.
 */

struct chest_trap *chest_traps;

/**
 * ------------------------------------------------------------------------
 * Funciones de análisis para chest_trap.txt y chest.txt
 * ------------------------------------------------------------------------ */
static enum parser_error parse_chest_trap_name(struct parser *p)
{
    const char *name = parser_getstr(p, "name");
    struct chest_trap *h = parser_priv(p);
    struct chest_trap *t = mem_zalloc(sizeof *t);

	/* Ordenar las trampas correctamente y establecer el pval */
	if (h) {
		h->next = t;
		t->pval = h->pval * 2;
	} else {
		chest_traps = t;
		t->pval = 1;
	}
    t->name = string_make(name);
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_code(struct parser *p)
{
    const char *code = parser_getstr(p, "code");
    struct chest_trap *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->code = string_make(code);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_level(struct parser *p)
{
    struct chest_trap *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->level = parser_getint(p, "level");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_effect(struct parser *p) {
    struct chest_trap *t = parser_priv(p);
	struct effect *effect;
	struct effect *new_effect = mem_zalloc(sizeof(*new_effect));

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Ir al siguiente efecto vacante y establecerlo al nuevo */
	if (t->effect) {
		effect = t->effect;
		while (effect->next)
			effect = effect->next;
		effect->next = new_effect;
	} else
		t->effect = new_effect;

	/* Rellenar los detalles */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_chest_trap_dice(struct parser *p) {
	struct chest_trap *t = parser_priv(p);
	dice_t *dice = NULL;
	struct effect *effect = t->effect;
	const char *string = NULL;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Si no hay efecto, asumir que es humano y no un error del analizador. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	dice = dice_new();

	if (dice == NULL)
		return PARSE_ERROR_INVALID_DICE;

	string = parser_getstr(p, "dice");

	if (dice_parse_string(dice, string)) {
		effect->dice = dice;
	}
	else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_expr(struct parser *p) {
	struct chest_trap *t = parser_priv(p);
	struct effect *effect = t->effect;
	expression_t *expression = NULL;
	expression_base_value_f function = NULL;
	const char *name;
	const char *base;
	const char *expr;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Si no hay efecto, asumir que es humano y no un error del analizador. */
	if (effect == NULL)
		return PARSE_ERROR_NONE;

	while (effect->next) effect = effect->next;

	/* Si no hay dados, asumir que es humano y no un error del analizador. */
	if (effect->dice == NULL)
		return PARSE_ERROR_NONE;

	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL)
		return PARSE_ERROR_INVALID_EXPRESSION;

	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0)
		return PARSE_ERROR_BAD_EXPRESSION_STRING;

	if (dice_bind_expression(effect->dice, name, expression) < 0)
		return PARSE_ERROR_UNBOUND_EXPRESSION;

	/* El objeto dice hace una copia profunda de la expresión, así que podemos liberarla */
	expression_free(expression);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_destroy(struct parser *p) {
    struct chest_trap *t = parser_priv(p);
	int val = 0;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    val = parser_getint(p, "val");
	if (val) {
		t->destroy = true;
	}
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_magic(struct parser *p) {
    struct chest_trap *t = parser_priv(p);
	int val = 0;

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    val = parser_getint(p, "val");
	if (val) {
		t->magic = true;
	}
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_msg(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg = string_append(t->msg, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_chest_trap_msg_death(struct parser *p) {
    struct chest_trap *t = parser_priv(p);

	if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_death = string_append(t->msg_death, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}

struct parser *init_parse_chest_trap(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_chest_trap_name);
    parser_reg(p, "code str code", parse_chest_trap_code);
    parser_reg(p, "level int level", parse_chest_trap_level);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_chest_trap_effect);
	parser_reg(p, "dice str dice", parse_chest_trap_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_chest_trap_expr);
	parser_reg(p, "destroy int val", parse_chest_trap_destroy);
	parser_reg(p, "magic int val", parse_chest_trap_magic);
	parser_reg(p, "msg str text", parse_chest_trap_msg);
	parser_reg(p, "msg-death str text", parse_chest_trap_msg_death);
    return p;
}

static errr run_parse_chest_trap(struct parser *p) {
    return parse_file_quit_not_found(p, "chest_trap");
}

static errr finish_parse_chest_trap(struct parser *p) {
	parser_destroy(p);
	return 0;
}

static void cleanup_chest_trap(void)
{
	struct chest_trap *trap = chest_traps;
	while (trap) {
		struct chest_trap *old = trap;
		string_free(trap->name);
		string_free(trap->code);
		string_free(trap->msg);
		string_free(trap->msg_death);
		free_effect(trap->effect);
		trap = trap->next;
		mem_free(old);
	}
}

struct file_parser chest_trap_parser = {
    "chest_trap",
    init_parse_chest_trap,
    run_parse_chest_trap,
    finish_parse_chest_trap,
    cleanup_chest_trap
};

/**
 * ------------------------------------------------------------------------
 * Información de trampas de cofres
 * ------------------------------------------------------------------------ */
/**
 * El nombre de una trampa de cofre
 */
const char *chest_trap_name(const struct object *obj)
{
	int16_t trap_value = obj->pval;

	/* Un valor distinto de cero significa que había o todavía hay trampas */
	if (trap_value < 0) {
		return (trap_value == -1) ? "abierto" : "desarmado";
	} else if (trap_value > 0) {
		struct chest_trap *trap = chest_traps, *found = NULL;
		while (trap) {
			if (trap_value & trap->pval) {
				if (found) {
					return "múltiples trampas";
				}
				found = trap;
			}
			trap = trap->next;
		}
		if (found) {
			return found->name;
		}
	}

	return "vacío";
}

/**
 * Determina si un cofre tiene trampas
 */
bool is_trapped_chest(const struct object *obj)
{
	if (!tval_is_chest(obj))
		return false;

	/* Los cofres desarmados o abiertos no tienen trampas */
	if (obj->pval <= 0)
		return false;

	/* Algunos cofres simplemente no tienen trampas */
	return (obj->pval == 1) ? false : true;
}


/**
 * Determina si un cofre está cerrado con llave o tiene trampas
 */
bool is_locked_chest(const struct object *obj)
{
	if (!tval_is_chest(obj))
		return false;

	/* Los cofres desarmados o abiertos no están cerrados con llave */
	return (obj->pval > 0);
}

/**
 * ------------------------------------------------------------------------
 * Acciones de trampas de cofres
 * ------------------------------------------------------------------------ */
/**
 * Elegir una única trampa de cofre para un nivel dado de objeto de cofre
 */
static int pick_one_chest_trap(int level)
{
	int count = 0, pick;
	struct chest_trap *trap;

	/* Contar las trampas posibles (empezando después de la trampa "cerrada con llave") */
	for (trap = chest_traps->next; trap; trap = trap->next) {
		if (trap->level <= level) count++;
	}

	/* Elegir una trampa, devolver el pval */
	pick = randint0(count);
	for (trap = chest_traps->next; trap; trap = trap->next) {
		if (!pick--) break;
	}
	return trap->pval;
}

/**
 * Elegir un conjunto de trampas para un cofre
 * Actualmente esto solo depende del nivel del objeto de cofre
 */
int pick_chest_traps(struct object *obj)
{
	int level = obj->kind->level;
	int trap = 0;

	/* Una posibilidad entre diez de que no haya trampa */
	if (one_in_(10)) {
		return 1;
	}

	/* Elegir una trampa, añadirla */
	trap |= pick_one_chest_trap(level);

	/* Probabilidad dependiente del nivel de una segunda trampa (puede superponerse a la primera) */
	if ((level > 5) && one_in_(1 + ((65 - level) / 10))) {
		trap |= pick_one_chest_trap(level);
	}

	/* Probabilidad de una tercera trampa para cofres profundos (puede superponerse a las existentes) */
	if ((level > 45) && one_in_(65 - level)) {
		trap |= pick_one_chest_trap(level);
		/* Pequeña probabilidad de una cuarta trampa (puede superponerse a las existentes) */
		if (one_in_(40)) {
			trap |= pick_one_chest_trap(level);
		}
	}

	return trap;
}

/**
 * Abrir un cofre
 */
void unlock_chest(struct object *obj)
{
	obj->pval = (0 - obj->pval);
}

/**
 * Determina si una casilla contiene un cofre que coincide con el tipo de consulta, y
 * devuelve un puntero al primer cofre de ese tipo
 */
struct object *chest_check(const struct player *p, struct loc grid,
		enum chest_query check_type)
{
	struct object *obj;

	/* Escanear todos los objetos en la casilla */
	for (obj = square_object(cave, grid); obj; obj = obj->next) {
		/* Ignorar si se solicita */
		if (ignore_item_ok(p, obj)) continue;

		/* Verificar cofres */
		switch (check_type) {
		case CHEST_ANY:
			if (tval_is_chest(obj))
				return obj;
			break;
		case CHEST_OPENABLE:
			if (tval_is_chest(obj) && (obj->pval != 0))
				return obj;
			break;
		case CHEST_TRAPPED:
			if (is_trapped_chest(obj) && obj->known && obj->known->pval)
				return obj;
			break;
		}
	}

	/* No hay cofre */
	return NULL;
}


/**
 * Devuelve el número de casillas que contienen cofres alrededor (o debajo) del personaje.
 * Si se solicita, contar solo los cofres con trampas.
 */
int count_chests(struct loc *grid, enum chest_query check_type)
{
	int d, count;

	/* Contar cuántas coincidencias */
	count = 0;

	/* Verificar alrededor (y debajo) del personaje */
	for (d = 0; d < 9; d++) {
		/* Extraer ubicación adyacente (legal) */
		struct loc grid1 = loc_sum(player->grid, ddgrid_ddd[d]);

		/* No hay cofre (visible) allí */
		if (!chest_check(player, grid1, check_type)) continue;

		/* Contarlo */
		++count;

		/* Recordar la ubicación del último cofre encontrado */
		*grid = grid1;
	}

	/* Todo listo */
	return count;
}


/**
 * Asignar objetos al abrir un cofre
 *
 * Dispensar tesoros del cofre dado, centrado en (x,y).
 *
 * Los cofres de madera contienen 1 objeto, los cofres de hierro contienen 2 objetos,
 * y los cofres de acero contienen 3 objetos. Los cofres pequeños ahora contienen objetos buenos,
 * los cofres grandes objetos excelentes, fuera de profundidad para el nivel en el que se genera
 * el cofre.
 *
 * El juicio sobre el tamaño y la construcción de los cofres se realiza actualmente a partir del nombre.
 */
static void chest_death(struct loc grid, struct object *chest)
{
	int number, level;
	bool large = strstr(chest->kind->name, "Large") ? true : false;;

	/* El pval cero significa cofre vacío */
	if (!chest->pval)
		return;

	/* Determinar cuánto soltar (ver arriba) */
	if (strstr(chest->kind->name, "wooden")) {
		number = 1;
	} else if (strstr(chest->kind->name, "iron")) {
		number = 2;
	} else if (strstr(chest->kind->name, "steel")) {
		number = 3;
	} else {
		number = randint1(3);
	}

	/* Soltar algunos objetos valiosos (no cofres) */
	level = chest->origin_depth + 5;
	while (number > 0) {
		struct object *treasure;
		treasure = make_object(cave, level, true, large, false, NULL, 0);
		if (!treasure)
			continue;
		if (tval_is_chest(treasure)) {
			object_delete(cave, player->cave, &treasure);
			continue;
		}

		treasure->origin = ORIGIN_CHEST;
		treasure->origin_depth = chest->origin_depth;
		drop_near(cave, &treasure, 0, grid, true, false);
		number--;
	}

	/* El cofre ahora está vacío */
	chest->pval = 0;
	chest->known->pval = 0;
}


/**
 * Los cofres también tienen trampas.
 */
static void chest_trap(struct object *obj)
{
	int traps = obj->pval;
	struct chest_trap *trap;
	bool ident = false;

	/* Ignorar cofres desarmados */
	if (traps <= 0) return;

	/* Aplicar efectos de las trampas */
	for (trap = chest_traps; trap; trap = trap->next) {
		if (trap->pval & traps) {
			if (trap->msg) {
				msg(trap->msg);
			}
			if (trap->effect) {
				effect_do(trap->effect, source_chest_trap(trap), obj, &ident,
						  false, 0, 0, 0, NULL);
			}
			if (trap->destroy) {
				obj->pval = 0;
				break;
			}
		}
	}
}


/**
 * Intentar abrir el cofre dado en la ubicación dada
 *
 * Asumir que no hay ningún monstruo bloqueando el destino
 *
 * Devuelve verdadero si los comandos repetidos pueden continuar
 */
bool do_cmd_open_chest(struct loc grid, struct object *obj)
{
	int i, j;

	bool flag = true;

	bool more = false;

	/* Intentar abrirlo */
	if (obj->pval > 0) {
		/* Asumir que está cerrado con llave, y por lo tanto no abierto */
		flag = false;

		/* Obtener el factor de "desarme" */
		i = player->state.skills[SKILL_DISARM_PHYS];

		/* Penalizar algunas condiciones */
		if (player->timed[TMD_BLIND] || no_light(player)) i = i / 10;
		if (player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) i = i / 10;

		/* Extraer la dificultad */
		j = i - obj->pval;

		/* Siempre tener una pequeña posibilidad de éxito */
		if (j < 2) j = 2;

		/* Éxito -- Puede que todavía tenga trampas */
		if (randint0(100) < j) {
			msgt(MSG_LOCKPICK, "Has abierto la cerradura.");
			player_exp_gain(player, 1);
			flag = true;
		} else {
			/* Podemos seguir repitiendo */
			more = true;
			event_signal(EVENT_INPUT_FLUSH);
			msgt(MSG_LOCKPICK_FAIL, "No pudiste abrir la cerradura.");
		}
	}

	/* Permitido abrir */
	if (flag) {
		/* Aplicar trampas del cofre, si las hay y el jugador no es inmune a trampas */
		if (!player_is_trapsafe(player)) {
			chest_trap(obj);
		} else if ((obj->pval > 0) && player_of_has(player, OF_TRAP_IMMUNE)) {
			/* Aprender inmunidad a trampas si hay trampas */
			equip_learn_flag(player, OF_TRAP_IMMUNE);
		}

		/* Dejar que el cofre suelte objetos */
		chest_death(grid, obj);

		/* Ignorar el cofre si la autoignoración lo requiere */
		player->upkeep->notice |= PN_IGNORE;
	}

	/* Los cofres vacíos siempre se ignoraban en ignore_item_okay, así que
	 * también podríamos ignorarlos aquí
	 */
	if (obj->pval == 0)
		obj->known->notice |= OBJ_NOTICE_IGNORE;

	/* Redibujar el cofre, para estar seguros (puede haber sido ignorado) */
	square_light_spot(cave, grid);

	/* Resultado */
	return (more);
}


/**
 * Intentar desarmar el cofre en la ubicación dada
 * Asumir que no hay ningún monstruo bloqueando el destino
 *
 * El cálculo de dificultad asume que hay 6 tipos de trampas de cofre;
 * si se añaden más, será necesario ajustarlo.
 *
 * Devuelve verdadero si los comandos repetidos pueden continuar
 */
bool do_cmd_disarm_chest(struct object *obj)
{
	int skill = player->state.skills[SKILL_DISARM_PHYS], diff;
	struct chest_trap *traps;
	bool physical = false;
	bool magic = false;
	bool more = false;

	/* Verificar si las trampas son mágicas, físicas o ambas */
	for (traps = chest_traps; traps; traps = traps->next) {
		if (!(traps->pval & obj->pval)) continue;
		if (traps->magic) {
			magic = true;
		} else {
			physical = true;
		}
	}

	/* El desarme físico es el predeterminado, si hay trampas mágicas ajustamos */
	if (magic) {
		if (physical) {
			skill = (player->state.skills[SKILL_DISARM_MAGIC] +
					 player->state.skills[SKILL_DISARM_PHYS]) / 2;
		} else {
			skill = player->state.skills[SKILL_DISARM_MAGIC];
		}
	}

	/* Penalizar algunas condiciones */
	if (player->timed[TMD_BLIND] || no_light(player)) {
		skill /= 10;
	}
	if (player->timed[TMD_CONFUSED] || player->timed[TMD_IMAGE]) {
		skill /= 10;
	}

	/* Extraer la dificultad */
	diff = skill - obj->pval;

	/* Siempre tener una pequeña posibilidad de éxito */
	if (diff < 2) diff = 2;

	/* Primero debe encontrar la trampa. */
	if (!obj->known->pval || ignore_item_ok(player, obj)) {
		msg("No veo ninguna trampa.");
	} else if (!is_trapped_chest(obj)) {
		/* Ya desarmado/abierto o sin trampas */
		msg("El cofre no tiene trampas.");
	} else if (randint0(100) < diff) {
		/* Éxito (obtener mucha experiencia) */
		msgt(MSG_DISARM, "Has desarmado el cofre.");
		player_exp_gain(player, obj->pval);
		obj->pval = (0 - obj->pval);
	} else if (randint0(100) < diff) {
		/* Fracaso -- Seguir intentando */
		more = true;
		event_signal(EVENT_INPUT_FLUSH);
		msg("No pudiste desarmar el cofre.");
	} else {
		/* Fracaso -- Activar la trampa */
		if (!player_is_trapsafe(player)) {
			msg("¡Activaste una trampa!");
			chest_trap(obj);
		} else if (player_of_has(player, OF_TRAP_IMMUNE)) {
			/* Aprender inmunidad a trampas. */
			equip_learn_flag(player, OF_TRAP_IMMUNE);
		}
	}

	/* Resultado */
	return (more);
}