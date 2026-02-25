/**
 * \file borg-messages.c
 * \brief Código para leer y analizar los mensajes que provienen del juego
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
 * b) the "Angband License":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "borg-messages.h"

#ifdef ALLOW_BORG

#include "../mon-msg.h"
#include "../ui-term.h"

#include "borg-cave.h"
#include "borg-danger.h"
#include "borg-fight-attack.h"
#include "borg-fight-defend.h"
#include "borg-flow-glyph.h"
#include "borg-flow-kill.h"
#include "borg-flow-stairs.h"
#include "borg-io.h"
#include "borg-messages-react.h"
#include "borg-think.h"
#include "borg-trait.h"
#include "borg-update.h"
#include "borg.h"

/*
 * Memoria de mensajes
 */
int16_t  borg_msg_len;
int16_t  borg_msg_siz;
char    *borg_msg_buf;
int16_t  borg_msg_num;
int16_t  borg_msg_max;
int16_t *borg_msg_pos;
int16_t *borg_msg_use;

static char **suffix_pain;

/*
 * Cadena de búsqueda de mensajes de estado
 */
char borg_match[128] = "anillo de oro liso";

/*
 * Métodos para matar a un monstruo (el orden no es importante).
 *
 * Véase "mon_take_hit()" para más detalles.
 */
static const char *prefix_kill[]
    = { "Has matado a ", 
        "Has derrotado a ", 
        "Has destruido a ", 
        NULL };

/*
 * Métodos de muerte de monstruos (el orden no es importante).
 *
 * Véase "project_m()", "do_cmd_fire()", "mon_take_hit()" para más detalles.
 * !FIX this should use MON_MSG* 
 */
static const char *suffix_died[] = { 
    " muere.",
    " muere.",
    " es destruido.", 
    " son destruidos.",
    " es destruido!",
    " son destruidos!",
    " se marchitan en la luz!",
    " se marchita en la luz!",
    " se disuelven!",
    " se disuelve!",
    " gritan de agonía!",
    " grita de agonía!",
    " se desintegran!",
    " se desintegra!",
    " se congela y se hace pedazos!",
    " se congela y se hace pedazos!",
    " queda completamente drenado!",
    NULL };

static const char *suffix_blink[] = { 
    " desaparece!", /* de teleportar a otro */
    " entona extrañas palabras.", /* de hechizo de polimorfismo */
    " se teletransporta.", /* RF6_TPORT */
    " parpadea.", /* RF6_BLINK */
    " hace un suave 'pop'.", 
    NULL };

/* a message can have up to three parts broken up by variables */
/* ex: "{name} hits {pronoun} followers with {type} ax." */
/* " hits ", " followers with ", " ax." */
/* if the message has more parts than that, they are ignored so  */
/* ex: "{name} hits {pronoun} followers with {type} ax and {type} breath." */
/* would end up as */
/* " hits ", " followers with ", " ax and " */
/* hopefully this is enough to keep the messages as unique as possible */
struct borg_read_message {
    char *message_p1;
    char *message_p2;
    char *message_p3;
};

struct borg_read_messages {
    int                       count;
    int                       allocated;
    struct borg_read_message *messages;
    int                      *index;
};

/*  methods of hitting the player */
static struct borg_read_messages suffix_hit_by;

/*  methods of casting spells at the player */
static struct borg_read_messages spell_msgs;
static struct borg_read_messages spell_invis_msgs;

/* check a message for a string */
static bool borg_message_contains(
    const char *value, struct borg_read_message *message)
{
    if (strstr(value, message->message_p1)
        && (!message->message_p2 || strstr(value, message->message_p2))
        && (!message->message_p3 || strstr(value, message->message_p3)))
        return true;
    return false;
}

/*
 * Sensaciones espontáneas del nivel (el orden es importante).
 *
 * Véase "do_cmd_feeling()" para más detalles.
 * !FIX !TODO: Hacer esto más robusto a cambios en los mensajes de sensación del nivel.
 */
static const char *prefix_feeling_danger[] = {
    "Aún no estás seguro sobre este lugar",
    "Presagios de muerte acechan este lugar", 
    "Este lugar parece asesino",
    "Este lugar parece terriblemente peligroso", 
    "Te sientes ansioso en este lugar",
    "Te sientes nervioso en este lugar", 
    "Este lugar no parece demasiado arriesgado",
    "Este lugar parece razonablemente seguro", 
    "Este parece un lugar tranquilo y resguardado",
    "Este parece un lugar silencioso y pacífico", 
    NULL
};

static const char *suffix_feeling_stuff[] = { 
    "Parece un nivel como cualquier otro.",
    "¡percibes un objeto de poder maravilloso!", 
    "hay tesoros soberbios aquí.",
    "hay excelentes tesoros aquí.",
    "hay muy buenos tesoros aquí.", 
    "hay buenos tesoros aquí.",
    "puede haber algo que valga la pena aquí.",
    "puede que no haya mucho interesante aquí.",
    "no hay muchos tesoros aquí.", 
    "solo hay fragmentos de basura aquí.",
    "no hay más que telarañas aquí.", 
    NULL };

/*
 * Analizar un mensaje del mundo
 *
 * Nótese que detectar la "muerte" es EXTREMADAMENTE importante para prevenir
 * todo tipo de errores que surgen al intentar analizar la pantalla de
 * "tumba", y para permitir al usuario "observar" la "causa" de la muerte.
 *
 * Nótese que detectar el "fallo" es EXTREMADAMENTE importante para prevenir
 * situaciones extrañas al fallar el uso de un bastón de percepciones,
 * que de lo contrario enviaría el "índice de objeto" que podría ser un
 * comando válido (como "a" para "apuntar"). Este método es necesario porque
 * el Borg no puede analizar "solicitudes", y debe asumir el éxito del
 * comando que genera la solicitud, salvo que se le indique lo contrario
 * mediante un mensaje de fallo. Además, necesitamos detectar el fallo
 * porque algunos comandos, como los hechizos de detección, necesitan
 * desencadenar procesamiento adicional si tienen éxito, pero los mensajes
 * solo se emiten si el comando falla.
 *
 * Nótese que ciertos otros mensajes pueden contener información útil,
 * por lo que se "analizan" y se envían a "borg_react()", que simplemente
 * pone en cola los mensajes para su análisis posterior en el contexto adecuado.
 *
 * Junto con el mensaje real, enviamos un buffer con formato especial,
 * que contiene un "código de operación" inicial, que puede contener información
 * adicional, como el índice de un hechizo, y un "argumento" (por ejemplo, el
 * nombre en mayúsculas de un monstruo), con "dos puntos" como separador.
 *
 * XXX XXX XXX Varias cadenas de mensajes incluyen un "posesivo" de la forma
 * "his" o "her" o "its". Estas cadenas están todas representadas por la
 * forma codificada "XXX" en las distintas cadenas de coincidencia.
 * Desafortunadamente, la forma codificada nunca se decodifica, por lo que el
 * Borg actualmente ignora los mensajes sobre varios hechizos (curarse a sí
 * mismo y acelerarse a sí mismo).
 *
 * XXX XXX XXX Detectamos algunos mensajes de "características del terreno"
 * para adquirir conocimiento sobre tipos de paredes y tipos de puertas.
 */
static void borg_parse_aux(char *msg, int len)
{
    int i, tmp;

    int y9;
    int x9;
    int ax, ay;
    int d;

    char who[256];
    char buf[256];

    borg_grid *ag = &borg_grids[borg.goal.g.y][borg.goal.g.x];

    /* Log (if needed) */
    if (borg_cfg[BORG_VERBOSE])
        borg_note(format("# Analizar Msg <%s>", msg));

    /* Detectar muerte */
    if (prefix(msg, "Moriste.")) {
        /* Abort (unless cheating) */
        if (!(player->wizard || OPT(player, cheat_live) || borg_cheat_death)) {
            /* Abort */
            borg_oops("death");

            /* Abort right now! */
            borg_active = false;
            /* Noise XXX XXX XXX */
            Term_xtra(TERM_XTRA_NOISE, 1);
        }

        /* Done */
        return;
    }

    /* Detectar "fallo" */
    if (prefix(msg, "Fallaste ")) {
        /* Store the keypress */
        borg_note("# Fallo normal.");

        /* Set the failure flag */
        borg_failure = true;

        /* Flush our key-buffer */
        borg_flush();

        /* If we were casting a targetted spell and failed */
        /* it does not mean we can't target that location */
        successful_target = 0;

        /* In case we failed our emergency use of MM */
        borg_confirm_target = false;

        /* In case it was a Resistance refresh */
        if (borg_attempting_refresh_resist) {
            if (borg.resistance > 1)
                borg.resistance -= 25000;
            borg_attempting_refresh_resist = false;
        }

        return;
    }

    /* Mega-Hack -- Check against the search string */
    if (borg_match[0] && strstr(msg, borg_match)) {
        /* Clean cancel */
        borg_cancel = true;
    }

    /* Ignorar trampa de teletransporte */
    if (prefix(msg, "Golpeaste una teletransportación"))
        return;

    /* Ignorar trampas de flechas */
    if (prefix(msg, "Una flecha "))
        return;

    /* Ignorar trampas de dardos */
    if (prefix(msg, "Un pequeño dardo "))
        return;

    if (prefix(msg, "La cueva ")) {
        borg_react(msg, "QUAKE:Somebody");
        borg_needs_new_sea = true;
        return;
    }

    if (prefix(msg, "Tienes demasiado miedo para atacar ")) {
        tmp = strlen("Tienes demasiado miedo para atacar ");
        strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
        strnfmt(buf, 256, "AFRAID:%s", who);
        borg_react(msg, buf);
        return;
    }

    /* amnesia attacks, re-id wands, staves, equipment. */
    if (prefix(msg, "Sientes que tus recuerdos se desvanecen.")) {
        /* Set the borg flag */
        borg.trait[BI_ISFORGET] = true;
    }
    if (streq(msg, "Tus recuerdos regresan a raudales.")) {
        borg.trait[BI_ISFORGET] = false;
    }

    if (streq(msg, "Has sido noqueado.")) {
        borg_note("Ignorando mensajes mientras está KO'd");
        borg_dont_react = true;
    }
    if (streq(msg, "Estás paralizado")) {
        borg_note("Ignorando mensajes mientras está Paralizado");
        borg_dont_react = true;
    }

    /* Alucinación -- Inicio */
    if (streq(msg, "¡Te sientes drogado!")) {
        borg_note("# Alucinando. Control especial de varitas.");
        borg.trait[BI_ISIMAGE] = true;
    }

    if (streq(msg, "El drenaje falla.")) {
        borg_react(msg, "MISS_BY:something");
        return;
    }

    /* Alucinación -- Fin */
    if (streq(msg, "Puedes ver claramente de nuevo.")) {
        borg_note("# Alucinación terminada. Control normal de varitas.");
        borg.trait[BI_ISIMAGE] = false;
    }

    /* Golpear a alguien */
    if (prefix(msg, "Golpeas a ")) {
        tmp = strlen("Golpeas a ");
        strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
        strnfmt(buf, 256, "HIT:%s", who);
        borg_react(msg, buf);
        return;
    }
    if (prefix(msg, "Muerdes a ")) {
        tmp = strlen("Muerdes a ");
        strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
        strnfmt(buf, 256, "HIT:%s", who);
        borg_react(msg, buf);
        target_closest = 1;
        return;
    }
    if (prefix(msg, "Extraes poder de")) {
        target_closest = 1;
        return;
    }
    if (prefix(msg, "No hay objetivo disponible.")) {
        target_closest = -12;
        return;
    }
    if (prefix(msg, "Este hechizo debe apuntar a un monstruo.")) {
        target_closest = -12;
        return;
    }
    if (prefix(msg, "No hay suficiente espacio junto a ")) {
        target_closest = -12;
        return;
    }

    /* Fallar a alguien */
    if (prefix(msg, "Fallas a ")) {
        tmp = strlen("Fallas a ");
        strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
        strnfmt(buf, 256, "MISS:%s", who);
        borg_react(msg, buf);
        return;
    }

    /* Fallar a alguien (por miedo) */
    if (prefix(msg, "Tienes demasiado miedo para atacar ")) {
        tmp = strlen("Tienes demasiado miedo para atacar ");
        strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
        strnfmt(buf, 256, "MISS:%s", who);
        borg_react(msg, buf);
        return;
    }

    /* "Your <equipment> is unaffected!"
     * Note that this check must be before the suffix_pain
     * because suffix_pain will look for 'is unaffected!' and
     * assume it is talking about a monster which in turn will
     * yield to the Player Ghost being created.
     */
    if (prefix(msg, "Tu ")) {
        if (suffix(msg, " no es afectado!")) {
            /* Your equipment ignored the attack.
             * Ignore the message
             */
            return;
        }
    } else {
        /* "It screams in pain." (etc) */
        for (i = 0; suffix_pain[i]; i++) {
            /* "It screams in pain." (etc) */
            if (suffix(msg, suffix_pain[i])) {
                tmp = strlen(suffix_pain[i]);
                strnfmt(who, 1 + len - tmp, "%s", msg);
                strnfmt(buf, 256, "PAIN:%s", who);
                borg_react(msg, buf);
                return;
            }
        }

        /* "You have killed it." (etc) */
        for (i = 0; prefix_kill[i]; i++) {
            /* "You have killed it." (etc) */
            if (prefix(msg, prefix_kill[i])) {
                tmp = strlen(prefix_kill[i]);
                strnfmt(who, 1 + len - (tmp + 1), "%s", msg + tmp);
                strnfmt(buf, 256, "KILL:%s", who);
                borg_react(msg, buf);
                return;
            }
        }

        /* "It dies." (etc) */
        for (i = 0; suffix_died[i]; i++) {
            /* "It dies." (etc) */
            if (suffix(msg, suffix_died[i])) {
                tmp = strlen(suffix_died[i]);
                strnfmt(who, 1 + len - tmp, "%s", msg);
                strnfmt(buf, 256, "DIED:%s", who);
                borg_react(msg, buf);
                return;
            }
        }

        /* "It blinks or telports." (etc) */
        for (i = 0; suffix_blink[i]; i++) {
            /* "It teleports." (etc) */
            if (suffix(msg, suffix_blink[i])) {
                tmp = strlen(suffix_blink[i]);
                strnfmt(who, 1 + len - tmp, "%s", msg);
                strnfmt(buf, 256, "BLINK:%s", who);
                borg_react(msg, buf);
                return;
            }
        }

        /* "Te falla." */
        if (suffix(msg, " te falla.")) {
            tmp = strlen(" te falla.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "MISS_BY:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* "Es repelido.." */
        /* tratar como fallo */
        if (suffix(msg, " es repelido.")) {
            tmp = strlen(" es repelido.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "MISS_BY:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* "It hits you." (etc) */
        for (i = 0; suffix_hit_by.messages[i].message_p1; i++) {
            /* "It hits you." (etc) */
            if (borg_message_contains(msg, &suffix_hit_by.messages[i])) {
                char *start = strstr(msg, suffix_hit_by.messages[i].message_p1);
                if (start) {
                    strnfmt(who, (start - msg), "%s", msg);
                    strnfmt(buf, 256, "HIT_BY:%s", who);
                    borg_react(msg, buf);

                    /* If I was hit, then I am not on a glyph */
                    if (track_glyph.num) {
                        /* erase them all and
                         * allow the borg to scan the screen and rebuild the
                         * array. He won't see the one under him though.  So a
                         * special check must be made.
                         */
                        /* Remove the entire array */
                        for (i = 0; i < track_glyph.num; i++) {
                            /* Stop if we already new about this glyph */
                            track_glyph.x[i] = 0;
                            track_glyph.y[i] = 0;
                        }
                        track_glyph.num = 0;

                        /* Check for glyphs under player -- Cheat*/
                        if (square_iswarded(cave, borg.c)) {
                            track_glyph.x[track_glyph.num] = borg.c.x;
                            track_glyph.y[track_glyph.num] = borg.c.y;
                            track_glyph.num++;
                        }
                    }
                    return;
                }
            }
        }

        for (i = 0; spell_invis_msgs.messages[i].message_p1; i++) {
            /* eliminar los mensajes que no sean de hechizos invisibles */
            if (!prefix(msg, "Algo ") && !prefix(msg, "Tú "))
                break;
            if (borg_message_contains(msg, &spell_invis_msgs.messages[i])) {
                strnfmt(buf, 256, "SPELL_%03d:%s", spell_invis_msgs.index[i],
                    "Algo");
                borg_react(msg, buf);
                return;
            }
        }
        for (i = 0; spell_msgs.messages[i].message_p1; i++) {
            if (borg_message_contains(msg, &spell_msgs.messages[i])) {
                char *start = strstr(msg, spell_msgs.messages[i].message_p1);
                if (start) {
                    strnfmt(who, (start - msg), "%s", msg);
                    strnfmt(
                        buf, 256, "SPELL_%03d:%s", spell_msgs.index[i], who);
                    borg_react(msg, buf);
                    return;
                }
            }
        }

        /* Estado -- Dormido */
        if (suffix(msg, " se queda dormido!")) {
            tmp = strlen(" se queda dormido!");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE_SLEEP:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Confundido */
        if (suffix(msg, " parece confundido.")) {
            tmp = strlen(" parece confundido.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE_CONFUSED:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Confundido */
        if (suffix(msg, " parece más confundido.")) {
            tmp = strlen(" parece más confundido.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE_CONFUSED:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Despierto */
        if (suffix(msg, " se despierta.")) {
            tmp = strlen(" se despierta.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE_AWAKE:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Aterrorizado */
        if (suffix(msg, " huye aterrorizado!")) {
            tmp = strlen(" huye aterrorizado!");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE__FEAR:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Sin miedo */
        if (suffix(msg, " recobra su valentía.")) {
            tmp = strlen(" recobra su valentía.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE__BOLD:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Sin miedo */
        if (suffix(msg, " recobra su valentía.")) {
            tmp = strlen(" recobra su valentía.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE__BOLD:%s", who);
            borg_react(msg, buf);
            return;
        }

        /* Estado -- Sin miedo */
        if (suffix(msg, " recobra su valentía.")) {
            tmp = strlen(" recobra su valentía.");
            strnfmt(who, 1 + len - tmp, "%s", msg);
            strnfmt(buf, 256, "STATE__BOLD:%s", who);
            borg_react(msg, buf);
            return;
        }
    }

    /* Feature XXX XXX XXX */
    if (streq(msg, "La puerta parece estar rota.")) {
        /* Only process open doors */
        if (ag->feat == FEAT_OPEN) {
            /* Mark as broken */
            ag->feat = FEAT_BROKEN;

            /* Clear goals */
            borg.goal.type = 0;
        }
        return;
    }

    /* Feature XXX XXX XXX */
    if (streq(msg, "Esto parece ser roca permanente.")) {
        /* Only process walls */
        if ((ag->feat >= FEAT_GRANITE) && (ag->feat <= FEAT_PERM)) {
            /* Mark the wall as permanent */
            ag->feat = FEAT_PERM;

            /* Clear goals */
            borg.goal.type = 0;
        }

        return;
    }

    /* Feature XXX XXX XXX */
    if (streq(msg, "Excavas en la pared de granito.")) {
        /* reseting my panel clock */
        borg.time_this_panel = 1;

        /* Only process walls */
        if ((ag->feat >= FEAT_GRANITE) && (ag->feat <= FEAT_PERM)) {
            /* Mark the wall as granite */
            ag->feat = FEAT_GRANITE;

            /* Clear goals */
            borg.goal.type = 0;
        }

        return;
    }

    /* Feature XXX XXX XXX */
    if (streq(msg, "Excavas en la veta de cuarzo.")) {
        /* Process magma veins with treasure */
        if (ag->feat == FEAT_MAGMA_K) {
            /* Mark the vein */
            ag->feat = FEAT_QUARTZ_K;

            /* Clear goals */
            borg.goal.type = 0;
        }

        /* Process magma veins */
        else if (ag->feat == FEAT_MAGMA) {
            /* Mark the vein */
            ag->feat = FEAT_QUARTZ;

            /* Clear goals */
            borg.goal.type = 0;
        }

        return;
    }

    /* Feature XXX XXX XXX */
    if (streq(msg, "Excavas en la veta de magma.")) {
        /* Process quartz veins with treasure */
        if (ag->feat == FEAT_QUARTZ_K) {
            /* Mark the vein */
            ag->feat = FEAT_MAGMA_K;

            /* Clear goals */
            borg.goal.type = 0;
        }

        /* Process quartz veins */
        else if (ag->feat == FEAT_QUARTZ) {
            /* Mark the vein */
            ag->feat = FEAT_MAGMA;

            /* Clear goals */
            borg.goal.type = 0;
        }

        return;
    }

    /* check for trying to dig when you can't */
    if (prefix(msg, "Picas inútilmente ")) {
        /* get rid of the goal monster we were chasing */
        if (borg.goal.type == GOAL_KILL && ag->kill)
            borg_delete_kill(ag->kill);
        return;
    }


    /* Palabra de Recuerdo -- Ignición */
    if (prefix(msg, "El aire a tu alrededor se vuelve ")) {
        /* Initiate recall */
        /* Guess how long it will take to lift off */
        /* Guess. game turns x 1000 ( 15+rand(20))*/
        borg.goal.recalling = 15000 + 5000;
        return;
    }

    /* Descenso Profundo -- Ignición */
    if (prefix(msg, "El aire a tu alrededor comienza ")) {
        /* Initiate descent */
        /* Guess how long it will take to lift off */
        /* Guess. game turns x 1000 ( 3+rand(4))*/
        borg.goal.descending = 3000 + 2000;
        return;
    }

    /* Palabra de Recuerdo -- Despegue */
    if (prefix(msg, "Sientes que eres jalado ")) {
        /* Flush our key-buffer */
        /* this is done in case the borg had been aiming a */
        /* shot before recall hit */
        borg_flush();

        /* Recall complete */
        borg.goal.recalling = 0;
        return;
    }

    /* Descenso Profundo -- Despegue */
    if (prefix(msg, "¡El suelo se abre bajo tus pies!")) {
        /* Flush our key-buffer */
        /* this is done in case the borg had been aiming a */
        /* shot before descent hit */
        borg_flush();

        /* Recall complete */
        borg.goal.descending = 0;
        return;
    }

    /* Palabra de Recuerdo -- Cancelada */
    if (prefix(msg, "Una tensión abandona ")) {
        /* Oops */
        borg.goal.recalling = 0;
        return;
    }

    /* Descenso Profundo -- Cancelado (solo ocurre al morir) */
    if (prefix(msg, "El aire a tu alrededor se detiene ")) {
        /* Oops */
        borg.goal.descending = 0;
        return;
    }

    /* Llevando objeto maldito */
    if (prefix(msg, "¡Vaya! ¡Se siente mortalmente frío!")) {
        /* this should only happen with STICKY items, The Crown of Morgoth or
         * The One Ring */
        /* !FIX !TODO handle crown eventually */
        return;
    }

    /* protección del mal */
    if (prefix(msg, "¡Te sientes a salvo del mal!")) {
        borg.temp.prot_from_evil = true;
        return;
    }
    if (prefix(msg, "Ya no te sientes a salvo del mal.")) {
        borg.temp.prot_from_evil = false;
        return;
    }
    /* acelerarse */
    if (prefix(msg, "¡Sientes que te mueves más rápido!")) {
        borg.temp.fast = true;
        return;
    }
    if (prefix(msg, "Sientes que te vuelves más lento.")) {
        borg.temp.fast = false;
        return;
    }
    /* Bendición */
    if (prefix(msg, "Te sientes virtuoso")) {
        borg.temp.bless = true;
        return;
    }
    if (prefix(msg, "La oración ha expirado.")) {
        borg.temp.bless = false;
        return;
    }

    /* lanzamiento rápido */
    if (prefix(msg, "Sientes que tu mente se acelera.")) {
        borg.temp.fastcast = true;
        return;
    }
    if (prefix(msg, "Sientes que tu mente se ralentiza de nuevo.")) {
        borg.temp.fastcast = false;
        return;
    }

    /* heroísmo */
    if (prefix(msg, "¡Te sientes como un héroe!")) {
        borg.temp.hero = true;
        return;
    }
    if (prefix(msg, "Ya no te sientes heroico.")) {
        borg.temp.hero = false;
        return;
    }

    /* berserker */
    if (prefix(msg, "¡Te sientes como una máquina de matar!")) {
        borg.temp.berserk = true;
        return;
    }
    if (prefix(msg, "Ya no te sientes en berserker.")) {
        borg.temp.berserk = false;
        return;
    }

    /* Ver Invisible */
    if (prefix(msg, "¡Tus ojos se sienten muy sensibles!")) {
        borg.see_inv = 30000;
        return;
    }
    if (prefix(msg, "Tus ojos ya no se sienten tan sensibles.")) {
        borg.see_inv = 0;
        return;
    }

    /* verificar si hay una pared bloqueando pero no cuando está confundido */
    if ((prefix(msg, "Hay una pared ") && (!borg.trait[BI_ISCONFUSED]))) {
        my_need_redraw = true;
        my_need_alter  = true;
        borg.goal.type = 0;
        return;
    }

    /* verificar si hay puerta cerrada pero no cuando está confundido */
    if ((prefix(msg, "Hay una puerta cerrada bloqueando tu camino.")
            && (!borg.trait[BI_ISCONFUSED] && !borg.trait[BI_ISIMAGE]))) {
        my_need_redraw = true;
        my_need_alter  = true;
        borg.goal.type = 0;
        return;
    }

    /* check for mis-alter command.  Sometime induced by never_move guys*/
    if (prefix(msg, "Giras sobre ti mismo.") && !borg.trait[BI_ISCONFUSED]) {
        /* Examine all the monsters */
        for (i = 1; i < borg_kills_nxt; i++) {

            borg_kill *kill = &borg_kills[i];

            /* Skip dead monsters */
            if (!kill->r_idx)
                continue;

            /* Now do distance considerations */
            x9 = kill->pos.x;
            y9 = kill->pos.y;

            /* Distance components */
            ax = (x9 > borg.c.x) ? (x9 - borg.c.x) : (borg.c.x - x9);
            ay = (y9 > borg.c.y) ? (y9 - borg.c.y) : (borg.c.y - y9);

            /* Distance */
            d = MAX(ax, ay);

            /* if the guy is too close then delete him. */
            if (d < 4) {
                /* Kill em */
                borg_delete_kill(i);
            }
        }

        my_no_alter    = true;
        borg.goal.type = 0;
        return;
    }

    /* Check for the missing staircase */
    if (prefix(msg, "No hay camino conocido hacia ") || 
        prefix(msg, "Hay algo aquí.")) {
        /* make sure the aligned dungeon is on */

        /* make sure the borg does not think he's on one */
        /* Remove all stairs from the array. */
        track_less.num                      = 0;
        track_more.num                      = 0;
        borg_grids[borg.c.y][borg.c.x].feat = FEAT_BROKEN;

        return;
    }

    /* Feature XXX XXX XXX */
    if (prefix(msg, "No ves nada allí ")) {
        ag->feat    = FEAT_BROKEN;

        my_no_alter = true;
        /* Clear goals */
        borg.goal.type = 0;
        return;
    }

    /* Hack to protect against clock overflows and errors */
    if (prefix(msg, "Ilegal ")) {
        /* Oops */
        borg_respawning = 7;
        borg_keypress(ESCAPE);
        borg_keypress(ESCAPE);
        borg.time_this_panel += 100;
        return;
    }

    /* Hack to protect against clock overflows and errors */
    if (prefix(msg, "No tienes nada que identificar")) {
        /* Oops */
        borg_keypress(ESCAPE);
        borg_keypress(ESCAPE);
        borg.time_this_panel += 100;

        /* ID all items (equipment) */
        for (i = INVEN_WIELD; i <= INVEN_FEET; i++) {
            borg_item *item = &borg_items[i];

            /* Skip empty items */
            if (!item->iqty)
                continue;

            item->ident = true;
        }

        /* ID all items  (inventory) */
        for (i = 0; i <= z_info->pack_size; i++) {
            borg_item *item = &borg_items[i];

            /* Skip empty items */
            if (!item->iqty)
                continue;

            item->ident = true;
        }
        return;
    }

    /* Hack to protect against clock overflows and errors */
    if (prefix(msg, "Identificando El Fial")) {

        /* ID item (equipment) */
        borg_item *item = &borg_items[INVEN_LIGHT];
        item->ident     = true;

        /* Oops */
        borg_keypress(ESCAPE);
        borg_keypress(ESCAPE);
        borg.time_this_panel += 100;
    }

    /* resistencia al ácido */
    if (prefix(msg, "¡Te sientes resistente al ácido!")) {
        borg.temp.res_acid = true;
        return;
    }
    if (prefix(msg, "Ya no eres resistente al ácido.")) {
        borg.temp.res_acid = false;
        return;
    }
    /* resistencia a la electricidad */
    if (prefix(msg, "¡Te sientes resistente a la electricidad!")) {
        borg.temp.res_elec = true;
        return;
    }
    if (prefix(msg, "Ya no eres resistente a la electricidad.")) {
        borg.temp.res_elec = false;
        return;
    }
    /* resistencia al fuego */
    if (prefix(msg, "¡Te sientes resistente al fuego!")) {
        borg.temp.res_fire = true;
        return;
    }
    if (prefix(msg, "Ya no eres resistente al fuego.")) {
        borg.temp.res_fire = false;
        return;
    }
    /* resistencia al frío */
    if (prefix(msg, "¡Te sientes resistente al frío!")) {
        borg.temp.res_cold = true;
        return;
    }
    if (prefix(msg, "Ya no eres resistente al frío.")) {
        borg.temp.res_cold = false;
        return;
    }
    /* resistencia al veneno */
    if (prefix(msg, "¡Te sientes resistente al veneno!")) {
        borg.temp.res_pois = true;
        return;
    }
    if (prefix(msg, "Ya no eres resistente al veneno.")) {
        borg.temp.res_pois = false;
        return;
    }

    /* Escudo */
    if (prefix(msg, "¡Un escudo místico se forma alrededor de tu cuerpo!")
        || prefix(msg, "Tu piel se convierte en piedra.")) {
        borg.temp.shield = true;
        return;
    }
    if (prefix(msg, "Tu escudo místico se desmorona.")
        || prefix(msg, "Un tono carnoso vuelve a tu piel.")) {
        borg.temp.shield = false;
        return;
    }

    /* Glifo de Protección (el hechizo ya no da aviso)*/
    /* Lamentablemente, la Runa de Protección no tiene mensaje */
    if (prefix(msg, "¡Inscribes un símbolo místico en el suelo!")) {
        /* Check for an existing glyph */
        for (i = 0; i < track_glyph.num; i++) {
            /* Stop if we already new about this glyph */
            if ((track_glyph.x[i] == borg.c.x)
                && (track_glyph.y[i] == borg.c.y))
                break;
        }

        /* Track the newly discovered glyph */
        if ((i == track_glyph.num) && (i < track_glyph.size)) {
            borg_note("# Registrando la creación de un glifo.");
            track_glyph.x[i] = borg.c.x;
            track_glyph.y[i] = borg.c.y;
            track_glyph.num++;
        }

        return;
    }
    if (prefix(msg, "¡La runa de protección está rota!")) {
        /* we won't know which is broken so erase them all and
         * allow the borg to scan the screen and rebuild the array.
         * He won't see the one under him though.  So a special check
         * must be made.
         */

        /* Remove the entire array */
        for (i = 0; i < track_glyph.num; i++) {
            /* Stop if we already new about this glyph */
            track_glyph.x[i] = 0;
            track_glyph.y[i] = 0;
        }
        /* no known glyphs */
        track_glyph.num = 0;

        /* Check for glyphs under player -- Cheat*/
        if (square_iswarded(cave, borg.c)) {
            track_glyph.x[track_glyph.num] = borg.c.x;
            track_glyph.y[track_glyph.num] = borg.c.y;
            track_glyph.num++;
        }
        return;
    }
    /* failed glyph spell message */
    if (prefix(msg, "El objeto resiste el hechizo")
        || prefix(msg, "No hay suelo despejado")) {

        /* Forget the newly created-though-failed  glyph */
        track_glyph.x[track_glyph.num] = 0;
        track_glyph.y[track_glyph.num] = 0;
        track_glyph.num--;

        /* note it */
        borg_note("# Eliminando el Glifo bajo mí, reemplazando con puerta rota.");

        /* mark that we are not on a clear spot.  The borg ignores
         * broken doors and this will keep him from casting it again.
         */
        ag->feat = FEAT_BROKEN;
        return;
    }

    /* Escombros eliminados. Importante cuando no hay luz */
    if (prefix(msg, "Has eliminado los ")) {
        int x, y;
        /* remove rubbles from array */
        for (y = borg.c.y - 1; y < borg.c.y + 1; y++) {
            for (x = borg.c.x - 1; x < borg.c.x + 1; x++) {
                /* replace all rubble with broken doors, the borg ignores
                 * broken doors.  This routine is only needed if the borg
                 * is out of lite and searching in the dark.
                 */
                if (borg.trait[BI_LIGHT])
                    continue;

                if (ag->feat == FEAT_RUBBLE)
                    ag->feat = FEAT_BROKEN;
            }
        }
        return;
    }

    if (prefix(msg, "El encantamiento falló")) {
        /* reset our panel clock for this */
        borg.time_this_panel = 1;
        return;
    }

    /* need to kill monsters when WoD is used */
    if (prefix(msg, "¡Hay un cegador destello de luz!")) {
        /* Examine all the monsters */
        for (i = 1; i < borg_kills_nxt; i++) {
            borg_kill *kill = &borg_kills[i];

            x9              = kill->pos.x;
            y9              = kill->pos.y;

            /* Skip dead monsters */
            if (!kill->r_idx)
                continue;

            /* Distance components */
            ax = (x9 > borg.c.x) ? (x9 - borg.c.x) : (borg.c.x - x9);
            ay = (y9 > borg.c.y) ? (y9 - borg.c.y) : (borg.c.y - y9);

            /* Distance */
            d = MAX(ax, ay);

            /* Minimal distance */
            if (d > 12)
                continue;

            /* Kill em */
            borg_delete_kill(i);
        }

        /* Remove the region fear as well */
        borg_fear_region[borg.c.y / 11][borg.c.x / 11] = 0;

        return;
    }

    /* Be aware and concerned of busted doors */
    if (prefix(msg, "¡Escuchas una puerta abrirse de golpe!")) {
        /* on level 1 and 2 be concerned.  Could be Grip or Fang */
        if (borg.trait[BI_CDEPTH] <= 3 && borg.trait[BI_CLEVEL] <= 5)
            scaryguy_on_level = true;
    }

    /* Some spells move the borg from his grid */
    if (prefix(msg, "te ordena regresar.")
        || prefix(msg, "te teletransporta.")
        || prefix(msg, "gesticula a tus pies.")) {
        /* Si está en modo Lunal mejor desactivarlo, ya no está en las escaleras */
        borg.lunal_mode = false;
        borg_note("# Desconectando el modo Lunal debido a hechizo de monstruo.");
    }

    /* Feelings about the level */
    for (i = 0; prefix_feeling_danger[i]; i++) {
        /* "You feel..." (etc) */
        if (prefix(msg, prefix_feeling_danger[i])) {
            strnfmt(buf, 256, "FEELING_DANGER:%d", i);
            borg_react(msg, buf);
            return;
        }
    }

    for (i = 0; suffix_feeling_stuff[i]; i++) {
        /* "You feel..." (etc) */
        if (suffix(msg, suffix_feeling_stuff[i])) {
            strnfmt(buf, 256, "FEELING_STUFF:%d", i);
            borg_react(msg, buf);
            return;
        }
    }
}

/*
 * Parse a message, piece of a message, or set of messages.
 *
 * We must handle long messages which are "split" into multiple
 * pieces, and also multiple messages which may be "combined"
 * into a single set of messages.
 */
void borg_parse(char *msg)
{
    static int  len = 0;
    static char buf[1024];

    /* Note the long message */
    if (borg_cfg[BORG_VERBOSE] && msg)
        borg_note(format("# Analizando msg <%s>", msg));

    /* Flush messages */
    if (len && (!msg || (msg[0] != ' '))) {
        int i, j;

        /* Split out punctuation */
        for (j = i = 0; i < len - 1; i++) {
            /* Check for punctuation */
            if ((buf[i] == '.') || (buf[i] == '!') || (buf[i] == '?')
                || (buf[i] == '"')) {
                /* Require space */
                if (buf[i + 1] == ' ') {
                    /* Terminate */
                    buf[i + 1] = '\0';

                    /* Parse fragment */
                    borg_parse_aux(buf + j, (i + 1) - j);

                    /* Restore */
                    buf[i + 1] = ' ';

                    /* Advance past spaces */
                    for (j = i + 2; buf[j] == ' '; j++) /* loop */
                        ;
                }
            }
        }

        /* Parse tail */
        borg_parse_aux(buf + j, len - j);

        /* Forget */
        len = 0;
    }

    /* No message */
    if (!msg) {
        /* Start over */
        len = 0;
    }

    /* Continued message */
    else if (msg[0] == ' ') {
        /* Collect, verify, and grow */
        len += strnfmt(buf + len, 1024 - len, "%s", msg);
    }

    /* New message */
    else {
        /* Collect, verify, and grow */
        len = strnfmt(buf, 1024, "%s", msg);
    }
}

/* all parts equal or same nullness */
static bool borg_read_message_equal(
    struct borg_read_message *msg1, struct borg_read_message *msg2)
{
    if (((msg1->message_p1 && msg2->message_p1
             && streq(msg1->message_p1, msg2->message_p1))
            || (!msg1->message_p1 && !msg2->message_p1))
        && ((msg1->message_p2 && msg2->message_p2
                && streq(msg1->message_p2, msg2->message_p2))
            || (!msg1->message_p2 && !msg2->message_p2))
        && ((msg1->message_p3 && msg2->message_p3
                && streq(msg1->message_p3, msg2->message_p3))
            || (!msg1->message_p3 && !msg2->message_p3)))
        return true;
    return false;
}

static void insert_msg(struct borg_read_messages *msgs,
    struct borg_read_message *msg, int spell_number)
{
    int  i;
    bool found_dup = false;

    /* this way we don't have to pre-create the array */
    if (msgs->messages == NULL) {
        msgs->allocated = 10;
        msgs->messages
            = mem_alloc(sizeof(struct borg_read_message) * msgs->allocated);
        msgs->index = mem_alloc(sizeof(int) * msgs->allocated);
    }

    if (msg == NULL) {
        msgs->messages[msgs->count].message_p1 = NULL;
        msgs->messages[msgs->count].message_p2 = NULL;
        msgs->messages[msgs->count].message_p3 = NULL;
        msgs->count++;
        /* shrink array down, we are done*/
        msgs->messages = mem_realloc(
            msgs->messages, sizeof(struct borg_read_message) * msgs->count);
        msgs->index     = mem_realloc(msgs->index, sizeof(int) * msgs->count);
        msgs->allocated = msgs->count;
        return;
    }

    for (i = 0; i < msgs->count; i++) {
        if (borg_read_message_equal(&msgs->messages[i], msg)) {
            found_dup = true;
            break;
        }
    }
    if (!found_dup) {
        memcpy(&(msgs->messages[msgs->count]), msg,
            sizeof(struct borg_read_message));
        msgs->index[msgs->count] = spell_number;
        msgs->count++;
        if (msgs->count == msgs->allocated) {
            msgs->allocated += 10;
            msgs->messages = mem_realloc(msgs->messages,
                sizeof(struct borg_read_message) * msgs->allocated);
            msgs->index
                = mem_realloc(msgs->index, sizeof(int) * msgs->allocated);
        }
    } else {
        string_free(msg->message_p1);
        string_free(msg->message_p2);
        string_free(msg->message_p3);
    }
}

static void clean_msgs(struct borg_read_messages *msgs)
{
    int i;

    for (i = 0; i < msgs->count; ++i) {
        string_free(msgs->messages[i].message_p1);
        string_free(msgs->messages[i].message_p2);
        string_free(msgs->messages[i].message_p3);
    }
    mem_free(msgs->messages);
    msgs->messages = NULL;
    mem_free(msgs->index);
    msgs->index     = NULL;
    msgs->count     = 0;
    msgs->allocated = 0;
}

/* get rid of leading spaces */
static char *borg_trim_lead_space(char *orig)
{
    if (!orig || orig[0] != ' ')
        return orig;

    return &orig[1];
}

/*
 * break a string into a borg_read_message
 *
 * a message can have up to three parts broken up by variables
 * ex: "{name} hits {pronoun} followers with {type} ax."
 * " hits ", " followers with ", " ax."
 * if the message has more parts than that, they are ignored so
 * ex: "{name} hits {pronoun} followers with {type} ax and {type} breath."
 * would end up as
 * " hits ", " followers with ", " ax and "
 * hopefully this is enough to keep the messages as unique as possible
 */
static void borg_load_read_message(
    char *message, struct borg_read_message *read_message)
{
    read_message->message_p1 = NULL;
    read_message->message_p2 = NULL;
    read_message->message_p3 = NULL;

    char *suffix             = strchr(message, '}');
    if (!suffix) {
        /* no variables, use message as is */
        read_message->message_p1 = string_make(borg_trim_lead_space(message));
        return;
    }
    /* skip leading variable, if there is one */
    if (message[0] == '{')
        suffix++;
    else
        suffix = message;
    char *var = strchr(suffix, '{');
    if (!var) {
        /* one variable, use message post variable */
        read_message->message_p1 = string_make(borg_trim_lead_space(suffix));
        return;
    }
    while (suffix[0] == ' ')
        suffix++;
    int part_len             = strlen(suffix) - strlen(var);
    read_message->message_p1 = string_make(format("%.*s", part_len, suffix));
    suffix += part_len;
    suffix = strchr(var, '}');
    if (!suffix)
        return; /* this should never happen but ... if a string is { with no }*/
    suffix++;
    var = strchr(suffix, '{');
    if (!var) {
        while (suffix[0] == ' ')
            suffix++;

        /* two variables, ignore if last part is just . */
        if (strlen(suffix) && !streq(suffix, "."))
            read_message->message_p2 = string_make(suffix);
        return;
    }
    while (suffix[0] == ' ')
        suffix++;
    part_len = strlen(suffix) - strlen(var);
    if (part_len) {
        read_message->message_p2 = string_make(
            borg_trim_lead_space(format("%.*s", part_len, suffix)));
    }
    suffix += part_len;
    suffix = strchr(var, '}');
    if (!suffix)
        return; /* this should never happen but ... if a string is { with no }*/
    suffix++;
    var = strchr(suffix, '{');
    if (!var) {
        while (suffix[0] == ' ')
            suffix++;
        /* three variables, ignore if last part is just . */
        if (strlen(suffix) && !streq(suffix, ".")) {
            if (read_message->message_p2) {
                read_message->message_p3
                    = string_make(borg_trim_lead_space(suffix));
            } else {
                read_message->message_p2
                    = string_make(borg_trim_lead_space(suffix));
            }
        }
        return;
    }
    while (suffix[0] == ' ')
        suffix++;
    part_len = strlen(suffix) - strlen(var);
    if (read_message->message_p2)
        read_message->message_p3 = string_make(
            borg_trim_lead_space(format("%.*s", part_len, suffix)));
    else
        read_message->message_p2 = string_make(
            borg_trim_lead_space(format("%.*s", part_len, suffix)));

    return;
}

/* load monster spell messages */
static void borg_init_spell_messages(void)
{
    const struct monster_spell       *spell = monster_spells;
    const struct monster_spell_level *spell_level;
    struct borg_read_message          read_message;

    while (spell) {
        spell_level = spell->level;
        while (spell_level) {
            if (spell_level->blind_message) {
                borg_load_read_message(
                    spell_level->blind_message, &read_message);
                insert_msg(&spell_invis_msgs, &read_message, spell->index);
            }
            if (spell_level->message) {
                borg_load_read_message(spell_level->message, &read_message);
                insert_msg(&spell_msgs, &read_message, spell->index);
            }
            if (spell_level->miss_message) {
                borg_load_read_message(
                    spell_level->miss_message, &read_message);
                insert_msg(&spell_msgs, &read_message, spell->index);
            }
            spell_level = spell_level->next;
        }
        spell = spell->next;
    }
    /* null terminate */
    insert_msg(&spell_invis_msgs, NULL, 0);
    insert_msg(&spell_msgs, NULL, 0);
}

/* HACK pluralize ([|] parsing) code stolen from mon-msg.c */
/* State machine constants for get_message_text() */
#define MSG_PARSE_NORMAL 0
#define MSG_PARSE_SINGLE 1
#define MSG_PARSE_PLURAL 2

/* load monster in pain messages */
static char *borg_get_parsed_pain(const char *pain, bool do_plural)
{
    size_t buflen = strlen(pain) + 1;
    char  *buf    = mem_zalloc(buflen);

    int    state  = MSG_PARSE_NORMAL;
    size_t maxlen = strlen(pain);
    size_t pos    = 1;

    /* for the borg, always start with a space */
    buf[0] = ' ';

    /* Put the message characters in the buffer */
    /* XXX This logic should be used everywhere for pluralising strings */
    for (size_t i = 0; i < maxlen && pos < buflen - 1; i++) {
        char cur = pain[i];

        /*
         * The characters '[|]' switch parsing mode and are never output.
         * The syntax is [singular|plural]
         */
        if (state == MSG_PARSE_NORMAL && cur == '[') {
            state = MSG_PARSE_SINGLE;
        } else if (state == MSG_PARSE_SINGLE && cur == '|') {
            state = MSG_PARSE_PLURAL;
        } else if (state != MSG_PARSE_NORMAL && cur == ']') {
            state = MSG_PARSE_NORMAL;
        } else if (state == MSG_PARSE_NORMAL
                   || (state == MSG_PARSE_SINGLE && do_plural == false)
                   || (state == MSG_PARSE_PLURAL && do_plural == true)) {
            /* Copy the characters according to the mode */
            buf[pos++] = cur;
        }
    }
    return buf;
}

static void borg_insert_pain(const char *pain, int *capacity, int *count)
{
    char *new_message;
    if (*capacity <= (*count) + 2) {
        *capacity += 14;
        suffix_pain = mem_realloc(suffix_pain, sizeof(char *) * (*capacity));
    }

    new_message             = borg_get_parsed_pain(pain, false);
    suffix_pain[(*count)++] = new_message;
    new_message             = borg_get_parsed_pain(pain, true);
    suffix_pain[(*count)++] = new_message;
}

/* !FIX see mon-msg.c */
static const struct
{
    const char *msg;
    bool omit_subject;
    int type;
} borg_msg_repository[] = {
    #define MON_MSG(x, t, o, s) { s, o, t },
    #include "list-mon-message.h"
    #undef MON_MSG
};

static void borg_init_pain_messages(void)
{
    int                  capacity = 1;
    int                  count    = 0;
    int                  idx, i;
    struct monster_pain *pain;

    suffix_pain = mem_alloc(sizeof(char *) * capacity);

    for (idx = 0; idx < z_info->mp_max; idx++) {
        pain = &pain_messages[idx];
        for (i = 0; i < 7; i++) {
            if (pain == NULL || pain->messages[i] == NULL)
                break;
            borg_insert_pain(pain->messages[i], &capacity, &count);
        }
    }

    /* some more standard messages */
    for (idx = 0; idx < MON_MSG_MAX; idx++) {
        if (borg_msg_repository[idx].type == MSG_KILL)
            continue;

        const char *std_pain = borg_msg_repository[idx].msg;

        switch (idx) {
        case MON_MSG_DISAPPEAR:
        case MON_MSG_95:
        case MON_MSG_75: 
        case MON_MSG_50: 
        case MON_MSG_35: 
        case MON_MSG_20: 
        case MON_MSG_10: 
        case MON_MSG_0:  continue;
        }
        if (std_pain != NULL)
            borg_insert_pain(std_pain, &capacity, &count);
    }

    if ((count + 1) != capacity)
        suffix_pain = mem_realloc(suffix_pain, sizeof(char *) * (count + 1));
    suffix_pain[count] = NULL;
}

/* load player hit by messages */
static void borg_init_hit_by_messages(void)
{
    struct borg_read_message read_message;

    for (int i = 0; i < z_info->blow_methods_max; i++) {
        struct blow_message *messages = blow_methods[i].messages;

        while (messages) {
            if (messages->act_msg) {
                borg_load_read_message(messages->act_msg, &read_message);
                insert_msg(&suffix_hit_by, &read_message, blow_methods[i].msgt);
            }
            messages = messages->next;
        }
    }
    /* null terminate */
    insert_msg(&suffix_hit_by, NULL, 0);
}

/* init all messages used by the borg */
void borg_init_messages(void)
{
    borg_init_spell_messages();
    borg_init_pain_messages();
    borg_init_hit_by_messages();

    /*** Message tracking ***/

    /* No chars saved yet */
    borg_msg_len = 0;

    /* Maximum buffer size */
    borg_msg_siz = 4096;

    /* Allocate a buffer */
    borg_msg_buf = mem_zalloc(borg_msg_siz * sizeof(char));

    /* No msg's saved yet */
    borg_msg_num = 0;

    /* Maximum number of messages */
    borg_msg_max = 256;

    /* Allocate array of positions */
    borg_msg_pos = mem_zalloc(borg_msg_max * sizeof(int16_t));

    /* Allocate array of use-types */
    borg_msg_use = mem_zalloc(borg_msg_max * sizeof(int16_t));
}

/* free all messages used by the borg */
void borg_free_messages(void)
{
    int i;

    mem_free(borg_msg_use);
    borg_msg_use = NULL;
    mem_free(borg_msg_pos);
    borg_msg_pos = NULL;
    borg_msg_num = 0;
    mem_free(borg_msg_buf);
    borg_msg_buf = NULL;
    borg_msg_siz = 0;

    if (suffix_pain) {
        for (i = 0; suffix_pain[i]; ++i) {
            mem_free(suffix_pain[i]);
            suffix_pain[i] = NULL;
        }
        mem_free(suffix_pain);
        suffix_pain = NULL;
    }
    clean_msgs(&suffix_hit_by);
    clean_msgs(&spell_invis_msgs);
    clean_msgs(&spell_msgs);
}

#endif
