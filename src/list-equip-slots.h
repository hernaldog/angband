/**
 * \file list-equip-slots.h
 * \brief types of slot for equipment
 *
 * Fields:
 * slot - The index name of the slot
 * acid_v - whether equipment in the slot needs checking for acid damage
 * name - whether the actual item name is mentioned when things happen to it
 * mention - description for when the slot is mentioned briefly
 * heavy describe - description for when the slot item is too heavy
 * describe - description for when the slot is described at length
 */
/* slot				acid_v	name	mention			heavy decribe	describe */
EQUIP(NONE,			false,	false,	"",				"",				"")
EQUIP(WEAPON,		false,	false,	"Empuñado",		"solo levantando",	"atacar monstruos con")
EQUIP(BOW,			false,	false,	"A distancia",		"solo sujetando",	"disparar proyectiles con")
EQUIP(RING,			false,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(AMULET,		false,	true,	"En %s",	"",				"llevar alrededor de tu %s")
EQUIP(LIGHT,		false,	false,	"Fuente de luz",	"",				"usar para iluminar tu camino")
EQUIP(BODY_ARMOR,	true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(CLOAK,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(SHIELD,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(HAT,			true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(GLOVES,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(BOOTS,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")