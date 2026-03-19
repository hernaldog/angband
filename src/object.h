/**
 * \file object.h
 * \brief estructuras básicas y enumeraciones de objetos
 */
#ifndef INCLUDED_OBJECT_H
#define INCLUDED_OBJECT_H

#include "z-type.h"
#include "z-quark.h"
#include "z-bitflag.h"
#include "z-dice.h"
#include "obj-properties.h"


/*** Constantes del juego ***/

/**
 * Elementos
 */
enum
{
	#define ELEM(a) ELEM_##a,
	#include "list-elements.h"
	#undef ELEM

	ELEM_MAX
};

#define ELEM_BASE_MIN  ELEM_ACID
#define ELEM_BASE_MAX  (ELEM_COLD + 1)
#define ELEM_HIGH_MIN  ELEM_POIS
#define ELEM_HIGH_MAX  (ELEM_DISEN + 1)

/**
 * Tipos de origen de objeto
 */

enum {
	#define ORIGIN(a, b, c) ORIGIN_##a,
	#include "list-origins.h"
	#undef ORIGIN

	ORIGIN_MAX
};


/*** Estructuras ***/

/**
 * Efecto
 */
struct effect {
	struct effect *next;
	uint16_t index;	/**< El índice del efecto */
	dice_t *dice;	/**< Expresión de dados usada en el efecto */
	int y;			/**< Coordenada Y o distancia */
	int x;			/**< Coordenada X o distancia */
	int subtype;	/**< Tipo de proyección, tipo de efecto temporizado, etc. */
	int radius;		/**< Radio del efecto (si tiene) */
	int other;		/**< Parámetro extra para pasar al manejador */
	char *msg;		/**< Mensaje para muerte o lo que sea */
};

/**
 * Cofres
 */
struct chest_trap {
	struct chest_trap *next;
	char *name;
	char *code;
	int level;
	struct effect *effect;
	int pval;
	bool destroy;
	bool magic;
	char *msg;
	char *msg_death;
};

/**
 * Tipo de marca elemental
 */
struct brand {
	char *code;
	char *name;
	char *verb;
	int resist_flag;
	int vuln_flag;
	int multiplier;
	int o_multiplier;
	int power;
	struct brand *next;
};

/**
 * Tipo de matanza
 */
struct slay {
	char *code;
	char *name;
	char *base;
	char *melee_verb;
	char *range_verb;
	int race_flag;
	int multiplier;
	int o_multiplier;
	int power;
	struct slay *next;
};

/**
 * Tipo de maldición
 */
struct curse {
	struct curse *next;
	char *name;
	bool *poss;
	struct object *obj;
	char *conflict;
	bitflag conflict_flags[OF_SIZE];
	char *desc;
};

enum {
	EL_INFO_HATES = 0x01,
	EL_INFO_IGNORE = 0x02,
	EL_INFO_RANDOM = 0x04,
};

/**
 * Tipo de información de elemento
 */
struct element_info {
	int16_t res_level;
	bitflag flags;
};

/**
 * Estructura de activación
 */
struct activation {
	struct activation *next;
	char *name;
	int index;
	bool aim;
	int level;
	int power;
	struct effect *effect;
	char *message;
	char *desc;
};

extern struct activation *activations;

/**
 * Información sobre tipos de objeto, como varas, varitas, etc.
 */
struct object_base {
	char *name;

	int tval;
	struct object_base *next;

	int attr;

	bitflag flags[OF_SIZE];
	bitflag kind_flags[KF_SIZE];			/**< Banderas de tipo */
	struct element_info el_info[ELEM_MAX];

	int break_perc;
	int max_stack;
	int num_svals;
};

extern struct object_base *kb_info;

/**
 * Información sobre tipos de objetos, incluyendo el conocimiento del jugador.
 *
 * TODO: separar las partes modificables por el usuario en una estructura aparte para que
 * esta pueda ser de solo lectura.
 */
struct object_kind {
	char *name;
	char *text;

	struct object_base *base;

	struct object_kind *next;
	uint32_t kidx;

	int tval;					/**< Tipo general de objeto (ver macros TV_) */
	int sval;					/**< Subtipo de objeto */

	random_value pval;			/* Parámetro extra del objeto */

	random_value to_h;			/**< Bonificación para golpear */
	random_value to_d;			/**< Bonificación para daño */
	random_value to_a;			/**< Bonificación para armadura */
	int ac;					/**< Armadura base */

	int dd;					/**< Dados de daño */
	int ds;					/**< Caras de los dados */
	int weight;				/**< Peso, en 1/10 libras */

	int cost;					/**< Coste base del objeto */

	bitflag flags[OF_SIZE];					/**< Banderas */
	bitflag kind_flags[KF_SIZE];			/**< Banderas de tipo */

	random_value modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;
	int *curses;			/**< Array de poderes de maldición */

	uint8_t d_attr;			/**< Atributo de objeto predeterminado */
	wchar_t d_char;			/**< Carácter de objeto predeterminado */

	int alloc_prob;			/**< Asignación: frecuencia */
	int alloc_min;			/**< Nivel de mazmorra normal más alto */
	int alloc_max;			/**< Nivel de mazmorra normal más bajo */
	int level;				/**< Nivel (dificultad de activación) */

	struct activation *activation;	/**< Activación similar a artefacto */
	struct effect *effect;	/**< Efecto que produce este objeto (effects.c) */
	int power;				/**< Poder del efecto del objeto */
	char *effect_msg;
	char *vis_msg;
	random_value time;		/**< Tiempo de recarga (varas/activación) */
	random_value charge;	/**< Número de cargas (bastones/varitas) */

	int gen_mult_prob;		/**< Probabilidad de generar más de uno */
	random_value stack_size;/**< Número a generar */

	struct flavor *flavor;	/**< Sabor especial del objeto (o cero) */

	/** También guardado en el archivo de guardado **/

	quark_t note_aware; 	/**< Número quark de autoinscripción */
	quark_t note_unaware; 	/**< Número quark de autoinscripción */

	bool aware;		/**< Verdadero si el jugador conoce los efectos del tipo */
	bool tried;		/**< Verdadero si se ha probado el tipo */

	uint8_t ignore;  	/**< Configuración de ignorar */
	bool everseen; 	/**< El tipo ha sido visto (para no estropear los menús de ignorar) */
};

extern struct object_kind *k_info;
extern struct object_kind *unknown_item_kind;
extern struct object_kind *unknown_gold_kind;
extern struct object_kind *pile_kind;
extern struct object_kind *curse_object_kind;

/**
 * Información inmutable sobre artefactos.
 */
struct artifact {
	char *name;
	char *text;

	uint32_t aidx;

	struct artifact *next;

	int tval;		/**< Tipo general de artefacto (ver macros TV_) */
	int sval;		/**< Subtipo de artefacto */

	int to_h;		/**< Bonificación para golpear */
	int to_d;		/**< Bonificación para daño */
	int to_a;		/**< Bonificación para armadura */
	int ac;		/**< Armadura base */

	int dd;		/**< Dados de daño base */
	int ds;		/**< Caras de los dados base */

	int weight;	/**< Peso en 1/10 libras */

	int cost;		/**< Valor (pseudo) del artefacto */

	bitflag flags[OF_SIZE];			/**< Banderas */

	int modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;
	int *curses;		/**< Array de poderes de maldición */

	int level;			/** Nivel de dificultad para la activación */

	int alloc_prob;		/** Probabilidad de ser generado (ej. rareza) */
	int alloc_min;		/** Profundidad mínima (puede aparecer antes) */
	int alloc_max;		/** Profundidad máxima (NUNCA aparecerá más profundo) */

	struct activation *activation;	/**< Activación del artefacto */
	char *alt_msg;

	random_value time;	/**< Tiempo de recarga (si corresponde) */
};

/**
 * Información sobre artefactos que cambia durante el juego;
 * excepto aidx, se guarda en el archivo de guardado
 */
struct artifact_upkeep {
	uint32_t aidx;	/**< Para índice cruzado con struct artifact */
	bool created;	/**< Si este artefacto ha sido creado */
	bool seen;	/**< Si este artefacto ha sido visto en esta partida */
	bool everseen;	/**< Si este artefacto ha sido visto alguna vez */
};

/**
 * Los arrays de artefactos
 */
extern struct artifact *a_info;
extern struct artifact_upkeep *aup_info;


/**
 * Estructura para posibles tipos de objeto para un objeto de ego
 */
struct poss_item {
	uint32_t kidx;
	struct poss_item *next;
};

/**
 * Información sobre objetos-ego.
 */
struct ego_item {
	struct ego_item *next;

	char *name;
	char *text;

	uint32_t eidx;

	int cost;						/* "Coste" del objeto-ego */

	bitflag flags[OF_SIZE];			/**< Banderas */
	bitflag flags_off[OF_SIZE];		/**< Banderas a eliminar */
	bitflag kind_flags[KF_SIZE];	/**< Banderas de tipo */

	random_value modifiers[OBJ_MOD_MAX];
	int min_modifiers[OBJ_MOD_MAX];
	struct element_info el_info[ELEM_MAX];

	bool *brands;
	bool *slays;
	int *curses;			/**< Array de poderes de maldición */

	int rating;			/* Aumento de nivel de valoración */
	int alloc_prob; 		/** Probabilidad de ser generado (ej. rareza) */
	int alloc_min;			/** Profundidad mínima (puede aparecer antes) */
	int alloc_max;			/** Profundidad máxima (NUNCA aparecerá más profundo) */

	struct poss_item *poss_items;

	random_value to_h;		/* Bonificación extra para golpear */
	random_value to_d;		/* Bonificación extra para daño */
	random_value to_a;		/* Bonificación extra para CA */

	int min_to_h;			/* Valor mínimo para golpear */
	int min_to_d;			/* Valor mínimo para daño */
	int min_to_a;			/* Valor mínimo para CA */

	struct activation *activation;	/**< Activación */
	random_value time;		/**< Tiempo de recarga para la activación */

	bool everseen;			/* No estropear los menús de ignorar */
};

/*
 * Los arrays de objetos-ego
 */
extern struct ego_item *e_info;

/**
 * Banderas para el campo obj->notice
 */
enum {
	OBJ_NOTICE_WORN = 0x01,
	OBJ_NOTICE_ASSESSED = 0x02,
	OBJ_NOTICE_IGNORE = 0x04,
	OBJ_NOTICE_IMAGINED = 0x08,
};

struct curse_data {
	int power;
	int timeout;
};

/**
 * Información de objeto, para un objeto específico.
 *
 * Nótese que las inscripciones ahora se manejan mediante la función "quark_str()"
 * aplicada al campo "note", que devolverá NULL si "note" es cero.
 *
 * Cada casilla de la cueva apunta a uno (o cero) objetos a través del campo "obj" en
 * su estructura "squares". Cada objeto entonces apunta a uno (o cero) objetos
 * a través del campo "next", y (aparte del primero) hacia atrás a través de su campo "prev",
 * formando una lista doblemente enlazada, que en términos del juego representa una
 * pila de objetos en la misma casilla.
 *
 * Cada monstruo apunta a uno (o cero) objetos a través del campo "held_obj"
 * (ver monster.h). Cada objeto entonces apunta a uno (o cero) objetos
 * y hacia atrás a objetos anteriores por sus propios campos "next" y "prev",
 * formando una lista doblemente enlazada, que en términos del juego representa el
 * inventario del monstruo.
 *
 * El campo "held_m_idx" se usa para indicar qué monstruo, si hay alguno,
 * está sosteniendo el objeto. Los objetos que están siendo sostenidos tienen (0, 0) como casilla.
 *
 * Nótese que los registros de objetos ahora no se copian, sino que se asignan al
 * crear el objeto y se liberan al destruirlo. Estos registros se pasan
 * entre los inventarios del jugador y los monstruos y el suelo con bastante
 * regularidad, y se debe tener cuidado al manejar dichos objetos.
 */
struct object {
	struct object_kind *kind;	/**< Tipo del objeto */
	struct ego_item *ego;		/**< Información de objeto-ego del objeto, si la hay */
	const struct artifact *artifact;	/**< Información de artefacto del objeto, si la hay */

	struct object *prev;	/**< Objeto anterior en una pila */
	struct object *next;	/**< Objeto siguiente en una pila */
	struct object *known;	/**< Versión conocida de este objeto */

	uint16_t oidx;		/**< Índice de la lista de objetos, si lo hay */

	struct loc grid;	/**< posición en el mapa, o (0, 0) */

	uint8_t tval;		/**< Tipo de objeto (del tipo) */
	uint8_t sval;		/**< Subtipo de objeto (del tipo) */

	int16_t pval;		/**< Parámetro extra del objeto */

	int16_t weight;		/**< Peso del objeto */

	uint8_t dd;		/**< Número de dados de daño */
	uint8_t ds;		/**< Número de caras en cada dado de daño */
	int16_t ac;		/**< CA normal */
	int16_t to_a;		/**< Bonificaciones a la CA */
	int16_t to_h;		/**< Bonificaciones para golpear */
	int16_t to_d;		/**< Bonificaciones para daño */

	bitflag flags[OF_SIZE];	/**< Banderas del objeto */
	int16_t modifiers[OBJ_MOD_MAX];	/**< Modificadores del objeto*/
	struct element_info el_info[ELEM_MAX];	/**< Información elemental del objeto */
	bool *brands;			/**< Bandera de ausencia/presencia de cada marca elemental */
	bool *slays;			/**< Bandera de ausencia/presencia de cada matanza */
	struct curse_data *curses;	/**< Array de poderes de maldición y tiempos de espera */

	struct effect *effect;	/**< Efecto que produce este objeto (effects.c) */
	char *effect_msg;		/**< Mensaje al usar */
	struct activation *activation;	/**< Activación de artefacto, si corresponde */
	random_value time;		/**< Tiempo de recarga (varas/activación) */
	int16_t timeout;		/**< Contador de tiempo de espera */

	uint8_t number;			/**< Número de objetos */
	bitflag notice;			/**< Atención prestada al objeto */

	int16_t held_m_idx;		/**< Monstruo que nos sostiene (si lo hay) */
	int16_t mimicking_m_idx;	/**< Monstruo que nos imita (si lo hay) */

	uint8_t origin;			/**< Cómo se encontró este objeto */
	uint8_t origin_depth;		/**< A qué profundidad se encontró el objeto */
	const struct monster_race *origin_race;	/**< Raza de monstruo que lo soltó */

	quark_t note; 			/**< Índice de inscripción */
};

/**
 * Constante de objeto nulo, para inicialización segura.
 */
static struct object const OBJECT_NULL = {
	.kind = NULL,
	.ego = NULL,
	.artifact = NULL,
	.prev = NULL,
	.next = NULL,
	.known = NULL,
	.oidx = 0,
	.grid = { 0, 0 },
	.tval = 0,
	.sval = 0,
	.pval = 0,
	.weight = 0,
	.dd = 0,
	.ds = 0,
	.ac = 0,
	.to_a = 0,
	.to_h = 0,
	.to_d = 0,
	.flags = { 0 },
	.modifiers = { 0 },
	.el_info = { { 0, 0 } },
	.brands = NULL,
	.slays = NULL,
	.curses = NULL,
	.effect = NULL,
	.effect_msg = NULL,
	.activation = NULL,
	.time = { 0, 0, 0, 0 },
	.timeout = 0,
	.number = 0,
	.notice = 0,
	.held_m_idx = 0,
	.mimicking_m_idx = 0,
	.origin = 0,
	.origin_depth = 0,
	.origin_race = NULL,
	.note = 0,
};

struct flavor
{
	char *text;
	struct flavor *next;
	unsigned int fidx;

	uint8_t tval;	/* Tipo de objeto asociado */
	uint8_t sval;	/* Subtipo de objeto asociado */

	uint8_t d_attr;	/* Atributo de sabor predeterminado */
	wchar_t d_char;	/* Carácter de sabor predeterminado */
};

extern struct flavor *flavors;


typedef bool (*item_tester)(const struct object *);


#endif /* !INCLUDED_OBJECT_H */