/**
 * \file player-calcs.c
 * \brief Cálculo del estado del jugador, señalando eventos de la interfaz de usuario
 *	basados en cambios de estado.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2014 Nick McConnell
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
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "init.h"
#include "mon-msg.h"
#include "mon-util.h"
#include "obj-curse.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"

/**
 * Tabla de Estadísticas (INT) -- Dispositivos mágicos
 */
static const int adj_int_dev[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	1	/* 8 */,
	1	/* 9 */,
	1	/* 10 */,
	1	/* 11 */,
	1	/* 12 */,
	1	/* 13 */,
	1	/* 14 */,
	2	/* 15 */,
	2	/* 16 */,
	2	/* 17 */,
	3	/* 18/00-18/09 */,
	3	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	3	/* 18/30-18/39 */,
	3	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	4	/* 18/60-18/69 */,
	5	/* 18/70-18/79 */,
	5	/* 18/80-18/89 */,
	6	/* 18/90-18/99 */,
	6	/* 18/100-18/109 */,
	7	/* 18/110-18/119 */,
	7	/* 18/120-18/129 */,
	8	/* 18/130-18/139 */,
	8	/* 18/140-18/149 */,
	9	/* 18/150-18/159 */,
	9	/* 18/160-18/169 */,
	10	/* 18/170-18/179 */,
	10	/* 18/180-18/189 */,
	11	/* 18/190-18/199 */,
	11	/* 18/200-18/209 */,
	12	/* 18/210-18/219 */,
	13	/* 18/220+ */
};

/**
 * Tabla de Estadísticas (SAB) -- Tirada de salvación
 */
static const int adj_wis_sav[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	1	/* 8 */,
	1	/* 9 */,
	1	/* 10 */,
	1	/* 11 */,
	1	/* 12 */,
	1	/* 13 */,
	1	/* 14 */,
	2	/* 15 */,
	2	/* 16 */,
	2	/* 17 */,
	3	/* 18/00-18/09 */,
	3	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	3	/* 18/30-18/39 */,
	3	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	4	/* 18/60-18/69 */,
	5	/* 18/70-18/79 */,
	5	/* 18/80-18/89 */,
	6	/* 18/90-18/99 */,
	7	/* 18/100-18/109 */,
	8	/* 18/110-18/119 */,
	9	/* 18/120-18/129 */,
	10	/* 18/130-18/139 */,
	11	/* 18/140-18/149 */,
	12	/* 18/150-18/159 */,
	13	/* 18/160-18/169 */,
	14	/* 18/170-18/179 */,
	15	/* 18/180-18/189 */,
	16	/* 18/190-18/199 */,
	17	/* 18/200-18/209 */,
	18	/* 18/210-18/219 */,
	19	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (DES) -- desarme
 */
static const int adj_dex_dis[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	1	/* 8 */,
	1	/* 9 */,
	1	/* 10 */,
	1	/* 11 */,
	1	/* 12 */,
	1	/* 13 */,
	1	/* 14 */,
	2	/* 15 */,
	2	/* 16 */,
	2	/* 17 */,
	3	/* 18/00-18/09 */,
	3	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	4	/* 18/30-18/39 */,
	4	/* 18/40-18/49 */,
	5	/* 18/50-18/59 */,
	6	/* 18/60-18/69 */,
	7	/* 18/70-18/79 */,
	8	/* 18/80-18/89 */,
	9	/* 18/90-18/99 */,
	10	/* 18/100-18/109 */,
	10	/* 18/110-18/119 */,
	11	/* 18/120-18/129 */,
	12	/* 18/130-18/139 */,
	13	/* 18/140-18/149 */,
	14	/* 18/150-18/159 */,
	15	/* 18/160-18/169 */,
	16	/* 18/170-18/179 */,
	17	/* 18/180-18/189 */,
	18	/* 18/190-18/199 */,
	19	/* 18/200-18/209 */,
	19	/* 18/210-18/219 */,
	19	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (INT) -- desarme
 */
static const int adj_int_dis[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	1	/* 8 */,
	1	/* 9 */,
	1	/* 10 */,
	1	/* 11 */,
	1	/* 12 */,
	1	/* 13 */,
	1	/* 14 */,
	2	/* 15 */,
	2	/* 16 */,
	2	/* 17 */,
	3	/* 18/00-18/09 */,
	3	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	4	/* 18/30-18/39 */,
	4	/* 18/40-18/49 */,
	5	/* 18/50-18/59 */,
	6	/* 18/60-18/69 */,
	7	/* 18/70-18/79 */,
	8	/* 18/80-18/89 */,
	9	/* 18/90-18/99 */,
	10	/* 18/100-18/109 */,
	10	/* 18/110-18/119 */,
	11	/* 18/120-18/129 */,
	12	/* 18/130-18/139 */,
	13	/* 18/140-18/149 */,
	14	/* 18/150-18/159 */,
	15	/* 18/160-18/169 */,
	16	/* 18/170-18/179 */,
	17	/* 18/180-18/189 */,
	18	/* 18/190-18/199 */,
	19	/* 18/200-18/209 */,
	19	/* 18/210-18/219 */,
	19	/* 18/220+ */
};

/**
 * Tabla de Estadísticas (DES) -- bonificación a CA
 */
static const int adj_dex_ta[STAT_RANGE] =
{
	-4	/* 3 */,
	-3	/* 4 */,
	-2	/* 5 */,
	-1	/* 6 */,
	0	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	0	/* 10 */,
	0	/* 11 */,
	0	/* 12 */,
	0	/* 13 */,
	0	/* 14 */,
	1	/* 15 */,
	1	/* 16 */,
	1	/* 17 */,
	2	/* 18/00-18/09 */,
	2	/* 18/10-18/19 */,
	2	/* 18/20-18/29 */,
	2	/* 18/30-18/39 */,
	2	/* 18/40-18/49 */,
	3	/* 18/50-18/59 */,
	3	/* 18/60-18/69 */,
	3	/* 18/70-18/79 */,
	4	/* 18/80-18/89 */,
	5	/* 18/90-18/99 */,
	6	/* 18/100-18/109 */,
	7	/* 18/110-18/119 */,
	8	/* 18/120-18/129 */,
	9	/* 18/130-18/139 */,
	9	/* 18/140-18/149 */,
	10	/* 18/150-18/159 */,
	11	/* 18/160-18/169 */,
	12	/* 18/170-18/179 */,
	13	/* 18/180-18/189 */,
	14	/* 18/190-18/199 */,
	15	/* 18/200-18/209 */,
	15	/* 18/210-18/219 */,
	15	/* 18/220+ */
};

/**
 * Tabla de Estadísticas (FUE) -- bonificación a daño
 */
const int adj_str_td[STAT_RANGE] =
{
	-2	/* 3 */,
	-2	/* 4 */,
	-1	/* 5 */,
	-1	/* 6 */,
	0	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	0	/* 10 */,
	0	/* 11 */,
	0	/* 12 */,
	0	/* 13 */,
	0	/* 14 */,
	0	/* 15 */,
	1	/* 16 */,
	2	/* 17 */,
	2	/* 18/00-18/09 */,
	2	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	3	/* 18/30-18/39 */,
	3	/* 18/40-18/49 */,
	3	/* 18/50-18/59 */,
	3	/* 18/60-18/69 */,
	4	/* 18/70-18/79 */,
	5	/* 18/80-18/89 */,
	5	/* 18/90-18/99 */,
	6	/* 18/100-18/109 */,
	7	/* 18/110-18/119 */,
	8	/* 18/120-18/129 */,
	9	/* 18/130-18/139 */,
	10	/* 18/140-18/149 */,
	11	/* 18/150-18/159 */,
	12	/* 18/160-18/169 */,
	13	/* 18/170-18/179 */,
	14	/* 18/180-18/189 */,
	15	/* 18/190-18/199 */,
	16	/* 18/200-18/209 */,
	18	/* 18/210-18/219 */,
	20	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (DES) -- bonificación a golpear
 */
const int adj_dex_th[STAT_RANGE] =
{
	-3	/* 3 */,
	-2	/* 4 */,
	-2	/* 5 */,
	-1	/* 6 */,
	-1	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	0	/* 10 */,
	0	/* 11 */,
	0	/* 12 */,
	0	/* 13 */,
	0	/* 14 */,
	0	/* 15 */,
	1	/* 16 */,
	2	/* 17 */,
	3	/* 18/00-18/09 */,
	3	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	3	/* 18/30-18/39 */,
	3	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	4	/* 18/60-18/69 */,
	4	/* 18/70-18/79 */,
	4	/* 18/80-18/89 */,
	5	/* 18/90-18/99 */,
	6	/* 18/100-18/109 */,
	7	/* 18/110-18/119 */,
	8	/* 18/120-18/129 */,
	9	/* 18/130-18/139 */,
	9	/* 18/140-18/149 */,
	10	/* 18/150-18/159 */,
	11	/* 18/160-18/169 */,
	12	/* 18/170-18/179 */,
	13	/* 18/180-18/189 */,
	14	/* 18/190-18/199 */,
	15	/* 18/200-18/209 */,
	15	/* 18/210-18/219 */,
	15	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (FUE) -- bonificación a golpear
 */
static const int adj_str_th[STAT_RANGE] =
{
	-3	/* 3 */,
	-2	/* 4 */,
	-1	/* 5 */,
	-1	/* 6 */,
	0	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	0	/* 10 */,
	0	/* 11 */,
	0	/* 12 */,
	0	/* 13 */,
	0	/* 14 */,
	0	/* 15 */,
	0	/* 16 */,
	0	/* 17 */,
	1	/* 18/00-18/09 */,
	1	/* 18/10-18/19 */,
	1	/* 18/20-18/29 */,
	1	/* 18/30-18/39 */,
	1	/* 18/40-18/49 */,
	1	/* 18/50-18/59 */,
	1	/* 18/60-18/69 */,
	2	/* 18/70-18/79 */,
	3	/* 18/80-18/89 */,
	4	/* 18/90-18/99 */,
	5	/* 18/100-18/109 */,
	6	/* 18/110-18/119 */,
	7	/* 18/120-18/129 */,
	8	/* 18/130-18/139 */,
	9	/* 18/140-18/149 */,
	10	/* 18/150-18/159 */,
	11	/* 18/160-18/169 */,
	12	/* 18/170-18/179 */,
	13	/* 18/180-18/189 */,
	14	/* 18/190-18/199 */,
	15	/* 18/200-18/209 */,
	15	/* 18/210-18/219 */,
	15	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (FUE) -- límite de peso en deca-libras
 */
static const int adj_str_wgt[STAT_RANGE] =
{
	5	/* 3 */,
	6	/* 4 */,
	7	/* 5 */,
	8	/* 6 */,
	9	/* 7 */,
	10	/* 8 */,
	11	/* 9 */,
	12	/* 10 */,
	13	/* 11 */,
	14	/* 12 */,
	15	/* 13 */,
	16	/* 14 */,
	17	/* 15 */,
	18	/* 16 */,
	19	/* 17 */,
	20	/* 18/00-18/09 */,
	22	/* 18/10-18/19 */,
	24	/* 18/20-18/29 */,
	26	/* 18/30-18/39 */,
	28	/* 18/40-18/49 */,
	30	/* 18/50-18/59 */,
	30	/* 18/60-18/69 */,
	30	/* 18/70-18/79 */,
	30	/* 18/80-18/89 */,
	30	/* 18/90-18/99 */,
	30	/* 18/100-18/109 */,
	30	/* 18/110-18/119 */,
	30	/* 18/120-18/129 */,
	30	/* 18/130-18/139 */,
	30	/* 18/140-18/149 */,
	30	/* 18/150-18/159 */,
	30	/* 18/160-18/169 */,
	30	/* 18/170-18/179 */,
	30	/* 18/180-18/189 */,
	30	/* 18/190-18/199 */,
	30	/* 18/200-18/209 */,
	30	/* 18/210-18/219 */,
	30	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (FUE) -- límite de peso del arma en libras
 */
const int adj_str_hold[STAT_RANGE] =
{
	4	/* 3 */,
	5	/* 4 */,
	6	/* 5 */,
	7	/* 6 */,
	8	/* 7 */,
	10	/* 8 */,
	12	/* 9 */,
	14	/* 10 */,
	16	/* 11 */,
	18	/* 12 */,
	20	/* 13 */,
	22	/* 14 */,
	24	/* 15 */,
	26	/* 16 */,
	28	/* 17 */,
	30	/* 18/00-18/09 */,
	30	/* 18/10-18/19 */,
	35	/* 18/20-18/29 */,
	40	/* 18/30-18/39 */,
	45	/* 18/40-18/49 */,
	50	/* 18/50-18/59 */,
	55	/* 18/60-18/69 */,
	60	/* 18/70-18/79 */,
	65	/* 18/80-18/89 */,
	70	/* 18/90-18/99 */,
	80	/* 18/100-18/109 */,
	80	/* 18/110-18/119 */,
	80	/* 18/120-18/129 */,
	80	/* 18/130-18/139 */,
	80	/* 18/140-18/149 */,
	90	/* 18/150-18/159 */,
	90	/* 18/160-18/169 */,
	90	/* 18/170-18/179 */,
	90	/* 18/180-18/189 */,
	90	/* 18/190-18/199 */,
	100	/* 18/200-18/209 */,
	100	/* 18/210-18/219 */,
	100	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (FUE) -- valor de excavación
 */
static const int adj_str_dig[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	1	/* 5 */,
	2	/* 6 */,
	3	/* 7 */,
	4	/* 8 */,
	4	/* 9 */,
	5	/* 10 */,
	5	/* 11 */,
	6	/* 12 */,
	6	/* 13 */,
	7	/* 14 */,
	7	/* 15 */,
	8	/* 16 */,
	8	/* 17 */,
	9	/* 18/00-18/09 */,
	10	/* 18/10-18/19 */,
	12	/* 18/20-18/29 */,
	15	/* 18/30-18/39 */,
	20	/* 18/40-18/49 */,
	25	/* 18/50-18/59 */,
	30	/* 18/60-18/69 */,
	35	/* 18/70-18/79 */,
	40	/* 18/80-18/89 */,
	45	/* 18/90-18/99 */,
	50	/* 18/100-18/109 */,
	55	/* 18/110-18/119 */,
	60	/* 18/120-18/129 */,
	65	/* 18/130-18/139 */,
	70	/* 18/140-18/149 */,
	75	/* 18/150-18/159 */,
	80	/* 18/160-18/169 */,
	85	/* 18/170-18/179 */,
	90	/* 18/180-18/189 */,
	95	/* 18/190-18/199 */,
	100	/* 18/200-18/209 */,
	100	/* 18/210-18/219 */,
	100	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (FUE) -- índice de ayuda para la tabla de "golpes"
 */
const int adj_str_blow[STAT_RANGE] =
{
	3	/* 3 */,
	4	/* 4 */,
	5	/* 5 */,
	6	/* 6 */,
	7	/* 7 */,
	8	/* 8 */,
	9	/* 9 */,
	10	/* 10 */,
	11	/* 11 */,
	12	/* 12 */,
	13	/* 13 */,
	14	/* 14 */,
	15	/* 15 */,
	16	/* 16 */,
	17	/* 17 */,
	20 /* 18/00-18/09 */,
	30 /* 18/10-18/19 */,
	40 /* 18/20-18/29 */,
	50 /* 18/30-18/39 */,
	60 /* 18/40-18/49 */,
	70 /* 18/50-18/59 */,
	80 /* 18/60-18/69 */,
	90 /* 18/70-18/79 */,
	100 /* 18/80-18/89 */,
	110 /* 18/90-18/99 */,
	120 /* 18/100-18/109 */,
	130 /* 18/110-18/119 */,
	140 /* 18/120-18/129 */,
	150 /* 18/130-18/139 */,
	160 /* 18/140-18/149 */,
	170 /* 18/150-18/159 */,
	180 /* 18/160-18/169 */,
	190 /* 18/170-18/179 */,
	200 /* 18/180-18/189 */,
	210 /* 18/190-18/199 */,
	220 /* 18/200-18/209 */,
	230 /* 18/210-18/219 */,
	240 /* 18/220+ */
};


/**
 * Tabla de Estadísticas (DES) -- índice para la tabla de "golpes"
 */
static const int adj_dex_blow[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	1	/* 10 */,
	1	/* 11 */,
	1	/* 12 */,
	1	/* 13 */,
	1	/* 14 */,
	1	/* 15 */,
	1	/* 16 */,
	2	/* 17 */,
	2	/* 18/00-18/09 */,
	2	/* 18/10-18/19 */,
	3	/* 18/20-18/29 */,
	3	/* 18/30-18/39 */,
	4	/* 18/40-18/49 */,
	4	/* 18/50-18/59 */,
	5	/* 18/60-18/69 */,
	5	/* 18/70-18/79 */,
	6	/* 18/80-18/89 */,
	6	/* 18/90-18/99 */,
	7	/* 18/100-18/109 */,
	7	/* 18/110-18/119 */,
	8	/* 18/120-18/129 */,
	8	/* 18/130-18/139 */,
	8	/* 18/140-18/149 */,
	9	/* 18/150-18/159 */,
	9	/* 18/160-18/169 */,
	9	/* 18/170-18/179 */,
	10	/* 18/180-18/189 */,
	10	/* 18/190-18/199 */,
	11	/* 18/200-18/209 */,
	11	/* 18/210-18/219 */,
	11	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (DES) -- probabilidad de evitar "robo" y "caída"
 */
const int adj_dex_safe[STAT_RANGE] =
{
	0	/* 3 */,
	1	/* 4 */,
	2	/* 5 */,
	3	/* 6 */,
	4	/* 7 */,
	5	/* 8 */,
	5	/* 9 */,
	6	/* 10 */,
	6	/* 11 */,
	7	/* 12 */,
	7	/* 13 */,
	8	/* 14 */,
	8	/* 15 */,
	9	/* 16 */,
	9	/* 17 */,
	10	/* 18/00-18/09 */,
	10	/* 18/10-18/19 */,
	15	/* 18/20-18/29 */,
	15	/* 18/30-18/39 */,
	20	/* 18/40-18/49 */,
	25	/* 18/50-18/59 */,
	30	/* 18/60-18/69 */,
	35	/* 18/70-18/79 */,
	40	/* 18/80-18/89 */,
	45	/* 18/90-18/99 */,
	50	/* 18/100-18/109 */,
	60	/* 18/110-18/119 */,
	70	/* 18/120-18/129 */,
	80	/* 18/130-18/139 */,
	90	/* 18/140-18/149 */,
	100	/* 18/150-18/159 */,
	100	/* 18/160-18/169 */,
	100	/* 18/170-18/179 */,
	100	/* 18/180-18/189 */,
	100	/* 18/190-18/199 */,
	100	/* 18/200-18/209 */,
	100	/* 18/210-18/219 */,
	100	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (CON) -- tasa de regeneración base
 */
const int adj_con_fix[STAT_RANGE] =
{
	0	/* 3 */,
	0	/* 4 */,
	0	/* 5 */,
	0	/* 6 */,
	0	/* 7 */,
	0	/* 8 */,
	0	/* 9 */,
	0	/* 10 */,
	0	/* 11 */,
	0	/* 12 */,
	0	/* 13 */,
	1	/* 14 */,
	1	/* 15 */,
	1	/* 16 */,
	1	/* 17 */,
	2	/* 18/00-18/09 */,
	2	/* 18/10-18/19 */,
	2	/* 18/20-18/29 */,
	2	/* 18/30-18/39 */,
	2	/* 18/40-18/49 */,
	3	/* 18/50-18/59 */,
	3	/* 18/60-18/69 */,
	3	/* 18/70-18/79 */,
	3	/* 18/80-18/89 */,
	3	/* 18/90-18/99 */,
	4	/* 18/100-18/109 */,
	4	/* 18/110-18/119 */,
	5	/* 18/120-18/129 */,
	6	/* 18/130-18/139 */,
	6	/* 18/140-18/149 */,
	7	/* 18/150-18/159 */,
	7	/* 18/160-18/169 */,
	8	/* 18/170-18/179 */,
	8	/* 18/180-18/189 */,
	8	/* 18/190-18/199 */,
	9	/* 18/200-18/209 */,
	9	/* 18/210-18/219 */,
	9	/* 18/220+ */
};


/**
 * Tabla de Estadísticas (CON) -- puntos de golpe extra 1/100 por nivel
 */
static const int adj_con_mhp[STAT_RANGE] =
{
	-250	/* 3 */,
	-150	/* 4 */,
	-100	/* 5 */,
	 -75	/* 6 */,
	 -50	/* 7 */,
	 -25	/* 8 */,
	 -10	/* 9 */,
	  -5	/* 10 */,
	   0	/* 11 */,
	   5	/* 12 */,
	  10	/* 13 */,
	  25	/* 14 */,
	  50	/* 15 */,
	  75	/* 16 */,
	 100	/* 17 */,
	 150	/* 18/00-18/09 */,
	 175	/* 18/10-18/19 */,
	 200	/* 18/20-18/29 */,
	 225	/* 18/30-18/39 */,
	 250	/* 18/40-18/49 */,
	 275	/* 18/50-18/59 */,
	 300	/* 18/60-18/69 */,
	 350	/* 18/70-18/79 */,
	 400	/* 18/80-18/89 */,
	 450	/* 18/90-18/99 */,
	 500	/* 18/100-18/109 */,
	 550	/* 18/110-18/119 */,
	 600	/* 18/120-18/129 */,
	 650	/* 18/130-18/139 */,
	 700	/* 18/140-18/149 */,
	 750	/* 18/150-18/159 */,
	 800	/* 18/160-18/169 */,
	 900	/* 18/170-18/179 */,
	1000	/* 18/180-18/189 */,
	1100	/* 18/190-18/199 */,
	1250	/* 18/200-18/209 */,
	1250	/* 18/210-18/219 */,
	1250	/* 18/220+ */
};

static const int adj_mag_study[STAT_RANGE] =
{
	  0	/* 3 */,
	  0	/* 4 */,
	 10	/* 5 */,
	 20	/* 6 */,
	 30	/* 7 */,
	 40	/* 8 */,
	 50	/* 9 */,
	 60	/* 10 */,
	 70	/* 11 */,
	 80	/* 12 */,
	 85	/* 13 */,
	 90	/* 14 */,
	 95	/* 15 */,
	100	/* 16 */,
	105	/* 17 */,
	110	/* 18/00-18/09 */,
	115	/* 18/10-18/19 */,
	120	/* 18/20-18/29 */,
	130	/* 18/30-18/39 */,
	140	/* 18/40-18/49 */,
	150	/* 18/50-18/59 */,
	160	/* 18/60-18/69 */,
	170	/* 18/70-18/79 */,
	180	/* 18/80-18/89 */,
	190	/* 18/90-18/99 */,
	200	/* 18/100-18/109 */,
	210	/* 18/110-18/119 */,
	220	/* 18/120-18/129 */,
	230	/* 18/130-18/139 */,
	240	/* 18/140-18/149 */,
	250	/* 18/150-18/159 */,
	250	/* 18/160-18/169 */,
	250	/* 18/170-18/179 */,
	250	/* 18/180-18/189 */,
	250	/* 18/190-18/199 */,
	250	/* 18/200-18/209 */,
	250	/* 18/210-18/219 */,
	250	/* 18/220+ */
};

/**
 * Tabla de Estadísticas (INT/SAB) -- puntos de maná extra 1/100 por nivel
 */
static const int adj_mag_mana[STAT_RANGE] =
{
	  0	/* 3 */,
	 10	/* 4 */,
	 20	/* 5 */,
	 30	/* 6 */,
	 40	/* 7 */,
	 50	/* 8 */,
	 60	/* 9 */,
	 70	/* 10 */,
	 80	/* 11 */,
	 90	/* 12 */,
	100	/* 13 */,
	110	/* 14 */,
	120	/* 15 */,
	130	/* 16 */,
	140	/* 17 */,
	150	/* 18/00-18/09 */,
	160	/* 18/10-18/19 */,
	170	/* 18/20-18/29 */,
	180	/* 18/30-18/39 */,
	190	/* 18/40-18/49 */,
	200	/* 18/50-18/59 */,
	225	/* 18/60-18/69 */,
	250	/* 18/70-18/79 */,
	300	/* 18/80-18/89 */,
	350	/* 18/90-18/99 */,
	400	/* 18/100-18/109 */,
	450	/* 18/110-18/119 */,
	500	/* 18/120-18/129 */,
	550	/* 18/130-18/139 */,
	600	/* 18/140-18/149 */,
	650	/* 18/150-18/159 */,
	700	/* 18/160-18/169 */,
	750	/* 18/170-18/179 */,
	800	/* 18/180-18/189 */,
	800	/* 18/190-18/199 */,
	800	/* 18/200-18/209 */,
	800	/* 18/210-18/219 */,
	800	/* 18/220+ */
};

/**
 * Esta tabla se utiliza para ayudar a calcular el número de golpes que el jugador puede
 * realizar en una sola ronda de ataques (un turno de jugador) con un arma normal.
 *
 * Este número va desde un solo golpe/ronda para jugadores débiles hasta hasta seis
 * golpes/ronda para guerreros poderosos.
 *
 * Nótese que ciertos artefactos y objetos de égida dan golpes extra/ronda.
 *
 * Primero, de la clase del jugador, extraemos algunos valores:
 *
 *    Guerrero --> num = 6; mul = 5; div = MAX(30, peso_arma);
 *    Mago    --> num = 4; mul = 2; div = MAX(40, peso_arma);
 *    Sacerdote --> num = 4; mul = 3; div = MAX(35, peso_arma);
 *    Pícaro   --> num = 5; mul = 4; div = MAX(30, peso_arma);
 *    Guardabosques --> num = 5; mul = 4; div = MAX(35, peso_arma);
 *    Paladín --> num = 5; mul = 5; div = MAX(30, peso_arma);
 * (todo especificado en class.txt ahora)
 *
 * Para obtener "P", buscamos el "adj_str_blow[]" relevante (ver arriba),
 * lo multiplicamos por "mul", y luego lo dividimos por "div", redondeando hacia abajo.
 *
 * Para obtener "D", buscamos el "adj_dex_blow[]" relevante (ver arriba).
 *
 * Luego buscamos el coste de energía de cada golpe usando "blows_table[P][D]".
 * El jugador obtiene golpes/ronda igual a 100/este número, hasta un máximo de
 * "num" golpes/ronda, más cualquier golpe/ronda "extra".
 */
static const int blows_table[12][12] =
{
	/* P */
   /* D:   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11+ */
   /* DES: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */

	/* 0  */
	{  100, 100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23 },

	/* 1  */
	{  100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23,  21 },

	/* 2  */
	{  95,  85,  75,  60,  50,  42,  35,  30,  26,  23,  21,  20 },

	/* 3  */
	{  85,  75,  60,  50,  42,  36,  32,  28,  25,  22,  20,  19 },

	/* 4  */
	{  75,  60,  50,  42,  36,  33,  28,  25,  23,  21,  19,  18 },

	/* 5  */
	{  60,  50,  42,  36,  33,  30,  27,  24,  22,  21,  19,  17 },

	/* 6  */
	{  50,  42,  36,  33,  30,  27,  25,  23,  21,  20,  18,  17 },

	/* 7  */
	{  42,  36,  33,  30,  28,  26,  24,  22,  20,  19,  18,  17 },

	/* 8  */
	{  36,  33,  30,  28,  26,  24,  22,  21,  20,  19,  17,  16 },

	/* 9  */
	{  35,  32,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16 },

	/* 10 */
	{  34,  30,  27,  25,  23,  22,  21,  20,  19,  18,  17,  16 },

	/* 11+ */
	{  33,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16,  15 },
   /* DES: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */
};

/**
 * Decidir qué objeto aparece antes en la lista de inventario estándar,
 * por defecto el primero si nada los separa.
 *
 * \return si reemplazar el objeto original con el nuevo
 */
bool earlier_object(struct object *orig, struct object *new, bool store)
{
	/* Verificar que tenemos objetos reales */
	if (!new) return false;
	if (!orig) return true;

	if (!store) {
		/* Los libros legibles siempre van primero */
		if (obj_can_browse(orig) && !obj_can_browse(new)) return false;
		if (!obj_can_browse(orig) && obj_can_browse(new)) return true;
	}

	/* La munición utilizable va antes que otra munición */
	if (tval_is_ammo(orig) && tval_is_ammo(new)) {
		/* Primero favorecer la munición utilizable */
		if ((player->state.ammo_tval == orig->tval) &&
			(player->state.ammo_tval != new->tval))
			return false;
		if ((player->state.ammo_tval != orig->tval) &&
			(player->state.ammo_tval == new->tval))
			return true;
	}

	/* Los objetos se ordenan por tipo decreciente */
	if (orig->tval > new->tval) return false;
	if (orig->tval < new->tval) return true;

	if (!store) {
		/* Los objetos no identificados (con sabor) siempre van al final (por defecto orig) */
		if (!object_flavor_is_aware(new)) return false;
		if (!object_flavor_is_aware(orig)) return true;
	}

	/* Los objetos se ordenan por sval creciente */
	if (orig->sval < new->sval) return false;
	if (orig->sval > new->sval) return true;

	if (!store) {
		/* Los objetos no identificados siempre van al final (por defecto orig) */
		if (new->kind->flavor && !object_flavor_is_aware(new)) return false;
		if (orig->kind->flavor && !object_flavor_is_aware(orig)) return true;

		/* Las luces se ordenan por combustible decreciente */
		if (tval_is_light(orig)) {
			if (orig->pval > new->pval) return false;
			if (orig->pval < new->pval) return true;
		}
	}

	/* Los objetos se ordenan por valor decreciente, excepto la munición */
	if (tval_is_ammo(orig)) {
		if (object_value(orig, 1) < object_value(new, 1))
			return false;
		if (object_value(orig, 1) >	object_value(new, 1))
			return true;
	} else {
		if (object_value(orig, 1) >	object_value(new, 1))
			return false;
		if (object_value(orig, 1) <	object_value(new, 1))
			return true;
	}

	/* Sin preferencia */
	return false;
}

int equipped_item_slot(struct player_body body, struct object *item)
{
	int i;

	if (item == NULL) return body.count;

	/* Buscar una ranura de equipo con este objeto */
	for (i = 0; i < body.count; i++)
		if (item == body.slots[i].obj) break;

	/* Ranura correcta, o body.count si no está equipado */
	return i;
}

/**
 * Poner el inventario y carcaj del jugador en matrices fácilmente accesibles. La
 * mochila puede tener un objeto de más
 */
void calc_inventory(struct player *p)
{
	int old_inven_cnt = p->upkeep->inven_cnt;
	int n_stack_split = 0;
	int n_pack_remaining = z_info->pack_size - pack_slots_used(p);
	int n_max = 1 + z_info->pack_size + z_info->quiver_size
		+ p->body.count;
	struct object **old_quiver = mem_zalloc(z_info->quiver_size
		* sizeof(*old_quiver));
	struct object **old_pack = mem_zalloc(z_info->pack_size
		* sizeof(*old_pack));
	bool *assigned = mem_alloc(n_max * sizeof(*assigned));
	struct object *current;
	int i, j;

	/*
	 * Los objetos equipados ya están atendidos. Solo los demás necesitan
	 * ser probados para su asignación al carcaj o a la mochila.
	 */
	for (current = p->gear, j = 0; current; current = current->next, ++j) {
		assert(j < n_max);
		assigned[j] = object_is_equipped(p->body, current);
	}
	for (; j < n_max; ++j) {
		assigned[j] = false;
	}

	/* Prepararse para llenar el carcaj */
	p->upkeep->quiver_cnt = 0;

	/* Copiar el carcaj actual y luego dejarlo vacío. */
	for (i = 0; i < z_info->quiver_size; i++) {
		if (p->upkeep->quiver[i]) {
			old_quiver[i] = p->upkeep->quiver[i];
			p->upkeep->quiver[i] = NULL;
		} else {
			old_quiver[i] = NULL;
		}
	}

	/* Llenar carcaj. Primero, asignar objetos inscritos. */
	for (current = p->gear, j = 0; current; current = current->next, ++j) {
		int prefslot;

		/* Saltar objetos ya asignados (ej. equipados). */
		if (assigned[j]) continue;

		prefslot  = preferred_quiver_slot(current);
		if (prefslot >= 0 && prefslot < z_info->quiver_size
				&& !p->upkeep->quiver[prefslot]) {
			/*
			 * La ranura preferida está vacía. Dividir el montón si
			 * es necesario. No permitir división si pudiera resultar
			 * en sobrellenar la mochila en más de una ranura.
			 */
			int mult = tval_is_ammo(current) ?
				1 : z_info->thrown_quiver_mult;
			struct object *to_quiver;

			if (current->number * mult
					<= z_info->quiver_slot_size) {
				to_quiver = current;
			} else {
				int nsplit = z_info->quiver_slot_size / mult;

				assert(nsplit < current->number);
				if (nsplit > 0 && n_stack_split
						<= n_pack_remaining) {
					/*
					 * Separar la parte que va a la
					 * mochila. Como el montón en el
					 * carcaj está antes en la lista de equipo,
					 * preferirá permanecer en el carcaj
					 * en futuras llamadas a calc_inventory()
					 * y será el objetivo preferido para
					 * combine_pack().
					 */
					to_quiver = current;
					gear_insert_end(p, object_split(current,
						current->number - nsplit));
					++n_stack_split;
				} else {
					to_quiver = NULL;
				}
			}

			if (to_quiver) {
				p->upkeep->quiver[prefslot] = to_quiver;
				p->upkeep->quiver_cnt += to_quiver->number * mult;

				/* Esa parte del equipo ha sido tratada. */
				assigned[j] = true;
			}
		}
	}

	/* Ahora llenar el resto de las ranuras en orden. */
	for (i = 0; i < z_info->quiver_size; ++i) {
		struct object *first = NULL;
		int jfirst = -1;

		/* Si la ranura está llena, continuar. */
		if (p->upkeep->quiver[i]) continue;

		/* Encontrar el objeto de carcaj que debería ir allí. */
		j = 0;
		current = p->gear;
		while (1) {
			if (!current) break;
			assert(j < n_max);

			/*
			 * Solo intentar asignar si no está asignado, es munición, y,
			 * si es necesario dividir, hay espacio para los montones
			 * divididos.
			 */
			if (!assigned[j] && tval_is_ammo(current)
					&& (current->number
					<= z_info->quiver_slot_size
					|| (z_info->quiver_slot_size > 0
					&& n_stack_split
					<= n_pack_remaining))) {
				/* Elegir el primero en orden. */
				if (earlier_object(first, current, false)) {
					first = current;
					jfirst = j;
				}
			}

			current = current->next;
			++j;
		}

		/* Dejar de buscar si no queda nada en el equipo. */
		if (!first) break;

		/* Poner el objeto en la ranura, dividiendo (si es necesario) para que quepa. */
		if (first->number > z_info->quiver_slot_size) {
			assert(z_info->quiver_slot_size > 0
				&& n_stack_split <= n_pack_remaining);
			/* Como arriba, separar la parte que va a la mochila. */
			gear_insert_end(p, object_split(first,
				first->number - z_info->quiver_slot_size));
		}
		p->upkeep->quiver[i] = first;
		p->upkeep->quiver_cnt += first->number;

		/* Esa parte del equipo ha sido tratada. */
		assigned[jfirst] = true;
	}

	/* Notar reordenamiento */
	if (character_dungeon) {
		for (i = 0; i < z_info->quiver_size; i++) {
			if (old_quiver[i] && p->upkeep->quiver[i] != old_quiver[i]) {
				msg("Reorganizas tu carcaj.");
				break;
			}
		}
	}

	/* Copiar la mochila actual */
	for (i = 0; i < z_info->pack_size; i++) {
		old_pack[i] = p->upkeep->inven[i];
	}

	/* Prepararse para llenar el inventario */
	p->upkeep->inven_cnt = 0;

	for (i = 0; i <= z_info->pack_size; i++) {
		struct object *first = NULL;
		int jfirst = -1;

		/* Encontrar el objeto que debería ir allí. */
		j = 0;
		current = p->gear;
		while (1) {
			if (!current) break;
			assert(j < n_max);

			/* Considerarlo si aún no ha sido tratado. */
			if (!assigned[j]) {
				/* Elegir el primero en orden. */
				if (earlier_object(first, current, false)) {
					first = current;
					jfirst = j;
				}
			}

			current = current->next;
			++j;
		}

		/* Asignar */
		p->upkeep->inven[i] = first;
		if (first) {
			++p->upkeep->inven_cnt;
			assigned[jfirst] = true;
		}
	}

	/* Notar reordenamiento */
	if (character_dungeon && p->upkeep->inven_cnt == old_inven_cnt) {
		for (i = 0; i < z_info->pack_size; i++) {
			if (old_pack[i] && p->upkeep->inven[i] != old_pack[i]
					 && !object_is_equipped(p->body, old_pack[i])) {
				msg("Reorganizas tu mochila.");
				break;
			}
		}
	}

	mem_free(assigned);
	mem_free(old_pack);
	mem_free(old_quiver);
}

/**
 * Promedio de las estadísticas de hechizo del jugador en todos los reinos desde los que puede lanzar,
 * redondeado hacia arriba
 *
 * Si el jugador solo puede lanzar desde un único reino, esto es simplemente la estadística
 * para ese reino
 */
static int average_spell_stat(struct player *p, struct player_state *state)
{
	int i, count, sum = 0;
	struct magic_realm *realm = class_magic_realms(p->class, &count), *r_next;

	for (i = count; i > 0; i--) {
		sum += state->stat_ind[realm->stat];
		r_next = realm->next;
		mem_free(realm);
		realm = r_next;
	}
	return (sum + count - 1) / count;
}

/**
 * Calcular el número de hechizos que el jugador debería tener, y olvidar,
 * o recordar, hechizos hasta que ese número se refleje adecuadamente.
 *
 * Nótese que esta función induce varios mensajes de "estado",
 * que deben ser omitidos hasta que el personaje sea creado.
 */
static void calc_spells(struct player *p)
{
	int i, j, k, levels;
	int num_allowed, num_known, num_total = p->class->magic.total_spells;
	int percent_spells;

	const struct class_spell *spell;

	int16_t old_spells;

	/* Debe ser alfabetizado */
	if (!p->class->magic.total_spells) return;

	/* Esperar a la creación */
	if (!character_generated) return;

	/* Manejar modo parcial */
	if (p->upkeep->only_partial) return;

	/* Guardar el valor de new_spells */
	old_spells = p->upkeep->new_spells;

	/* Determinar el número de hechizos permitidos */
	levels = p->lev - p->class->magic.spell_first + 1;

	/* Sin hechizos negativos */
	if (levels < 0) levels = 0;

	/* Número de 1/100 hechizos por nivel (o algo - necesita aclaración) */
	percent_spells = adj_mag_study[average_spell_stat(p, &p->state)];

	/* Extraer total de hechizos permitidos (redondeado hacia arriba) */
	num_allowed = (((percent_spells * levels) + 50) / 100);

	/* Asumir ninguno conocido */
	num_known = 0;

	/* Contar el número de hechizos que conocemos */
	for (j = 0; j < num_total; j++)
		if (p->spell_flags[j] & PY_SPELL_LEARNED)
			num_known++;

	/* Ver cuántos hechizos debemos olvidar o podemos aprender */
	p->upkeep->new_spells = num_allowed - num_known;

	/* Olvidar hechizos que son demasiado difíciles */
	for (i = num_total - 1; i >= 0; i--) {
		/* Obtener el hechizo */
		j = p->spell_order[i];

		/* Saltar no-hechizos */
		if (j >= 99) continue;

		/* Obtener el hechizo */
		spell = spell_by_index(p, j);

		/* Saltar hechizos que tenemos permitido conocer */
		if (spell->slevel <= p->lev) continue;

		/* ¿Es conocido? */
		if (p->spell_flags[j] & PY_SPELL_LEARNED) {
			/* Marcar como olvidado */
			p->spell_flags[j] |= PY_SPELL_FORGOTTEN;

			/* Ya no es conocido */
			p->spell_flags[j] &= ~PY_SPELL_LEARNED;

			/* Mensaje */
			msg("Has olvidado %s de %s.", spell->realm->spell_noun,
				spell->name);

			/* Uno más puede ser aprendido */
			p->upkeep->new_spells++;
		}
	}

	/* Olvidar hechizos si sabemos demasiados */
	for (i = num_total - 1; i >= 0; i--) {
		/* Parar cuando sea posible */
		if (p->upkeep->new_spells >= 0) break;

		/* Obtener el (i+1)º hechizo aprendido */
		j = p->spell_order[i];

		/* Saltar hechizos desconocidos */
		if (j >= 99) continue;

		/* Obtener el hechizo */
		spell = spell_by_index(p, j);

		/* Olvidarlo (si fue aprendido) */
		if (p->spell_flags[j] & PY_SPELL_LEARNED) {
			/* Marcar como olvidado */
			p->spell_flags[j] |= PY_SPELL_FORGOTTEN;

			/* Ya no es conocido */
			p->spell_flags[j] &= ~PY_SPELL_LEARNED;

			/* Mensaje */
			msg("Has olvidado %s de %s.", spell->realm->spell_noun,
				spell->name);

			/* Uno más puede ser aprendido */
			p->upkeep->new_spells++;
		}
	}

	/* Comprobar hechizos para recordar */
	for (i = 0; i < num_total; i++) {
		/* Ninguno más que recordar */
		if (p->upkeep->new_spells <= 0) break;

		/* Obtener el siguiente hechizo que aprendimos */
		j = p->spell_order[i];

		/* Saltar hechizos desconocidos */
		if (j >= 99) break;

		/* Obtener el hechizo */
		spell = spell_by_index(p, j);

		/* Saltar hechizos que no podemos recordar */
		if (spell->slevel > p->lev) continue;

		/* Primer conjunto de hechizos */
		if (p->spell_flags[j] & PY_SPELL_FORGOTTEN) {
			/* Ya no está olvidado */
			p->spell_flags[j] &= ~PY_SPELL_FORGOTTEN;

			/* Conocido de nuevo */
			p->spell_flags[j] |= PY_SPELL_LEARNED;

			/* Mensaje */
			msg("Has recordado %s de %s.", spell->realm->spell_noun,
				spell->name);

			/* Uno menos puede ser aprendido */
			p->upkeep->new_spells--;
		}
	}

	/* Asumir que no hay hechizos disponibles */
	k = 0;

	/* Contar hechizos que pueden ser aprendidos */
	for (j = 0; j < num_total; j++) {
		/* Obtener el hechizo */
		spell = spell_by_index(p, j);

		/* Saltar hechizos que no podemos recordar o que no existen */
		if (!spell) continue;
		if (spell->slevel > p->lev || spell->slevel == 0) continue;

		/* Saltar hechizos que ya conocemos */
		if (p->spell_flags[j] & PY_SPELL_LEARNED)
			continue;

		/* Contarlo */
		k++;
	}

	/* No se pueden aprender más hechizos de los que existen */
	if (p->upkeep->new_spells > k) p->upkeep->new_spells = k;

	/* El contador de hechizos cambió */
	if (old_spells != p->upkeep->new_spells) {
		/* Mensaje si es necesario */
		if (p->upkeep->new_spells) {
			int count;
			struct magic_realm *r = class_magic_realms(p->class, &count), *r1;
			char buf[120];

			my_strcpy(buf, r->spell_noun, sizeof(buf));
			if (p->upkeep->new_spells > 1) {
				my_strcat(buf, "s", sizeof(buf));
			}
			r1 = r->next;
			mem_free(r);
			r = r1;
			if (count > 1) {
				while (r) {
					count--;
					if (count) {
						my_strcat(buf, ", ", sizeof(buf));
					} else {
						my_strcat(buf, " o ", sizeof(buf));
					}
					my_strcat(buf, r->spell_noun, sizeof(buf));
					if (p->upkeep->new_spells > 1) {
						my_strcat(buf, "s", sizeof(buf));
					}
					r1 = r->next;
					mem_free(r);
					r = r1;
				}
			}
			/* Mensaje */
			msg("Puedes aprender %d %s más.", p->upkeep->new_spells, buf);
		}

		/* Redibujar Estado de Estudio */
		p->upkeep->redraw |= (PR_STUDY | PR_OBJECT);
	}
}


/**
 * Calcular el maná máximo. No necesitas conocer ningún hechizo.
 * Nótese que el maná se reduce por armadura pesada (o inapropiada).
 *
 * Esta función induce mensajes de estado.
 */
static void calc_mana(struct player *p, struct player_state *state, bool update)
{
	int i, msp, levels, cur_wgt, max_wgt; 

	/* Debe ser alfabetizado */
	if (!p->class->magic.total_spells) {
		p->msp = 0;
		p->csp = 0;
		p->csp_frac = 0;
		return;
	}

	/* Extraer nivel "efectivo" del jugador */
	levels = (p->lev - p->class->magic.spell_first) + 1;
	if (levels > 0) {
		msp = 1;
		msp += adj_mag_mana[average_spell_stat(p, state)] * levels / 100;
	} else {
		levels = 0;
		msp = 0;
	}

	/* Asumir que el jugador no está agobiado por la armadura */
	state->cumber_armor = false;

	/* Pesar la armadura */
	cur_wgt = 0;
	for (i = 0; i < p->body.count; i++) {
		struct object *obj_local = slot_object(p, i);

		/* Ignorar no-armadura */
		if (slot_type_is(p, i, EQUIP_WEAPON)) continue;
		if (slot_type_is(p, i, EQUIP_BOW)) continue;
		if (slot_type_is(p, i, EQUIP_RING)) continue;
		if (slot_type_is(p, i, EQUIP_AMULET)) continue;
		if (slot_type_is(p, i, EQUIP_LIGHT)) continue;

		/* Añadir peso */
		if (obj_local)
			cur_wgt += object_weight_one(obj_local);
	}

	/* Determinar la tolerancia de peso */
	max_wgt = p->class->magic.spell_weight;

	/* La armadura pesada penaliza el maná */
	if (((cur_wgt - max_wgt) / 10) > 0) {
		/* Agobiado */
		state->cumber_armor = true;

		/* Reducir maná */
		msp -= ((cur_wgt - max_wgt) / 10);
	}

	/* El maná nunca puede ser negativo */
	if (msp < 0) msp = 0;

	/* Regresar si no hay actualizaciones */
	if (!update) return;

	/* El maná máximo ha cambiado */
	if (p->msp != msp) {
		/* Guardar nuevo límite */
		p->msp = msp;

		/* Aplicar nuevo límite */
		if (p->csp >= msp) {
			p->csp = msp;
			p->csp_frac = 0;
		}

		/* Mostrar maná después */
		p->upkeep->redraw |= (PR_MANA);
	}
}


/**
 * Calcular los puntos de golpe (máximos) del jugador
 *
 * Ajustar los puntos de golpe actuales si es necesario
 */
static void calc_hitpoints(struct player *p)
{
	long bonus;
	int mhp;

	/* Obtener el valor de "bonificación de 1/100 de punto de golpe por nivel" */
	bonus = adj_con_mhp[p->state.stat_ind[STAT_CON]];

	/* Calcular puntos de golpe */
	mhp = p->player_hp[p->lev-1] + (bonus * p->lev / 100);

	/* Siempre tener al menos un punto de golpe por nivel */
	if (mhp < p->lev + 1) mhp = p->lev + 1;

	/* Nuevos puntos de golpe máximos */
	if (p->mhp != mhp) {
		/* Guardar nuevo límite */
		p->mhp = mhp;

		/* Aplicar nuevo límite */
		if (p->chp >= mhp) {
			p->chp = mhp;
			p->chp_frac = 0;
		}

		/* Mostrar puntos de golpe (después) */
		p->upkeep->redraw |= (PR_HP);
	}
}


/**
 * Calcular y establecer el radio de luz actual.
 *
 * El radio de luz será el total de todas las luces llevadas.
 */
static void calc_light(struct player *p, struct player_state *state,
					   bool update)
{
	int i;

	/* Asumir sin luz */
	state->cur_light = 0;

	/* Determinar luminosidad si en la ciudad */
	if (!p->depth && is_daytime() && update) {
		/* Actualizar los visuales si es necesario */
		if (p->state.cur_light != state->cur_light)
			p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

		return;
	}

	/* Examinar todos los objetos usados, usar el más brillante */
	for (i = 0; i < p->body.count; i++) {
		int amt = 0;
		struct object *obj = slot_object(p, i);

		/* Saltar ranuras vacías */
		if (!obj) continue;

		/* Radio de luz - innato más modificador */
		if (of_has(obj->flags, OF_LIGHT_2)) {
			amt = 2;
		} else if (of_has(obj->flags, OF_LIGHT_3)) {
			amt = 3;
		}
		amt += obj->modifiers[OBJ_MOD_LIGHT];

		/* Ajuste para permitir que los jugadores SINLUZ usen equipo +1 LUZ */
		if ((obj->modifiers[OBJ_MOD_LIGHT] > 0) && pf_has(state->pflags, PF_UNLIGHT)) {
			amt--;
		}

		/* Examinar luces reales */
		if (tval_is_light(obj) && !of_has(obj->flags, OF_NO_FUEL) &&
				obj->timeout == 0)
			/* Las luces sin combustible no proporcionan luz */
			amt = 0;

		/* Alterar p->state.cur_light si es razonable */
	    state->cur_light += amt;
	}
}

/**
 * Llena `chances` con la probabilidad del jugador de excavar a través de
 * los tipos de terreno excavable en un turno de 1600.
 */
void calc_digging_chances(struct player_state *state, int chances[DIGGING_MAX])
{
	int i;

	chances[DIGGING_RUBBLE] = state->skills[SKILL_DIGGING] * 8;
	chances[DIGGING_MAGMA] = (state->skills[SKILL_DIGGING] - 10) * 4;
	chances[DIGGING_QUARTZ] = (state->skills[SKILL_DIGGING] - 20) * 2;
	chances[DIGGING_GRANITE] = (state->skills[SKILL_DIGGING] - 40) * 1;
	/* Aproximadamente una probabilidad de 1/1200 por punto de habilidad sobre 30 */
	chances[DIGGING_DOORS] = (state->skills[SKILL_DIGGING] * 4 - 119) / 3;

	/* No permitir que ninguna probabilidad negativa pase */
	for (i = 0; i < DIGGING_MAX; i++)
		chances[i] = MAX(0, chances[i]);
}

/*
 * Devolver la probabilidad, de 100, de abrir una puerta cerrada con la
 * potencia de cerradura dada.
 *
 * \param p es el jugador que intenta abrir la puerta.
 * \param lock_power es la potencia de la cerradura.
 * \param lock_unseen, si es true, asume que el jugador no tiene suficiente
 * luz para trabajar con la cerradura.
 */
int calc_unlocking_chance(const struct player *p, int lock_power,
		bool lock_unseen)
{
	int skill = p->state.skills[SKILL_DISARM_PHYS];

	if (lock_unseen || p->timed[TMD_BLIND]) {
		skill /= 10;
	}
	if (p->timed[TMD_CONFUSED] || p->timed[TMD_IMAGE]) {
		skill /= 10;
	}

	/* Siempre permitir alguna probabilidad de abrir. */
	return MAX(2, skill - 4 * lock_power);
}

/**
 * Calcular los golpes que obtendría un jugador.
 *
 * \param p es el jugador de interés
 * \param obj es el objeto para el cual estamos calculando los golpes
 * \param state es el estado del jugador para el cual estamos calculando los golpes
 * \param extra_blows es el número de +golpes disponibles de este objeto y
 * este estado
 *
 * N.B. state->num_blows ahora es 100x el número de golpes.
 */
int calc_blows(struct player *p, const struct object *obj,
			   struct player_state *state, int extra_blows)
{
	int blows;
	int str_index, dex_index;
	int div;
	int blow_energy;

	int weight = (obj == NULL) ? 0 : object_weight_one(obj);
	int min_weight = p->class->min_weight;

	/* Aplicar un "peso" mínimo (décimas de libra) */
	div = (weight < min_weight) ? min_weight : weight;

	/* Obtener la fuerza contra el peso */
	str_index = adj_str_blow[state->stat_ind[STAT_STR]] *
			p->class->att_multiply / div;

	/* Valor máximo */
	if (str_index > 11) str_index = 11;

	/* Índice por destreza */
	dex_index = MIN(adj_dex_blow[state->stat_ind[STAT_DEX]], 11);

	/* Usar la tabla de golpes para obtener energía por golpe */
	blow_energy = blows_table[str_index][dex_index];

	blows = MIN((10000 / blow_energy), (100 * p->class->max_attacks));

	/* Requerir al menos un golpe, dos para combate O */
	return MAX(blows + (100 * extra_blows),
			   OPT(p, birth_percent_damage) ? 200 : 100);
}


/**
 * Calcula el límite de peso actual.
 */
static int weight_limit(struct player_state *state)
{
	int i;

	/* Límite de peso basado solo en la fuerza */
	i = adj_str_wgt[state->stat_ind[STAT_STR]] * 100;

	/* Devolver el resultado */
	return (i);
}


/**
 * Calcula el peso restante antes de estar agobiado.
 */
int weight_remaining(struct player *p)
{
	int i;

	/* Límite de peso basado solo en la fuerza */
	i = 60 * adj_str_wgt[p->state.stat_ind[STAT_STR]]
		- p->upkeep->total_weight - 1;

	/* Devolver el resultado */
	return (i);
}


/**
 * Ajustar un valor por un factor relativo del valor absoluto. Imita los
 * cálculos en línea de valor = (valor * (den + num)) / den cuando el valor es
 * positivo.
 * \param v Es un puntero al valor a ajustar.
 * \param num Es el numerador del factor relativo. Usar un valor negativo
 * para una disminución en el valor, y un valor positivo para un aumento.
 * \param den Es el denominador del factor relativo. Debe ser positivo.
 * \param minv Es el valor absoluto mínimo de v a usar al calcular el
 * ajuste; usar cero para esto para obtener un ajuste puramente relativo. Debe ser
 * no negativo.
 */
static void adjust_skill_scale(int *v, int num, int den, int minv)
{
	if (num >= 0) {
		*v += (MAX(minv, ABS(*v)) * num) / den;
	} else {
		/*
		 * Para imitar lo que daría (valor * (den + num)) / den para
		 * valor positivo, necesitamos redondear hacia arriba el ajuste.
		 */
		*v -= (MAX(minv, ABS(*v)) * -num + den - 1) / den;
	}
}


/**
 * Calcular el efecto de un cambio de forma en el estado del jugador
 */
static void calc_shapechange(struct player_state *state, bool vuln[ELEM_MAX],
							 struct player_shape *shape,
							 int *blows, int *shots, int *might, int *moves)
{
	int i;

	/* Estadísticas de combate */
	state->to_a += shape->to_a;
	state->to_h += shape->to_h;
	state->to_d += shape->to_d;

	/* Habilidades */
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] += shape->skills[i];
	}

	/* Banderas de objeto */
	of_union(state->flags, shape->flags);

	/* Banderas de jugador */
	pf_union(state->pflags, shape->pflags);

	/* Estadísticas */
	for (i = 0; i < STAT_MAX; i++) {
		state->stat_add[i] += shape->modifiers[i];
	}

	/* Otros modificadores */
	state->skills[SKILL_STEALTH] += shape->modifiers[OBJ_MOD_STEALTH];
	state->skills[SKILL_SEARCH] += (shape->modifiers[OBJ_MOD_SEARCH] * 5);
	state->see_infra += shape->modifiers[OBJ_MOD_INFRA];
	state->skills[SKILL_DIGGING] += (shape->modifiers[OBJ_MOD_TUNNEL] * 20);
	state->speed += shape->modifiers[OBJ_MOD_SPEED];
	state->dam_red += shape->modifiers[OBJ_MOD_DAM_RED];
	*blows += shape->modifiers[OBJ_MOD_BLOWS];
	*shots += shape->modifiers[OBJ_MOD_SHOTS];
	*might += shape->modifiers[OBJ_MOD_MIGHT];
	*moves += shape->modifiers[OBJ_MOD_MOVES];

	/* Resistencias y vulnerabilidades */
	for (i = 0; i < ELEM_MAX; i++) {
		if (shape->el_info[i].res_level == -1) {
			/* Recordar vulnerabilidades para aplicación posterior. */
			vuln[i] = true;
		} else if (shape->el_info[i].res_level
				> state->el_info[i].res_level) {
			/*
			 * De lo contrario, aplicar el nivel de resistencia de la forma si
			 * es mejor; esto está bien porque cualquier vulnerabilidad
			 * no ha sido incluida aún en el nivel de resistencia del estado.
			 */
			state->el_info[i].res_level =
				shape->el_info[i].res_level;
		}
	}
}

/**
 * Calcular el "estado" actual del jugador, teniendo en cuenta
 * no solo las características innatas de raza/clase, sino también los objetos que se llevan puestos
 * y los efectos de hechizos temporales.
 *
 * Ver también calc_mana() y calc_hitpoints().
 *
 * Tomar nota del nuevo "código de velocidad", en particular, un jugador muy
 * fuerte comenzará a ralentizarse tan pronto como alcance las 150 libras,
 * pero no hasta que alcance las 450 libras será la mitad de rápido que
 * un kobold normal. Esto perjudica y ayuda al jugador, perjudica
 * porque en los viejos tiempos un jugador podía simplemente evitar 300 libras,
 * y ayuda porque ahora llevar 300 libras no es muy doloroso.
 *
 * El "arma" y el "arco" *no* añaden a las bonificaciones para golpear o para
 * daño, ya que eso afectaría a cosas no relacionadas con el combate. Estos valores
 * se añaden después, en el lugar apropiado.
 *
 * Si known_only es true, calc_bonuses() solo usará la información conocida
 * de los objetos; por lo tanto, devuelve lo que el jugador _sabe_ que es
 * el estado del personaje.
 */
void calc_bonuses(struct player *p, struct player_state *state, bool known_only,
				  bool update)
{
	int i, j, hold;
	int extra_blows = 0;
	int extra_shots = 0;
	int extra_might = 0;
	int extra_moves = 0;
	struct object *launcher = equipped_item_by_slot_name(p, "shooting");
	struct object *weapon = equipped_item_by_slot_name(p, "weapon");
	bitflag f[OF_SIZE];
	bitflag collect_f[OF_SIZE];
	bool vuln[ELEM_MAX];

	/* Truco para permitir calcular golpes hipotéticos para FUE, DES extra - NRM */
	int str_ind = state->stat_ind[STAT_STR];
	int dex_ind = state->stat_ind[STAT_DEX];

	/* Reiniciar */
	memset(state, 0, sizeof *state);

	/* Establecer varios valores por defecto */
	state->speed = 110;
	state->num_blows = 100;

	/* Extraer información de raza/clase */
	state->see_infra = p->race->infra;
	for (i = 0; i < SKILL_MAX; i++) {
		state->skills[i] = p->race->r_skills[i]	+ p->class->c_skills[i];
	}
	for (i = 0; i < ELEM_MAX; i++) {
		vuln[i] = false;
		if (p->race->el_info[i].res_level == -1) {
			vuln[i] = true;
		} else {
			state->el_info[i].res_level = p->race->el_info[i].res_level;
		}
	}

	/* Pflags base */
	pf_wipe(state->pflags);
	pf_copy(state->pflags, p->race->pflags);
	pf_union(state->pflags, p->class->pflags);

	/* Extraer las banderas del jugador */
	player_flags(p, collect_f);

	/* Analizar equipo */
	for (i = 0; i < p->body.count; i++) {
		int index = 0;
		struct object *obj = slot_object(p, i);
		struct curse_data *curse = obj ? obj->curses : NULL;

		while (obj) {
			int dig = 0;

			/* Extraer las banderas del objeto */
			if (known_only) {
				object_flags_known(obj, f);
			} else {
				object_flags(obj, f);
			}
			of_union(collect_f, f);

			/* Aplicar modificadores */
			state->stat_add[STAT_STR] += obj->modifiers[OBJ_MOD_STR]
				* p->obj_k->modifiers[OBJ_MOD_STR];
			state->stat_add[STAT_INT] += obj->modifiers[OBJ_MOD_INT]
				* p->obj_k->modifiers[OBJ_MOD_INT];
			state->stat_add[STAT_WIS] += obj->modifiers[OBJ_MOD_WIS]
				* p->obj_k->modifiers[OBJ_MOD_WIS];
			state->stat_add[STAT_DEX] += obj->modifiers[OBJ_MOD_DEX]
				* p->obj_k->modifiers[OBJ_MOD_DEX];
			state->stat_add[STAT_CON] += obj->modifiers[OBJ_MOD_CON]
				* p->obj_k->modifiers[OBJ_MOD_CON];
			state->skills[SKILL_STEALTH] += obj->modifiers[OBJ_MOD_STEALTH]
				* p->obj_k->modifiers[OBJ_MOD_STEALTH];
			state->skills[SKILL_SEARCH] += (obj->modifiers[OBJ_MOD_SEARCH] * 5)
				* p->obj_k->modifiers[OBJ_MOD_SEARCH];

			state->see_infra += obj->modifiers[OBJ_MOD_INFRA]
				* p->obj_k->modifiers[OBJ_MOD_INFRA];
			if (tval_is_digger(obj)) {
				if (of_has(obj->flags, OF_DIG_1))
					dig = 1;
				else if (of_has(obj->flags, OF_DIG_2))
					dig = 2;
				else if (of_has(obj->flags, OF_DIG_3))
					dig = 3;
			}
			dig += obj->modifiers[OBJ_MOD_TUNNEL]
				* p->obj_k->modifiers[OBJ_MOD_TUNNEL];
			state->skills[SKILL_DIGGING] += (dig * 20);
			state->speed += obj->modifiers[OBJ_MOD_SPEED]
				* p->obj_k->modifiers[OBJ_MOD_SPEED];
			state->dam_red += obj->modifiers[OBJ_MOD_DAM_RED]
				* p->obj_k->modifiers[OBJ_MOD_DAM_RED];
			extra_blows += obj->modifiers[OBJ_MOD_BLOWS]
				* p->obj_k->modifiers[OBJ_MOD_BLOWS];
			extra_shots += obj->modifiers[OBJ_MOD_SHOTS]
				* p->obj_k->modifiers[OBJ_MOD_SHOTS];
			extra_might += obj->modifiers[OBJ_MOD_MIGHT]
				* p->obj_k->modifiers[OBJ_MOD_MIGHT];
			extra_moves += obj->modifiers[OBJ_MOD_MOVES]
				* p->obj_k->modifiers[OBJ_MOD_MOVES];

			/* Aplicar información de elemento, notando vulnerabilidades para procesamiento posterior */
			for (j = 0; j < ELEM_MAX; j++) {
				if (!known_only || obj->known->el_info[j].res_level) {
					if (obj->el_info[j].res_level == -1)
						vuln[j] = true;

					/* OK porque el nivel de resistencia aún no ha incluido la vulnerabilidad */
					if (obj->el_info[j].res_level > state->el_info[j].res_level)
						state->el_info[j].res_level = obj->el_info[j].res_level;
				}
			}

			/* Aplicar bonificaciones de combate */
			state->ac += obj->ac;
			if (!known_only || obj->known->to_a)
				state->to_a += obj->to_a;
			if (!slot_type_is(p, i, EQUIP_WEAPON)
					&& !slot_type_is(p, i, EQUIP_BOW)) {
				if (!known_only || obj->known->to_h) {
					state->to_h += obj->to_h;
				}
				if (!known_only || obj->known->to_d) {
					state->to_d += obj->to_d;
				}
			}

			/* Moverse a cualquier objeto de maldición no procesado */
			if (curse) {
				index++;
				obj = NULL;
				while (index < z_info->curse_max) {
					if (curse[index].power) {
						obj = curses[index].obj;
						break;
					} else {
						index++;
					}
				}
			} else {
				obj = NULL;
			}
		}
	}

	/* Aplicar las banderas recogidas */
	of_union(state->flags, collect_f);

	/* Añadir información de cambio de forma */
	calc_shapechange(state, vuln, p->shape, &extra_blows, &extra_shots,
		&extra_might, &extra_moves);

	/* Ahora lidiar con las vulnerabilidades */
	for (i = 0; i < ELEM_MAX; i++) {
		if (vuln[i] && (state->el_info[i].res_level < 3))
			state->el_info[i].res_level--;
	}

	/* Calcular luz */
	calc_light(p, state, update);

	/* Sinluz - necesita cambio si se introduce algo que no sea resistencia a la oscuridad */
	if (pf_has(state->pflags, PF_UNLIGHT) && character_dungeon) {
		state->el_info[ELEM_DARK].res_level = 1;
	}

	/* Malvado */
	if (pf_has(state->pflags, PF_EVIL) && character_dungeon) {
		state->el_info[ELEM_NETHER].res_level = 1;
		state->el_info[ELEM_HOLY_ORB].res_level = -1;
	}

	/* Calcular los diversos valores de estadísticas */
	for (i = 0; i < STAT_MAX; i++) {
		int add, use, ind;

		add = state->stat_add[i];
		add += (p->race->r_adj[i] + p->class->c_adj[i]);
		state->stat_top[i] =  modify_stat_value(p->stat_max[i], add);
		use = modify_stat_value(p->stat_cur[i], add);

		state->stat_use[i] = use;

		if (use <= 3) {/* Valores: n/a */
			ind = 0;
		} else if (use <= 18) {/* Valores: 3, 4, ..., 18 */
			ind = (use - 3);
		} else if (use <= 18+219) {/* Rangos: 18/00-18/09, ..., 18/210-18/219 */
			ind = (15 + (use - 18) / 10);
		} else {/* Rango: 18/220+ */
			ind = (37);
		}

		assert((0 <= ind) && (ind < STAT_RANGE));

		/* Truco para golpes hipotéticos - NRM */
		if (!update) {
			if (i == STAT_STR) {
				ind += str_ind;
				ind = MIN(ind, 37);
				ind = MAX(ind, 3);
			} else if (i == STAT_DEX) {
				ind += dex_ind;
				ind = MIN(ind, 37);
				ind = MAX(ind, 3);
			}
		}

		/* Guardar el nuevo índice */
		state->stat_ind[i] = ind;
	}

	/* Efectos de la comida fuera del rango "Alimentado" */
	if (!player_timed_grade_eq(p, TMD_FOOD, "Alimentado")) {
		int excess = p->timed[TMD_FOOD] - PY_FOOD_FULL;
		int lack = PY_FOOD_HUNGRY - p->timed[TMD_FOOD];
		if ((excess > 0) && !p->timed[TMD_ATT_VAMP]) {
			/* Escalar a unidades 1/10 del rango y restar de la velocidad */
			excess = (excess * 10) / (PY_FOOD_MAX - PY_FOOD_FULL);
			state->speed -= excess;
		} else if (lack > 0) {
			/* Escalar a unidades 1/20 del rango */
			lack = (lack * 20) / PY_FOOD_HUNGRY;

			/* Aplicar efectos progresivamente */
			state->to_h -= lack;
			state->to_d -= lack;
			if ((lack > 10) && (lack <= 15)) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-1, 10, 0);
			} else if ((lack > 15) && (lack <= 18)) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-1, 5, 0);
				state->skills[SKILL_DISARM_PHYS] *= 9;
				state->skills[SKILL_DISARM_PHYS] /= 10;
				state->skills[SKILL_DISARM_MAGIC] *= 9;
				state->skills[SKILL_DISARM_MAGIC] /= 10;
			} else if (lack > 18) {
				adjust_skill_scale(&state->skills[SKILL_DEVICE],
					-3, 10, 0);
				state->skills[SKILL_DISARM_PHYS] *= 8;
				state->skills[SKILL_DISARM_PHYS] /= 10;
				state->skills[SKILL_DISARM_MAGIC] *= 8;
				state->skills[SKILL_DISARM_MAGIC] /= 10;
				state->skills[SKILL_SAVE] *= 9;
				state->skills[SKILL_SAVE] /= 10;
				state->skills[SKILL_SEARCH] *=9;
				state->skills[SKILL_SEARCH] /= 10;
			}
		}
	}

	/* Otros efectos temporales */
	player_flags_timed(p, state->flags);

	if (player_timed_grade_eq(p, TMD_STUN, "Aturdimiento Fuerte")) {
		state->to_h -= 20;
		state->to_d -= 20;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
		if (update) {
			p->timed[TMD_FASTCAST] = 0;
		}
	} else if (player_timed_grade_eq(p, TMD_STUN, "Aturdimiento")) {
		state->to_h -= 5;
		state->to_d -= 5;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
		if (update) {
			p->timed[TMD_FASTCAST] = 0;
		}
	}
	if (p->timed[TMD_INVULN]) {
		state->to_a += 100;
	}
	if (p->timed[TMD_BLESSED]) {
		state->to_a += 5;
		state->to_h += 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->timed[TMD_SHIELD]) {
		state->to_a += 50;
	}
	if (p->timed[TMD_STONESKIN]) {
		state->to_a += 40;
		state->speed -= 5;
	}
	if (p->timed[TMD_HERO]) {
		state->to_h += 12;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], 1, 20, 0);
	}
	if (p->timed[TMD_SHERO]) {
		state->skills[SKILL_TO_HIT_MELEE] += 75;
		state->to_a -= 10;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 10, 0);
	}
	if (p->timed[TMD_FAST] || p->timed[TMD_SPRINT]) {
		state->speed += 10;
	}
	if (p->timed[TMD_SLOW]) {
		state->speed -= 10;
	}
	if (p->timed[TMD_SINFRA]) {
		state->see_infra += 5;
	}
	if (p->timed[TMD_TERROR]) {
		state->speed += 10;
	}
	for (i = 0; i < TMD_MAX; ++i) {
		if (p->timed[i] && timed_effects[i].temp_resist != -1
				&& state->el_info[timed_effects[i].temp_resist].res_level
				< 2) {
			state->el_info[timed_effects[i].temp_resist].res_level++;
		}
	}
	if (p->timed[TMD_CONFUSED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 4, 0);
	}
	if (p->timed[TMD_AMNESIA]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->timed[TMD_POISONED]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}
	if (p->timed[TMD_IMAGE]) {
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 5, 0);
	}
	if (p->timed[TMD_BLOODLUST]) {
		state->to_d += p->timed[TMD_BLOODLUST] / 2;
		extra_blows += p->timed[TMD_BLOODLUST] / 20;
	}
	if (p->timed[TMD_STEALTH]) {
		state->skills[SKILL_STEALTH] += 10;
	}

	/* Analizar banderas - comprobar miedo */
	if (of_has(state->flags, OF_AFRAID)) {
		state->to_h -= 20;
		state->to_a += 8;
		adjust_skill_scale(&state->skills[SKILL_DEVICE], -1, 20, 0);
	}

	/* Analizar peso */
	j = p->upkeep->total_weight;
	i = weight_limit(state);
	if (j > i / 2)
		state->speed -= ((j - (i / 2)) / (i / 10));
	if (state->speed < 0)
		state->speed = 0;
	if (state->speed > 199)
		state->speed = 199;

	/* Aplicar bonificaciones de modificador (desinflar bonificaciones de estadísticas) */
	state->to_a += adj_dex_ta[state->stat_ind[STAT_DEX]];
	state->to_d += adj_str_td[state->stat_ind[STAT_STR]];
	state->to_h += adj_dex_th[state->stat_ind[STAT_DEX]];
	state->to_h += adj_str_th[state->stat_ind[STAT_STR]];


	/* Modificar habilidades */
	state->skills[SKILL_DISARM_PHYS] += adj_dex_dis[state->stat_ind[STAT_DEX]];
	state->skills[SKILL_DISARM_MAGIC] += adj_int_dis[state->stat_ind[STAT_INT]];
	state->skills[SKILL_DEVICE] += adj_int_dev[state->stat_ind[STAT_INT]];
	state->skills[SKILL_SAVE] += adj_wis_sav[state->stat_ind[STAT_WIS]];
	state->skills[SKILL_DIGGING] += adj_str_dig[state->stat_ind[STAT_STR]];
	for (i = 0; i < SKILL_MAX; i++)
		state->skills[i] += (p->class->x_skills[i] * p->lev / 10);

	if (state->skills[SKILL_DIGGING] < 1) state->skills[SKILL_DIGGING] = 1;
	if (state->skills[SKILL_STEALTH] > 30) state->skills[SKILL_STEALTH] = 30;
	if (state->skills[SKILL_STEALTH] < 0) state->skills[SKILL_STEALTH] = 0;
	hold = adj_str_hold[state->stat_ind[STAT_STR]];


	/* Analizar lanzador */
	state->heavy_shoot = false;
	if (launcher) {
		int16_t launcher_weight = object_weight_one(launcher);

		if (hold < launcher_weight / 10) {
			state->to_h += 2 * (hold - launcher_weight / 10);
			state->heavy_shoot = true;
		}

		state->num_shots = 10;

		/* Tipo de munición */
		if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_SHOTS))
			state->ammo_tval = TV_SHOT;
		else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_ARROWS))
			state->ammo_tval = TV_ARROW;
		else if (kf_has(launcher->kind->kind_flags, KF_SHOOTS_BOLTS))
			state->ammo_tval = TV_BOLT;

		/* Multiplicador */
		state->ammo_mult = launcher->pval;

		/* Aplicar banderas especiales */
		if (!state->heavy_shoot) {
			state->num_shots += extra_shots;
			state->ammo_mult += extra_might;
			if (pf_has(state->pflags, PF_FAST_SHOT)) {
				state->num_shots += p->lev / 3;
			}
		}

		/* Requerir al menos un disparo */
		if (state->num_shots < 10) state->num_shots = 10;
	}


	/* Analizar arma */
	state->heavy_wield = false;
	state->bless_wield = false;
	if (weapon) {
		int16_t weapon_weight = object_weight_one(weapon);

		/* Es difícil sostener un arma pesada */
		if (hold < weapon_weight / 10) {
			state->to_h += 2 * (hold - weapon_weight / 10);
			state->heavy_wield = true;
		}

		/* Armas normales */
		if (!state->heavy_wield) {
			state->num_blows = calc_blows(p, weapon, state, extra_blows);
			state->skills[SKILL_DIGGING] += weapon_weight / 10;
		}

		/* Bonificación de arma divina para armas benditas */
		if (pf_has(state->pflags, PF_BLESS_WEAPON)
				&& (weapon->tval == TV_HAFTED
				|| of_has(state->flags, OF_BLESSED))) {
			state->to_d += 2;
			state->bless_wield = true;
		}
	} else {
		/* Sin armas */
		state->num_blows = calc_blows(p, NULL, state, extra_blows);
	}

	/* Maná */
	calc_mana(p, state, update);
	if (!p->msp) {
		pf_on(state->pflags, PF_NO_MANA);
	}

	/* Velocidad de movimiento */
	state->num_moves = extra_moves;

	return;
}

/**
 * Calcular bonificaciones e imprimir varias cosas al cambiar.
 */
static void update_bonuses(struct player *p)
{
	int i;

	struct player_state state = p->state;
	struct player_state known_state = p->known_state;


	/* ------------------------------------
	 * Calcular bonificaciones
	 * ------------------------------------ */

	calc_bonuses(p, &state, false, true);
	calc_bonuses(p, &known_state, true, true);


	/* ------------------------------------
	 * Notar cambios
	 * ------------------------------------ */

	/* Analizar estadísticas */
	for (i = 0; i < STAT_MAX; i++) {
		/* Notar cambios */
		if (state.stat_top[i] != p->state.stat_top[i])
			/* Redibujar las estadísticas después */
			p->upkeep->redraw |= (PR_STATS);

		/* Notar cambios */
		if (state.stat_use[i] != p->state.stat_use[i])
			/* Redibujar las estadísticas después */
			p->upkeep->redraw |= (PR_STATS);

		/* Notar cambios */
		if (state.stat_ind[i] != p->state.stat_ind[i]) {
			/* El cambio en CON afecta a los Puntos de Golpe */
			if (i == STAT_CON)
				p->upkeep->update |= (PU_HP);

			/* El cambio en estadísticas puede afectar Maná/Hechizos */
			p->upkeep->update |= (PU_MANA | PU_SPELLS);
		}
	}


	/* Cambio de Telepatía */
	if (of_has(state.flags, OF_TELEPATHY) !=
		of_has(p->state.flags, OF_TELEPATHY))
		/* Actualizar visibilidad de monstruos */
		p->upkeep->update |= (PU_MONSTERS);
	/* Cambio de Ver Invisible */
	if (of_has(state.flags, OF_SEE_INVIS) !=
		of_has(p->state.flags, OF_SEE_INVIS))
		/* Actualizar visibilidad de monstruos */
		p->upkeep->update |= (PU_MONSTERS);

	/* Redibujar velocidad (si es necesario) */
	if (state.speed != p->state.speed)
		p->upkeep->redraw |= (PR_SPEED);

	/* Redibujar armadura (si es necesario) */
	if ((known_state.ac != p->known_state.ac) || 
		(known_state.to_a != p->known_state.to_a))
		p->upkeep->redraw |= (PR_ARMOR);

	/* Notar cambios en el "radio de luz" */
	if (p->state.cur_light != state.cur_light) {
		/* Actualizar los visuales */
		p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
	}

	/* Notar cambios en el límite de peso. */
	if (weight_limit(&p->state) != weight_limit(&state)) {
		p->upkeep->redraw |= (PR_INVEN);
	}

	/* Manejar modo parcial */
	if (!p->upkeep->only_partial) {
		/* Tomar nota cuando cambia "arco pesado" */
		if (p->state.heavy_shoot != state.heavy_shoot) {
			/* Mensaje */
			if (state.heavy_shoot)
				msg("Tienes problemas para usar un arco tan pesado.");
			else if (equipped_item_by_slot_name(p, "shooting"))
				msg("No tienes problemas para usar tu arco.");
			else
				msg("Te sientes aliviado al dejar tu arco pesado.");
		}

		/* Tomar nota cuando cambia "arma pesada" */
		if (p->state.heavy_wield != state.heavy_wield) {
			/* Mensaje */
			if (state.heavy_wield)
				msg("Tienes problemas para empuñar un arma tan pesada.");
			else if (equipped_item_by_slot_name(p, "weapon"))
				msg("No tienes problemas para empuñar tu arma.");
			else
				msg("Te sientes aliviado al dejar tu arma pesada.");	
		}

		/* Tomar nota cuando cambia "arma bendita" */
		if (p->state.bless_wield != state.bless_wield) {
			/* Mensaje */
			if (state.bless_wield) {
				msg("Te sientes en sintonía con tu arma.");
			} else if (equipped_item_by_slot_name(p, "weapon")) {
				msg("Te sientes menos en sintonía con tu arma.");
			}
		}

		/* Tomar nota cuando cambia "estado de armadura" */
		if (p->state.cumber_armor != state.cumber_armor) {
			/* Mensaje */
			if (state.cumber_armor)
				msg("El peso de tu armadura reduce tus PM máximos.");
			else
				msg("Tus PM máximos ya no se ven reducidos por el peso de la armadura.");
		}
	}

	memcpy(&p->state, &state, sizeof(state));
	memcpy(&p->known_state, &known_state, sizeof(known_state));
}




/**
 * ------------------------------------------------------------------------
 * Funciones de rastreo de monstruos y objetos
 * ------------------------------------------------------------------------ */

/**
 * Rastrear el monstruo dado
 */
void health_track(struct player_upkeep *upkeep, struct monster *mon)
{
	upkeep->health_who = mon;
	upkeep->redraw |= PR_HEALTH;
}

/**
 * Rastrear la raza de monstruo dada
 */
void monster_race_track(struct player_upkeep *upkeep, struct monster_race *race)
{
	/* Guardar este ID de monstruo */
	upkeep->monster_race = race;

	/* Cosas de ventana */
	upkeep->redraw |= (PR_MONSTER);
}

/**
 * Rastrear el objeto dado
 */
void track_object(struct player_upkeep *upkeep, struct object *obj)
{
	upkeep->object = obj;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Rastrear el tipo de objeto dado
 */
void track_object_kind(struct player_upkeep *upkeep, struct object_kind *kind)
{
	upkeep->object = NULL;
	upkeep->object_kind = kind;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * Cancelar todo el rastreo de objetos
 */
void track_object_cancel(struct player_upkeep *upkeep)
{
	upkeep->object = NULL;
	upkeep->object_kind = NULL;
	upkeep->redraw |= (PR_OBJECT);
}

/**
 * ¿Es el objeto dado el rastreado?
 */
bool tracked_object_is(struct player_upkeep *upkeep, struct object *obj)
{
	return (upkeep->object == obj);
}



/**
 * ------------------------------------------------------------------------
 * Funciones genéricas de "lidiar con"
 * ------------------------------------------------------------------------ */

/**
 * Manejar "player->upkeep->notice"
 */
void notice_stuff(struct player *p)
{
	/* Cosas a notar */
	if (!p->upkeep->notice) return;

	/* Lidiar con cosas de ignorar */
	if (p->upkeep->notice & PN_IGNORE) {
		p->upkeep->notice &= ~(PN_IGNORE);
		ignore_drop(p);
	}

	/* Combinar la mochila */
	if (p->upkeep->notice & PN_COMBINE) {
		p->upkeep->notice &= ~(PN_COMBINE);
		combine_pack(p);
	}

	/* Volcar los mensajes de monstruos */
	if (p->upkeep->notice & PN_MON_MESSAGE) {
		p->upkeep->notice &= ~(PN_MON_MESSAGE);

		/* Asegurarse de que esto viene después de todos los mensajes de monstruos */
		show_monster_messages();
	}
}

/**
 * Manejar "player->upkeep->update"
 */
void update_stuff(struct player *p)
{
	/* Cosas a actualizar */
	if (!p->upkeep->update) return;


	if (p->upkeep->update & (PU_INVEN)) {
		p->upkeep->update &= ~(PU_INVEN);
		calc_inventory(p);
	}

	if (p->upkeep->update & (PU_BONUS)) {
		p->upkeep->update &= ~(PU_BONUS);
		update_bonuses(p);
	}

	if (p->upkeep->update & (PU_TORCH)) {
		p->upkeep->update &= ~(PU_TORCH);
		calc_light(p, &p->state, true);
	}

	if (p->upkeep->update & (PU_HP)) {
		p->upkeep->update &= ~(PU_HP);
		calc_hitpoints(p);
	}

	if (p->upkeep->update & (PU_MANA)) {
		p->upkeep->update &= ~(PU_MANA);
		calc_mana(p, &p->state, true);
	}

	if (p->upkeep->update & (PU_SPELLS)) {
		p->upkeep->update &= ~(PU_SPELLS);
		if (p->class->magic.total_spells > 0) {
			calc_spells(p);
		}
	}

	/* El personaje aún no está listo, no hay actualizaciones de mapa */
	if (!character_generated) return;

	/* El mapa no se muestra, no hay actualizaciones de mapa */
	if (!map_is_visible()) return;

	if (p->upkeep->update & (PU_UPDATE_VIEW)) {
		p->upkeep->update &= ~(PU_UPDATE_VIEW);
		update_view(cave, p);
	}

	if (p->upkeep->update & (PU_DISTANCE)) {
		p->upkeep->update &= ~(PU_DISTANCE);
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(true);
	}

	if (p->upkeep->update & (PU_MONSTERS)) {
		p->upkeep->update &= ~(PU_MONSTERS);
		update_monsters(false);
	}


	if (p->upkeep->update & (PU_PANEL)) {
		p->upkeep->update &= ~(PU_PANEL);
		event_signal(EVENT_PLAYERMOVED);
	}
}



struct flag_event_trigger
{
	uint32_t flag;
	game_event_type event;
};



/**
 * Eventos desencadenados por las diversas banderas.
 */
static const struct flag_event_trigger redraw_events[] =
{
	{ PR_MISC,    EVENT_RACE_CLASS },
	{ PR_TITLE,   EVENT_PLAYERTITLE },
	{ PR_LEV,     EVENT_PLAYERLEVEL },
	{ PR_EXP,     EVENT_EXPERIENCE },
	{ PR_STATS,   EVENT_STATS },
	{ PR_ARMOR,   EVENT_AC },
	{ PR_HP,      EVENT_HP },
	{ PR_MANA,    EVENT_MANA },
	{ PR_GOLD,    EVENT_GOLD },
	{ PR_HEALTH,  EVENT_MONSTERHEALTH },
	{ PR_DEPTH,   EVENT_DUNGEONLEVEL },
	{ PR_SPEED,   EVENT_PLAYERSPEED },
	{ PR_STATE,   EVENT_STATE },
	{ PR_STATUS,  EVENT_STATUS },
	{ PR_STUDY,   EVENT_STUDYSTATUS },
	{ PR_DTRAP,   EVENT_DETECTIONSTATUS },
	{ PR_FEELING, EVENT_FEELING },
	{ PR_LIGHT,   EVENT_LIGHT },

	{ PR_INVEN,   EVENT_INVENTORY },
	{ PR_EQUIP,   EVENT_EQUIPMENT },
	{ PR_MONLIST, EVENT_MONSTERLIST },
	{ PR_ITEMLIST, EVENT_ITEMLIST },
	{ PR_MONSTER, EVENT_MONSTERTARGET },
	{ PR_OBJECT, EVENT_OBJECTTARGET },
	{ PR_MESSAGE, EVENT_MESSAGE },
};

/**
 * Manejar "player->upkeep->redraw"
 */
void redraw_stuff(struct player *p)
{
	size_t i;
	uint32_t redraw = p->upkeep->redraw;

	/* Cosas a redibujar */
	if (!redraw) return;

	/* El personaje aún no está listo, no hay actualizaciones de pantalla */
	if (!character_generated) return;

	/* El mapa no se muestra, solo actualizaciones de subventanas */
	if (!map_is_visible()) 
		redraw &= PR_SUBWINDOW;

	/* Truco - raramente actualizar mientras descansas o corres, lo hace más rápido */
	if (((player_resting_count(p) % 100) || (p->upkeep->running % 100))
		&& !(redraw & (PR_MESSAGE | PR_MAP)))
		return;

	/* Para cada bandera listada, enviar la señal apropiada a la UI */
	for (i = 0; i < N_ELEMENTS(redraw_events); i++) {
		const struct flag_event_trigger *hnd = &redraw_events[i];

		if (redraw & hnd->flag)
			event_signal(hnd->event);
	}

	/* Luego las que requieren que se proporcionen parámetros. */
	if (redraw & PR_MAP) {
		/* Marcar todo el mapa para ser redibujado */
		event_signal_point(EVENT_MAP, -1, -1);
	}

	p->upkeep->redraw &= ~redraw;

	/* El mapa no se muestra, solo actualizaciones de subventanas */
	if (!map_is_visible()) return;

	/*
	 * Hacer cualquier trazado, etc., retrasado de antes - este conjunto de actualizaciones
	 * ha terminado.
	 */
	event_signal(EVENT_END);
}


/**
 * Manejar "player->upkeep->update" y "player->upkeep->redraw"
 */
void handle_stuff(struct player *p)
{
	if (p->upkeep->update) update_stuff(p);
	if (p->upkeep->redraw) redraw_stuff(p);
}