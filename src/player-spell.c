/**
 * \file player-spell.c
 * \brief Lanzamiento de hechizos y plegarias
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
#include "effects.h"
#include "init.h"
#include "monster.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "project.h"
#include "target.h"

/**
 * Utilizado por get_spell_info() para pasar información mientras itera a través de los efectos.
 */
struct spell_info_iteration_state {
	const struct effect *pre;
	char pre_special[40];
	random_value pre_rv;
	random_value shared_rv;
	bool have_shared;
};

/**
 * Tabla de Estadísticas (INT/SAB) -- Tasa de fallo mínimo (porcentaje)
 */
static const int adj_mag_fail[STAT_RANGE] =
{
	99	/* 3 */,
	99	/* 4 */,
	99	/* 5 */,
	99	/* 6 */,
	99	/* 7 */,
	50	/* 8 */,
	30	/* 9 */,
	20	/* 10 */,
	15	/* 11 */,
	12	/* 12 */,
	11	/* 13 */,
	10	/* 14 */,
	9	/* 15 */,
	8	/* 16 */,
	7	/* 17 */,
	6	/* 18/00-18/09 */,
	6	/* 18/10-18/19 */,
	5	/* 18/20-18/29 */,
	5	/* 18/30-18/39 */,
	5	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	4	/* 18/60-18/69 */,
	4	/* 18/70-18/79 */,
	4	/* 18/80-18/89 */,
	3	/* 18/90-18/99 */,
	3	/* 18/100-18/109 */,
	2	/* 18/110-18/119 */,
	2	/* 18/120-18/129 */,
	2	/* 18/130-18/139 */,
	2	/* 18/140-18/149 */,
	1	/* 18/150-18/159 */,
	1	/* 18/160-18/169 */,
	1	/* 18/170-18/179 */,
	1	/* 18/180-18/189 */,
	1	/* 18/190-18/199 */,
	0	/* 18/200-18/209 */,
	0	/* 18/210-18/219 */,
	0	/* 18/220+ */
};

/**
 * Tabla de Estadísticas (INT/SAB) -- ajuste de la tasa de fallo
 */
static const int adj_mag_stat[STAT_RANGE] =
{
	-5	/* 3 */,
	-4	/* 4 */,
	-3	/* 5 */,
	-3	/* 6 */,
	-2	/* 7 */,
	-1	/* 8 */,
	 0	/* 9 */,
	 0	/* 10 */,
	 0	/* 11 */,
	 0	/* 12 */,
	 0	/* 13 */,
	 1	/* 14 */,
	 2	/* 15 */,
	 3	/* 16 */,
	 4	/* 17 */,
	 5	/* 18/00-18/09 */,
	 6	/* 18/10-18/19 */,
	 7	/* 18/20-18/29 */,
	 8	/* 18/30-18/39 */,
	 9	/* 18/40-18/49 */,
	10	/* 18/50-18/59 */,
	11	/* 18/60-18/69 */,
	12	/* 18/70-18/79 */,
	15	/* 18/80-18/89 */,
	18	/* 18/90-18/99 */,
	21	/* 18/100-18/109 */,
	24	/* 18/110-18/119 */,
	27	/* 18/120-18/129 */,
	30	/* 18/130-18/139 */,
	33	/* 18/140-18/149 */,
	36	/* 18/150-18/159 */,
	39	/* 18/160-18/169 */,
	42	/* 18/170-18/179 */,
	45	/* 18/180-18/189 */,
	48	/* 18/190-18/199 */,
	51	/* 18/200-18/209 */,
	54	/* 18/210-18/219 */,
	57	/* 18/220+ */
};

/**
 * Inicializar los hechizos del jugador
 */
void player_spells_init(struct player *p)
{
	int i, num_spells = p->class->magic.total_spells;

	/* Ninguno */
	if (!num_spells) return;

	/* Asignar */
	p->spell_flags = mem_zalloc(num_spells * sizeof(uint8_t));
	p->spell_order = mem_zalloc(num_spells * sizeof(uint8_t));

	/* Ninguno de los hechizos ha sido aprendido todavía */
	for (i = 0; i < num_spells; i++)
		p->spell_order[i] = 99;
}

/**
 * Liberar los hechizos del jugador
 */
void player_spells_free(struct player *p)
{
	mem_free(p->spell_flags);
	mem_free(p->spell_order);
}

/**
 * Hacer una lista de los reinos mágicos de los que la clase del jugador tiene libros
 */
struct magic_realm *class_magic_realms(const struct player_class *c, int *count)
{
	int i;
	struct magic_realm *r = mem_zalloc(sizeof(struct magic_realm));

	*count = 0;

	if (!c->magic.total_spells) {
		mem_free(r);
		return NULL;
	}

	for (i = 0; i < c->magic.num_books; i++) {
		struct magic_realm *r_test = r;
		struct class_book *book = &c->magic.books[i];
		bool found = false;

		/* Probar para el primer reino */
		if (r->name == NULL) {
			memcpy(r, book->realm, sizeof(struct magic_realm));
			r->next = NULL;
			(*count)++;
			continue;
		}

		/* Probar si ya está registrado */
		while (r_test) {
			if (streq(r_test->name, book->realm->name)) {
				found = true;
			}
			r_test = r_test->next;
		}
		if (found) continue;

		/* Añadirlo */
		r_test = mem_zalloc(sizeof(struct magic_realm));
		memcpy(r_test, book->realm, sizeof(struct magic_realm));
		r_test->next = r;
		r = r_test;
		(*count)++;
	}

	return r;
}


/**
 * Obtener la estructura de libro de hechizos de cualquier objeto que sea un libro
 */
const struct class_book *object_kind_to_book(const struct object_kind *kind)
{
	struct player_class *class = classes;
	while (class) {
		int i;

		for (i = 0; i < class->magic.num_books; i++)
		if ((kind->tval == class->magic.books[i].tval) &&
			(kind->sval == class->magic.books[i].sval)) {
			return &class->magic.books[i];
		}
		class = class->next;
	}

	return NULL;
}

/**
 * Obtener la estructura de libro de hechizos de un objeto que es un libro del que el jugador puede lanzar
 */
const struct class_book *player_object_to_book(const struct player *p,
		const struct object *obj)
{
	int i;

	for (i = 0; i < p->class->magic.num_books; i++)
		if ((obj->tval == p->class->magic.books[i].tval) &&
			(obj->sval == p->class->magic.books[i].sval))
			return &p->class->magic.books[i];

	return NULL;
}

const struct class_spell *spell_by_index(const struct player *p, int index)
{
	int book = 0, count = 0;
	const struct class_magic *magic = &p->class->magic;

	/* Verificar la validez del índice */
	if (index < 0 || index >= magic->total_spells)
		return NULL;

	/* Encontrar el libro, contar los hechizos en libros anteriores */
	while (count + magic->books[book].num_spells - 1 < index)
		count += magic->books[book++].num_spells;

	/* Encontrar el hechizo */
	return &magic->books[book].spells[index - count];
}

/**
 * Recopilar hechizos de un libro en el array spells[], asignando
 * memoria apropiada.
 */
int spell_collect_from_book(const struct player *p, const struct object *obj,
		int **spells)
{
	const struct class_book *book = player_object_to_book(p, obj);
	int i, n_spells = 0;

	if (!book) {
		return n_spells;
	}

	/* Contar los hechizos */
	for (i = 0; i < book->num_spells; i++)
		n_spells++;

	/* Asignar el array */
	*spells = mem_zalloc(n_spells * sizeof(*spells));

	/* Escribir los hechizos */
	for (i = 0; i < book->num_spells; i++)
		(*spells)[i] = book->spells[i].sidx;

	return n_spells;
}


/**
 * Devolver el número de hechizos lanzables en el libro de hechizos 'obj'.
 */
int spell_book_count_spells(const struct player *p, const struct object *obj,
		bool (*tester)(const struct player *p, int spell))
{
	const struct class_book *book = player_object_to_book(p, obj);
	int i, n_spells = 0;

	if (!book) {
		return n_spells;
	}

	for (i = 0; i < book->num_spells; i++)
		if (tester(p, book->spells[i].sidx))
			n_spells++;

	return n_spells;
}


/**
 * Verdadero si al menos un hechizo en spells[] es OK según spell_test.
 */
bool spell_okay_list(const struct player *p,
		bool (*spell_test)(const struct player *p, int spell),
		const int spells[], int n_spells)
{
	int i;
	bool okay = false;

	for (i = 0; i < n_spells; i++)
		if (spell_test(p, spells[i]))
			okay = true;

	return okay;
}

/**
 * Verdadero si el hechizo es lanzable.
 */
bool spell_okay_to_cast(const struct player *p, int spell)
{
	return (p->spell_flags[spell] & PY_SPELL_LEARNED);
}

/**
 * Verdadero si el hechizo puede ser estudiado.
 */
bool spell_okay_to_study(const struct player *p, int spell_index)
{
	const struct class_spell *spell = spell_by_index(p, spell_index);
	return spell && spell->slevel <= p->lev
		&& !(p->spell_flags[spell_index] & PY_SPELL_LEARNED);
}

/**
 * Verdadero si el hechizo puede ser examinado.
 */
bool spell_okay_to_browse(const struct player *p, int spell_index)
{
	const struct class_spell *spell = spell_by_index(p, spell_index);
	return spell && spell->slevel < 99;
}

/**
 * Ajuste de fallo de hechizo por el nivel de la estadística de lanzamiento
 */
static int fail_adjust(struct player *p, const struct class_spell *spell)
{
	int stat = spell->realm->stat;
	return adj_mag_stat[p->state.stat_ind[stat]];
}

/**
 * Fallo mínimo de hechizo por el nivel de la estadística de lanzamiento
 */
static int min_fail(struct player *p, const struct class_spell *spell)
{
	int stat = spell->realm->stat;
	return adj_mag_fail[p->state.stat_ind[stat]];
}

/**
 * Devuelve la probabilidad de fallo para un hechizo
 */
int16_t spell_chance(int spell_index)
{
	int chance = 100, minfail;

	const struct class_spell *spell;

	/* Paranoia -- debe ser alfabetizado */
	if (!player->class->magic.total_spells) return chance;

	/* Obtener el hechizo */
	spell = spell_by_index(player, spell_index);
	if (!spell) return chance;

	/* Extraer la tasa de fallo base del hechizo */
	chance = spell->sfail;

	/* Reducir la tasa de fallo mediante el ajuste de nivel "efectivo" */
	chance -= 3 * (player->lev - spell->slevel);

	/* Reducir la tasa de fallo mediante el ajuste del nivel de la estadística de lanzamiento */
	chance -= fail_adjust(player, spell);

	/* No hay suficiente maná para lanzar */
	if (spell->smana > player->csp)
		chance += 5 * (spell->smana - player->csp);

	/* Obtener la tasa de fallo mínima para el nivel de la estadística de lanzamiento */
	minfail = min_fail(player, spell);

	/* Los personajes sin fallo cero nunca mejoran del 5 por ciento */
	if (!player_has(player, PF_ZERO_FAIL) && minfail < 5) {
		minfail = 5;
	}

	/* Los nigromantes son castigados por estar en casillas iluminadas */
	if (player_has(player, PF_UNLIGHT) && square_islit(cave, player->grid)) {
		chance += 25;
	}

	/* El miedo dificulta los hechizos (antes del fallo mínimo) */
	/* Nótese que los hechizos que eliminan el miedo tienen una tasa de fallo mucho más baja que
	 * los hechizos circundantes, para asegurar que esto no cause un mega fallo */
	if (player_of_has(player, OF_AFRAID)) chance += 20;

	/* Tasa de fallo mínima y máxima */
	if (chance < minfail) chance = minfail;
	if (chance > 50) chance = 50;

	/* El aturdimiento dificulta los hechizos (después del fallo mínimo) */
	if (player->timed[TMD_STUN] > 50) {
		chance += 25;
	} else if (player->timed[TMD_STUN]) {
		chance += 15;
	}

	/* La amnesia dificulta mucho los hechizos */
	if (player->timed[TMD_AMNESIA]) {
		chance = 50 + chance / 2;
	}

	/* Siempre hay un 5 por ciento de probabilidad de funcionar */
	if (chance > 95) {
		chance = 95;
	}

	/* Devolver la probabilidad */
	return (chance);
}


/**
 * Aprender el hechizo especificado.
 */
void spell_learn(int spell_index)
{
	int i;
	const struct class_spell *spell = spell_by_index(player, spell_index);

	/* Aprender el hechizo */
	player->spell_flags[spell_index] |= PY_SPELL_LEARNED;

	/* Encontrar la siguiente entrada vacía en "spell_order[]" */
	for (i = 0; i < player->class->magic.total_spells; i++)
		if (player->spell_order[i] == 99) break;

	/* Añadir el hechizo a la lista conocida */
	player->spell_order[i] = spell_index;

	/* Mencionar el resultado */
	msgt(MSG_STUDY, "Has aprendido %s de %s.", spell->realm->spell_noun,
		 spell->name);

	/* Un hechizo menos disponible */
	player->upkeep->new_spells--;

	/* Mensaje si es necesario */
	if (player->upkeep->new_spells)
		msg("Puedes aprender %d %s más%s.", player->upkeep->new_spells,
			spell->realm->spell_noun, PLURAL(player->upkeep->new_spells));

	/* Redibujar Estado de Estudio */
	player->upkeep->redraw |= (PR_STUDY | PR_OBJECT);
}

static int beam_chance(void)
{
	int plev = player->lev;
	return (player_has(player, PF_BEAM) ? plev : (plev / 2));
}

/**
 * Lanzar el hechizo especificado
 */
bool spell_cast(int spell_index, int dir, struct command *cmd)
{
	int chance;
	bool ident = false;
	int beam  = beam_chance();

	/* Obtener el hechizo */
	const struct class_spell *spell = spell_by_index(player, spell_index);

	/* Probabilidad de fallo del hechizo */
	chance = spell_chance(spell_index);

	/* Fallar o tener éxito */
	if (randint0(100) < chance) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("¡No has podido concentrarte lo suficiente!");
	} else {
		/* Lanzar el hechizo */
		if (!effect_do(spell->effect, source_player(), NULL, &ident, true, dir,
					   beam, 0, cmd)) {
			return false;
		}

		/* Recompensar a COMBAT_REGEN con pequeña recuperación de PG */
		if (player_has(player, PF_COMBAT_REGEN)) {
			convert_mana_to_hp(player, spell->smana << 16);
		}

		/* Se lanzó un hechizo */
		sound(MSG_SPELL);

		if (!(player->spell_flags[spell_index] & PY_SPELL_WORKED)) {
			int e = spell->sexp;

			/* El hechizo funcionó */
			player->spell_flags[spell_index] |= PY_SPELL_WORKED;

			/* Ganar experiencia */
			player_exp_gain(player, e * spell->slevel);

			/* Redibujar el recuerdo de objeto */
			player->upkeep->redraw |= (PR_OBJECT);
		}
	}

	/* ¿Maná suficiente? */
	if (spell->smana <= player->csp) {
		/* Usar algo de maná */
		player->csp -= spell->smana;
	} else {
		int oops = spell->smana - player->csp;

		/* No queda maná */
		player->csp = 0;
		player->csp_frac = 0;

		/* Esforzar al jugador en exceso */
		player_over_exert(player, PY_EXERT_FAINT, 100, 5 * oops + 1);
		player_over_exert(player, PY_EXERT_CON, 50, 0);
	}

	/* Redibujar maná */
	player->upkeep->redraw |= (PR_MANA);

	return true;
}


bool spell_needs_aim(int spell_index)
{
	const struct class_spell *spell = spell_by_index(player, spell_index);
	assert(spell);
	return effect_aim(spell->effect);
}

static size_t append_random_value_string(char *buffer, size_t size,
										 random_value *rv)
{
	size_t offset = 0;

	if (rv->base > 0) {
		offset += strnfmt(buffer + offset, size - offset, "%d", rv->base);

		if (rv->dice > 0 && rv->sides > 0) {
			offset += strnfmt(buffer + offset, size - offset, "+");
		}
	}

	if (rv->dice == 1 && rv->sides > 0) {
		offset += strnfmt(buffer + offset, size - offset, "d%d", rv->sides);
	} else if (rv->dice > 1 && rv->sides > 0) {
		offset += strnfmt(buffer + offset, size - offset, "%dd%d", rv->dice,
						  rv->sides);
	}

	return offset;
}

static void spell_effect_append_value_info(const struct effect *effect,
		char *p, size_t len, struct spell_info_iteration_state *ist)
{
	random_value rv = { 0, 0, 0, 0 };
	const char *type = NULL;
	char special[40] = "";
	size_t offset = strlen(p);

	if (effect->index == EF_CLEAR_VALUE) {
		ist->have_shared = false;
	} else if (effect->index == EF_SET_VALUE && effect->dice) {
		ist->have_shared = true;
		dice_roll(effect->dice, &ist->shared_rv);
	}

	type = effect_info(effect);
	if (type == NULL) return;

	if (effect->dice != NULL) {
		dice_roll(effect->dice, &rv);
	} else if (ist->have_shared) {
		rv = ist->shared_rv;
	}

	/* Manejar algunos casos especiales donde queremos añadir información adicional */
	switch (effect->index) {
		case EF_HEAL_HP:
			/* Añadir solo porcentaje, ya que el valor fijo siempre se muestra */
			if (rv.m_bonus) {
				strnfmt(special, sizeof(special), "/%d%%",
					rv.m_bonus);
			}
			break;
		case EF_TELEPORT:
			/* m_bonus significa que es una cosa aleatoria extraña */
			if (rv.m_bonus) {
				my_strcpy(special, "aleatorio", sizeof(special));
			}
			break;
		case EF_SPHERE:
			/* Añadir radio */
			if (effect->radius) {
				int rad = effect->radius;
				strnfmt(special, sizeof(special), ", rad %d",
					rad);
			} else {
				my_strcpy(special, ", rad 2", sizeof(special));
			}
			break;
		case EF_BALL:
			/* Añadir radio */
			if (effect->radius) {
				int rad = effect->radius;
				if (effect->other) {
					rad += player->lev / effect->other;
				}
				strnfmt(special, sizeof(special), ", rad %d",
					rad);
			} else {
				my_strcpy(special, "rad 2", sizeof(special));
			}
			break;
		case EF_STRIKE:
			/* Añadir radio */
			if (effect->radius) {
				strnfmt(special, sizeof(special), ", rad %d",
					effect->radius);
			}
			break;
		case EF_SHORT_BEAM: {
			/* Añadir longitud del haz */
			int beam_len = effect->radius;
			if (effect->other) {
				beam_len += player->lev / effect->other;
				beam_len = MIN(beam_len, z_info->max_range);
			}
			strnfmt(special, sizeof(special), ", long %d", beam_len);
			break;
		}
		case EF_SWARM:
			/* Añadir número de proyectiles. */
			strnfmt(special, sizeof(special), "x%d", rv.m_bonus);
			break;
	}

	/*
	 * Solo mostrar si tiene dados y no es redundante con el
	 * anterior que se mostró.
	 */
	if ((rv.base > 0 || (rv.dice > 0 && rv.sides > 0))
			&& (!ist->pre
			|| ist->pre->index != effect->index
			|| !streq(special, ist->pre_special)
			|| ist->pre_rv.base != rv.base
			|| (((ist->pre_rv.dice > 0 && ist->pre_rv.sides > 0)
			|| (rv.dice > 0 && rv.sides > 0))
			&& (ist->pre_rv.dice != rv.dice
			|| ist->pre_rv.sides != rv.sides)))) {
		if (offset) {
			offset += strnfmt(p + offset, len - offset, ";");
		}

		offset += strnfmt(p + offset, len - offset, " %s ", type);
		offset += append_random_value_string(p + offset, len - offset, &rv);

		if (strlen(special) > 1) {
			strnfmt(p + offset, len - offset, "%s", special);
		}

		ist->pre = effect;
		my_strcpy(ist->pre_special, special, sizeof(ist->pre_special));
		ist->pre_rv = rv;
	}
}

void get_spell_info(int spell_index, char *p, size_t len)
{
	struct effect *effect = spell_by_index(player, spell_index)->effect;
	struct spell_info_iteration_state ist = {
		NULL, "", { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, false };

	p[0] = '\0';

	while (effect) {
		spell_effect_append_value_info(effect, p, len, &ist);
		effect = effect->next;
	}
}