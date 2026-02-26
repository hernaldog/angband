/**
 * \file ui-birth.c
 * \brief Interfaz de usuario basada en texto para la creación de personajes
 *
 * Copyright (c) 1987 - 2015 Angband contributors
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
#include "cmd-core.h"
#include "game-event.h"
#include "game-input.h"
#include "obj-tval.h"
#include "player.h"
#include "player-birth.h"
#include "player-spell.h"
#include "ui-birth.h"
#include "ui-display.h"
#include "ui-game.h"
#include "ui-help.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-options.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-target.h"

/**
 * Visión general
 * ==============
 * Este archivo implementa la parte de la interfaz de usuario del proceso de
 * nacimiento para la interfaz de usuario clásica basada en terminal de Angband.
 *
 * Modela el nacimiento como una serie de pasos que deben llevarse a cabo en
 * un orden específico, con la opción de retroceder para revisar elecciones
 * pasadas.
 *
 * Comienza cuando recibimos el evento EVENT_ENTER_BIRTH del juego,
 * y termina cuando recibimos el evento EVENT_LEAVE_BIRTH. Entre medias,
 * se nos pedirá repetidamente que proporcionemos un comando de juego, que
 * cambiará el estado del personaje que se está creando. Una vez que el jugador
 * está satisfecho con su personaje, enviamos el comando CMD_ACCEPT_CHARACTER.
 */

/**
 * Un global local a este archivo para mantener la parte más importante del estado
 * entre llamadas al juego propiamente dicho. Probablemente no es estrictamente necesario,
 * pero reduce un poco la complejidad. */
enum birth_stage
{
	BIRTH_BACK = -1,
	BIRTH_RESET = 0,
	BIRTH_QUICKSTART,
	BIRTH_RACE_CHOICE,
	BIRTH_CLASS_CHOICE,
	BIRTH_ROLLER_CHOICE,
	BIRTH_POINTBASED,
	BIRTH_ROLLER,
	BIRTH_NAME_CHOICE,
	BIRTH_HISTORY_CHOICE,
	BIRTH_FINAL_CONFIRM,
	BIRTH_COMPLETE
};


enum birth_questions
{
	BQ_METHOD = 0,
	BQ_RACE,
	BQ_CLASS,
	BQ_ROLLER,
	MAX_BIRTH_QUESTIONS
};

enum birth_rollers
{
	BR_POINTBASED = 0,
	BR_NORMAL,
	MAX_BIRTH_ROLLERS
};


static void finish_with_random_choices(enum birth_stage current);
static void point_based_start(void);
static bool quickstart_allowed = false;
bool arg_force_name;

/**
 * ------------------------------------------------------------------------
 * Pantalla de inicio rápido.
 * ------------------------------------------------------------------------ */
static enum birth_stage textui_birth_quickstart(void)
//cambios de nombre fantasma
{
	const char *prompt = "['S': usar tal cual; 'N': rehacer; 'C': cambiar nombre/historia; '=': establecer opciones de nacimiento]";

	enum birth_stage next = BIRTH_QUICKSTART;

	/* Preguntar por ello */
	prt("¿Nuevo personaje basado en el anterior?:", 0, 0);
	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	do {
		/* Obtener una tecla */
		struct keypress ke = inkey();
		
		if (ke.code == 'N' || ke.code == 'n') {
			cmdq_push(CMD_BIRTH_RESET);
			next = BIRTH_RACE_CHOICE;
		} else if (ke.code == KTRL('X')) {
			quit(NULL);
		} else if ( !arg_force_name && (ke.code == 'C' || ke.code == 'c')) {
			next = BIRTH_NAME_CHOICE;
		} else if (ke.code == '=') {
			do_cmd_options_birth();
		} else if (ke.code == 'S' || ke.code == 's') {
			cmdq_push(CMD_ACCEPT_CHARACTER);
			next = BIRTH_COMPLETE;
		}
	} while (next == BIRTH_QUICKSTART);

	/* Limpiar mensaje */
	clear_from(23);

	return next;
}

/**
 * ------------------------------------------------------------------------
 * Las diversas partes del "menú" del proceso de nacimiento - concretamente la elección de raza,
 * clase, y tipo de generador.
 * ------------------------------------------------------------------------ */

/**
 * Los diversos menús
 */
static struct menu race_menu, class_menu, roller_menu;

/**
 * Ubicaciones de los menús, etc. en la pantalla
 */
#define HEADER_ROW       1
#define QUESTION_ROW     7
#define TABLE_ROW        9

#define QUESTION_COL     2
#define RACE_COL         2
#define RACE_AUX_COL    19
#define CLASS_COL       19
#define CLASS_AUX_COL   36
#define ROLLER_COL      36
#define HIST_INSTRUCT_ROW 18

#define MENU_ROWS TABLE_ROW + 14

/**
 * columna y fila superior izquierda, ancho, y columna inferior
 */
static region race_region = {RACE_COL, TABLE_ROW, 17, MENU_ROWS};
static region class_region = {CLASS_COL, TABLE_ROW, 17, MENU_ROWS};
static region roller_region = {ROLLER_COL, TABLE_ROW, 34, MENU_ROWS};

/**
 * Usamos diferentes "funciones de exploración" del menú para mostrar el texto de ayuda
 * a veces suministrado con los elementos del menú - actualmente solo la lista
 * de bonificaciones, etc., correspondientes a cada raza y clase.
 */
typedef void (*browse_f) (int oid, void *db, const region *l);

/**
 * Tenemos una de estas estructuras para cada menú que mostramos - contiene
 * la información útil para el menú - texto de los elementos del menú, texto de "ayuda",
 * selección actual (o por defecto), si se permite la selección aleatoria,
 * y la etapa actual del proceso para configurar un menú contextual y
 * transmitir el resultado de una selección en ese menú.
 */
struct birthmenu_data 
{
	const char **items;
	const char *hint;
	bool allow_random;
	enum birth_stage stage_inout;
};

/**
 * Una función de "visualización" personalizada para nuestros menús que simplemente muestra el
 * texto de nuestros datos almacenados en un color diferente si está actualmente
 * seleccionado.
 */
static void birthmenu_display(struct menu *menu, int oid, bool cursor,
			      int row, int col, int width)
{
	struct birthmenu_data *data = menu->menu_data;

	uint8_t attr = curs_attrs[CURS_KNOWN][0 != cursor];
	c_put_str(attr, data->items[oid], row, col);
}

/**
 * Nuestro iterador de menú personalizado, solo realmente necesario para permitirnos anular
 * el manejo por defecto de "comandos" en los iteradores estándar (por lo tanto
 * solo definiendo las partes de visualización y manejador).
 */
static const menu_iter birth_iter = { NULL, NULL, birthmenu_display, NULL, NULL };

static void skill_help(const int r_skills[], const int c_skills[], int mhp, int exp, int infra)
{
	int16_t skills[SKILL_MAX];
	unsigned i;

	for (i = 0; i < SKILL_MAX ; ++i)
		skills[i] = (r_skills ? r_skills[i] : 0 ) + (c_skills ? c_skills[i] : 0);

	text_out_e("Golpear/Disparar/Lanzar: %+d/%+d/%+d\n", skills[SKILL_TO_HIT_MELEE],
			   skills[SKILL_TO_HIT_BOW], skills[SKILL_TO_HIT_THROW]);
	text_out_e("Dado de golpe: %2d   Modificador EXP: %d%%\n", mhp, exp);
	text_out_e("Desarmar: %+3d/%+3d   Dispositivos: %+3d\n", skills[SKILL_DISARM_PHYS],
			   skills[SKILL_DISARM_MAGIC], skills[SKILL_DEVICE]);
	text_out_e("Salvación:   %+3d   Sigilo: %+3d\n", skills[SKILL_SAVE],
			   skills[SKILL_STEALTH]);
	if (infra >= 0)
		text_out_e("Infravisión:  %d pies\n", infra * 10);
	text_out_e("Excavar:      %+d\n", skills[SKILL_DIGGING]);
	text_out_e("Buscar:       %+d", skills[SKILL_SEARCH]);
	if (infra < 0)
		text_out_e("\n");
}

static void race_help(int i, void *db, const region *l)
{
	int j;
	struct player_race *r = player_id2race(i);
	int len = (STAT_MAX + 1) / 2;

	struct player_ability *ability;
	int n_flags = 0;
	int flag_space = 3;

	if (!r) return;

	/* Salida a la pantalla */
	text_out_hook = text_out_to_screen;
	
	/* Sangrar salida */
	text_out_indent = RACE_AUX_COL;
	Term_gotoxy(RACE_AUX_COL, TABLE_ROW);

	for (j = 0; j < len; j++) {  
		const char *name = stat_names_reduced[j];
		int adj = r->r_adj[j];

		text_out_e("%s%+3d", name, adj);

		if (j * 2 + 1 < STAT_MAX) {
			name = stat_names_reduced[j + len];
			adj = r->r_adj[j + len];
			text_out_e("  %s%+3d", name, adj);
		}

		text_out("\n");
	}
	
	text_out_e("\n");
	skill_help(r->r_skills, NULL, r->r_mhp, r->r_exp, r->infra);
	text_out_e("\n");

	for (ability = player_abilities; ability; ability = ability->next) {
		if (n_flags >= flag_space) break;
		if (streq(ability->type, "object") &&
			!of_has(r->flags, ability->index)) {
			continue;
		} else if (streq(ability->type, "player") &&
				   !pf_has(r->pflags, ability->index)) {
			continue;
		} else if (streq(ability->type, "element") &&
				   (r->el_info[ability->index].res_level != ability->value)) {
			continue;
		}
		text_out_e("\n%s", ability->name);
		n_flags++;
	}

	while (n_flags < flag_space) {
		text_out_e("\n");
		n_flags++;
	}

	/* Restablecer sangría de text_out() */
	text_out_indent = 0;
}

static void class_help(int i, void *db, const region *l)
{
	int j;
	struct player_class *c = player_id2class(i);
	const struct player_race *r = player->race;
	int len = (STAT_MAX + 1) / 2;

	struct player_ability *ability;
	int n_flags = 0;
	int flag_space = 5;

	if (!c) return;

	/* Salida a la pantalla */
	text_out_hook = text_out_to_screen;
	
	/* Sangrar salida */
	text_out_indent = CLASS_AUX_COL;
	Term_gotoxy(CLASS_AUX_COL, TABLE_ROW);

	for (j = 0; j < len; j++) {  
		const char *name = stat_names_reduced[j];
		int adj = c->c_adj[j] + r->r_adj[j];

		text_out_e("%s%+3d", name, adj);

		if (j*2 + 1 < STAT_MAX) {
			name = stat_names_reduced[j + len];
			adj = c->c_adj[j + len] + r->r_adj[j + len];
			text_out_e("  %s%+3d", name, adj);
		}

		text_out("\n");
	}

	text_out_e("\n");
	
	skill_help(r->r_skills, c->c_skills, r->r_mhp + c->c_mhp,
			   r->r_exp + c->c_exp, -1);

	if (c->magic.total_spells) {
		int count;
		struct magic_realm *realm = class_magic_realms(c, &count), *realm_next;
		char buf[120];

		my_strcpy(buf, realm->name, sizeof(buf));
		realm_next = realm->next;
		mem_free(realm);
		realm = realm_next;
		if (count > 1) {
			while (realm) {
				count--;
				if (count) {
					my_strcat(buf, ", ", sizeof(buf));
				} else {
					my_strcat(buf, " y ", sizeof(buf));
				}
				my_strcat(buf, realm->name, sizeof(buf));
				realm_next = realm->next;
				mem_free(realm);
				realm = realm_next;
			}
		}
		text_out_e("\nAprende magia de %s", buf);
	}

	for (ability = player_abilities; ability; ability = ability->next) {
		if (n_flags >= flag_space) break;
		if (streq(ability->type, "object") &&
			!of_has(c->flags, ability->index)) {
			continue;
		} else if (streq(ability->type, "player") &&
				   !pf_has(c->pflags, ability->index)) {
			continue;
		} else if (streq(ability->type, "element")) {
			continue;
		}

		text_out_e("\n%s", ability->name);
		n_flags++;
	}

	while (n_flags < flag_space) {
		text_out_e("\n");
		n_flags++;
	}

	/* Restablecer sangría de text_out() */
	text_out_indent = 0;
}

/**
 * Mostrar y manejar la interacción del usuario con un menú contextual apropiado para la
 * etapa actual. De esa manera, las acciones disponibles con ciertas teclas también están
 * disponibles si solo se usa el ratón.
 *
 * \param current_menu es el menú estándar (no contextual) para la etapa.
 * \param in es el evento que desencadena el menú contextual. El tipo de in debe ser
 * EVT_MOUSE.
 * \param out es el evento que se pasará hacia arriba (al manejo interno en
 * menu_select() o, potencialmente, al llamador de menu_select()).
 * \return true si el evento fue manejado; de lo contrario, devuelve false.
 *
 * La lógica aquí se superpone con lo que se hace para manejar cmd_keys en
 * menu_question().
 */
static bool use_context_menu_birth(struct menu *current_menu,
		const ui_event *in, ui_event *out)
{
	enum {
		ACT_CTX_BIRTH_OPT,
		ACT_CTX_BIRTH_RAND,
		ACT_CTX_BIRTH_FINISH_RAND,
		ACT_CTX_BIRTH_QUIT,
		ACT_CTX_BIRTH_HELP
	};
	struct birthmenu_data *menu_data = menu_priv(current_menu);
	char *labels;
	struct menu *m;
	int selected;

	assert(in->type == EVT_MOUSE);
	if (in->mouse.y != QUESTION_ROW && in->mouse.y != QUESTION_ROW + 1) {
		return false;
	}

	labels = string_make(lower_case);
	m = menu_dynamic_new();

	m->selections = labels;
	menu_dynamic_add_label(m, "Mostrar opciones de nacimiento", '=',
		ACT_CTX_BIRTH_OPT, labels);
	if (menu_data->allow_random) {
		menu_dynamic_add_label(m, "Seleccionar uno al azar", '*',
			ACT_CTX_BIRTH_RAND, labels);
	}
	menu_dynamic_add_label(m, "Terminar con elecciones aleatorias", '@',
		ACT_CTX_BIRTH_FINISH_RAND, labels);
	menu_dynamic_add_label(m, "Salir", 'q', ACT_CTX_BIRTH_QUIT, labels);
	menu_dynamic_add_label(m, "Ayuda", '?', ACT_CTX_BIRTH_HELP, labels);

	screen_save();

	menu_dynamic_calc_location(m, in->mouse.x, in->mouse.y);
	region_erase_bordered(&m->boundary);

	selected = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	switch (selected) {
	case ACT_CTX_BIRTH_OPT:
		do_cmd_options_birth();
		/* La etapa permanece igual, así que dejar stage_inout como está. */
		out->type = EVT_SWITCH;
		break;

	case ACT_CTX_BIRTH_RAND:
		current_menu->cursor = randint0(current_menu->count);
		out->type = EVT_SELECT;
		break;

	case ACT_CTX_BIRTH_FINISH_RAND:
		finish_with_random_choices(menu_data->stage_inout);
		menu_data->stage_inout = BIRTH_FINAL_CONFIRM;
		out->type = EVT_SWITCH;
		break;

	case ACT_CTX_BIRTH_QUIT:
		quit(NULL);
		break;

	case ACT_CTX_BIRTH_HELP:
		do_cmd_help();
		menu_data->stage_inout = BIRTH_RESET;
		out->type = EVT_SWITCH;

	default:
		/* No hay nada que hacer. */
		break;
	}

	return true;
}

/**
 * Configurar uno de nuestros menús listo para mostrar opciones para una pregunta de nacimiento.
 * Esto es ligeramente complicado.
 */
static void init_birth_menu(struct menu *menu, int n_choices,
							int initial_choice, const region *reg,
							bool allow_random, browse_f aux)
{
	struct birthmenu_data *menu_data;

	/* Inicializar un menú básico */
	menu_init(menu, MN_SKIN_SCROLL, &birth_iter);

	/* Un par de banderas de comportamiento - queremos selecciones como letras
	   omitiendo los movimientos de dirección cardinal de tipo roguelike y un
	   doble toque para actuar como una selección. */
	menu->selections = all_letters_nohjkl;
	menu->flags = MN_DBL_TAP;

	/* Copiar la selección inicial sugerida del juego, etc. */
	menu->cursor = initial_choice;

	/* Asignar suficiente espacio para nuestras propias partes de información del menú. */
	menu_data = mem_alloc(sizeof *menu_data);

	/* Asignar espacio para una matriz de textos de elementos del menú y textos de ayuda
	   (donde corresponda) */
	menu_data->items = mem_alloc(n_choices * sizeof *menu_data->items);
	menu_data->allow_random = allow_random;

	/* Establecer datos privados */
	menu_setpriv(menu, n_choices, menu_data);

	/* Configurar el gancho "browse" para mostrar texto de ayuda (donde corresponda). */
	menu->browse_hook = aux;

	/*
	 * Todos usan el mismo gancho para mostrar un menú contextual para que la
	 * funcionalidad impulsada por la entrada del teclado (ver cómo se usa cmd_keys
	 * en menu_question()) también esté disponible usando el ratón.
	 */
	menu->context_hook = use_context_menu_birth;

	/* Diseñar el menú apropiadamente */
	menu_layout(menu, reg);
}



static void setup_menus(void)
{
	int i, n;
	struct player_class *c;
	struct player_race *r;

	const char *roller_choices[MAX_BIRTH_ROLLERS] = { 
		"Basado en puntos", 
		"Generador estándar" 
	};

	struct birthmenu_data *mdata;

	/* Contar las razas */
	n = 0;
	for (r = races; r; r = r->next) n++;

	/* Menú de raza. */
	init_birth_menu(&race_menu, n, player->race ? player->race->ridx : 0,
	                &race_region, true, race_help);
	mdata = race_menu.menu_data;

	for (i = 0, r = races; r; r = r->next, i++)
		mdata->items[r->ridx] = r->name;
	mdata->hint = "La raza afecta a las estadísticas y habilidades, da resistencias y capacidades.";

	/* Contar las clases */
	n = 0;
	for (c = classes; c; c = c->next) n++;

	/* Menú de clase similar al de raza. */
	init_birth_menu(&class_menu, n, player->class ? player->class->cidx : 0,
	                &class_region, true, class_help);
	mdata = class_menu.menu_data;

	for (i = 0, c = classes; c; c = c->next, i++)
		mdata->items[c->cidx] = c->name;
	mdata->hint = "La clase afecta a las estadísticas, habilidades y otros rasgos del personaje.";
		
	/* Menú de generador sencillo */
	init_birth_menu(&roller_menu, MAX_BIRTH_ROLLERS, 0, &roller_region, false,
					NULL);
	mdata = roller_menu.menu_data;
	for (i = 0; i < MAX_BIRTH_ROLLERS; i++)
		mdata->items[i] = roller_choices[i];
	mdata->hint = "Elige cómo generar tus estadísticas. Se recomienda el basado en puntos.";
}

/**
 * Limpia nuestra información de menú almacenada cuando hemos terminado con ella.
 */
static void free_birth_menu(struct menu *menu)
{
	struct birthmenu_data *data = menu->menu_data;

	if (data) {
		mem_free(data->items);
		mem_free(data);
	}
}

static void free_birth_menus(void)
{
	/* Ya no necesitamos estos. */
	free_birth_menu(&race_menu);
	free_birth_menu(&class_menu);
	free_birth_menu(&roller_menu);
}

/**
 * Limpiar la pregunta anterior
 */
static void clear_question(void)
{
	int i;

	for (i = QUESTION_ROW; i < TABLE_ROW; i++)
		/* Limpiar línea, posicionar cursor */
		Term_erase(0, i, 255);
}


#define BIRTH_MENU_HELPTEXT \
	"{light blue}Por favor, selecciona los rasgos de tu personaje:{/}\n\n" \
	"Usa las {light green}teclas de movimiento{/} para desplazarte por el menú, " \
	"{light green}Enter{/} para seleccionar el elemento, '{light green}*{/}' " \
	"para usar una opción aleatoria, '{light green}@{/}' para armar el personaje completo de forma aleatoria, " \
	"'{light green}ESC{/}' para retroceder en el proceso, " \
	"'{light green}={/}' para ver opciones de nacimiento, '{light green}?{/}' " \
	"para ayuda, o '{light green}Ctrl-X{/}' para salir."

/**
 * Mostrar las instrucciones de nacimiento en una pantalla por lo demás en blanco
 */	
static void print_menu_instructions(void)
{
	/* Limpiar pantalla */
	Term_clear();
	
	/* Salida a la pantalla */
	text_out_hook = text_out_to_screen;
	
	/* Sangrar salida */
	text_out_indent = QUESTION_COL;
	Term_gotoxy(QUESTION_COL, HEADER_ROW);
	
	/* Mostrar información útil */
	text_out_e(BIRTH_MENU_HELPTEXT);
	
	/* Restablecer sangría de text_out() */
	text_out_indent = 0;
}

/**
 * Avanzar la generación del personaje al paso de confirmación usando elecciones aleatorias
 * y una compra por puntos por defecto para las estadísticas.
 *
 * \param current es la etapa actual para la generación del personaje.
 */
static void finish_with_random_choices(enum birth_stage current)
{
	struct {
		cmd_code code;
		const char* arg_name;
		int arg_choice;
		char* arg_str;
		bool arg_is_choice;
	} cmds[4];
	int ncmd = 0;
	const struct player_race *pr;
	char name[PLAYER_NAME_LEN];
	char history[240];

	if (current <= BIRTH_RACE_CHOICE) {
		int n, i;

		for (pr = races, n = 0; pr; pr = pr->next, ++n) {}
		i = randint0(n);
		pr = player_id2race(i);

		assert(ncmd < (int)N_ELEMENTS(cmds));
		cmds[ncmd].code = CMD_CHOOSE_RACE;
		cmds[ncmd].arg_name = "choice";
		cmds[ncmd].arg_choice = i;
		cmds[ncmd].arg_is_choice = true;
		++ncmd;
	} else {
		pr = player->race;
	}

	if (current <= BIRTH_CLASS_CHOICE) {
		struct player_class *pc;
		int n, i;

		for (pc = classes, n = 0; pc; pc = pc->next, ++n) {}
		i = randint0(n);

		assert(ncmd < (int)N_ELEMENTS(cmds));
		cmds[ncmd].code = CMD_CHOOSE_CLASS;
		cmds[ncmd].arg_name = "choice";
		cmds[ncmd].arg_choice = i;
		cmds[ncmd].arg_is_choice = true;
		++ncmd;
	}

	if (current <= BIRTH_NAME_CHOICE) {
		/*
		 * Imitar lo que sucede en get_name_command() para el
		 * caso arg_force_name.
		 */
		if (arg_force_name) {
			if (arg_name[0]) {
				my_strcpy(player->full_name, arg_name,
					sizeof(player->full_name));
			}
		} else {
			int ntry = 0;

			while (1) {
				if (ntry > 100) {
					quit("Posible error: no se pudo generar "
						"un nombre aleatorio que no estuviera "
						"en uso para un archivo guardado");
				}
				player_random_name(name, sizeof(name));
				/*
				 * Estamos listos para continuar si el frontend especificó
				 * un archivo guardado a usar o el nombre del archivo guardado
				 * correspondiente al nombre aleatorio no está ya en uso.
				 */
				if (savefile[0] || !savefile_name_already_used(name, true, true)) {
					break;
				}
				++ntry;
			}
			assert(ncmd < (int)N_ELEMENTS(cmds));
			cmds[ncmd].code = CMD_NAME_CHOICE;
			cmds[ncmd].arg_name = "name";
			cmds[ncmd].arg_str = name;
			cmds[ncmd].arg_is_choice = false;
			++ncmd;
		}
	}

	if (current <= BIRTH_HISTORY_CHOICE) {
		char *buf;

		buf = get_history(pr->history);
		my_strcpy(history, buf, sizeof(history));
		string_free(buf);

		assert(ncmd < (int)N_ELEMENTS(cmds));
		cmds[ncmd].code = CMD_HISTORY_CHOICE;
		cmds[ncmd].arg_name = "history";
		cmds[ncmd].arg_str = history;
		cmds[ncmd].arg_is_choice = false;
		++ncmd;
	}

	/* Insertar en orden inverso: el último insertado se ejecutará primero. */
	while (ncmd > 0) {
		--ncmd;
		cmdq_push(cmds[ncmd].code);
		if (cmds[ncmd].arg_name) {
			if (cmds[ncmd].arg_is_choice) {
				cmd_set_arg_choice(cmdq_peek(),
					cmds[ncmd].arg_name,
					cmds[ncmd].arg_choice);
			} else {
				cmd_set_arg_string(cmdq_peek(),
					cmds[ncmd].arg_name,
					cmds[ncmd].arg_str);
			}
		}
	}
}

/**
 * Permitir al usuario seleccionar del menú actual, y devolver el
 * comando correspondiente al juego. Algunas acciones son manejadas completamente
 * por la interfaz de usuario (mostrar texto de ayuda, por ejemplo).
 */
static enum birth_stage menu_question(enum birth_stage current,
									  struct menu *current_menu,
									  cmd_code choice_command)
{
	struct birthmenu_data *menu_data = menu_priv(current_menu);
	ui_event cx;

	enum birth_stage next = BIRTH_RESET;
	
	/* Imprimir la pregunta que se está haciendo actualmente. */
	clear_question();
	Term_putstr(QUESTION_COL, QUESTION_ROW, -1, COLOUR_YELLOW, menu_data->hint);

	current_menu->cmd_keys = "?=*@\x18";	 /* ?, =, *, @, <ctl-X> */

	while (next == BIRTH_RESET) {
		/* Mostrar el menú, esperar a que se haga algún tipo de selección. */
		menu_data->stage_inout = current;
		cx = menu_select(current_menu, EVT_KBRD, false);

		/* Como todos los menús se muestran en estilo "jerárquico", permitimos
		   el uso de "atrás" (tecla de flecha izquierda o equivalente) para retroceder en
		   el proceso, así como "escape". */
		if (cx.type == EVT_ESCAPE) {
			next = BIRTH_BACK;
		} else if (cx.type == EVT_SELECT) {
			if (current == BIRTH_ROLLER_CHOICE) {
				if (current_menu->cursor) {
					/* Hacer una primera tirada de las estadísticas */
					cmdq_push(CMD_ROLL_STATS);
					next = current + 2;
				} else {
					/* 
					 * Asegurarse de que tenemos un personaje basado en puntos con el que jugar.
					 * Llamamos a point_based_start aquí para asegurarnos de obtener
					 * una actualización de los totales de puntos antes de intentar
					 * mostrar la pantalla. La llamada a CMD_RESET_STATS
					 * fuerza una recompra de las estadísticas para darnos totales
					 * actualizados. Esto es, no hace falta decirlo, un truco.
					 */
					point_based_start();
					cmdq_push(CMD_RESET_STATS);
					cmd_set_arg_choice(cmdq_peek(), "choice", true);
					next = current + 1;
				}
			} else {
				cmdq_push(choice_command);
				cmd_set_arg_choice(cmdq_peek(), "choice", current_menu->cursor);
				next = current + 1;
			}
		} else if (cx.type == EVT_SWITCH) {
			next = menu_data->stage_inout;
		} else if (cx.type == EVT_KBRD) {
			/* '*' elige una opción al azar de las proporcionadas por el juego */
			if (cx.key.code == '*' && menu_data->allow_random) {
				current_menu->cursor = randint0(current_menu->count);
				cmdq_push(choice_command);
				cmd_set_arg_choice(cmdq_peek(), "choice", current_menu->cursor);

				menu_refresh(current_menu, false);
				next = current + 1;
			} else if (cx.key.code == '=') {
				do_cmd_options_birth();
				next = current;
			} else if (cx.key.code == '@') {
				/*
				 * Usar elecciones aleatorias para completar el personaje.
				 */
				finish_with_random_choices(current);
				next = BIRTH_FINAL_CONFIRM;
			} else if (cx.key.code == KTRL('X')) {
				quit(NULL);
			} else if (cx.key.code == '?') {
				do_cmd_help();
			}
		}
	}
	
	return next;
}

/**
 * ------------------------------------------------------------------------
 * La parte de tirada del generador.
 * ------------------------------------------------------------------------ */
static enum birth_stage roller_command(bool first_call)
{
	enum {
		ACT_CTX_BIRTH_ROLL_NONE,
		ACT_CTX_BIRTH_ROLL_ESCAPE,
		ACT_CTX_BIRTH_ROLL_REROLL,
		ACT_CTX_BIRTH_ROLL_PREV,
		ACT_CTX_BIRTH_ROLL_ACCEPT,
		ACT_CTX_BIRTH_ROLL_QUIT,
		ACT_CTX_BIRTH_ROLL_HELP
	};
	char prompt[80] = "";
	size_t promptlen = 0;
	int action = ACT_CTX_BIRTH_ROLL_NONE;
	ui_event in;

	enum birth_stage next = BIRTH_ROLLER;

	/* Se usa para llevar la cuenta de si hemos tirado un personaje antes o no. */
	static bool prev_roll = false;

	/* Mostrar el jugador - un poco tramposo, pero no importa. */
	display_player(0);

	if (first_call)
		prev_roll = false;

	/* Preparar un mensaje (debe apretar todo) */
	strnfcat(prompt, sizeof (prompt), &promptlen, "['r' para tirar");
	if (prev_roll) 
		strnfcat(prompt, sizeof(prompt), &promptlen, ", 'p' tirada anterior");
	strnfcat(prompt, sizeof (prompt), &promptlen, " o 'Enter' para aceptar]");

	/* Preguntar por ello */
	prt(prompt, Term->hgt - 1, Term->wid / 2 - promptlen / 2);
	
	/*
	 * Obtener la respuesta. Emular lo que hace inkey() sin forzar eventos de ratón
	 * a parecer pulsaciones de teclas.
	 */
	while (1) {
		in = inkey_ex();
		if (in.type == EVT_KBRD || in.type == EVT_MOUSE) {
			break;
		}
		if (in.type == EVT_BUTTON) {
			in.type = EVT_KBRD;
			break;
		}
		if (in.type == EVT_ESCAPE) {
			in.type = EVT_KBRD;
			in.key.code = ESCAPE;
			in.key.mods = 0;
			break;
		}
	}

	/* Analizar el comando */
	if (in.type == EVT_KBRD) {
		if (in.key.code == ESCAPE) {
			action = ACT_CTX_BIRTH_ROLL_ESCAPE;
		} else if (in.key.code == KC_ENTER) {
			action = ACT_CTX_BIRTH_ROLL_ACCEPT;
		} else if (in.key.code == ' ' || in.key.code == 'r') {
			action = ACT_CTX_BIRTH_ROLL_REROLL;
		} else if (prev_roll && in.key.code == 'p') {
			action = ACT_CTX_BIRTH_ROLL_PREV;
		} else if (in.key.code == KTRL('X')) {
			action = ACT_CTX_BIRTH_ROLL_QUIT;
		} else if (in.key.code == '?') {
			action = ACT_CTX_BIRTH_ROLL_HELP;
		} else {
			/* Nada manejado directamente aquí */
			bell();
		}
	} else if (in.type == EVT_MOUSE) {
		if (in.mouse.button == 2) {
			action = ACT_CTX_BIRTH_ROLL_ESCAPE;
		} else {
			/* Presentar un menú contextual con las otras acciones. */
			char *labels = string_make(lower_case);
			struct menu *m = menu_dynamic_new();

			m->selections = labels;
			menu_dynamic_add_label(m, "Volver a tirar", 'r',
				ACT_CTX_BIRTH_ROLL_REROLL, labels);
			if (prev_roll) {
				menu_dynamic_add_label(m, "Recuperar anterior",
					'p', ACT_CTX_BIRTH_ROLL_PREV, labels);
			}
			menu_dynamic_add_label(m, "Aceptar", 'a',
				ACT_CTX_BIRTH_ROLL_ACCEPT, labels);
			menu_dynamic_add_label(m, "Salir", 'q',
				ACT_CTX_BIRTH_ROLL_QUIT, labels);
			menu_dynamic_add_label(m, "Ayuda", '?',
				ACT_CTX_BIRTH_ROLL_HELP, labels);

			screen_save();

			menu_dynamic_calc_location(m, in.mouse.x, in.mouse.y);
			region_erase_bordered(&m->boundary);

			action = menu_dynamic_select(m);

			menu_dynamic_free(m);
			string_free(labels);

			screen_load();
		}
	}

	switch (action) {
	case ACT_CTX_BIRTH_ROLL_ESCAPE:
		/* Retroceder a la etapa de nacimiento anterior. */
		next = BIRTH_BACK;
		break;

	case ACT_CTX_BIRTH_ROLL_REROLL:
		/* Volver a tirar este personaje. */
		cmdq_push(CMD_ROLL_STATS);
		prev_roll = true;
		break;

	case ACT_CTX_BIRTH_ROLL_PREV:
		/* Intercambiar con la tirada anterior. */
		cmdq_push(CMD_PREV_STATS);
		break;

	case ACT_CTX_BIRTH_ROLL_ACCEPT:
		/* Aceptar la tirada. Ir a la siguiente etapa. */
		next = BIRTH_NAME_CHOICE;
		break;

	case ACT_CTX_BIRTH_ROLL_QUIT:
		quit(NULL);
		break;

	case ACT_CTX_BIRTH_ROLL_HELP:
		do_cmd_help();
		break;
	}

	return next;
}

/**
 * ------------------------------------------------------------------------
 * Asignación de estadísticas basada en puntos.
 * ------------------------------------------------------------------------ */

/* Las ubicaciones del área de "costes" en la pantalla de nacimiento. */
#define COSTS_ROW 2
#define COSTS_COL (42 + 32)
#define TOTAL_COL (42 + 19)

/*
 * Recordar lo que es posible para una estadística dada. 0 significa que no se puede comprar ni vender.
 * 1 significa que se puede vender. 2 significa que se puede comprar. 3 significa que se puede comprar o vender.
 */
static int buysell[STAT_MAX];

/**
 * Esto se llama cada vez que cambia una estadística. Tomamos el camino fácil, y simplemente
 * las volvemos a mostrar todas usando la función estándar.
 */
static void point_based_stats(game_event_type type, game_event_data *data,
							  void *user)
{
	display_player_stat_info();
}

/**
 * Esto se llama cada vez que cambia cualquiera de las otras cosas misceláneas dependientes de estadísticas.
 * Estamos enganchados a cambios en la cantidad de oro en este caso,
 * pero lo volvemos a mostrar todo porque es más fácil.
 */
static void point_based_misc(game_event_type type, game_event_data *data,
							 void *user)
{
	display_player_xtra_info();
}


/**
 * Esto se llama cada vez que se cambian los totales de puntos (en birth.c), para
 * que podamos actualizar nuestra visualización de cuántos puntos se han gastado y
 * están disponibles.
 */
static void point_based_points(game_event_type type, game_event_data *data,
							   void *user)
{
	int i;
	int sum = 0;
	const int *spent = data->birthpoints.points;
	const int *inc = data->birthpoints.inc_points;
	int remaining = data->birthpoints.remaining;

	/* Mostrar el encabezado de costes */
	put_str("Coste", COSTS_ROW - 1, COSTS_COL);
	
	for (i = 0; i < STAT_MAX; i++) {
		/* Recordar lo que está permitido. */
		buysell[i] = 0;
		if (spent[i] > 0) {
			buysell[i] |= 1;
		}
		if (inc[i] <= remaining) {
			buysell[i] |= 2;
		}
		/* Mostrar coste */
		put_str(format("%4d", spent[i]), COSTS_ROW + i, COSTS_COL);
		sum += spent[i];
	}
	
	put_str(format("Coste Total: %2d/%2d", sum, remaining + sum),
		COSTS_ROW + STAT_MAX, TOTAL_COL);
}

static void point_based_start(void)
{
	const char *prompt = "[arriba/abajo mover, izq/der modificar, 'r' reiniciar, 'Enter' aceptar]";
	int i;

	/* Limpiar */
	Term_clear();

	/* Mostrar el jugador */
	display_player_xtra_info();
	display_player_stat_info();

	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	for (i = 0; i < STAT_MAX; ++i) {
		buysell[i] = 0;
	}

	/* Registrar manejadores para varios eventos - hacer trampa un poco porque redibujamos
	   todo de una vez en lugar de cada parte por separado. */
	event_add_handler(EVENT_BIRTHPOINTS, point_based_points, NULL);	
	event_add_handler(EVENT_STATS, point_based_stats, NULL);	
	event_add_handler(EVENT_GOLD, point_based_misc, NULL);	
}

static void point_based_stop(void)
{
	event_remove_handler(EVENT_BIRTHPOINTS, point_based_points, NULL);	
	event_remove_handler(EVENT_STATS, point_based_stats, NULL);	
	event_remove_handler(EVENT_GOLD, point_based_misc, NULL);	
}

static enum birth_stage point_based_command(void)
{
	static int stat = 0;
	enum {
		ACT_CTX_BIRTH_PTS_NONE,
		ACT_CTX_BIRTH_PTS_BUY,
		ACT_CTX_BIRTH_PTS_SELL,
		ACT_CTX_BIRTH_PTS_ESCAPE,
		ACT_CTX_BIRTH_PTS_RESET,
		ACT_CTX_BIRTH_PTS_ACCEPT,
		ACT_CTX_BIRTH_PTS_QUIT
	};
	int action = ACT_CTX_BIRTH_PTS_NONE;
	ui_event in;
	enum birth_stage next = BIRTH_POINTBASED;

	/* Colocar cursor justo después del coste de la estadística actual */
	Term_gotoxy(COSTS_COL + 4, COSTS_ROW + stat);

	/*
	 * Obtener entrada. Emular lo que hace inkey() sin forzar eventos de ratón
	 * a parecer pulsaciones de teclas.
	 */
	while (1) {
		in = inkey_ex();
		if (in.type == EVT_KBRD || in.type == EVT_MOUSE) {
			break;
		}
		if (in.type == EVT_BUTTON) {
			in.type = EVT_KBRD;
		}
		if (in.type == EVT_ESCAPE) {
			in.type = EVT_KBRD;
			in.key.code = ESCAPE;
			in.key.mods = 0;
			break;
		}
	}

	/* Determinar qué hacer. */
	if (in.type == EVT_KBRD) {
		if (in.key.code == KTRL('X')) {
			action = ACT_CTX_BIRTH_PTS_QUIT;
		} else if (in.key.code == ESCAPE) {
			action = ACT_CTX_BIRTH_PTS_ESCAPE;
		} else if (in.key.code == 'r' || in.key.code == 'R') {
			action = ACT_CTX_BIRTH_PTS_RESET;
		} else if (in.key.code == KC_ENTER) {
			action = ACT_CTX_BIRTH_PTS_ACCEPT;
		} else {
			int dir;

			if (in.key.code == '-') {
				dir = 4;
			} else if (in.key.code == '+') {
				dir = 6;
			} else {
				dir = target_dir(in.key);
			}

			/*
			 * Ir a la estadística anterior. Volver a la última si estamos
			 * en la primera.
			 */
			if (dir == 8) {
				stat = (stat + STAT_MAX - 1) % STAT_MAX;
			}

			/*
			 * Ir a la siguiente estadística. Volver a la primera si estamos en la
			 * última.
			 */
			if (dir == 2) {
				stat = (stat + 1) % STAT_MAX;
			}

			/* Disminuir estadística (si es posible). */
			if (dir == 4) {
				action = ACT_CTX_BIRTH_PTS_SELL;
			}

			/* Aumentar estadística (si es posible). */
			if (dir == 6) {
				action = ACT_CTX_BIRTH_PTS_BUY;
			}
		}
	} else if (in.type == EVT_MOUSE) {
		assert(stat >= 0 && stat < STAT_MAX);
		if (in.mouse.button == 2) {
			action = ACT_CTX_BIRTH_PTS_ESCAPE;
		} else if (in.mouse.y >= COSTS_ROW
				&& in.mouse.y < COSTS_ROW + STAT_MAX
				&& in.mouse.y != COSTS_ROW + stat) {
			/*
			 * Hacer que esa estadística sea la actual si se está comprando o vendiendo.
			 */
			stat = in.mouse.y - COSTS_ROW;
		} else {
			/* Presentar un menú contextual con las otras acciones. */
			char *labels = string_make(lower_case);
			struct menu *m = menu_dynamic_new();

			m->selections = labels;
			if (in.mouse.y == COSTS_ROW + stat
					&& (buysell[stat] & 1)) {
				menu_dynamic_add_label(m, "Vender", 's',
					ACT_CTX_BIRTH_PTS_SELL, labels);
			}
			if (in.mouse.y == COSTS_ROW + stat
					&& (buysell[stat] & 2)) {
				menu_dynamic_add_label(m, "Comprar", 'b',
					ACT_CTX_BIRTH_PTS_BUY, labels);
			}
			menu_dynamic_add_label(m, "Aceptar", 'a',
				ACT_CTX_BIRTH_PTS_ACCEPT, labels);
			menu_dynamic_add_label(m, "Reiniciar", 'r',
				ACT_CTX_BIRTH_PTS_RESET, labels);
			menu_dynamic_add_label(m, "Salir", 'q',
				ACT_CTX_BIRTH_PTS_QUIT, labels);

			screen_save();

			menu_dynamic_calc_location(m, in.mouse.x, in.mouse.y);
			region_erase_bordered(&m->boundary);

			action = menu_dynamic_select(m);

			menu_dynamic_free(m);
			string_free(labels);

			screen_load();
		}
	}

	/* Hacerlo. */
	switch (action) {
	case ACT_CTX_BIRTH_PTS_SELL:
		assert(stat >= 0 && stat < STAT_MAX);
		cmdq_push(CMD_SELL_STAT);
		cmd_set_arg_choice(cmdq_peek(), "choice", stat);
		break;

	case ACT_CTX_BIRTH_PTS_BUY:
		assert(stat >= 0 && stat < STAT_MAX);
		cmdq_push(CMD_BUY_STAT);
		cmd_set_arg_choice(cmdq_peek(), "choice", stat);
		break;

	case ACT_CTX_BIRTH_PTS_ESCAPE:
		/* Retroceder un paso o volver al inicio de este paso. */
		next = BIRTH_BACK;
		break;

	case ACT_CTX_BIRTH_PTS_RESET:
		cmdq_push(CMD_RESET_STATS);
		cmd_set_arg_choice(cmdq_peek(), "choice", false);
		break;

	case ACT_CTX_BIRTH_PTS_ACCEPT:
		/* Terminado con esta etapa. Proceder a la siguiente. */
		next = BIRTH_NAME_CHOICE;
		break;

	case ACT_CTX_BIRTH_PTS_QUIT:
		quit(NULL);
		break;

	default:
		/* No hacer nada y permanecer en esta etapa. */
		break;
	}

	return next;
}
	
/**
 * ------------------------------------------------------------------------
 * Preguntar por el nombre elegido por el jugador.
 * ------------------------------------------------------------------------ */
//cambios fantasma para el servidor
static enum birth_stage get_name_command(void)
{
	enum birth_stage next;
	char name[PLAYER_NAME_LEN];

	/* Usar el nombre de archivo guardado proporcionado por el frontend si se solicita */
	if (arg_name[0]) {
		my_strcpy(player->full_name, arg_name, sizeof(player->full_name));
	}

	/*
	 * Si no se fuerza el nombre del personaje, el frontend no estableció el
	 * archivo guardado a usar, y el nombre elegido para el personaje llevaría
	 * a sobrescribir un archivo guardado existente, confirmar que está bien con el
	 * jugador.
	 */
	if (arg_force_name) {
		next = BIRTH_HISTORY_CHOICE;
	} else if (get_character_name(name, sizeof(name))
			&& (savefile[0]
			|| !savefile_name_already_used(name, true, true)
			|| get_check("¿Ya existe un archivo guardado para ese nombre. ¿Sobrescribirlo? "))) {
		cmdq_push(CMD_NAME_CHOICE);
		cmd_set_arg_string(cmdq_peek(), "name", name);
		next = BIRTH_HISTORY_CHOICE;
	} else {
		next = BIRTH_BACK;
	}

	
	return next;
}

static void get_screen_loc(size_t cursor, int *x, int *y, size_t n_lines,
	size_t *line_starts, size_t *line_lengths)
{
	size_t lengths_so_far = 0;
	size_t i;

	if (!line_starts || !line_lengths) return;

	for (i = 0; i < n_lines; i++) {
		if (cursor >= line_starts[i]) {
			if (cursor <= (line_starts[i] + line_lengths[i])) {
				*y = i;
				*x = cursor - lengths_so_far;
				break;
			}
		}
		/* +1 por el espacio */
		lengths_so_far += line_lengths[i] + 1;
	}
}

static int edit_text(char *buffer, int buflen) {
	int len = strlen(buffer);
	bool done = false;
	int cursor = 0;

	while (!done) {
		int x = 0, y = 0;
		struct keypress ke;

		region area = { 1, HIST_INSTRUCT_ROW + 1, 71, 5 };
		textblock *tb = textblock_new();

		size_t *line_starts = NULL, *line_lengths = NULL;
		size_t n_lines;
		/*
		 * Este es el número total de caracteres UTF-8; puede ser menor
		 * que len, el número de unidades de 8 bits en el búfer,
		 * si un solo carácter se codifica con más de una unidad de 8 bits.
		 */
		int ulen;

		/* Mostrar en pantalla */
		clear_from(HIST_INSTRUCT_ROW);
		textblock_append(tb, "%s", buffer);
		textui_textblock_place(tb, area, NULL);

		n_lines = textblock_calculate_lines(tb,
				&line_starts, &line_lengths, area.width);
		ulen = (n_lines > 0) ? line_starts[n_lines - 1] +
			line_lengths[n_lines - 1]: 0;

		/* Establecer cursor a la posición de edición actual */
		get_screen_loc(cursor, &x, &y, n_lines, line_starts, line_lengths);
		Term_gotoxy(1 + x, 19 + y);

		ke = inkey();
		switch (ke.code) {
			case ESCAPE:
				return -1;

			case KC_ENTER:
				done = true;
				break;

			case ARROW_LEFT:
				if (cursor > 0) cursor--;
				break;

			case ARROW_RIGHT:
				if (cursor < ulen) cursor++;
				break;

			case ARROW_DOWN: {
				int add = line_lengths[y] + 1;
				if (cursor + add < ulen) cursor += add;
				break;
			}

			case ARROW_UP:
				if (y > 0) {
					int up = line_lengths[y - 1] + 1;
					if (cursor - up >= 0) cursor -= up;
				}
				break;

			case KC_END:
				cursor = MAX(0, ulen);
				break;

			case KC_HOME:
				cursor = 0;
				break;

			case KC_BACKSPACE:
			case KC_DELETE: {
				char *ocurs, *oshift;

				/* Rechazar retroceder hacia la nada */
				if ((ke.code == KC_BACKSPACE && cursor == 0) ||
						(ke.code == KC_DELETE && cursor >= ulen))
					break;

				/*
				 * Mover la cadena desde k hasta nulo hacia la
				 * izquierda en 1. Primero, hay que obtener el desplazamiento
				 * correspondiente a la posición del cursor.
				 */
				ocurs = utf8_fskip(buffer, cursor, NULL);
				assert(ocurs);
				if (ke.code == KC_BACKSPACE) {
					/* Obtener desplazamiento del carácter anterior. */
					oshift = utf8_rskip(ocurs, 1, buffer);
					assert(oshift);
					memmove(oshift, ocurs,
						len - (ocurs - buffer));
					/* Decrementar */
					--cursor;
					len -= ocurs - oshift;
				} else {
					/* Obtener desplazamiento del siguiente carácter. */
					oshift = utf8_fskip(ocurs, 1, NULL);
					assert(oshift);
					memmove(ocurs, oshift,
						len - (oshift - buffer));
					/* Decrementar. */
					len -= oshift - ocurs;
				}

				/* Terminar */
				buffer[len] = '\0';

				break;
			}
			
			default: {
				bool atnull = (cursor == ulen);
				char encoded[5];
				int n_enc;
				char *ocurs;

				if (!keycode_isprint(ke.code))
					break;

				n_enc = utf32_to_utf8(encoded,
					N_ELEMENTS(encoded), &ke.code, 1, NULL);

				/*
				 * Asegurarse de que tenemos algo que añadir y tenemos
				 * suficiente espacio.
				 */
				if (n_enc == 0 || n_enc + len >= buflen) {
					break;
				}

				/* Insertar el carácter codificado. */
				if (atnull) {
					ocurs = buffer + len;
				} else {
					ocurs = utf8_fskip(buffer, cursor, NULL);
					assert(ocurs);
					/*
					 * Mover el resto del búfer hacia adelante
					 * para hacer espacio.
					 */
					memmove(ocurs + n_enc, ocurs,
						len - (ocurs - buffer));
				}
				memcpy(ocurs, encoded, n_enc);

				/* Actualizar posición del cursor y longitud. */
				++cursor;
				len += n_enc;

				/* Terminar */
				buffer[len] = '\0';

				break;
			}
		}

		mem_free(line_starts);
		mem_free(line_lengths);
		textblock_free(tb);
	}

	return 0;
}

/**
 * ------------------------------------------------------------------------
 * Permitir al jugador elegir su historia.
 * ------------------------------------------------------------------------ */
static enum birth_stage get_history_command(void)
{
	enum birth_stage next = 0;
	struct keypress ke;
	char old_history[240];

	/* Guardar la historia original */
	my_strcpy(old_history, player->history, sizeof(old_history));

	/* Preguntar por alguna historia */
	prt("¿Aceptar la historia del personaje? [s/n]", 0, 0);
	ke = inkey();

	/* Salir, retroceder, cambiar historia, o aceptar */
	if (ke.code == KTRL('X')) {
		quit(NULL);
	} else if (ke.code == ESCAPE) {
		next = BIRTH_BACK;
	} else if (ke.code == 'N' || ke.code == 'n') {
		char history[240];
		my_strcpy(history, player->history, sizeof(history));

		switch (edit_text(history, sizeof(history))) {
			case -1:
				next = BIRTH_BACK;
				break;
			case 0:
				cmdq_push(CMD_HISTORY_CHOICE);
				cmd_set_arg_string(cmdq_peek(), "history", history);
				next = BIRTH_HISTORY_CHOICE;
		}
	} else {
		next = BIRTH_FINAL_CONFIRM;
	}

	return next;
}

/**
 * ------------------------------------------------------------------------
 * Confirmación final del personaje.
 * ------------------------------------------------------------------------ */
static enum birth_stage get_confirm_command(void)
{
	const char *prompt = "['ESC' retroceder, 'S' empezar de nuevo, otra tecla para continuar]";
	struct keypress ke;

	enum birth_stage next = BIRTH_RESET;

	/* Preguntar por ello */
	prt(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

	/* Obtener una tecla */
	ke = inkey();
	
	/* Empezar de nuevo */
	if (ke.code == 'S' || ke.code == 's') {
		next = BIRTH_RESET;
	} else if (ke.code == KTRL('X')) {
		quit(NULL);
	} else if (ke.code == ESCAPE) {
		next = BIRTH_BACK;
	} else {
		cmdq_push(CMD_ACCEPT_CHARACTER);
		next = BIRTH_COMPLETE;
	}

	/* Limpiar mensaje */
	clear_from(23);

	return next;
}



/**
 * ------------------------------------------------------------------------
 * Cosas que se relacionan con el mundo fuera de este archivo: recibir eventos del juego
 * y que se nos pidan comandos del juego.
 * ------------------------------------------------------------------------ */

/**
 * Esto se llama cuando recibimos una solicitud de un comando en el proceso de nacimiento.

 * El proceso de nacimiento continúa hasta que enviamos un comando de confirmación final
 * del personaje (o salimos), así que esto se llama efectivamente en un bucle por el juego
 * principal.
 *
 * Estamos imponiendo un sistema basado en pasos al juego principal aquí, así que necesitamos
 * llevar la cuenta de dónde estamos, a dónde lleva cada paso, etc.
 */
int textui_do_birth(void)
{
	enum birth_stage current_stage = BIRTH_RESET;
	enum birth_stage prev = BIRTH_BACK;
	enum birth_stage roller = BIRTH_RESET;
	enum birth_stage next = current_stage;

	bool done = false;

	cmdq_push(CMD_BIRTH_INIT);
	cmdq_execute(CTX_BIRTH);

	while (!done) {

		switch (current_stage)
		{
			case BIRTH_RESET:
			{
				cmdq_push(CMD_BIRTH_RESET);

				roller = BIRTH_RESET;
				
				if (quickstart_allowed)
					next = BIRTH_QUICKSTART;
				else
					next = BIRTH_RACE_CHOICE;

				break;
			}

			case BIRTH_QUICKSTART:
			{
				display_player(0);
				next = textui_birth_quickstart();
				if (next == BIRTH_COMPLETE)
					done = true;
				break;
			}

			case BIRTH_CLASS_CHOICE:
			case BIRTH_RACE_CHOICE:
			case BIRTH_ROLLER_CHOICE:
			{
				struct menu *menu = &race_menu;
				cmd_code command = CMD_CHOOSE_RACE;

				Term_clear();
				print_menu_instructions();

				if (current_stage > BIRTH_RACE_CHOICE) {
					menu_refresh(&race_menu, false);
					menu = &class_menu;
					command = CMD_CHOOSE_CLASS;
				}

				if (current_stage > BIRTH_CLASS_CHOICE) {
					menu_refresh(&class_menu, false);
					menu = &roller_menu;
				}

				next = menu_question(current_stage, menu, command);

				if (next == BIRTH_BACK)
					next = current_stage - 1;

				/* Asegurarse de que el personaje se reinicia antes del inicio rápido */
				if (next == BIRTH_QUICKSTART) 
					next = BIRTH_RESET;

				break;
			}

			case BIRTH_POINTBASED:
			{
				roller = BIRTH_POINTBASED;
		
				if (prev > BIRTH_POINTBASED) {
					point_based_start();
					/*
					 * Forzar un redibujado de las asignaciones de puntos
					 * pero no reiniciarlas.
					 */
					cmdq_push(CMD_REFRESH_STATS);
					cmdq_execute(CTX_BIRTH);
				}

				next = point_based_command();

				if (next == BIRTH_BACK)
					next = BIRTH_ROLLER_CHOICE;

				if (next != BIRTH_POINTBASED)
					point_based_stop();

				break;
			}

			case BIRTH_ROLLER:
			{
				roller = BIRTH_ROLLER;
				next = roller_command(prev < BIRTH_ROLLER);
				if (next == BIRTH_BACK)
					next = BIRTH_ROLLER_CHOICE;

				break;
			}

			case BIRTH_NAME_CHOICE:
			{
				if (prev < BIRTH_NAME_CHOICE)
					display_player(0);

				next = get_name_command();
				if (next == BIRTH_BACK)
					next = roller;

				break;
			}

			case BIRTH_HISTORY_CHOICE:
			{
				if (prev < BIRTH_HISTORY_CHOICE)
					display_player(0);

				next = get_history_command();
				if (next == BIRTH_BACK)
					next = BIRTH_NAME_CHOICE;

				break;
			}

			case BIRTH_FINAL_CONFIRM:
			{
				if (prev < BIRTH_FINAL_CONFIRM)
					display_player(0);

				next = get_confirm_command();
				if (next == BIRTH_BACK)
					next = BIRTH_HISTORY_CHOICE;

				if (next == BIRTH_COMPLETE)
					done = true;

				break;
			}

			default:
			{
				/* Eliminar advertencia de compilador, */
			}
		}

		prev = current_stage;
		current_stage = next;

		/* Ejecutar los comandos que se hayan enviado */
		cmdq_execute(CTX_BIRTH);
	}

	return 0;
}

/**
 * Llamado cuando entramos en el modo de nacimiento - así que configuramos manejadores, ganchos de comando,
 * etc., aquí.
 */
static void ui_enter_birthscreen(game_event_type type, game_event_data *data,
								 void *user)
{
	/* Establecer el feo global estático que nos dice si el inicio rápido está disponible. */
	quickstart_allowed = data->flag;

	setup_menus();
}

static void ui_leave_birthscreen(game_event_type type, game_event_data *data,
								 void *user)
{
	/* Establecer el nombre del archivo guardado si aún no está establecido */
	if (!savefile[0])
		savefile_set_name(player->full_name, true, true);

	free_birth_menus();
}


void ui_init_birthstate_handlers(void)
{
	event_add_handler(EVENT_ENTER_BIRTH, ui_enter_birthscreen, NULL);
	event_add_handler(EVENT_LEAVE_BIRTH, ui_leave_birthscreen, NULL);
}