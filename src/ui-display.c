/**
 * \file ui-display.c
 * \brief Maneja la configuración, actualización y limpieza de la visualización del juego.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007 Antony Sidwell
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
#include "cave.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-world.h"
#include "grafmode.h"
#include "hint.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-predicate.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-pile.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-timed.h"
#include "player-util.h"
#include "player.h"
#include "project.h"
#include "savefile.h"
#include "target.h"
#include "trap.h"
#include "ui-birth.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-input.h"
#include "ui-map.h"
#include "ui-mon-list.h"
#include "ui-mon-lore.h"
#include "ui-object.h"
#include "ui-obj-list.h"
#include "ui-output.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-store.h"
#include "ui-term.h"
#include "ui-visuals.h"
#include "wizard.h"

/**
 * Hay algunas funciones instaladas para ser activadas por varios de los
 * eventos básicos del jugador. Por conveniencia, se han agrupado
 * en esta lista.
 */
static game_event_type player_events[] =
{
	EVENT_RACE_CLASS,
	EVENT_PLAYERTITLE,
	EVENT_EXPERIENCE,
	EVENT_PLAYERLEVEL,
	EVENT_GOLD,
	EVENT_EQUIPMENT,  /* Para los caracteres "equippy" */
	EVENT_STATS,
	EVENT_HP,
	EVENT_MANA,
	EVENT_AC,

	EVENT_MONSTERHEALTH,

	EVENT_PLAYERSPEED,
	EVENT_DUNGEONLEVEL,
};

static game_event_type statusline_events[] =
{
	EVENT_STUDYSTATUS,
	EVENT_STATUS,
	EVENT_DETECTIONSTATUS,
	EVENT_STATE,
	EVENT_FEELING,
	EVENT_LIGHT,
};

/**
 * Abreviaturas de estadísticas saludables
 */
const char *stat_names[STAT_MAX] =
{
	"FUE: ", "INT: ", "SAB: ", "DES: ", "CON: "
};

/**
 * Abreviaturas de estadísticas dañadas
 */
const char *stat_names_reduced[STAT_MAX] =
{
	"Fue: ", "Int: ", "Sab: ", "Des: ", "Con: "
};

/**
 * Convierte un número de estadística en una cadena de seis caracteres (justificada a la derecha)
 */
void cnv_stat(int val, char *out_val, size_t out_len)
{
	/* Las estadísticas por encima de 18 necesitan tratamiento especial */
	if (val > 18) {
		int bonus = (val - 18);

		if (bonus >= 220)
			strnfmt(out_val, out_len, "18/***");
		else if (bonus >= 100)
			strnfmt(out_val, out_len, "18/%03d", bonus);
		else
			strnfmt(out_val, out_len, " 18/%02d", bonus);
	} else {
		strnfmt(out_val, out_len, "    %2d", val);
	}
}

/**
 * ------------------------------------------------------------------------
 * Funciones de visualización de la barra lateral
 * ------------------------------------------------------------------------ */

/**
 * Imprimir información del personaje en la fila, columna dadas en un campo de 13 caracteres
 */
static void prt_field(const char *info, int row, int col)
{
	/* Volcar 13 espacios para limpiar */
	c_put_str(COLOUR_WHITE, "             ", row, col);

	/* Volcar la información en sí */
	c_put_str(COLOUR_L_BLUE, info, row, col);
}


/**
 * Imprimir estadística del personaje en la fila, columna dadas
 */
static void prt_stat(int stat, int row, int col)
{
	char tmp[32];

	/* Estadística dañada o saludable */
	if (player->stat_cur[stat] < player->stat_max[stat]) {
		put_str(stat_names_reduced[stat], row, col);
		cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
		c_put_str(COLOUR_YELLOW, tmp, row, col + 6);
	} else {
		put_str(stat_names[stat], row, col);
		cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
		c_put_str(COLOUR_L_GREEN, tmp, row, col + 6);
	}

	/* Indicar máximo natural */
	if (player->stat_max[stat] == 18+100)
		put_str("!", row, col + 3);
}

static int fmt_title(char buf[], int max, bool short_mode)
{
	buf[0] = 0;

	/* Mago, ganador o ninguno */
	if (player->wizard) {
		my_strcpy(buf, "[=-MAGO-=]", max);
	} else if (player->total_winner || (player->lev > PY_MAX_LEVEL)) {
		my_strcpy(buf, "***GANADOR***", max);
	} else if (player_is_shapechanged(player)) {		
		my_strcpy(buf, player->shape->name, max);
		my_strcap(buf);		
	} else if (!short_mode) {
		my_strcpy(buf, player->class->title[(player->lev - 1) / 5], max);
	}

	return strlen(buf);
}

/**
 * Imprime el título, incluyendo mago, ganador o forma según sea necesario.
 */
static void prt_title(int row, int col)
{	
	char buf[32];

	fmt_title(buf, sizeof(buf), false);	

	prt_field(buf, row, col);
}

/**
 * Imprime nivel
 */
static void prt_level(int row, int col)
{
	char tmp[32];

	strnfmt(tmp, sizeof(tmp), "%6d", player->lev);

	if (player->lev >= player->max_lev) {
		put_str("NIVEL ", row, col);
		c_put_str(COLOUR_L_GREEN, tmp, row, col + 6);
	} else {
		put_str("Nivel ", row, col);
		c_put_str(COLOUR_YELLOW, tmp, row, col + 6);
	}
}


/**
 * Mostrar la experiencia
 */
static void prt_exp(int row, int col)
{
	char out_val[32];
	bool lev50 = (player->lev == 50);

	long xp = (long)player->exp;


	/* Calcular XP para el siguiente nivel */
	if (!lev50)
		xp = (long)(player_exp[player->lev - 1] * player->expfact / 100L) -
			player->exp;

	/* Formatear XP */
	strnfmt(out_val, sizeof(out_val), "%8ld", xp);


	if (player->exp >= player->max_exp) {
		put_str((lev50 ? "EXP" : "SIG"), row, col);
		c_put_str(COLOUR_L_GREEN, out_val, row, col + 4);
	} else {
		put_str((lev50 ? "Exp" : "Sig"), row, col);
		c_put_str(COLOUR_YELLOW, out_val, row, col + 4);
	}
}


/**
 * Imprime el oro actual
 */
static void prt_gold(int row, int col)
{
	char tmp[32];

	put_str("AU ", row, col);
	strnfmt(tmp, sizeof(tmp), "%9ld", (long)player->au);
	c_put_str(COLOUR_L_GREEN, tmp, row, col + 3);
}


/**
 * Caracteres "equippy" (representación ASCII del equipo en orden de ranura)
 */
static void prt_equippy(int row, int col)
{
	int i;

	uint8_t a;
	wchar_t c;

	struct object *obj;

	/* Volcar caracteres "equippy" */
	for (i = 0; i < player->body.count; i++) {
		/* Objeto */
		obj = slot_object(player, i);

		/* Obtener atributo/carácter para mostrar; limpiar si hay mosaicos grandes o ningún objeto */
		if (obj && tile_width == 1 && tile_height == 1) {
			c = object_char(obj);
			a = object_attr(obj);
		} else {
			c = L' ';
			a = COLOUR_WHITE;
		}

		/* Volcar */
		Term_putch(col + i, row, a, c);
	}
}


/**
 * Imprime la CA actual
 */
static void prt_ac(int row, int col)
{
	char tmp[32];

	put_str("CA Act ", row, col);
	strnfmt(tmp, sizeof(tmp), "%5d", 
			player->known_state.ac + player->known_state.to_a);
	c_put_str(COLOUR_L_GREEN, tmp, row, col + 7);
}

/**
 * Imprime los puntos de golpe actuales
 */
static void prt_hp(int row, int col)
{
	char cur_hp[32], max_hp[32];
	uint8_t color = player_hp_attr(player);

	put_str("PG ", row, col);

	strnfmt(max_hp, sizeof(max_hp), "%4d", player->mhp);
	strnfmt(cur_hp, sizeof(cur_hp), "%4d", player->chp);
	
	c_put_str(color, cur_hp, row, col + 3);
	c_put_str(COLOUR_WHITE, "/", row, col + 7);
	c_put_str(COLOUR_L_GREEN, max_hp, row, col + 8);
}

/**
 * Imprime los puntos de hechizo máximos/actuales del jugador
 */
static void prt_sp(int row, int col)
{
	char cur_sp[32], max_sp[32];
	uint8_t color = player_sp_attr(player);

	/* No mostrar maná a menos que debamos tener algo */
	if (!player->class->magic.total_spells
			|| (player->lev < player->class->magic.spell_first)) {
		/*
		 * Pero limpiar si el drenaje de experiencia puede haber dejado sin puntos después
		 * de tener puntos.
		 */
		if (player->class->magic.total_spells
				&& player->exp < player->max_exp) {
			put_str("            ", row, col);
		}
		return;
	}

	put_str("PM ", row, col);

	strnfmt(max_sp, sizeof(max_sp), "%4d", player->msp);
	strnfmt(cur_sp, sizeof(cur_sp), "%4d", player->csp);

	/* Mostrar maná */
	c_put_str(color, cur_sp, row, col + 3);
	c_put_str(COLOUR_WHITE, "/", row, col + 7);
	c_put_str(COLOUR_L_GREEN, max_sp, row, col + 8);
}

/**
 * Calcular el color de la barra de monstruo por separado, para los puertos.
 */
uint8_t monster_health_attr(void)
{
	struct monster *mon = player->upkeep->health_who;
	uint8_t attr;

	if (!mon) {
		/* No rastreando */
		attr = COLOUR_DARK;

	} else if (!monster_is_visible(mon) || mon->hp < 0 ||
			   player->timed[TMD_IMAGE]) {
		/* La salud del monstruo es "desconocida" */
		attr = COLOUR_WHITE;

	} else {
		int pct;

		/* Por defecto, casi muerto */
		attr = COLOUR_RED;

		/* Extraer el "porcentaje" de salud */
		pct = 100L * mon->hp / mon->maxhp;

		/* Gravemente herido */
		if (pct >= 10) attr = COLOUR_L_RED;

		/* Herido */
		if (pct >= 25) attr = COLOUR_ORANGE;

		/* Algo herido */
		if (pct >= 60) attr = COLOUR_YELLOW;

		/* Saludable */
		if (pct >= 100) attr = COLOUR_L_GREEN;

		/* Asustado */
		if (mon->m_timed[MON_TMD_FEAR]) attr = COLOUR_VIOLET;

		/* Desencantado */
		if (mon->m_timed[MON_TMD_DISEN]) attr = COLOUR_L_UMBER;

		/* Comandado */
		if (mon->m_timed[MON_TMD_COMMAND]) attr = COLOUR_L_PURPLE;

		/* Confundido */
		if (mon->m_timed[MON_TMD_CONF]) attr = COLOUR_UMBER;

		/* Aturdido */
		if (mon->m_timed[MON_TMD_STUN]) attr = COLOUR_L_BLUE;

		/* Dormido */
		if (mon->m_timed[MON_TMD_SLEEP]) attr = COLOUR_BLUE;

		/* Paralizado */
		if (mon->m_timed[MON_TMD_HOLD]) attr = COLOUR_BLUE;
	}

	return attr;
}

static int prt_health_aux(int row, int col)
{
	uint8_t attr = monster_health_attr();
	struct monster *mon = player->upkeep->health_who;

	/* No rastreando */
	if (!mon) {
		/* Borrar la barra de salud */
		Term_erase(col, row, 12);
		return 0;
	}

	/* Rastreando un monstruo no visto, alucinado o muerto */
	if (!monster_is_visible(mon) || /* No visto */
		(player->timed[TMD_IMAGE]) || /* Alucinación */
		(mon->hp < 0)) { /* Muerto (?) */
		/* La salud del monstruo es "desconocida" */
		Term_putstr(col, row, 12, attr, "[----------]");
	} else { /* Visible */
		/* Extraer el "porcentaje" de salud */
		int pct = 100L * mon->hp / mon->maxhp;

		/* Convertir porcentaje en "salud" */
		int len = (pct < 10) ? 1 : (pct < 90) ? (pct / 10 + 1) : 10;

		/* Por defecto, "desconocido" */
		Term_putstr(col, row, 12, COLOUR_WHITE, "[----------]");

		/* Volcar la "salud" actual (usar símbolos '*') */
		Term_putstr(col + 1, row, len, attr, "**********");
	}

	return 12;
}

/**
 * Redibujar la "barra de salud del monstruo"
 *
 * La "barra de salud del monstruo" proporciona retroalimentación visual sobre la "salud"
 * del monstruo que se está "rastreando" actualmente. Hay varias formas
 * de "rastrear" un monstruo, incluyendo apuntarle, atacarlo y
 * afectarlo (y a nadie más) con un ataque a distancia. Cuando no se
 * está rastreando nada, limpiamos la barra de salud. Si el monstruo que se está
 * rastreando no es visible actualmente, se muestra una barra de salud especial.
 */
static void prt_health(int row, int col)
{
	prt_health_aux(row, col);
}

static int prt_speed_aux(char buf[], int max, uint8_t *attr)
{
	int i = player->state.speed;
	const char *type = NULL;

	*attr = COLOUR_WHITE;
	buf[0] = 0;

	/* 110 es velocidad normal, y no requiere visualización */
	if (i > 110) {
		*attr = COLOUR_L_GREEN;
		type = "Rápido";
	} else if (i < 110) {
		*attr = COLOUR_L_UMBER;
		type = "Lento";
	}

	if (type && !OPT(player, effective_speed))
		strnfmt(buf, max, "%s (%+d)", type, (i - 110));
	else if (type && OPT(player, effective_speed))
	{
		int multiplier = 10 * extract_energy[i] / extract_energy[110];
		int int_mul = multiplier / 10;
		int dec_mul = multiplier % 10;
		strnfmt(buf, max, "%s (%d.%dx)", type, int_mul, dec_mul);
	}

	return strlen(buf);
}

/**
 * Imprime la velocidad de un personaje.
 */
static void prt_speed(int row, int col)
{
	uint8_t attr = COLOUR_WHITE;
	char buf[32] = "";

	prt_speed_aux(buf, sizeof(buf), &attr);

	/* Mostrar la velocidad */
	c_put_str(attr, format("%-11s", buf), row, col);
}

static int fmt_depth(char buf[], int max)
{
	if (!player->depth)
		my_strcpy(buf, "Ciudad", max);
	else
		strnfmt(buf, max, "%d' (N%d)",
		        player->depth * 50, player->depth);
	return strlen(buf);
}

/**
 * Imprime la profundidad en el área de estadísticas
 */
static void prt_depth(int row, int col)
{
	char depths[32];

	fmt_depth(depths, sizeof(depths));

	/* Alinear a la derecha la "profundidad" y limpiar valores antiguos */
	put_str(format("%-13s", depths), row, col);
}




/**
 * Algunas funciones envoltorio simples
 */
static void prt_str(int row, int col) { prt_stat(STAT_STR, row, col); }
static void prt_dex(int row, int col) { prt_stat(STAT_DEX, row, col); }
static void prt_wis(int row, int col) { prt_stat(STAT_WIS, row, col); }
static void prt_int(int row, int col) { prt_stat(STAT_INT, row, col); }
static void prt_con(int row, int col) { prt_stat(STAT_CON, row, col); }
static void prt_race(int row, int col) {
	if (player_is_shapechanged(player)) {
		prt_field("", row, col);
	} else {
		prt_field(player->race->name, row, col);
	}
}

static int prt_race_class_short(int row, int col)
{
	char buf[512] = "";

	if (player_is_shapechanged(player)) return 0;

	strnfmt(buf, sizeof(buf), "%s %s",
		player->race->name,
		player->class->title[(player->lev - 1) / 5]);

	c_put_str(COLOUR_L_GREEN, buf, row, col);

	return strlen(buf)+1;
}

static void prt_class(int row, int col) {
	if (player_is_shapechanged(player)) {
		prt_field("", row, col);
	} else {
		prt_field(player->class->name, row, col);
	}
}

/**
 * Imprime nivel
 */
static int prt_level_short(int row, int col)
{
	char tmp[32];

	strnfmt(tmp, sizeof(tmp), "%d", player->lev);

	if (player->lev >= player->max_lev) {
		put_str("N:", row, col);
		c_put_str(COLOUR_L_GREEN, tmp, row, col + 2);
	} else {
		put_str("n:", row, col);
		c_put_str(COLOUR_YELLOW, tmp, row, col + 2);
	}

	return 3+strlen(tmp);
}

static int prt_stat_short(int stat, int row, int col)
{
	char tmp[32];

	/* Estadística dañada o saludable */
	if (player->stat_cur[stat] < player->stat_max[stat]) {
		put_str(format("%c:", stat_names_reduced[stat][0]), row, col);		
		cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
		/* Eliminar espacios en blanco */
		strskip(tmp,' ', 0);
		c_put_str(COLOUR_YELLOW, tmp, row, col + 2);
	} else {
		put_str(format("%c:", stat_names[stat][0]), row, col);
		cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
		/* Eliminar espacios en blanco */
		strskip(tmp,' ', 0);
		if (player->stat_max[stat] == 18+100) {
			c_put_str(COLOUR_L_BLUE, tmp, row, col + 2);
		}
		else {
			c_put_str(COLOUR_L_GREEN, tmp, row, col + 2);	
		}		
	}

	return 3+strlen(tmp);
}

static int prt_exp_short(int row, int col)
{
	char out_val[32];
	bool lev50 = (player->lev == 50);

	long xp = (long)player->exp;

	/* Calcular XP para el siguiente nivel */
	if (!lev50)
		xp = (long)(player_exp[player->lev - 1] * player->expfact / 100L) -
			player->exp;

	/* Formatear XP */
	strnfmt(out_val, sizeof(out_val), "%ld", xp);

	if (player->exp >= player->max_exp) {
		put_str((lev50 ? "EXP:" : "SIG:"), row, col);
		c_put_str(COLOUR_L_GREEN, out_val, row, col + 4);
	} else {
		put_str((lev50 ? "exp:" : "sig:"), row, col);
		c_put_str(COLOUR_YELLOW, out_val, row, col + 4);
	}

	return 5+strlen(out_val);
}

static int prt_ac_short(int row, int col)
{
	char tmp[32];

	put_str("CA:", row, col);
	strnfmt(tmp, sizeof(tmp), "%d", 
			player->known_state.ac + player->known_state.to_a);
	c_put_str(COLOUR_L_GREEN, tmp, row, col + 3);
	return 4+strlen(tmp);
}

static int prt_gold_short(int row, int col)
{
	char tmp[32];

	put_str("AU:", row, col);
	strnfmt(tmp, sizeof(tmp), "%ld", (long)player->au);
	c_put_str(COLOUR_L_GREEN, tmp, row, col + 3);
	return 4+strlen(tmp);
}

static int prt_hp_short(int row, int col)
{
	char cur_hp[32], max_hp[32];
	uint8_t color = player_hp_attr(player);

	put_str("PG:", row, col);
	col += 3;

	strnfmt(max_hp, sizeof(max_hp), "%d", player->mhp);
	strnfmt(cur_hp, sizeof(cur_hp), "%d", player->chp);
	
	c_put_str(color, cur_hp, row, col);
	col += strlen(cur_hp);
	c_put_str(COLOUR_WHITE, "/", row, col);
	col += 1;
	c_put_str(COLOUR_L_GREEN, max_hp, row, col);
	return 5+strlen(cur_hp)+strlen(max_hp);
}

static int prt_sp_short(int row, int col)
{
	char cur_sp[32], max_sp[32];
	uint8_t color = player_sp_attr(player);

	/* No mostrar maná a menos que debamos tener algo */
	if (!player->class->magic.total_spells
			|| (player->lev < player->class->magic.spell_first))
		return 0;

	put_str("PM:", row, col);
	col += 3;

	strnfmt(max_sp, sizeof(max_sp), "%d", player->msp);
	strnfmt(cur_sp, sizeof(cur_sp), "%d", player->csp);

	/* Mostrar maná */
	c_put_str(color, cur_sp, row, col);
	col += strlen(cur_sp);
	c_put_str(COLOUR_WHITE, "/", row, col);
	col += 1;
	c_put_str(COLOUR_L_GREEN, max_sp, row, col);
	return 5+strlen(cur_sp)+strlen(max_sp);
}

static int prt_health_short(int row, int col)
{
	int len = prt_health_aux(row, col);
	if (len > 0) {
		return len+1;
	}
	return 0;
}

static int prt_speed_short(int row, int col)
{
	char buf[32];
	uint8_t attr;

	int len = prt_speed_aux(buf, sizeof(buf), &attr);	
	if (len > 0) {
		c_put_str(attr, buf, row, col);
		return len+1;
	}
	return 0;
}

static int prt_depth_short(int row, int col)
{
	char buf[32];
	
	int len = fmt_depth(buf, sizeof(buf));
	put_str(buf, row, col);
	return len+1;
}

static int prt_title_short(int row, int col)
{
	char buf[32];
	
	int len = fmt_title(buf, sizeof(buf), true);
	if (len > 0) {
		c_put_str(COLOUR_YELLOW, buf, row, col);	
		return len+1;
	}	
	return 0;
}

static void update_topbar(game_event_type type, game_event_data *data,
						  void *user, int row)
{	
	int col = 0;	

	prt("", row, col);	

	col += prt_level_short(row, col);

	col += prt_exp_short(row, col);
	
	col += prt_stat_short(STAT_STR, row, col);
	col += prt_stat_short(STAT_INT, row, col);
	col += prt_stat_short(STAT_WIS, row, col);
	col += prt_stat_short(STAT_DEX, row, col);
	col += prt_stat_short(STAT_CON, row, col);

	col += prt_ac_short(row, col);

	col += prt_gold_short(row, col);

	col += prt_race_class_short(row, col);

	++row;
	col = 0;

	prt("", row, col);

	col += prt_hp_short(row, col);
	col += prt_sp_short(row, col);
	col += prt_health_short(row, col);	
	col += prt_speed_short(row, col);
	col += prt_depth_short(row, col);
	col += prt_title_short(row, col);
}


/**
 * Estructura de manejadores de la barra lateral.
 */
static const struct side_handler_t
{
	void (*hook)(int, int);	 /* int fila, int columna */
	int priority;		 /* 1 es el más importante (siempre mostrado) */
	game_event_type type;	 /* bandera PR_* a la que corresponde */
} side_handlers[] = {
	{ prt_race,    19, EVENT_RACE_CLASS },
	{ prt_title,   18, EVENT_PLAYERTITLE },
	{ prt_class,   22, EVENT_RACE_CLASS },
	{ prt_level,   10, EVENT_PLAYERLEVEL },
	{ prt_exp,     16, EVENT_EXPERIENCE },
	{ prt_gold,    11, EVENT_GOLD },
	{ prt_equippy, 17, EVENT_EQUIPMENT },
	{ prt_str,      6, EVENT_STATS },
	{ prt_int,      5, EVENT_STATS },
	{ prt_wis,      4, EVENT_STATS },
	{ prt_dex,      3, EVENT_STATS },
	{ prt_con,      2, EVENT_STATS },
	{ NULL,        15, 0 },
	{ prt_ac,       7, EVENT_AC },
	{ prt_hp,       8, EVENT_HP },
	{ prt_sp,       9, EVENT_MANA },
	{ NULL,        21, 0 },
	{ prt_health,  12, EVENT_MONSTERHEALTH },
	{ NULL,        20, 0 },
	{ NULL,        22, 0 },
	{ prt_speed,   13, EVENT_PLAYERSPEED }, /* Lento (-NN) / Rápido (+NN) */
	{ prt_depth,   14, EVENT_DUNGEONLEVEL }, /* Nivel NNN / NNNN pies */
};


/**
 * Esto imprime la barra lateral, utilizando un método inteligente que significa que solo
 * imprimirá tanto como se pueda mostrar en pantallas de <24 líneas.
 *
 * A cada fila se le da una prioridad; los números más altos son los menos importantes y los números
 * más bajos los más importantes. A medida que la pantalla se hace más pequeña, las filas comienzan a
 * desaparecer en el orden de menor a mayor importancia.
 */
static void update_sidebar(game_event_type type, game_event_data *data,
						   void *user)
{
	int x, y, row;
	int max_priority;
	size_t i;

	if (Term->sidebar_mode == SIDEBAR_NONE) {		
		return;
	}

	if (Term->sidebar_mode == SIDEBAR_TOP) {
		update_topbar(type, data, user, 1);
		return;
	}

	Term_get_size(&x, &y);

	/* Mantener las líneas superior e inferior limpias. */
	max_priority = y - 2;

	/* Mostrar entradas de la lista */
	for (i = 0, row = 1; i < N_ELEMENTS(side_handlers); i++) {
		const struct side_handler_t *hnd = &side_handlers[i];
		int priority = hnd->priority;
		bool from_bottom = false;

		/* Negativo significa imprimir desde abajo */
		if (priority < 0) {
			priority = -priority;
			from_bottom = true;
		}

		/* Si esto tiene la prioridad suficientemente alta, mostrarlo */
		if (priority <= max_priority) {
			if (hnd->type == type && hnd->hook) {
				if (from_bottom)
					hnd->hook(Term->hgt - (N_ELEMENTS(side_handlers) - i), 0);
				else
				    hnd->hook(row, 0);
			}

			/* Incrementar para la próxima vez */
			row++;
		}
	}
}

/**
 * Redibujar al jugador, ya que el color del jugador indica la salud aproximada. Nótese
 * que usar este comando es solo para cuando el modo gráfico está desactivado, ya que
 * de lo contrario hace que el carácter sea un cuadrado negro.
 */
static void hp_colour_change(game_event_type type, game_event_data *data,
							 void *user)
{
	if ((OPT(player, hp_changes_color)) && (use_graphics == GRAPHICS_NONE))
		square_light_spot(cave, player->grid);
}



/**
 * ------------------------------------------------------------------------
 * Funciones de visualización de la línea de estado
 * ------------------------------------------------------------------------ */

/**
 * Estructura para describir diferentes efectos temporales
 */
struct state_info
{
	int value;
	const char *str;
	size_t len;
	uint8_t attr;
};

/**
 * Imprimir estado de retorno.
 */
static size_t prt_recall(int row, int col)
{
	if (player->word_recall) {
		c_put_str(COLOUR_WHITE, "Retorno", row, col);
		return sizeof "Retorno";
	}

	return 0;
}


/**
 * Imprimir estado de descenso profundo.
 */
static size_t prt_descent(int row, int col)
{
	if (player->deep_descent) {
		c_put_str(COLOUR_WHITE, "Descenso", row, col);
		return sizeof "Descenso";
	}

	return 0;
}


/**
 * Imprime el estado de Descanso o 'contador'
 * La pantalla tiene siempre exactamente 10 caracteres de ancho (ver abajo)
 *
 * Esta función era un cuello de botella importante al descansar, por lo que gran parte
 * del código de formato de texto se optimizó in situ a continuación.
 */
static size_t prt_state(int row, int col)
{
	uint8_t attr = COLOUR_WHITE;

	char text[16] = "";


	/* Los estados mostrados son descanso y repetición */
	if (player_is_resting(player)) {
		int i;
		int n = player_resting_count(player);

		/* Empezar con "Desc" */
		my_strcpy(text, "Desc      ", sizeof(text));

		/* Mostrar según la longitud o la intención del descanso */
		if (n >= 1000) {
			i = n / 100;
			text[9] = '0';
			text[8] = '0';
			text[7] = I2D(i % 10);
			if (i >= 10) {
				i = i / 10;
				text[6] = I2D(i % 10);
				if (i >= 10)
					text[5] = I2D(i / 10);
			}
		} else if (n >= 100) {
			i = n;
			text[9] = I2D(i % 10);
			i = i / 10;
			text[8] = I2D(i % 10);
			text[7] = I2D(i / 10);
		} else if (n >= 10) {
			i = n;
			text[9] = I2D(i % 10);
			text[8] = I2D(i / 10);
		} else if (n > 0) {
			i = n;
			text[9] = I2D(i);
		} else if (n == REST_ALL_POINTS)
			text[5] = text[6] = text[7] = text[8] = text[9] = '*';
		else if (n == REST_COMPLETE)
			text[5] = text[6] = text[7] = text[8] = text[9] = '&';
		else if (n == REST_SOME_POINTS)
			text[5] = text[6] = text[7] = text[8] = text[9] = '!';

	} else if (cmd_get_nrepeats()) {
		int nrepeats = cmd_get_nrepeats();

		if (nrepeats > 999)
			strnfmt(text, sizeof(text), "Rep. %3d00", nrepeats / 100);
		else
			strnfmt(text, sizeof(text), "Repetir %3d", nrepeats);
	}

	/* Mostrar la información (o espacios en blanco) */
	c_put_str(attr, text, row, col);

	return strlen(text) + 1;
}

static const uint8_t obj_feeling_color[] =
{
	/* Colores utilizados para mostrar cada sensación de objeto */
	COLOUR_WHITE,  /* "Parece un nivel cualquiera." */
	COLOUR_L_PURPLE, /* "¡sientes un objeto de poder maravilloso!" */
	COLOUR_L_RED, /* "hay tesoros soberbios aquí." */
	COLOUR_ORANGE, /* "hay tesoros excelentes aquí." */
	COLOUR_YELLOW, /* "hay tesoros muy buenos aquí." */
	COLOUR_YELLOW, /* "hay tesoros buenos aquí." */
	COLOUR_L_GREEN, /* "puede haber algo que valga la pena aquí." */
	COLOUR_L_GREEN, /* "puede que no haya mucho interesante aquí." */
	COLOUR_L_GREEN, /* "no hay muchos tesoros aquí." */
	COLOUR_L_BLUE, /* "solo hay restos de basura aquí." */
	COLOUR_L_BLUE  /* "no hay más que telarañas aquí. */
};

static const uint8_t mon_feeling_color[] =
{
	/* Colores utilizados para mostrar cada sensación de monstruo */
	COLOUR_WHITE, /* "Aún no estás seguro sobre este lugar" */
	COLOUR_RED, /* "Augurios de muerte acechan este lugar" */
	COLOUR_ORANGE, /* "Este lugar parece asesino" */
	COLOUR_ORANGE, /* "Este lugar parece terriblemente peligroso" */
	COLOUR_YELLOW, /* "Te sientes ansioso sobre este lugar" */
	COLOUR_YELLOW, /* "Te sientes nervioso sobre este lugar" */
	COLOUR_GREEN, /* "Este lugar no parece demasiado arriesgado" */
	COLOUR_GREEN, /* "Este lugar parece razonablemente seguro" */
	COLOUR_BLUE, /* "Este parece un lugar manso y resguardado" */
	COLOUR_BLUE, /* "Este parece un lugar tranquilo y pacífico" */
};

/**
 * Imprime las sensaciones de nivel en el estado si están habilitadas.
 */
static size_t prt_level_feeling(int row, int col)
{
	uint16_t obj_feeling;
	uint16_t mon_feeling;
	char obj_feeling_str[6];
	char mon_feeling_str[6];
	int new_col;
	uint8_t obj_feeling_color_print;

	/* No mostrar sensaciones para personajes de corazón frío */
	if (!OPT(player, birth_feelings)) return 0;

	/* Sin sensación útil en la ciudad */
	if (!player->depth) return 0;

	/* Obtener sensaciones */
	obj_feeling = cave->feeling / 10;
	mon_feeling = cave->feeling - (10 * obj_feeling);

	/*
	 *   Convertir la sensación de objeto a un símbolo más fácil de interpretar
	 * para un humano.
	 *   0 -> * "Parece un nivel cualquiera."
	 *   1 -> $ "¡sientes un objeto de poder maravilloso!" (sensación especial)
	 *   2 a 10 son sensaciones desde 2 que significa sensación soberbia hasta 10
	 * que significa no hay más que telarañas.
	 *   Es más fácil para el jugador tener las sensaciones malas como un
	 * número bajo y las sensaciones soberbias como uno más alto. Así que para
	 * la pantalla invertimos estos números y restamos 1.
	 *   Así (2-10) se convierte en (1-9 invertido)
	 *
	 *   Pero antes de eso comprobar si el jugador ha explorado lo suficiente
	 * para obtener una sensación. Si no, mostrar como ?
	 */
	if (cave->feeling_squares < z_info->feeling_need) {
		my_strcpy(obj_feeling_str, "?", sizeof(obj_feeling_str));
		obj_feeling_color_print = COLOUR_WHITE;
	} else {
		obj_feeling_color_print = obj_feeling_color[obj_feeling];
		if (obj_feeling == 0)
			my_strcpy(obj_feeling_str, "*", sizeof(obj_feeling_str));
		else if (obj_feeling == 1)
			my_strcpy(obj_feeling_str, "$", sizeof(obj_feeling_str));
		else
			strnfmt(obj_feeling_str, 5, "%d", (unsigned int) (11-obj_feeling));
	}

	/* 
	 *   Convertir la sensación de monstruo a un símbolo más fácil de interpretar
	 * para un humano.
	 *   0 -> ? . La sensación de monstruo nunca debería ser 0, pero lo comprobamos
	 * por si acaso.
	 *   1 a 9 son sensaciones desde augurios de muerte hasta tranquilo y pacífico.
	 * También invertimos esto para que lo que mostramos sea una sensación de peligro.
	 */
	if (mon_feeling == 0)
		my_strcpy( mon_feeling_str, "?", sizeof(mon_feeling_str) );
	else
		strnfmt(mon_feeling_str, 5, "%d", (unsigned int) ( 10-mon_feeling ));

	/* Mostrarlo */
	c_put_str(COLOUR_WHITE, "SN:", row, col);
	new_col = col + 3;
	c_put_str(mon_feeling_color[mon_feeling], mon_feeling_str, row, new_col);
	new_col += strlen( mon_feeling_str );
	c_put_str(COLOUR_WHITE, "-", row, new_col);
	++new_col;
	c_put_str(obj_feeling_color_print, obj_feeling_str,	row, new_col);
	new_col += strlen( obj_feeling_str ) + 1;

	return new_col - col;
}

/**
 * Imprime el nivel de luz de la casilla del jugador
 */
static size_t prt_light(int row, int col)
{
	int light = square_light(cave, player->grid);

	if (light > 0) {
		c_put_str(COLOUR_YELLOW, format("Luz %d ", light), row, col);
	} else {
		c_put_str(COLOUR_PURPLE, format("Luz %d ", light), row, col);
	}

	return 8 + (ABS(light) > 9 ? 1 : 0) + (light < 0 ? 1 : 0);
}

/**
 * Imprime la velocidad de movimiento de un personaje.
 */
static size_t prt_moves(int row, int col)
{
	int i = player->state.num_moves;

	/* 1 movimiento es normal y no requiere visualización */
	if (i > 0) {
		/* Mostrar el número de movimientos */
		c_put_str(COLOUR_L_TEAL, format("Mov +%d ", i), row, col);
	} else if (i < 0) {
		/* Mostrar el número de movimientos */
		c_put_str(COLOUR_L_TEAL, format("Mov -%d ", ABS(i)), row, col);
	}

	/* No debería tener doble dígito, pero seamos paranoicos */
	return (i != 0) ? (9 + ABS(i) / 10) : 0;
}

/**
 * Obtener el nombre de terreno o trampa relevante más largo para prt_terrain()
 */
static int longest_terrain_name(void)
{
	size_t i, max = 0;
	for (i = 0; i < z_info->trap_max; i++) {
		if (strlen(trap_info[i].name) > max) {
			max = strlen(trap_info[i].name);
		}
	}
	for (i = 0; i < FEAT_MAX; i++) {
		if (strlen(f_info[i].name) > max) {
			max = strlen(f_info[i].name);
		}
	}
	return max;
}

/**
 * Imprime la trampa del jugador (si la hay) o el terreno
 */
static size_t prt_terrain(int row, int col)
{
	struct feature *feat = square_feat(cave, player->grid);
	struct trap *trap = square_trap(cave, player->grid);
	char buf[30];
	uint8_t attr;

	if (trap && !square_isinvis(cave, player->grid)) {
		my_strcpy(buf, trap->kind->name, sizeof(buf));
		attr = trap->kind->d_attr;
	} else {
		my_strcpy(buf, feat->name, sizeof(buf));
		attr = feat->d_attr;
	}
	my_strcap(buf);
	c_put_str(attr, format("%s ", buf), row, col);

	return longest_terrain_name() + 1;
}

/**
 * Imprime el estado de detección de trampas
 */
static size_t prt_dtrap(int row, int col)
{
	/* El jugador está en una casilla con trampas detectadas */
	if (square_isdtrap(cave, player->grid)) {
		/* El jugador está en el borde */
		if (square_dtrap_edge(cave, player->grid))
			c_put_str(COLOUR_YELLOW, "DTrampa ", row, col);
		else
			c_put_str(COLOUR_L_GREEN, "DTrampa ", row, col);

		return 6;
	}

	return 0;
}

/**
 * Imprime cuántos hechizos puede estudiar el jugador.
 */
static size_t prt_study(int row, int col)
{
	char *text;
	int attr = COLOUR_WHITE;

	/* ¿Puede el jugador aprender nuevos hechizos? */
	if (player->upkeep->new_spells) {
		/* Si el jugador no lleva un libro con hechizos que pueda estudiar,
		   el mensaje se muestra en un color más oscuro */
		if (!player_book_has_unlearned_spells(player))
			attr = COLOUR_L_DARK;

		/* Imprimir mensaje de estudio */
		text = format("Estudio (%d)", player->upkeep->new_spells);
		c_put_str(attr, text, row, col);
		return strlen(text) + 1;
	}

	return 0;
}


/**
 * Imprime todos los efectos temporales.
 */
static size_t prt_tmd(int row, int col)
{
	size_t i, len = 0;

	for (i = 0; i < TMD_MAX; i++) {
		if (player->timed[i]) {
			struct timed_grade *grade = timed_effects[i].grade;
			while (player->timed[i] > grade->max) {
				grade = grade->next;
			}
			if (!grade->name) continue;
			c_put_str(grade->color, grade->name, row, col + len);
			len += strlen(grade->name) + 1;

			/* Medidor de comida */
			if (i == TMD_FOOD) {
				char *meter = format("%d %%", player->timed[i] / 100);
				c_put_str(grade->color, meter, row, col + len);
				len += strlen(meter) + 1;
			}
		}
	}

	return len;
}

/**
 * Imprime el estado de "no ignorar"
 */
static size_t prt_unignore(int row, int col)
{
	if (player->unignoring) {
		const char *str = "NoIgnorar";
		put_str(str, row, col);
		return strlen(str) + 1;
	}

	return 0;
}

/**
 * Definición de tipo descriptivo para manejadores de estado
 */
typedef size_t status_f(int row, int col);

static status_f *status_handlers[] =
{ prt_level_feeling, prt_light, prt_moves, prt_unignore, prt_recall,
  prt_descent, prt_state, prt_study, prt_tmd, prt_dtrap, prt_terrain };


static void update_statusline_aux(int row, int col)
{
	size_t i;

	/* Limpiar el resto de la línea */
	prt("", row, col);

	/* Mostrar aquellos que necesitan redibujado */
	for (i = 0; i < N_ELEMENTS(status_handlers); i++)
		col += status_handlers[i](row, col);
}

/**
 * Imprimir la línea de estado.
 */
static void update_statusline(game_event_type type, game_event_data *data, void *user)
{
	int row = Term->hgt - 1;

	if (Term->sidebar_mode == SIDEBAR_TOP) {
		row = 3;
	}

	update_statusline_aux(row, COL_MAP);
}


/**
 * ------------------------------------------------------------------------
 * Redibujado del mapa.
 * ------------------------------------------------------------------------ */

#ifdef MAP_DEBUG
static void trace_map_updates(game_event_type type, game_event_data *data,
							  void *user)
{
	if (data->point.x == -1 && data->point.y == -1)
		printf("Redibujar mapa completo\n");
	else
		printf("Redibujar (%i, %i)\n", data->point.x, data->point.y);
}
#endif

/**
 * Actualizar ya sea una sola casilla del mapa o un mapa completo
 */
static void update_maps(game_event_type type, game_event_data *data, void *user)
{
	term *t = user;

	/* Esto señala un redibujado de mapa completo. */
	if (data->point.x == -1 && data->point.y == -1)
		prt_map();

	/* Punto único a redibujar */
	else {
		struct grid_data g;
		int a, ta;
		wchar_t c, tc;

		int ky, kx;
		int vy, vx;
		int clipy;

		/* Ubicación relativa al panel */
		ky = data->point.y - t->offset_y;
		kx = data->point.x - t->offset_x;

		if (t == angband_term[0]) {
			/* Verificar ubicación */
			if ((ky < 0) || (ky >= SCREEN_HGT)) return;
			if ((kx < 0) || (kx >= SCREEN_WID)) return;

			/* Ubicación en la ventana */
			vy = tile_height * ky + ROW_MAP;
			vx = tile_width * kx + COL_MAP;

			/* Proteger la línea de estado contra modificación. */
			clipy = ROW_MAP + SCREEN_ROWS;
		} else {
			/* Verificar ubicación */
			if ((ky < 0) || (ky >= t->hgt / tile_height)) return;
			if ((kx < 0) || (kx >= t->wid / tile_width)) return;

			/* Ubicación en la ventana */
			vy = tile_height * ky;
			vx = tile_width * kx;

			/* Todas las filas pueden ser usadas para el mapa. */
			clipy = t->hgt;
		}


		/* Redibujar la casilla */
		map_info(data->point, &g);
		grid_data_as_text(&g, &a, &c, &ta, &tc);
		Term_queue_char(t, vx, vy, a, c, ta, tc);
#ifdef MAP_DEBUG
		/* Trazar actualizaciones 'puntuales' en verde claro para hacerlas visibles */
		Term_queue_char(t, vx, vy, COLOUR_L_GREEN, c, ta, tc);
#endif

		if ((tile_width > 1) || (tile_height > 1))
			Term_big_queue_char(t, vx, vy, clipy, a, c, COLOUR_WHITE, L' ');
	}

	/* Refrescar la pantalla principal a menos que el mapa necesite centrarse */
	if (player->upkeep->update & (PU_PANEL) && OPT(player, center_player)) {
		int hgt = (t == angband_term[0]) ? SCREEN_HGT / 2 :
			t->hgt / (tile_height * 2);
		int wid = (t == angband_term[0]) ? SCREEN_WID / 2 :
			t->wid / (tile_width * 2);

		if (panel_should_modify(t, player->grid.y - hgt, player->grid.x - wid))
			return;
	}

	Term_fresh();
}

/**
 * ------------------------------------------------------------------------
 * Animaciones.
 * ------------------------------------------------------------------------ */

static bool animations_allowed = true;
/**
 * Un contador para seleccionar el color del paso de la tabla de parpadeo.
 */
static uint8_t flicker = 0;

/**
 * Esto anima monstruos y/u objetos según sea necesario.
 */
static void do_animation(void)
{
	int i;

	for (i = 1; i < cave_monster_max(cave); i++) {
		uint8_t attr;
		struct monster *mon = cave_monster(cave, i);

		if (!mon || !mon->race || !monster_is_visible(mon))
			continue;
		else if (rf_has(mon->race->flags, RF_ATTR_MULTI))
			attr = randint1(BASIC_COLORS - 1);
		else if (rf_has(mon->race->flags, RF_ATTR_FLICKER)) {
			uint8_t base_attr = monster_x_attr[mon->race->ridx];

			/* Obtener el atributo de color cíclico, si está disponible. */
			attr = visuals_cycler_get_attr_for_race(mon->race, flicker);

			if (attr == BASIC_COLORS) {
				/* Recurrir al atributo de parpadeo. */
				attr = visuals_flicker_get_attr_for_frame(base_attr, flicker);
			}

			if (attr == BASIC_COLORS) {
				/* Recurrir al atributo estático si falla el ciclo. */
				attr = base_attr;
			}
		}
		else
			continue;

		mon->attr = attr;
		player->upkeep->redraw |= (PR_MAP | PR_MONLIST);
	}

	flicker++;
}

/**
 * Permitir animaciones
 */
void allow_animations(void)
{
	animations_allowed = true;
}

/**
 * Deshabilitar animaciones
 */
void disallow_animations(void)
{
	animations_allowed = false;
}

/**
 * Actualizar animaciones a petición
 */
static void animate(game_event_type type, game_event_data *data, void *user)
{
	do_animation();
}

/**
 * Esto se usa cuando el usuario está inactivo para permitir animaciones simples.
 * Actualmente lo único que realmente hace es animar monstruos brillantes.
 */
void idle_update(void)
{
	if (!animations_allowed) return;
	if (msg_flag) return;
	if (!character_dungeon) return;
	if (!OPT(player, animate_flicker) || (use_graphics != GRAPHICS_NONE))
		return;

	/* Animar y redibujar si es necesario */
	do_animation();
	redraw_stuff(player);

	/* Refrescar la pantalla principal */
	Term_fresh();
}


/**
 * Encontrar el par atributo/carácter a usar para un efecto de hechizo
 *
 * Se está moviendo (o se ha movido) desde (x, y) a (nx, ny); si la distancia no es
 * "uno", podemos devolver "*".
 */
static void bolt_pict(int y, int x, int ny, int nx, int typ, uint8_t *a,
					  wchar_t *c)
{
	int motion;

	/* Convertir coordenadas en movimiento */
	if ((ny == y) && (nx == x))
		motion = BOLT_NO_MOTION;
	else if (nx == x)
		motion = BOLT_0;
	else if ((ny-y) == (x-nx))
		motion = BOLT_45;
	else if (ny == y)
		motion = BOLT_90;
	else if ((ny-y) == (nx-x))
		motion = BOLT_135;
	else
		motion = BOLT_NO_MOTION;

	/* Decidir el carácter de salida */
	if (use_graphics == GRAPHICS_NONE) {
		/* ASCII es simple */
		wchar_t chars[] = L"*|/-\\";

		*c = chars[motion];
		*a = projections[typ].color;
	} else {
		*a = proj_to_attr[typ][motion];
		*c = proj_to_char[typ][motion];
	}
}

/**
 * Dibujar una explosión
 */
static void display_explosion(game_event_type type, game_event_data *data,
							  void *user)
{
	bool new_radius = false;
	bool drawn = false;
	int i, y, x;
	int msec = player->opts.delay_factor;
	int proj_type = data->explosion.proj_type;
	int num_grids = data->explosion.num_grids;
	int *distance_to_grid = data->explosion.distance_to_grid;
	bool drawing = data->explosion.drawing;
	bool *player_sees_grid = data->explosion.player_sees_grid;
	struct loc *blast_grid = data->explosion.blast_grid;
	struct loc centre = data->explosion.centre;

	/* Dibujar la explosión de adentro hacia afuera */
	for (i = 0; i < num_grids; i++) {
		/* Extraer la ubicación */
		y = blast_grid[i].y;
		x = blast_grid[i].x;

		/* Solo hacer efectos visuales si el jugador puede ver la explosión */
		if (player_sees_grid[i]) {
			uint8_t a;
			wchar_t c;

			drawn = true;

			/* Obtener la imagen de la explosión */
			bolt_pict(y, x, y, x, proj_type, &a, &c);

			/* Solo mostrar la imagen, ignorando lo que había debajo */
			print_rel(c, a, y, x);
		}

		/* Centrar el cursor para evitar que siga las casillas de la explosión */
		move_cursor_relative(centre.y, centre.x);

		/* Verificar nuevo radio, teniendo cuidado de no sobrepasar la matriz */
		if (i == num_grids - 1)
			new_radius = true;
		else if (distance_to_grid[i + 1] > distance_to_grid[i])
			new_radius = true;

		/* Tenemos todas las casillas en el radio actual, así que dibujarlo */
		if (new_radius) {
			/* Vaciar todas las casillas en este radio */
			Term_fresh();
			if (player->upkeep->redraw)
				redraw_stuff(player);

			/* Demora para mostrar este radio apareciendo */
			if (drawn || drawing) {
				Term_xtra(TERM_XTRA_DELAY, msec);
			}

			new_radius = false;
		}
	}

	/* Borrar y vaciar */
	if (drawn) {
		/* Borrar la explosión dibujada arriba */
		for (i = 0; i < num_grids; i++) {
			/* Extraer la ubicación */
			y = blast_grid[i].y;
			x = blast_grid[i].x;

			/* Borrar casillas visibles y válidas */
			if (player_sees_grid[i])
				event_signal_point(EVENT_MAP, x, y);
		}

		/* Centrar el cursor */
		move_cursor_relative(centre.y, centre.x);

		/* Vaciar la explosión */
		Term_fresh();
		if (player->upkeep->redraw)
			redraw_stuff(player);
	}
}

/**
 * Dibujar un efecto de hechizo en movimiento (proyectil o haz)
 */
static void display_bolt(game_event_type type, game_event_data *data,
						 void *user)
{
	int msec = player->opts.delay_factor;
	int proj_type = data->bolt.proj_type;
	bool drawing = data->bolt.drawing;
	bool seen = data->bolt.seen;
	bool beam = data->bolt.beam;
	int oy = data->bolt.oy;
	int ox = data->bolt.ox;
	int y = data->bolt.y;
	int x = data->bolt.x;

	/* Solo hacer efectos visuales si el jugador puede "ver" el proyectil */
	if (seen) {
		uint8_t a;
		wchar_t c;

		/* Obtener la imagen del proyectil */
		bolt_pict(oy, ox, y, x, proj_type, &a, &c);

		/* Efectos visuales */
		print_rel(c, a, y, x);
		move_cursor_relative(y, x);
		Term_fresh();
		if (player->upkeep->redraw)
			redraw_stuff(player);
		Term_xtra(TERM_XTRA_DELAY, msec);
		event_signal_point(EVENT_MAP, x, y);
		Term_fresh();
		if (player->upkeep->redraw)
			redraw_stuff(player);

		/* Mostrar casillas de "haz" */
		if (beam) {

			/* Obtener la imagen de la explosión */
			bolt_pict(y, x, y, x, proj_type, &a, &c);

			/* Efectos visuales */
			print_rel(c, a, y, x);
		}
	} else if (drawing) {
		/* Demora para mantener la consistencia */
		Term_xtra(TERM_XTRA_DELAY, msec);
	}
}

/**
 * Dibujar un proyectil en movimiento
 */
static void display_missile(game_event_type type, game_event_data *data,
							void *user)
{
	int msec = player->opts.delay_factor;
	struct object *obj = data->missile.obj;
	bool seen = data->missile.seen;
	int y = data->missile.y;
	int x = data->missile.x;

	/* Solo hacer efectos visuales si el jugador puede "ver" el proyectil */
	if (seen) {
		print_rel(object_char(obj), object_attr(obj), y, x);
		move_cursor_relative(y, x);

		Term_fresh();
		if (player->upkeep->redraw) redraw_stuff(player);

		Term_xtra(TERM_XTRA_DELAY, msec);
		event_signal_point(EVENT_MAP, x, y);

		Term_fresh();
		if (player->upkeep->redraw) redraw_stuff(player);
	}
}

/**
 * ------------------------------------------------------------------------
 * Visualizaciones de subventanas
 * ------------------------------------------------------------------------ */

/**
 * true cuando se supone que debemos mostrar el equipo en la ventana de inventario,
 * o viceversa.
 */
static bool flip_inven;

static void update_inven_subwindow(game_event_type type, game_event_data *data,
				       void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	if (!flip_inven)
		show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
	else
		show_equip(OLIST_WINDOW | OLIST_WEIGHT, NULL);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

static void update_equip_subwindow(game_event_type type, game_event_data *data,
				   void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	if (!flip_inven)
		show_equip(OLIST_WINDOW | OLIST_WEIGHT, NULL);
	else
		show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

/**
 * Invertir "inventario" y "equipo" en cualquier subventana
 */
void toggle_inven_equip(void)
{
	term *old = Term;
	int i;

	/* Cambiar la configuración real */
	flip_inven = !flip_inven;

	/* Redibujar cualquier subventana que muestre las listas de inventario/equipo */
	for (i = 0; i < ANGBAND_TERM_MAX; i++) {
		/* Omitir subventanas no utilizadas. */
		if (!angband_term[i]) continue;

		Term_activate(angband_term[i]); 

		if (window_flag[i] & PW_INVEN) {
			if (!flip_inven)
				show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
			else
				show_equip(OLIST_WINDOW | OLIST_WEIGHT, NULL);
			
			Term_fresh();
		} else if (window_flag[i] & PW_EQUIP) {
			if (!flip_inven)
				show_equip(OLIST_WINDOW | OLIST_WEIGHT, NULL);
			else
				show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
			
			Term_fresh();
		}
	}

	Term_activate(old);
}

static void update_itemlist_subwindow(game_event_type type,
									  game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

    clear_from(0);
    object_list_show_subwindow(Term->hgt, Term->wid);
	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

static void update_monlist_subwindow(game_event_type type,
									 game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	clear_from(0);
	monster_list_show_subwindow(Term->hgt, Term->wid);
	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}


static void update_monster_subwindow(game_event_type type,
									 game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	/* Mostrar información de raza de monstruo */
	if (player->upkeep->monster_race)
		lore_show_subwindow(player->upkeep->monster_race, 
							get_lore(player->upkeep->monster_race));

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}


static void update_object_subwindow(game_event_type type,
									game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;
	
	/* Activar */
	Term_activate(inv_term);
	
	if (player->upkeep->object != NULL)
		display_object_recall(player->upkeep->object);
	else if (player->upkeep->object_kind)
		display_object_kind_recall(player->upkeep->object_kind);
	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}


static void update_messages_subwindow(game_event_type type,
									  game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	int i;
	int w, h;
	int x, y;
	bool is_fresh = true;
	static const char* prev_last_msg = NULL;

	const char *msg;

	/* Activar */
	Term_activate(inv_term);

	/* Obtener tamaño */
	Term_get_size(&w, &h);

	/* Volcar mensajes */
	const char* last_msg = NULL;
	for (i = 0; i < h; i++) {
		uint16_t count = message_count(i);
		const char *str = message_str(i);
		if (is_fresh && prev_last_msg == str) {
			is_fresh = false;
		}
		uint8_t color = is_fresh? COLOUR_RED: message_color(i);

		if (count == 1)
			msg = str;
		else if (count == 0)
			msg = " ";
		else {
			msg = format("%s <%dx>", str, count);
		}

		Term_putstr(0, (h - 1) - i, -1, color, msg);


		/* Cursor */
		Term_locate(&x, &y);

		/* Limpiar hasta el final de la línea */
		Term_erase(x, y, 255);
		if (i == 0){
			last_msg = str;
		}
	}
	prev_last_msg = last_msg;

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

static struct minimap_flags
{
	int win_idx;
	bool needs_redraw;
} minimap_data[ANGBAND_TERM_MAX];

static void update_minimap_subwindow(game_event_type type,
	game_event_data *data, void *user)
{
	struct minimap_flags *flags = user;

	if (player_resting_count(player) || player->upkeep->running) return;

	if (type == EVENT_END) {
		term *old = Term;
		term *t = angband_term[flags->win_idx];

		/* Activar */
		Term_activate(t);

		/* Si es redibujado de mapa completo, limpiar ventana primero. */
		if (flags->needs_redraw)
			Term_clear();

		/* Redibujar mapa */
		display_map(NULL, NULL);
		Term_fresh();

		/* Restaurar */
		Term_activate(old);

		flags->needs_redraw = false;
	} else if (type == EVENT_DUNGEONLEVEL) {
		/* XXX map_height y map_width deben mantenerse sincronizados con
		 * display_map() */
		term *t = angband_term[flags->win_idx];
		int map_height = t->hgt - 2;
		int map_width = t->wid - 2;

		/* Limpiar todo el terminal si el nuevo mapa no va a caber en su totalidad */
		if (cave->height <= map_height || cave->width <= map_width) {
			flags->needs_redraw = true;
		}
	}
}


/**
 * Mostrar jugador en subventanas (modo 0)
 */
static void update_player0_subwindow(game_event_type type,
									 game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	/* Mostrar banderas */
	display_player(0);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

/**
 * Mostrar jugador en subventanas (modo 1)
 */
static void update_player1_subwindow(game_event_type type,
									 game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	/* Mostrar banderas */
	display_player(1);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

static void update_topbar_subwindow(game_event_type type,
									game_event_data *data, void *user)
{
	term *old = Term;
	term *inv_term = user;

	/* Verificar cordura */
	if (!(player && player->race && player->class && cave)) return;

	/* Activar */
	Term_activate(inv_term);

	update_topbar(type, data, user, 0);

	update_statusline_aux(2, 0);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

/**
 * Mostrar el lado izquierdo del terminal principal, de forma más compacta.
 */
static void update_player_compact_subwindow(game_event_type type,
											game_event_data *data, void *user)
{
	int row = 0;
	int col = 0;
	int i;

	term *old = Term;
	term *inv_term = user;

	/* Activar */
	Term_activate(inv_term);

	/* Raza y Clase */
	prt_field(player->race->name, row++, col);
	prt_field(player->class->name, row++, col);

	/* Título */
	prt_title(row++, col);

	/* Nivel/Experiencia */
	prt_level(row++, col);
	prt_exp(row++, col);

	/* Oro */
	prt_gold(row++, col);

	/* Caracteres "equippy" */
	prt_equippy(row++, col);

	/* Todas las estadísticas */
	for (i = 0; i < STAT_MAX; i++) prt_stat(i, row++, col);

	/* Fila vacía */
	row++;

	/* Armadura */
	prt_ac(row++, col);

	/* Puntos de golpe */
	prt_hp(row++, col);

	/* Puntos de hechizo */
	prt_sp(row++, col);

	/* Salud del monstruo */
	prt_health(row, col);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}


static void flush_subwindow(game_event_type type, game_event_data *data,
							void *user)
{
	term *old = Term;
	term *t = user;

	/* Activar */
	Term_activate(t);

	Term_fresh();
	
	/* Restaurar */
	Term_activate(old);
}

/**
 * Ciertas "pantallas" siempre usan la pantalla principal, incluyendo Noticias, Nacimiento,
 * Mazmorra, Lápida, Puntuaciones Altas, Macros, Colores, Visuales, Opciones.
 *
 * Más tarde, banderas especiales pueden permitir que las subventanas "roben" cosas de la
 * ventana principal, incluyendo Volcado de archivo (ayuda), Volcado de archivo (artefactos, únicos),
 * Pantalla de personaje, Mapa a pequeña escala, Mensajes anteriores, Pantalla de tienda, etc.
 */
const char *window_flag_desc[32] =
{
	"Mostrar inv/equip",
	"Mostrar equip/inv",
	"Mostrar jugador (básico)",
	"Mostrar jugador (extra)",
	"Mostrar jugador (compacto)",
	"Mostrar vista de mapa",
	"Mostrar mensajes",
	"Mostrar vista general",
	"Mostrar recuerdo de monstruo",
	"Mostrar recuerdo de objeto",
	"Mostrar lista de monstruos",
	"Mostrar estado",
	"Mostrar lista de objetos",
	"Mostrar jugador (barra superior)",
#ifdef ALLOW_BORG
	"Mostrar mensajes de borg",
	"Mostrar estado de borg",
#else
	NULL,
	NULL,
#endif
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void subwindow_flag_changed(int win_idx, uint32_t flag, bool new_state)
{
	void (*register_or_deregister)(game_event_type type, game_event_handler *fn,
								   void *user);
	void (*set_register_or_deregister)(game_event_type *type, size_t n_events,
									   game_event_handler *fn, void *user);

	/* Decidir si registrar o cancelar el registro de un manejador de eventos */
	if (new_state == false) {
		register_or_deregister = event_remove_handler;
		set_register_or_deregister = event_remove_handler_set;
	} else {
		register_or_deregister = event_add_handler;
		set_register_or_deregister = event_add_handler_set;
	}

	switch (flag)
	{
		case PW_INVEN:
		{
			register_or_deregister(EVENT_INVENTORY,
					       update_inven_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_EQUIP:
		{
			register_or_deregister(EVENT_EQUIPMENT,
					       update_equip_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_PLAYER_0:
		{
			set_register_or_deregister(player_events, 
						   N_ELEMENTS(player_events),
						   update_player0_subwindow,
						   angband_term[win_idx]);
			break;
		}

		case PW_PLAYER_1:
		{
			set_register_or_deregister(player_events, 
						   N_ELEMENTS(player_events),
						   update_player1_subwindow,
						   angband_term[win_idx]);
			break;
		}

		case PW_PLAYER_2:
		{
			set_register_or_deregister(player_events, 
						   N_ELEMENTS(player_events),
						   update_player_compact_subwindow,						   
						   angband_term[win_idx]);
			break;
		}

		case PW_PLAYER_3:
		{
			/* Barra superior */
			set_register_or_deregister(player_events, 
						   N_ELEMENTS(player_events),						 
						   update_topbar_subwindow,
						   angband_term[win_idx]);

			/* También actualizar estado */
			set_register_or_deregister(statusline_events,
						   N_ELEMENTS(statusline_events),
						   update_topbar_subwindow,
						   angband_term[win_idx]);

			break;
		}

		case PW_MAP:
		{
			minimap_data[win_idx].win_idx = win_idx;

			register_or_deregister(EVENT_MAP,
					       update_minimap_subwindow,
					       &minimap_data[win_idx]);

			register_or_deregister(EVENT_DUNGEONLEVEL, update_minimap_subwindow,
								   &minimap_data[win_idx]);

			register_or_deregister(EVENT_END,
					       update_minimap_subwindow,
					       &minimap_data[win_idx]);
			break;
		}

		case PW_MESSAGE:
		{
			register_or_deregister(EVENT_STATE,
					       update_messages_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_OVERHEAD:
		{
			register_or_deregister(EVENT_MAP,
					       update_maps,
					       angband_term[win_idx]);

			register_or_deregister(EVENT_END,
					       flush_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_MONSTER:
		{
			register_or_deregister(EVENT_MONSTERTARGET,
					       update_monster_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_OBJECT:
		{
			register_or_deregister(EVENT_OBJECTTARGET,
						   update_object_subwindow,
						   angband_term[win_idx]);
			break;
		}

		case PW_MONLIST:
		{
			register_or_deregister(EVENT_MONSTERLIST,
					       update_monlist_subwindow,
					       angband_term[win_idx]);
			break;
		}

		case PW_ITEMLIST:
		{
			register_or_deregister(EVENT_ITEMLIST,
						   update_itemlist_subwindow,
						   angband_term[win_idx]);
			break;
		}
	}
}


/**
 * Establecer las banderas para un Terminal, llamando a "subwindow_flag_changed" con cada bandera
 * que ha cambiado de configuración para que pueda hacer cualquier tarea de mantenimiento relacionada con
 * mostrar lo nuevo o ya no mostrar lo antiguo.
 */
static void subwindow_set_flags(int win_idx, uint32_t new_flags)
{
	term *old = Term;
	int i;

	/* Lidiar con las banderas cambiadas viendo qué ha cambiado */
	for (i = 0; i < 32; i++)
		/* Solo procesar banderas válidas */
		if (window_flag_desc[i]) {
			uint32_t flag = ((uint32_t) 1) << i;

			if ((new_flags & flag) !=
					(window_flag[win_idx] & flag)) {
				subwindow_flag_changed(win_idx, flag,
						(new_flags & flag) != 0);
			}
		}

	/* Almacenar las nuevas banderas */
	window_flag[win_idx] = new_flags;
	
	/* Activar */
	Term_activate(angband_term[win_idx]);
	
	/* Borrar */
	Term_clear();
	
	/* Refrescar */
	Term_fresh();
			
	/* Restaurar */
	Term_activate(old);
}

/**
 * Llamado con una matriz de las nuevas banderas para todas las subventanas, en orden
 * para establecerlas a los nuevos valores, con la oportunidad de realizar tareas de mantenimiento.
 */
void subwindows_set_flags(uint32_t *new_flags, size_t n_subwindows)
{
	size_t j;

	for (j = 0; j < n_subwindows; j++) {
		/* Ventana muerta */
		if (!angband_term[j]) continue;

		/* Ignorar no cambios */
		if (window_flag[j] != new_flags[j])
			subwindow_set_flags(j, new_flags[j]);
	}
}

/**
 * ------------------------------------------------------------------------
 * Mostrar y actualizar la pantalla de presentación.
 * ------------------------------------------------------------------------ */
/**
 * Explicar una carpeta "lib" rota y salir (ver abajo).
 */
static void init_angband_aux(const char *why)
{
	quit_fmt("%s\n\n%s", why,
	         "El directorio 'lib' probablemente falta o está dañado.\n"
	         "Quizás el archivo no se extrajo correctamente.\n"
	         "Consulta el archivo 'readme.txt' para más información.");
}

/*
 * Tomar notas en la línea 23
 */
static void splashscreen_note(game_event_type type, game_event_data *data,
							  void *user)
{
	if (data->message.type == MSG_BIRTH) {
		static int y = 2;

		/* Dibujar el mensaje */
		prt(data->message.msg, y, 0);
		pause_line(Term);

		/* Avanzar una línea (envolver si es necesario) */
		if (++y >= 24) y = 2;
	} else {
		char *s = format("[%s]", data->message.msg);
		Term_erase(0, (Term->hgt - 23) / 5 + 23, 255);
		Term_putstr((Term->wid - strlen(s)) / 2, (Term->hgt - 23) / 5 + 23, -1,
					COLOUR_WHITE, s);
	}

	Term_fresh();
}

static void show_splashscreen(game_event_type type, game_event_data *data,
							  void *user)
{
	ang_file *fp;

	char buf[1024];

	/* Verificar el archivo "news" */
	path_build(buf, sizeof(buf), ANGBAND_DIR_SCREENS, "news.txt");
	if (!file_exists(buf)) {
		char why[1024];

		/* Chocar y arder */
		strnfmt(why, sizeof(why), "¡No se puede acceder al archivo '%s'!", buf);
		init_angband_aux(why);
	}


	/* Prepararse para mostrar el archivo "news" */
	Term_clear();

	/* Abrir el archivo de Noticias */
	path_build(buf, sizeof(buf), ANGBAND_DIR_SCREENS, "news.txt");
	fp = file_open(buf, MODE_READ, FTYPE_TEXT);

	text_out_hook = text_out_to_screen;

	/* Volcar */
	if (fp) {
		/* Centrar la pantalla de presentación - asumir que news.txt tiene 80 de ancho, 23 de alto */
		text_out_indent = (Term->wid - 80) / 2;
		Term_gotoxy(0, (Term->hgt - 23) / 5);

		/* Volcar el archivo a la pantalla */
		while (file_getl(fp, buf, sizeof(buf))) {
			char *version_marker = strstr(buf, "$VERSION");
			if (version_marker) {
				ptrdiff_t pos = version_marker - buf;
				strnfmt(version_marker, sizeof(buf) - pos, "%-8s", buildver);
			}

			text_out_e("%s", buf);
			text_out("\n");
		}

		text_out_indent = 0;
		file_close(fp);
	}

	/* Vaciar */
	Term_fresh();
}


/**
 * ------------------------------------------------------------------------
 * Actualizaciones visuales entre turnos de jugador.
 * ------------------------------------------------------------------------ */
static void refresh(game_event_type type, game_event_data *data, void *user)
{
	/* Colocar cursor sobre jugador/objetivo */
	if (OPT(player, show_target) && target_sighted()) {
		struct loc target;
		target_get(&target);
		move_cursor_relative(target.y, target.x);
	}

	Term_fresh();
}

static void repeated_command_display(game_event_type type,
									 game_event_data *data, void *user)
{
	/* Asumir que los mensajes fueron vistos */
	msg_flag = false;

	/* Limpiar la línea superior */
	prt("", 0, 0);
}

/**
 * Tareas de mantenimiento al llegar a un nuevo nivel
 */
static void new_level_display_update(game_event_type type,
									 game_event_data *data, void *user)
{
	/* Forzar panel ilegal */
	Term->offset_y = z_info->dungeon_hgt;
	Term->offset_x = z_info->dungeon_wid;

	/* Elegir panel */
	verify_panel();

	/* Limpiar */
	Term_clear();

	/* Invocar modo de actualización parcial */
	player->upkeep->only_partial = true;

	/* Actualizar cosas */
	player->upkeep->update |= (PU_BONUS | PU_HP | PU_SPELLS);

	/* Calcular radio de la antorcha */
	player->upkeep->update |= (PU_TORCH);

	/* Actualizar completamente los visuales (y distancias de monstruos) */
	player->upkeep->update |= (PU_UPDATE_VIEW | PU_DISTANCE);

	/* Redibujar mazmorra */
	player->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP);

	/* Redibujar cosas de "estado" */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_MONSTER | PR_MONLIST | PR_ITEMLIST);

	/* Porque cambiar de nivel no gasta un turno y PR_MONLIST podría no
	 * establecerse durante algunos turnos de juego, forzar manualmente una actualización al cambiar de nivel. */
	monster_list_force_subwindow_update();

	/* Si el autoguardado está pendiente, hacerlo ahora. */
	if (player->upkeep->autosave) {
		save_game();
		player->upkeep->autosave = false;
	}

	/*
	 * Guardar tiene el efecto secundario de llamar a handle_stuff(), pero si no
	 * guardamos o guardar ya no llama a handle_stuff(), llamar a
	 * handle_stuff() ahora para procesar las actualizaciones y redibujados pendientes.
	 */
	handle_stuff(player);

	/* Matar modo de actualización parcial */
	player->upkeep->only_partial = false;

	/* Refrescar */
	Term_fresh();
}


/**
 * ------------------------------------------------------------------------
 * Soluciones temporales (con suerte) poco elegantes.
 * ------------------------------------------------------------------------ */
static void cheat_death(game_event_type type, game_event_data *data, void *user)
{
	msg("Invitas al modo mago y burlas a la muerte.");
	event_signal(EVENT_MESSAGE_FLUSH);

	wiz_cheat_death();
}

static void check_panel(game_event_type type, game_event_data *data, void *user)
{
	verify_panel();
}

static void see_floor_items(game_event_type type, game_event_data *data,
							void *user)
{
	int floor_max = z_info->floor_size;
	struct object **floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
	int floor_num = 0;
	bool blind = ((player->timed[TMD_BLIND]) || (no_light(player)));

	const char *p = "ves";
	bool can_pickup = false;
	int i;

	/* Escanear todos los objetos visibles y detectados en la casilla */
	floor_num = scan_floor(floor_list, floor_max, player,
		OFLOOR_SENSE | OFLOOR_VISIBLE, NULL);
	if (floor_num == 0) {
		mem_free(floor_list);
		return;
	}

	/* ¿Podemos recoger alguno? */
	for (i = 0; i < floor_num; i++)
	    if (inven_carry_okay(floor_list[i]))
			can_pickup = true;

	/* Un objeto */
	if (floor_num == 1) {
		/* Obtener el objeto */
		struct object *obj = floor_list[0];
		char o_name[80];

		if (!can_pickup)
			p = "no tienes espacio para";
		else if (blind)
			p = "sientes";

		/* Describir el objeto. Menos detalle si está ciego. */
		if (blind) {
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_BASE, player);
		} else {
			object_desc(o_name, sizeof(o_name), obj,
				ODESC_PREFIX | ODESC_FULL, player);
		}

		/* Mensaje */
		event_signal(EVENT_MESSAGE_FLUSH);
		msg("%s %s %s.", (can_pickup && !blind) ? "Ves" : p, p, o_name);
	} else {
		ui_event e;

		if (!can_pickup)
			p = "no tienes espacio para los siguientes objetos";
		else if (blind)
			p = "sientes algo en el suelo";

		/* Mostrar objetos en el suelo */
		screen_save();
		show_floor(floor_list, floor_num, OLIST_WEIGHT, NULL);
		prt(format("Tú %s: ", p), 0, 0);

		/* Esperar. Usar tecla como siguiente comando. */
		e = inkey_ex();
		Term_event_push(&e);

		/* Restaurar pantalla */
		screen_load();
	}

	mem_free(floor_list);
}

/**
 * ------------------------------------------------------------------------
 * Inicialización
 * ------------------------------------------------------------------------ */

/**
 * Procesar los archivos de preferencias de usuario relevantes para un personaje recién cargado
 */
static void process_character_pref_files(void)
{
	bool found;
	char buf[1024];

	/* Procesar el archivo "window.prf" */
	process_pref_file("window.prf", true, true);

	/* Procesar el archivo "user.prf" */
	process_pref_file("user.prf", true, true);

	/* Obtener el nombre seguro para el sistema de archivos y añadir .prf */
	player_safe_name(buf, sizeof(buf), player->full_name, true);
	my_strcat(buf, ".prf", sizeof(buf));

	found = process_pref_file(buf, true, true);

    /* Intentar archivo de preferencias usando el nombre del archivo guardado si fallamos usando el nombre del personaje */
    if (!found) {
		int filename_index = path_filename_index(savefile);
		char filename[128];

		my_strcpy(filename, &savefile[filename_index], sizeof(filename));
		strnfmt(buf, sizeof(buf), "%s.prf", filename);
		process_pref_file(buf, true, true);
    }
}


static void ui_enter_init(game_event_type type, game_event_data *data,
						  void *user)
{
	show_splashscreen(type, data, user);

	/* Configurar nuestros manejadores de pantalla de presentación */
	event_add_handler(EVENT_INITSTATUS, splashscreen_note, NULL);
}

static void ui_leave_init(game_event_type type, game_event_data *data,
						  void *user)
{
	/* Reiniciar visuales, luego cargar preferencias y reaccionar a cambios */
	reset_visuals(true);
	process_character_pref_files();
	Term_xtra(TERM_XTRA_REACT, 0);
	(void) Term_redraw_all();

	/* Eliminar nuestros manejadores de pantalla de presentación */
	event_remove_handler(EVENT_INITSTATUS, splashscreen_note, NULL);

	/* Mostrar un mensaje */
	prt("Espera por favor...", 0, 0);

	/* Vaciar el mensaje */
	Term_fresh();
}

static void ui_enter_world(game_event_type type, game_event_data *data,
						  void *user)
{
	/* Permitir cursor grande */
	smlcurs = false;

	/* Redibujar cosas */
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_MONSTER | PR_MESSAGE);
	redraw_stuff(player);

	/* Debido a la barra lateral "flexible", todas estas cosas activan
	   la misma función. */
	event_add_handler_set(player_events, N_ELEMENTS(player_events),
			      update_sidebar, NULL);

	/* La barra de estado flexible tiene requisitos similares, por lo que
	   también es activada por un gran conjunto de eventos. */
	event_add_handler_set(statusline_events, N_ELEMENTS(statusline_events),
			      update_statusline, NULL);

	/* Los PG del jugador pueden opcionalmente cambiar el color del '@' ahora. */
	event_add_handler(EVENT_HP, hp_colour_change, NULL);

	/* La forma más simple de mantener el mapa actualizado - servirá por ahora */
	event_add_handler(EVENT_MAP, update_maps, angband_term[0]);
#ifdef MAP_DEBUG
	event_add_handler(EVENT_MAP, trace_map_updates, angband_term[0]);
#endif

	/* Verificar si el panel debería desplazarse cuando el jugador se mueve */
	event_add_handler(EVENT_PLAYERMOVED, check_panel, NULL);

	/* Tomar nota de lo que hay en el suelo */
	event_add_handler(EVENT_SEEFLOOR, see_floor_items, NULL);

	/* Entrar a una tienda */
	event_add_handler(EVENT_ENTER_STORE, enter_store, NULL);

	/* Mostrar una explosión */
	event_add_handler(EVENT_EXPLOSION, display_explosion, NULL);

	/* Mostrar un hechizo de proyectil */
	event_add_handler(EVENT_BOLT, display_bolt, NULL);

	/* Mostrar un proyectil físico */
	event_add_handler(EVENT_MISSILE, display_missile, NULL);

	/* Verificar si el jugador ha intentado cancelar el procesamiento del juego */
	event_add_handler(EVENT_CHECK_INTERRUPT, check_for_player_interrupt, NULL);

	/* Refrescar la pantalla y colocar el cursor en el lugar apropiado */
	event_add_handler(EVENT_REFRESH, refresh, NULL);

	/* Hacer las actualizaciones visuales requeridas en un nuevo nivel de mazmorra */
	event_add_handler(EVENT_NEW_LEVEL_DISPLAY, new_level_display_update, NULL);

	/* Limpiar mensajes automáticamente mientras el juego repite comandos */
	event_add_handler(EVENT_COMMAND_REPEAT, repeated_command_display, NULL);

	/* Hacer animaciones (ej. cambios de color de monstruos) */
	event_add_handler(EVENT_ANIMATE, animate, NULL);

	/* Permitir al jugador burlar a la muerte, si corresponde */
	event_add_handler(EVENT_CHEAT_DEATH, cheat_death, NULL);

	/* Disminuir la profundidad "icky" */
	screen_save_depth--;
}

static void ui_leave_world(game_event_type type, game_event_data *data,
						  void *user)
{
	/* Deshabilitar cursor grande */
	smlcurs = true;

	/* Debido a la barra lateral "flexible", todas estas cosas activaban
	   la misma función. */
	event_remove_handler_set(player_events, N_ELEMENTS(player_events),
			      update_sidebar, NULL);

	/* La barra de estado flexible tenía requisitos similares, por lo que
	   también era activada por un gran conjunto de eventos. */
	event_remove_handler_set(statusline_events, N_ELEMENTS(statusline_events),
			      update_statusline, NULL);

	/* Los PG del jugador podían opcionalmente cambiar el color del '@'. */
	event_remove_handler(EVENT_HP, hp_colour_change, NULL);

	/* La forma más simple de mantener el mapa actualizado - servirá por ahora */
	event_remove_handler(EVENT_MAP, update_maps, angband_term[0]);
#ifdef MAP_DEBUG
	event_remove_handler(EVENT_MAP, trace_map_updates, angband_term[0]);
#endif

	/* Verificar si el panel debería desplazarse cuando el jugador se mueve */
	event_remove_handler(EVENT_PLAYERMOVED, check_panel, NULL);

	/* Tomar nota de lo que hay en el suelo */
	event_remove_handler(EVENT_SEEFLOOR, see_floor_items, NULL);

	/* Mostrar una explosión */
	event_remove_handler(EVENT_EXPLOSION, display_explosion, NULL);

	/* Mostrar un hechizo de proyectil */
	event_remove_handler(EVENT_BOLT, display_bolt, NULL);

	/* Mostrar un proyectil físico */
	event_remove_handler(EVENT_MISSILE, display_missile, NULL);

	/* Verificar si el jugador ha intentado cancelar el procesamiento del juego */
	event_remove_handler(EVENT_CHECK_INTERRUPT, check_for_player_interrupt, NULL);

	/* Refrescar la pantalla y colocar el cursor en el lugar apropiado */
	event_remove_handler(EVENT_REFRESH, refresh, NULL);

	/* Hacer las actualizaciones visuales requeridas en un nuevo nivel de mazmorra */
	event_remove_handler(EVENT_NEW_LEVEL_DISPLAY, new_level_display_update, NULL);

	/* Limpiar mensajes automáticamente mientras el juego repite comandos */
	event_remove_handler(EVENT_COMMAND_REPEAT, repeated_command_display, NULL);

	/* Hacer animaciones (ej. cambios de color de monstruos) */
	event_remove_handler(EVENT_ANIMATE, animate, NULL);

	/* Permitir al jugador burlar a la muerte, si corresponde */
	event_remove_handler(EVENT_CHEAT_DEATH, cheat_death, NULL);

	/* Prepararse para interactuar con una tienda */
	event_add_handler(EVENT_USE_STORE, use_store, NULL);

	/* Si hemos entrado en una tienda, necesitamos saber cómo salir */
	event_add_handler(EVENT_LEAVE_STORE, leave_store, NULL);

	/* Aumentar la profundidad "icky" */
	screen_save_depth++;
}

static void ui_enter_game(game_event_type type, game_event_data *data,
						  void *user)
{
	/* Mostrar un mensaje al jugador */
	event_add_handler(EVENT_MESSAGE, display_message, NULL);

	/* Mostrar un mensaje y hacer ruido al jugador */
	event_add_handler(EVENT_BELL, bell_message, NULL);

	/* Decir a la UI que ignore toda la entrada pendiente */
	event_add_handler(EVENT_INPUT_FLUSH, flush, NULL);

	/* Imprimir todos los mensajes en espera */
	event_add_handler(EVENT_MESSAGE_FLUSH, message_flush, NULL);
}

static void ui_leave_game(game_event_type type, game_event_data *data,
						  void *user)
{
	/* Mostrar un mensaje al jugador */
	event_remove_handler(EVENT_MESSAGE, display_message, NULL);

	/* Mostrar un mensaje y hacer ruido al jugador */
	event_remove_handler(EVENT_BELL, bell_message, NULL);

	/* Decir a la UI que ignore toda la entrada pendiente */
	event_remove_handler(EVENT_INPUT_FLUSH, flush, NULL);

	/* Imprimir todos los mensajes en espera */
	event_remove_handler(EVENT_MESSAGE_FLUSH, message_flush, NULL);
}

void init_display(void)
{
	event_add_handler(EVENT_ENTER_INIT, ui_enter_init, NULL);
	event_add_handler(EVENT_LEAVE_INIT, ui_leave_init, NULL);

	event_add_handler(EVENT_ENTER_GAME, ui_enter_game, NULL);
	event_add_handler(EVENT_LEAVE_GAME, ui_leave_game, NULL);

	event_add_handler(EVENT_ENTER_WORLD, ui_enter_world, NULL);
	event_add_handler(EVENT_LEAVE_WORLD, ui_leave_world, NULL);

	ui_init_birthstate_handlers();
}