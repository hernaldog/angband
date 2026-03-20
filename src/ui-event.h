/**
 * \archivo ui-event.h
 * \brief Funciones de utilidad relacionadas con eventos de la interfaz de usuario
 *
 * Copyright (c) 2011 Andi Sidwell
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
#ifndef INCLUDED_UI_EVENT_H
#define INCLUDED_UI_EVENT_H

/**
 * Los diversos eventos de interfaz de usuario que pueden ocurrir.
 */
typedef enum
{
	EVT_NONE	= 0x0000,

	/* Eventos básicos */
	EVT_KBRD	= 0x0001,	/* Pulsación de tecla */
	EVT_MOUSE	= 0x0002,	/* Pulsación del ratón */
	EVT_RESIZE	= 0x0004,	/* Cambio de tamaño de pantalla */

	EVT_BUTTON	= 0x0008,	/* Pulsación de botón */

	/* Eventos 'abstractos' */
	EVT_ESCAPE	= 0x0010,	/* Salir de este menú */
	EVT_MOVE	= 0x0020,	/* Movimiento del menú */
	EVT_SELECT	= 0x0040,	/* Selección del menú */
	EVT_SWITCH	= 0x0080	/* Cambio de menú */
} ui_event_type;


/**
 * Modificadores de tecla.
 */
#define KC_MOD_CONTROL  0x01
#define KC_MOD_SHIFT    0x02
#define KC_MOD_ALT      0x04
#define KC_MOD_META     0x08
#define KC_MOD_KEYPAD   0x10


/**
 * El juego asume que en ciertos casos, el efecto de una tecla modificadora
 * se codificará en el propio código de tecla (ej. 'A' es mayúsculas-'a'). En estos casos
 * (especificados a continuación), el valor 'mods' de una pulsación de tecla no debería codificarlos también.
 *
 * Si el carácter proviene del teclado numérico:
 *   Incluir todos los modificadores
 * De lo contrario, si el carácter está en el rango 0x01-0x1F, y la pulsación de tecla fue
 * de una tecla que sin modificadores estaría en el rango 0x40-0x5F o
 * 0x61-0x7A:
 *   CONTROL está codificado en el código de tecla y no debería estar en mods
 * De lo contrario, si el carácter está en el rango 0x21-0x2F, 0x3A-0x60 o 0x7B-0x7E:
 *   MAYÚSCULAS se usa a menudo para producir estos y no debería codificarse en mods
 *
 * (Todos los rangos son inclusivos.)
 *
 * Puedes usar estas macros para parte de las condiciones anteriores.
 */
#define MODS_INCLUDE_CONTROL(v) \
	(((v) >= 0x01 && (v) <= 0x1F) ? false : true)

#define MODS_INCLUDE_SHIFT(v) \
	((((v) >= 0x21 && (v) <= 0x2F) || \
			((v) >= 0x3A && (v) <= 0x60) || \
			((v) >= 0x7B && (v) <= 0x7E)) ? false : true)


/**
 * Si el código de tecla al que intentas aplicar control está entre 0x40-0x5F
 * inclusive o 0x61-0x7A inclusive, entonces debes aplicar AND bit a bit al código de tecla
 * con 0x1f y dejar KC_MOD_CONTROL sin establecer. De lo contrario, deja el código de tecla
 * sin cambios y establece KC_MOD_CONTROL en mods.
 *
 * Esta macro devuelve verdadero en el primer caso y falso en el segundo.
 */
#define ENCODE_KTRL(v) \
	((((v) >= 0x40 && (v) <= 0x5F) || ((v) >= 0x61 && (v) <= 0x7A)) ? true : false)


/**
 * Dado un carácter X, lo convierte en un carácter de control.
 */
#define KTRL(X) \
	((X) & 0x1F)


/**
 * Dado un carácter de control X, lo convierte en su equivalente ASCII en minúscula
 * a menos que sea 0x00 o 0x1B a 0x1F, entonces usa los caracteres de puntuación
 * que flanquean las letras mayúsculas ASCII. La representación en minúscula es
 * preferida porque:
 *   1) Algunos front-ends pueden distinguir entre ctrl-letra_minúscula y
 *      ctrl-letra_mayúscula, pero otros no (GCU, por ejemplo).
 *   2) La búsqueda de comandos actual solo mira el código de tecla y no si
 *      hay algún modificador establecido. Entonces, ctrl-letra_minúscula y
 *      ctrl-letra_mayúscula para invocar un comando integrado tienen el mismo efecto
 *      en la mayoría de los casos porque el mismo código de tecla se pasa al núcleo y, con
 *      los front-ends que establecen el modificador de mayúsculas para ctrl-letra_mayúscula,
 *      ese modificador se ignora.
 *   3) Un puñado de plataformas no responden al menos a algunas instancias de
 *      ctrl-letra_mayúscula. Las conocidas son el front-end GCU ejecutándose
 *      en Cygwin y el front-end GCU ejecutándose en la Terminal de Apple para macOS.
 *      En este último caso, la pulsación de tecla para ctrl-O nunca llega al front-end.
 *      En Cygwin, no sé cuál es la causa del problema.
 * Los caracteres de puntuación que flanquean las letras mayúsculas son preferidos
 * porque eso es lo que se usaba en el pasado y, en muchos teclados, no es
 * cierto que mayúsculas + una tecla que da el código de tecla 0x60 o 0x7B a 0x7F resulte
 * en un código de tecla que es 0x40 o 0x5B a 0x5F.
 */
#define UN_KTRL(X) \
	(((X) < 0x01 || (X) > 0x1B) ? (X) + 64 : (X) + 96)


/**
 * Dado un carácter de control X, lo convierte en su equivalente ASCII en mayúscula.
 * Prefiere usar UN_KTRL() sobre esto excepto para pruebas de inscripción y atajos
 * de menú donde hay conflictos para el conjunto de teclas roguelike (UN_KTRL()
 * para el comando de ignorar roguelike, '^d', da 'd' que entra en conflicto con el
 * comando de soltar).
 */
#define UN_KTRL_CAP(X) \
	((X) + 64)

/**
 * Mapeos de conjuntos de teclas para varias teclas.
 */
#define ARROW_DOWN    0x80
#define ARROW_LEFT    0x81
#define ARROW_RIGHT   0x82
#define ARROW_UP      0x83

#define KC_F1         0x84
#define KC_F2         0x85
#define KC_F3         0x86
#define KC_F4         0x87
#define KC_F5         0x88
#define KC_F6         0x89
#define KC_F7         0x8A
#define KC_F8         0x8B
#define KC_F9         0x8C
#define KC_F10        0x8D
#define KC_F11        0x8E
#define KC_F12        0x8F
#define KC_F13        0x90
#define KC_F14        0x91
#define KC_F15        0x92

#define KC_HELP       0x93
#define KC_HOME       0x94
#define KC_PGUP       0x95
#define KC_END        0x96
#define KC_PGDOWN     0x97
#define KC_INSERT     0x98
#define KC_PAUSE      0x99
#define KC_BREAK      0x9a
#define KC_BEGIN      0x9b
#define KC_ENTER      0x9c /* ASCII \r */
#define KC_TAB        0x9d /* ASCII \t */
#define KC_DELETE     0x9e
#define KC_BACKSPACE  0x9f /* ASCII \h */
#define ESCAPE        0xE000

/* tenemos hasta 0x9F antes de empezar a entrar en Unicode mostrable */
/* luego podríamos pasar al área de uso privado 1, 0xE000 en adelante */

/**
 * Análogo a isdigit() etc en ctypes
 */
#define isarrow(c)  ((c >= ARROW_DOWN) && (c <= ARROW_UP))


/**
 * Tipo capaz de contener cualquier tecla de entrada que podamos usar.
 */
typedef uint32_t keycode_t;


/**
 * Estructura que contiene toda la información relevante para las pulsaciones de tecla.
 */
struct keypress {
	ui_event_type type;
	keycode_t code;
	uint8_t mods;
};

/**
 * Constante de pulsación de tecla nula, para inicialización segura.
 */
static struct keypress const KEYPRESS_NULL = {
	.type = EVT_NONE,
	.code = 0,
	.mods = 0
};

/**
 * Estructura que contiene toda la información relevante para los clics del ratón.
 */
struct mouseclick {
	ui_event_type type;
	uint8_t x;
	uint8_t y;
	uint8_t button;
	uint8_t mods;
};

/**
 * Tipo unión para contener información sobre cualquier evento dado.
 */
typedef union {
	ui_event_type type;
	struct mouseclick mouse;
	struct keypress key;
} ui_event;

/**
 * Forma fácil de inicializar un ui_event sin ver los detalles internos.
 */
#define EVENT_EMPTY		{ 0 }


/*** Funciones ***/

/**
 * Dada una cadena (y la longitud de esa cadena), devuelve el código de tecla correspondiente
 */
keycode_t keycode_find_code(const char *str, size_t len);

/**
 * Dado un código de tecla, devuelve su descripción
 */
const char *keycode_find_desc(keycode_t kc);

/**
 * Dado un código de tecla, devuelve si corresponde a un carácter imprimible.
 */
bool keycode_isprint(keycode_t kc);

/**
 * Convierte una cadena de pulsaciones de tecla en su representación textual
 */
void keypress_to_text(char *buf, size_t len, const struct keypress *src,
	bool expand_backslash);

/**
 * Convierte una representación textual de pulsaciones de tecla en pulsaciones de tecla reales
 */
void keypress_from_text(struct keypress *buf, size_t len, const char *str);

/**
 * Convierte una pulsación de tecla en algo que el usuario pueda leer (no diseñado para usarse
 * internamente)
 */
void keypress_to_readable(char *buf, size_t len, struct keypress src);


extern bool char_matches_key(wchar_t c, keycode_t key);

bool event_is_key(ui_event e, keycode_t key);

bool event_is_mouse(ui_event e, uint8_t button);

bool event_is_mouse_m(ui_event e, uint8_t button, uint8_t mods);


#endif /* INCLUDED_UI_EVENT_H */