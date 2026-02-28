/**
 * \file cave.c
 * \brief Asignación de fragmentos y funciones de utilidad
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
#include "cave.h"
#include "cmds.h"
#include "cmd-core.h"
#include "game-event.h"
#include "game-world.h"
#include "init.h"
#include "mon-group.h"
#include "monster.h"
#include "obj-ignore.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "player-timed.h"
#include "trap.h"

struct feature *f_info;
struct chunk *cave = NULL;

/**
 * Matriz global para iterar a través de las "direcciones del teclado numérico".
 */
const int16_t ddd[9] =
{ 2, 8, 6, 4, 3, 1, 9, 7, 5 };

/**
 * Matrices globales para convertir la "dirección del teclado numérico" en "desplazamientos".
 */
const int16_t ddx[10] =
{ 0, -1, 0, 1, -1, 0, 1, -1, 0, 1 };

const int16_t ddy[10] =
{ 0, 1, 1, 1, 0, 0, 0, -1, -1, -1 };


const struct loc ddgrid[10] =
{ {0, 0}, {-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1},
  {1, -1} };

/**
 * Matrices globales para optimizar "ddx[ddd[i]]", "ddy[ddd[i]]" y
 * "loc(ddx[ddd[i]], ddy[ddd[i]])".
 *
 * Esto significa que cada entrada en esta matriz corresponde a la dirección
 * con el mismo índice de matriz en ddd[].
 */
const int16_t ddx_ddd[9] =
{ 0, 0, 1, -1, 1, -1, 1, -1, 0 };

const int16_t ddy_ddd[9] =
{ 1, -1, 0, 0, 1, 1, -1, -1, 0 };

const struct loc ddgrid_ddd[9] =
{{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {0, 0}};

/* Se pueden multiplicar estos por 45° o 1.5 en el reloj, ej. [6] -> 270° o las 9 en punto */
const int16_t clockwise_ddd[9] =
{ 8, 9, 6, 3, 2, 1, 4, 7, 5 };

const struct loc clockwise_grid[9] =
{{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, 0}};

/**
 * Precalcular un montón de llamadas a distance().
 *
 * El par de matrices dist_offsets_y[n] y dist_offsets_x[n] contienen los
 * desplazamientos de todas las ubicaciones con una distancia de n desde un punto central,
 * con un desplazamiento de (0,0) indicando que no hay más desplazamientos a esta distancia.
 *
 * Esto es, por supuesto, bastante ilegible, pero elimina múltiples bucles
 * de la versión anterior.
 *
 * Probablemente sea mejor reemplazar estas matrices con código para calcular
 * las matrices relevantes, incluso si el almacenamiento está preasignado en tamaños
 * fijos. Como mínimo, se debería incluir código que sea
 * capaz de generar y volcar estas matrices (ala "los()").  XXX XXX XXX
 */


static const int d_off_y_0[] =
{ 0 };

static const int d_off_x_0[] =
{ 0 };


static const int d_off_y_1[] =
{ -1, -1, -1, 0, 0, 1, 1, 1, 0 };

static const int d_off_x_1[] =
{ -1, 0, 1, -1, 1, -1, 0, 1, 0 };


static const int d_off_y_2[] =
{ -1, -1, -2, -2, -2, 0, 0, 1, 1, 2, 2, 2, 0 };

static const int d_off_x_2[] =
{ -2, 2, -1, 0, 1, -2, 2, -2, 2, -1, 0, 1, 0 };


static const int d_off_y_3[] =
{ -1, -1, -2, -2, -3, -3, -3, 0, 0, 1, 1, 2, 2,
  3, 3, 3, 0 };

static const int d_off_x_3[] =
{ -3, 3, -2, 2, -1, 0, 1, -3, 3, -3, 3, -2, 2,
  -1, 0, 1, 0 };


static const int d_off_y_4[] =
{ -1, -1, -2, -2, -3, -3, -3, -3, -4, -4, -4, 0,
  0, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 0 };

static const int d_off_x_4[] =
{ -4, 4, -3, 3, -2, -3, 2, 3, -1, 0, 1, -4, 4,
  -4, 4, -3, 3, -2, -3, 2, 3, -1, 0, 1, 0 };


static const int d_off_y_5[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -4, -4, -5, -5,
  -5, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 0 };

static const int d_off_x_5[] =
{ -5, 5, -4, 4, -4, 4, -2, -3, 2, 3, -1, 0, 1,
  -5, 5, -5, 5, -4, 4, -4, 4, -2, -3, 2, 3, -1,
  0, 1, 0 };


static const int d_off_y_6[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -5, -5,
  -6, -6, -6, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
  5, 5, 6, 6, 6, 0 };

static const int d_off_x_6[] =
{ -6, 6, -5, 5, -5, 5, -4, 4, -2, -3, 2, 3, -1,
  0, 1, -6, 6, -6, 6, -5, 5, -5, 5, -4, 4, -2,
  -3, 2, 3, -1, 0, 1, 0 };


static const int d_off_y_7[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -5, -5,
  -6, -6, -6, -6, -7, -7, -7, 0, 0, 1, 1, 2, 2, 3,
  3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 0 };

static const int d_off_x_7[] =
{ -7, 7, -6, 6, -6, 6, -5, 5, -4, -5, 4, 5, -2,
  -3, 2, 3, -1, 0, 1, -7, 7, -7, 7, -6, 6, -6,
  6, -5, 5, -4, -5, 4, 5, -2, -3, 2, 3, -1, 0,
  1, 0 };


static const int d_off_y_8[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6,
  -6, -6, -7, -7, -7, -7, -8, -8, -8, 0, 0, 1, 1,
  2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
  8, 8, 8, 0 };

static const int d_off_x_8[] =
{ -8, 8, -7, 7, -7, 7, -6, 6, -6, 6, -4, -5, 4,
  5, -2, -3, 2, 3, -1, 0, 1, -8, 8, -8, 8, -7,
  7, -7, 7, -6, 6, -6, 6, -4, -5, 4, 5, -2, -3,
  2, 3, -1, 0, 1, 0 };


static const int d_off_y_9[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6,
  -7, -7, -7, -7, -8, -8, -8, -8, -9, -9, -9, 0,
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7,
  7, 8, 8, 8, 8, 9, 9, 9, 0 };

static const int d_off_x_9[] =
{ -9, 9, -8, 8, -8, 8, -7, 7, -7, 7, -6, 6, -4,
  -5, 4, 5, -2, -3, 2, 3, -1, 0, 1, -9, 9, -9,
  9, -8, 8, -8, 8, -7, 7, -7, 7, -6, 6, -4, -5,
  4, 5, -2, -3, 2, 3, -1, 0, 1, 0 };


const int *dist_offsets_y[10] =
{
	d_off_y_0, d_off_y_1, d_off_y_2, d_off_y_3, d_off_y_4,
	d_off_y_5, d_off_y_6, d_off_y_7, d_off_y_8, d_off_y_9
};

const int *dist_offsets_x[10] =
{
	d_off_x_0, d_off_x_1, d_off_x_2, d_off_x_3, d_off_x_4,
	d_off_x_5, d_off_x_6, d_off_x_7, d_off_x_8, d_off_x_9
};


/**
 * Dada una dirección central en la posición [nº de dir][0], devuelve una serie
 * de direcciones que irradian a ambos lados desde la dirección central
 * todo el camino de vuelta a su parte trasera.
 *
 * Las direcciones laterales vienen en pares; por ejemplo, las direcciones '1' y '3'
 * flanquean la dirección '2'. El código debería saber qué lado considerar
 * primero. Si es el izquierdo, debe sumar 10 a la dirección central para
 * acceder a la segunda parte de la tabla.
 */
const uint8_t side_dirs[20][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},	/* sesgo derecho */
	{1, 4, 2, 7, 3, 8, 6, 9},
	{2, 1, 3, 4, 6, 7, 9, 8},
	{3, 2, 6, 1, 9, 4, 8, 7},
	{4, 7, 1, 8, 2, 9, 3, 6},
	{5, 5, 5, 5, 5, 5, 5, 5},
	{6, 3, 9, 2, 8, 1, 7, 4},
	{7, 8, 4, 9, 1, 6, 2, 3},
	{8, 9, 7, 6, 4, 3, 1, 2},
	{9, 6, 8, 3, 7, 2, 4, 1},

	{0, 0, 0, 0, 0, 0, 0, 0},	/* sesgo izquierdo */
	{1, 2, 4, 3, 7, 6, 8, 9},
	{2, 3, 1, 6, 4, 9, 7, 8},
	{3, 6, 2, 9, 1, 8, 4, 7},
	{4, 1, 7, 2, 8, 3, 9, 6},
	{5, 5, 5, 5, 5, 5, 5, 5},
	{6, 9, 3, 8, 2, 7, 1, 4},
	{7, 4, 8, 1, 9, 2, 6, 3},
	{8, 7, 9, 4, 6, 1, 3, 2},
	{9, 8, 6, 7, 3, 4, 2, 1}
};

/**
 * Dadas una ubicación de "inicio" y "final", extraer una "dirección",
 * que moverá un paso desde el "inicio" hacia el "final".
 *
 * Nótese que usamos movimiento "diagonal" siempre que sea posible.
 *
 * Devolvemos DIR_NONE si no se necesita movimiento.
 */
int motion_dir(struct loc start, struct loc finish)
{
	/* No se requiere movimiento */
	if (loc_eq(start, finish)) return (DIR_NONE);

	/* Sur o Norte */
	if (start.x == finish.x) return ((start.y < finish.y) ? DIR_S : DIR_N);

	/* Este u Oeste */
	if (start.y == finish.y) return ((start.x < finish.x) ? DIR_E : DIR_W);

	/* Sureste o Suroeste */
	if (start.y < finish.y) return ((start.x < finish.x) ? DIR_SE : DIR_SW);

	/* Noreste o Noroeste */
	if (start.y > finish.y) return ((start.x < finish.x) ? DIR_NE : DIR_NW);

	/* Paranoia */
	return (DIR_NONE);
}

/**
 * Dada una casilla y una dirección, extraer la casilla adyacente en esa dirección
 */
struct loc next_grid(struct loc grid, int dir)
{
	return loc(grid.x + ddgrid[dir].x, grid.y + ddgrid[dir].y);
}

/**
 * Encontrar un índice de característica del terreno por su nombre imprimible.
 */
int lookup_feat(const char *name)
{
	int i;

	/* Buscarlo */
	for (i = 0; i < FEAT_MAX; i++) {
		struct feature *feat = &f_info[i];
		if (!feat->name)
			continue;

		/* Probar igualdad */
		if (streq(name, feat->name))
			return i;
	}

	/* Fallar horriblemente */
	quit_fmt("Fallo al encontrar la caracteristica de terreno %s", name);
	return -1;
}

static const char *feat_code_list[] = {
	#define FEAT(x) #x,
	#include "list-terrain.h"
	#undef FEAT
	NULL
};

/**
 * Encontrar una característica del terreno por su nombre de código.
 */
int lookup_feat_code(const char *code)
{
	int i = 0;

	while (1) {
		assert(i >= 0 && i < (int) N_ELEMENTS(feat_code_list));
		if (!feat_code_list[i]) {
			return -1;
		}
		if (streq(code, feat_code_list[i])) {
			break;
		}
		++i;
	}
	return i;
}

/**
 * Devolver el nombre de código de la característica, especificado como un índice.
 * Devolverá NULL si el índice es inválido.
 */
const char *get_feat_code_name(int idx)
{
	return (idx < 0 || idx >= FEAT_MAX) ? NULL : feat_code_list[idx];
}

/**
 * Asignar un nuevo fragmento del mundo
 */
struct chunk *cave_new(int height, int width) {
	int y, x;

	struct chunk *c = mem_zalloc(sizeof *c);
	c->height = height;
	c->width = width;
	c->feat_count = mem_zalloc((FEAT_MAX + 1) * sizeof(int));

	c->squares = mem_zalloc(c->height * sizeof(struct square*));
	c->noise.grids = mem_zalloc(c->height * sizeof(uint16_t*));
	c->scent.grids = mem_zalloc(c->height * sizeof(uint16_t*));
	for (y = 0; y < c->height; y++) {
		c->squares[y] = mem_zalloc(c->width * sizeof(struct square));
		for (x = 0; x < c->width; x++) {
			c->squares[y][x].info = mem_zalloc(SQUARE_SIZE * sizeof(bitflag));
		}
		c->noise.grids[y] = mem_zalloc(c->width * sizeof(uint16_t));
		c->scent.grids[y] = mem_zalloc(c->width * sizeof(uint16_t));
	}

	c->objects = mem_zalloc(OBJECT_LIST_SIZE * sizeof(struct object*));
	c->obj_max = OBJECT_LIST_SIZE - 1;

	c->monsters = mem_zalloc(z_info->level_monster_max *sizeof(struct monster));
	c->mon_max = 1;
	c->mon_current = -1;

	c->monster_groups = mem_zalloc(z_info->level_monster_max *
								   sizeof(struct monster_group*));

	c->turn = turn;
	return c;
}

/**
 * Liberar una lista enlazada de conexiones de cueva.
 */
void cave_connectors_free(struct connector *join)
{
	while (join) {
		struct connector *current = join;

		join = current->next;
		mem_free(current->info);
		mem_free(current);
	}
}

/**
 * Liberar un fragmento
 */
void cave_free(struct chunk *c) {
	struct chunk *p_c = (c == cave && player) ? player->cave : NULL;
	int y, x, i;

	cave_connectors_free(c->join);

	/* Buscar objetos huérfanos y eliminarlos. */
	for (i = 1; i < c->obj_max; i++) {
		if (c->objects[i] && loc_is_zero(c->objects[i]->grid)) {
			object_delete(c, p_c, &c->objects[i]);
		}
	}

	for (y = 0; y < c->height; y++) {
		for (x = 0; x < c->width; x++) {
			mem_free(c->squares[y][x].info);
			if (c->squares[y][x].trap)
				square_free_trap(c, loc(x, y));
			if (c->squares[y][x].obj)
				object_pile_free(c, p_c, c->squares[y][x].obj);
		}
		mem_free(c->squares[y]);
		mem_free(c->noise.grids[y]);
		mem_free(c->scent.grids[y]);
	}
	mem_free(c->squares);
	mem_free(c->noise.grids);
	mem_free(c->scent.grids);

	mem_free(c->feat_count);
	mem_free(c->objects);
	mem_free(c->monsters);
	mem_free(c->monster_groups);
	if (c->name)
		string_free(c->name);
	mem_free(c);
}


/**
 * Introducir un objeto en la lista de objetos para el nivel/fragmento actual.
 * Esta función es robusta contra la inclusión de duplicados o no-objetos.
 */
void list_object(struct chunk *c, struct object *obj)
{
	int i, newsize;

	/* Verificar duplicados y objetos ya eliminados o combinados */
	if (!obj) return;
	for (i = 1; i < c->obj_max; i++)
		if (c->objects[i] == obj)
			return;

	/* Poner objetos en los huecos de la lista de objetos */
	for (i = 1; i < c->obj_max; i++) {
		/* Si hay un objeto conocido, saltar esta ranura */
		if ((c == cave) && player->cave && player->cave->objects[i]) {
			continue;
		}

		/* Poner el objeto en un hueco */
		if (c->objects[i] == NULL) {
			c->objects[i] = obj;
			obj->oidx = i;
			return;
		}
	}

	/* Extender la lista */
	newsize = (c->obj_max + OBJECT_LIST_INCR + 1) * sizeof(struct object*);
	c->objects = mem_realloc(c->objects, newsize);
	c->objects[c->obj_max] = obj;
	obj->oidx = c->obj_max;
	for (i = c->obj_max + 1; i <= c->obj_max + OBJECT_LIST_INCR; i++)
		c->objects[i] = NULL;
	c->obj_max += OBJECT_LIST_INCR;

	/* Si estamos en el nivel actual, extender la lista conocida */
	if ((c == cave) && player->cave) {
		player->cave->objects = mem_realloc(player->cave->objects, newsize);
		for (i = player->cave->obj_max; i <= c->obj_max; i++)
			player->cave->objects[i] = NULL;
		player->cave->obj_max = c->obj_max;
	}
}

/**
 * Eliminar un objeto de la lista de objetos para el nivel/fragmento actual.
 * Esta función es robusta contra la eliminación de objetos no listados.
 */
void delist_object(struct chunk *c, struct object *obj)
{
	if (!obj->oidx) return;
	assert(c->objects[obj->oidx] == obj);

	/* No eliminar un objeto real si todavía tiene un objeto conocido listado */
	if ((c == cave) && player->cave->objects[obj->oidx]) return;

	c->objects[obj->oidx] = NULL;
	obj->oidx = 0;
}

/**
 * Verificar la consistencia de una lista de objetos o un par de listas de objetos
 *
 * Si es una lista, verificar que los objetos listados se relacionan correctamente
 * con las ubicaciones de los objetos.
 */
void object_lists_check_integrity(struct chunk *c, struct chunk *c_k)
{
	int i;
	if (c_k) {
		assert(c->obj_max == c_k->obj_max);
		for (i = 0; i < c->obj_max; i++) {
			struct object *obj = c->objects[i];
			struct object *known_obj = c_k->objects[i];
			if (obj) {
				assert(obj->oidx == i);
				if (!loc_is_zero(obj->grid))
					assert(pile_contains(square_object(c, obj->grid), obj));
			}
			if (known_obj) {
				assert (obj);
				if (player->upkeep->playing) {
					assert(known_obj == obj->known);
				}
				if (!loc_is_zero(known_obj->grid))
					assert (pile_contains(square_object(c_k, known_obj->grid),
										  known_obj));
				assert (known_obj->oidx == i);
			}
		}
	} else {
		for (i = 0; i < c->obj_max; i++) {
			struct object *obj = c->objects[i];
			if (obj) {
				assert(obj->oidx == i);
				if (!loc_is_zero(obj->grid))
					assert(pile_contains(square_object(c, obj->grid), obj));
			}
		}
	}
}

/**
 * Función estándar "encuéntrame una ubicación", ¡ahora con todas las salidas legales!
 *
 * Obtiene una ubicación legal dentro de la distancia dada de la ubicación inicial,
 * y con "los()" desde la ubicación de origen a la ubicación de destino.
 *
 * Esta función a menudo se llama desde un bucle que busca
 * ubicaciones mientras aumenta la distancia "d".
 *
 * need_los determina si se necesita línea de visión
 */
void scatter(struct chunk *c, struct loc *place, struct loc grid, int d,
			 bool need_los)
{
	(void) scatter_ext(c, place, 1, grid, d, need_los, NULL);
}


/**
 * Intentar encontrar un número dado de ubicaciones distintas, seleccionadas
 * aleatoriamente, que estén dentro de una distancia dada de una casilla,
 * completamente dentro de los límites y, opcionalmente, estén en la línea de
 * visión de la casilla dada y cumplan una condición adicional.
 * \param c Es el fragmento a buscar.
 * \param places Apunta al almacenamiento para las ubicaciones encontradas. Ese almacenamiento
 * debe tener espacio para al menos n casillas.
 * \param n Es el número de ubicaciones a encontrar.
 * \param grid Es la ubicación a usar como origen para la búsqueda.
 * \param d Es la distancia máxima, en casillas, que una ubicación puede estar de
 * grid y aún ser aceptada.
 * \param need_los Si es true, cualquier ubicación encontrada también estará en la línea de
 * visión desde grid.
 * \param pred Si no es NULL, evaluar esa función en una ubicación encontrada, lct,
 * devolverá true, ej. (*pred)(c, lct) será true.
 * \return Devolver el número de ubicaciones encontradas. Ese número será menor
 * o igual que n si n no es negativo y será cero si n es negativo.
 */
int scatter_ext(struct chunk *c, struct loc *places, int n, struct loc grid,
		int d, bool need_los, bool (*pred)(struct chunk *, struct loc))
{
	int result = 0;
	/* Almacena ubicaciones factibles. */
	struct loc *feas = mem_alloc(MIN(c->width, (1 + 2 * MAX(0, d)))
			* (size_t) MIN(c->height, (1 + 2 * MAX(0, d)))
			* sizeof(*feas));
	int nfeas = 0;
	struct loc g;

	/* Obtener las ubicaciones factibles. */
	for (g.y = grid.y - d; g.y <= grid.y + d; ++g.y) {
		for (g.x = grid.x - d; g.x <= grid.x + d; ++g.x) {
			if (!square_in_bounds_fully(c, g)) continue;
			if (d > 1 && distance(grid, g) > d) continue;
			if (need_los && !los(c, grid, g)) continue;
			if (pred && !(*pred)(c, g)) continue;
			feas[nfeas] = g;
			++nfeas;
		}
	}

	/* Ensamblar el resultado. */
	while (result < n && nfeas > 0) {
		/* Elegir uno al azar y añadirlo a la lista de salida. */
		int choice = randint0(nfeas);

		places[result] = feas[choice];
		++result;
		/* Desplazar el último factible para reemplazar el seleccionado. */
		--nfeas;
		feas[choice] = feas[nfeas];
	}

	mem_free(feas);
	return result;
}

/**
 * Obtener un monstruo en el nivel actual por su índice.
 */
struct monster *cave_monster(struct chunk *c, int idx) {
	if (idx <= 0) return NULL;
	return &c->monsters[idx];
}

/**
 * El número máximo de monstruos permitidos en el nivel.
 */
int cave_monster_max(struct chunk *c) {
	return c->mon_max;
}

/**
 * El número actual de monstruos presentes en el nivel.
 */
int cave_monster_count(struct chunk *c) {
	return c->mon_cnt;
}

/**
 * Devolver el número de casillas coincidentes alrededor (o debajo) del personaje.
 * \param grid Si no es NULL, *grid se establece en la ubicación de la última coincidencia.
 * \param test Es el predicado a usar al probar una coincidencia.
 * \param under Si es true, la casilla del personaje también se prueba.
 * Solo prueba casillas que son conocidas y están completamente dentro de los límites.
 */
int count_feats(struct loc *grid,
				bool (*test)(struct chunk *c, struct loc grid), bool under)
{
	int d;
	struct loc grid1;
	int count = 0; /* Contar cuántas coincidencias */

	/* Verificar alrededor (y debajo) del personaje */
	for (d = 0; d < 9; d++) {
		/* si no se busca debajo del jugador continuar */
		if ((d == 8) && !under) continue;

		/* Extraer ubicación (legal) adyacente */
		grid1 = loc_sum(player->grid, ddgrid_ddd[d]);

		/* Paranoia */
		if (!square_in_bounds_fully(cave, grid1)) continue;

		/* Debe tener conocimiento */
		if (!square_isknown(cave, grid1)) continue;

		/* No se busca esta característica; probar contra la memoria del jugador */
		if (!((*test)(player->cave, grid1))) continue;

		/* Contarlo */
		++count;

		/* Recordar la ubicación de la última coincidencia */
		if (grid) {
			*grid = grid1;
		}
	}

	/* Todo hecho */
	return count;
}

/**
 * Devolver el número de casillas coincidentes alrededor de una ubicación.
 * \param match Si no es NULL, *match se establece en la ubicación de la última coincidencia.
 * \param c Es el fragmento a usar.
 * \param grid Es la ubicación cuyos vecinos serán probados.
 * \param test Es el predicado a usar al probar una coincidencia.
 * \param under Si es true, grid también se prueba.
 */
int count_neighbors(struct loc *match, struct chunk *c, struct loc grid,
	bool (*test)(struct chunk *c, struct loc grid), bool under)
{
	int dlim = (under) ? 9 : 8;
	int count = 0; /* Contar cuántas coincidencias */
	int d;
	struct loc grid1;

	/* Verificar los vecinos de la casilla y, si under es true, la casilla */
	for (d = 0; d < dlim; d++) {
		/* Extraer ubicación (legal) adyacente */
		grid1 = loc_sum(grid, ddgrid_ddd[d]);
		if (!square_in_bounds(c, grid1)) continue;

		/* Rechazar aquellos que no coinciden */
		if (!((*test)(c, grid1))) continue;

		/* Contarlo */
		++count;

		/* Recordar la ubicación de la última coincidencia */
		if (match) {
			*match = grid1;
		}
	}

	/* Todo hecho */
	return count;
}

struct loc cave_find_decoy(struct chunk *c)
{
	return c->decoy;
}