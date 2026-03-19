/**
   \file obj-desc.h
   \brief Crear descripciones de nombres de objetos
 *
 * Copyright (c) 1997 - 2007 Colaboradores de Angband
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

#ifndef OBJECT_DESC_H
#define OBJECT_DESC_H

/**
 * Modos para object_desc().
 */
enum {
	ODESC_BASE   = 0x00,   /*!< Describir solo el nombre base */
	ODESC_COMBAT = 0x01,   /*!< Mostrar también las bonificaciones de combate */
	ODESC_EXTRA  = 0x02,   /*!< Mostrar cargas/inscripciones/pvals */

	ODESC_FULL   = ODESC_COMBAT | ODESC_EXTRA,
	                       /*!< Mostrar la descripción completa */

	ODESC_STORE  = 0x04,   /*!< Esta es una descripción en la tienda */
	ODESC_PLURAL = 0x08,   /*!< Siempre pluralizar */
	ODESC_SINGULAR    = 0x10,    /*!< Siempre singular */
	ODESC_SPOIL  = 0x20,    /*!< Mostrar independientemente del conocimiento del jugador */
	ODESC_PREFIX = 0x40,   /* */

	ODESC_CAPITAL = 0x80,	/*!< Poner en mayúscula el nombre del objeto */
	ODESC_TERSE = 0x100,  	/*!< Usar nombres concisos */
	ODESC_NOEGO = 0x200,  	/*!< No mostrar nombres de ego */
	ODESC_ALTNUM = 0x400	/*!< Usar los 16 bits superiores del modo en lugar
					de obj->number como el número
					de objetos; no es completamente compatible
					con ODESC_EXTRA */
};


extern const char *inscrip_text[];

void object_base_name(char *buf, size_t max, int tval, bool plural);
void object_kind_name(char *buf, size_t max, const struct object_kind *kind,
					  bool easy_know);
size_t obj_desc_name_format(char *buf, size_t max, size_t end, const char *fmt,
							const char *modstr, bool pluralise);
size_t object_desc(char *buf, size_t max, const struct object *obj,
	uint32_t mode, const struct player *p);

#endif /* OBJECT_DESC_H */