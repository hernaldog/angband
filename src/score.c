/**
 * \file score.c
 * \brief Manejo de puntuaciones altas para Angband
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
#include "buildid.h"
#include "game-world.h"
#include "init.h"
#include "score.h"


/**
 * Calcula el número total de puntos ganados (wow - NRM)
 */
static long total_points(const struct player *p)
{
	return p->max_exp + 100 * p->max_depth;
}


/**
 * Leer un archivo de puntuaciones altas.
 */
size_t highscore_read(struct high_score scores[], size_t sz)
{
	char fname[1024];
	ang_file *scorefile;
	size_t i;

	/* Limpiar las puntuaciones actuales */
	memset(scores, 0, sz * sizeof(struct high_score));

	path_build(fname, sizeof(fname), ANGBAND_DIR_SCORES, "scores.raw");
	safe_setuid_grab();
	scorefile = file_open(fname, MODE_READ, FTYPE_TEXT);
	safe_setuid_drop();

	if (!scorefile) return 0;

	for (i = 0; i < sz; i++)
		if (file_read(scorefile, (char *)&scores[i],
					  sizeof(struct high_score)) <= 0)
			break;

	file_close(scorefile);
	/*
	 * En una lectura corta, también comprobar el registro uno después del final
	 * en caso de que estuviera parcialmente sobrescrito.
	 */
	(void)highscore_regularize(scores, (i < sz) ? i + 1 : sz);

	return i;
}


/**
 * Colocar una entrada en una matriz de puntuaciones altas
 */
size_t highscore_add(const struct high_score *entry, struct high_score scores[],
					 size_t sz)
{
	size_t slot = highscore_where(entry, scores, sz);

	memmove(&scores[slot + 1], &scores[slot],
			sizeof(struct high_score) * (sz - 1 - slot));
	memcpy(&scores[slot], entry, sizeof(struct high_score));

	return slot;
}

static size_t highscore_count(const struct high_score scores[], size_t sz)
{
	size_t i;
	for (i = 0; i < sz; i++)
		if (scores[i].what[0] == '\0')
			break;

	return i;
}


/**
 * Realmente colocar una entrada en el archivo de puntuaciones altas
 */
static void highscore_write(const struct high_score scores[], size_t sz)
{
	size_t n;

	ang_file *lok;
	ang_file *scorefile;

	char old_name[1024];
	char cur_name[1024];
	char new_name[1024];
	char lok_name[1024];
	bool exists;

	path_build(old_name, sizeof(old_name), ANGBAND_DIR_SCORES, "scores.old");
	path_build(cur_name, sizeof(cur_name), ANGBAND_DIR_SCORES, "scores.raw");
	path_build(new_name, sizeof(new_name), ANGBAND_DIR_SCORES, "scores.new");
	path_build(lok_name, sizeof(lok_name), ANGBAND_DIR_SCORES, "scores.lok");


	/* Leer y añadir nueva puntuación */
	n = highscore_count(scores, sz);


	/* Bloquear puntuaciones */
	safe_setuid_grab();
	exists = file_exists(lok_name);
	safe_setuid_drop();
	if (exists) {
		msg("Archivo de bloqueo en su lugar para el archivo de puntuaciones; no se escribe.");
		return;
	}

	safe_setuid_grab();
	lok = file_open(lok_name, MODE_WRITE, FTYPE_RAW);
	if (!lok) {
		safe_setuid_drop();
		msg("Fallo al crear bloqueo para el archivo de puntuaciones; no se escribe.");
		return;
	} else {
		file_lock(lok);
		safe_setuid_drop();
	}

	/* Abrir el nuevo archivo para escritura */
	safe_setuid_grab();
	scorefile = file_open(new_name, MODE_WRITE, FTYPE_RAW);
	safe_setuid_drop();

	if (!scorefile) {
		msg("Fallo al abrir el nuevo archivo de puntuaciones para escritura.");

		file_close(lok);
		safe_setuid_grab();
		file_delete(lok_name);
		safe_setuid_drop();
		return;
	}

	file_write(scorefile, (const char *)scores, sizeof(struct high_score)*n);
	file_close(scorefile);

	/* Ahora mover archivos */
	safe_setuid_grab();

	if (file_exists(old_name) && !file_delete(old_name))
		msg("No se pudo eliminar el archivo de puntuaciones antiguo");

	if (file_exists(cur_name) && !file_move(cur_name, old_name))
		msg("No se pudo mover el antiguo scores.raw fuera del camino");

	if (!file_move(new_name, cur_name))
		msg("No se pudo renombrar el nuevo archivo de puntuaciones a scores.raw");

	/* Eliminar el bloqueo */
	file_close(lok);
	file_delete(lok_name);

	safe_setuid_drop();
}



/**
 * Rellenar un registro de puntuación para el jugador dado.
 *
 * \param entry apunta al registro a rellenar.
 * \param p es el jugador cuya puntuación debe registrarse.
 * \param died_from es la razón de la muerte. En uso típico, será
 * p->died_from, pero cuando el jugador aún no está muerto, la función llamadora
 * puede querer usar otra cosa: "nadie (¡todavía!)" es tradicional.
 * \param death_time apunta al momento en que el jugador murió. Puede ser NULL
 * cuando el jugador no está muerto.
 *
 * Error: toma un argumento de jugador, pero aún accede a un poco de estado global,
 * player_uid, refiriéndose al jugador
 */
void build_score(struct high_score *entry, const struct player *p,
		const char *died_from, const time_t *death_time)
{
	memset(entry, 0, sizeof(struct high_score));

	/* Guardar la versión */
	strnfmt(entry->what, sizeof(entry->what), "%s", buildid);

	/* Calcular y guardar los puntos */
	strnfmt(entry->pts, sizeof(entry->pts), "%9ld", total_points(p));

	/* Guardar el oro actual */
	strnfmt(entry->gold, sizeof(entry->gold), "%9ld", (long)p->au);

	/* Guardar el turno actual */
	strnfmt(entry->turns, sizeof(entry->turns), "%9ld", (long)turn);

	/* Hora de la muerte */
	if (death_time)
		strftime(entry->day, sizeof(entry->day), "@%Y%m%d",
				 localtime(death_time));
	else
		my_strcpy(entry->day, "HOY", sizeof(entry->day));

	/* Guardar el nombre del jugador (15 caracteres) */
	strnfmt(entry->who, sizeof(entry->who), "%-.15s", p->full_name);

	/* Guardar la información del jugador XXX XXX XXX */
	strnfmt(entry->uid, sizeof(entry->uid), "%7u", player_uid);
	strnfmt(entry->p_r, sizeof(entry->p_r), "%2d", p->race->ridx);
	strnfmt(entry->p_c, sizeof(entry->p_c), "%2d", p->class->cidx);

	/* Guardar el nivel y tal */
	strnfmt(entry->cur_lev, sizeof(entry->cur_lev), "%3d", p->lev);
	strnfmt(entry->cur_dun, sizeof(entry->cur_dun), "%3d", p->depth);
	strnfmt(entry->max_lev, sizeof(entry->max_lev), "%3d", p->max_lev);
	strnfmt(entry->max_dun, sizeof(entry->max_dun), "%3d", p->max_depth);

	/* Sin causa de muerte */
	my_strcpy(entry->how, died_from, sizeof(entry->how));
}



/**
 * Introducir el nombre de un jugador en una tabla de puntuaciones altas, si es "legal".
 *
 * \param p es el jugador a introducir
 * \param death_time apunta al momento en que el jugador murió; puede ser NULL
 * para un jugador que aún no está muerto
 * Asume que se ha llamado a "signals_ignore_tstp()".
 */
void enter_score(const struct player *p, const time_t *death_time)
{
	int j;

	/* Los tramposos no son puntuados */
	for (j = 0; j < OPT_MAX; ++j) {
		if (option_type(j) != OP_SCORE)
			continue;
		if (!p->opts.opt[j])
			continue;

		msg("Puntuación no registrada para tramposos.");
		event_signal(EVENT_MESSAGE_FLUSH);
		return;
	}

	/* Añadir una nueva entrada, si está permitido */
	if (p->noscore & (NOSCORE_WIZARD | NOSCORE_DEBUG)) {
		msg("Puntuación no registrada para magos.");
		event_signal(EVENT_MESSAGE_FLUSH);
#ifdef ALLOW_BORG
#ifndef SCORE_BORGS
	}	else if (p->noscore & (NOSCORE_BORG)) {
		msg("Puntuación no registrada para borgs.");
		event_signal(EVENT_MESSAGE_FLUSH);
#endif
#endif
	} else if (!p->total_winner && streq(p->died_from, "Interrupción")) {
		msg("Puntuación no registrada debido a interrupción.");
		event_signal(EVENT_MESSAGE_FLUSH);
	} else if (!p->total_winner && streq(p->died_from, "Retirada")) {
		msg("Puntuación no registrada debido a retirada.");
		event_signal(EVENT_MESSAGE_FLUSH);
	} else {
		struct high_score entry;
		struct high_score scores[MAX_HISCORES];

		build_score(&entry, p, p->died_from, death_time);

		highscore_read(scores, N_ELEMENTS(scores));
		highscore_add(&entry, scores, N_ELEMENTS(scores));
		highscore_write(scores, N_ELEMENTS(scores));
	}

	/* Éxito */
	return;
}