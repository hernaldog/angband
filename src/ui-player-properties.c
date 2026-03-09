/**
 * \file ui-player-properties.c
 * \brief Interfaz de usuario para habilidades de clase y raza
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Leon Marrick, Bahman Rabii, Nick McConnell
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
#include "player-properties.h"
#include "ui-input.h"
#include "ui-menu.h"
#include "ui-player-properties.h"

/**
 * ------------------------------------------------------------------------
 * Código para ver habilidades de raza y clase
 * ------------------------------------------------------------------------ */

static char view_ability_tag(struct menu *menu, int oid)
{
	return all_letters_nohjkl[oid];
}

/**
 * Mostrar una entrada en el menú de ganar habilidad
 */
static void view_ability_display(struct menu *menu, int oid, bool cursor,
	int row, int col, int width)
{
	char buf[80];
	uint8_t color;
	struct player_ability *choices = menu->menu_data;

	switch (choices[oid].group) {
	case PLAYER_FLAG_SPECIAL:
		{
			strnfmt(buf, sizeof(buf), "Habilidad Especial: %s",
				choices[oid].name);
			color = COLOUR_GREEN;
			break;
		}
	case PLAYER_FLAG_CLASS:
		{
			strnfmt(buf, sizeof(buf), "Clase: %s",
				choices[oid].name);
			color = COLOUR_UMBER;
			break;
		}
	case PLAYER_FLAG_RACE:
		{
			strnfmt(buf, sizeof(buf), "Racial: %s",
				choices[oid].name);
			color = COLOUR_ORANGE;
			break;
		}
	default:
		{
			my_strcpy(buf, "Misterioso", sizeof(buf));
			color = COLOUR_PURPLE;
		}
	}

	/* Mostrarlo */
	c_put_str(cursor ? COLOUR_WHITE : color, buf, row, col);

}


/**
 * Mostrar descripción larga de habilidad al navegar
 */
static void view_ability_menu_browser(int oid, void *data, const region *loc)
{
	struct player_ability *choices = data;

	/* Redirigir salida a la pantalla */
	text_out_hook = text_out_to_screen;
	text_out_wrap = 60;
	text_out_indent = loc->col - 1;
	text_out_pad = 1;

	clear_from(loc->row + loc->page_rows);
	Term_gotoxy(loc->col, loc->row + loc->page_rows);
	text_out_c(COLOUR_L_BLUE, "\n%s\n", (char *) choices[oid].desc);

	/* XXX */
	text_out_pad = 0;
	text_out_indent = 0;
	text_out_wrap = 0;
}

/**
 * Mostrar lista de especialidades disponibles.
 */
void textui_view_ability_menu(struct player_ability *ability_list,
							  int num_abilities)
{
	struct menu menu;
	menu_iter menu_f = { view_ability_tag, 0, view_ability_display, 0, 0 };
	region loc = { 0, 0, 70, -99 };
	char buf[80];

	/* Guardar la pantalla y limpiarla */
	screen_save();

	/* Opciones de mensaje */
	strnfmt(buf, sizeof(buf),
		"Habilidades de raza y clase (%c-%c, ESC=salir): ",
		all_letters_nohjkl[0], all_letters_nohjkl[num_abilities - 1]);

	/* Configurar el menú */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.header = buf;
	menu_setpriv(&menu, num_abilities, ability_list);
	loc.page_rows = num_abilities + 1;
	menu.flags = MN_DBL_TAP;
	menu.browse_hook = view_ability_menu_browser;
	region_erase_bordered(&loc);
	menu_layout(&menu, &loc);

	menu_select(&menu, 0, false);

	/* Cargar pantalla */
	screen_load();

	return;
}