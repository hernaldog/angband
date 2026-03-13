/**
 * \file init.h
 * \brief inicialización
 *
 * Copyright (c) 2000 Robert Ruehlmann
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 */

#ifndef INCLUDED_INIT_H
#define INCLUDED_INIT_H

#include "h-basic.h"
#include "z-bitflag.h"
#include "z-file.h"
#include "z-rand.h"
#include "z-util.h"
#include "datafile.h"
#include "object.h"

/* Definir un nivel de gravedad para un crítico no-O */
struct critical_level {
	struct critical_level *next;
	int cutoff;		/* poderes menores que este son incluidos;
					ignorado para el último nivel */
	int mult;		/* multiplicador de daño para este nivel */
	int add;		/* daño aditivo para este nivel */
	int msgt;		/* tipo de mensaje a usar para este nivel */
};

/* Definir un nivel de gravedad para un crítico O */
struct o_critical_level {
	struct o_critical_level *next;
	unsigned int chance;		/* una probabilidad entre este número para este nivel a menos
						que este sea el último nivel; el resto
						pasa al siguiente nivel */
	unsigned int added_dice;	/* número de dados añadidos para este
						nivel */
	int msgt;			/* tipo de mensaje a usar para este nivel */
};

/**
 * Información sobre índices máximos de ciertos arreglos.
 *
 * Esto se convertirá en una lista de "todas" las constantes del juego - NRM
 */
struct angband_constants
{
	/* Límites de arreglos, etc., establecidos al analizar archivos de edición */
	uint16_t store_max;	/**< Número máximo de tiendas */
	uint16_t trap_max;	/**< Número máximo de tipos de trampas */
	uint16_t k_max;		/**< Número máximo de tipos base de objetos */
	uint16_t a_max;		/**< Número máximo de tipos de artefactos */
	uint16_t e_max;		/**< Número máximo de tipos de objetos-ego */
	uint16_t r_max;		/**< Número máximo de razas de monstruos */
	uint16_t mp_max;	/**< Número máximo de conjuntos de mensajes de dolor de monstruos */
	uint16_t s_max;		/**< Número máximo de hechizos mágicos */
	uint16_t pit_max;	/**< Número máximo de tipos de fosos de monstruos */
	uint16_t act_max;	/**< Número máximo de activaciones para artefactos aleatorios */
	uint8_t curse_max;	/**< Número máximo de maldiciones */
	uint8_t slay_max;	/**< Número máximo de matanzas */
	uint8_t brand_max;	/**< Número máximo de marcas elementales */
	uint16_t mon_blows_max;	/**< Número máximo de golpes de monstruo */
	uint16_t blow_methods_max;	/**< Número máximo de métodos de golpe de monstruo */
	uint16_t blow_effects_max;	/**< Número máximo de efectos de golpe de monstruo */
	uint16_t equip_slots_max;	/**< Número máximo de ranuras de equipo del jugador */
	uint16_t profile_max;	/**< Número máximo de perfiles de cueva */
	uint16_t quest_max;	/**< Número máximo de misiones */
	uint16_t projection_max;	/**< Número máximo de tipos de proyección */
	uint16_t calculation_max;	/**< Número máximo de cálculos de poder de objetos */
	uint16_t property_max;	/**< Número máximo de propiedades de objetos */
	uint16_t ordinary_kind_max;	/**< Número máximo de objetos en object.txt */
	uint16_t shape_max;	/**< Número máximo de formas del jugador */

	/* Máximos de cosas en un nivel dado, leídos de constants.txt */
	uint16_t level_monster_max;	/**< Número máximo de monstruos en un nivel dado */

	/* Constantes de generación de monstruos, leídas de constants.txt */
	uint16_t alloc_monster_chance;	/**< 1/probabilidad-por-turno de generación */
	uint16_t level_monster_min;	/**< Número mínimo generado */
	uint16_t town_monsters_day;	/**< Habitantes del pueblo generados - día */
	uint16_t town_monsters_night;	/**< Habitantes del pueblo generados - noche */
	uint16_t repro_monster_max;	/**< Máximo de criaturas reproductoras en un nivel */
	uint16_t ood_monster_chance;	/**< Probabilidad de monstruo fuera de profundidad es 1 entre este */
	uint16_t ood_monster_amount;	/**< Número máximo de niveles fuera de profundidad */
	uint16_t monster_group_max;	/**< Tamaño máximo de un grupo */
	uint16_t monster_group_dist;	/**< Distancia máxima de un grupo de un grupo relacionado */

	/* Constantes de jugabilidad de monstruos, leídas de constants.txt */
	uint16_t glyph_hardness;	/**< Qué difícil es para un monstruo romper un glifo */
	uint16_t repro_monster_rate;	/**< Tasa de reproducción de monstruos - más lenta */
	uint16_t life_drain_percent;	/**< Porcentaje de vida del jugador drenado */
	uint16_t flee_range;		/**< Los monstruos huyen esta cantidad de casillas fuera de la vista */
	uint16_t turn_range;		/**< Los monstruos se giran para luchar más cerca de esto */

	/* Constantes de generación de mazmorras, leídas de constants.txt */
	uint16_t level_room_max;	/**< Número máximo de habitaciones en un nivel */
	uint16_t level_door_max;	/**< Número máximo de puertas potenciales en un nivel */
	uint16_t wall_pierce_max;	/**< Número máximo de perforaciones de pared potenciales */
	uint16_t tunn_grid_max;		/**< Número máximo de casillas de túnel */
	uint16_t room_item_av;		/**< Número promedio de objetos en habitaciones */
	uint16_t both_item_av;		/**< Número promedio de objetos en lugares aleatorios */
	uint16_t both_gold_av;		/**< Número promedio de objetos de dinero */
	uint16_t level_pit_max;		/**< Número máximo de fosos en un nivel */

	/* Constantes de forma del mundo, leídas de constants.txt */
	uint16_t max_depth;	/* Profundidad máxima de la mazmorra */
	uint16_t day_length;	/* Número de turnos de amanecer a amanecer */
	uint16_t dungeon_hgt;	/**< Número máximo de casillas verticales en un nivel */
	uint16_t dungeon_wid;	/**< Número máximo de casillas horizontales en un nivel */
	uint16_t town_hgt;	/**< Número máximo de casillas verticales en el pueblo */
	uint16_t town_wid;	/**< Número máximo de casillas horizontales en el pueblo */
	uint16_t feeling_total;	/* Número total de casillas de sensación por nivel */
	uint16_t feeling_need;	/* Casillas necesarias de ver para obtener la primera sensación */
	uint16_t stair_skip;	/* Número de niveles a saltar por cada escalera descendente */
	uint16_t move_energy;	/* Energía que el jugador o monstruo necesita para moverse */

	/* Constantes de capacidad de carga, leídas de constants.txt */
	uint16_t pack_size;		/**< Número máximo de espacios en la mochila */
	uint16_t quiver_size;		/**< Número máximo de espacios en la aljaba */
	uint16_t quiver_slot_size;	/**< Número máximo de proyectiles por espacio de aljaba */
	uint16_t thrown_quiver_mult;	/**< Multiplicador de tamaño para no-municiones en la aljaba */
	uint16_t floor_size;		/**< Número máximo de objetos por casilla de suelo */

	/* Parámetros de tienda, leídos de constants.txt */
	uint16_t store_inven_max;	/**< Número máximo de objetos en el inventario de la tienda */
	uint16_t store_turns;		/**< Número de turnos entre rotaciones */
	uint16_t store_shuffle;		/**< 1/probabilidad-por-día de cambio de dueño */
	uint16_t store_magic_level;	/**< Nivel para apply_magic() en tiendas normales */

	/* Constantes de creación de objetos, leídas de constants.txt */
	uint16_t max_obj_depth;	/* Profundidad máxima usada en la asignación de objetos */
	uint16_t great_obj;	/* 1/probabilidad de inflar el nivel de objeto solicitado */
	uint16_t great_ego;	/* 1/probabilidad de inflar el nivel de objeto-ego solicitado */
	uint16_t fuel_torch;	/* Cantidad máxima de combustible en una antorcha */
	uint16_t fuel_lamp;	/* Cantidad máxima de combustible en un farol */
	uint16_t default_lamp;	/* Cantidad predeterminada de combustible en un farol */

	/* Constantes del jugador, leídas de constants.txt */
	uint16_t max_sight;	/* Rango visual máximo */
	uint16_t max_range;	/* Rango máximo de proyectiles y hechizos */
	uint16_t start_gold;	/* Cantidad de oro con la que comienza el jugador */
	uint16_t food_value;	/* Número de turnos que dura el 1% de comida */

	/*
	 * Constantes para cálculos críticos de combate cuerpo a cuerpo no-O; leídas de
	 * constants.txt
	 */
	int m_crit_debuff_toh;
	int m_crit_chance_weight_scl;
	int m_crit_chance_toh_scl;
	int m_crit_chance_level_scl;
	int m_crit_chance_toh_skill_scl;
	int m_crit_chance_offset;
	int m_crit_chance_range;
	int m_crit_power_weight_scl;
	int m_crit_power_random;
	struct critical_level *m_crit_level_head;

	/*
	 * Constantes para cálculos críticos de combate a distancia no-O; leídas de
	 * constants.txt
	 */
	int r_crit_debuff_toh;
	int r_crit_chance_weight_scl;
	int r_crit_chance_toh_scl;
	int r_crit_chance_level_scl;
	int r_crit_chance_launched_toh_skill_scl;
	int r_crit_chance_thrown_toh_skill_scl;
	int r_crit_chance_offset;
	int r_crit_chance_range;
	int r_crit_power_weight_scl;
	int r_crit_power_random;
	struct critical_level *r_crit_level_head;

	/*
	 * Constantes para cálculos críticos de combate cuerpo a cuerpo O; leídas de
	 * constants.txt
	 */
	int o_m_crit_debuff_toh;
	int o_m_crit_power_toh_scl_num;
	int o_m_crit_power_toh_scl_den;
	int o_m_crit_chance_power_scl_num;
	int o_m_crit_chance_power_scl_den;
	int o_m_crit_chance_add_den;
	struct o_critical_level *o_m_crit_level_head;
	/*
	 * Para información de objetos, los niveles críticos O no dependen de las
	 * propiedades del jugador o el arma, por lo que pueden sumarse una vez
	 * después de cargar el archivo de constantes y almacenarse aquí.
	 * Las sumas se realizan en obj-info.c.
	 */
	struct my_rational o_m_max_added;

	/*
	 * Constantes para cálculos críticos de combate a distancia O; leídas de
	 * constants.txt
	 */
	int o_r_crit_debuff_toh;
	int o_r_crit_power_launched_toh_scl_num;
	int o_r_crit_power_launched_toh_scl_den;
	int o_r_crit_power_thrown_toh_scl_num;
	int o_r_crit_power_thrown_toh_scl_den;
	int o_r_crit_chance_power_scl_num;
	int o_r_crit_chance_power_scl_den;
	int o_r_crit_chance_add_den;
	struct o_critical_level *o_r_crit_level_head;
	/* Ver comentario para o_m_max_added arriba. */
	struct my_rational o_r_max_added;
};

struct init_module {
	const char *name;
	void (*init)(void);
	void (*cleanup)(void);
};

extern bool play_again;

extern const char *list_element_names[];
extern const char *list_obj_flag_names[];

extern struct angband_constants *z_info;

extern const char *ANGBAND_SYS;

extern char *ANGBAND_DIR_GAMEDATA;
extern char *ANGBAND_DIR_CUSTOMIZE;
extern char *ANGBAND_DIR_HELP;
extern char *ANGBAND_DIR_SCREENS;
extern char *ANGBAND_DIR_FONTS;
extern char *ANGBAND_DIR_TILES;
extern char *ANGBAND_DIR_SOUNDS;
extern char *ANGBAND_DIR_ICONS;
extern char *ANGBAND_DIR_USER;
extern char *ANGBAND_DIR_SAVE;
extern char *ANGBAND_DIR_PANIC;
extern char *ANGBAND_DIR_SCORES;
extern char *ANGBAND_DIR_ARCHIVE;

extern struct parser *init_parse_artifact(void);
extern struct parser *init_parse_ego(void);
extern struct parser *init_parse_object(void);
extern struct parser *init_parse_object_base(void);
extern struct parser *init_parse_pain(void);
extern struct parser *init_parse_pit(void);
extern struct parser *init_parse_monster(void);
extern struct parser *init_parse_vault(void);
extern struct parser *init_parse_chest_trap(void);
extern struct parser *init_parse_quest(void);

/* Estos son públicos principalmente para facilitar la escritura de casos de prueba */
extern struct file_parser body_parser;
extern struct file_parser class_parser;
extern struct file_parser constants_parser;
extern struct file_parser feat_parser;
extern struct file_parser flavor_parser;
extern struct file_parser hints_parser;
extern struct file_parser history_parser;
extern struct file_parser names_parser;
extern struct file_parser player_property_parser;
extern struct file_parser p_race_parser;
extern struct file_parser realm_parser;
extern struct file_parser shape_parser;
extern struct file_parser trap_parser;
extern struct file_parser world_parser;

errr grab_effect_data(struct parser *p, struct effect *effect);
extern void init_file_paths(const char *config, const char *lib, const char *data);
extern void init_game_constants(void);
extern void init_arrays(void);
extern void create_needed_dirs(void);
extern bool init_angband(void);
extern void cleanup_angband(void);

#endif /* INCLUDED_INIT_H */