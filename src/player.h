/**
 * \archivo player.h
 * \brief Implementación del jugador
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2011 elly+angband@leptoquark.net. See COPYING.
 * Copyright (c) 2015 Nick McConnell
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

#ifndef PLAYER_H
#define PLAYER_H

#include "guid.h"
#include "obj-properties.h"
#include "object.h"
#include "option.h"

/**
 * Índices de las estadísticas del jugador (fijados por los archivos guardados).
 */
enum {
	#define STAT(a) STAT_##a,
	#include "list-stats.h"
	#undef STAT

	STAT_MAX
};

/**
 * Banderas de raza y clase del jugador
 */
enum
{
	#define PF(a) PF_##a,
	#include "list-player-flags.h"
	#undef PF
	PF_MAX
};

#define PF_SIZE                FLAG_SIZE(PF_MAX)

#define pf_has(f, flag)        flag_has_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_next(f, flag)       flag_next(f, PF_SIZE, flag)
#define pf_is_empty(f)         flag_is_empty(f, PF_SIZE)
#define pf_is_full(f)          flag_is_full(f, PF_SIZE)
#define pf_is_inter(f1, f2)    flag_is_inter(f1, f2, PF_SIZE)
#define pf_is_subset(f1, f2)   flag_is_subset(f1, f2, PF_SIZE)
#define pf_is_equal(f1, f2)    flag_is_equal(f1, f2, PF_SIZE)
#define pf_on(f, flag)         flag_on_dbg(f, PF_SIZE, flag, #f, #flag)
#define pf_off(f, flag)        flag_off(f, PF_SIZE, flag)
#define pf_wipe(f)             flag_wipe(f, PF_SIZE)
#define pf_setall(f)           flag_setall(f, PF_SIZE)
#define pf_negate(f)           flag_negate(f, PF_SIZE)
#define pf_copy(f1, f2)        flag_copy(f1, f2, PF_SIZE)
#define pf_union(f1, f2)       flag_union(f1, f2, PF_SIZE)
#define pf_inter(f1, f2)       flag_inter(f1, f2, PF_SIZE)
#define pf_diff(f1, f2)        flag_diff(f1, f2, PF_SIZE)

/**
 * El rango de posibles índices en tablas basadas en estadísticas.
 * Actualmente las cosas van de 3 a 18/220 = 40.
 */
#define STAT_RANGE 38

/**
 * Constantes del jugador
 */
#define PY_MAX_EXP		99999999L	/* Exp máxima */
#define PY_KNOW_LEVEL	30			/* Nivel para conocer todas las runas */
#define PY_MAX_LEVEL	50			/* Nivel máximo */

/**
 * Banderas para player.spell_flags[]
 */
#define PY_SPELL_LEARNED    0x01 	/* El hechizo ha sido aprendido */
#define PY_SPELL_WORKED     0x02 	/* El hechizo se ha probado con éxito */
#define PY_SPELL_FORGOTTEN  0x04 	/* El hechizo ha sido olvidado */

#define BTH_PLUS_ADJ    	3 		/* Ajustar BTH por bonificación para golpear */

/**
 * Formas en que los jugadores pueden ser marcados como tramposos
 */
#define NOSCORE_WIZARD		0x0002
#define NOSCORE_DEBUG		0x0008
#define NOSCORE_JUMPING     0x0010
#ifdef ALLOW_BORG
#define NOSCORE_BORG		0x0020
#endif

/**
 * Terreno que el jugador tiene probabilidad de excavar
 */
enum {
	DIGGING_RUBBLE = 0,
	DIGGING_MAGMA,
	DIGGING_QUARTZ,
	DIGGING_GRANITE,
	DIGGING_DOORS,

	DIGGING_MAX
};

/**
 * Índices de habilidades
 */
enum {
	SKILL_DISARM_PHYS,		/* Desarme - físico */
	SKILL_DISARM_MAGIC,		/* Desarme - mágico */
	SKILL_DEVICE,			/* Dispositivos Mágicos */
	SKILL_SAVE,				/* Tirada de salvación */
	SKILL_SEARCH,			/* Capacidad de búsqueda */
	SKILL_STEALTH,			/* Factor de sigilo */
	SKILL_TO_HIT_MELEE,		/* Probabilidad de golpe (normal) */
	SKILL_TO_HIT_BOW,		/* Probabilidad de golpe (disparo) */
	SKILL_TO_HIT_THROW,		/* Probabilidad de golpe (lanzar) */
	SKILL_DIGGING,			/* Excavar */

	SKILL_MAX
};

/**
 * Estructura para las "misiones"
 */
struct quest
{
	struct quest *next;
	uint8_t index;
	char *name;
	uint8_t level;			/* Nivel de la mazmorra */
	struct monster_race *race;	/* Raza del monstruo */
	int cur_num;			/* Número asesinado (sin usar) */
	int max_num;			/* Número requerido (sin usar) */
};

/**
 * Una sola ranura de equipo
 */
struct equip_slot {
	struct equip_slot *next;

	uint16_t type;
	char *name;
	struct object *obj;
};

/**
 * Un 'cuerpo' de jugador
 */
struct player_body {
	struct player_body *next;

	char *name;
	uint16_t count;
	struct equip_slot *slots;
};

/**
 * Información de raza del jugador
 */
struct player_race {
	struct player_race *next;
	const char *name;

	unsigned int ridx;

	int r_mhp;		/**< Modificador de dados de vida */
	int r_exp;		/**< Factor de experiencia */

	int b_age;		/**< Edad base */
	int m_age;		/**< Edad modificada */

	int base_hgt;	/**< Altura base */
	int mod_hgt;	/**< Altura modificada */
	int base_wgt;	/**< Peso base */
	int mod_wgt;	/**< Peso modificado */

	int infra;		/**< Rango de infravisión */

	int body;		/**< Cuerpo de la raza */

	int r_adj[STAT_MAX];		/**< Bonificaciones de estadísticas */

	int r_skills[SKILL_MAX];	/**< Habilidades */

	bitflag flags[OF_SIZE];		/**< Banderas raciales (de objeto) */
	bitflag pflags[PF_SIZE];	/**< Banderas raciales (de jugador) */

	struct history_chart *history;

	struct element_info el_info[ELEM_MAX]; /**< Resistencias */
};

/**
 * Nombres de golpes para jugadores transformados
 */
struct player_blow {
	struct player_blow *next;
	char *name;
};

/**
 * Información de forma de cambio de forma del jugador
 */
struct player_shape {
	struct player_shape *next;
	const char *name;

	int sidx;

	int to_a;				/**< Bonificaciones a CA */
	int to_h;				/**< Bonificaciones para golpear */
	int to_d;				/**< Bonificaciones para daño */

	int skills[SKILL_MAX];  /**< Habilidades */
	bitflag flags[OF_SIZE];		/**< Banderas de forma (de objeto) */
	bitflag pflags[PF_SIZE];	/**< Banderas de forma (de jugador) */
	int modifiers[OBJ_MOD_MAX];	/**< Modificadores de estadísticas y otros */
	struct element_info el_info[ELEM_MAX]; /**< Resistencias */

	struct effect *effect;	/**< Efecto al tomar esta forma (effects.c) */

	struct player_blow *blows;
	int num_blows;
};

/**
 * Objetos con los que el jugador comienza. Usado en player_class y especificado en
 * class.txt.
 */
struct start_item {
	int tval;	/**< Tipo general de objeto (ver macros TV_) */
	int sval;	/**< Subtipo de objeto */
	int min;	/**< Cantidad mínima inicial */
	int max;	/**< Cantidad máxima inicial */
	int *eopts;     /**< Índices (array terminado en cero) para opciones de nacimiento que pueden excluir el objeto */
	struct start_item *next;
};

/**
 * Estructura para reinos mágicos
 */
struct magic_realm {
	struct magic_realm *next;
	char *code;
	char *name;
	int stat;
	char *verb;
	char *spell_noun;
	char *book_noun;
};

/**
 * Una estructura para contener información dependiente de la clase sobre hechizos.
 */
struct class_spell {
	char *name;
	char *text;

	struct effect *effect;	/**< El efecto del hechizo */
	const struct magic_realm *realm;	/**< El reino mágico de este hechizo */

	int sidx;				/**< El índice de este hechizo para esta clase */
	int bidx;				/**< El índice en el array de libros del jugador */
	int slevel;				/**< Nivel requerido (para aprender) */
	int smana;				/**< Maná requerido (para lanzar) */
	int sfail;				/**< Probabilidad base de fallo */
	int sexp;				/**< Bonificación de experiencia codificada */
};

/**
 * Una estructura para contener información dependiente de la clase sobre libros de hechizos.
 */
struct class_book {
	int tval;							/**< Tipo de objeto del libro */
	int sval;							/**< Subtipo de objeto del libro */
	bool dungeon;						/**< Si este es un libro de mazmorra */
	int num_spells;						/**< Número de hechizos en este libro */
	const struct magic_realm *realm;	/**< El reino mágico de este libro */
	struct class_spell *spells;			/**< Hechizos en el libro */
};

/**
 * Información sobre el conocimiento mágico de la clase
 */
struct class_magic {
	int spell_first;			/**< Nivel del primer hechizo */
	int spell_weight;			/**< Peso máximo de armadura para evitar penalizaciones de maná */
	int num_books;				/**< Número de libros de hechizos */
	struct class_book *books;	/**< Detalles de los libros de hechizos */
	int total_spells;			/**< Número de hechizos para esta clase */
};

/**
 * Información de clase del jugador
 */
struct player_class {
	struct player_class *next;
	const char *name;
	unsigned int cidx;

	const char *title[10];		/**< Títulos */

	int c_adj[STAT_MAX];		/**< Modificador de estadísticas */

	int c_skills[SKILL_MAX];	/**< Habilidades de clase */
	int x_skills[SKILL_MAX];	/**< Habilidades extra */

	int c_mhp;					/**< Ajuste de dados de vida */
	int c_exp;					/**< Factor de experiencia */

	bitflag flags[OF_SIZE];		/**< Banderas (de objeto) */
	bitflag pflags[PF_SIZE];	/**< Banderas (de jugador) */

	int max_attacks;			/**< Ataques máximos posibles */
	int min_weight;				/**< Peso mínimo del arma para cálculos */
	int att_multiply;			/**< Multiplicador para cálculos de ataque */

	struct start_item *start_items; /**< Inventario inicial */

	struct class_magic magic;	/**< Hechizos mágicos */
};

/**
 * Información para habilidades del jugador
 */
struct player_ability {
	struct player_ability *next;
	uint16_t index;			/* PF_*, OF_* o índice de elemento */
	char *type;			/* Tipo de habilidad */
	char *name;			/* Nombre de la habilidad */
	char *desc;			/* Descripción de la habilidad */
	int group;			/* Grupo de habilidad (establecido localmente al ver) */
	int value;			/* Valor de resistencia para elementos */
};

/**
 * Las historias son un grafo de tablas; cada tabla contiene un conjunto de entradas
 * individuales para esa tabla, y cada entrada contiene una descripción de texto y una
 * tabla sucesora para mover la generación de la historia.
 * Por ejemplo:
 * 	tabla 1 {
 * 		entrada {
 * 			desc "Eres el hijo ilegítimo y no reconocido";
 * 			next 2;
 * 		};
 * 		entrada {
 * 			desc "Eres el hijo ilegítimo pero reconocido";
 * 			next 2;
 * 		};
 * 		entrada {
 * 			desc "Eres uno de varios hijos";
 * 			next 3;
 * 		};
 * 	};
 *
 * La generación de la historia funciona recorriendo el grafo desde la tabla inicial para
 * cada raza, eligiendo una entrada aleatoria (con probabilidad ponderada) cada vez.
 */
struct history_entry {
	struct history_entry *next;
	struct history_chart *succ;
	int isucc;
	int roll;
	char *text;
};

struct history_chart {
	struct history_chart *next;
	struct history_entry *entries;
	unsigned int idx;
};

/**
 * Información del historial del jugador
 *
 * Ver player-history.c/.h
 */
struct player_history {
	struct history_info *entries;	/**< Lista de entradas */
	size_t next;					/**< Primera entrada no utilizada */
	size_t length;					/**< Longitud actual */
};

/**
 * Todo el estado variable que cambia cuando te pones/te quitas equipo.
 * Las banderas del jugador no son actualmente variables, pero son útiles aquí para que los
 * monstruos puedan aprenderlas.
 */
struct player_state {
	int stat_add[STAT_MAX];	/**< Bonificaciones de estadísticas del equipo */
	int stat_ind[STAT_MAX];	/**< Índices en las tablas de estadísticas */
	int stat_use[STAT_MAX];	/**< Estadísticas modificadas actuales */
	int stat_top[STAT_MAX];	/**< Estadísticas modificadas máximas */

	int skills[SKILL_MAX];		/**< Habilidades */

	int speed;			/**< Velocidad actual */

	int num_blows;		/**< Número de golpes x100 */
	int num_shots;		/**< Número de disparos x10 */
	int num_moves;		/**< Número de acciones de movimiento extra */

	int ammo_mult;		/**< Multiplicador de munición */
	int ammo_tval;		/**< Variedad de munición */

	int ac;				/**< CA base */
	int dam_red;		/**< Reducción de daño */
	int perc_dam_red;	/**< Porcentaje de reducción de daño */
	int to_a;			/**< Bonificación a CA */
	int to_h;			/**< Bonificación para golpear */
	int to_d;			/**< Bonificación para daño */

	int see_infra;		/**< Rango de infravisión */

	int cur_light;		/**< Radio de luz (si la hay) */

	bool heavy_wield;	/**< Arma pesada */
	bool heavy_shoot;	/**< Disparador pesado */
	bool bless_wield;	/**< Arma bendecida (o contundente) */

	bool cumber_armor;	/**< Armadura que drena maná */

	bitflag flags[OF_SIZE];					/**< Banderas de estado de raza y objetos */
	bitflag pflags[PF_SIZE];				/**< Banderas intrínsecas del jugador */
	struct element_info el_info[ELEM_MAX];	/**< Resistencias de raza y objetos */
};

#define player_has(p, flag)       (pf_has(p->state.pflags, (flag)))

/**
 * Variables temporales, derivadas y relacionadas con el jugador usadas durante el juego pero no guardadas
 *
 * XXX Algunas de estas probablemente deberían ir a la interfaz de usuario
 */
struct player_upkeep {
	bool playing;			/* Verdadero si el jugador está jugando */
	bool autosave;			/* Verdadero si el autoguardado está pendiente */
	bool generate_level;	/* Verdadero si el nivel necesita regenerarse */
	bool only_partial;		/* Verdadero si solo se necesitan actualizaciones parciales */
	bool dropping;			/* Verdadero si el autosoltar está en progreso */

	int energy_use;			/* Uso de energía este turno */
	int new_spells;			/* Número de hechizos disponibles */

	struct monster *health_who;			/* Objetivo de la barra de salud */
	struct monster_race *monster_race;	/* Objetivo de raza de monstruo */
	struct object *object;				/* Objetivo de objeto */
	struct object_kind *object_kind;	/* Objetivo de tipo de objeto */

	uint32_t notice;		/* Banderas de bits para acciones pendientes como
							 * reordenar inventario, ignorar, etc. */
	uint32_t update;		/* Banderas de bits para recálculos necesarios
							 * como PV, o área visible */
	uint32_t redraw;		/* Banderas de bits para cosas que /han/ cambiado,
							 * y solo necesitan ser redibujadas por la interfaz de usuario,
							 * como PV, Velocidad, etc. */

	int command_wrk;		/* Usado por la interfaz de usuario para decidir si
							 * comenzar mostrando equipo o
							 * listados de inventario al ofrecer
							 * una elección. Ver obj-ui.c */

	bool create_up_stair;	/* Crear escalera ascendente en el siguiente nivel */
	bool create_down_stair;	/* Crear escalera descendente en el siguiente nivel */
	bool light_level;		/* El nivel debe iluminarse al crearse */
	bool arena_level;		/* El nivel actual es una arena */

	int resting;			/* Contador de descanso */

	int running;			/* Contador de carrera */
	bool running_firststep;		/* ¿Es este nuestro primer paso corriendo o siguiendo una ruta precalculada? */

	struct object **quiver;	/* Objetos de la aljaba */
	struct object **inven;	/* Objetos del inventario */
	int total_weight;		/* Peso total que se lleva */
	int inven_cnt;			/* Número de objetos en el inventario */
	int equip_cnt;			/* Número de objetos en el equipo */
	int quiver_cnt;			/* Número de objetos en la aljaba */
	int recharge_pow;		/* Poder del efecto de recarga */
	int step_count;			/* Búsqueda de camino: número de pasos restantes */
	int16_t *steps;			/* Búsqueda de camino: pasos en orden inverso */
	struct loc path_dest;		/* Búsqueda de camino: casilla de destino */
};

/**
 * La mayor parte de la información del "jugador" va aquí.
 *
 * Esta estructura nos da una gran colección de variables del jugador.
 *
 * Esta estructura completa se borra cuando nace un nuevo personaje.
 *
 * Esta estructura está más o menos dispuesta de manera que la información
 * que debe guardarse en el archivo de guardado precede a toda la información
 * que puede ser recalculada según sea necesario.
 */
struct player {
	const struct player_race *race;
	const struct player_class *class;

	struct loc grid;	/* Ubicación del jugador */
	struct loc old_grid;/* Ubicación del jugador antes de irse a una arena */

	uint8_t hitdie;		/* Dados de vida (caras) */
	uint8_t expfact;	/* Factor de experiencia */

	int16_t age;		/* Edad del personaje */
	int16_t ht;		/* Altura */
	int16_t wt;		/* Peso */

	int32_t au;		/* Oro actual */

	int16_t max_depth;	/* Profundidad máxima */
	int16_t recall_depth;	/* Profundidad de retorno */
	int16_t depth;		/* Profundidad actual */

	int16_t max_lev;	/* Nivel máximo */
	int16_t lev;		/* Nivel actual */

	int32_t max_exp;	/* Experiencia máxima */
	int32_t exp;		/* Experiencia actual */
	uint16_t exp_frac;	/* Fracción de exp actual (veces 2^16) */

	int16_t mhp;		/* Puntos de vida máximos */
	int16_t chp;		/* Puntos de vida actuales */
	uint16_t chp_frac;	/* Fracción de PV actual (veces 2^16) */

	int16_t msp;		/* Puntos de maná máximos */
	int16_t csp;		/* Puntos de maná actuales */
	uint16_t csp_frac;	/* Fracción de maná actual (veces 2^16) */

	int16_t stat_max[STAT_MAX];	/* Valores estadísticos "máximos" actuales */
	int16_t stat_cur[STAT_MAX];	/* Valores estadísticos "naturales" actuales */
	int16_t stat_map[STAT_MAX];	/* Rastrea estadísticas reasignadas por intercambio temporal de estadísticas */

	int16_t *timed;				/* Efectos temporizados */

	int16_t word_recall;			/* Contador de Palabra de Retorno */
	int16_t deep_descent;			/* Contador de Descenso Profundo */

	int16_t energy;				/* Energía actual */
	uint32_t total_energy;			/* Energía total usada (incluyendo descanso) */
	uint32_t resting_turn;			/* Número de turnos de jugador pasados descansando */

	int16_t food;				/* Nutrición actual */

	uint8_t unignoring;			/* Dejando de ignorar */

	uint8_t skip_cmd_coercion;		/* Verdadero si la comprobación de sed de sangre
							debe omitirse en el siguiente comando
							(el comando anterior pasó con éxito
							la comprobación de sed de sangre
							pero luego fue cancelado
							por el usuario) */
	uint8_t *spell_flags;			/* Banderas de hechizo */
	uint8_t *spell_order;			/* Orden de hechizos */

	char full_name[PLAYER_NAME_LEN];	/* Nombre completo */
	char died_from[80];					/* Causa de la muerte */
	char *history;						/* Historial del jugador */
	struct quest *quests;				/* Historial de misiones */
	uint16_t total_winner;			/* Ganador total */

	uint16_t noscore;			/* Banderas de trampa */

	bool is_dead;				/* El jugador está muerto */

	bool wizard;				/* El jugador está en modo mago */

	int16_t player_hp[PY_MAX_LEVEL];	/* PV ganados por nivel */

	/* Valores guardados para inicio rápido */
	int32_t au_birth;			/* Oro de nacimiento cuando la opción birth_money es falsa */
	int16_t stat_birth[STAT_MAX];		/* Valores estadísticos "naturales" de nacimiento */
	int16_t ht_birth;			/* Altura de nacimiento */
	int16_t wt_birth;			/* Peso de nacimiento */

	struct player_options opts;			/* Opciones del jugador */
	struct player_history hist;			/* Historial del jugador (ver player-history.c) */

	struct player_body body;			/* Ranuras de equipo disponibles */
	struct player_shape *shape;			/* Forma actual del jugador */

	struct object *gear;				/* Equipo real */
	struct object *gear_k;				/* Equipo conocido */

	struct object *obj_k;				/* Conocimiento de objetos ("runas") */
	struct chunk *cave;					/* Versión conocida del nivel actual */

	struct player_state state;			/* Estado calculable */
	struct player_state known_state;	/* Lo que el jugador puede saber de lo anterior */
	struct player_upkeep *upkeep;		/* Valores temporales relacionados con el jugador */
};


/**
 * ------------------------------------------------------------------------
 * Externos
 * ------------------------------------------------------------------------ */

extern struct player_body *bodies;
extern struct player_race *races;
extern struct player_shape *shapes;
extern struct player_class *classes;
extern struct player_ability *player_abilities;
extern struct magic_realm *realms;

extern const int32_t player_exp[PY_MAX_LEVEL];
extern struct player *player;

/* player-class.c */
struct player_class *player_id2class(guid id);

/* player.c */
int stat_name_to_idx(const char *name);
const char *stat_idx_to_name(int type);
const struct magic_realm *lookup_realm(const char *code);
bool player_stat_inc(struct player *p, int stat);
bool player_stat_dec(struct player *p, int stat, bool permanent);
void player_exp_gain(struct player *p, int32_t amount);
void player_exp_lose(struct player *p, int32_t amount, bool permanent);
void player_flags(struct player *p, bitflag f[OF_SIZE]);
void player_flags_timed(struct player *p, bitflag f[OF_SIZE]);
uint8_t player_hp_attr(struct player *p);
uint8_t player_sp_attr(struct player *p);
bool player_restore_mana(struct player *p, int amt);
size_t player_random_name(char *buf, size_t buflen);
void player_safe_name(char *safe, size_t safelen, const char *name, bool strip_suffix);
void player_cleanup_members(struct player *p);

/* player-race.c */
struct player_race *player_id2race(guid id);

#endif /* !PLAYER_H */