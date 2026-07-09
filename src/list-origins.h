/**
 * \file list-origins.h
 * \brief List of object origins
 */
ORIGIN(NONE,		-1,	"")
ORIGIN(FLOOR,		1,	"Found lying on the floor %s")
ORIGIN(CHEST,		1,	"Taken from a chest found %s")
ORIGIN(SPECIAL,		1,	"Found lying on the floor of a special room %s")
ORIGIN(PIT,			1,	"Found lying on the floor of a pit %s")
ORIGIN(VAULT,		1,	"Found lying on the floor of a vault %s")
ORIGIN(LABYRINTH,	1,	"Found lying on the floor of a labyrinth %s")
ORIGIN(CAVERN,		1,	"Found lying on the floor of a cavern %s")
ORIGIN(RUBBLE,		1,	"Found under some rubble %s")
ORIGIN(MIXED,		-1,	"")                 /* stack with mixed origins */
ORIGIN(DROP,		2,	"Dropped by %s %s") /* objetos dejados por monstruos normales */
ORIGIN(DROP_SPECIAL,2,	"Dropped by %s %s") /* de monstruos en habitaciones especiales */
ORIGIN(DROP_PIT,	2,	"Dropped by %s %s") /* de monstruos en fosos/nidos */
ORIGIN(DROP_VAULT,	2,	"Dropped by %s %s") /* de monstruos en bóvedas */
ORIGIN(STATS,		-1,	"")  /* ^ solo lo anterior es considerado por main-stats */
ORIGIN(ACQUIRE,		1,	"Conjured into existence %s")
ORIGIN(STORE,		0,	"Bought from a store")
ORIGIN(STOLEN,		-1,	"")
ORIGIN(BIRTH,		0,	"An inheritance from your family")
ORIGIN(CHEAT,		0,	"Created by debug option")
ORIGIN(DROP_BREED,	2,	"Dropped by %s %s") /* de criadores */
ORIGIN(DROP_SUMMON,	2,	"Dropped by %s %s") /* de invocaciones en combate */
ORIGIN(DROP_UNKNOWN,1,	"Dropped by an unknown monster %s")
ORIGIN(DROP_POLY,	2,	"Dropped by %s %s") /* de polimorfeados */
ORIGIN(DROP_MIMIC,	2,	"Dropped by %s %s") /* de imitadores (mimics) */
ORIGIN(DROP_WIZARD,	2,	"Dropped by %s %s") /* de invocaciones en modo mago */