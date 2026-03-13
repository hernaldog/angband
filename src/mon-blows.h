/**
 * \archivo mon-blows.h
 * \brief Funciones para manejar el combate cuerpo a cuerpo de monstruos.
 *
 * Copyright (c) 1997 Ben Harrison, David Reeve Sward, Keldon Jones.
 *               2013 Ben Semmler
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

#ifndef MON_BLOWS_H
#define MON_BLOWS_H

#include "player.h"
#include "monster.h"

struct blow_message {
    char *act_msg;
    struct blow_message *next;
};

struct blow_method {
    char *name;
    bool cut;
    bool stun;
    bool miss;
    bool phys;
    int msgt;
    struct blow_message *messages;
    int num_messages;
    char *desc;
    struct blow_method *next;
};

extern struct blow_method *blow_methods;

/**
 * Almacenamiento para información de contexto para los manejadores de efectos
 * llamados en make_attack_normal().
 *
 * Los miembros de esta estructura se inicializan en un orden dependiente
 * (para ser más multiplataforma). Si los miembros cambian, asegúrate de
 * cambiar cualquier inicializador. Idealmente, esto debería usar inicializadores
 * con nombre en el futuro.
 */
typedef struct melee_effect_handler_context_s {
    struct player * const p;    /* Objetivo (si es jugador) */
    struct monster * const mon; /* Atacante */
    struct monster * const t_mon;   /* Objetivo (si es otro monstruo) */
    const int rlev;
    const struct blow_method *method;
    const int ac;
    const char *ddesc;      /* nombre corto del monstruo para mensajes
                        de muerte; sin uso si el objetivo no
                        es el jugador */
    bool obvious;
    bool blinked;
    int damage;
    const char *m_name;     /* nombre del monstruo para mensajes */
} melee_effect_handler_context_t;

/**
 * Manejador de efectos de golpes cuerpo a cuerpo.
 */
typedef void (*melee_effect_handler_f)(melee_effect_handler_context_t *);

struct blow_effect {
    char *name;
    int power;
    int eval;
    char *desc;
    uint8_t lore_attr;      /* Color del ataque usado en el texto de lore */
    uint8_t lore_attr_resist;   /* Color usado en el texto de lore cuando es resistido */
    uint8_t lore_attr_immune;   /* Color usado en el texto de lore cuando es resistido fuertemente */
    char *effect_type;
    int resist;
    int lash_type;
    struct blow_effect *next;
};

extern struct blow_effect *blow_effects;

/* Funciones */
int blow_index(const char *name);
char *monster_blow_method_action(const struct blow_method *method, int midx);
extern melee_effect_handler_f melee_handler_for_blow_effect(const char *name);

#endif /* MON_BLOWS_H */
