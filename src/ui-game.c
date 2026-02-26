/**
 * \file ui-game.c
 * \brief Gestión del juego para la interfaz de texto tradicional
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#include "angband.h"
#include "cmds.h"
#include "datafile.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "grafmode.h"
#include "init.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "obj-knowledge.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-path.h"
#include "player-properties.h"
#include "player-util.h"
#include "savefile.h"
#include "target.h"
#include "ui-birth.h"
#include "ui-command.h"
#include "ui-context.h"
#include "ui-death.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-help.h"
#include "ui-init.h"
#include "ui-input.h"
#include "ui-keymap.h"
#include "ui-knowledge.h"
#include "ui-map.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-spell.h"
#include "ui-score.h"
#include "ui-signals.h"
#include "ui-spoil.h"
#include "ui-store.h"
#include "ui-target.h"
#include "ui-wizard.h"
#include "z-file.h"
#include "z-util.h"
#include "z-virt.h"


struct savefile_getter_impl {
	ang_dir *d;
	struct savefile_details details;
#ifdef SETGID
	char uid_c[10];
#endif
	bool have_details;
	bool have_savedir;
};


bool arg_wizard;			/* Argumento de línea de comandos -- Solicitar modo mago */

#ifdef ALLOW_BORG
bool screensaver = false;
#endif /* ALLOW_BORG */

/**
 * Búfer para contener el nombre del archivo guardado actual
 */
char savefile[1024];

/**
 * Búfer para contener el nombre del guardado de pánico correspondiente a savefile.
 * Solo se establece y usa según sea necesario (en start_game() y handle_signal_abort()).
 * Usar almacenamiento estático para evitar complicaciones en el manejador de señales
 * (ej. espacio de pila limitado o la posibilidad de un montón lleno o corrupto).
 */
char panicfile[1024];

/**
 * Establecido por el frontend para realizar acciones necesarias al reiniciar después de la muerte
 * sin salir. Puede ser NULL.
 */
void (*reinit_hook)(void) = NULL;


/**
 * Aquí hay listas de comandos, almacenadas en este formato para que puedan ser
 * manipuladas fácilmente para, ej., pantallas de ayuda, o si un puerto quiere proporcionar un
 * menú nativo que contenga una lista de comandos.
 *
 * Considerar un diseño de dos paneles para los menús de comandos. XXX
 */

/**
 * Comandos de objetos
 */
struct cmd_info cmd_item[] =
{
	{ "Inscribir un objeto", { '{' }, CMD_INSCRIBE, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Desinscribir un objeto", { '}' }, CMD_UNINSCRIBE, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Usar/empuñar un objeto", { 'w' }, CMD_WIELD, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Quitar/soltar un objeto", { 't', 'T'}, CMD_TAKEOFF, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Examinar un objeto", { 'I' }, CMD_NULL, textui_obj_examine, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Soltar un objeto", { 'd' }, CMD_DROP, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Disparar tu arma de proyectiles", { 'f', 't' }, CMD_FIRE, NULL, player_can_fire_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Usar un báculo", { 'u', 'Z' }, CMD_USE_STAFF, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Apuntar una varita", {'a', 'z'}, CMD_USE_WAND, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Activar una vara", {'z', 'a'}, CMD_USE_ROD, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Activar un objeto", {'A' }, CMD_ACTIVATE, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Comer algo", { 'E' }, CMD_EAT, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Beber una poción", { 'q' }, CMD_QUAFF, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Leer un pergamino", { 'r' }, CMD_READ_SCROLL, NULL, player_can_read_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Recargar tu fuente de luz", { 'F' }, CMD_REFILL, NULL, player_can_refuel_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Usar un objeto", { 'U', 'X' }, CMD_USE, NULL, NULL, 0, NULL, NULL, NULL, 0 }
};

/**
 * Acciones generales
 */
struct cmd_info cmd_action[] =
{
	{ "Desarmar una trampa o cofre", { 'D' }, CMD_DISARM, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Descansar un rato", { 'R' }, CMD_NULL, textui_cmd_rest, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mirar alrededor", { 'l', 'x' }, CMD_NULL, do_cmd_look, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Apuntar a monstruo o ubicación", { '*' }, CMD_NULL, textui_target, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Apuntar al monstruo más cercano", { '\'' }, CMD_NULL, textui_target_closest, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Excavar un túnel", { 'T', KTRL('T') }, CMD_TUNNEL, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Subir escaleras", {'<' }, CMD_GO_UP, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Bajar escaleras", { '>' }, CMD_GO_DOWN, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Abrir una puerta o cofre", { 'o' }, CMD_OPEN, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Cerrar una puerta", { 'c' }, CMD_CLOSE, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Disparar al objetivo más cercano", { 'h', KC_TAB }, CMD_NULL, do_cmd_fire_at_nearest, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Lanzar un objeto", { 'v' }, CMD_THROW, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Caminar hacia una trampa", { 'W', '-' }, CMD_JUMP, NULL, NULL, 0, NULL, NULL, NULL, 0 },
};

/**
 * Comandos de gestión de objetos
 */
struct cmd_info cmd_item_manage[] =
{
	{ "Mostrar lista de equipo", { 'e' }, CMD_NULL, do_cmd_equip, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar lista de inventario", { 'i' }, CMD_NULL, do_cmd_inven, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar lista de carcaj", { '|' }, CMD_NULL, do_cmd_quiver, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Recoger objetos", { 'g' }, CMD_PICKUP, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Ignorar un objeto", { 'k', KTRL('D') }, CMD_IGNORE, textui_cmd_ignore, NULL, 0, NULL, NULL, NULL, 0 },
};

/**
 * Comandos de acceso a información
 */
struct cmd_info cmd_info[] =
{
	{ "Examinar un libro", { 'b', 'P' }, CMD_BROWSE_SPELL, textui_spell_browse, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Aprender nuevos hechizos", { 'G' }, CMD_STUDY, NULL, player_can_study_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Ver habilidades", { 'S' }, CMD_NULL, do_cmd_abilities, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Lanzar un hechizo", { 'm' }, CMD_CAST, NULL, player_can_cast_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Mapa completo de la mazmorra", { 'M' }, CMD_NULL, do_cmd_view_map, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Alternar ignorado de objetos", { 'K', 'O' }, CMD_NULL, textui_cmd_toggle_ignore, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar lista de objetos visibles", { ']' }, CMD_NULL, do_cmd_itemlist, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar lista de monstruos visibles", { '[' }, CMD_NULL, do_cmd_monlist, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Localizar jugador en el mapa", { 'L', 'W' }, CMD_NULL, do_cmd_locate, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Ayuda", { '?' }, CMD_NULL, do_cmd_help, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Identificar símbolo", { '/' }, CMD_NULL, do_cmd_query_symbol, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Descripción del personaje", { 'C' }, CMD_NULL, do_cmd_change_name, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Consultar conocimiento", { '~' }, CMD_NULL, textui_browse_knowledge, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Repetir sensación de nivel", { KTRL('F') }, CMD_NULL, do_cmd_feeling, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar mensaje anterior", { KTRL('O') }, CMD_NULL, do_cmd_message_one, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Mostrar mensajes anteriores", { KTRL('P') }, CMD_NULL, do_cmd_messages, NULL, 0, NULL, NULL, NULL, 0 }
};

/**
 * Comandos de utilidad/varios
 */
struct cmd_info cmd_util[] =
{
	{ "Interactuar con opciones", { '=' }, CMD_NULL, do_cmd_xxx_options, NULL, 0, NULL, NULL, NULL, 0 },

	{ "Guardar y no salir", { KTRL('S') }, CMD_NULL, save_game, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Guardar y salir", { KTRL('X') }, CMD_NULL, textui_quit, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Retirar personaje y salir", { 'Q' }, CMD_NULL, textui_cmd_retire, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Redibujar la pantalla", { KTRL('R') }, CMD_NULL, do_cmd_redraw, NULL, 0, NULL, NULL, NULL, 0 },

	{ "Guardar \"captura de pantalla\"", { ')' }, CMD_NULL, do_cmd_save_screen, NULL, 0, NULL, NULL, NULL, 0 }
};

/**
 * Comandos que no deben mostrarse al usuario
 */
struct cmd_info cmd_hidden[] =
{
	{ "Tomar notas", { ':' }, CMD_NULL, do_cmd_note, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Información de versión", { 'V' }, CMD_NULL, do_cmd_version, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Cargar una línea de preferencias", { '"' }, CMD_NULL, do_cmd_pref, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Alternar ventanas", { KTRL('E') }, CMD_NULL, toggle_inven_equip, NULL, 0, NULL, NULL, NULL, 0 }, /* XXX */
	{ "Alterar una casilla", { '+' }, CMD_ALTER, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Robar de un monstruo", { 's' }, CMD_STEAL, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Caminar", { ';' }, CMD_WALK, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Empezar a correr", { '.', ',' }, CMD_RUN, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Empezar a explorar", { 'p' }, CMD_EXPLORE, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Quedarse quieto", { ',', '.' }, CMD_HOLD, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Centrar mapa", { KTRL('L'), '@' }, CMD_NULL, do_cmd_center_map, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Alternar modo mago", { KTRL('W') }, CMD_NULL, do_cmd_wizard, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Repetir comando anterior", { 'n', KTRL('V') }, CMD_REPEAT, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Hacer recogida automática", { KTRL('G') }, CMD_AUTOPICKUP, NULL, NULL, 0, NULL, NULL, NULL, 0 },
	{ "Comandos de modo depuración", { KTRL('A') }, CMD_NULL, NULL, NULL, 1, "Comando de Depuración: ", "Ese no es un comando de depuración válido.", "Depuración", -1 },
#ifdef ALLOW_BORG
	{ "Comandos Borg", { KTRL('Z') }, CMD_NULL, do_cmd_try_borg, NULL, 0, NULL, NULL, NULL, 0 },
#endif
};

/**
 * Categorías de comandos del modo depuración; marcadores de posición para el sistema de menú Enter
 */
struct cmd_info cmd_debug[] =
{
	{ "Objetos", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgObj", -1 },
	{ "Jugador", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgPlayer", -1 },
	{ "Teletransporte", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgTele", -1 },
	{ "Efectos", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgEffects", -1 },
	{ "Invocar", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgSummon", -1 },
	{ "Archivos", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgFiles", -1 },
	{ "Estadísticas", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgStat", -1 },
	{ "Consulta", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgQuery", -1 },
	{ "Miscelánea", { '\0' }, CMD_NULL, NULL, NULL, 0, NULL, NULL, "DbgMisc", -1 },
};

struct cmd_info cmd_debug_obj[] =
{
	{ "Crear un objeto", { 'c' }, CMD_NULL, wiz_create_nonartifact, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Crear un artefacto", { 'C' }, CMD_NULL, wiz_create_artifact, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Crear todos del tval", { 'V' }, CMD_NULL, wiz_create_all_for_tval, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Adquirir bueno", { 'g' }, CMD_NULL, wiz_acquire_good, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Adquirir excelente", { 'v' }, CMD_NULL, wiz_acquire_great, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Jugar con objeto", { 'o' }, CMD_WIZ_PLAY_ITEM, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_player[] =
{
	{ "Curar todo", { 'a' }, CMD_WIZ_CURE_ALL, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Hacer poderoso", { 'A' }, CMD_WIZ_ADVANCE, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Aumentar experiencia", { 'x' }, CMD_WIZ_INCREASE_EXP, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Reevaluar puntos de golpe", { 'h' }, CMD_WIZ_RERATE, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Editar jugador", { 'e' }, CMD_WIZ_EDIT_PLAYER_START, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Aprender tipos de objetos", { 'l' }, CMD_NULL, wiz_learn_all_object_kinds, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Recordar monstruo", { 'r' }, CMD_WIZ_RECALL_MONSTER, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Borrar recuerdo de monstruo", { 'W' }, CMD_WIZ_WIPE_RECALL, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_tele[] =
{
	{ "A ubicación", { 'b' }, CMD_WIZ_TELEPORT_TO, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Aleatorio cercano", { 'p' }, CMD_NULL, wiz_phase_door, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Aleatorio lejano", { 't' }, CMD_NULL, wiz_teleport, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Saltar a un nivel", { 'j' }, CMD_WIZ_JUMP_LEVEL, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_effects[] =
{
	{ "Detectar todo cercano", { 'd' }, CMD_WIZ_DETECT_ALL_LOCAL, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Detectar todos los monstruos", { 'u' }, CMD_WIZ_DETECT_ALL_MONSTERS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Mapa del área local", { 'm' }, CMD_WIZ_MAGIC_MAP, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Golpear todo en LdV", { 'H' }, CMD_WIZ_HIT_ALL_LOS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Realizar un efecto", { 'E' }, CMD_WIZ_PERFORM_EFFECT, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Demostración de gráficos", { 'G' }, CMD_NULL, wiz_proj_demo, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_summon[] =
{
	{ "Invocar específico", { 'n' }, CMD_WIZ_SUMMON_NAMED, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Invocar aleatorio", { 's' }, CMD_WIZ_SUMMON_RANDOM, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_files[] =
{
	{ "Crear spoilers", { '"' }, CMD_NULL, do_cmd_spoilers, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Escribir mapa", { 'M' }, CMD_WIZ_DUMP_LEVEL_MAP, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_stats[] =
{
	{ "Objetos y monstruos", { 'S' }, CMD_WIZ_COLLECT_OBJ_MON_STATS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Cámaras", { 'P' }, CMD_WIZ_COLLECT_PIT_STATS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Niveles desconectados", { 'D' }, CMD_WIZ_COLLECT_DISCONNECT_STATS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Tecla alterna Obj/mon", { 'f' }, CMD_WIZ_COLLECT_OBJ_MON_STATS, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_query[] =
{
	{ "Característica", { 'F' }, CMD_WIZ_QUERY_FEATURE, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Bandera de casilla", { 'q' }, CMD_WIZ_QUERY_SQUARE_FLAG, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Ruido y olor", { '_' }, CMD_WIZ_PEEK_NOISE_SCENT, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Registro de pulsaciones", { 'L' }, CMD_WIZ_DISPLAY_KEYLOG, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

struct cmd_info cmd_debug_misc[] =
{
	{ "Nivel de luz de mago", { 'w' }, CMD_WIZ_WIZARD_LIGHT, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Crear una trampa", { 'T' }, CMD_WIZ_CREATE_TRAP, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Expulsar monstruos cercanos", { 'z' }, CMD_WIZ_BANISH, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Empujar objetos de la casilla", { '>' }, CMD_WIZ_PUSH_OBJECT, NULL, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
	{ "Salir sin guardar", { 'X' }, CMD_NULL, wiz_confirm_quit_no_save, player_can_debug_prereq, 0, NULL, NULL, NULL, 0 },
};

/**
 * Lista de listas de comandos; debido a la implementación en ui-context.c, todas las
 * entradas con menu_level == 0 deberían aparecer primero; la geometría fija en
 * ui-context.c limita el nivel máximo de anidamiento a 2
 */
struct command_list cmds_all[] =
{
	{ "Objetos",           cmd_item,        N_ELEMENTS(cmd_item), 0, 0 },
	{ "Acciones",          cmd_action,      N_ELEMENTS(cmd_action), 0, 0 },
	{ "Gestionar objetos", cmd_item_manage, N_ELEMENTS(cmd_item_manage), 0, false },
	{ "Información",       cmd_info,        N_ELEMENTS(cmd_info), 0, 0 },
	{ "Utilidades",        cmd_util,        N_ELEMENTS(cmd_util), 0, 0 },
	{ "Ocultos",           cmd_hidden,      N_ELEMENTS(cmd_hidden), 0, 0 },
	/*
	 * Esto está anidado debajo de "Ocultos"->"Comandos de modo depuración" y solo
	 * contiene categorías.
	 */
	{ "Depuración", cmd_debug, N_ELEMENTS(cmd_debug), 1, -1 },
	/* Estos están anidados en "Depuración"; los nombres tienen que coincidir con cmd_debug. */
	{ "DbgObj", cmd_debug_obj, N_ELEMENTS(cmd_debug_obj), 2, 1 },
	{ "DbgPlayer", cmd_debug_player, N_ELEMENTS(cmd_debug_player), 2, 1 },
	{ "DbgTele", cmd_debug_tele, N_ELEMENTS(cmd_debug_tele), 2, 1 },
	{ "DbgEffects", cmd_debug_effects, N_ELEMENTS(cmd_debug_effects), 2, 1 },
	{ "DbgSummon", cmd_debug_summon, N_ELEMENTS(cmd_debug_summon), 2, 1 },
	{ "DbgFiles", cmd_debug_files, N_ELEMENTS(cmd_debug_files), 2, 1 },
	{ "DbgStat", cmd_debug_stats, N_ELEMENTS(cmd_debug_stats), 2, 1 },
	{ "DbgQuery", cmd_debug_query, N_ELEMENTS(cmd_debug_query), 2, 1 },
	{ "DbgMisc", cmd_debug_misc, N_ELEMENTS(cmd_debug_misc), 2, 1 },
	{ NULL,              NULL,            0, 0, false }
};



/*** Funciones exportadas ***/

#define KEYMAP_MAX 2

/* Lista de comandos directamente accesibles indexados por carácter */
static struct cmd_info *converted_list[KEYMAP_MAX][UCHAR_MAX+1];

/*
 * Listas de comandos anidados; cada lista también está indexada por carácter pero no hay
 * distinción entre teclas originales/roguelike
 */
static int n_nested = 0;
static struct cmd_info ***nested_lists = NULL;

/**
 * Inicializar la lista de comandos.
 */
void cmd_init(void)
{
	size_t i, j;

	memset(converted_list, 0, sizeof(converted_list));

	/* Configurar almacenamiento para las listas de comandos anidados */
	if (nested_lists != NULL) {
		assert(n_nested >= 0);
		for (j = 0; j < (size_t)n_nested; j++) {
			mem_free(nested_lists[j]);
		}
		nested_lists = NULL;
	}
	n_nested = 0;
	for (j = 0; j < N_ELEMENTS(cmds_all) - 1; j++) {
		n_nested = MAX(n_nested, cmds_all[j].keymap);
	}
	if (n_nested > 0) {
		nested_lists = mem_zalloc(n_nested * sizeof(*nested_lists));
		for (j = 0; j < (size_t)n_nested; j++) {
			nested_lists[j] = mem_zalloc((UCHAR_MAX + 1) *
				sizeof(*(nested_lists[j])));
		}
	}

	/* Recorrer todos los comandos genéricos (-1 para la entrada final NULL) */
	for (j = 0; j < N_ELEMENTS(cmds_all) - 1; j++)
	{
		struct cmd_info *commands = cmds_all[j].list;

		/* Rellenar todo */
		if (cmds_all[j].keymap == 0) {
			for (i = 0; i < cmds_all[j].len; i++) {
				/* Si una tecla roguelike no está establecida, usar la predeterminada */
				if (!commands[i].key[1])
					commands[i].key[1] = commands[i].key[0];

				/* Saltar entradas que no tienen una tecla válida. */
				if (!commands[i].key[0] || !commands[i].key[1])
					continue;

				converted_list[0][commands[i].key[0]] =
					&commands[i];
				converted_list[1][commands[i].key[1]] =
					&commands[i];
			}
		} else if (cmds_all[j].keymap > 0) {
			int kidx = cmds_all[j].keymap - 1;

			assert(kidx < n_nested);
			for (i = 0; i < cmds_all[j].len; i++) {
				/*
				 * Los comandos anidados no pasan por un mapa de teclas;
				 * usar la predeterminada para la tecla roguelike.
				 */
				commands[i].key[1] = commands[i].key[0];

				/*
				 * Verificar si hay teclas duplicadas en el mismo
				 * conjunto de comandos.
				 */
				assert(!nested_lists[kidx][commands[i].key[0]]);

				nested_lists[kidx][commands[i].key[0]] =
					&commands[i];
			}
		}
	}
}

unsigned char cmd_lookup_key(cmd_code lookup_cmd, int mode)
{
	unsigned int i;

	assert(mode == KEYMAP_MODE_ROGUE || mode == KEYMAP_MODE_ORIG);

	for (i = 0; i < N_ELEMENTS(converted_list[mode]); i++) {
		struct cmd_info *cmd = converted_list[mode][i];

		if (cmd && cmd->cmd == lookup_cmd)
			return cmd->key[mode];
	}

	return 0;
}

unsigned char cmd_lookup_key_unktrl(cmd_code lookup_cmd, int mode)
{
	unsigned char c = cmd_lookup_key(lookup_cmd, mode);

	/*
	 * Porque UN_KTRL('ctrl-d') (ej. comando de ignorar en roguelike) da 'd'
	 * que es el comando de soltar en ambos conjuntos de teclas, usar UN_KTRL_CAP().
	 */
	if (c < 0x20)
		c = UN_KTRL_CAP(c);

	return c;
}

cmd_code cmd_lookup(unsigned char key, int mode)
{
	assert(mode == KEYMAP_MODE_ROGUE || mode == KEYMAP_MODE_ORIG);

	if (!converted_list[mode][key])
		return CMD_NULL;

	return converted_list[mode][key]->cmd;
}


/**
 * Devolver el índice en cmds_all para el nombre dado o -2 si no se encuentra.
 */
size_t cmd_list_lookup_by_name(const char *name)
{
	size_t i = 0;

	while (1) {
		if (i >= (int) N_ELEMENTS(cmds_all)) {
			/*
			 * Devolver un valor negativo diferente de -1 para evitar
			 * búsquedas futuras para el mismo nombre por ui-context.c.
			 * Esas búsquedas están garantizadas a fallar ya que los
			 * nombres en cmds_all no cambian.
			 */
			return -2;
		}
		if (streq(cmds_all[i].name, name)) {
			return i;
		}
		++i;
	}
}


/**
 * Analizar y ejecutar el comando actual
 * Dar "Advertencia" en comandos ilegales.
 */
void textui_process_command(void)
{
	int count = 0;
	bool done = true;
	ui_event e = textui_get_command(&count);
	struct cmd_info *cmd = NULL;
	unsigned char key = '\0';
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	switch (e.type) {
		case EVT_RESIZE: do_cmd_redraw(); return;
		case EVT_MOUSE: textui_process_click(e); return;
		case EVT_BUTTON:
		case EVT_KBRD: done = textui_process_key(e.key, &key, count); break;
		default: ;
	}

	/* Comando nulo */
	if (!key && done)
		return;

	if (key == KC_ENTER) {
		/* Usar menús de comandos */
		cmd = textui_action_menu_choose();
	} else {
		/* Tecla de comando */
		cmd = converted_list[mode][key];
	}

	if (cmd && done) {
		if (cmd->cmd || cmd->hook) {
			/* Confirmar para inscripciones de equipo usado. */
			if (!key_confirm_command(key)) cmd = NULL;
		} else {
			/*
			 * Se refiere a comandos anidados. Obtener el comando
			 * anidado. Estos no están sujetos a mapas de teclas y
			 * heredan el contador.
			 */
			while (cmd && !cmd->cmd && !cmd->hook) {
				char nestkey;

				if (cmd->nested_keymap > 0 &&
						cmd->nested_keymap <= n_nested &&
						cmd->nested_prompt) {
					if (get_com(cmd->nested_prompt, &nestkey)) {
						const char* em =
							cmd->nested_error;

						cmd = nested_lists[cmd->nested_keymap - 1][(unsigned char) nestkey];
						if (!cmd) {
							msg("%s", em ? em : "Ese no es un comando anidado válido.");
						}
					} else {
						cmd = NULL;
					}
				} else {
					cmd = NULL;
				}
			}
		}

		/* Verificar requisitos previos. */
		if (cmd && cmd->prereq && !cmd->prereq()) cmd = NULL;

		/* Dividir por tipo de comando */
		if (cmd && cmd->hook) {
			/* Comando de UI */
			cmd->hook();
		} else if (cmd && cmd->cmd) {
			/* Comando de juego */
			cmdq_push_repeat(cmd->cmd, count);
		} else if (!cmd && inkey_next) {
			/*
			 * Si se está procesando un mapa de teclas, saltarse el resto si falla una
			 * búsqueda de comando, confirmación o requisito previo. Para
			 * mapas de teclas que especifican múltiples comandos, el jugador
			 * podría querer continuar con el mapa de teclas, pero eso
			 * requeriría saltarse las teclas en el mapa de teclas que
			 * proporcionan entrada al comando que falló.
			 */
			inkey_next = NULL;
		}
	} else {
		/* Error */
		do_cmd_unknown();
		if (inkey_next) {
			/*
			 * Como arriba, abandonar el resto de un mapa de teclas cuando se
			 * esperaba un comando y lo que obtuvimos no fue reconocido.
			 */
			inkey_next = NULL;
		}
	}
}

errr textui_get_cmd(cmd_context context)
{
	if (context == CTX_GAME)
		textui_process_command();

	/* Si hemos llegado aquí, no hemos obtenido un comando. */
	return 1;
}


/**
 * Permitir la interrupción por parte del usuario durante comandos repetidos, correr y descansar.
 *
 * Esto solo se comprobará durante cada 128º turno de juego mientras se descansa.
 */
void check_for_player_interrupt(game_event_type type, game_event_data *data,
								void *user)
{
	/* Verificar "interrupción del jugador" */
	if (player->upkeep->running ||
	    cmd_get_nrepeats() > 0 ||
	    (player_is_resting(player) && !(turn & 0x7F))) {
		ui_event e;

		/* No esperar */
		inkey_scan = SCAN_INSTANT;

		/* Verificar si hay una tecla */
		e = inkey_ex();
		if (e.type != EVT_NONE) {
			/* Vaciar búfer y molestar */
			event_signal(EVENT_INPUT_FLUSH);
			disturb(player);
			msg("Cancelado.");
		}
	}
}

static void pre_turn_refresh(void)
{
	term *old = Term;
	int j;
	if (character_dungeon) {
		/* Redibujar mapa */
		player->upkeep->redraw |= (PR_MAP | PR_STATE);
		player->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
		handle_stuff(player);

		if (OPT(player, show_target) && target_sighted()) {
			struct loc target;
			target_get(&target);
			move_cursor_relative(target.y, target.x);
		} else {
			move_cursor_relative(player->grid.y, player->grid.x);
		}

		for (j = 0; j < ANGBAND_TERM_MAX; j++) {
			if (!angband_term[j]) continue;

			Term_activate(angband_term[j]);
			Term_fresh();
		}
	}
	Term_activate(old);
}

/**
 * Empezar a jugar realmente, ya sea cargando un archivo guardado o creando
 * un nuevo personaje
 */
static bool start_game(bool new_game)
{
	const char *loadpath = savefile;
	bool exists;

	/* El jugador será resucitado si está vivo en el archivo guardado */
	player->is_dead = true;

	/* Intentar cargar */
	savefile_get_panic_name(panicfile, sizeof(panicfile), loadpath);
	safe_setuid_grab();
	exists = loadpath[0] && file_exists(panicfile);
	safe_setuid_drop();
	if (exists) {
		bool newer;

		safe_setuid_grab();
		newer = file_newer(panicfile, loadpath);
		safe_setuid_drop();
		if (newer) {
			if (get_check("Existe un guardado de pánico. ¿Usarlo? ")) {
				loadpath = panicfile;
			}
		} else {
			/* Eliminar el guardado de pánico desactualizado. */
			safe_setuid_grab();
			file_delete(panicfile);
			safe_setuid_drop();
		}
	}
	safe_setuid_grab();
	exists = file_exists(loadpath);
	safe_setuid_drop();
	if (exists && !savefile_load(loadpath, arg_wizard)) {
		return false;
	}

	/* No se cargó un personaje vivo */
	if (player->is_dead || new_game) {
		character_generated = false;
		textui_do_birth();
	} else {
		/*
		 * Poner al día los objetos de maldición estándar con lo que el
		 * jugador sabe.
		 */
		update_player_object_knowledge(player);
	}

	/* Informar a la UI de que hemos comenzado. */
	event_signal(EVENT_LEAVE_INIT);
	event_signal(EVENT_ENTER_GAME);
	event_signal(EVENT_ENTER_WORLD);

	/* Guardado aún no requerido. */
	player->upkeep->autosave = false;

	/* Entrar al nivel, generando uno nuevo si es necesario */
	if (!character_dungeon) {
		prepare_next_level(player);
	}
	on_new_level();

	return true;
}

/**
 * Ayuda para select_savefile(): limpiar la matriz de cadenas
 */
static void cleanup_savefile_selection_strings(char **entries, int count)
{
	int i;

	for (i = 0; i < count; ++i) {
		string_free(entries[i]);
	}
	mem_free(entries);
}

/**
 * Ayuda para play_game(): implementar el menú de selección de archivo guardado.
 *
 * \param retry indica que esta es una llamada repetida porque el archivo guardado seleccionado
 * por una anterior no pudo cargarse.
 * \param new_game apunta a un booleano. El valor apuntado al inicio no se usa.
 * Al salir, *new_game será true si el usuario seleccionó comenzar una nueva partida.
 * En caso contrario, será false.
 */
static void select_savefile(bool retry, bool *new_game)
{
	/* Construir la lista de selecciones. */
	savefile_getter g = NULL;
	/*
	 * Dejar la primera entrada para seleccionar una nueva partida. Se rellenará la
	 * etiqueta más tarde.
	 */
	int count = 1, allocated = 16;
	char **entries = mem_zalloc(allocated * sizeof(*entries));
	char **names = mem_zalloc(allocated * sizeof(*names));
	int default_entry = 0;
	struct region m_region = { 0, 3, 0, 0 };
	bool allow_new_game = true;
	bool failed;
	struct menu *m;
	ui_event selection;

	while (got_savefile(&g)) {
		const struct savefile_details *details =
			get_savefile_details(g);

		assert(details);
		if (count == allocated) {
			allocated *= 2;
			entries = mem_realloc(entries,
				allocated * sizeof(*entries));
			names = mem_realloc(names, allocated * sizeof(*names));
		}
		if (details->desc) {
			entries[count] = string_make(format("Usar %s: %s",
				details->fnam + details->foff, details->desc));
		} else {
			entries[count] = string_make(format("Usar %s",
				details->fnam + details->foff));
		}
		names[count] = string_make(details->fnam);
		if (suffix(savefile, details->fnam)) {
			/*
			 * Coincide con lo que está en savefile; ponerlo segundo en la
			 * lista y marcarlo como entrada predeterminada. Si no se
			 * fuerza el nombre, limpiar savefile y arg_name para
			 * que la opción de nueva partida no esté configurada para sobrescribir
			 * un archivo guardado existente.
			 */
			if (count != 1) {
				char *hold_entry = entries[count];
				char *hold_name = names[count];
				int i;

				for (i = count; i > 1; --i) {
					entries[i] = entries[i - 1];
					names[i] = names[i - 1];
				}
				entries[1] = hold_entry;
				names[1] = hold_name;
			}
			default_entry = 1;
			if (!arg_force_name) {
				savefile[0] = '\0';
				arg_name[0] = '\0';
			}
		}
		++count;
	}
	if (got_savefile_dir(g)) {
		assert(allocated > 0 && !entries[0]);
		if (default_entry && arg_force_name) {
			/*
			 * El nombre establecido por el frontend ya está en uso y los nombres
			 * están forzados, así que no permitir la opción de nueva partida.
			 */
			int i;

			assert(!entries[0] && !names[0]);
			for (i = 0; i < count - 1; ++i) {
				entries[i] = entries[i + 1];
				names[i] = names[i + 1];
			}
			--default_entry;
			--count;
			allow_new_game = false;
		} else {
			entries[0] = string_make("Nueva partida");
		}
		failed = false;
	} else {
		failed = true;
	}
	cleanup_savefile_getter(g);
	if (failed) {
		cleanup_savefile_selection_strings(names, count);
		cleanup_savefile_selection_strings(entries, count);
		quit("No se puede abrir el directorio de archivos guardados");
	}

	m = menu_new(MN_SKIN_SCROLL, menu_find_iter(MN_ITER_STRINGS));
	menu_setpriv(m, count, entries);
	menu_layout(m, &m_region);
	m->cursor = default_entry;
	m->flags |= MN_DBL_TAP;

	screen_save();
	prt("Selecciona el guardado a usar (teclas de movimiento y enter o ratón) o salir",
		0, 0);
	prt("(escape o segundo botón del ratón).", 1, 0);
	prt((retry) ? "El archivo guardado seleccionado anteriormente no era utilizable." : "",
		2, 0);
	selection = menu_select(m, 0, false);
	screen_load();

	if (selection.type == EVT_SELECT) {
		if (m->cursor == 0 && allow_new_game) {
			*new_game = true;
		} else {
			*new_game = false;
			assert(m->cursor > 0 && m->cursor < count);
			path_build(savefile, sizeof(savefile),
				ANGBAND_DIR_SAVE, names[m->cursor]);
		}
	}

	menu_free(m);
	cleanup_savefile_selection_strings(names, count);
	cleanup_savefile_selection_strings(entries, count);

	if (selection.type == EVT_ESCAPE) {
		quit(NULL);
	}
}

/**
 * Jugar a Angband
 */
void play_game(enum game_mode_type mode)
{
	while (1) {
		play_again = false;

		/* Cargar un archivo guardado o crear un personaje, o ambos */
		switch (mode) {
		case GAME_LOAD:
		case GAME_NEW:
			if (!start_game(mode == GAME_NEW)) {
				quit("Archivo guardado corrupto");
			}
			break;

		case GAME_SELECT:
			{
				bool new_game = false, retry = false;

				while (1) {
					select_savefile(retry, &new_game);
					if (start_game(new_game)) {
						break;
					}
					retry = true;
				}
			}
			break;

		default:
			quit("Modo de juego inválido en play_game()");
			break;
		}

		/* Obtener comandos del usuario, luego procesar el mundo del juego
		 * hasta que la cola de comandos esté vacía y se necesite un nuevo
		 * comando del jugador */
		while (!player->is_dead && player->upkeep->playing) {
			pre_turn_refresh();
			cmd_get_hook(CTX_GAME);
			run_game_loop();
		}

		/* Cerrar juego al morir o al salir */
		close_game(true);

		if (!play_again) break;

		cleanup_angband();
		init_display();
		init_angband();
		if (reinit_hook != NULL) {
			(*reinit_hook)();
		}
		textui_init();
		if (mode == GAME_LOAD) {
			mode = GAME_NEW;
		}
	}
}

/**
 * Establecer el nombre del archivo guardado.
 */
void savefile_set_name(const char *fname, bool make_safe, bool strip_suffix)
{
	char path[128];
	size_t pathlen = sizeof path;
	size_t off = 0;

#if defined(SETGID)
	/*
	 * En sistemas SETGID, prefijamos el nombre del archivo con el UID del usuario
	 * para saber de quién es cada uno.
	 */

	strnfmt(path, pathlen, "%d.", player_uid);
	off = strlen(path);
	pathlen -= off;
	set_archive_user_prefix(path);
#endif

	if (make_safe) {
		player_safe_name(path + off, pathlen, fname, strip_suffix);
	} else {
		my_strcpy(path + off, fname, pathlen);
	}

	/* Guardar la ruta */
	path_build(savefile, sizeof(savefile), ANGBAND_DIR_SAVE, path);
}

/**
 * Probar si savefile_set_name() genera un nombre que ya está en uso.
 */
bool savefile_name_already_used(const char *fname, bool make_safe,
		bool strip_suffix)
{
	char *hold = string_make(savefile);
	bool result;

	savefile_set_name(fname, make_safe, strip_suffix);
	safe_setuid_grab();
	result = file_exists(savefile);
	safe_setuid_drop();
	my_strcpy(savefile, hold, sizeof(savefile));
	string_free(hold);
	return result;
}

/**
 * Guardar el juego.
 */
void save_game(void)
{
	(void) save_game_checked();
}

/**
 * Guardar el juego.
 *
 * \return si el guardado fue exitoso.
 */
bool save_game_checked(void)
{
	char path[1024];
	bool result;

	/* Molestar al jugador */
	disturb(player);

	/* Limpiar mensajes */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Manejar eventos */
	handle_stuff(player);

	/* Mensaje */
	prt("Guardando partida...", 0, 0);

	/* Refrescar */
	Term_fresh();

	/* El jugador no está muerto */
	my_strcpy(player->died_from, "(guardado)", sizeof(player->died_from));

	/* Prohibir suspender */
	signals_ignore_tstp();

	/* Guardar el jugador */
	if (savefile_save(savefile)) {
		prt("Guardando partida... hecho.", 0, 0);
		result = true;
	} else {
		prt("¡Guardando partida... falló!", 0, 0);
		result = false;
	}

	/* Refrescar */
	Term_fresh();

	/* Permitir suspender de nuevo */
	signals_handle_tstp();

	/* Guardar las preferencias de ventana */
	path_build(path, sizeof(path), ANGBAND_DIR_USER, "window.prf");
	if (!prefs_save(path, option_dump, "Volcar configuración de ventanas"))
		prt("Fallo al guardar preferencias de subventana", 0, 0);

	/* Refrescar */
	Term_fresh();

	/* Guardar memoria de monstruos en el directorio de usuario */
	if (!lore_save("lore.txt")) {
		msg("¡fallo al guardar lore!");
		event_signal(EVENT_MESSAGE_FLUSH);
	}

	/* Refrescar */
	Term_fresh();

	/* Notar que el jugador no está muerto */
	my_strcpy(player->died_from, "(vivo y coleando)", sizeof(player->died_from));

	return result;
}


/**
 * Cerrar la partida actual (el jugador puede estar muerto o no).
 *
 * \param prompt_failed_save Si es true, preguntar al usuario si quiere reintentar si falla el guardado.
 * En caso contrario, no se emite ningún aviso.
 *
 * Nótese que el archivo guardado no se guarda hasta que la lápida está
 * realmente visible y el jugador tiene la oportunidad de examinar
 * el inventario y tal. Esto permite hacer trampas si el juego
 * está equipado con un método "salir sin guardar". XXX XXX XXX
 */
void close_game(bool prompt_failed_save)
{
	bool prompting = true;

	/* Informar a la UI de que hemos terminado con el mundo */
	event_signal(EVENT_LEAVE_WORLD);

	/* Manejar eventos */
	handle_stuff(player);

	/* Vaciar los mensajes */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Vaciar la entrada */
	event_signal(EVENT_INPUT_FLUSH);

	/* No suspender ahora */
	signals_ignore_tstp();

	/* Aumentar la profundidad "icky" */
	screen_save_depth++;

	/* Gestionar el archivo de randarts */
	if (OPT(player, birth_randarts)) {
		deactivate_randart_file();
	}

	/* Gestionar muerte o vida */
	if (player->is_dead) {
		death_knowledge(player);
		death_screen();

		/* Guardar jugador muerto */
		while (prompting && !savefile_save(savefile)) {
			if (!prompt_failed_save
					|| !get_check("Fallo al guardar. ¿Reintentar? ")) {
				prompting = false;
				msg("¡fallo al guardar la muerte!");
				event_signal(EVENT_MESSAGE_FLUSH);
			}
		}
	} else {
		/* Guardar la partida */
		while (prompting && !save_game_checked()) {
			if (!prompt_failed_save
					|| !get_check("Fallo al guardar. ¿Reintentar? ")) {
				prompting = false;
			}
		}

		if (Term->mapped_flag) {
			struct keypress ch;

			prt("Pulsa Return (o Escape).", 0, 40);
			ch = inkey();
			if (ch.code != ESCAPE)
				predict_score(false);
		}
	}

	/* Limpiar la lista de monstruos */
	wipe_mon_list(cave, player);

	/* Disminuir la profundidad "icky" */
	screen_save_depth--;

	/* Informar a la UI de que hemos terminado con el estado del juego */
	event_signal(EVENT_LEAVE_GAME);

	/* Permitir suspender ahora */
	signals_handle_tstp();
}


/**
 * Enumerar los archivos guardados en el directorio de guardados que están disponibles para el
 * jugador actual.
 *
 * \param pg apunta al estado de la enumeración. Si *pg es NULL, la
 * enumeración comenzará desde cero. Después de enumerar, *pg debe ser
 * pasado a cleanup_savefile_getter() para liberar los recursos asignados.
 * \return true si se encontró otro archivo guardado útil para el jugador.
 * En ese caso, llamar a get_savefile_details() en *pg devolverá un resultado
 * no NULL. En caso contrario, devuelve false.
 */
bool got_savefile(savefile_getter *pg)
{
	char fname[256];

	if (*pg == NULL) {
		/* Inicializar la enumeración. */
		*pg = mem_zalloc(sizeof(**pg));
		/* Se necesitan privilegios elevados para leer del directorio de guardados. */
		safe_setuid_grab();
		(*pg)->d = my_dopen(ANGBAND_DIR_SAVE);
		safe_setuid_drop();
		if (!(*pg)->d) {
			return false;
		}
		(*pg)->have_savedir = true;
		/*
		 * Configurar el prefijo específico del usuario. Imita savefile_set_name().
		 */
#ifdef SETGID
		strnfmt((*pg)->uid_c, sizeof((*pg)->uid_c), "%d.", player_uid);
		(*pg)->details.foff = strlen((*pg)->uid_c);
#else
		(*pg)->details.foff = 0;
#endif
	} else {
		if (!(*pg)->d) {
			assert(!(*pg)->have_details);
			return false;
		}
	}

	while (1) {
		char path[1024];
		const char *desc;
		bool no_entry;

		/*
		 * También se necesitan privilegios elevados para las consultas de atributos de archivo
		 * en my_dread().
		 */
		safe_setuid_grab();
		no_entry = !my_dread((*pg)->d, fname, sizeof(fname));
		safe_setuid_drop();
		if (no_entry) {
			break;
		}

#ifdef SETGID
		/* Verificar que el nombre del archivo guardado comienza con el ID del usuario. */
		if (!prefix(fname, (*pg)->uid_c)) {
			continue;
		}
#endif

		path_build(path, sizeof(path), ANGBAND_DIR_SAVE, fname);
		desc = savefile_get_description(path);
		string_free((*pg)->details.fnam);
		(*pg)->details.fnam = string_make(fname);
		string_free((*pg)->details.desc);
		(*pg)->details.desc = string_make(desc);
		(*pg)->have_details = true;
		return true;
	}

	my_dclose((*pg)->d);
	(*pg)->d = NULL;
	(*pg)->have_details = false;

	return false;
}


/**
 * Devolver si el directorio de archivos guardados era al menos legible.
 */
bool got_savefile_dir(const savefile_getter g)
{
	return g && g->have_savedir;
}


/**
 * Devolver los detalles para un archivo guardado enumerado por una llamada anterior a
 * got_savefile().
 *
 * \return es NULL si la llamada anterior a get_savefile() falló. En caso contrario,
 * devuelve un puntero no nulo con los detalles sobre el archivo guardado enumerado.
 */
const struct savefile_details *get_savefile_details(const savefile_getter g)
{
	return (g && g->have_details) ? &g->details : NULL;
}


/**
 * Limpiar los recursos asignados por got_savefile().
 */
void cleanup_savefile_getter(savefile_getter g)
{
	if (g) {
		string_free(g->details.desc);
		string_free(g->details.fnam);
		if (g->d) {
			my_dclose(g->d);
		}
		mem_free(g);
	}
}