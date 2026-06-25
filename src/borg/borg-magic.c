/**
 * \file  borg-magic.c
 * \brief The basic magic definitions and routines to cast spells
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

#include "borg-magic.h"

#ifdef ALLOW_BORG

#include "../effects.h"
#include "../player-spell.h"
#include "../ui-menu.h"

#include "borg-cave.h"
#include "borg-cave-view.h"
#include "borg-init.h"
#include "borg-io.h"
#include "borg-trait.h"

/*
 * Spell info - individualized for class by spell number 
*/

borg_magic *borg_magics = NULL; 


static borg_spell_rating *borg_spell_ratings;
// !FIX !TODO for now put this in the code.  It should probably end up in borg.txt or a new borg.cfg
// I also gave low ratings to spells that are new since the borg doesn't know when to use them yet.
static borg_spell_rating borg_spell_ratings_MAGE[] =
{
    { "Proyectil Mágico", 95, MAGIC_MISSILE },
    { "Iluminar Habitación", 65, LIGHT_ROOM },
    { "Detec Trampas Puertas Escale", 85, FIND_TRAPS_DOORS_STAIRS },
    { "Puerta de Fase", 95, PHASE_DOOR },
    { "Arco eléctrico", 85, ELECTRIC_ARC },
    { "Detectar Monstruos", 85, DETECT_MONSTERS },
    { "Bola de Fuego", 75, FIRE_BALL },
    { "Recarga", 65, RECHARGING },
    { "Identificar Runa", 95, IDENTIFY_RUNE },
    { "Detección de Tesoros", 5, TREASURE_DETECTION }, /* borg never uses this */
    { "Rayo de Escarcha", 75, FROST_BOLT },
    { "Revelar Monstruos", 85, REVEAL_MONSTERS },
    { "Rocío de Ácido", 75, ACID_SPRAY },
    { "Desactivar Trampas y Destruir Puertas", 95, DISABLE_TRAPS_DESTROY_DOORS },
    { "Teletransportarse", 95, TELEPORT_SELF },
    { "Teletransportar a Otro", 75, TELEPORT_OTHER },
    { "Resistencia", 90, RESISTANCE },
    { "Extraer Energía Mágica", 5, TAP_MAGICAL_ENERGY }, /* need to figure out when to cast this one */
    { "Canalizar Maná", 95, MANA_CHANNEL },
    { "Creación de Puertas", 65, DOOR_CREATION },
    { "Rayo de Maná", 95, MANA_BOLT },
    { "Teletransporte de Nivel", 65, TELEPORT_LEVEL },
    { "Detección", 95, DETECTION },
    { "Portal Dimensional", 95, DIMENSION_DOOR },
    { "Empujar Lejos", 55, THRUST_AWAY },
    { "Onda de Choque", 85, SHOCK_WAVE },
    { "Explosión", 85, EXPLOSION },
    { "Destierro", 75, BANISHMENT },
    { "Destierro Masivo", 65, MASS_BANISHMENT },
    { "Tormenta de Maná", 75, MANA_STORM }
};
static borg_spell_rating borg_spell_ratings_DRUID[] =
{
    { "Detectar Vida", 95,  DETECT_LIFE },
    { "Forma de Zorro", 5, FOX_FORM }, // !FIX !TODO need to know when to cast any of the shapechanges
    { "Saciar Hambre", 85, REMOVE_HUNGER },
    { "Nube Pestilente", 95, STINKING_CLOUD },
    { "Confundir Monstruo", 55, CONFUSE_MONSTER },
    { "Ralentizar Monstruo", 65, SLOW_MONSTER },
    { "Curar Veneno", 55, CURE_POISON },
    { "Resistir Veneno", 60, RESIST_POISON },
    { "Piedra a Lodo", 80, TURN_STONE_TO_MUD },
    { "Percibir Entorno", 80, SENSE_SURROUNDINGS },
    { "Golpe de Relámpago", 85, LIGHTNING_STRIKE },
    { "Levantamiento de Tierra", 70, EARTH_RISING },
    { "Trance", 55, TRANCE },
    { "Sueño en Masa", 80, MASS_SLEEP },
    { "Convertirse en Hombre-Pukel", 5, BECOME_PUKEL_MAN }, // !FIX !TODO shapechange
    { "Vuelo de Águila", 5, EAGLES_FLIGHT }, // !FIX !TODO shapechange
    { "Forma de Oso", 5, BEAR_FORM }, // !FIX !TODO shapechange
    { "Temblor", 80, TREMOR },
    { "Acelerarse", 90, HASTE_SELF },
    { "Revitalizar", 95, REVITALIZE },
    { "Regeneración Rápida", 55, RAPID_REGENERATION },
    { "Curación con Hierbas", 90, HERBAL_CURING },
    { "Lluvia de Meteoros", 90, METEOR_SWARM },
    { "Grieta", 90, RIFT },
    { "Tormenta de Hielo", 85, ICE_STORM },
    { "Erupción Volcánica", 60, VOLCANIC_ERUPTION },
    { "Río de Relámpagos", 90, RIVER_OF_LIGHTNING }
};

static borg_spell_rating borg_spell_ratings_PRIEST[] =
{
    { "Llamar a la Luz", 65, CALL_LIGHT },
    { "Detección del Mal", 85, DETECT_EVIL },
    { "Curación Menor", 65, MINOR_HEALING },
    { "Bendición", 85, BLESS },
    { "Sentir lo Invisible", 75, SENSE_INVISIBLE },
    { "Heroísmo", 75, HEROISM },
    { "Esfera de Drenaje", 95, ORB_OF_DRAINING },
    { "Lanza de Luz", 75, SPEAR_OF_LIGHT },
    { "Dispersar No Muerto", 65, DISPEL_UNDEAD },
    { "Disipar el Mal", 65, DISPEL_EVIL },
    { "Protección contra el Mal", 85, PROTECTION_FROM_EVIL },
    { "Eliminar Maldición", 85, REMOVE_CURSE },
    { "Portal", 85, PORTAL },
    { "Recuerdo", 75, REMEMBRANCE },
    { "Palabra de Retorno", 95, WORD_OF_RECALL },
    { "Curación", 95, HEALING },
    { "Restauración", 75, RESTORATION },
    { "Clarividencia", 85, CLAIRVOYANCE },
    { "Encantar Arma", 75, ENCHANT_WEAPON },
    { "Encantar Armadura", 75, ENCHANT_ARMOUR },
    { "Castigar al Mal", 75, SMITE_EVIL },
    { "Glifo de Protección", 95, GLYPH_OF_WARDING },
    { "Azote de Demonios", 85, DEMON_BANE },
    { "Expulsar al Mal", 85, BANISH_EVIL },
    { "Palabra de Destrucción", 75, WORD_OF_DESTRUCTION },
    { "Palabra Sagrada", 85, HOLY_WORD },
    { "Lanza de Orom\xC3\xab", 85, SPEAR_OF_OROME }, /* "Lanza de Orom(e + diaresis)" */
    { "Luz de Manw\xC3\xab", 85, LIGHT_OF_MANWE } /* "Luz de Manw(e + diaresis)"*/
};
static borg_spell_rating borg_spell_ratings_NECROMANCER[] =
{
    { "Rayo de Inframundo", 95, NETHER_BOLT },
    { "Sentir lo Invisible", 85, SENSE_INVISIBLE },
    { "Crear Oscuridad", 5, CREATE_DARKNESS }, 
    { "Forma de Murciélago", 5, BAT_FORM }, // !FIX !TODO shapechange
    { "Leer Mentes", 85, READ_MINDS },
    { "Drenar No-Vida", 85, TAP_UNLIFE },
    { "Aplastar", 95, CRUSH },
    { "Dormir a Malvados", 85, SLEEP_EVIL },
    { "Cambio de Sombra", 95, SHADOW_SHIFT },
    { "Desencantar", 25, DISENCHANT },
    { "Atemorizar", 85, FRIGHTEN },
    { "Golpe Vampírico", 75, VAMPIRE_STRIKE },
    { "Disipar la Vida", 65, DISPEL_LIFE },
    { "Lanza Oscura", 65, DARK_SPEAR },
    { "Forma de Huargo", 5, WARG_FORM }, // !FIX !TODO shapechange
    { "Desterrar Espíritus", 65, BANISH_SPIRITS },
    { "Aniquilar", 95, ANNIHILATE },
    { "Golpe de Grond", 85, GRONDS_BLOW },
    { "Liberar el Caos", 85, UNLEASH_CHAOS },
    { "Vapores de Mordor", 75, FUME_OF_MORDOR },
    { "Tormenta de Oscuridad", 65, STORM_OF_DARKNESS },
    { "Sacrificio de Poder", 5, POWER_SACRIFICE },  /* not sure if this is borg happy. */
    { "Zona Antimágica", 5, ZONE_OF_UNMAGIC },  // !FIX !TODO defense?  not sure how to code. 
    { "Forma de Vampiro", 5, VAMPIRE_FORM }, // !FIX !TODO shapechange
    { "Maldecir", 65, CURSE },
    { "Dominar", 5, COMMAND } // !FIX !TODO defense?  not sure how to code. 
};
static borg_spell_rating borg_spell_ratings_PALADIN[] =
{
    { "Bendición", 95, BLESS },
    { "Detección del Mal", 85, DETECT_EVIL },
    { "Llamar a la Luz", 85, CALL_LIGHT },
    { "Curación Menor", 95, MINOR_HEALING },
    { "Sentir lo Invisible", 65, SENSE_INVISIBLE },
    { "Heroísmo", 85, HEROISM },
    { "Protección contra el Mal", 85, PROTECTION_FROM_EVIL },
    { "Eliminar Maldición", 65, REMOVE_CURSE },
    { "Palabra de Retorno", 95, WORD_OF_RECALL },
    { "Curación", 95, HEALING },
    { "Clarividencia", 85, CLAIRVOYANCE },
    { "Castigar al Mal", 55, SMITE_EVIL },
    { "Azote de Demonios", 55, DEMON_BANE },
    { "Encantar Arma", 75, ENCHANT_WEAPON },
    { "Encantar Armadura", 85, ENCHANT_ARMOUR },
    { "Combate Singular", 95, SINGLE_COMBAT } // !FIX !TODO defense?  not sure how to code.
};
static borg_spell_rating borg_spell_ratings_ROGUE[] =
{
    { "Detectar Monstruos", 85, DETECT_MONSTERS },
    { "Puerta de Fase", 95, PHASE_DOOR },
    { "Detección de Objetos", 55, OBJECT_DETECTION },
    { "Detectar Escaleras", 55, DETECT_STAIRS },
    { "Recarga", 85, RECHARGING },
    { "Revelar Monstruos", 85, REVEAL_MONSTERS },
    { "Teletransportarse", 95, TELEPORT_SELF },
    { "Golpear y Huir", 15, HIT_AND_RUN }, // !FIX !TODO not sure how to code this
    { "Teletransportar a Otro", 85, TELEPORT_OTHER },
    { "Teletransporte de Nivel", 75, TELEPORT_LEVEL }
};
static borg_spell_rating borg_spell_ratings_RANGER[] =
{
    { "Saciar Hambre", 95, REMOVE_HUNGER },
    { "Detectar Vida", 85, DETECT_LIFE },
    { "Curación con Hierbas", 95, HERBAL_CURING },
    { "Resistir Veneno", 85, RESIST_POISON },
    { "Piedra a Lodo", 85, TURN_STONE_TO_MUD },
    { "Percibir Entorno", 75, SENSE_SURROUNDINGS },
    { "Cubrir Huellas", 25, COVER_TRACKS }, // !FIX !TODO prep?
    { "Crear Flechas", 85, CREATE_ARROWS }, // !FIX !TODO 
    { "Acelerarse", 95, HASTE_SELF },
    { "Señuelo", 5, DECOY }, // !FIX !TODO not sure what to do with this
    { "Encantar Munición", 95, BRAND_AMMUNITION }
};
static borg_spell_rating borg_spell_ratings_BLACKGUARD[] =
{
    { "Buscar Batalla", 55, SEEK_BATTLE },
    { "Fuerza Berserker", 95, BERSERK_STRENGTH },
    { "Ataque Torbellino", 85, WHIRLWIND_ATTACK },
    { "Destrozar Piedra", 95, SHATTER_STONE },
    { "Saltar a la Batalla", 65, LEAP_INTO_BATTLE },
    { "Propósito Sombrío", 65, GRIM_PURPOSE },
    { "Mutilar Enemigo", 75, MAIM_FOE },
    { "Aullido de los Condenados", 55, HOWL_OF_THE_DAMNED },
    { "Burla Implacable", 5, RELENTLESS_TAUNTING }, /* seems to dangerous for borg right now */
    { "Veneno", 55, VENOM },
    { "Forma de Hombre Lobo", 5, WEREWOLF_FORM }, // !FIX !TODO shapechange
    { "Sed de Sangre", 5, BLOODLUST }, /* seems to dangerous for borg right now */
    { "Tregua Profana", 95, UNHOLY_REPRIEVE },
    { "Golpe Contundente", 5, FORCEFUL_BLOW }, // !FIX !TODO need to code this 
    { "Terremoto", 95, QUAKE }
};

/*
 * get the stat used for casting spells
 *
 * Assumes the first spell determines the realm thus stat for all spells
 */
int borg_spell_stat(void)
{
    if (borg_can_cast()) {
        struct class_spell *spell = &(player->class->magic.books[0].spells[0]);
        if (spell != NULL) {
            return spell->realm->stat;
        }
    }

    return -1;
}

/*
 * Does this player cast spells
 */
bool borg_can_cast(void)
{
    return player->class->magic.total_spells != 0;
}

/*
 * Does this player mostly cast spells
 * HACK: Rather than hard code classes, assume any class with
 * more than three books is primarily casting
 * !FIX !TODO consider adding is_primary_caster to class struct
 */
bool borg_primarily_caster(void)
{
    return player->class->magic.num_books > 3;
}

/*
 * get the level at which Heroism (spell) grants Heroism (effect)
 */
int borg_heroism_level(void)
{
    if (borg.trait[BI_CLASS] == CLASS_PRIEST)
        return 20;
    if (borg.trait[BI_CLASS] == CLASS_PALADIN)
        return 15;
    return 99;
}

/*
 * find the index in the books array given the books sval
 */
int borg_get_book_num(int sval)
{
    if (!borg_can_cast())
        return -1;

    for (int book_num = 0; book_num < player->class->magic.num_books;
         book_num++) {
        if (player->class->magic.books[book_num].sval == sval)
            return book_num;
    }
    return -1;
}

/*
 * is this a dungeon book (not a basic book)
 */
bool borg_is_dungeon_book(int tval, int sval)
{
    switch (tval) {
    case TV_PRAYER_BOOK:
    case TV_MAGIC_BOOK:
    case TV_NATURE_BOOK:
    case TV_SHADOW_BOOK:
    case TV_OTHER_BOOK:
        break;
    default:
        return false;
    }

    /* keep track of if this is a book from the dungeon */
    for (int i = 0; i < player->class->magic.num_books; i++) {
        struct class_book book = player->class->magic.books[i];
        if (tval == book.tval && sval == book.sval && book.dungeon)
            return true;
    }

    return false;
}

/*
 * Find the magic structure given a book/entry
 */
borg_magic *borg_get_spell_entry(int book, int entry)
{
    int entry_in_book = 0;

    for (int spell_num = 0; spell_num < player->class->magic.total_spells;
         spell_num++) {
        if (borg_magics[spell_num].book == book) {
            if (entry_in_book == entry)
                return &borg_magics[spell_num];
            entry_in_book++;
        }
    }
    return NULL;
}

/*
 * Find the spell index for a given spell
 */
static int borg_get_spell_number(const enum borg_spells spell)
{
    /* The borg must be able to "cast" spells */
    if (borg_magics == NULL)
        return -1;

    int total_spells = player->class->magic.total_spells;
    for (int spell_num = 0; spell_num < total_spells; spell_num++) {
        if (borg_magics[spell_num].spell_enum == spell)
            return spell_num;
    }

    return -1;
}

/*
 * Find the power (cost in sp) value for a given spell
 */
int borg_get_spell_power(const enum borg_spells spell)
{
    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return -1;

    borg_magic *as = &borg_magics[spell_num];

    return as->power;
}

/*
 * Determine if borg can cast a given spell (when fully rested)
 */
bool borg_spell_legal(const enum borg_spells spell)
{
    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* The book must be possessed */
    if (borg.book_idx[as->book] < 0)
        return false;

    /* The spell must be "known" */
    if (borg_magics[spell_num].status < BORG_MAGIC_TEST)
        return false;

    /* The spell must be affordable (when rested) */
    if (borg_magics[spell_num].power > borg.trait[BI_MAXSP])
        return false;

    /* Success */
    return true;
}

/*
 * check a spell for a given effect
 */
static bool borg_spell_has_effect(int spell_num, uint16_t effect)
{
    const struct class_spell *cspell = spell_by_index(player, spell_num);
    struct effect            *eff    = cspell->effect;
    while (eff != NULL) {
        if (eff->index == effect)
            return true;
        eff = eff->next;
    }
    return false;
}

/*
 * Determine if borg can cast a given spell (right now)
 */
bool borg_spell_okay(const enum borg_spells spell)
{
    int reserve_mana = 0;

    int spell_num    = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* Dark */
    if (no_light(player))
        return false;

    /* Define reserve_mana for each class */
    switch (borg.trait[BI_CLASS]) {
    case CLASS_MAGE:
        reserve_mana = 6;
        break;
    case CLASS_RANGER:
        reserve_mana = 22;
        break;
    case CLASS_ROGUE:
        reserve_mana = 20;
        break;
    case CLASS_NECROMANCER:
        reserve_mana = 10;
        break;
    case CLASS_PRIEST:
        reserve_mana = 8;
        break;
    case CLASS_PALADIN:
        reserve_mana = 20;
        break;
    case CLASS_BLACKGUARD:
        reserve_mana = 0;
        break;
    }

    /* Low level spell casters should not worry about this */
    if (borg.trait[BI_CLEVEL] < 35)
        reserve_mana = 0;

    /* Require ability (when rested) */
    if (!borg_spell_legal(spell))
        return false;

    /* Blind/confused/amnesia */
    if (borg.trait[BI_ISBLIND] || borg.trait[BI_ISCONFUSED])
        return false;

    /* The spell must be affordable (now) */
    if (as->power > borg.trait[BI_CURSP])
        return false;

    /* Do not cut into reserve mana (for final teleport) */
    if (borg.trait[BI_CURSP] - as->power < reserve_mana) {
        /* nourishing spells okay */
        if (borg_spell_has_effect(spell_num, EF_NOURISH))
            return true;

        /* okay to run away */
        if (borg_spell_has_effect(spell_num, EF_TELEPORT))
            return true;

        /* Magic Missile OK */
        if (MAGIC_MISSILE == spell && borg.trait[BI_CDEPTH] <= 35)
            return true;

        /* others are rejected */
        return false;
    }

    /* Success */
    return true;
}

/*
 * fail rate on a spell
 */
int borg_spell_fail_rate(const enum borg_spells spell)
{
    int chance, minfail;

    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return 100;

    borg_magic *as = &borg_magics[spell_num];

    /* Access the spell  */
    chance = as->sfail;

    /* Reduce failure rate by "effective" level adjustment */
    chance -= 3 * (borg.trait[BI_CLEVEL] - as->level);

    /* Reduce failure rate by stat adjustment */
    chance -= borg.trait[BI_FAIL1];

    /* Fear makes the failrate higher */
    if (borg.trait[BI_ISAFRAID])
        chance += 20;

    /* Extract the minimum failure rate */
    minfail = borg.trait[BI_FAIL2];

    /* Non mage characters never get too good */
    if (!player_has(player, PF_ZERO_FAIL)) {
        if (minfail < 5)
            minfail = 5;
    }

    /* Necromancers are punished by being on lit squares */
    /* necromancers like the dark */
    if (borg.trait[BI_CLASS] == CLASS_NECROMANCER &&
        borg_grids[borg.c.y][borg.c.x].info & BORG_LIGHT) {
        chance += 25;

    }

    /* Minimum failure rate and max */
    if (chance < minfail)
        chance = minfail;
    if (chance > 50)
        chance = 50;

    /* Stunning makes spells harder */
    if (borg.trait[BI_ISHEAVYSTUN])
        chance += 25;
    if (borg.trait[BI_ISSTUN])
        chance += 15;

    /* Amnesia makes it harder */
    if (borg.trait[BI_ISFORGET])
        chance *= 2;

    /* Always a 5 percent chance of working */
    if (chance > 95)
        chance = 95;

    /* Return the chance */
    return (chance);
}

/*
 * same as borg_spell_okay with a fail % check
 */
bool borg_spell_okay_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell_okay(spell);
}

/*
 * Same as borg_spell with a fail % check
 */
bool borg_spell_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell(spell);
}

/*
 * Same as borg_spell_legal with a fail % check
 */
bool borg_spell_legal_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell_legal(spell);
}

/*
 * Attempt to cast a spell
 */
bool borg_spell(const enum borg_spells spell)
{
    int i;

    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* Require ability (right now) */
    if (!borg_spell_okay(spell))
        return false;

    /* Look for the book */
    i = borg.book_idx[as->book];

    /* Paranoia */
    if (i < 0)
        return false;

    /* Debugging Info */
    borg_note(format("# Casting %s (%d,%d).", as->name, i, as->book_offset));

    /* Cast a spell */
    borg_keypress('m');
    borg_keypress(all_letters_nohjkl[i]);
    borg_keypress(all_letters_nohjkl[as->book_offset]);

    /* increment the spell counter */
    as->times++;

    /* Success */
    return true;
}

/*
 * Cheat the "spell" info for a single book
 */
static void borg_cheat_spell(int book_num)
{
    struct class_book *book = &player->class->magic.books[book_num];
    for (int spell_num = 0; spell_num < book->num_spells; spell_num++) {
        struct class_spell *cspell = &book->spells[spell_num];
        borg_magic *as = &borg_magics[cspell->sidx];

        /* Note "forgotten" spells */
        if (player->spell_flags[cspell->sidx] & PY_SPELL_FORGOTTEN) {
            /* Forgotten */
            as->status = BORG_MAGIC_LOST;
        }

        /* Note "difficult" spells */
        else if (borg.trait[BI_CLEVEL] < as->level) {
            /* Unknown */
            as->status = BORG_MAGIC_HIGH;
        }

        /* Note "Unknown" spells */
        else if (!(player->spell_flags[cspell->sidx] & PY_SPELL_LEARNED)) {
            /* UnKnown */
            as->status = BORG_MAGIC_OKAY;
        }

        /* Note "untried" spells */
        else if (!(player->spell_flags[cspell->sidx] & PY_SPELL_WORKED)) {
            /* Untried */
            as->status = BORG_MAGIC_TEST;
        }

        /* Note "known" spells */
        else {
            /* Known */
            as->status = BORG_MAGIC_KNOW;
        }
    }
}

/*
 * Cheat the "spell" info
 */
void borg_cheat_spells(void)
{
    int i;

    /* Assume no books */
    for (i = 0; i < 9; i++)
        borg.book_idx[i] = -1;

    /* Scan the pack */
    for (i = 0; i < z_info->pack_size; i++) {
        int        book_num;
        borg_item *item = &borg_items[i];

        for (book_num = 0; book_num < player->class->magic.num_books;
            book_num++) {
            struct class_book book = player->class->magic.books[book_num];
            if (item->tval == book.tval && item->sval == book.sval) {
                /* Note book locations */
                borg.book_idx[book_num] = i;
                break;
            }
        }
    }

    /* only browse spells if casting is possible */
    if (!borg_can_cast())
        return;

    /* XXX XXX XXX Dark */

    for (int book_idx = 0; book_idx < 8; book_idx++)         {
        /* Look for the book */
        i = borg.book_idx[book_idx];

        /* Cheat the "spell" screens (all of them) */
        if (i >= 0)
            /* Cheat that page */
            borg_cheat_spell(book_idx);
    }

    return;
}


/*
 * Get the offset in the book this spell is so you can cast it (book) (offset)
 */
static int borg_get_book_offset(int index)
{
    int                       book = 0, count = 0;
    const struct class_magic *magic = &player->class->magic;

    /* Check index validity */
    if (index < 0 || index >= magic->total_spells)
        return -1;

    /* Find the book, count the spells in previous books */
    while (count + magic->books[book].num_spells - 1 < index)
        count += magic->books[book++].num_spells;

    /* Find the spell */
    return index - count;
}

/*
 * initialize the spell data
 */
static void borg_init_spell(borg_magic *spells, int spell_num)
{
    borg_magic               *spell  = &spells[spell_num];
    const struct class_spell *cspell = spell_by_index(player, spell_num);
    if (strcmp(cspell->name, borg_spell_ratings[spell_num].name)) {
        borg_note(format("**STARTUP FAILURE** spell definition mismatch. "
                         "<%s> not the same as <%s>",
            cspell->name, borg_spell_ratings[spell_num].name));
        borg_init_failure = true;
        return;
    }
    spell->rating       = borg_spell_ratings[spell_num].rating;
    spell->name         = borg_spell_ratings[spell_num].name;
    spell->spell_enum   = borg_spell_ratings[spell_num].spell_enum;
    spell->level        = cspell->slevel;
    spell->book_offset  = borg_get_book_offset(cspell->sidx);
    spell->effect_index = cspell->effect->index;
    spell->power        = cspell->smana;
    spell->sfail        = cspell->sfail;
    spell->status       = spell_okay_to_cast(player, spell_num);
    spell->times        = 0;
    spell->book         = cspell->bidx;
}

/*
 * Prepare a book
 */
void borg_prepare_book_info(void)
{
    switch (player->class->cidx) {
    case CLASS_MAGE:
        borg_spell_ratings = borg_spell_ratings_MAGE;
        break;
    case CLASS_DRUID:
        borg_spell_ratings = borg_spell_ratings_DRUID;
        break;
    case CLASS_PRIEST:
        borg_spell_ratings = borg_spell_ratings_PRIEST;
        break;
    case CLASS_NECROMANCER:
        borg_spell_ratings = borg_spell_ratings_NECROMANCER;
        break;
    case CLASS_PALADIN:
        borg_spell_ratings = borg_spell_ratings_PALADIN;
        break;
    case CLASS_ROGUE:
        borg_spell_ratings = borg_spell_ratings_ROGUE;
        break;
    case CLASS_RANGER:
        borg_spell_ratings = borg_spell_ratings_RANGER;
        break;
    case CLASS_BLACKGUARD:
        borg_spell_ratings = borg_spell_ratings_BLACKGUARD;
        break;
    default:
        borg_spell_ratings = NULL;
        return;
    }

    if (borg_magics)
        mem_free(borg_magics);

    borg_magics
        = mem_zalloc(player->class->magic.total_spells * sizeof(borg_magic));

    for (int spell = 0; spell < player->class->magic.total_spells; spell++) {
        borg_init_spell(borg_magics, spell);
    }
}

#endif
