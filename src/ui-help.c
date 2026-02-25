/**
 * \file ui-help.c
 * \brief Ayuda dentro del juego
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
#include "buildid.h"
#include "init.h"
#include "ui-help.h"
#include "ui-input.h"
#include "ui-output.h"
#include "ui-term.h"

/**
 * Convertir una cadena a minúsculas.
 */
static void string_lower(char *buf)
{
	char *s;

	/* Convertir la cadena a minúsculas */
	for (s = buf; *s != 0; s++) *s = tolower((unsigned char)*s);
}


/**
 * Lectura recursiva de archivos.
 *
 * Devuelve falso en "?", si no, verdadero.
 *
 * Esta función podría ser mucho más eficiente con el uso de la funcionalidad "seek",
 * especialmente cuando se retrocede en un archivo, o se avanza en un archivo
 * menos de una página a la vez. XXX XXX XXX
 */
bool show_file(const char *name, const char *what, int line, int mode)
{
	int i, k, n;

	struct keypress ch = KEYPRESS_NULL;

	/* Número de líneas "reales" pasadas */
	int next = 0;

	/* Número de líneas "reales" en el archivo */
	int size;

	/* Valor de respaldo para "line" */
	int back = 0;

	/* Esta pantalla tiene subpantallas */
	bool menu = false;

	/* Búsqueda sensible a mayúsculas */
	bool case_sensitive = false;

	/* Archivo de ayuda actual */
	ang_file *fff = NULL;

	/* Encontrar esta cadena (si la hay) */
	char *find = NULL;

	/* Saltar a esta etiqueta */
	const char *tag = NULL;

	/* Contener una cadena a buscar */
	char finder[80] = "";

	/* Contener una cadena a mostrar */
	char shower[80] = "";

	/* Nombre de archivo */
	char filename[1024];

	/* Describir esta cosa */
	char caption[128] = "";

	/* Búfer de ruta */
	char path[1024];

	/* Búfer general */
	char buf[1024];

	/* Versión en minúsculas del búfer, para buscar */
	char lc_buf[1024];

	/* Información del submenú */
	char hook[26][32];

	int wid, hgt;
	
	/* verdadero si estamos dentro de un bloque RST que debe omitirse */
	bool skip_lines = false;



	/* Limpiar los ganchos */
	for (i = 0; i < 26; i++) hook[i][0] = '\0';

	/* Obtener tamaño */
	Term_get_size(&wid, &hgt);

	/* Copiar el nombre del archivo */
	my_strcpy(filename, name, sizeof(filename));

	n = strlen(filename);

	/* Extraer la etiqueta del nombre del archivo */
	for (i = 0; i < n; i++) {
		if (filename[i] == '#') {
			filename[i] = '\0';
			tag = filename + i + 1;
			break;
		}
	}

	/* Redirigir el nombre */
	name = filename;

	/* Facilidad actualmente no utilizada para mostrar y describir archivos arbitrarios */
	if (what) {
		my_strcpy(caption, what, sizeof(caption));

		my_strcpy(path, name, sizeof(path));
		fff = file_open(path, MODE_READ, FTYPE_TEXT);
	}

	/* Buscar en "help" */
	if (!fff) {
		strnfmt(caption, sizeof(caption), "Archivo de ayuda '%s'", name);

		path_build(path, sizeof(path), ANGBAND_DIR_HELP, name);
		fff = file_open(path, MODE_READ, FTYPE_TEXT);
	}

	/* Ups */
	if (!fff) {
		/* Mensaje */
		msg("No se puede abrir '%s'.", name);
		event_signal(EVENT_MESSAGE_FLUSH);

		/* Ups */
		return (true);
	}


	/* Pre-Analizar el archivo */
	while (true) {
		/* Leer una línea o detenerse */
		if (!file_getl(fff, buf, sizeof(buf))) break;

		/* Omitir líneas si estamos dentro de una directiva RST */
		if (skip_lines){
			if (contains_only_spaces(buf))
				skip_lines = false;
			continue;
		}

		/* Analizar un subconjunto muy pequeño de RST */
		/* TODO: debería ser más flexible */
		if (prefix(buf, ".. ")) {
			/* analizar ".. menu:: [x] filename.txt" (con espaciado exacto) */
			if (prefix(buf+strlen(".. "), "menu:: [") && 
                           buf[strlen(".. menu:: [x")]==']') {
				/* Este es un archivo de menú */
				menu = true;

				/* Extraer el elemento del menú */
				k = A2I(buf[strlen(".. menu:: [")]);

				/* Almacenar el elemento del menú (si es válido) */
				if ((k >= 0) && (k < 26))
					my_strcpy(hook[k], buf + strlen(".. menu:: [x] "),
							  sizeof(hook[0]));
			} else if (buf[strlen(".. ")] == '_') {
				/* analizar ".. _some_hyperlink_target:" */
				if (tag) {
					/* Eliminar el '>' final de la etiqueta */
					buf[strlen(buf) - 1] = '\0';

					/* Comparar con la etiqueta solicitada */
					if (streq(buf + strlen(".. _"), tag)) {
						/* Recordar la línea etiquetada */
						line = next;
					}
				}
			}

			/* Omitir esto y entrar en modo de omisión */
			skip_lines = true;
			continue;
		}

		/* Contar las líneas "reales" */
		next++;
	}

	/* Guardar el número de líneas "reales" */
	size = next;


	/* Mostrar el archivo */
	while (true) {
		/* Limpiar pantalla */
		Term_clear();


		/* Restringir el rango visible */
		if (line > (size - (hgt - 4))) line = size - (hgt - 4);
		if (line < 0) line = 0;

		skip_lines = false;

		/* Reabrir el archivo si es necesario */
		if (next > line) {
			/* Cerrarlo */
			file_close(fff);

			/* Reabrir el archivo */
			fff = file_open(path, MODE_READ, FTYPE_TEXT);
			if (!fff) return (true);

			/* El archivo se ha reiniciado */
			next = 0;
		}


		/* Ir a la línea seleccionada */
		while (next < line) {
			/* Obtener una línea */
			if (!file_getl(fff, buf, sizeof(buf))) break;

			/* Omitir líneas si estamos dentro de una directiva RST */
			if (skip_lines) {
				if (contains_only_spaces(buf))
					skip_lines=false;
				continue;
			}

			/* Omitir directivas RST */
			if (prefix(buf, ".. ")) {
				skip_lines=true;
				continue;
			}

			/* Contar las líneas */
			next++;
		}


		/* Volcar las siguientes líneas del archivo */
		for (i = 0; i < hgt - 4; ) {
			/* Rastrear la "primera" línea */
			if (!i) line = next;

			/* Obtener una línea del archivo o detenerse */
			if (!file_getl(fff, buf, sizeof(buf))) break;

			/* Omitir líneas si estamos dentro de una directiva RST */
			if (skip_lines) {
				if (contains_only_spaces(buf))
					skip_lines = false;
				continue;
			}

			/* Omitir directivas RST */
			if (prefix(buf, ".. ")) {
				skip_lines=true;
				continue;
			}

			/* Contar las líneas "reales" */
			next++;

			/* Hacer una copia de la línea actual para buscar */
			my_strcpy(lc_buf, buf, sizeof(lc_buf));

			/* Convertir la línea a minúsculas */
			if (!case_sensitive) string_lower(lc_buf);

			/* Seguir buscando */
			if (find && !i && !strstr(lc_buf, find)) continue;

			/* Dejar de buscar */
			find = NULL;

			/* Volcar la línea */
			Term_putstr(0, i+2, -1, COLOUR_WHITE, buf);

			/* Resaltar "shower" */
			if (strlen(shower)) {
				const char *str = lc_buf;

				/* Mostrar coincidencias */
				while ((str = strstr(str, shower)) != NULL) {
					int len = strlen(shower);

					/* Mostrar la coincidencia */
					Term_putstr(str-lc_buf, i+2, len, COLOUR_YELLOW,
								&buf[str-lc_buf]);

					/* Avanzar */
					str += len;
				}
			}

			/* Contar las líneas impresas */
			i++;
		}

		/* Búsqueda fallida */
		if (find) {
			bell();
			line = back;
			find = NULL;
			continue;
		}


		/* Mostrar un "título" general */
		prt(format("[%s, %s, Línea %d-%d/%d]", buildid,
		           caption, line, line + hgt - 4, size), 0, 0);


		/* Mensaje */
		if (menu) {
			/* Pantalla de menú */
			prt("[Pulsa una Letra, o ESC para salir.]", hgt - 1, 0);
		} else if (size <= hgt - 4) {
			/* Archivos pequeños */
			prt("[Pulsa ESC para salir.]", hgt - 1, 0);
		} else {
			/* Archivos grandes */
			prt("[Pulsa Espacio para avanzar, o ESC para salir.]", hgt - 1, 0);
		}

		/* Obtener una pulsación de tecla */
		ch = inkey();

		/* Salir de la ayuda */
		if (ch.code == '?') break;

		/* Activar/desactivar sensibilidad a mayúsculas */
		if (ch.code == '!')
			case_sensitive = !case_sensitive;

		/* Intentar mostrar */
		if (ch.code == '&') {
			/* Obtener "shower" */
			prt("Mostrar: ", hgt - 1, 0);
			(void)askfor_aux(shower, sizeof(shower), NULL);

			/* Convertir "shower" a minúsculas */
			if (!case_sensitive) string_lower(shower);
		}

		/* Intentar buscar */
		if (ch.code == '/') {
			/* Obtener "finder" */
			prt("Buscar: ", hgt - 1, 0);
			if (askfor_aux(finder, sizeof(finder), NULL)) {
				/* Encontrarlo */
				find = finder;
				back = line;
				line = line + 1;

				/* Convertir "finder" a minúsculas */
				if (!case_sensitive) string_lower(finder);

				/* Mostrarlo */
				my_strcpy(shower, finder, sizeof(shower));
			}
		}

		/* Ir a una línea específica */
		if (ch.code == '#') {
			char tmp[80] = "0";

			prt("Ir a Línea: ", hgt - 1, 0);
			if (askfor_aux(tmp, sizeof(tmp), NULL))
				line = atoi(tmp);
		}

		/* Ir a un archivo específico */
		if (ch.code == '%') {
			char ftmp[80];

			if (OPT(player, rogue_like_commands)) {
				my_strcpy(ftmp, "r_index.txt", sizeof(ftmp));
			} else {
				my_strcpy(ftmp, "index.txt", sizeof(ftmp));
			}

			prt("Ir a Archivo: ", hgt - 1, 0);
			if (askfor_aux(ftmp, sizeof(ftmp), NULL)) {
				if (!show_file(ftmp, NULL, 0, mode))
					ch.code = ESCAPE;
			}
		}

		switch (ch.code) {
			/* subir una línea */
			case ARROW_UP:
			case 'k':
			case '8': line--; break;

			/* subir una página */
			case KC_PGUP:
			case '9':
			case '-': line -= (hgt - 4); break;

			/* inicio */
			case KC_HOME:
			case '7': line = 0; break;

			/* bajar una línea */
			case ARROW_DOWN:
			case '2':
			case 'j':
			case KC_ENTER: line++; break;

			/* bajar una página */
			case KC_PGDOWN:
			case '3':
			case ' ': line += hgt - 4; break;

			/* fin */
			case KC_END:
			case '1': line = size; break;
		}

		/* Recurrir en letras */
		if (menu && isalpha((unsigned char)ch.code)) {
			/* Extraer el elemento de menú solicitado */
			k = A2I(ch.code);

			/* Verificar el elemento de menú */
			if ((k >= 0) && (k <= 25) && hook[k][0]) {
				/* Recurrir en ese archivo */
				if (!show_file(hook[k], NULL, 0, mode)) ch.code = ESCAPE;
			}
		}

		/* Salir con escape */
		if (ch.code == ESCAPE) break;
	}

	/* Cerrar el archivo */
	file_close(fff);

	/* Hecho */
	return (ch.code != '?');
}


/**
 * Consultar la ayuda en línea
 */
void do_cmd_help(void)
{
	/* Guardar pantalla */
	screen_save();

	/* Consultar el archivo de ayuda principal */
	(void)show_file((OPT(player, rogue_like_commands)) ?
		"r_index.txt" : "index.txt", NULL, 0, 0);

	/* Cargar pantalla */
	screen_load();
}