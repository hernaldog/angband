/**
 * \file list-origins.h
 * \brief Lista de orígenes de objetos
 */
ORIGIN(NONE,		-1,	"")
ORIGIN(FLOOR,		1,	"Hallado en el suelo %s")
ORIGIN(CHEST,		1,	"Tomado de un cofre encontrado %s")
ORIGIN(SPECIAL,		1,	"Hallado en el suelo de una habitación especial %s")
ORIGIN(PIT,			1,	"Hallado en el suelo de un foso %s")
ORIGIN(VAULT,		1,	"Hallado en el suelo de una bóveda %s")
ORIGIN(LABYRINTH,	1,	"Hallado en el suelo de un laberinto %s")
ORIGIN(CAVERN,		1,	"Hallado en el suelo de una caverna %s")
ORIGIN(RUBBLE,		1,	"Hallado bajo unos escombros %s")
ORIGIN(MIXED,		-1,	"")                 /* montón con orígenes mixtos */
ORIGIN(DROP,		2,	"Arrojado por %s %s") /* objetos dejados por monstruos normales */
ORIGIN(DROP_SPECIAL,2,	"Arrojado por %s %s") /* de monstruos en habitaciones especiales */
ORIGIN(DROP_PIT,	2,	"Arrojado por %s %s") /* de monstruos en fosos/nidos */
ORIGIN(DROP_VAULT,	2,	"Arrojado por %s %s") /* de monstruos en bóvedas */
ORIGIN(STATS,		-1,	"")  /* ^ solo lo anterior es considerado por main-stats */
ORIGIN(ACQUIRE,		1,	"Conjurado por medios mágicos %s")
ORIGIN(STORE,		0,	"Comprado en una tienda")
ORIGIN(STOLEN,		-1,	"")
ORIGIN(BIRTH,		0,	"Una herencia de tu familia")
ORIGIN(CHEAT,		0,	"Creado por opción de depuración")
ORIGIN(DROP_BREED,	2,	"Arrojado por %s %s") /* de criadores */
ORIGIN(DROP_SUMMON,	2,	"Arrojado por %s %s") /* de invocaciones en combate */
ORIGIN(DROP_UNKNOWN,1,	"Arrojado por un monstruo desconocido %s")
ORIGIN(DROP_POLY,	2,	"Arrojado por %s %s") /* de polimorfeados */
ORIGIN(DROP_MIMIC,	2,	"Arrojado por %s %s") /* de imitadores (mimics) */
ORIGIN(DROP_WIZARD,	2,	"Arrojado por %s %s") /* de invocaciones en modo mago */