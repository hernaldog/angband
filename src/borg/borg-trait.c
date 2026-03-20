/**
 * \archivo borg-trait.c
 * \brief Los cálculos para determinar qué objetos y habilidades tiene
 *        Este código generalmente carga los arrays (borg.trait/has/activation)
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 *
 * Este trabajo es software libre; puedes redistribuirlo y/o modificarlo
 * bajo los términos de:
 *
 * a) la Licencia Pública General de GNU publicada por la Free Software
 *    Foundation, versión 2, o
 *
 * b) la "Licencia Angband":
 *    Este software puede ser copiado y distribuido con fines educativos, de
 *    investigación y sin ánimo de lucro siempre que se incluyan este copyright
 *    y esta declaración en todas las copias. Pueden aplicarse otros derechos de autor.
 */

#include "borg-trait.h"

#ifdef ALLOW_BORG

#include "../effects.h"
#include "../obj-util.h"
#include "../player-calcs.h"
#include "../player-timed.h"
#include "../player-util.h"

#include "borg-flow.h"
#include "borg-flow-kill.h"
#include "borg-item-activation.h"
#include "borg-item-analyze.h"
#include "borg-item-id.h"
#include "borg-item-use.h"
#include "borg-item-val.h"
#include "borg-item-wear.h"
#include "borg-magic.h"
#include "borg-think.h"
#include "borg-trait-swap.h"
#include "borg.h"
#include "borg-home-notice.h"

/* GRAN TRUCO copiado porque es estático en el código principal */
/* Haría que no fueran estáticos pero intento no cambiar el código base */
/* por ahora !FIX !TODO */
static const int borg_adj_mag_mana[STAT_RANGE] = {
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

static const int borg_adj_dex_ta[STAT_RANGE] = {
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
 * Tabla de Estadísticas (STR) -- bonificación al daño
 */
const int borg_adj_str_td[STAT_RANGE] = {
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
 * Tabla de Estadísticas (DEX) -- bonificación al golpe
 */
const int borg_adj_dex_th[STAT_RANGE] = {
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
 * Tabla de Estadísticas (STR) -- bonificación al golpe
 */
static const int borg_adj_str_th[STAT_RANGE] = {
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

static const int borg_adj_dex_blow[STAT_RANGE] = {
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

static const int borg_blows_table[12][12] = {
    /* P */
   /* D:   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11+ */
   /* DEX: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */

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
    /* DEX: 3,   10,  17,  /20, /40, /60, /80, /100,/120,/150,/180,/200 */
};

static const int borg_adj_dex_dis[STAT_RANGE] = {
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

static const int borg_adj_int_dis[STAT_RANGE] = {
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

static const int borg_adj_int_dev[STAT_RANGE] = {
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

static const int borg_adj_str_dig[STAT_RANGE] = {
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

static const int borg_adj_wis_sav[STAT_RANGE] = {
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

static const int borg_adj_str_wgt[STAT_RANGE] = {
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

static const int borg_adj_con_mhp[STAT_RANGE] = {
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

/**
 * Tabla de Estadísticas (INT/WIS) -- Porcentaje mínimo de fallo
 */
static const int borg_adj_mag_fail[STAT_RANGE] = {
    99 /* 3 */,
    99 /* 4 */,
    99 /* 5 */,
    99 /* 6 */,
    99 /* 7 */,
    50 /* 8 */,
    30 /* 9 */,
    20 /* 10 */,
    15 /* 11 */,
    12 /* 12 */,
    11 /* 13 */,
    10 /* 14 */,
    9 /* 15 */,
    8 /* 16 */,
    7 /* 17 */,
    6 /* 18/00-18/09 */,
    6 /* 18/10-18/19 */,
    5 /* 18/20-18/29 */,
    5 /* 18/30-18/39 */,
    5 /* 18/40-18/49 */,
    4 /* 18/50-18/59 */,
    4 /* 18/60-18/69 */,
    4 /* 18/70-18/79 */,
    4 /* 18/80-18/89 */,
    3 /* 18/90-18/99 */,
    3 /* 18/100-18/109 */,
    2 /* 18/110-18/119 */,
    2 /* 18/120-18/129 */,
    2 /* 18/130-18/139 */,
    2 /* 18/140-18/149 */,
    1 /* 18/150-18/159 */,
    1 /* 18/160-18/169 */,
    1 /* 18/170-18/179 */,
    1 /* 18/180-18/189 */,
    1 /* 18/190-18/199 */,
    0 /* 18/200-18/209 */,
    0 /* 18/210-18/219 */,
    0 /* 18/220+ */
};

/**
 * Tabla de Estadísticas (INT/WIS) -- ajuste de la tasa de fallo
 */
static const int borg_adj_mag_stat[STAT_RANGE] = {
    -5 /* 3 */,
    -4 /* 4 */,
    -3 /* 5 */,
    -3 /* 6 */,
    -2 /* 7 */,
    -1 /* 8 */,
    0 /* 9 */,
    0 /* 10 */,
    0 /* 11 */,
    0 /* 12 */,
    0 /* 13 */,
    1 /* 14 */,
    2 /* 15 */,
    3 /* 16 */,
    4 /* 17 */,
    5 /* 18/00-18/09 */,
    6 /* 18/10-18/19 */,
    7 /* 18/20-18/29 */,
    8 /* 18/30-18/39 */,
    9 /* 18/40-18/49 */,
    10 /* 18/50-18/59 */,
    11 /* 18/60-18/69 */,
    12 /* 18/70-18/79 */,
    15 /* 18/80-18/89 */,
    18 /* 18/90-18/99 */,
    21 /* 18/100-18/109 */,
    24 /* 18/110-18/119 */,
    27 /* 18/120-18/129 */,
    30 /* 18/130-18/139 */,
    33 /* 18/140-18/149 */,
    36 /* 18/150-18/159 */,
    39 /* 18/160-18/169 */,
    42 /* 18/170-18/179 */,
    45 /* 18/180-18/189 */,
    48 /* 18/190-18/199 */,
    51 /* 18/200-18/209 */,
    54 /* 18/210-18/219 */,
    57 /* 18/220+ */
};

/*
 * Toda la información que el borg sabe sobre sí mismo
 */
struct borg_struct borg;

/*
 * Variables objetivo
 */
bool borg_simulate; /* Bandera de simulación */
bool borg_attacking; /* Bandera de simulación */

/* banderas de defensa */
bool borg_on_glyph; /* borg está parado sobre un glifo de protección */
bool borg_create_door; /* borg va a crear puertas */
bool borg_sleep_spell;
bool borg_sleep_spell_ii;
bool borg_crush_spell;
bool borg_slow_spell; /* borg está a punto de lanzar el hechizo */
bool borg_confuse_spell;
bool borg_fear_mon_spell;

int16_t borg_game_ratio; /* la proporción de tiempo borg vs tiempo de juego */

/* NOTA: Esto debe coincidir exactamente con el enum en borg-trait.h */
const char *prefix_pref[] = {
    /* atributos personales */
    "str",
    "int",
    "wis",
    "dex",
    "con",
    "str adj",
    "int adj",
    "wis adj",
    "dex adj",
    "con adj",
    "cur str",
    "cur int",
    "cur wis",
    "cur dex",
    "cur con",
    "str index",
    "int index",
    "wis index",
    "dex index",
    "con index",
    "sust str",
    "sust int",
    "sust wis",
    "sust dex",
    "sust con",
    "class",
    "light",
    "cur hp",
    "max hp",
    "hp adj",
    "cur sp",
    "max sp",
    "sp adj",
    "SFAIL1",
    "SFAIL2",
    "clevel",
    "max clevel",
    "esp",
    "recall",
    "food",
    "food high",
    "food low",
    "food cure conf",
    "food cure blind",
    "speed",
    "gold",
    "extra moves",
    "damage reduction",
    "slow dig",
    "feather fall",
    "regen",
    "see inv",
    "infravision",
    "fast shots",
    "disarm ph",
    "disarm mag",
    "use device",
    "save",
    "stealth",
    "search",
    "to hit normal",
    "to hit bow",
    "to hit throw",
    "dig",
    "immune fire",
    "immune acid",
    "immune cold",
    "immune elec",
    "immune poison",
    "resist fire",
    "resist cold",
    "resist elec",
    "resist acid",
    "resist poison",
    "resist fear",
    "resist lite",
    "resist dark",
    "resist blind",
    "resist conf",
    "resist sound",
    "resist shards",
    "resist nexus",
    "resist nether",
    "resist chaos",
    "resist dis",
    "hold life",
    "free action",
    "resist fire with swap", /* igual que sin S pero incluye swap */
    "resist cold with swap",
    "resist elec with swap",
    "resist acid with swap",
    "resist poison with swap",
    "resist fear with swap",
    "resist lite with swap",
    "resist dark with swap",
    "resist blind with swap",
    "resist conf with swap",
    "resist sound with swap",
    "resist shards with swap",
    "resist nexus with swap",
    "resist nether with swap",
    "resist chaos with swap",
    "resist dis with swap",
    "hold life with swap",
    "free action with swap",

    /* variable extra aleatoria */
    "depth", /* profundidad actual del borg ? */
    "max depth", /* profundidad de recuerdo */
    "king", /* borg ha ganado */

    /* cosas de estado del jugador */
    "is weak",
    "is hungry",
    "is full",
    "is gorged",
    "is blind",
    "is afraid",
    "is confused",
    "is poisoned",
    "is cut",
    "is stun",
    "is heavystun",
    "is paralyzed",
    "is image",
    "is forget",
    "is encumb",
    "is study",
    "is fixlev",
    "is fixexp",
    "has fixexp",
    "is fixstr",
    "is fixint",
    "is fixwis",
    "is fixdex",
    "is fixcon",
    "is fixall",

    /* algunas cosas de combate */
    "armor",
    "to hit", /* golpe base, no incluye el arma */
    "to damage", /* daño base, no incluye el arma */
    "wep to hit", /* golpe del arma */
    "wep to damage", /* daño del arma */
    "wep id", /* arma identificada */
    "wep damage dice", /* dado de daño del arma */
    "wep damage sides", /* caras del dado de daño del arma */
    "bow id", /* arma identificada */
    "bow to hit", /* golpe del arco */
    "bow to damage", /* daño del arco */
    "bow is sling",
    "bow artifact",
    "blows",
    "EXTRA_BLOWS",
    "shots",
    "heavy weapon",
    "heavy bow",
    "ammo count", /* recuento de todas las municiones */
    "ammo tval",
    "ammo sides",
    "ammo power",
    "amt missiles", /* solo el recuento de las que sirven para tu arco actual */
    "amt ego missiles", /* y son ego */
    "amt cursed missiles", /* y están malditas */
    "quiver slots", /* número de espacios de inventario que ocupan los objetos en el carcaj */
    "first cursed", /* primer objeto maldito */
    "where cursed", /* dónde están las maldiciones 1 inv, 2 equ, 4 quiv */

    /* maldiciones */
    "enveloping",
    "irritation",
    "teleport",
    "curse poison",
    "siren",
    "hallucinate",
    "paralysis",
    "summon demon",
    "summon dragon",
    "summon undead",
    "curse stone",
    "no teleport",
    "treach wep",
    "aggravate",
    "vulnerable",
    "dullness",
    "sickness",
    "weakness",
    "clumsiness",
    "slowness",
    "annoyance",
    "impair hp", /* Recuperación de HP deteriorada */
    "CRSMPIMP", /* Recuperación de MP deteriorada */
    "curse steel",
    "air swing",
    "fear", /* Bandera de maldición de miedo */
    "drain xp", /* Bandera de drenar XP */
    "vuln fire", /* Vulnerable al fuego */
    "vuln elec", /* Vulnerable a la electricidad */
    "vuln cold", /* Vulnerable al frío */
    "vuln acid", /* Vulnerable al ácido */
    "unknown curse",

    /* atributos del arma */
    "wep slay animal", /* WS = el arma mata */
    "wep slay evil",
    "wep slay undead",
    "wep slay demon",
    "wep slay orc",
    "wep slay troll",
    "wep slay giant",
    "wep slay dragon",
    "wep kill undead", /* WK = el arma aniquila */
    "wep kill demon",
    "wep kill dragon",
    "wep impact",
    "wep brand acid", /* WB = El arma está imbuida con */
    "wep brand elec",
    "wep brand fire",
    "wep brand cold",
    "wep brand poison",

    /* cantidades */
    "amt phase",
    "amt teleport", /* todas las fuentes de teletransporte */
    "amt escape", /* Bastón, artefacto (se puede usar ciego/confundido) */
    "fuel",
    "amt heal",
    "amt ezheal",
    "amt life",
    "amt id",
    "amt speed",
    "amt staff magi", /* Cantidad de Cargas de Bastón */
    "amt staff destruction",
    "amt teleport other", /* ¿Cuántas cargas de Teletransportar Otro tienes? */
    "amt cure poison",
    "amt detect traps",
    "amt detect door",
    "amt detect evil",
    "amt magic map",
    "amt recharge",
    "amt call lite",
    "amt prot evil", /* Protección contra el Mal */
    "amt glyph", /* Protección Rúnica */
    "amt potion ccw", /* Pociones CCW (solo porque las usamos a menudo) */
    "amt potion csw", /* Pociones CSW (+ CLW si está cortado) */
    "amt potion clw",
    "amt ench to hit", /* encantar armas y armaduras (+hechizos) */
    "amt ench to dam",
    "amt *ench to wep*",
    "amt ench to armor",
    "amt *ench to armor*",
    "amt brand",
    "need ench to armor",
    "need ench to hit",
    "need ench to dam",
    "need brand",
    "amt resist heat", /* pociones de res calor */
    "amt resist cold", /* poc de res frío */
    "amt resist poison", /* Pociones de Res Veneno */
    "amt teleport level", /* pergamino de teletransporte de nivel */
    "holy word", /* Oración Palabra Santa Legal*/
    "mass banishment", /* ?Destierro Masivo */
    "amt cool shroom", /* Número de setas geniales */
    "amt attack rods1", /* Varitas de ataque */
    "amt attack rods2", /* Varitas de ataque */
    "worn need id", /* un objeto equipado que necesita ID */
    "amt need id", /* número de ids necesarios (equipados o no) */
    "amt diggers", /* cantidad de excavadores en el inventario */
    "amt good staff chg", /* cargas de bastón buenas */
    "amt good wand chg", /* cargas de varita buenas */
    "multi bonus", /* Objetos con múltiples bonificaciones útiles */
    "detect inv", /* Ver Invisibilidad es Legal */
    "weight", /* peso de todo el inventario y equipo */
    "carry", /* capacidad de carga basada en str */
    "empty slots", /* número de espacios vacíos */
    "sauron dead",
    "prep big fight",
    NULL
};

/*
 * auxiliar para desactivar objetos de intercambio cuando está a más de profundidad 90.
 */
bool borg_uses_swaps(void)
{
    return borg_cfg[BORG_USES_SWAPS] && borg.trait[BI_MAXDEPTH] < 90;
}

/**
 * Calcular los golpes que recibiría un jugador.
 *
 * copiado y ajustado de player-calcs.c
 */
int borg_calc_blows(borg_item *item)
{
    int blows;
    int str_index, dex_index;
    int div;
    int blow_energy;

    int weight     = item->weight * item->iqty;
    int min_weight = player->class->min_weight;

    /* Imponer un "peso" mínimo (décimas de libra) */
    div = (weight < min_weight) ? min_weight : weight;

    /* Obtener la fuerza vs el peso */
    str_index = adj_str_blow[borg.trait[BI_STR_INDEX]]
                * player->class->att_multiply / div;

    /* Valor máximo */
    if (str_index > 11)
        str_index = 11;

    /* Indexar por destreza */
    dex_index = MIN(borg_adj_dex_blow[borg.trait[BI_DEX_INDEX]], 11);

    /* Usar la tabla de golpes para obtener energía por golpe */
    blow_energy = borg_blows_table[str_index][dex_index];

    blows = MIN((10000 / blow_energy), (100 * player->class->max_attacks));

    /* Requerir al menos un golpe, dos para combate O */
    return (MAX(blows
                    + (100
                        * (item->modifiers[OBJ_MOD_BLOWS]
                            + borg.trait[BI_EXTRA_BLOWS])),
               OPT(player, birth_percent_damage) ? 200 : 100))
           / 100;
}

/*
 * Nótese que asumimos que cualquier objeto con cantidad cero no existe,
 * por lo tanto, al simular mundos posibles, no tenemos que
 * "optimizar" los espacios vacíos.
 *
 * Las funciones "notice" examinan varios aspectos del inventario del jugador,
 * el equipo del jugador, o el contenido del hogar, y extraen varias cantidades
 * numéricas basadas en esos aspectos, ajustándolas por varias "habilidades",
 * como la capacidad de lanzar ciertos hechizos, etc.
 *
 * Las funciones "power" usan las cantidades numéricas descritas anteriormente, y
 * las usan para hacer dos cosas diferentes: (1) clasificar el "valor" de tener
 * varias habilidades en relación con la posible recompensa de "dinero" por llevar
 * objetos vendibles en su lugar, y (2) clasificar el valor de varias habilidades
 * entre sí, que se usa para determinar qué usar/comprar,
 * y en qué orden usar/comprar esos objetos.
 *
 * Estas funciones usan algunos valores muy heurísticos, por cierto...
 *
 * Probablemente deberíamos tener en cuenta cosas como el posible encantamiento
 * (especialmente cuando estamos en la ciudad), y los objetos que se pueden encontrar pronto.
 *
 * Consideramos varias cosas:
 *   (1) el "poder" real del arma y arco actuales
 *   (2) las varias "banderas" impartidas por el equipo
 *   (3) las varias habilidades impartidas por el equipo
 *   (4) las penalizaciones inducidas por armadura pesada, guantes o armas afiladas
 *   (5) las habilidades necesarias para entrar en el nivel de mazmorra "max_depth"
 *   (6) las varias habilidades de algunos objetos de inventario útiles
 *
 * Nótese el uso de "contadores de objetos" especiales para evaluar el valor de
 * una colección de objetos del tipo dado. Básicamente, el primer objeto
 * del tipo dado es siempre el más valioso, y los objetos subsiguientes
 * valen menos, hasta que se alcanza el "límite", después del cual cualquier
 * objeto extra solo vale lo que se puede vender.
 */

/*
 * Función auxiliar -- notificar un espacio de munición
 */
static void borg_notice_ammo(int slot)
{
    borg_item *item = &borg_items[slot];

    /* Saltar objetos vacíos */
    if (!item->iqty)
        return;


    /* número de espacios de inventario que usa el carcaj */
    if (slot >= QUIVER_START)
        borg.trait[BI_QUIVER_SLOTS]++;

    /* sumar el peso de los objetos */
    borg.trait[BI_WEIGHT] += borg_item_weight(item);

    /* Contar toda la munición */
    borg.trait[BI_AMMO_COUNT] += item->iqty;

    if (item->tval != borg.trait[BI_AMMO_TVAL])
        return;

    /* Contar los proyectiles que sirven para tu arco */
    borg.trait[BI_AMISSILES] += item->iqty;

    /* rastrear el primer objeto no maldecible */
    if (item->uncursable) {
        borg.trait[BI_WHERE_CURSED] |= BORG_QUILL;
        if (!borg.trait[BI_FIRST_CURSED])
            borg.trait[BI_FIRST_CURSED] = slot + 1;

        borg.trait[BI_AMISSILES_CURSED] += item->iqty;
        return;
    }

    if (item->ego_idx)
        borg.trait[BI_AMISSILES_SPECIAL] += item->iqty;

    /* comprobar munición para encantar */

    /* Ignorar proyectiles sin valor */
    if (item->value <= 0)
        return;

    /* Solo encantar munición si tenemos un buen tirador,
     * de lo contrario, guardar los encantamientos en el hogar.
     */
    if (borg.trait[BI_AMMO_POWER] >= 3) {

        if ((borg_equips_item(act_firebrand, false)
                || borg_spell_legal_fail(BRAND_AMMUNITION, 65))
            && item->iqty >= 5 &&
            /* Saltar artefactos y objetos ego */
            !item->ego_idx && !item->art_idx && item->ident
            && item->tval == borg.trait[BI_AMMO_TVAL]) {
            borg.trait[BI_NEED_BRAND_WEAPON] += 10L;
        }

        /* si tenemos mucho dinero (como tendremos a nivel 35), */
        /* encantar proyectiles */
        if (borg.trait[BI_CLEVEL] > 35) {
            if (borg_spell_legal_fail(ENCHANT_WEAPON, 65) && item->iqty >= 5) {
                if (item->to_h < 10) {
                    borg.trait[BI_NEED_ENCHANT_TO_H] += (10 - item->to_h);
                }

                if (item->to_d < 10) {
                    borg.trait[BI_NEED_ENCHANT_TO_D] += (10 - item->to_d);
                }
            } else {
                if (item->to_h < 8) {
                    borg.trait[BI_NEED_ENCHANT_TO_H] += (8 - item->to_h);
                }

                if (item->to_d < 8) {
                    borg.trait[BI_NEED_ENCHANT_TO_D] += (8 - item->to_d);
                }
            }
        }
    } /* Poder de Munición > 3 */

    /* Solo encantar munición si tenemos un buen tirador,
     * de lo contrario, guardar los encantamientos en el hogar.
     */
    if (borg.trait[BI_AMMO_POWER] < 3)
        return;

    if ((borg_equips_item(act_firebrand, false)
            || borg_spell_legal_fail(BRAND_AMMUNITION, 65))
        && item->iqty >= 5 &&
        /* Saltar artefactos y objetos ego */
        !item->art_idx && !item->ego_idx && item->ident
        && item->tval == borg.trait[BI_AMMO_TVAL]) {
        borg.trait[BI_NEED_BRAND_WEAPON] += 10L;
    }
}

/* no dar crédito por objetos permanentemente malditos no artefactos */
static bool cursed_nonartifact(borg_item *item)
{
    if (!item || item->iqty == 0)
        return false;

    if (item->art_idx)
        return false;

    if (item->uncursable)
        return false;

    return item->cursed;
}

/*
 * Función auxiliar -- notificar el equipo del jugador
 */
static void borg_notice_equipment(void)
{
    int                        i, hold;
    const struct player_race  *rb_ptr = player->race;
    const struct player_class *cb_ptr = player->class;

    int extra_shots                   = 0;
    int extra_might                   = 0;
    int my_num_fire;

    bitflag f[OF_SIZE];

    borg_item *item;

    int16_t stat_cur[STAT_MAX]; /* Valores de estadísticas "naturales" actuales    */

    /* Empezar con un disparo por turno */
    my_num_fire = 1;

    /* Infravisión base (puramente racial) */
    borg.trait[BI_INFRA] = rb_ptr->infra;

    /* Habilidad base -- desarmado */
    borg.trait[BI_DISP] = rb_ptr->r_skills[SKILL_DISARM_PHYS]
                          + cb_ptr->c_skills[SKILL_DISARM_PHYS];
    borg.trait[BI_DISM] = rb_ptr->r_skills[SKILL_DISARM_MAGIC]
                          + cb_ptr->c_skills[SKILL_DISARM_MAGIC];

    /* Habilidad base -- dispositivos mágicos */
    borg.trait[BI_DEV]
        = rb_ptr->r_skills[SKILL_DEVICE] + cb_ptr->c_skills[SKILL_DEVICE];

    /* Habilidad base -- tirada de salvación */
    borg.trait[BI_SAV]
        = rb_ptr->r_skills[SKILL_SAVE] + cb_ptr->c_skills[SKILL_SAVE];

    /* Habilidad base -- sigilo */
    borg.trait[BI_STL]
        = rb_ptr->r_skills[SKILL_STEALTH] + cb_ptr->c_skills[SKILL_STEALTH];

    /* Habilidad base -- capacidad de búsqueda */
    borg.trait[BI_SRCH]
        = rb_ptr->r_skills[SKILL_SEARCH] + cb_ptr->c_skills[SKILL_SEARCH];

    /* Habilidad base -- combate (normal) */
    borg.trait[BI_THN] = rb_ptr->r_skills[SKILL_TO_HIT_MELEE]
                         + cb_ptr->c_skills[SKILL_TO_HIT_MELEE];

    /* Habilidad base -- combate (disparo) */
    borg.trait[BI_THB] = rb_ptr->r_skills[SKILL_TO_HIT_BOW]
                         + cb_ptr->c_skills[SKILL_TO_HIT_BOW];

    /* Habilidad base -- combate (lanzamiento) */
    borg.trait[BI_THT] = rb_ptr->r_skills[SKILL_TO_HIT_THROW]
                         + cb_ptr->c_skills[SKILL_TO_HIT_THROW];

    /* Afectar Habilidad -- excavación (STR) */
    borg.trait[BI_DIG]
        = rb_ptr->r_skills[SKILL_DIGGING] + cb_ptr->c_skills[SKILL_DIGGING];

    /** Habilidades Raciales **/

    /* Extraer las banderas del jugador */
    player_flags(player, f);

    /* Buenas banderas */
    if (of_has(f, OF_SLOW_DIGEST))
        borg.trait[BI_SDIG] = true;
    if (of_has(f, OF_FEATHER))
        borg.trait[BI_FEATH] = true;
    if (of_has(f, OF_REGEN))
        borg.trait[BI_REG] = true;
    if (of_has(f, OF_TELEPATHY))
        borg.trait[BI_ESP] = true;
    if (of_has(f, OF_SEE_INVIS))
        borg.trait[BI_SINV] = true;
    if (of_has(f, OF_FREE_ACT))
        borg.trait[BI_FRACT] = true;
    if (of_has(f, OF_HOLD_LIFE))
        borg.trait[BI_HLIFE] = true;

    /* Banderas raras */

    /* Malas banderas */
    if (of_has(f, OF_IMPACT))
        borg.trait[BI_W_IMPACT] = true;
    if (of_has(f, OF_AGGRAVATE))
        borg.trait[BI_CRSAGRV] = true;
    if (of_has(f, OF_AFRAID))
        borg.trait[BI_CRSFEAR] = true;
    if (of_has(f, OF_DRAIN_EXP))
        borg.trait[BI_CRSDRAIN_XP] = true;

    if (rb_ptr->el_info[ELEM_FIRE].res_level == -1)
        borg.trait[BI_CRSFVULN] = true;
    if (rb_ptr->el_info[ELEM_ACID].res_level == -1)
        borg.trait[BI_CRSAVULN] = true;
    if (rb_ptr->el_info[ELEM_COLD].res_level == -1)
        borg.trait[BI_CRSCVULN] = true;
    if (rb_ptr->el_info[ELEM_ELEC].res_level == -1)
        borg.trait[BI_CRSEVULN] = true;

    /* Banderas de inmunidad */
    if (rb_ptr->el_info[ELEM_FIRE].res_level == 3)
        borg.trait[BI_IFIRE] = true;
    if (rb_ptr->el_info[ELEM_ACID].res_level == 3)
        borg.trait[BI_IACID] = true;
    if (rb_ptr->el_info[ELEM_COLD].res_level == 3)
        borg.trait[BI_ICOLD] = true;
    if (rb_ptr->el_info[ELEM_ELEC].res_level == 3)
        borg.trait[BI_IELEC] = true;

    /* Banderas de resistencia */
    if (rb_ptr->el_info[ELEM_FIRE].res_level > 0)
        borg.trait[BI_RACID] = true;
    if (rb_ptr->el_info[ELEM_ELEC].res_level > 0)
        borg.trait[BI_RELEC] = true;
    if (rb_ptr->el_info[ELEM_FIRE].res_level > 0)
        borg.trait[BI_RFIRE] = true;
    if (rb_ptr->el_info[ELEM_COLD].res_level > 0)
        borg.trait[BI_RCOLD] = true;
    if (rb_ptr->el_info[ELEM_POIS].res_level > 0)
        borg.trait[BI_RPOIS] = true;
    if (rb_ptr->el_info[ELEM_LIGHT].res_level > 0)
        borg.trait[BI_RLITE] = true;
    if (rb_ptr->el_info[ELEM_DARK].res_level > 0)
        borg.trait[BI_RDARK] = true;
    if (rb_ptr->el_info[ELEM_SOUND].res_level > 0)
        borg.trait[BI_RSND] = true;
    if (rb_ptr->el_info[ELEM_SHARD].res_level > 0)
        borg.trait[BI_RSHRD] = true;
    if (rb_ptr->el_info[ELEM_NEXUS].res_level > 0)
        borg.trait[BI_RNXUS] = true;
    if (rb_ptr->el_info[ELEM_NETHER].res_level > 0)
        borg.trait[BI_RNTHR] = true;
    if (rb_ptr->el_info[ELEM_CHAOS].res_level > 0)
        borg.trait[BI_RKAOS] = true;
    if (rb_ptr->el_info[ELEM_DISEN].res_level > 0)
        borg.trait[BI_RDIS] = true;
    if (rf_has(f, OF_PROT_FEAR))
        borg.trait[BI_RFEAR] = true;
    if (rf_has(f, OF_PROT_BLIND))
        borg.trait[BI_RBLIND] = true;
    if (rf_has(f, OF_PROT_CONF))
        borg.trait[BI_RCONF] = true;

    /* Banderas de sostenimiento */
    if (rf_has(f, OF_SUST_STR))
        borg.trait[BI_SSTR] = true;
    if (rf_has(f, OF_SUST_INT))
        borg.trait[BI_SINT] = true;
    if (rf_has(f, OF_SUST_WIS))
        borg.trait[BI_SWIS] = true;
    if (rf_has(f, OF_SUST_DEX))
        borg.trait[BI_SDEX] = true;
    if (rf_has(f, OF_SUST_CON))
        borg.trait[BI_SCON] = true;

    /* si está acelerado */
    if (player->timed[TMD_FAST] || player->timed[TMD_SPRINT])
        borg.trait[BI_SPEED] += 10;
    else if (player->timed[TMD_TERROR])
        borg.trait[BI_SPEED] += 5;

    /* Estoy bastante seguro de que las CF_flags serán capturadas por el
     * código de arriba cuando se comprueben las banderas del jugador
     */

    /* rastrear activaciones */
    /* nota que esto se hace primero para que podamos usar este */
    /* array en borg_equips_item */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++) {
        if (borg_items[i].activ_idx) {
            borg.activation[borg_items[i].activ_idx] += 1;
        }
    }

    if (borg.activation[act_staff_magi])
        borg.trait[BI_ASTFMAGI] += 10;

    /* Escanear el inventario utilizable */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++) {
        item = &borg_items[i];

        /* Saltar objetos vacíos */
        if (!item->iqty)
            continue;

        /* rastrear el primer objeto no maldecible */
        if (item->uncursable) {
            borg.trait[BI_WHERE_CURSED] |= BORG_EQUIP;
            if (!borg.trait[BI_FIRST_CURSED]) 
                borg.trait[BI_FIRST_CURSED] = i + 1;
        }
        
        /* saltar objetos malditos no artefactos */
        if (cursed_nonartifact(item))
            continue;

        /* sumar el peso de los objetos */
        borg.trait[BI_WEIGHT] += borg_item_weight(item);

        if (borg_item_note_needs_id(item)) {
            borg.trait[BI_ALL_NEED_ID] += 1;
            borg.trait[BI_WORN_NEED_ID] += 1;
        }
 
        /* Afectar estadísticas */
        borg.trait[BI_ASTR] += item->modifiers[OBJ_MOD_STR]
                               * player->obj_k->modifiers[OBJ_MOD_STR];
        borg.trait[BI_AINT] += item->modifiers[OBJ_MOD_INT]
                               * player->obj_k->modifiers[OBJ_MOD_INT];
        borg.trait[BI_AWIS] += item->modifiers[OBJ_MOD_WIS]
                               * player->obj_k->modifiers[OBJ_MOD_WIS];
        borg.trait[BI_ADEX] += item->modifiers[OBJ_MOD_DEX]
                               * player->obj_k->modifiers[OBJ_MOD_DEX];
        borg.trait[BI_ACON] += item->modifiers[OBJ_MOD_CON]
                               * player->obj_k->modifiers[OBJ_MOD_CON];

        /* varios asesinatos */
        borg.trait[BI_WS_ANIMAL] = item->slays[RF_ANIMAL];
        borg.trait[BI_WS_EVIL]   = item->slays[RF_EVIL];
        borg.trait[BI_WS_UNDEAD] = item->slays[RF_UNDEAD];
        borg.trait[BI_WS_DEMON]  = item->slays[RF_DEMON];
        borg.trait[BI_WS_ORC]    = item->slays[RF_ORC];
        borg.trait[BI_WS_TROLL]  = item->slays[RF_TROLL];
        borg.trait[BI_WS_GIANT]  = item->slays[RF_GIANT];
        borg.trait[BI_WS_DRAGON] = item->slays[RF_DRAGON];

        /* varias marcas */
        if (item->brands[ELEM_ACID])
            borg.trait[BI_WB_ACID] = true;
        if (item->brands[ELEM_ELEC])
            borg.trait[BI_WB_ELEC] = true;
        if (item->brands[ELEM_FIRE])
            borg.trait[BI_WB_FIRE] = true;
        if (item->brands[ELEM_COLD])
            borg.trait[BI_WB_COLD] = true;
        if (item->brands[ELEM_POIS])
            borg.trait[BI_WB_POIS] = true;
        if (of_has(item->flags, OF_IMPACT))
            borg.trait[BI_W_IMPACT] = true;

        /* Afectar infravisión */
        borg.trait[BI_INFRA] += item->modifiers[OBJ_MOD_INFRA];

        /* Afectar sigilo */
        borg.trait[BI_STL] += item->modifiers[OBJ_MOD_STEALTH];

        /* Afectar capacidad de búsqueda (factor de cinco) */
        borg.trait[BI_SRCH] += (item->modifiers[OBJ_MOD_SEARCH] * 5);

        /* las armas de tipo excavación obtienen una bonificación especial */
        int dig = 0;
        if (item->tval == TV_DIGGING) {
            if (of_has(item->flags, OF_DIG_1))
                dig = 1;
            else if (of_has(item->flags, OF_DIG_2))
                dig = 2;
            else if (of_has(item->flags, OF_DIG_3))
                dig = 3;
        }
        dig += item->modifiers[OBJ_MOD_TUNNEL];

        /* Afectar excavación (factor de 20) */
        borg.trait[BI_DIG] += (dig * 20);

        /* Afectar velocidad */
        borg.trait[BI_SPEED] += item->modifiers[OBJ_MOD_SPEED];

        /* Afectar golpes (no del arma principal) */
        if (i != INVEN_WIELD)
            borg.trait[BI_EXTRA_BLOWS] += item->modifiers[OBJ_MOD_BLOWS];

        /* Aumentar disparos */
        extra_shots += item->modifiers[OBJ_MOD_SHOTS];

        /* Aumentar poder */
        extra_might += item->modifiers[OBJ_MOD_MIGHT];


        if (i != INVEN_LIGHT ||
            of_has(borg_items[i].flags, OF_NO_FUEL)
            || item->timeout != 0) {
            /* El objeto hace brillar al jugador o tiene un radio de luz */
            borg.trait[BI_LIGHT] += item->modifiers[OBJ_MOD_LIGHT];

            /* LIGHT_2 y LIGHT_3 */
            if (of_has(item->flags, OF_LIGHT_2)) {
                borg.trait[BI_LIGHT] += 2;
            }
            else if (of_has(item->flags, OF_LIGHT_3)) {
                borg.trait[BI_LIGHT] += 3;
            }

            /* la gente con "oscuridad" puede usar artefactos de radio 1 */
            if ((item->modifiers[OBJ_MOD_LIGHT] > 0)
                && (borg.trait[BI_CLASS] == CLASS_NECROMANCER))
                borg.trait[BI_LIGHT]--;

            borg.trait[BI_LIGHT] += item->modifiers[OBJ_MOD_LIGHT];
        }

        /* Aumentar movimientos mod */
        borg.trait[BI_MOD_MOVES] += item->modifiers[OBJ_MOD_MOVES];

        /* Aumentar reducción de daño */
        borg.trait[BI_DAM_RED] += item->modifiers[OBJ_MOD_DAM_RED];

        /* Varias banderas */
        if (of_has(item->flags, OF_SLOW_DIGEST))
            borg.trait[BI_SDIG] = true;
        if (of_has(item->flags, OF_AGGRAVATE))
            borg.trait[BI_CRSAGRV] = true;
        if (of_has(item->flags, OF_IMPAIR_HP))
            borg.trait[BI_CRSHPIMP] = true;
        if (of_has(item->flags, OF_IMPAIR_MANA))
            borg.trait[BI_CRSMPIMP] = true;
        if (of_has(item->flags, OF_AFRAID))
            borg.trait[BI_CRSFEAR] = true;
        if (of_has(item->flags, OF_DRAIN_EXP))
            borg.trait[BI_CRSDRAIN_XP] = true;

        /* maldiciones que no tienen banderas o cambios de estadísticas que se rastrean
         * en otro lugar */
        if (item->curses[BORG_CURSE_VULNERABILITY])
            borg.trait[BI_CRSVULN] = true;
        if (item->curses[BORG_CURSE_TELEPORTATION])
            borg.trait[BI_CRSTELE] = true;
        if (item->curses[BORG_CURSE_DULLNESS])
            borg.trait[BI_CRSDULL] = true;
        if (item->curses[BORG_CURSE_SICKLINESS])
            borg.trait[BI_CRSSICK] = true;
        if (item->curses[BORG_CURSE_ENVELOPING])
            borg.trait[BI_CRSENVELOPING] = true;
        if (item->curses[BORG_CURSE_IRRITATION]) {
            borg.trait[BI_CRSAGRV]       = true;
            borg.trait[BI_CRSIRRITATION] = true;
        }
        if (item->curses[BORG_CURSE_WEAKNESS])
            borg.trait[BI_CRSWEAK] = true;
        if (item->curses[BORG_CURSE_CLUMSINESS])
            borg.trait[BI_CRSCLUM] = true;
        if (item->curses[BORG_CURSE_SLOWNESS])
            borg.trait[BI_CRSSLOW] = true;
        if (item->curses[BORG_CURSE_ANNOYANCE])
            borg.trait[BI_CRSANNOY] = true;
        if (item->curses[BORG_CURSE_POISON])
            borg.trait[BI_CRSPOIS] = true;
        if (item->curses[BORG_CURSE_SIREN])
            borg.trait[BI_CRSSIREN] = true;
        if (item->curses[BORG_CURSE_HALLUCINATION])
            borg.trait[BI_CRSHALU] = true;
        if (item->curses[BORG_CURSE_PARALYSIS])
            borg.trait[BI_CRSPARA] = true;
        if (item->curses[BORG_CURSE_DEMON_SUMMON])
            borg.trait[BI_CRSSDEM] = true;
        if (item->curses[BORG_CURSE_DRAGON_SUMMON])
            borg.trait[BI_CRSSDRA] = true;
        if (item->curses[BORG_CURSE_UNDEAD_SUMMON])
            borg.trait[BI_CRSSUND] = true;
        if (item->curses[BORG_CURSE_IMPAIR_MANA_RECOVERY])
            borg.trait[BI_CRSMPIMP] = true;
        if (item->curses[BORG_CURSE_IMPAIR_HITPOINT_RECOVERY])
            borg.trait[BI_CRSHPIMP] = true;
        if (item->curses[BORG_CURSE_COWARDICE])
            borg.trait[BI_CRSFEAR] = true;
        if (item->curses[BORG_CURSE_STONE])
            borg.trait[BI_CRSSTONE] = true;
        if (item->curses[BORG_CURSE_ANTI_TELEPORTATION])
            borg.trait[BI_CRSNOTEL] = true;
        if (item->curses[BORG_CURSE_TREACHEROUS_WEAPON])
            borg.trait[BI_CRSTWEP] = true;
        if (item->curses[BORG_CURSE_BURNING_UP]) {
            borg.trait[BI_CRSFVULN] = true;
            borg.trait[BI_RCOLD]    = true;
        }
        if (item->curses[BORG_CURSE_CHILLED_TO_THE_BONE]) {
            borg.trait[BI_CRSCVULN] = true;
            borg.trait[BI_RFIRE]    = true;
        }
        if (item->curses[BORG_CURSE_STEELSKIN])
            borg.trait[BI_CRSSTEELSKIN] = true;
        if (item->curses[BORG_CURSE_AIR_SWING])
            borg.trait[BI_CRSAIRSWING] = true;
        if (item->curses[BORG_CURSE_UNKNOWN])
            borg.trait[BI_CRSUNKNO] = true;

        if (item->el_info[ELEM_FIRE].res_level == -1)
            borg.trait[BI_CRSFVULN] = true;
        if (item->el_info[ELEM_ACID].res_level == -1)
            borg.trait[BI_CRSAVULN] = true;
        if (item->el_info[ELEM_COLD].res_level == -1)
            borg.trait[BI_CRSCVULN] = true;
        if (item->el_info[ELEM_ELEC].res_level == -1)
            borg.trait[BI_CRSEVULN] = true;

        if (of_has(item->flags, OF_REGEN))
            borg.trait[BI_REG] = true;
        if (of_has(item->flags, OF_TELEPATHY))
            borg.trait[BI_ESP] = true;
        if (of_has(item->flags, OF_SEE_INVIS))
            borg.trait[BI_SINV] = true;
        if (of_has(item->flags, OF_FEATHER))
            borg.trait[BI_FEATH] = true;
        if (of_has(item->flags, OF_FREE_ACT))
            borg.trait[BI_FRACT] = true;
        if (of_has(item->flags, OF_HOLD_LIFE))
            borg.trait[BI_HLIFE] = true;
        if (of_has(item->flags, OF_PROT_CONF))
            borg.trait[BI_RCONF] = true;
        if (of_has(item->flags, OF_PROT_BLIND))
            borg.trait[BI_RBLIND] = true;

        /* Banderas de inmunidad */
        /* si eres inmune automáticamente resistes */
        if (item->el_info[ELEM_FIRE].res_level == 3) {
            borg.trait[BI_IFIRE] = true;
            borg.trait[BI_RFIRE] = true;
            borg.temp.res_fire   = true;
        }
        if (item->el_info[ELEM_ACID].res_level == 3) {
            borg.trait[BI_IACID] = true;
            borg.trait[BI_RACID] = true;
            borg.temp.res_acid   = true;
        }
        if (item->el_info[ELEM_COLD].res_level == 3) {
            borg.trait[BI_ICOLD] = true;
            borg.trait[BI_RCOLD] = true;
            borg.temp.res_cold   = true;
        }
        if (item->el_info[ELEM_ELEC].res_level == 3) {
            borg.trait[BI_IELEC] = true;
            borg.trait[BI_RELEC] = true;
            borg.temp.res_elec   = true;
        }

        /* Banderas de resistencia */
        if (item->el_info[ELEM_ACID].res_level > 0)
            borg.trait[BI_RACID] = true;
        if (item->el_info[ELEM_ELEC].res_level > 0)
            borg.trait[BI_RELEC] = true;
        if (item->el_info[ELEM_FIRE].res_level > 0)
            borg.trait[BI_RFIRE] = true;
        if (item->el_info[ELEM_COLD].res_level > 0)
            borg.trait[BI_RCOLD] = true;
        if (item->el_info[ELEM_POIS].res_level > 0)
            borg.trait[BI_RPOIS] = true;
        if (item->el_info[ELEM_SOUND].res_level > 0)
            borg.trait[BI_RSND] = true;
        if (item->el_info[ELEM_LIGHT].res_level > 0)
            borg.trait[BI_RLITE] = true;
        if (item->el_info[ELEM_DARK].res_level > 0)
            borg.trait[BI_RDARK] = true;
        if (item->el_info[ELEM_CHAOS].res_level > 0)
            borg.trait[BI_RKAOS] = true;
        if (item->el_info[ELEM_DISEN].res_level > 0)
            borg.trait[BI_RDIS] = true;
        if (item->el_info[ELEM_SHARD].res_level > 0)
            borg.trait[BI_RSHRD] = true;
        if (item->el_info[ELEM_NEXUS].res_level > 0)
            borg.trait[BI_RNXUS] = true;
        if (item->el_info[ELEM_NETHER].res_level > 0)
            borg.trait[BI_RNTHR] = true;

        /* Banderas de sostenimiento */
        if (of_has(item->flags, OF_SUST_STR))
            borg.trait[BI_SSTR] = true;
        if (of_has(item->flags, OF_SUST_INT))
            borg.trait[BI_SINT] = true;
        if (of_has(item->flags, OF_SUST_WIS))
            borg.trait[BI_SWIS] = true;
        if (of_has(item->flags, OF_SUST_DEX))
            borg.trait[BI_SDEX] = true;
        if (of_has(item->flags, OF_SUST_CON))
            borg.trait[BI_SCON] = true;

        /* Bueno tener un objeto con múltiples resistencias altas */
        int bonuses = ((item->el_info[ELEM_POIS].res_level > 0)
                       + (item->el_info[ELEM_SOUND].res_level > 0)
                       + (item->el_info[ELEM_SHARD].res_level > 0)
                       + (item->el_info[ELEM_NEXUS].res_level > 0)
                       + (item->el_info[ELEM_NETHER].res_level > 0)
                       + (item->el_info[ELEM_CHAOS].res_level > 0)
                       + (item->el_info[ELEM_DISEN].res_level > 0) +
                       /* resistir las 4 básicas */
                       ((item->el_info[ELEM_FIRE].res_level > 0)
                           && (item->el_info[ELEM_COLD].res_level > 0)
                           && (item->el_info[ELEM_ELEC].res_level > 0)
                           && (item->el_info[ELEM_ACID].res_level > 0))
                       +
                       /* sostiene todas las estadísticas */
                       (of_has(item->flags, OF_SUST_STR)
                           && of_has(item->flags, OF_SUST_INT)
                           && of_has(item->flags, OF_SUST_WIS)
                           && of_has(item->flags, OF_SUST_DEX)
                           && of_has(item->flags, OF_SUST_CON)));

        if (bonuses > 2)
            borg.trait[BI_MULTIPLE_BONUSES] += bonuses;

        /* TRUCO: Neto cero El borg leerá mal los objetos dañados por ácido como
         * Guantes de Cuero [2,-2] y supondrá falsamente que ayudan a su poder.
         * este truco reescribe la bonificación a un valor extremadamente negativo
         * animándolo así a eliminar el objeto no útil-no dañino pero
         * pesado.
         */
        if ((!item->art_idx && !item->ego_idx) && item->ac >= 1
            && item->to_a + item->ac <= 0) {
            item->to_a = -20;
        }

        /* Modificar la clase de armadura base */
        borg.trait[BI_ARMOR] += item->ac;

        /* Aplicar las bonificaciones a la clase de armadura */
        borg.trait[BI_ARMOR] += item->to_a;

        /* No aplicar bonificaciones de "arma" */
        if (i == INVEN_WIELD)
            continue;

        /* No aplicar bonificaciones de "arco" */
        if (i == INVEN_BOW)
            continue;

        /* Aplicar las bonificaciones al golpe/daño */
        borg.trait[BI_TOHIT] += item->to_h;
        borg.trait[BI_TODAM] += item->to_d;
    }

    /* si el jugador tiene oscuridad, contarlo como si tuviera luz si no tiene ninguna */
    if ((borg.trait[BI_CLASS] == CLASS_NECROMANCER)
        && borg.trait[BI_LIGHT] <= 0)
        borg.trait[BI_LIGHT] = 1;


    if (borg.trait[BI_CRSVULN]) {
        borg.trait[BI_CRSAGRV] = true;
        borg.trait[BI_ARMOR] -= 50;
    }
    if (borg.trait[BI_CRSDULL]) {
        borg.trait[BI_CINT] -= 5;
        borg.trait[BI_CWIS] -= 5;
    }
    if (borg.trait[BI_CRSSICK]) {
        borg.trait[BI_CSTR] -= 5;
        borg.trait[BI_CDEX] -= 5;
        borg.trait[BI_CCON] -= 5;
    }
    if (borg.trait[BI_CRSWEAK])
        borg.trait[BI_CSTR] -= 10;
    if (borg.trait[BI_CRSCLUM])
        borg.trait[BI_CDEX] -= 10;
    if (borg.trait[BI_CRSSLOW])
        borg.trait[BI_SPEED] -= 5;
    if (borg.trait[BI_CRSANNOY]) {
        borg.trait[BI_SPEED] -= 10;
        borg.trait[BI_STL] -= 10;
        borg.trait[BI_CRSAGRV] = true;
    }

    /* El borg necesita actualizar sus puntos de estadística base */
    for (i = 0; i < STAT_MAX; i++) {
        /* Tomar el número exacto del juego. Este número está disponible para
         * el jugador en la ventana de término extra.
         */
        stat_cur[i] = player->stat_cur[i];
    }

    /* Actualizar "estadísticas" */
    for (i = 0; i < STAT_MAX; i++) {
        int add, use, ind;

        add = borg.trait[BI_ASTR + i];

        /* Modificar las estadísticas para raza/clase */
        add += (player->race->r_adj[i] + player->class->c_adj[i]);

        /* Extraer el nuevo valor "use_stat" para la estadística */
        use = modify_stat_value(stat_cur[i], add);

        /* Valores: 3, ..., 17 */
        if (use <= 18)
            ind = (use - 3);

        /* Rangos: 18/00-18/09, ..., 18/210-18/219 */
        else if (use <= 18 + 219)
            ind = (15 + (use - 18) / 10);

        /* Rango: 18/220+ */
        else
            ind = (37);

        /* Guardar el índice */
        if (ind > 37)
            borg.trait[BI_STR_INDEX + i] = 37;
        else
            borg.trait[BI_STR_INDEX + i] = ind;
        borg.trait[BI_STR + i]  = use;
        borg.trait[BI_CSTR + i] = stat_cur[i];
    }

    borg.trait[BI_HP_ADJ] = player->player_hp[player->lev - 1]
                            + borg_adj_con_mhp[borg.trait[BI_CON_INDEX]]
                                  * borg.trait[BI_CLEVEL] / 100;

    /* 'Mana' es en realidad el 'ajuste de mana' */
    int spell_stat = borg_spell_stat();
    if (spell_stat >= 0) {
        borg.trait[BI_SP_ADJ]
            = ((borg_adj_mag_mana[borg.trait[BI_STR_INDEX + spell_stat]]
                   * (borg.trait[BI_CLEVEL] - player->class->magic.spell_first
                       + 1))
                / 2);
        borg.trait[BI_FAIL1] = 
            borg_adj_mag_stat[borg.trait[BI_STR_INDEX + spell_stat]];
        borg.trait[BI_FAIL2] = 
            borg_adj_mag_fail[borg.trait[BI_STR_INDEX + spell_stat]];
    }

    /* La hinchazón ralentiza al jugador (un poco) */
    if (borg.trait[BI_ISGORGED])
        borg.trait[BI_SPEED] -= 10;

    /* Bonificaciones de Modificador Real */
    borg.trait[BI_ARMOR] += borg_adj_dex_ta[borg.trait[BI_DEX_INDEX]];
    borg.trait[BI_TODAM] += borg_adj_str_td[borg.trait[BI_STR_INDEX]];
    borg.trait[BI_TOHIT] += borg_adj_dex_th[borg.trait[BI_DEX_INDEX]];
    borg.trait[BI_TOHIT] += borg_adj_str_th[borg.trait[BI_STR_INDEX]];

    /* Obtener el valor de "sostener" */
    hold = adj_str_hold[borg.trait[BI_STR_INDEX]];

    /* excavación */
    borg.trait[BI_DIG] += borg_adj_str_dig[borg.trait[BI_STR_INDEX]];

    /** Examinar el "arco actual" **/
    item = &borg_items[INVEN_BOW];

    /* atacar con las manos desnudas */
    if (item->iqty == 0 || cursed_nonartifact(item)) {
        item->ds     = 0;
        item->dd     = 0;
        item->to_d   = 0;
        item->to_h   = 0;
        item->weight = 0;
        item->ident  = true;
        item->sval = 0;
    }

    /* Bonificaciones reales */
    borg.trait[BI_BTOHIT] = item->to_h;
    borg.trait[BI_BTODAM] = item->to_d;
    borg.trait[BI_BID] = item->ident;
    borg.trait[BI_SLING] = item->sval == sv_sling;
    borg.trait[BI_BART] = item->art_idx;

    /* Es difícil cargar un arco pesado */
    if (hold < item->weight / 10) {
        borg.trait[BI_HEAVYBOW] = true;
        /* Es difícil usar un arco pesado */
        borg.trait[BI_TOHIT] += 2 * (hold - item->weight / 10);
    }

    /* Calcular "disparos extra" si es necesario */
    if (item->iqty && (hold >= item->weight / 10)) {
        /* Tomar nota del "tval" requerido para los proyectiles */
        if (item->sval == sv_sling) {
            borg.trait[BI_AMMO_TVAL]  = TV_SHOT;
            borg.trait[BI_AMMO_SIDES] = 3;
            borg.trait[BI_AMMO_POWER] = 2;
        } else if (item->sval == sv_short_bow) {
            borg.trait[BI_AMMO_TVAL]  = TV_ARROW;
            borg.trait[BI_AMMO_SIDES] = 4;
            borg.trait[BI_AMMO_POWER] = 2;
        } else if (item->sval == sv_long_bow) {
            borg.trait[BI_AMMO_TVAL]  = TV_ARROW;
            borg.trait[BI_AMMO_SIDES] = 4;
            borg.trait[BI_AMMO_POWER] = 3;
        } else if (item->sval == sv_light_xbow) {
            borg.trait[BI_AMMO_TVAL]  = TV_BOLT;
            borg.trait[BI_AMMO_SIDES] = 5;
            borg.trait[BI_AMMO_POWER] = 3;
        } else if (item->sval == sv_heavy_xbow) {
            borg.trait[BI_AMMO_TVAL]  = TV_BOLT;
            borg.trait[BI_AMMO_SIDES] = 5;
            borg.trait[BI_AMMO_POWER] = 4;
        }

        /* Añadir poder extra */
        borg.trait[BI_AMMO_POWER] += extra_might;

        /* Recompensar a los Exploradores de Alto Nivel que usan Arcos */
        if (player_has(player, PF_FAST_SHOT)) {
            if (borg.trait[BI_AMMO_TVAL] == TV_ARROW)
                /* Disparo extra a nivel 20 */
                if (borg.trait[BI_CLEVEL] >= 20)
                    my_num_fire++;

            /* Disparo extra a nivel 40 */
            if (borg.trait[BI_CLEVEL] >= 40)
                my_num_fire++;

            borg.trait[BI_FAST_SHOTS] = true;
        }

        /* Añadir los "disparos de bonificación" */
        my_num_fire += extra_shots;

        /* Requerir al menos un disparo */
        if (my_num_fire < 1)
            my_num_fire = 1;
    }
    borg.trait[BI_SHOTS] = my_num_fire;

    /* Examinar el "arma principal" */
    item = &borg_items[INVEN_WIELD];

    /* atacar con las manos desnudas */
    if (item->iqty == 0 || cursed_nonartifact(item)) {
        item->ds     = 0;
        item->dd     = 0;
        item->to_d   = 0;
        item->to_h   = 0;
        item->weight = 0;
        item->ident  = true;
    }

    /* Valores reales */
    borg.trait[BI_WTOHIT] = item->to_h;
    borg.trait[BI_WTODAM] = item->to_d;
    borg.trait[BI_WID] = item->ident;
    borg.trait[BI_WDD] = item->dd;
    borg.trait[BI_WDS] = item->ds;

    /* Es difícil sostener un arma pesada */
    if (hold < item->weight / 10) {
        borg.trait[BI_HEAVYWEPON] = true;

        /* Es difícil usar un arma pesada */
        borg.trait[BI_TOHIT] += 2 * (hold - item->weight / 10);
    }

    /* Armas normales */
    if (item->iqty && (hold >= item->weight / 10)) {
        /* calcular el número de golpes */
        borg.trait[BI_BLOWS] = borg_calc_blows(item);

        /* Aumentar la habilidad de excavación por el peso del arma */
        borg.trait[BI_DIG] += (item->weight / 10);
    }

    /* Recompensar a los Guerreros de Alto Nivel con Resistir Miedo */
    if (player_has(player, PF_BRAVERY_30)) {
        /* Resistir miedo a nivel 30 */
        if (borg.trait[BI_CLEVEL] >= 30)
            borg.trait[BI_RFEAR] = true;
    }

    /* Afectar Habilidad -- sigilo (bonificación uno) */
    borg.trait[BI_STL] += 1;

    /* Afectar Habilidad -- desarmado (DEX y INT) */
    borg.trait[BI_DISP] += borg_adj_dex_dis[borg.trait[BI_DEX_INDEX]];
    borg.trait[BI_DISM] += borg_adj_int_dis[borg.trait[BI_INT_INDEX]];

    /* Afectar Habilidad -- dispositivos mágicos (INT) */
    borg.trait[BI_DEV] += borg_adj_int_dev[borg.trait[BI_INT_INDEX]];

    /* Afectar Habilidad -- tirada de salvación (WIS) */
    borg.trait[BI_SAV] += borg_adj_wis_sav[borg.trait[BI_WIS_INDEX]];

    /* Afectar Habilidad -- desarmado (Nivel, por Clase) */
    borg.trait[BI_DISP] += (cb_ptr->x_skills[SKILL_DISARM_PHYS]
                            * borg.trait[BI_MAXCLEVEL] / 10);
    borg.trait[BI_DISM] += (cb_ptr->x_skills[SKILL_DISARM_MAGIC]
                            * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- dispositivos mágicos (Nivel, por Clase) */
    borg.trait[BI_DEV]
        += (cb_ptr->x_skills[SKILL_DEVICE] * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- tirada de salvación (Nivel, por Clase) */
    borg.trait[BI_SAV]
        += (cb_ptr->x_skills[SKILL_SAVE] * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- sigilo (Nivel, por Clase) */
    borg.trait[BI_STL]
        += (cb_ptr->x_skills[SKILL_STEALTH] * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- capacidad de búsqueda (Nivel, por Clase) */
    borg.trait[BI_SRCH]
        += (cb_ptr->x_skills[SKILL_SEARCH] * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- combate (normal) (Nivel, por Clase) */
    borg.trait[BI_THN] += (cb_ptr->x_skills[SKILL_TO_HIT_MELEE]
                           * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- combate (disparo) (Nivel, por Clase) */
    borg.trait[BI_THB]
        += (cb_ptr->x_skills[SKILL_TO_HIT_BOW] * borg.trait[BI_MAXCLEVEL] / 10);

    /* Afectar Habilidad -- combate (lanzamiento) (Nivel, por Clase) */
    borg.trait[BI_THT] += (cb_ptr->x_skills[SKILL_TO_HIT_THROW]
                           * borg.trait[BI_MAXCLEVEL] / 10);

    /* Limitar Habilidad -- sigilo de 0 a 30 */
    if (borg.trait[BI_STL] > 30)
        borg.trait[BI_STL] = 30;
    if (borg.trait[BI_STL] < 0)
        borg.trait[BI_STL] = 0;

    /* Limitar Habilidad -- excavación de 1 en adelante */
    if (borg.trait[BI_DIG] < 1)
        borg.trait[BI_DIG] = 1;

    /*** Algunas penalizaciones a considerar ***/

    /* Miedo por hechizo o efecto o bandera */
    if (borg.trait[BI_ISAFRAID] || borg.trait[BI_CRSFEAR]) {
        borg.trait[BI_TOHIT] -= 20;
        borg.trait[BI_ARMOR] += 8;
        borg.trait[BI_DEV] = borg.trait[BI_DEV] * 95 / 100;
    }

    /* penalización de arma de sacerdote para armas de filo no bendecidas */
    if (player_has(player, PF_BLESS_WEAPON)
        && (item->tval == TV_HAFTED || 
            of_has(item->flags, OF_BLESSED))) {
        /* Reducir las bonificaciones reales */
        borg.trait[BI_TOHIT] += 2;
        borg.trait[BI_TODAM] += 2;
    }

    /*** Contar los encantamientos necesarios ***/

    /* Encantar todo el equipo (armas) */
    for (i = INVEN_WIELD; i <= INVEN_BOW; i++) {
        item = &borg_items[i];

        /* Saltar objetos vacíos */
        if (!item->iqty)
            continue;

        /* Saltar objetos "desconocidos" */
        if (!item->ident)
            continue;

        /* saltar objetos malditos no artefactos */
        if (cursed_nonartifact(item))
            continue;

        /* La mayoría de las clases guardan los encantamientos hasta que obtienen
         * un tirador 3x (como un arco largo).
         * --Importante: También mirar en borg7.c para el encantamiento.
         * --No queremos que el arco sea encantado por error.
         */
        if (i == INVEN_BOW && /* arco */
            borg.trait[BI_AMMO_POWER] < 3 && /* tirador 3x */
            (!item->art_idx && !item->ego_idx)) /* No Ego o Artefacto */
            continue;

        /* Encantar todas las armas (al golpe) */
        if ((borg_spell_legal_fail(ENCHANT_WEAPON, 65)
                || borg.trait[BI_AENCH_SWEP] >= 1)) {
            if (item->to_h < borg_cfg[BORG_ENCHANT_LIMIT]) {
                borg.trait[BI_NEED_ENCHANT_TO_H]
                    += (borg_cfg[BORG_ENCHANT_LIMIT] - item->to_h);
            }

            /* Encantar todas las armas (al daño) */
            if (item->to_d < borg_cfg[BORG_ENCHANT_LIMIT]) {
                borg.trait[BI_NEED_ENCHANT_TO_D]
                    += (borg_cfg[BORG_ENCHANT_LIMIT] - item->to_d);
            }
        } else /* No tengo el hechizo o *encantar* */
        {
            if (item->to_h < 8) {
                borg.trait[BI_NEED_ENCHANT_TO_H] += (8 - item->to_h);
            }

            /* Encantar todas las armas (al daño) */
            if (item->to_d < 8) {
                borg.trait[BI_NEED_ENCHANT_TO_D] += (8 - item->to_d);
            }
        }
    }

    /* Encantar todo el equipo (armadura) */
    for (i = INVEN_BODY; i <= INVEN_FEET; i++) {
        item = &borg_items[i];

        /* Saltar objetos vacíos */
        if (!item->iqty)
            continue;

        /* Saltar objetos "desconocidos" */
        if (!item->ident)
            continue;

        /* saltar objetos malditos no artefactos */
        if (cursed_nonartifact(item))
            continue;

        /* Notar necesidad de encantamiento */
        if (borg_spell_legal_fail(ENCHANT_ARMOUR, 65)
            || borg.trait[BI_AENCH_SARM] >= 1) {
            if (item->to_a < borg_cfg[BORG_ENCHANT_LIMIT]) {
                borg.trait[BI_NEED_ENCHANT_TO_A]
                    += (borg_cfg[BORG_ENCHANT_LIMIT] - item->to_a);
            }
        } else {
            if (item->to_a < 8) {
                borg.trait[BI_NEED_ENCHANT_TO_A] += (8 - item->to_a);
            }
        }
    }

    /* Manera especial de manejar Ver Invisibilidad */
    if (borg.see_inv >= 1)
        borg.trait[BI_SINV] = true;
    if (borg.trait[BI_CDEPTH] == 0
        && /* solo en la ciudad. Permitirle recordar hacia abajo */
        borg_spell_legal(SENSE_INVISIBLE))
        borg.trait[BI_SINV] = true;

    /* Manejo muy especial de Acción Libre.
     * Si la persona tiene una tirada de salvación perfecta, puede ser
     * considerado ok en Acción Libre. Esto puede liberar un
     * espacio de equipo.
     */
    if (borg.trait[BI_SAV] >= 100)
        borg.trait[BI_FRACT] = true;

    /* Caso especial para Resistir Ceguera. Las tiradas de salvación perfectas y las
     * resistencias para luz y oscuridad son suficientes para Resistir Ceguera
     */
    if (borg.trait[BI_SAV] >= 100 && borg.trait[BI_RDARK]
        && borg.trait[BI_RLITE])
        borg.trait[BI_RBLIND] = true;

    /*** El carcaj necesita ser evaluado ***/

    /* Ignorar proyectiles inválidos */
    for (i = QUIVER_START; i < QUIVER_END; i++)
        borg_notice_ammo(i);
}

/*
 * Función auxiliar -- notificar el inventario del jugador
 */
static void borg_notice_inventory(void)
{
    int i;

    borg_item *item;

    /*** Reiniciar contadores ***/

    /* Reiniciar pociones de estadísticas */
    for (i = 0; i < STAT_MAX; i++) {
        borg.need_statgain[i] = false;
        borg.amt_statgain[i]  = 0;
    }

    /* Reiniciar libros */
    for (i = 0; i < 9; i++)
        borg.amt_book[i] = 0;

    /*** Procesar el inventario ***/

    /* Escanear el inventario */
    for (i = 0; i < PACK_SLOTS; i++) {
        item = &borg_items[i];

        /* Saltar objetos vacíos */
        if (!item->iqty) {
            borg.trait[BI_EMPTY]++;
            continue;
        }

        /* caso especial para munición fuera del carcaj. */
        /* esto sucede cuando decidimos qué comprar, así que los objetos */
        /* se colocan en espacios vacíos */
        if (borg_is_ammo(item->tval)) {
            borg_notice_ammo(i);
            continue;
        }

        /* sumar el peso de los objetos */
        borg.trait[BI_WEIGHT] += borg_item_weight(item);

        /* ¿Necesita el borg obtener una ID? */
        if (borg_item_note_needs_id(item))
            borg.trait[BI_ALL_NEED_ID] += 1;

        /* rastrear el primer objeto no maldecible */
        if (item->uncursable) {
            borg.trait[BI_WHERE_CURSED] |= BORG_INVEN;
            if (!borg.trait[BI_FIRST_CURSED])
                borg.trait[BI_FIRST_CURSED] = i + 1;
        }

        /* Saltar objetos no conocidos */
        if (!item->aware)
            continue;

        /* contar los objetos que tiene el borg (no contar artefactos */
        /* que no están equipados) */
        borg.has[item->kind] += item->iqty;

        /* saltar objetos malditos no artefactos */
        if (cursed_nonartifact(item))
            continue;

        /* Analizar el objeto */
        switch (item->tval) {
            /* Libros */
        case TV_MAGIC_BOOK:
        case TV_PRAYER_BOOK:
        case TV_NATURE_BOOK:
        case TV_SHADOW_BOOK:
        case TV_OTHER_BOOK:
            /* Saltar libros incorrectos (si podemos examinar este libro, es bueno) */
            if (!obj_kind_can_browse(&k_info[item->kind]))
                break;
            /* Contar los libros */
            borg.amt_book[borg_get_book_num(item->sval)] += item->iqty;
            break;

        /* Comida */
        case TV_MUSHROOM:
            if (item->sval == sv_mush_purging || item->sval == sv_mush_restoring
                || item->sval == sv_mush_cure_mind) {
                if (borg_cfg[BORG_MUNCHKIN_START]
                    && borg.trait[BI_MAXCLEVEL]
                           < borg_cfg[BORG_MUNCHKIN_LEVEL]) {
                    break;
                }
            }
            if (item->sval == sv_mush_second_sight
                || item->sval == sv_mush_emergency
                || item->sval == sv_mush_terror
                || item->sval == sv_mush_stoneskin
                || item->sval == sv_mush_debility
                || item->sval == sv_mush_sprinting)
                if (borg_cfg[BORG_MUNCHKIN_START]
                    && borg.trait[BI_MAXCLEVEL]
                           >= borg_cfg[BORG_MUNCHKIN_LEVEL]) {
                    borg.trait[BI_ASHROOM] += item->iqty;
                }
        /* fall through */
        case TV_FOOD:
            /* Analizar */
            {
                /* comprobar comida que nos daña */
                if (borg_obj_has_effect(item->kind, EF_CRUNCH, -1)
                    || borg_obj_has_effect(
                        item->kind, EF_TIMED_INC, TMD_CONFUSED))
                    break;

                /* comprobar comida que da nutrición */
                if (item->tval == TV_MUSHROOM) {
                    /* las setas que aumentan la nutrición son de bajo efecto */
                    if (borg_obj_has_effect(item->kind, EF_NOURISH, 0))
                        borg.trait[BI_FOOD_LO] += item->iqty;
                } else /* TV_FOOD */
                {
                    if (item->sval == sv_food_apple
                        || item->sval == sv_food_handful
                        || item->sval == sv_food_slime_mold
                        || item->sval == sv_food_pint
                        || item->sval == sv_food_sip) {
                        borg.trait[BI_FOOD_LO] += item->iqty;
                    } else if (item->sval == sv_food_ration
                               || item->sval == sv_food_slice
                               || item->sval == sv_food_honey_cake
                               || item->sval == sv_food_waybread
                               || item->sval == sv_food_draught)
                        borg.trait[BI_FOOD_HI] += item->iqty;
                }

                /* comprobar comida que hace cosas */
                if (borg_obj_has_effect(item->kind, EF_CURE, TMD_POISONED))
                    borg.trait[BI_ACUREPOIS] += item->iqty;
                if (borg_obj_has_effect(item->kind, EF_CURE, TMD_CONFUSED))
                    borg.trait[BI_FOOD_CURE_CONF] += item->iqty;
                if (borg_obj_has_effect(item->kind, EF_CURE, TMD_BLIND))
                    borg.trait[BI_FOOD_CURE_BLIND] += item->iqty;
            }
            break;

        /* Pociones */
        case TV_POTION:
            /* Analizar */
            if (item->sval == sv_potion_healing)
                borg.trait[BI_AHEAL] += item->iqty;
            else if (item->sval == sv_potion_star_healing)
                borg.trait[BI_AEZHEAL] += item->iqty;
            else if (item->sval == sv_potion_life)
                borg.trait[BI_ALIFE] += item->iqty;
            else if (item->sval == sv_potion_cure_critical)
                borg.trait[BI_ACCW] += item->iqty;
            else if (item->sval == sv_potion_cure_serious)
                borg.trait[BI_ACSW] += item->iqty;
            else if (item->sval == sv_potion_cure_light)
                borg.trait[BI_ACLW] += item->iqty;
            else if (item->sval == sv_potion_cure_poison)
                borg.trait[BI_ACUREPOIS] += item->iqty;
            else if (item->sval == sv_potion_resist_heat)
                borg.trait[BI_ARESHEAT] += item->iqty;
            else if (item->sval == sv_potion_resist_cold)
                borg.trait[BI_ARESCOLD] += item->iqty;
            else if (item->sval == sv_potion_resist_pois)
                borg.trait[BI_ARESPOIS] += item->iqty;
            else if (item->sval == sv_potion_inc_str)
                borg.amt_statgain[STAT_STR] += item->iqty;
            else if (item->sval == sv_potion_inc_int)
                borg.amt_statgain[STAT_INT] += item->iqty;
            else if (item->sval == sv_potion_inc_wis)
                borg.amt_statgain[STAT_WIS] += item->iqty;
            else if (item->sval == sv_potion_inc_dex)
                borg.amt_statgain[STAT_DEX] += item->iqty;
            else if (item->sval == sv_potion_inc_con)
                borg.amt_statgain[STAT_CON] += item->iqty;
            else if (item->sval == sv_potion_inc_all) {
                borg.amt_statgain[STAT_STR] += item->iqty;
                borg.amt_statgain[STAT_INT] += item->iqty;
                borg.amt_statgain[STAT_WIS] += item->iqty;
                borg.amt_statgain[STAT_DEX] += item->iqty;
                borg.amt_statgain[STAT_CON] += item->iqty;
            } else if (item->sval == sv_potion_restore_life)
                borg.trait[BI_HASFIXEXP] = true;
            else if (item->sval == sv_potion_speed)
                borg.trait[BI_ASPEED] += item->iqty;
            break;

        /* Pergaminos */
        case TV_SCROLL:

            if (item->sval == sv_scroll_identify)
                borg.trait[BI_AID] += item->iqty;
            else if (item->sval == sv_scroll_recharging)
                borg.trait[BI_ARECHARGE] += item->iqty;
            else if (item->sval == sv_scroll_phase_door)
                borg.trait[BI_APHASE] += item->iqty;
            else if (item->sval == sv_scroll_teleport)
                borg.trait[BI_ATELEPORT] += item->iqty;
            else if (item->sval == sv_scroll_word_of_recall)
                borg.trait[BI_RECALL] += item->iqty;
            else if (item->sval == sv_scroll_enchant_armor)
                borg.trait[BI_AENCH_ARM] += item->iqty;
            else if (item->sval == sv_scroll_star_enchant_armor)
                borg.trait[BI_AENCH_SARM] += item->iqty;
            else if (item->sval == sv_scroll_enchant_weapon_to_hit)
                borg.trait[BI_AENCH_TOH] += item->iqty;
            else if (item->sval == sv_scroll_enchant_weapon_to_dam)
                borg.trait[BI_AENCH_TOD] += item->iqty;
            else if (item->sval == sv_scroll_star_enchant_weapon)
                borg.trait[BI_AENCH_SWEP] += item->iqty;
            else if (item->sval == sv_scroll_protection_from_evil)
                borg.trait[BI_APFE] += item->iqty;
            else if (item->sval == sv_scroll_rune_of_protection)
                borg.trait[BI_AGLYPH] += item->iqty;
            else if (item->sval == sv_scroll_teleport_level) {
                borg.trait[BI_ATELEPORTLVL] += item->iqty;
                borg.trait[BI_ATELEPORT] += 1;
            } else if (item->sval == sv_scroll_mass_banishment)
                borg.trait[BI_AMASSBAN] += item->iqty;
            break;

        /* Varitas */
        case TV_ROD:

            /* Analizar */
            if (item->sval == sv_rod_recall) {
                /* No contar con ella si soy malo con las activaciones */
                if (borg_activate_failure(item->tval, item->sval) < 500) {
                    borg.trait[BI_RECALL] += item->iqty * 100;
                } else {
                    borg.trait[BI_RECALL] += item->iqty;
                }
            } else if (item->sval == sv_rod_detection) {
                borg.trait[BI_ADETTRAP] += item->iqty * 100;
                borg.trait[BI_ADETDOOR] += item->iqty * 100;
                borg.trait[BI_ADETEVIL] += item->iqty * 100;
            } else if (item->sval == sv_rod_illumination)
                borg.trait[BI_ALITE] += item->iqty * 100;
            else if (item->sval == sv_rod_speed) {
                /* No contar con ella si soy malo con las activaciones */
                if (borg_activate_failure(item->tval, item->sval) < 500) {
                    borg.trait[BI_ASPEED] += item->iqty * 100;
                } else {
                    borg.trait[BI_ASPEED] += item->iqty;
                }
            } else if (item->sval == sv_rod_mapping)
                borg.trait[BI_AMAGICMAP] += item->iqty * 100;
            else if (item->sval == sv_rod_healing) {
                /* solo +2 por varita debido al largo tiempo de carga. */
                /* No contar con ella si soy malo con las activaciones */
                if (borg_activate_failure(item->tval, item->sval) < 500) {
                    borg.trait[BI_AHEAL] += item->iqty * 3;
                } else {
                    borg.trait[BI_AHEAL] += item->iqty + 1;
                }
            } else if (item->sval == sv_rod_light
                       || item->sval == sv_rod_fire_bolt
                       || item->sval == sv_rod_elec_bolt
                       || item->sval == sv_rod_cold_bolt
                       || item->sval == sv_rod_acid_bolt) {
                borg.trait[BI_AROD1] += item->iqty;
            } else if (item->sval == sv_rod_drain_life
                       || item->sval == sv_rod_fire_ball
                       || item->sval == sv_rod_elec_ball
                       || item->sval == sv_rod_cold_ball
                       || item->sval == sv_rod_acid_ball) {
                borg.trait[BI_AROD2] += item->iqty;
            }
            break;

        /* Bastones */
        case TV_WAND:

            /* Analizar cada uno */
            if (item->sval == sv_wand_teleport_away) {
                borg.trait[BI_ATPORTOTHER] += item->pval;
            }

            if (item->sval == sv_wand_stinking_cloud
                && borg.trait[BI_MAXDEPTH] < 30) {
                borg.trait[BI_GOOD_W_CHG] += item->pval;
            }

            if (item->sval == sv_wand_magic_missile
                && borg.trait[BI_MAXDEPTH] < 30) {
                borg.trait[BI_GOOD_W_CHG] += item->pval;
            }

            if (item->sval == sv_wand_annihilation) {
                borg.trait[BI_GOOD_W_CHG] += item->pval;
            }

            break;

        /* Bastones */
        case TV_STAFF:
            /* Analizar */
            if (item->sval == sv_staff_teleportation) {
                borg.trait[BI_AESCAPE] += (item->iqty);
                if (borg_activate_failure(item->tval, item->sval) < 500) {
                    borg.trait[BI_AESCAPE] += item->pval;
                }
            } else if (item->sval == sv_staff_speed) {
                borg.trait[BI_ASPEED] += item->pval;
            } else if (item->sval == sv_staff_healing)
                borg.trait[BI_AHEAL] += item->pval;
            else if (item->sval == sv_staff_the_magi)
                borg.trait[BI_ASTFMAGI] += item->pval;
            else if (item->sval == sv_staff_destruction)
                borg.trait[BI_ASTFDEST] += item->pval;
            else if (item->sval == sv_staff_power)
                borg.trait[BI_GOOD_S_CHG] += item->iqty;
            else if (item->sval == sv_staff_holiness) {
                borg.trait[BI_GOOD_S_CHG] += item->iqty;
                borg.trait[BI_AHEAL] += item->pval;
            }

            break;

        /* Frascos */
        case TV_FLASK:

            /* Usar como combustible si equipamos una linterna */
            if (borg_items[INVEN_LIGHT].sval == sv_light_lantern)
                borg.trait[BI_AFUEL] += item->iqty;

            break;

        /* Antorchas */
        case TV_LIGHT:

            /* Usar como combustible si es una antorcha y llevamos una antorcha */
            if ((item->sval == sv_light_torch && item->timeout >= 1)
                && (borg_items[INVEN_LIGHT].sval == sv_light_torch)
                && borg_items[INVEN_LIGHT].iqty) {
                borg.trait[BI_AFUEL] += item->iqty;
            }

            break;

        /* Armas */
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
            /* Estos objetos se comprueban un poco más tarde en una subrutina
             * para notar las banderas. Se hace fuera de este switch.
             */
            break;

        /* Palas y similares */
        case TV_DIGGING:

            /* Ignorar las que no valen nada (incluidas las malditas) */
            if (item->value <= 0)
                break;
            if (item->cursed)
                break;

            /* No llevar si es débil, no podrá excavar de todos modos */
            if (borg.trait[BI_DIG] < BORG_DIG)
                break;

            borg.trait[BI_ADIGGER] += item->iqty;
            break;
        }
    }

    /* los frascos de aceite son munición a bajos niveles */
    if (borg.has[kv_flask_oil] && borg.trait[BI_CLEVEL] < 15) {
        /* solo contar los primeros 15 */
        if (borg.has[kv_flask_oil] < 15)
            borg.trait[BI_AMISSILES] += borg.has[kv_flask_oil];
        else
            borg.trait[BI_AMISSILES] += 15;
    }

    /*** Procesar los Hechizos y Oraciones ***/
    /*    las activaciones de artefactos se cuentan aquí
     *  Pero algunos artefactos no se cuentan por dos razones.
     *  1.  Algunos hechizos-poderes se necesitan instantáneamente y se consideran en
     *  el código de preparación del borg. Un artefacto puede no estar cargado en el
     *  momento que lo necesita. Entonces necesitaría el hechizo y no podría
     *  lanzarlo. (ej. teletransporte, fase)
     *  2.  Un artefacto puede otorgar un poder, entonces él asume que tiene cantidades
     *  infinitas. Entonces vende sus pergaminos con el poder duplicado.
     *  Cuando llega el momento de mejorar y cambiar el artefacto, no lo hará
     *  porque su poder disminuye ya que ya no tiene los pergaminos.
     *  y no compra objetos primero.
     *
     *  Una posible solución sería que guarde algunos pergaminos como
     *  respaldo, o que elimine la bonificación para la preparación de nivel de borg_power.
     *  Por ahora creo que es mejor que no considere los artefactos
     *  cuyos poderes se consideran en borg_prep.
     */

    /* Manejar "satisfacer hambre" -> comida infinita */
    if (borg_spell_legal_fail(REMOVE_HUNGER, 80)
        || borg_spell_legal_fail(HERBAL_CURING, 80)) /* VAMPIRE_STRIKE? */
    {
        borg.trait[BI_FOOD] += 1000;
    }

    /* Manejar "identificar" -> identificaciones infinitas */
    if (borg_spell_legal(IDENTIFY_RUNE)) {
        borg.trait[BI_AID] += 1000;
    }

    /* Manejar "detectar trampas" */
    if (borg_spell_legal(FIND_TRAPS_DOORS_STAIRS)
        || borg_spell_legal(DETECTION)) {
        borg.trait[BI_ADETTRAP] = 1000;
    }

    /* Manejar "detectar malvados y monstruos" */
    if (borg_spell_legal(REVEAL_MONSTERS) || borg_spell_legal(DETECT_LIFE)
        || borg_spell_legal(DETECT_EVIL) || borg_spell_legal(READ_MINDS)
        || borg_spell_legal(DETECT_MONSTERS) || borg_spell_legal(SEEK_BATTLE)) {
        borg.trait[BI_ADETEVIL] = 1000;
    }

    /* Manejar DETECTION */
    if (borg_spell_legal(DETECTION)
        || borg_equips_item(act_enlightenment, false)
        || borg_equips_item(act_clairvoyance, false)) {
        borg.trait[BI_ADETDOOR] = 1000;
        borg.trait[BI_ADETTRAP] = 1000;
        borg.trait[BI_ADETEVIL] = 1000;
    }

    /* Manejar "Ver Invisible" de una manera especial. */
    if (borg_spell_legal(SENSE_INVISIBLE)) {
        borg.trait[BI_DINV] = true;
    }

    /* Manejar "mapeo mágico" */
    if (borg_spell_legal(SENSE_SURROUNDINGS)
        || borg_equips_item(act_detect_all, false)
        || borg_equips_item(act_mapping, false)) {
        borg.trait[BI_ADETDOOR]  = 1000;
        borg.trait[BI_ADETTRAP]  = 1000;
        borg.trait[BI_AMAGICMAP] = 1000;
    }

    /* Manejar "llamar luz" */
    if (borg_spell_legal(LIGHT_ROOM) || borg_equips_item(act_light, false)
        || borg_equips_item(act_illumination, false)
        || borg_spell_legal(CALL_LIGHT)) {
        borg.trait[BI_ALITE] += 1000;
    }

    /* Manejar PROTECTION_FROM_EVIL */
    if (borg_spell_legal(PROTECTION_FROM_EVIL)
        || borg_equips_item(act_protevil, false) || borg.has[kv_staff_holiness]
        || borg_equips_item(act_staff_holy, false)) {
        borg.trait[BI_APFE] += 1000;
    }

    /* Manejar "runa de protección" glifo" */
    if (borg_spell_legal(GLYPH_OF_WARDING)
        || borg_equips_item(act_glyph, false)) {
        borg.trait[BI_AGLYPH] += 1000;
    }

    /* Manejar "detectar trampas/puertas" */
    if (borg_spell_legal(FIND_TRAPS_DOORS_STAIRS)) {
        borg.trait[BI_ADETDOOR] = 1000;
        borg.trait[BI_ADETTRAP] = 1000;
    }

    /* Manejar ENCHANT_WEAPON */
    if (borg_spell_legal_fail(ENCHANT_WEAPON, 65)
        || borg_equips_item(act_enchant_weapon, false)) {
        borg.trait[BI_AENCH_TOH] += 1000;
        borg.trait[BI_AENCH_TOD] += 1000;
        borg.trait[BI_AENCH_SWEP] += 1000;
    }
    if (borg_equips_item(act_enchant_tohit, false)) {
        borg.trait[BI_AENCH_TOH] += 1000;
    }
    if (borg_equips_item(act_enchant_todam, false)) {
        borg.trait[BI_AENCH_TOD] += 1000;
    }

    /* Manejar "Marcar Arma (proyectiles)" */
    if (borg_equips_item(act_firebrand, false)
        || borg_spell_legal_fail(BRAND_AMMUNITION, 65)) {
        borg.trait[BI_ABRAND] += 1000;
    }

    /* Manejar "encantar armadura" */
    if (borg_spell_legal_fail(ENCHANT_ARMOUR, 65)
        || borg_equips_item(act_enchant_armor, false)
        || borg_equips_item(act_enchant_armor2, false)) {
        borg.trait[BI_AENCH_ARM] += 1000;
        borg.trait[BI_AENCH_SARM] += 1000;
    }

    /* Manejar Excavadores (piedra a lodo) */
    if (borg_spell_legal_fail(TURN_STONE_TO_MUD, 40)
        || borg_equips_item(act_stone_to_mud, false)
        || borg_equips_ring(sv_ring_digging)) {
        borg.trait[BI_ADIGGER] += 1;
    }

    /* Manejar recuerdo */
    if (borg_spell_legal_fail(WORD_OF_RECALL, 40)
        || (borg.trait[BI_CDEPTH] == 100 && borg_spell_legal(WORD_OF_RECALL))) {
        borg.trait[BI_RECALL] += 1000;
    }
    if (borg_equips_item(act_recall, false)) {
        borg.trait[BI_RECALL] += 1;
    }

    /* Manejar teleport_level */
    if (borg_spell_legal_fail(TELEPORT_LEVEL, 20)) {
        borg.trait[BI_ATELEPORTLVL] += 1000;
    }

    /* Manejar el hechizo PhaseDoor cuidadosamente */
    if (borg_spell_legal_fail(PHASE_DOOR, 3)) {
        borg.trait[BI_APHASE] += 1000;
    }
    if (borg_equips_item(act_tele_phase, false)) {
        borg.trait[BI_APHASE] += 1;
    }

    /* Manejar el hechizo teleport cuidadosamente */
    if (borg_spell_legal_fail(TELEPORT_SELF, 1)
        || borg_spell_legal_fail(PORTAL, 1)
        || borg_spell_legal_fail(SHADOW_SHIFT, 1)
        || borg_spell_legal_fail(DIMENSION_DOOR, 1)) {
        borg.trait[BI_ATELEPORT] += 1000;
    }
    if (borg_equips_item(act_tele_long, false)) {
        borg.trait[BI_AESCAPE] += 1;
        borg.trait[BI_ATELEPORT] += 1;
    }

    /* Manejar teletransportar a otro */
    if (borg_spell_legal_fail(TELEPORT_OTHER, 40)) {
        borg.trait[BI_ATPORTOTHER] += 1000;
    }

    /* Manejar la oración Palabra Santa solo para ver si es legal */
    if (borg_spell_legal(HOLY_WORD)) {
        borg.trait[BI_AHWORD] += 1000;
    }

    /* hechizos de velocidad HASTE*/
    if (borg_spell_legal(HASTE_SELF) || borg_equips_item(act_haste, false)
        || borg_equips_item(act_haste1, false)
        || borg_equips_item(act_haste2, false)) {
        borg.trait[BI_ASPEED] += 1000;
    }

    /* Manejar "curar heridas leves" */
    if (borg_equips_item(act_cure_light, false)) {
        borg.trait[BI_ACLW] += 1000;
    }

    /* Manejar "curar heridas graves" */
    if (borg_equips_item(act_cure_serious, false)) {
        borg.trait[BI_ACSW] += 1000;
    }

    /* Manejar "curar heridas críticas" */
    if (borg_equips_item(act_cure_critical, false)) {
        borg.trait[BI_ACCW] += 1000;
    }

    /* Manejar "sanar" */
    if (borg_equips_item(act_cure_full, false)
        || borg_equips_item(act_cure_full2, false)
        || borg_equips_item(act_cure_nonorlybig, false)
        || borg_equips_item(act_heal1, false)
        || borg_equips_item(act_heal2, false)
        || borg_equips_item(act_heal3, false) || borg_spell_legal(HEALING)) {
        borg.trait[BI_AHEAL] += 1000;
    }

    /* Manejar "arreglar exp" */
    if (borg_equips_item(act_cure_nonorlybig, false)
        || borg_equips_item(act_restore_exp, false)
        || borg_equips_item(act_restore_st_lev, false)
        || borg_equips_item(act_restore_life, false)) {
        borg.trait[BI_HASFIXEXP] = true;
    }

    /* Manejar REMEMBRANCE -- es tan bueno como Retener Vida */
    if (borg_spell_legal(REMEMBRANCE)
        || borg_equips_item(act_cure_nonorlybig, false)
        || borg_equips_item(act_restore_exp, false)
        || borg_equips_item(act_restore_st_lev, false)
        || borg_equips_item(act_restore_life, false)) {
        borg.trait[BI_HLIFE] = true;
    }

    /* Manejar "recargar" */
    if (borg_equips_item(act_recharge, false) || borg_spell_legal(RECHARGING)) {
        borg.trait[BI_ARECHARGE] += 1000;
    }

    /*** Procesar las Necesidades ***/

    /* No hay necesidad de combustible si sabemos que no lo necesita */
    if (of_has(borg_items[INVEN_LIGHT].flags, OF_NO_FUEL)
        || (borg.trait[BI_CLASS] == CLASS_NECROMANCER))
        borg.trait[BI_AFUEL] += 1000;

    /* No hay necesidad de *comprar* pociones de aumento de estadísticas */
    if (borg.trait[BI_CSTR] < (18 + 100))
        borg.need_statgain[STAT_STR] = true;

    if (borg.trait[BI_CINT] < (18 + 100))
        borg.need_statgain[STAT_INT] = true;

    if (borg.trait[BI_CWIS] < (18 + 100))
        borg.need_statgain[STAT_WIS] = true;

    if (borg.trait[BI_CDEX] < (18 + 100))
        borg.need_statgain[STAT_DEX] = true;

    if (borg.trait[BI_CCON] < (18 + 100))
        borg.need_statgain[STAT_CON] = true;

    /* No hay necesidad de reparación de experiencia */
    if (!borg.trait[BI_ISFIXEXP])
        borg.trait[BI_HASFIXEXP] = true;

    /* Corregir las comidas de alta y baja caloría */
    borg.trait[BI_FOOD] += borg.trait[BI_FOOD_HI];
    borg.trait[BI_FOOD] += borg.trait[BI_FOOD_LO];

    /* Si está débil, no contar los hechizos de comida */
    if (borg.trait[BI_ISWEAK] && (borg.trait[BI_FOOD] >= 1000))
        borg.trait[BI_FOOD] -= 1000;
}

/*
 * Analizar el equipo y el inventario
 */
void borg_notice(bool notice_swap)
{
    /* Limpiar los arrays de rasgos */
    memset(borg.has, 0, z_info->k_max * sizeof(int));
    memset(borg.trait, 0, BI_MAX * sizeof(int));
    memset(borg.activation, 0, z_info->act_max * sizeof(int));

    /* Empezar con un solo golpe por turno */
    borg.trait[BI_BLOWS] = 1;

    /* la velocidad empieza en 110 */
    borg.trait[BI_SPEED] = 110;

    /* Reiniciar los atributos de "munición" */
    borg.trait[BI_AMMO_TVAL] = -1;
    borg.trait[BI_AMMO_SIDES] = 4;

    /* Muchas de nuestras variables están ligadas a borg.trait[], que se borra al
     * inicio de borg_notice(). Así que debemos actualizar el marco donde se
     * incorporan todas las habilidades no relacionadas con el inventario.
     */
    borg_notice_player();

    /*** Procesar libros/hechizos ***/
    if (borg_do_spell) {
        borg_cheat_spells();
        borg_do_spell = false;
    }

    /* Notificar el equipo */
    borg_notice_equipment();

    /* Notificar el inventario */
    borg_notice_inventory();

    /* Notificar y localizar mi arma de intercambio */
    if (notice_swap) {
        borg_notice_weapon_swap();
        borg_notice_armour_swap();
    }
    borg.trait[BI_SRACID]
        = borg.trait[BI_RACID] || armour_swap_resist_acid
          || weapon_swap_resist_acid
          || borg_spell_legal_fail(RESISTANCE, 15); /* Res FECAP */
    borg.trait[BI_SRELEC]
        = borg.trait[BI_RELEC] || armour_swap_resist_elec
          || weapon_swap_resist_elec
          || borg_spell_legal_fail(RESISTANCE, 15); /* Res FECAP */
    borg.trait[BI_SRFIRE]
        = borg.trait[BI_RFIRE] || armour_swap_resist_fire
          || weapon_swap_resist_fire
          || borg_spell_legal_fail(RESISTANCE, 15); /* Res FECAP */
    borg.trait[BI_SRCOLD]
        = borg.trait[BI_RCOLD] || armour_swap_resist_cold
          || weapon_swap_resist_cold
          || borg_spell_legal_fail(RESISTANCE, 15); /* Res FECAP */
    borg.trait[BI_SRPOIS]
        = borg.trait[BI_RPOIS] || armour_swap_resist_pois
          || weapon_swap_resist_pois
          || borg_spell_legal_fail(RESIST_POISON, 15); /* Res P */
    borg.trait[BI_SRFEAR] = borg.trait[BI_RFEAR] || armour_swap_resist_fear
                            || weapon_swap_resist_fear;
    borg.trait[BI_SRLITE] = borg.trait[BI_RLITE] || armour_swap_resist_light
                            || weapon_swap_resist_light;
    borg.trait[BI_SRDARK] = borg.trait[BI_RDARK] || armour_swap_resist_dark
                            || weapon_swap_resist_dark;
    borg.trait[BI_SRBLIND] = borg.trait[BI_RBLIND] || armour_swap_resist_blind
                             || weapon_swap_resist_blind;
    borg.trait[BI_SRCONF] = borg.trait[BI_RCONF] || armour_swap_resist_conf
                            || weapon_swap_resist_conf;
    borg.trait[BI_SRSND] = borg.trait[BI_RSND] || armour_swap_resist_sound
                           || weapon_swap_resist_sound;
    borg.trait[BI_SRSHRD] = borg.trait[BI_RSHRD] || armour_swap_resist_shard
                            || weapon_swap_resist_shard;
    borg.trait[BI_SRNXUS] = borg.trait[BI_RNXUS] || armour_swap_resist_nexus
                            || weapon_swap_resist_nexus;
    borg.trait[BI_SRNTHR] = borg.trait[BI_RNTHR] || armour_swap_resist_neth
                            || weapon_swap_resist_neth;
    borg.trait[BI_SRKAOS] = borg.trait[BI_RKAOS] || armour_swap_resist_chaos
                            || weapon_swap_resist_chaos;
    borg.trait[BI_SRDIS] = borg.trait[BI_RDIS] || armour_swap_resist_disen
                           || weapon_swap_resist_disen;
    borg.trait[BI_SHLIFE] = borg.trait[BI_HLIFE] || armour_swap_hold_life
                            || weapon_swap_hold_life;
    borg.trait[BI_SFRACT]
        = borg.trait[BI_FRACT] || armour_swap_free_act || weapon_swap_free_act;

    /* Aplicar "sobrecarga" por peso */
    /* Extraer el "límite de peso" (en décimas de libra) */
    borg.trait[BI_CARRY] = borg_adj_str_wgt[borg.trait[BI_STR_INDEX]] * 100;

    /* Aplicar "sobrecarga" por peso */
    if (borg.trait[BI_WEIGHT] > borg.trait[BI_CARRY] / 2)
        borg.trait[BI_SPEED]
            -= ((borg.trait[BI_WEIGHT] - (borg.trait[BI_CARRY] / 2))
                / (borg.trait[BI_CARRY] / 10));

    /* velocidad máxima */
    if (borg.trait[BI_SPEED] > 199)
        borg.trait[BI_SPEED] = 199;

    /* Comprobar mi proporción para variables decrecientes */
    if (borg.trait[BI_SPEED] > 110) {
        borg_game_ratio = 100000 / (((borg.trait[BI_SPEED] - 110) * 10) + 100);
    } else {
        borg_game_ratio = 1000;
    }

    /* establecer si nos estamos preparando para luchar contra morgoth o sauron */
    borg.trait[BI_PREP_BIG_FIGHT] = false;
    if (borg.trait[BI_MAXDEPTH] >= 99) {

        /* Examinar el hogar */
        borg_notice_home(NULL, false);

        /* poción de sanación + *sanación* + vida */
        int total_big_heal = borg.has[kv_potion_healing];
        total_big_heal += borg.trait[BI_AEZHEAL];
        total_big_heal += borg.trait[BI_ALIFE];

        /* más lo mismo en el hogar */
        total_big_heal += num_heal_true;
        total_big_heal += num_ezheal_true;
        total_big_heal += num_life_true;

        /* queremos montones de sanación y velocidad para sentirnos preparados para la lucha */
        if (total_big_heal < 30 || (num_speed + borg.trait[BI_ASPEED]) < 15)
            borg.trait[BI_PREP_BIG_FIGHT] = true;
    }
}

/*
 * Actualizar el Borg basado en los valores actuales del jugador
 */
void borg_notice_player(void)
{
    int i;

    /*** Extraer la Clase ***/
    borg.trait[BI_CLASS] = player->class->cidx;

    /* Asumir que el nivel está bien */
    borg.trait[BI_ISFIXLEV] = false;

    /* Notar "Lev" vs "LEV" */
    if (player->lev < player->max_lev)
        borg.trait[BI_ISFIXLEV] = true;

    /* Extraer "LEVEL xxxxxx" */
    borg.trait[BI_CLEVEL] = player->lev;

    /* tomar el máximo nivel */
    borg.trait[BI_MAXCLEVEL] = player->max_lev;

    /* Notar "Ganador" */
    borg.trait[BI_KING] = player->total_winner;

    /* Asumir que la experiencia está bien */
    borg.trait[BI_ISFIXEXP] = false;

    /* Acceder a la profundidad */
    borg.trait[BI_CDEPTH] = player->depth;

    /* Acceder a la profundidad máxima */
    borg.trait[BI_MAXDEPTH] = player->max_depth;

    /* Notar "Exp" vs "EXP" y si soy más bajo que el nivel 50*/
    if (player->exp < player->max_exp) {
        /* arreglarlo si está en la ciudad */
        if (borg.trait[BI_CLEVEL] == 50 && borg.trait[BI_CDEPTH] == 0)
            borg.trait[BI_ISFIXEXP] = true;

        /* no preocuparse por arreglarlo en la mazmorra */
        if (borg.trait[BI_CLEVEL] == 50 && borg.trait[BI_CDEPTH] >= 1)
            borg.trait[BI_ISFIXEXP] = false;

        /* No está en el Nivel Máximo */
        if (borg.trait[BI_CLEVEL] != 50)
            borg.trait[BI_ISFIXEXP] = true;
    }

    /* Extraer "AU xxxxxxxxx" */
    borg.trait[BI_GOLD] = player->au;

    /* Un pequeño truco para ver si me perdí un mensaje sobre mi estado en algunos */
    /* hechizos temporizados */
    if (!borg.goal.recalling && player->word_recall)
        borg.goal.recalling = player->word_recall * 1000;
    if (borg.goal.recalling && !player->word_recall)
        borg.goal.recalling = 0;
    if (!borg.temp.prot_from_evil && player->timed[TMD_PROTEVIL])
        borg.temp.prot_from_evil = (player->timed[TMD_PROTEVIL] ? true : false);
    if (!borg.temp.fast
        && (player->timed[TMD_FAST] || player->timed[TMD_SPRINT]
            || player->timed[TMD_TERROR]))
        (borg.temp.fast = (player->timed[TMD_FAST] || player->timed[TMD_SPRINT]
                              || player->timed[TMD_TERROR])
                              ? true
                              : false);
    borg.temp.res_acid = (player->timed[TMD_OPP_ACID] ? true : false);
    borg.temp.res_elec = (player->timed[TMD_OPP_ELEC] ? true : false);
    borg.temp.res_fire = (player->timed[TMD_OPP_FIRE] ? true : false);
    borg.temp.res_cold = (player->timed[TMD_OPP_COLD] ? true : false);
    borg.temp.res_pois = (player->timed[TMD_OPP_POIS] ? true : false);
    borg.temp.bless    = (player->timed[TMD_BLESSED] ? true : false);
    borg.temp.shield
        = (player->timed[TMD_SHIELD] || player->timed[TMD_STONESKIN] ? true
                                                                     : false);
    borg.temp.fastcast   = (player->timed[TMD_FASTCAST] ? true : false);
    borg.temp.hero       = (player->timed[TMD_HERO] ? true : false);
    borg.temp.berserk    = (player->timed[TMD_SHERO] ? true : false);
    borg.temp.regen      = (player->timed[TMD_HEAL] ? true : false);
    borg.temp.venom      = (player->timed[TMD_ATT_POIS] ? true : false);
    borg.temp.smite_evil = (player->timed[TMD_ATT_EVIL] ? true : false);
    if (!borg.see_inv && player->timed[TMD_SINVIS])
        borg.see_inv = 1000;

    /* Extraer "HP actual xxxxx" */
    borg.trait[BI_CURHP] = player->chp;

    /* Extraer "HP máximo xxxxx" */
    borg.trait[BI_MAXHP] = player->mhp;

    /* Extraer "SP actual xxxxx" (o cero) */
    borg.trait[BI_CURSP] = player->csp;

    /* Extraer "SP máximo xxxxx" (o cero) */
    borg.trait[BI_MAXSP] = player->msp;

    /* Limpiar todas las "banderas de estado" */
    borg.trait[BI_ISWEAK] = borg.trait[BI_ISHUNGRY] = borg.trait[BI_ISFULL]
        = borg.trait[BI_ISGORGED]                   = false;
    borg.trait[BI_ISBLIND] = borg.trait[BI_ISCONFUSED] = borg.trait[BI_ISAFRAID]
        = borg.trait[BI_ISPOISONED]                    = false;
    borg.trait[BI_ISCUT] = borg.trait[BI_ISSTUN] = borg.trait[BI_ISHEAVYSTUN]
        = borg.trait[BI_ISIMAGE] = borg.trait[BI_ISSTUDY] = false;
    borg.trait[BI_ISPARALYZED]                            = false;
    borg.trait[BI_ISFORGET]                               = false;

    /* Comprobar "Débil" */
    if (player->timed[TMD_FOOD] < PY_FOOD_WEAK)
        borg.trait[BI_ISWEAK] = borg.trait[BI_ISHUNGRY] = true;

    /* Comprobar "Hambriento" */
    else if (player->timed[TMD_FOOD] < PY_FOOD_HUNGRY)
        borg.trait[BI_ISHUNGRY] = true;

    /* Comprobar "Normal" */
    else if (player->timed[TMD_FOOD] < PY_FOOD_FULL) /* Nada */
        ;

    /* Comprobar "Lleno" */
    else if (player->timed[TMD_FOOD] < PY_FOOD_MAX)
        borg.trait[BI_ISFULL] = true;

    /* Comprobar "Hinchado" */
    else
        borg.trait[BI_ISGORGED] = borg.trait[BI_ISFULL] = true;

    /* Comprobar "Ciego" */
    if (player->timed[TMD_BLIND])
        borg.trait[BI_ISBLIND] = true;

    /* Comprobar "Confundido" */
    if (player->timed[TMD_CONFUSED])
        borg.trait[BI_ISCONFUSED] = true;

    /* Comprobar "Asustado" */
    if (player->timed[TMD_AFRAID])
        borg.trait[BI_ISAFRAID] = true;

    /* Comprobar "Envenenado" */
    if (player->timed[TMD_POISONED])
        borg.trait[BI_ISPOISONED] = true;

    /* Comprobar cualquier texto */
    if (player->timed[TMD_CUT])
        borg.trait[BI_ISCUT] = true;

    /* Comprobar Aturdido */
    if (player->timed[TMD_STUN] && (player->timed[TMD_STUN] <= 50))
        borg.trait[BI_ISSTUN] = true;

    /* Comprobar Aturdido Fuerte */
    if (player->timed[TMD_STUN] > 50)
        borg.trait[BI_ISHEAVYSTUN] = true;

    /* Comprobar Paralizado */
    if (player->timed[TMD_PARALYZED] > 50)
        borg.trait[BI_ISPARALYZED] = true;

    /* Comprobar "Alucinando" */
    if (player->timed[TMD_IMAGE])
        borg.trait[BI_ISIMAGE] = true;

    /* Comprobar "Amnesia" */
    if (player->timed[TMD_AMNESIA])
        borg.trait[BI_ISFORGET] = true;

    /* Comprobar "Estudio" */
    if (player->upkeep->new_spells)
        borg.trait[BI_ISSTUDY] = true;

    /* Analizar estadísticas */
    for (i = 0; i < 5; i++) {
        borg.trait[BI_ISFIXSTR + i]
            = player->stat_cur[STAT_STR + i] < player->stat_max[STAT_STR + i];
        borg.trait[BI_CSTR + i] = player->stat_cur[STAT_STR + i];
    }

    /* Rastrear si Sauron está muerto */
    borg.trait[BI_SAURON_DEAD] = borg_race_death[borg_sauron_id];
}

void borg_trait_init(void)
{
    borg.has        = mem_zalloc(z_info->k_max * sizeof(int));
    borg.trait      = mem_zalloc(BI_MAX * sizeof(int));
    borg.activation = mem_zalloc(z_info->act_max * sizeof(int));
}

void borg_trait_free(void)
{
    mem_free(borg.has);
    borg.has = NULL;
    mem_free(borg.trait);
    borg.trait = NULL;
    mem_free(borg.activation);
    borg.activation = NULL;
}

#endif