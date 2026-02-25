/**
 * \file ui-input.c
 * \brief Algunas funciones de interfaz de alto nivel, inkey()
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
#include "game-event.h"
#include "game-input.h"
#include "game-world.h"
#include "init.h"
#include "obj-gear.h"
#include "obj-util.h"
#include "player-calcs.h"
#include "player-path.h"
#include "savefile.h"
#include "target.h"
#include "ui-birth.h"
#include "ui-command.h"
#include "ui-context.h"
#include "ui-curse.h"
#include "ui-display.h"
#include "ui-effect.h"
#include "ui-help.h"
#include "ui-keymap.h"
#include "ui-knowledge.h"
#include "ui-map.h"
#include "ui-menu.h"
#include "ui-object.h"
#include "ui-output.h"
#include "ui-player-properties.h"
#include "ui-player.h"
#include "ui-prefs.h"
#include "ui-signals.h"
#include "ui-spell.h"
#include "ui-store.h"
#include "ui-target.h"

static bool inkey_xtra;
uint32_t inkey_scan;		/* Ver la función "inkey()" */
bool inkey_flag;		/* Ver la función "inkey()" */

/**
 * Vaciar toda la entrada pendiente.
 *
 * En realidad, recordar el vaciado, usando la bandera "inkey_xtra", y en la
 * siguiente llamada a "inkey()", realizar el vaciado real, por eficiencia,
 * y corrección de la función "inkey()".
 */
void flush(game_event_type unused, game_event_data *data, void *user)
{
	/* Hacerlo más tarde */
	inkey_xtra = true;
}


/**
 * Función auxiliar llamada solo desde "inkey()"
 */
static ui_event inkey_aux(int scan_cutoff)
{
	int w = 0;	

	ui_event ke;
	
	/* Esperar una pulsación de tecla */
	if (scan_cutoff == SCAN_OFF) {
		(void)(Term_inkey(&ke, true, true));
	} else {
		w = 0;

		/* Esperar solo el tiempo que esperaría la activación de macro */
		while (Term_inkey(&ke, false, true) != 0) {
			/* Aumentar "espera" */
			w++;

			/* Demora excesiva */
			if (w >= scan_cutoff) {
				ui_event empty = EVENT_EMPTY;
				return empty;
			}

			/* Demora */
			Term_xtra(TERM_XTRA_DELAY, 10);
		}
	}

	return (ke);
}



/**
 * Mega-Truco -- puntero especial "inkey_next".  XXX XXX XXX
 *
 * Este puntero especial permite que una secuencia de teclas sea "insertada" en
 * el flujo de teclas devueltas por "inkey()". Esta secuencia de teclas no puede ser
 * omitida por el Borg. Lo usamos para implementar mapas de teclas.
 */
struct keypress *inkey_next = NULL;

/**
 * Ver si se omitirán más mensajes mientras se está en un mapa de teclas.
 */
static bool keymap_auto_more;

#ifdef ALLOW_BORG

/*
 * Mega-Truco -- gancho especial "inkey_hack".  XXX XXX XXX
 *
 * Este gancho de función especial permite que el "Borg" (ver en otra parte) tome
 * el control de la función "inkey()" y sustituya pulsaciones de teclas falsas.
 */
struct keypress(*inkey_hack)(int flush_first) = NULL;

#endif /* ALLOW_BORG */

/**
 * Obtener una pulsación de tecla del usuario.
 *
 * Esta función reconoce algunos "parámetros globales". Estas son variables
 * que, si se establecen a true antes de llamar a esta función, tendrán un efecto
 * en esta función, y que siempre se restablecen a false por esta función
 * antes de que esta función regrese. Por lo tanto, funcionan como parámetros
 * normales, excepto que la mayoría de las llamadas a esta función pueden ignorarlos.
 *
 * Si "inkey_xtra" es true, entonces se vaciarán todas las pulsaciones de tecla pendientes.
 * Esto es establecido por flush(), que en realidad no vacía nada por sí mismo
 * pero usa esa bandera para desencadenar un vaciado retrasado.
 *
 * Si "inkey_scan" es true, entonces devolveremos inmediatamente "cero" si no hay
 * una pulsación de tecla disponible, en lugar de esperar una.
 *
 * Si "inkey_flag" es true, entonces estamos esperando un comando en la interfaz
 * del mapa principal, y no deberíamos mostrar un cursor.
 *
 * Si estamos esperando una pulsación de tecla, y ninguna está lista, entonces
 * refrescaremos (una vez) la ventana que estaba activa cuando se llamó a esta función.
 *
 * Nótese que "back-quote" se convierte automáticamente en "escape" por
 * conveniencia en máquinas sin tecla "escape".
 *
 * Si "angband_term[0]" no está activo, lo activaremos durante esta
 * función, para que los diversos archivos "main-xxx.c" puedan asumir que la entrada
 * solo se solicita (a través de "Term_inkey()") cuando "angband_term[0]" está activo.
 *
 * Mega-Truco -- Esta función se usa como punto de entrada para limpiar la
 * variable "signal_count", y la variable "character_saved".
 *
 * Mega-Truco -- Nótese el uso de "inkey_hack" para permitir que el "Borg" robe
 * el control del teclado del usuario.
 */
ui_event inkey_ex(void)
{
	bool cursor_state;
	ui_event kk;
	ui_event ke = EVENT_EMPTY;

	bool done = false;

	term *old = Term;

	/* Vacío retrasado */
	if (inkey_xtra) {
		Term_flush();
		inkey_next = NULL;
		inkey_xtra = false;
	}

	/* Usar el puntero "inkey_next" */
	while (inkey_next && inkey_next->code) {
		/* Obtener el siguiente carácter y avanzar */
		ke.key = *inkey_next++;

		/* Cancelar los diversos "parámetros globales" */
		inkey_flag = false;
		inkey_scan = 0;

		/* Echar un vistazo a la tecla, y ver si queremos omitir más mensajes */
		if (ke.key.code == '(') {
			keymap_auto_more = true;
			/* Como no estamos devolviendo este carácter, asegurarnos de que la
			 * siguiente tecla funcione bien */
			if (!inkey_next || !inkey_next->code) {
				ke.type = EVT_NONE;
				break;
			}
			continue;
		} else if (ke.key.code == ')') {
			keymap_auto_more = false;
			/* Como no estamos devolviendo este carácter, asegurarnos de que la
			 * siguiente tecla funcione bien */
			if (!inkey_next || !inkey_next->code) {
				ke.type = EVT_NONE;
				break;
			}
			continue;
		}

		/* Aceptar resultado */
		return (ke);
	}

	/* asegurarse de que la bandera para omitir más mensajes está desactivada */
	keymap_auto_more = false;

	/* Olvidar puntero */
	inkey_next = NULL;

#ifdef ALLOW_BORG
	/* Mega-Truco -- Usar el gancho especial */
	if (inkey_hack)
	{
		ke.key = (*inkey_hack)(inkey_xtra);
		if (ke.key.type != EVT_NONE)
		{
			/* Cancelar los diversos "parámetros globales" */
			inkey_flag = false;
			inkey_scan = 0;
			ke.type = EVT_KBRD;

			/* Aceptar resultado */
			return (ke);
		}
	}
#endif /* ALLOW_BORG */

	/* Obtener el estado del cursor */
	(void)Term_get_cursor(&cursor_state);

	/* Mostrar el cursor si está esperando, excepto a veces en modo "comando" */
	if (!inkey_scan && (!inkey_flag || screen_save_depth ||
						(OPT(player, show_target) && target_sighted())))
		(void)Term_set_cursor(true);


	/* Activar pantalla principal */
	Term_activate(term_screen);


	/* Obtener una tecla */
	while (ke.type == EVT_NONE) {
		/* Manejar "inkey_scan == SCAN_INSTANT */
		if (inkey_scan == SCAN_INSTANT &&
			(0 != Term_inkey(&kk, false, false)))
			break;


		/* Vaciar salida una vez cuando ninguna tecla está lista */
		if (!done && (0 != Term_inkey(&kk, false, false))) {
			/* Activar terminal adecuado */
			Term_activate(old);

			/* Vaciar salida */
			Term_fresh();

			/* Activar pantalla principal */
			Term_activate(term_screen);

			/* Mega-Truco -- restablecer bandera guardada */
			character_saved = false;

			/* Mega-Truco -- restablecer contador de señal */
			signal_count = 0;

			/* Solo una vez */
			done = true;
		}


		/* Obtener una tecla (ver arriba) */
		ke = inkey_aux(inkey_scan);

		if (inkey_scan && ke.type == EVT_NONE)
			/* La pulsación de tecla expiró. Necesitamos detenernos aquí. */
			break;

		/* Tratar back-quote como escape */
		if (ke.key.code == '`')
			ke.key.code = ESCAPE;
	}

	/* Restaurar la terminal */
	Term_activate(old);

	/* Restaurar el cursor */
	Term_set_cursor(cursor_state);

	/* Cancelar los diversos "parámetros globales" */
	inkey_flag = false;
	inkey_scan = 0;

	/* Devolver la pulsación de tecla */
	return (ke);
}


/**
 * Obtener una pulsación de tecla o clic del ratón del usuario e ignorarlo.
 */
void anykey(void)
{
	ui_event ke = EVENT_EMPTY;
  
	/* Solo aceptar una pulsación de tecla o clic de ratón */
	while (ke.type != EVT_MOUSE && ke.type != EVT_KBRD)
		ke = inkey_ex();
}

/**
 * Obtener una "pulsación de tecla" del usuario.
 */
struct keypress inkey(void)
{
	ui_event ke = EVENT_EMPTY;

	while (ke.type != EVT_ESCAPE && ke.type != EVT_KBRD &&
		   ke.type != EVT_MOUSE && ke.type != EVT_BUTTON)
		ke = inkey_ex();

	/* Hacer que el evento sea una pulsación de tecla */
	if (ke.type == EVT_ESCAPE) {
		ke.type = EVT_KBRD;
		ke.key.code = ESCAPE;
		ke.key.mods = 0;
	} else if (ke.type == EVT_MOUSE) {
		if (ke.mouse.button == 1) {
			ke.type = EVT_KBRD;
			ke.key.code = '\n';
			ke.key.mods = 0;
		} else {
			ke.type = EVT_KBRD;
			ke.key.code = ESCAPE;
			ke.key.mods = 0;
		}
	} else if (ke.type == EVT_BUTTON) {
		ke.type = EVT_KBRD;
	}

	return ke.key;
}

/**
 * Obtener una "pulsación de tecla" o una "pulsación de ratón" del usuario.
 * al regresar, el evento debe ser una pulsación de tecla o una pulsación de ratón
 */
ui_event inkey_m(void)
{
	ui_event ke = EVENT_EMPTY;

	/* Solo aceptar una pulsación de tecla */
	while (ke.type != EVT_ESCAPE && ke.type != EVT_KBRD	&&
		   ke.type != EVT_MOUSE  && ke.type != EVT_BUTTON)
		ke = inkey_ex();
	if (ke.type == EVT_ESCAPE) {
		ke.type = EVT_KBRD;
		ke.key.code = ESCAPE;
		ke.key.mods = 0;
	} else if (ke.type == EVT_BUTTON) {
		ke.type = EVT_KBRD;
	}

  return ke;
}



/**
 * Vaciar
 */
static void msg_flush(int x)
{
	uint8_t a = COLOUR_L_BLUE;

	/* Pausa para respuesta */
	Term_putstr(x, 0, -1, a, "-más-");

	if ((!OPT(player, auto_more)) && !keymap_auto_more)
		anykey();

	/* Limpiar la línea */
	Term_erase(0, 0, 255);
}

/**
 * Como msg_flush() pero dividir lo que ya se ha enviado al búfer del Terminal
 * para hacer espacio para el mensaje "-más-".
 *
 * \param w es el número de columnas en la terminal
 * \param x apunta al entero que almacena la columna donde comenzará el siguiente
 * mensaje.
 */
static void msg_flush_split_existing(int w, int *x)
{
	/* Lugar por defecto para dividir lo que hay */
	int split = MIN(*x, w - 8);
	int i = split;
	wchar_t *svc = NULL;
	int *sva = NULL;

	/* Encontrar el punto de división más a la derecha. */
	while (i > w / 2) {
		int a;
		wchar_t c;

		--i;
		Term_what(i, 0, &a, &c);
		if (c == L' ') {
			split = i;
			break;
		}
	}

	/* Recordar lo que está en y después del punto de división. */
	*x -= split;
	if (*x > 0) {
		svc = mem_alloc(*x * sizeof(*svc));
		sva = mem_alloc(*x * sizeof(*sva));
		for (i = 0; i < *x; ++i) {
			Term_what(i + split, 0, &sva[i], &svc[i]);
		}
	}

	Term_erase(split, 0, w);
	msg_flush(split + 1);

	/* Volver a poner lo que se recordó. */
	if (*x > 0) {
		for (i = 0; i < *x; ++i) {
			Term_putch(i, 0, sva[i], svc[i]);
		}
		mem_free(sva);
		mem_free(svc);
	}
}

static int message_column = 0;


/**
 * El jugador tiene un mensaje pendiente
 */
bool msg_flag;

/**
 * Mostrar un mensaje en la línea superior de la pantalla.
 *
 * Dividir mensajes largos en múltiples piezas (40-72 caracteres).
 *
 * Permitir que múltiples mensajes cortos "compartan" la línea superior.
 *
 * Preguntar al usuario para asegurarse de que tiene la oportunidad de leerlos.
 *
 * Estos mensajes se memorizan para referencia posterior (ver arriba).
 *
 * Podríamos hacer un "Term_fresh()" para proporcionar "parpadeo" si es necesario.
 *
 * La variable global "msg_flag" se puede limpiar para decirnos que "borremos" cualquier
 * mensaje "pendiente" que aún esté en la pantalla, en lugar de usar "msg_flush()".
 * Esto solo debe hacerse cuando se sabe que el usuario ha leído el mensaje.
 *
 * Debemos tener mucho cuidado al usar las funciones "msg("%s", )" sin
 * llamar explícitamente a la función especial "msg("%s", NULL)", ya que esto puede
 * resultar en la pérdida de información si la pantalla se limpia, o si cualquier cosa
 * se muestra en la línea superior.
 *
 * Nótese que "msg("%s", NULL)" limpiará la línea superior incluso si no hay
 * mensajes pendientes.
 */
void display_message(game_event_type unused, game_event_data *data, void *user)
{
	int n;
	char *t;
	char buf[1024];
	uint8_t color;
	int w, h;

	int type;
	const char *msg;

	if (!data) return;

	type = data->message.type;
	msg = data->message.msg;

	if (Term && type == MSG_BELL) {
		Term_xtra(TERM_XTRA_NOISE, 0);
		return;
	}

	if (!msg || !Term || !character_generated)
		return;

	/* Obtener el tamaño */
	(void)Term_get_size(&w, &h);

	/* Reiniciar */
	if (!msg_flag) message_column = 0;

	/* Longitud del mensaje */
	n = (msg ? strlen(msg) : 0);

	/* Vaciar cuando se solicite o sea necesario */
	if (message_column && (!msg || ((message_column + n) > (w - 8)))) {
		/* Vaciar */
		if (message_column <= w - 8) {
			msg_flush(message_column);
			message_column = 0;
		} else {
			msg_flush_split_existing(w, &message_column);
		}

		/* Olvidarlo */
		msg_flag = false;
	}

	/* Sin mensaje */
	if (!msg) return;

	/* Paranoia */
	if (n > 1000) return;

	/* Copiarlo */
	my_strcpy(buf, msg, sizeof(buf));

	/* Analizar el búfer */
	t = buf;

	/* Obtener el color del mensaje */
	color = message_type_color(type);

	/* Dividir mensaje */
	while (message_column + n > w - 1) {
		/* División por defecto */
		int split = MAX(w - 8 - message_column, 0);
		int check = split;
		char oops;

		/* Encontrar el punto de división más a la derecha */
		while (check > MAX(w / 2 - message_column, 0)) {
			--check;
			if (t[check] == ' ') {
				split = check;
				break;
			}
		}

		/* Guardar el carácter de división */
		oops = t[split];

		/* Dividir el mensaje */
		t[split] = '\0';

		/* Mostrar parte del mensaje */
		Term_putstr(message_column, 0, split, color, t);

		/* Vaciar */
		msg_flush(message_column + split + 1);

		/* Restaurar el carácter de división */
		t[split] = oops;

		/* Insertar un espacio */
		t[--split] = ' ';

		/* Prepararse para recurrir en el resto de "buf" */
		t += split; n -= split; message_column = 0;
	}

	/* Mostrar la cola del mensaje */
	Term_putstr(message_column, 0, n, color, t);

	/* Recordar el mensaje */
	msg_flag = true;

	/* Recordar la posición */
	message_column += n + 1;
}

/**
 * Vaciar la salida antes de mostrar para dar énfasis
 */
void bell_message(game_event_type unused, game_event_data *data, void *user)
{
	/* Vaciar la salida */
	Term_fresh();

	display_message(unused, data, user);
	player->upkeep->redraw |= PR_MESSAGE;
}

/**
 * Imprimir los mensajes en cola.
 */
void message_flush(game_event_type unused, game_event_data *data, void *user)
{
	/* Reiniciar */
	if (!msg_flag) message_column = 0;

	/* Vaciar cuando sea necesario */
	if (message_column) {
		/* Imprimir mensajes pendientes */
		if (Term) {
			int w, h;

			(void)Term_get_size(&w, &h);
			while (message_column > w - 8) {
				msg_flush_split_existing(w, &message_column);
			}
			if (message_column) {
				msg_flush(message_column);
			}
		}

		/* Olvidarlo */
		msg_flag = false;

		/* Reiniciar */
		message_column = 0;
	}
}


/**
 * Limpiar la parte inferior de la pantalla
 */
void clear_from(int row)
{
	int y;

	/* Borrar filas solicitadas */
	for (y = row; y < Term->hgt; y++)
		Term_erase(0, y, 255);
}

/**
 * La función de "manejo de pulsaciones de tecla" por defecto para askfor_aux()/askfor_aux_ext(),
 * esta toma la pulsación de tecla dada, el búfer de entrada, la longitud, etc., y realiza la
 * acción apropiada para esa pulsación de tecla, como mover el cursor a la izquierda o
 * insertar un carácter.
 *
 * Debe devolver true cuando la edición del búfer esté "completa" (ej. al
 * presionar RETURN).
 */
bool askfor_aux_keypress(char *buf, size_t buflen, size_t *curs, size_t *len,
						 struct keypress keypress, bool firsttime)
{
	size_t ulen = utf8_strlen(buf);

	switch (keypress.code)
	{
		case ESCAPE:
		{
			*curs = 0;
			return true;
		}
		
		case KC_ENTER:
		{
			*curs = ulen;
			return true;
		}
		
		case ARROW_LEFT:
		{
			if (firsttime) {
				*curs = 0;
			} else if (*curs > 0) {
				(*curs)--;
			}
			break;
		}
		
		case ARROW_RIGHT:
		{
			if (firsttime) {
				*curs = ulen;
			} else if (*curs < ulen) {
				(*curs)++;
			}
			break;
		}
		
		case KC_BACKSPACE:
		case KC_DELETE:
		{
			char *ocurs, *oshift;

			/* Si es la primera vez, retroceso significa "borrar todo" */
			if (firsttime) {
				buf[0] = '\0';
				*curs = 0;
				*len = 0;
				break;
			}

			/* Rechazar retroceder hacia la nada */
			if ((keypress.code == KC_BACKSPACE && *curs == 0) ||
				(keypress.code == KC_DELETE && *curs >= ulen))
				break;

			/*
			 * Mover la cadena desde k hasta nulo hacia la izquierda
			 * en 1. Primero, hay que obtener el desplazamiento correspondiente a
			 * la posición del cursor.
			 */
			ocurs = utf8_fskip(buf, *curs, NULL);
			assert(ocurs);
			if (keypress.code == KC_BACKSPACE) {
				/* Obtener desplazamiento del carácter anterior. */
				oshift = utf8_rskip(ocurs, 1, buf);
				assert(oshift);
				memmove(oshift, ocurs, *len - (ocurs - buf));
				/* Disminuir. */
				(*curs)--;
				*len -= ocurs - oshift;
			} else {
				/* Obtener desplazamiento del siguiente carácter. */
				oshift = utf8_fskip(ocurs, 1, NULL);
				assert(oshift);
				memmove(ocurs, oshift, *len - (oshift - buf));
				/* Disminuir */
				*len -= oshift - ocurs;
			}

			/* Terminar */
			buf[*len] = '\0';

			break;
		}
		
		default:
		{
			bool atnull = (*curs == ulen);
			char encoded[5];
			size_t n_enc = 0;
			char *ocurs;

			if (keycode_isprint(keypress.code)) {
				n_enc = utf32_to_utf8(encoded,
					N_ELEMENTS(encoded), &keypress.code,
					1, NULL);
			}
			if (n_enc == 0) {
				bell();
				break;
			}

			/* Limpiar el búfer si es la primera vez */
			if (firsttime) {
				buf[0] = '\0';
				*curs = 0;
				*len = 0;
				atnull = 1;
			}

			/* Asegurarse de que tenemos suficiente espacio para el nuevo carácter */
			if (*len + n_enc >= buflen) {
				break;
			}

			/* Insertar el carácter codificado. */
			if (atnull) {
				ocurs = buf + *len;
			} else {
				ocurs = utf8_fskip(buf, *curs, NULL);
				assert(ocurs);
				/*
				 * Mover el resto del búfer hacia adelante para hacer
				 * espacio.
				 */
				memmove(ocurs + n_enc, ocurs,
					*len - (ocurs - buf));
			}
			memcpy(ocurs, encoded, n_enc);

			/* Actualizar posición y longitud. */
			(*curs)++;
			*len += n_enc;

			/* Terminar */
			buf[*len] = '\0';

			break;
		}
	}

	/* Por defecto, no hemos terminado. */
	return false;
}


/**
 * Manejar un evento de ratón durante la edición de una cadena. Este es el manejador de ratón
 * por defecto para askfor_aux_ext().
 *
 * \param buf es el búfer con la cadena a editar.
 * \param buflen es el número máximo de caracteres que se pueden almacenar en buf.
 * \param curs es el puntero a la posición del cursor en el búfer.
 * \param len es el puntero a la posición del primer carácter nulo en el búfer.
 * \param mouse es una descripción del evento de ratón a manejar.
 * \param firsttime es si esta es la primera llamada al manejador de teclas o
 * ratón en esta sesión de edición.
 * \return cero si la sesión de edición debe continuar, uno si la sesión de
 * edición debe terminar y se debe aceptar el contenido actual del búfer, o
 * dos si la sesión de edición debe terminar y se debe rechazar el contenido actual
 * del búfer.
 *
 * askfor_aux_mouse() es muy simple. Cualquier clic de ratón termina la sesión
 * de edición, y si ese clic es con el segundo botón, el resultado de la
 * edición se rechaza.
 */
int askfor_aux_mouse(char *buf, size_t buflen, size_t *curs, size_t *len,
		struct mouseclick mouse, bool firsttime)
{
	return (mouse.button == 2) ? 2 : 1;
}


/**
 * Obtener alguna entrada en la ubicación del cursor.
 *
 * Se asume que el búfer se ha inicializado con una cadena por defecto.
 * Nótese que esta cadena a menudo está "vacía" (ver abajo).
 *
 * El búfer por defecto se muestra en amarillo hasta que se limpia, lo que sucede
 * en la primera pulsación de tecla, a menos que esa pulsación sea Retorno.
 *
 * Los caracteres normales limpian el valor por defecto y añaden el carácter.
 * Retroceso limpia el valor por defecto o elimina el carácter final.
 * Retorno acepta el contenido actual del búfer y devuelve true.
 * Escape limpia el búfer y la ventana y devuelve false.
 *
 * Nótese que 'len' se refiere al tamaño del búfer. La longitud máxima
 * de la entrada es 'len-1'.
 *
 * 'keypress_h' es un puntero a una función para manejar pulsaciones de tecla, alterando
 * el búfer de entrada, la posición del cursor y similares según sea necesario. Ver
 * 'askfor_aux_keypress' (el manejador por defecto si suministras NULL para
 * 'keypress_h') para un ejemplo.
 */
bool askfor_aux(char *buf, size_t len, bool (*keypress_h)(char *, size_t, size_t *, size_t *, struct keypress, bool))
{
	int y, x;

	size_t k = 0;		/* Posición del cursor */
	size_t nul = 0;		/* Posición del byte nulo en la cadena */

	struct keypress ch = KEYPRESS_NULL;

	bool done = false;
	bool firsttime = true;

	if (keypress_h == NULL)
		keypress_h = askfor_aux_keypress;

	/* Localizar el cursor */
	Term_locate(&x, &y);

	/* Paranoia */
	if ((x < 0) || (x >= 80)) x = 0;

	/* Restringir la longitud */
	if (x + len > 80) len = 80 - x;

	/* Truncar la entrada por defecto */
	buf[len-1] = '\0';

	/* Obtener la posición del byte nulo */
	nul = strlen(buf);

	/* Mostrar la respuesta por defecto */
	Term_erase(x, y, (int)len);
	Term_putstr(x, y, -1, COLOUR_YELLOW, buf);

	/* Procesar entrada */
	while (!done) {
		/* Colocar cursor */
		Term_gotoxy(x + k, y);

		/* Obtener una tecla */
		ch = inkey();

		/* Dejar que el manejador de pulsaciones de tecla se encargue de la pulsación */
		done = keypress_h(buf, len, &k, &nul, ch, firsttime);

		/* Actualizar la entrada */
		Term_erase(x, y, (int)len);
		Term_putstr(x, y, -1, COLOUR_WHITE, buf);

		/* Ya no es la primera vez */
		firsttime = false;
	}

	/* Hecho */
	return (ch.code != ESCAPE);
}


/**
 * Actuar como askfor_aux() pero permitir la personalización de lo que sucede con la entrada
 * del ratón.
 *
 * \param buf es el búfer con la cadena a editar.
 * \param len es el número máximo de caracteres que buf puede contener.
 * \param keypress_h es la función a llamar para manejar una pulsación de tecla. Puede ser
 * NULL. En ese caso, se usa askfor_aux_keypress(). La función toma seis
 * argumentos y debe devolver si se debe terminar esta sesión de
 * edición. El primer argumento es el búfer con la cadena a editar. El
 * segundo argumento es el número máximo de caracteres que se pueden almacenar en
 * ese búfer. El tercer argumento es un puntero a la posición del cursor
 * en el búfer. El cuarto argumento es un puntero a la posición del primer
 * carácter nulo en el búfer. El quinto argumento es una descripción de la
 * pulsación de tecla a manejar. El sexto argumento es si esta es la primera
 * llamada al manejador de pulsaciones de tecla o al manejador de ratón en esta sesión
 * de edición.
 * \param mouse_h es la función a llamar para manejar un clic de ratón. Puede ser
 * NULL. En ese caso, se usa askfor_aux_mouse(). La función toma seis
 * argumentos y debe devolver cero (esta sesión de edición debe continuar),
 * uno (esta sesión de edición debe terminar y se debe aceptar el resultado en el búfer),
 * o un valor distinto de cero diferente de uno (esta sesión de edición debe
 * terminar y no se debe aceptar el resultado en el búfer). El primer argumento
 * es el búfer con la cadena a editar. El segundo argumento es el número máximo de
 * caracteres que se pueden almacenar en ese búfer. El tercer argumento es un puntero a
 * la posición del cursor en el búfer. El cuarto argumento es un puntero a la
 * posición del primer carácter nulo en el búfer. El quinto argumento es una
 * descripción del evento de ratón a manejar. El sexto argumento es si esta es la
 * primera llamada al manejador de pulsaciones de tecla o al manejador de ratón en esta
 * sesión de edición.
 */
bool askfor_aux_ext(char *buf, size_t len,
	bool (*keypress_h)(char *, size_t, size_t *, size_t *, struct keypress, bool),
	int (*mouse_h)(char *, size_t, size_t *, size_t *, struct mouseclick, bool))
{
	size_t k = 0;		/* Posición del cursor */
	size_t nul = 0;		/* Posición del byte nulo en la cadena */
	bool firsttime = true;
	bool done = false;
	bool accepted = true;
	int y, x;

	if (keypress_h == NULL) {
		keypress_h = askfor_aux_keypress;
	}
	if (mouse_h == NULL) {
		mouse_h = askfor_aux_mouse;
	}

	/* Localizar el cursor */
	Term_locate(&x, &y);

	/* Paranoia */
	if (x < 0 || x >= 80) x = 0;

	/* Restringir la longitud */
	if (x + len > 80) len = 80 - x;

	/* Truncar la entrada por defecto */
	buf[len-1] = '\0';

	/* Obtener la posición del byte nulo */
	nul = strlen(buf);

	/* Mostrar la respuesta por defecto */
	Term_erase(x, y, (int)len);
	Term_putstr(x, y, -1, COLOUR_YELLOW, buf);

	/* Procesar entrada */
	while (!done) {
		ui_event in;

		/* Colocar cursor */
		Term_gotoxy(x + k, y);

		/*
		 * Obtener entrada. Emular lo que hace inkey() sin forzar
		 * eventos de ratón a parecer pulsaciones de tecla.
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

		/* Pasar al manejador apropiado. */
		if (in.type == EVT_KBRD) {
			done = keypress_h(buf, len, &k, &nul, in.key,
				firsttime);
			accepted = (in.key.code != ESCAPE);
		} else if (in.type == EVT_MOUSE) {
			int result = mouse_h(buf, len, &k, &nul, in.mouse,
				firsttime);

			if (result != 0) {
				done = true;
				accepted = (result == 1);
			}
		}

		/* Actualizar la entrada */
		Term_erase(x, y, (int)len);
		Term_putstr(x, y, -1, COLOUR_WHITE, buf);

		/* Ya no es la primera vez */
		firsttime = false;
	}

	return accepted;
}


/**
 * Una función de "manejo de pulsaciones de tecla" para askfor_aux, que maneja el caso
 * especial de '*' para un nuevo "nombre" aleatorio y pasa cualquier otra "pulsación de tecla"
 * a través del manejador de "edición" por defecto.
 */
static bool get_name_keypress(char *buf, size_t buflen, size_t *curs,
							  size_t *len, struct keypress keypress,
							  bool firsttime)
{
	bool result;

	switch (keypress.code)
	{
		case '*':
		{
			*len = player_random_name(buf, buflen);
			*curs = 0;
			result = false;
			break;
		}

		default:
		{
			result = askfor_aux_keypress(buf, buflen, curs, len, keypress,
										 firsttime);
			break;
		}
	}

	return result;
}


/**
 * Manejar un evento de ratón durante la edición de una cadena: presenta un menú contextual
 * con opciones apropiadas para manejar la edición del nombre de un personaje.
 *
 * \param buf es el búfer con la cadena a editar.
 * \param buflen es el número máximo de caracteres que se pueden almacenar en buf.
 * \param curs es el puntero a la posición del cursor en el búfer.
 * \param len es el puntero a la posición del primer carácter nulo en el búfer.
 * \param mouse es una descripción del evento de ratón a manejar.
 * \param firsttime es si esta es la primera llamada al manejador de teclas o
 * ratón en esta sesión de edición.
 * \return cero si la sesión de edición debe continuar, uno si la sesión de
 * edición debe terminar y se debe aceptar el contenido actual del búfer, o
 * dos si la sesión de edición debe terminar y se debe rechazar el contenido actual
 * del búfer.
 */
static int handle_name_mouse(char *buf, size_t buflen, size_t *curs,
		size_t *len, struct mouseclick mouse, bool firsttime)
{
	enum { ACT_CTX_NAME_ACCEPT, ACT_CTX_NAME_RANDOM, ACT_CTX_NAME_CLEAR };
	int result = 2;
	char *labels;
	struct menu *m;
	int action;

	/*
	 * Un clic de ratón con el segundo botón termina la sesión de edición y
	 * indica que el resultado de la edición debe ser rechazado.
	 */
	if (mouse.button == 2) {
		return result;
	}

	/* Por defecto, no terminar la sesión de edición. */
	result = 0;

	/* Presentar un menú contextual con las acciones posibles. */
	labels = string_make(lower_case);
	m = menu_dynamic_new();

	m->selections = labels;
	menu_dynamic_add_label(m, "Aceptar", 'a', ACT_CTX_NAME_ACCEPT, labels);
	menu_dynamic_add_label(m, "Establecer nombre aleatorio", 'r',
		ACT_CTX_NAME_RANDOM, labels);
	menu_dynamic_add_label(m, "Borrar nombre", 'c', ACT_CTX_NAME_CLEAR,
		labels);

	screen_save();

	menu_dynamic_calc_location(m, mouse.x, mouse.y);
	region_erase_bordered(&m->boundary);

	action = menu_dynamic_select(m);

	menu_dynamic_free(m);
	string_free(labels);

	screen_load();

	/* Hacer lo solicitado. */
	switch (action) {
	case ACT_CTX_NAME_ACCEPT:
		/* Terminar la sesión de edición y aceptar el resultado. */
		result = 1;
		break;

	case ACT_CTX_NAME_RANDOM:
		*len = player_random_name(buf, buflen);
		*curs = 0;
		break;

	case ACT_CTX_NAME_CLEAR:
		assert(buflen > 0);
		buf[0] = '\0';
		*len = 0;
		*curs = 0;
		break;
	}

	return result;
}


/**
 * Obtiene un nombre para el personaje, reaccionando a los cambios de nombre.
 *
 * Si sf es true, cambiamos el nombre del archivo guardado dependiendo del nombre del personaje.
 */
bool get_character_name(char *buf, size_t buflen)
{
	bool res;

	/* Paranoia */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Mostrar mensaje */
	prt("Introduce un nombre para tu personaje (* aleatorio): ", 0, 0);

	/* Guardar el nombre del jugador */
	my_strcpy(buf, player->full_name, buflen);

	/* Preguntar al usuario por una cadena */
	res = askfor_aux_ext(buf, buflen, get_name_keypress, handle_name_mouse);

	/* Limpiar mensaje */
	prt("", 0, 0);

	/* Volver al nombre anterior si el jugador no elige uno nuevo. */
	if (!res)
		my_strcpy(buf, player->full_name, buflen);

	return res;
}



/**
 * Preguntar por una cadena del usuario.
 *
 * El "prompt" debe tener la forma "Prompt: ".
 *
 * Ver "askfor_aux" para algunas notas sobre "buf" y "len", y sobre
 * el valor de retorno de esta función.
 */
static bool textui_get_string(const char *prompt, char *buf, size_t len)
{
	bool res;

	/* Paranoia */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Mostrar mensaje */
	prt(prompt, 0, 0);

	/* Preguntar al usuario por una cadena */
	res = askfor_aux(buf, len, NULL);

	/* Limpiar mensaje */
	prt("", 0, 0);

	/* Resultado */
	return (res);
}



/**
 * Solicitar una "cantidad" del usuario
 */
static int textui_get_quantity(const char *prompt, int max)
{
	int amt = 1;

	/* Preguntar si es necesario */
	if (max != 1) {
		char tmp[80];
		char buf[80];

		/* Construir un mensaje si es necesario */
		if (!prompt) {
			/* Construir un mensaje */
			strnfmt(tmp, sizeof(tmp), "Cantidad (0-%d, *=todo): ", max);

			/* Usar ese mensaje */
			prompt = tmp;
		}

		/* Construir el valor por defecto */
		strnfmt(buf, sizeof(buf), "%d", amt);

		/* Preguntar por una cantidad */
		if (!get_string(prompt, buf, 7)) return (0);

		/* Extraer un número */
		amt = atoi(buf);

		/* Un asterisco o letra significa "todo" */
		if ((buf[0] == '*') || isalpha((unsigned char)buf[0])) amt = max;
	}

	/* Aplicar el máximo */
	if (amt > max) amt = max;

	/* Aplicar el mínimo */
	if (amt < 0) amt = 0;

	/* Devolver el resultado */
	return (amt);
}


/**
 * Verificar algo con el usuario
 *
 * El "prompt" debe tener la forma "¿Consulta? "
 *
 * Nótese que se añade "[s/n]" al mensaje.
 */
static bool textui_get_check(const char *prompt)
{
	ui_event ke;

	char buf[80];

	/*
	 * Construir un mensaje "útil"; hacer esto primero para que los mensajes construidos por
	 * format() no se vean afectados por los efectos secundarios de event_signal().
	 */
	strnfmt(buf, 78, "%.70s[s/n] ", prompt);

	/* Paranoia */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Preguntar por ello */
	prt(buf, 0, 0);
	ke = inkey_m();

	/* Borrar el mensaje */
	prt("", 0, 0);

	/* Negación normal */
	if (ke.type == EVT_MOUSE) {
		if ((ke.mouse.button != 1) && (ke.mouse.y != 0))
			return (false);
	} else {
		if ((ke.key.code != 'S') && (ke.key.code != 's'))
			return (false);
	}

	/* Éxito */
	return (true);
}

/* TODO: refactorizar get_check() en términos de get_char() */
/**
 * Preguntar al usuario que responda con un carácter. Las opciones son una cadena constante,
 * ej. "snm"; len es la longitud de la cadena constante, y fallback debe
 * ser la respuesta por defecto si el usuario pulsa escape o una tecla inválida.
 *
 * Ejemplo: get_char("¿Estudiar? ", "snm", 3, 'n')
 *     Esto pregunta "¿Estudiar? [snm]" y el valor por defecto es 'n'.
 *
 */
char get_char(const char *prompt, const char *options, size_t len, char fallback)
{
	struct keypress key;
	char buf[80];

	/* Paranoia */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Construir un mensaje "útil" */
	strnfmt(buf, 78, "%.70s[%s] ", prompt, options);

	/* Preguntar por ello */
	prt(buf, 0, 0);

	/* Obtener una respuesta aceptable */
	key = inkey();

	/* Convertir respuesta a minúsculas si es necesario */
	if (key.code >= 'A' && key.code <= 'Z') key.code += 32;

	/* Ver si la tecla está en nuestra cadena de opciones */
	if (!strchr(options, (char)key.code))
		key.code = fallback;

	/* Borrar el mensaje */
	prt("", 0, 0);

	/* Éxito */
	return key.code;
}


/**
 * Forma nativa de texto para obtener un nombre de archivo.
 */
static bool get_file_text(const char *suggested_name, char *path, size_t len)
{
	char buf[160];

	/* Obtener nombre de archivo */
	my_strcpy(buf, suggested_name, sizeof buf);
	
	if (!arg_force_name) {
			
			if (!get_string("Nombre de archivo: ", buf, sizeof buf)) return false;

			/* Asegurarse de que es realmente un nombre de archivo */
			if (buf[0] == '\0' || buf[0] == ' ') return false;
	} else {
		int old_len;
		time_t ltime;
		struct tm *today;

		/* Obtener la hora actual */
		time(&ltime);
		today = localtime(&ltime);

		prt("Nombre de archivo: ", 0,0);

		/* Sobrescribir el ".txt" que se añadió */
		assert(strlen(buf) >= 4);
		old_len = strlen(buf) - 4;
		strftime(buf + old_len, sizeof(buf) - len, "-%Y-%m-%d-%H-%M.txt", today);

		/* Preguntar al usuario para confirmar o cancelar el volcado de archivo */
		if (!get_check(format("¿Confirmar escritura en %s? ", buf))) return false;


	}

	/* Construir la ruta */
	path_build(path, len, ANGBAND_DIR_USER, buf);

	/* Verificar si ya existe */
	if (file_exists(path) && !get_check("¿Reemplazar archivo existente? "))
		return false;

	/* Decir al usuario dónde se guardó. */
	prt(format("Guardando como %s.", path), 0, 0);
	anykey();
	prt("", 0, 0);

	return true;
}




/**
 * Obtener un nombre de ruta para guardar un archivo, dado el nombre sugerido. Devuelve el
 * resultado en "path".
 */
bool (*get_file)(const char *suggested_name, char *path, size_t len) = get_file_text;




/**
 * Pregunta por una pulsación de tecla
 *
 * El "prompt" debe tener la forma "Comando: "
 * -------
 * Advertencia - esta función asume que el comando introducido es un carácter ASCII,
 *            y por lo tanto debe usarse con mucha precaución - NRM
 * -------
 * Devuelve true a menos que el carácter sea "Escape"
 */
static bool textui_get_com(const char *prompt, char *command)
{
	ui_event ke;
	bool result;

	result = get_com_ex(prompt, &ke);
	*command = (char)ke.key.code;

	return result;
}


bool get_com_ex(const char *prompt, ui_event *command)
{
	ui_event ke;

	/* Paranoia XXX XXX XXX */
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Mostrar un mensaje */
	prt(prompt, 0, 0);

	/* Obtener una tecla */
	ke = inkey_m();

	/* Limpiar el mensaje */
	prt("", 0, 0);

	/* Guardar el comando */
	*command = ke;

	/* Hecho */
	if ((ke.type == EVT_KBRD && ke.key.code != ESCAPE) ||
		(ke.type == EVT_MOUSE))
		return true;
	else
		return false;
}


/**
 * Pausa para respuesta del usuario
 *
 * Esta función es estúpida.  XXX XXX XXX
 */
void pause_line(struct term *tm)
{
	prt("", tm->hgt - 1, 0);
	put_str("[Pulsa cualquier tecla para continuar]", tm->hgt - 1, (tm->wid - 27) / 2);
	(void)anykey();
	prt("", tm->hgt - 1, 0);
}

static int dir_transitions[10][10] =
{
	/* 0-> */ { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
	/* 1-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 2-> */ { 0, 0, 2, 0, 1, 0, 3, 0, 5, 0 },
	/* 3-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 4-> */ { 0, 0, 1, 0, 4, 0, 5, 0, 7, 0 },
	/* 5-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 6-> */ { 0, 0, 3, 0, 5, 0, 6, 0, 9, 0 },
	/* 7-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	/* 8-> */ { 0, 0, 5, 0, 7, 0, 9, 0, 8, 0 },
	/* 9-> */ { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

/**
 * Solicitar una dirección de "movimiento" (1,2,3,4,5(opcional),6,7,8,9) del usuario.
 *
 * Devuelve true si se eligió una dirección, en caso contrario devuelve false.
 *
 * Esta función debe usarse para todos los comandos "repetibles", como
 * correr, caminar, abrir, cerrar, derribar, desarmar, clavar, excavar, etc., así
 * como todos los comandos que deben hacer referencia a una casilla adyacente al jugador.
 * Si el comando no permite la casilla debajo del jugador, pasa false
 * para allow_5. De lo contrario, usa true para allow_5.
 *
 * La dirección "0" es ilegal y no se aceptará.
 */
static bool textui_get_rep_dir(int *dp, bool allow_5)
{
	int dir = 0;

	ui_event ke;

	/* Inicializar */
	(*dp) = 0;

	/* Obtener una dirección */
	while (!dir) {
		/* Paranoia*/
		event_signal(EVENT_MESSAGE_FLUSH);

		/* Obtener la primera pulsación de tecla - la primera prueba es para evitar mostrar el
		 * mensaje de dirección si ya hay una pulsación de tecla en cola y esperando - esto solo
		 * evita un mensaje parpadeante si hay un retraso de movimiento "perezoso". */
		inkey_scan = SCAN_INSTANT;
		ke = inkey_ex();
		inkey_scan = SCAN_OFF;

		if (ke.type == EVT_NONE ||
				(ke.type == EVT_KBRD
				&& !target_dir_allow(ke.key, allow_5, true))) {
			prt("¿Dirección o <clic> (Escape para cancelar)? ", 0, 0);
			ke = inkey_ex();
		}

		/* Verificar coordenadas del ratón, u obtener pulsaciones de tecla hasta que se elija una dirección */
		if (ke.type == EVT_MOUSE) {
			if (ke.mouse.button == 1) {
				int y = KEY_GRID_Y(ke);
				int x = KEY_GRID_X(ke);
				struct loc from = player->grid;
				struct loc to = loc(x, y);

				dir = pathfind_direction_to(from, to);
			} else if (ke.mouse.button == 2) {
				/* Limpiar el mensaje */
				prt("", 0, 0);

				return (false);
			}
		} else if (ke.type == EVT_KBRD) {
			int keypresses_handled = 0;

			while (ke.type == EVT_KBRD && ke.key.code != 0) {
				int this_dir;

				if (ke.key.code == ESCAPE) {
					/* Limpiar el mensaje */
					prt("", 0, 0);

					return (false);
				}

				/* XXX Idealmente mostrar y mover el cursor aquí para indicar
				 la dirección actualmente "Pendiente". XXX */
				this_dir = target_dir_allow(ke.key, allow_5,
					true);

				if (this_dir == ESCAPE) {
					/* Limpiar el mensaje */
					prt("", 0, 0);

					return (false);
				} else if (this_dir) {
					dir = dir_transitions[dir][this_dir];
				}

				if (player->opts.lazymove_delay == 0 || ++keypresses_handled > 1)
					break;

				inkey_scan = player->opts.lazymove_delay;
				ke = inkey_ex();
			}

			/* 5 es equivalente a "escape" */
			if (dir == 5 && !allow_5) {
				/* Limpiar el mensaje */
				prt("", 0, 0);

				return (false);
			}
		}

		/* Ups */
		if (!dir) bell();
	}

	/* Limpiar el mensaje */
	prt("", 0, 0);

	/* Guardar dirección */
	(*dp) = dir;

	/* Éxito */
	return (true);
}

/**
 * Obtener una "dirección de puntería" (1,2,3,4,6,7,8,9 o 5) del usuario.
 *
 * Devuelve true si se eligió una dirección, en caso contrario devuelve false.
 *
 * La dirección "5" es especial, y significa "usar objetivo actual".
 *
 * Esta función rastrea y usa la "dirección global", y usa
 * esa como la "dirección deseada", si está establecida.
 *
 * Nótese que "Forzar Objetivo", si está activado, anulará la interacción del usuario,
 * si ya hay un objetivo utilizable establecido.
 */
static bool textui_get_aim_dir(int *dp)
{
	/* Dirección global */
	int dir = 0;
	ui_event ke;

	const char *p;

	/* Inicializar */
	(*dp) = 0;

	/* Auto-objetivo si se solicita */
	if (OPT(player, use_old_target) && target_okay() && !dir) dir = 5;

	/* Preguntar hasta estar satisfecho */
	while (!dir) {
		/*
		 * Si generar una advertencia audible sobre un fallo de
		 * objetivo.
		 */
		bool need_beep = false;

		/* Elegir un mensaje */
		if (!target_okay())
			p = "¿Dirección ('*' o <clic> para objetivo, \"'\" para el más cercano, Escape para cancelar)? ";
		else
			p = "¿Dirección ('5' para objetivo, '*' o <clic> para re-objetivar, Escape para cancelar)? ";

		/* Obtener un comando (o Cancelar) */
		if (!get_com_ex(p, &ke)) break;

		if (ke.type == EVT_MOUSE) {
			if (ke.mouse.button == 1) {
				if (target_set_interactive(TARGET_KILL,
						KEY_GRID_X(ke), KEY_GRID_Y(ke),
						false))
					dir = 5;
			} else if (ke.mouse.button == 2) {
				break;
			}
		} else if (ke.type == EVT_KBRD) {
			if (ke.key.code == '*') {
				/* Establecer nuevo objetivo, usar objetivo si es legal */
				if (target_set_interactive(TARGET_KILL, -1, -1,
						false))
					dir = 5;
			} else if (ke.key.code == '\'') {
				/* Establecer al objetivo más cercano */
				if (target_set_closest(TARGET_KILL, NULL)) {
					dir = 5;
				} else {
					need_beep = true;
				}
			} else if (ke.key.code == 't' || ke.key.code == '5' ||
					   ke.key.code == '0' || ke.key.code == '.') {
				if (target_okay()) {
					dir = 5;
				} else {
					need_beep = true;
				}
			} else {
				/* Dirección posible */
				int keypresses_handled = 0;

				while (ke.key.code != 0){
					int this_dir;

					/* XXX Idealmente mostrar y mover el cursor aquí para indicar
					 * la dirección actualmente "Pendiente". XXX */
					this_dir = target_dir_allow(ke.key,
						false, true);

					if (this_dir == ESCAPE) {
						return false;
					}
					if (this_dir) {
						dir = dir_transitions[dir][this_dir];
					} else {
						need_beep = true;
						break;
					}

					if (player->opts.lazymove_delay == 0 || ++keypresses_handled > 1)
						break;

					/* Ver si hay una segunda pulsación de tecla dentro del período
					 * de tiempo definido. */
					inkey_scan = player->opts.lazymove_delay;
					ke = inkey_ex();
				}
			}
		}

		/* Error */
		if (need_beep) bell();
	}

	/* Sin dirección */
	if (!dir) return (false);
	
	/* Guardar dirección */
	(*dp) = dir;
	
	/* Se introdujo una dirección "válida" */
	return (true);
}

/**
 * Inicializar los ganchos de UI para dar entrada solicitada por el juego
 */
void textui_input_init(void)
{
	get_string_hook = textui_get_string;
	get_quantity_hook = textui_get_quantity;
	get_check_hook = textui_get_check;
	get_com_hook = textui_get_com;
	get_rep_dir_hook = textui_get_rep_dir;
	get_aim_dir_hook = textui_get_aim_dir;
	get_spell_from_book_hook = textui_get_spell_from_book;
	get_spell_hook = textui_get_spell;
	get_effect_from_list_hook = textui_get_effect_from_list;
	get_item_hook = textui_get_item;
	get_curse_hook = textui_get_curse;
	get_panel_hook = textui_get_panel;
	panel_contains_hook = textui_panel_contains;
	map_is_visible_hook = textui_map_is_visible;
	view_abilities_hook = textui_view_ability_menu;
}


/*** Procesamiento de entrada ***/


/**
 * Obtener un contador de comandos, con la tecla '0'.
 */
static int textui_get_count(void)
{
	int count = 0;

	while (1) {
		struct keypress ke;

		prt(format("Repetir: %d", count), 0, 0);

		ke = inkey();
		if (ke.code == ESCAPE)
			return -1;

		/* Edición simple (suprimir o retroceso) */
		else if (ke.code == KC_DELETE || ke.code == KC_BACKSPACE)
			count = count / 10;

		/* Datos numéricos reales */
		else if (isdigit((unsigned char) ke.code)) {
			count = count * 10 + D2I(ke.code);

			if (count >= 9999) {
				bell();
				count = 9999;
			}
		} else {
			/* Cualquier cosa no numérica pasa directamente a la entrada de comandos */
			/* XXX molesto código fijo de la tecla del menú de acción */
			if (ke.code != KC_ENTER)
				Term_keypress(ke.code, ke.mods);

			break;
		}
	}

	return count;
}



/**
 * Búfer especial para contener la acción del mapa de teclas actual
 */
static struct keypress request_command_buffer[256];


/**
 * Solicitar un comando del usuario.
 *
 * Nótese que "caret" ("^") se trata de forma especial, y se usa para
 * permitir la entrada manual de caracteres de control. Esto se puede usar
 * en muchas máquinas para solicitar excavación repetida (Ctrl-H) y
 * en Macintosh para solicitar "Control-Caret".
 *
 * Nótese que "backslash" se trata de forma especial, y se usa para evitar cualquier
 * entrada de mapa de teclas para el siguiente carácter. Esto es útil para macros.
 */
ui_event textui_get_command(int *count)
{
	int mode = OPT(player, rogue_like_commands) ? KEYMAP_MODE_ROGUE : KEYMAP_MODE_ORIG;

	struct keypress tmp[2] = { KEYPRESS_NULL, KEYPRESS_NULL };

	ui_event ke = EVENT_EMPTY;

	const struct keypress *act = NULL;



	/* Obtener comando */
	while (1) {
		/* No se necesita vaciado */
		msg_flag = false;

		/* Activar "modo comando" */
		inkey_flag = true;

		/* Activar cursor si se solicita */
		if (OPT(player, highlight_player)) {
			Term_set_cursor(true);
			move_cursor_relative(player->grid.y, player->grid.x);
		}

		/* Obtener un comando */
		ke = inkey_ex();

		/* Desactivar cursor */
		if (OPT(player, highlight_player)) {
			Term_set_cursor(false);
		}

		if (ke.type == EVT_KBRD) {
			bool keymap_ok = true;
			switch (ke.key.code) {
				case '0': {
					if(ke.key.mods & KC_MOD_KEYPAD) break;

					int c = textui_get_count();

					if (c == -1 || !get_com_ex("Comando: ", &ke))
						continue;
					else
						*count = c;
					break;
				}

				case '\\': {
					/* Permitir omitir mapas de teclas */
					(void)get_com_ex("Comando: ", &ke);
					keymap_ok = false;
					break;
				}

				case '^': {
					/* Permitir introducir "caracteres de control" */
					if (!get_com_ex("Control: ", &ke)
							|| ke.type != EVT_KBRD) {
						continue;
					}
					if (ENCODE_KTRL(ke.key.code)) {
						ke.key.code = KTRL(ke.key.code);
					} else {
						ke.key.mods |= KC_MOD_CONTROL;
					}
					break;
				}
			}

			/* Encontrar cualquier mapa de teclas relevante */
			if (keymap_ok)
				act = keymap_find(mode, ke.key);
		}

		/* Borrar la línea de mensaje */
		prt("", 0, 0);

		if (ke.type == EVT_BUTTON) {
			/* Los botones siempre se especifican en el conjunto de teclas estándar */
			act = tmp;
			tmp[0] = ke.key;
		}

		/* Aplicar mapa de teclas si no estamos ya dentro de un mapa de teclas */
		if (ke.key.code && act && !inkey_next) {
			size_t n = 0;
			while (act[n].type)
				n++;

			/* Hacer espacio para el terminador */
			n += 1;

			/* Instalar el mapa de teclas */
			memcpy(request_command_buffer, act, n * sizeof(struct keypress));

			/* Empezar a usar el búfer */
			inkey_next = request_command_buffer;

			/* Continuar */
			continue;
		}

		/* Hecho */
		break;
	}

	return ke;
}

/**
 * Verificar que ningún objeto usado actualmente está impidiendo la acción 'c'
 */
bool key_confirm_command(unsigned char c)
{
	int i;

	/* Escanear equipo */
	for (i = 0; i < player->body.count; i++) {
		char verify_inscrip[] = "^*";
		unsigned n;

		struct object *obj = slot_object(player, i);
		if (!obj) continue;

		/* Configurar cadena a buscar, ej. "^d" */
		verify_inscrip[1] = c;

		/* Verificar comando */
		n = check_for_inscrip(obj, "^*") +
				check_for_inscrip(obj, verify_inscrip);
		while (n--) {
			if (!get_check("¿Estás seguro? "))
				return false;
		}
	}

	return true;
}


/**
 * Procesar una pulsación de tecla de la interfaz de texto.
 */
bool textui_process_key(struct keypress kp, unsigned char *c, int count)
{
	keycode_t key = kp.code;

	/* Comando nulo */
	if (key == '\0' || key == ESCAPE || key == ' ' || key == '\a')
		return true;

	/* Pulsación de tecla inválida */
	if (key > UCHAR_MAX)
		return false;

	*c = key;
	return true;
}