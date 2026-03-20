/**
 * \archivo borg-item.c
 * \brief definiciones de las listas de objetos que el borg está rastreando
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

#include "borg-item.h"

#ifdef ALLOW_BORG

#include "../init.h"
#include "../ui-menu.h"

#include "borg-io.h"
#include "borg-item-val.h"

/*
 * "Inventario" actual
 */
borg_item *borg_items;

/*
 * Arreglos de seguridad para simular mundos posibles
 */

borg_item *safe_items; /* "Inventario" de seguridad */

/*
 * obtener la inscripción (nota) del objeto
 */
const char *borg_get_note(const borg_item *item)
{
    if (item->note)
        return item->note;
    return "";
}

/*
 * Envía un comando para desinscribir el objeto número "i".
 */
void borg_deinscribe(int i)
{
    /* Está bien inscribir Mohos de Baba */
    if (borg_items[i].tval == TV_FOOD
        && borg_items[i].sval == sv_food_slime_mold)
        return;

    /* Etiquetarlo */
    borg_keypress('}');

    /* Elegir del inventario */
    if (i < INVEN_WIELD) {
        /* Elegir el objeto */
        borg_keypress(all_letters_nohjkl[i]);
    }

    /* Elegir del equipo */
    else {
        if (i < INVEN_FEET) {
            for (int j = 0; j < INVEN_WIELD; j++) {
                /* Ir al equipo (si es necesario) */
                if (borg_items[j].iqty && borg_items[j].note[0] == '{') {
                    borg_keypress('/');
                    break;
                }
            }
            /* Elegir el objeto */
            borg_keypress(all_letters_nohjkl[i - INVEN_WIELD]);

        } 
        else {
            for (int j = 0; j <= INVEN_FEET; j++) {
                /* Ir a la aljaba (si es necesario) */
                if (borg_items[j].iqty && borg_items[j].note[0] == '{') {
                    borg_keypress('|');
                    break;
                }
            }
            /* Elegir el objeto */
            borg_keypress('0' + (i - QUIVER_START));
        }
    }

    /* Puede pedir una confirmación */
    borg_keypress('y');
    borg_keypress('y');
}

/*
 * ayuda para dar el peso del objeto
 */
int16_t borg_item_weight(borg_item * item)
{
    return item->iqty * item->weight;
}

/*
 * asignar los arreglos de objetos
 */
void borg_init_item(void)
{
    /*** Arreglos de Objeto/Artículo ***/

    /* Crear el arreglo de inventario */
    borg_items = mem_zalloc(QUIVER_END * sizeof(borg_item));

    /* Crear el arreglo de inventario "seguro" */
    safe_items = mem_zalloc(QUIVER_END * sizeof(borg_item));
}

/*
 * liberar los arreglos de objetos
 */
void borg_free_item(void)
{
    /*** Arreglos de Objeto/Artículo ***/

    mem_free(safe_items);
    safe_items = NULL;
    mem_free(borg_items);
    borg_items = NULL;
}

#endif