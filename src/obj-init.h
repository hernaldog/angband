/**
 * \file obj-init.h
 * \brief Rutinas de inicialización de objetos
 *
 * Copyright (c) 1997 Ben Harrison
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
 *
 * Este archivo se utiliza para inicializar varias variables y arreglos para
 * objetos en el juego Angband.
 *
 * Varios de los arreglos para Angband se construyen a partir de archivos de datos en el
 * directorio "lib/gamedata".
 */


#ifndef OBJECT_INIT_H_
#define OBJECT_INIT_H_

extern struct file_parser projection_parser;
extern struct file_parser object_base_parser;
extern struct file_parser slay_parser;
extern struct file_parser brand_parser;
extern struct file_parser curse_parser;
extern struct file_parser object_parser;
extern struct file_parser act_parser;
extern struct file_parser ego_parser;
extern struct file_parser artifact_parser;
extern struct file_parser randart_parser;
extern struct file_parser object_property_parser;

#endif /* OBJECT_INIT_H_ */