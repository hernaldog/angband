/**
 * \file borg-item-val.c
 * \brief Carga los sval y kval de los objetos
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

#include "borg-item-val.h"

#ifdef ALLOW_BORG

#include "../init.h"
#include "../obj-tval.h"
#include "../obj-util.h"

#include "borg-init.h"
#include "borg-io.h"

int sv_food_apple;
int sv_food_ration;
int sv_food_slime_mold;
int sv_food_draught;
int sv_food_pint;
int sv_food_sip;
int sv_food_waybread;
int sv_food_honey_cake;
int sv_food_slice;
int sv_food_handful;

int sv_mush_second_sight;
int sv_mush_fast_recovery;
int sv_mush_restoring;
int sv_mush_mana;
int sv_mush_emergency;
int sv_mush_terror;
int sv_mush_stoneskin;
int kv_mush_stoneskin;
int sv_mush_debility;
int sv_mush_sprinting;
int sv_mush_cure_mind;
int sv_mush_purging;

int sv_light_lantern;
int sv_light_torch;

int sv_flask_oil;
int kv_flask_oil;

int sv_potion_cure_critical;
int sv_potion_cure_serious;
int sv_potion_cure_light;
int sv_potion_healing;
int kv_potion_healing;
int sv_potion_star_healing;
int sv_potion_life;
int sv_potion_restore_mana;
int kv_potion_restore_mana;
int sv_potion_cure_poison;
int sv_potion_resist_heat;
int sv_potion_resist_cold;
int sv_potion_resist_pois;
int sv_potion_inc_str;
int sv_potion_inc_int;
int sv_potion_inc_wis;
int sv_potion_inc_dex;
int sv_potion_inc_con;
int sv_potion_inc_str2;
int sv_potion_inc_int2;
int sv_potion_inc_wis2;
int sv_potion_inc_dex2;
int sv_potion_inc_con2;
int sv_potion_inc_all;
int sv_potion_restore_life;
int sv_potion_speed;
int sv_potion_berserk;
int sv_potion_sleep;
int sv_potion_slowness;
int sv_potion_poison;
int sv_potion_blindness;
int sv_potion_confusion;
int sv_potion_heroism;
int sv_potion_boldness;
int sv_potion_detect_invis;
int sv_potion_enlightenment;
int sv_potion_slime_mold;
int sv_potion_infravision;
int sv_potion_inc_exp;

int sv_scroll_identify;
int sv_scroll_phase_door;
int sv_scroll_teleport;
int sv_scroll_word_of_recall;
int sv_scroll_enchant_armor;
int sv_scroll_enchant_weapon_to_hit;
int sv_scroll_enchant_weapon_to_dam;
int sv_scroll_star_enchant_weapon;
int sv_scroll_star_enchant_armor;
int sv_scroll_protection_from_evil;
int sv_scroll_rune_of_protection;
int sv_scroll_teleport_level;
int sv_scroll_deep_descent;
int sv_scroll_recharging;
int sv_scroll_banishment;
int sv_scroll_mass_banishment;
int kv_scroll_mass_banishment;
int sv_scroll_blessing;
int sv_scroll_holy_chant;
int sv_scroll_holy_prayer;
int sv_scroll_detect_invis;
int sv_scroll_satisfy_hunger;
int sv_scroll_light;
int sv_scroll_mapping;
int sv_scroll_acquirement;
int sv_scroll_star_acquirement;
int sv_scroll_remove_curse;
int kv_scroll_remove_curse;
int sv_scroll_star_remove_curse;
int kv_scroll_star_remove_curse;
int sv_scroll_monster_confusion;
int sv_scroll_trap_door_destruction;
int sv_scroll_dispel_undead;

int sv_ring_flames;
int sv_ring_ice;
int sv_ring_acid;
int sv_ring_lightning;
int sv_ring_digging;
int sv_ring_speed;
int sv_ring_damage;
int sv_ring_dog;

int sv_amulet_teleportation;

int sv_rod_recall;
int kv_rod_recall;
int sv_rod_detection;
int sv_rod_illumination;
int sv_rod_speed;
int sv_rod_mapping;
int sv_rod_healing;
int kv_rod_healing;
int sv_rod_light;
int sv_rod_fire_bolt;
int sv_rod_elec_bolt;
int sv_rod_cold_bolt;
int sv_rod_acid_bolt;
int sv_rod_drain_life;
int sv_rod_fire_ball;
int sv_rod_elec_ball;
int sv_rod_cold_ball;
int sv_rod_acid_ball;
int sv_rod_teleport_other;
int sv_rod_slow_monster;
int sv_rod_sleep_monster;
int sv_rod_curing;

int sv_staff_teleportation;
int sv_staff_destruction;
int sv_staff_speed;
int sv_staff_healing;
int sv_staff_the_magi;
int sv_staff_power;
int sv_staff_holiness;
int kv_staff_holiness;
int sv_staff_curing;
int sv_staff_sleep_monsters;
int sv_staff_slow_monsters;
int sv_staff_detect_invis;
int sv_staff_detect_evil;
int sv_staff_dispel_evil;
int sv_staff_banishment;
int sv_staff_light;
int sv_staff_mapping;
int sv_staff_remove_curse;

int sv_wand_light;
int sv_wand_teleport_away;
int sv_wand_stinking_cloud;
int kv_wand_stinking_cloud;
int sv_wand_magic_missile;
int kv_wand_magic_missile;
int sv_wand_annihilation;
int kv_wand_annihilation;
int sv_wand_stone_to_mud;
int sv_wand_wonder;
int sv_wand_slow_monster;
int sv_wand_hold_monster;
int sv_wand_fear_monster;
int sv_wand_confuse_monster;
int sv_wand_fire_bolt;
int sv_wand_cold_bolt;
int sv_wand_acid_bolt;
int sv_wand_elec_bolt;
int sv_wand_fire_ball;
int sv_wand_cold_ball;
int sv_wand_acid_ball;
int sv_wand_elec_ball;
int sv_wand_dragon_cold;
int sv_wand_dragon_fire;
int sv_wand_drain_life;

int sv_dagger;

int sv_sling;
int sv_short_bow;
int sv_long_bow;
int sv_light_xbow;
int sv_heavy_xbow;

int sv_arrow_seeker;
int sv_arrow_mithril;

int sv_bolt_seeker;
int sv_bolt_mithril;

int sv_set_of_leather_gloves;

int sv_cloak;

int sv_robe;

int sv_iron_crown;

int sv_dragon_blue;
int sv_dragon_black;
int sv_dragon_white;
int sv_dragon_red;
int sv_dragon_green;
int sv_dragon_multihued;
int sv_dragon_shining;
int sv_dragon_law;
int sv_dragon_gold;
int sv_dragon_chaos;
int sv_dragon_balance;
int sv_dragon_power;

/* un ayudante para asegurar que nuestras definiciones son correctas */
static int borg_lookup_sval_fail(int tval, const char *name)
{
    int sval = lookup_sval(tval, name);
    if (sval == -1) {
        borg_note(
            format("**FALLO DE INICIALIZACIÓN** fallo de búsqueda de sval - %s ", name));
        borg_init_failure = true;
    }
    return sval;
}

void borg_init_item_val(void)
{
    int tval           = tval_find_idx("food");
    sv_food_apple      = borg_lookup_sval_fail(tval, "Manzana");
    sv_food_ration     = borg_lookup_sval_fail(tval, "Ración de Comida");
    sv_food_slime_mold = borg_lookup_sval_fail(tval, "Moho Baboso");
    sv_food_draught    = borg_lookup_sval_fail(tval, "Trago de los Ents");
    sv_food_pint       = borg_lookup_sval_fail(tval, "Pinta de Vino Fino");
    sv_food_sip        = borg_lookup_sval_fail(tval, "Sorbo de Miruvor");
    sv_food_waybread   = borg_lookup_sval_fail(tval, "Trozo de Pan Élfico");
    sv_food_honey_cake = borg_lookup_sval_fail(tval, "Pastel de Miel");
    sv_food_slice      = borg_lookup_sval_fail(tval, "Trozo de Carne");
    sv_food_handful    = borg_lookup_sval_fail(tval, "Puñado de Frutas Secas");

    tval               = tval_find_idx("mushroom");
    sv_mush_second_sight  = borg_lookup_sval_fail(tval, "Segunda Visión");
    sv_mush_fast_recovery = borg_lookup_sval_fail(tval, "Recuperación Rápida");
    sv_mush_restoring     = borg_lookup_sval_fail(tval, "Vigor");
    sv_mush_mana          = borg_lookup_sval_fail(tval, "Mente Clara");
    sv_mush_emergency     = borg_lookup_sval_fail(tval, "Emergencia");
    sv_mush_terror        = borg_lookup_sval_fail(tval, "Terror");
    sv_mush_stoneskin     = borg_lookup_sval_fail(tval, "Piel de Piedra");
    kv_mush_stoneskin     = borg_lookup_kind(tval, sv_mush_stoneskin);
    sv_mush_debility      = borg_lookup_sval_fail(tval, "Debilidad");
    sv_mush_sprinting     = borg_lookup_sval_fail(tval, "Espínt");
    sv_mush_cure_mind     = borg_lookup_sval_fail(tval, "Mente Clara");
    sv_mush_purging       = borg_lookup_sval_fail(tval, "Purgación");

    tval                  = tval_find_idx("light");
    sv_light_lantern      = borg_lookup_sval_fail(tval, "Linterna");
    sv_light_torch        = borg_lookup_sval_fail(tval, "Antorcha de Madera");

    tval                  = tval_find_idx("flask");
    sv_flask_oil          = borg_lookup_sval_fail(tval, "Frasco de Aceite");
    kv_flask_oil          = borg_lookup_kind(tval, sv_flask_oil);

    tval                  = tval_find_idx("potion");
    sv_potion_cure_critical
        = borg_lookup_sval_fail(tval, "Cura Heridas Críticas");
    sv_potion_cure_serious = borg_lookup_sval_fail(tval, "Cura Heridas Graves");
    sv_potion_cure_light   = borg_lookup_sval_fail(tval, "Cura Heridas Leves");
    sv_potion_healing      = borg_lookup_sval_fail(tval, "Curación");
    kv_potion_healing      = borg_lookup_kind(tval, sv_potion_healing);
    sv_potion_star_healing = borg_lookup_sval_fail(tval, "*Curación*");
    sv_potion_life         = borg_lookup_sval_fail(tval, "Vida");
    sv_potion_restore_mana = borg_lookup_sval_fail(tval, "Restaurar Maná");
    kv_potion_restore_mana = borg_lookup_kind(tval, sv_potion_restore_mana);
    sv_potion_cure_poison  = borg_lookup_sval_fail(tval, "Neutralizar Veneno");
    sv_potion_resist_heat  = borg_lookup_sval_fail(tval, "Resistir Calor");
    sv_potion_resist_cold  = borg_lookup_sval_fail(tval, "Resistir Frío");
    sv_potion_resist_pois  = borg_lookup_sval_fail(tval, "Resistir Veneno");
    sv_potion_inc_str      = borg_lookup_sval_fail(tval, "Fuerza");
    sv_potion_inc_int      = borg_lookup_sval_fail(tval, "Inteligencia");
    sv_potion_inc_wis      = borg_lookup_sval_fail(tval, "Sabiduría");
    sv_potion_inc_dex      = borg_lookup_sval_fail(tval, "Destreza");
    sv_potion_inc_con      = borg_lookup_sval_fail(tval, "Constitución");
    sv_potion_inc_all      = borg_lookup_sval_fail(tval, "Aumento");
    sv_potion_inc_str2     = borg_lookup_sval_fail(tval, "Fortaleza");
    sv_potion_inc_int2     = borg_lookup_sval_fail(tval, "intelecto");
    sv_potion_inc_wis2     = borg_lookup_sval_fail(tval, "Contemplación");
    sv_potion_inc_dex2     = borg_lookup_sval_fail(tval, "Agilidad");
    sv_potion_inc_con2     = borg_lookup_sval_fail(tval, "Robustez");
    sv_potion_restore_life = borg_lookup_sval_fail(tval, "Restaurar Niveles de Vida");
    sv_potion_speed        = borg_lookup_sval_fail(tval, "Velocidad");
    sv_potion_berserk      = borg_lookup_sval_fail(tval, "Fuerza Berserker");
    sv_potion_sleep        = borg_lookup_sval_fail(tval, "Sueño");
    sv_potion_slowness     = borg_lookup_sval_fail(tval, "Lentitud");
    sv_potion_poison       = borg_lookup_sval_fail(tval, "Veneno");
    sv_potion_blindness    = borg_lookup_sval_fail(tval, "Ceguera");
    sv_potion_confusion    = borg_lookup_sval_fail(tval, "Confusión");
    sv_potion_heroism      = borg_lookup_sval_fail(tval, "Heroísmo");
    sv_potion_boldness     = borg_lookup_sval_fail(tval, "Osadía");
    sv_potion_detect_invis = borg_lookup_sval_fail(tval, "Visión Verdadera");
    sv_potion_enlightenment  = borg_lookup_sval_fail(tval, "Iluminación");
    sv_potion_slime_mold     = borg_lookup_sval_fail(tval, "Jugo de Moho Baboso");
    sv_potion_infravision    = borg_lookup_sval_fail(tval, "Infravición");
    sv_potion_inc_exp        = borg_lookup_sval_fail(tval, "Experiencia");

    tval                     = tval_find_idx("scroll");
    sv_scroll_identify       = borg_lookup_sval_fail(tval, "Runa de Identificar");
    sv_scroll_phase_door     = borg_lookup_sval_fail(tval, "Puerta de Fase");
    sv_scroll_teleport       = borg_lookup_sval_fail(tval, "Teletransporte");
    sv_scroll_word_of_recall = borg_lookup_sval_fail(tval, "Palabra de Retorno");
    sv_scroll_enchant_armor  = borg_lookup_sval_fail(tval, "Encantar Armadura");
    sv_scroll_enchant_weapon_to_hit
        = borg_lookup_sval_fail(tval, "Encantar Arma Para-Golpear");
    sv_scroll_enchant_weapon_to_dam
        = borg_lookup_sval_fail(tval, "Encantar Arma Para-Dañar");
    sv_scroll_star_enchant_armor
        = borg_lookup_sval_fail(tval, "*Encantar Armadura*");
    sv_scroll_star_enchant_weapon
        = borg_lookup_sval_fail(tval, "*Encantar Arma*");
    sv_scroll_protection_from_evil
        = borg_lookup_sval_fail(tval, "Protección Contra el Mal");
    sv_scroll_rune_of_protection
        = borg_lookup_sval_fail(tval, "Runa de Protección");
    sv_scroll_teleport_level  = borg_lookup_sval_fail(tval, "Teletransporte de Nivel");
    sv_scroll_deep_descent    = borg_lookup_sval_fail(tval, "Descenso Profundo");
    sv_scroll_recharging      = borg_lookup_sval_fail(tval, "Recarga");
    sv_scroll_banishment      = borg_lookup_sval_fail(tval, "Exilio");
    sv_scroll_mass_banishment = borg_lookup_sval_fail(tval, "Exilio Masivo");
    kv_scroll_mass_banishment
        = borg_lookup_kind(tval, sv_scroll_mass_banishment);
    sv_scroll_blessing       = borg_lookup_sval_fail(tval, "Bendición");
    sv_scroll_holy_chant     = borg_lookup_sval_fail(tval, "Cántico Sagrado");
    sv_scroll_holy_prayer    = borg_lookup_sval_fail(tval, "Plegaria Sagrada");
    sv_scroll_detect_invis   = borg_lookup_sval_fail(tval, "Detectar Invisibles");
    sv_scroll_satisfy_hunger = borg_lookup_sval_fail(tval, "Quitar Hambre");
    sv_scroll_light          = borg_lookup_sval_fail(tval, "Luz");
    sv_scroll_mapping        = borg_lookup_sval_fail(tval, "Mapa Mágico");
    sv_scroll_acquirement    = borg_lookup_sval_fail(tval, "Adquisición");
    sv_scroll_star_acquirement = borg_lookup_sval_fail(tval, "*Adquisición*");
    sv_scroll_remove_curse     = borg_lookup_sval_fail(tval, "Eliminar Maldición");
    kv_scroll_remove_curse     = borg_lookup_kind(tval, sv_scroll_remove_curse);
    sv_scroll_star_remove_curse = borg_lookup_sval_fail(tval, "*Eliminar Maldición*");
    kv_scroll_star_remove_curse
        = borg_lookup_kind(tval, sv_scroll_star_remove_curse);
    sv_scroll_monster_confusion
        = borg_lookup_sval_fail(tval, "Confusión de Monstruos");
    sv_scroll_trap_door_destruction
        = borg_lookup_sval_fail(tval, "Destrucción de Puertas");
    sv_scroll_dispel_undead = borg_lookup_sval_fail(tval, "Disipar No-muertos");

    tval                    = tval_find_idx("ring");
    sv_ring_flames          = borg_lookup_sval_fail(tval, "Llamas");
    sv_ring_ice             = borg_lookup_sval_fail(tval, "Hielo");
    sv_ring_acid            = borg_lookup_sval_fail(tval, "Ácido");
    sv_ring_lightning       = borg_lookup_sval_fail(tval, "Relámpagos");
    sv_ring_digging         = borg_lookup_sval_fail(tval, "Excavación");
    sv_ring_speed           = borg_lookup_sval_fail(tval, "Velocidad");
    sv_ring_damage          = borg_lookup_sval_fail(tval, "Daño");
    sv_ring_dog             = borg_lookup_sval_fail(tval, "del Perro");

    tval                    = tval_find_idx("amulet");
    sv_amulet_teleportation = borg_lookup_sval_fail(tval, "Teletransporte");

    tval                    = tval_find_idx("rod");
    sv_rod_recall           = borg_lookup_sval_fail(tval, "Retorno");
    kv_rod_recall           = borg_lookup_kind(tval, sv_rod_recall);
    sv_rod_detection        = borg_lookup_sval_fail(tval, "Detección");
    sv_rod_illumination     = borg_lookup_sval_fail(tval, "Iluminación");
    sv_rod_speed            = borg_lookup_sval_fail(tval, "Velocidad");
    sv_rod_mapping          = borg_lookup_sval_fail(tval, "Mapa Mágico");
    sv_rod_healing          = borg_lookup_sval_fail(tval, "Curación");
    kv_rod_healing          = borg_lookup_kind(tval, sv_rod_healing);
    sv_rod_light            = borg_lookup_sval_fail(tval, "Luz");
    sv_rod_fire_bolt        = borg_lookup_sval_fail(tval, "Proyectil de Fuego");
    sv_rod_elec_bolt        = borg_lookup_sval_fail(tval, "Proyectil de Relámpago");
    sv_rod_cold_bolt        = borg_lookup_sval_fail(tval, "Proyectil de Escarcha");
    sv_rod_acid_bolt        = borg_lookup_sval_fail(tval, "Proyectil de Ácido");
    sv_rod_drain_life       = borg_lookup_sval_fail(tval, "Drenar Vida");
    sv_rod_fire_ball        = borg_lookup_sval_fail(tval, "Bola de Fuego");
    sv_rod_elec_ball        = borg_lookup_sval_fail(tval, "Bola de Relámpagos");
    sv_rod_cold_ball        = borg_lookup_sval_fail(tval, "Bola de Frío");
    sv_rod_acid_ball        = borg_lookup_sval_fail(tval, "Bola de Ácido");
    sv_rod_teleport_other   = borg_lookup_sval_fail(tval, "Teletransportar Otro");
    sv_rod_slow_monster     = borg_lookup_sval_fail(tval, "Ralentizar Monstruo");
    sv_rod_sleep_monster    = borg_lookup_sval_fail(tval, "Paralizar Monstruo");
    sv_rod_curing           = borg_lookup_sval_fail(tval, "Curación");

    tval                    = tval_find_idx("staff");
    sv_staff_teleportation  = borg_lookup_sval_fail(tval, "Teletransporte");
    sv_staff_destruction    = borg_lookup_sval_fail(tval, "*Destrucción*");
    sv_staff_speed          = borg_lookup_sval_fail(tval, "Velocidad");
    sv_staff_healing        = borg_lookup_sval_fail(tval, "Curación");
    sv_staff_the_magi       = borg_lookup_sval_fail(tval, "del Mago");
    sv_staff_power          = borg_lookup_sval_fail(tval, "Poder");
    sv_staff_holiness       = borg_lookup_sval_fail(tval, "Santidad");
    kv_staff_holiness       = borg_lookup_kind(tval, sv_staff_holiness);
    sv_staff_curing         = borg_lookup_sval_fail(tval, "Curación");
    sv_staff_sleep_monsters = borg_lookup_sval_fail(tval, "Dormir Monstruos");
    sv_staff_slow_monsters  = borg_lookup_sval_fail(tval, "Ralentizar Monstruos");
    sv_staff_detect_invis   = borg_lookup_sval_fail(tval, "Detectar Invisibles");
    sv_staff_detect_evil    = borg_lookup_sval_fail(tval, "Detectar Mal");
    sv_staff_dispel_evil    = borg_lookup_sval_fail(tval, "Disipar Mal");
    sv_staff_banishment     = borg_lookup_sval_fail(tval, "Exilio");
    sv_staff_light          = borg_lookup_sval_fail(tval, "Luz");
    sv_staff_mapping        = borg_lookup_sval_fail(tval, "Mapa");
    sv_staff_remove_curse   = borg_lookup_sval_fail(tval, "Eliminar Maldición");

    tval                    = tval_find_idx("wand");
    sv_wand_light           = borg_lookup_sval_fail(tval, "Luz");
    sv_wand_teleport_away   = borg_lookup_sval_fail(tval, "Teletransportar Otro");
    sv_wand_stinking_cloud  = borg_lookup_sval_fail(tval, "Nube Apestosa");
    kv_wand_stinking_cloud  = borg_lookup_kind(tval, sv_wand_stinking_cloud);
    sv_wand_magic_missile   = borg_lookup_sval_fail(tval, "Proyectil Mágico");
    kv_wand_magic_missile   = borg_lookup_kind(tval, sv_wand_magic_missile);
    sv_wand_annihilation    = borg_lookup_sval_fail(tval, "Aniquilación");
    kv_wand_annihilation    = borg_lookup_kind(tval, sv_wand_annihilation);
    sv_wand_stone_to_mud    = borg_lookup_sval_fail(tval, "Piedra a Lodo");
    sv_wand_wonder          = borg_lookup_sval_fail(tval, "Maravilla");
    sv_wand_hold_monster    = borg_lookup_sval_fail(tval, "Paralizar Monstruo");
    sv_wand_slow_monster    = borg_lookup_sval_fail(tval, "Ralentizar Monstruo");
    sv_wand_fear_monster    = borg_lookup_sval_fail(tval, "Asustar Monstruo");
    sv_wand_confuse_monster = borg_lookup_sval_fail(tval, "Confundir Monstruo");
    sv_wand_fire_bolt       = borg_lookup_sval_fail(tval, "Proyectil de Fuego");
    sv_wand_cold_bolt       = borg_lookup_sval_fail(tval, "Proyectil de Escarcha");
    sv_wand_acid_bolt       = borg_lookup_sval_fail(tval, "Proyectil de Ácido");
    sv_wand_elec_bolt       = borg_lookup_sval_fail(tval, "Proyectil de Relámpago");
    sv_wand_fire_ball       = borg_lookup_sval_fail(tval, "Bola de Fuego");
    sv_wand_cold_ball       = borg_lookup_sval_fail(tval, "Bola de Frío");
    sv_wand_acid_ball       = borg_lookup_sval_fail(tval, "Bola de Ácido");
    sv_wand_elec_ball       = borg_lookup_sval_fail(tval, "Bola de Relámpagos");
    sv_wand_dragon_cold     = borg_lookup_sval_fail(tval, "Escarcha de Dragón");
    sv_wand_dragon_fire     = borg_lookup_sval_fail(tval, "Llama de Dragón");
    sv_wand_drain_life      = borg_lookup_sval_fail(tval, "Drenar Vida");

    tval                    = tval_find_idx("sword");
    sv_dagger               = borg_lookup_sval_fail(tval, "Daga");

    tval                    = tval_find_idx("bow");
    sv_sling                = borg_lookup_sval_fail(tval, "Honda");
    sv_short_bow            = borg_lookup_sval_fail(tval, "Arco Corto");
    sv_long_bow             = borg_lookup_sval_fail(tval, "Arco Largo");
    sv_light_xbow           = borg_lookup_sval_fail(tval, "Ballesta Ligera");
    sv_heavy_xbow           = borg_lookup_sval_fail(tval, "Ballesta Pesada");

    tval                    = tval_find_idx("arrow");
    sv_arrow_seeker         = borg_lookup_sval_fail(tval, "Flecha Buscadora");
    sv_arrow_mithril        = borg_lookup_sval_fail(tval, "Flecha de Mithril");

    tval                    = tval_find_idx("bolt");
    sv_bolt_seeker          = borg_lookup_sval_fail(tval, "Virote Buscador");
    sv_bolt_mithril         = borg_lookup_sval_fail(tval, "Virote de Mithril");

    tval                    = tval_find_idx("gloves");
    sv_set_of_leather_gloves
        = borg_lookup_sval_fail(tval, "Par de Guantes de Cuero");

    tval            = tval_find_idx("cloak");
    sv_cloak        = borg_lookup_sval_fail(tval, "Capa");

    tval            = tval_find_idx("soft armor");
    sv_robe         = borg_lookup_sval_fail(tval, "Túnica");

    tval            = tval_find_idx("crown");
    sv_iron_crown   = borg_lookup_sval_fail(tval, "Corona de Hierro");

    tval            = tval_find_idx("dragon armor");
    sv_dragon_blue  = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Azul");
    sv_dragon_black = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Negro");
    sv_dragon_white = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Blanco");
    sv_dragon_red   = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Rojo");
    sv_dragon_green = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Verde");
    sv_dragon_multihued
        = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Multicolor");
    sv_dragon_shining
        = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Brillante");
    sv_dragon_law   = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón de la Ley");
    sv_dragon_gold  = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón Dorado");
    sv_dragon_chaos = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón del Caos");
    sv_dragon_balance
        = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón del Equilibrio");
    sv_dragon_power = borg_lookup_sval_fail(tval, "Malla de Escamas de Dragón del Poder");
}

/**
 * Devuelve el k_idx del tipo de objeto con el `tval` y `sval` dados, o 0.
 */
int borg_lookup_kind(int tval, int sval)
{
    int k;

    /* Buscarlo */
    for (k = 1; k < z_info->k_max; k++) {
        struct object_kind *k_ptr = &k_info[k];

        /* Encontró una coincidencia */
        if ((k_ptr->tval == tval) && (k_ptr->sval == sval))
            return (k);
    }

    /* Fracaso */
    msg("No hay objeto (%s,%d,%d)", tval_find_name(tval), tval, sval);
    return 0;
}

#endif