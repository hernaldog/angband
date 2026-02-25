/**
 * \file list-equip-slots.h
 * \brief tipos de ranura para el equipo
 *
 * Campos:
 * slot - El nombre índice de la ranura
 * acid_v - si el equipo en la ranura necesita comprobación de daño por ácido
 * name - si se menciona el nombre real del objeto cuando le suceden cosas
 * mention - descripción para cuando se menciona la ranura brevemente
 * heavy describe - descripción para cuando el objeto de la ranura es demasiado pesado
 * describe - descripción para cuando la ranura se describe extensamente
 */
/* ranura			acid_v	nombre	mencion				desc_pesado		descripcion */
EQUIP(NONE,			false,	false,	"",					"",				"")
EQUIP(WEAPON,		false,	false,	"Empuñando",		"solo levantando",	"atacar monstruos con")
EQUIP(BOW,			false,	false,	"A distancia",		"solo sujetando",	"disparar proyectiles con")
EQUIP(RING,			false,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(AMULET,		false,	true,	"Alrededor de %s",	"",				"llevar alrededor de tu %s")
EQUIP(LIGHT,		false,	false,	"Fuente de luz",	"",				"usar para iluminar tu camino")
EQUIP(BODY_ARMOR,	true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(CLOAK,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(SHIELD,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(HAT,			true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(GLOVES,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")
EQUIP(BOOTS,		true,	true,	"En %s",			"",				"llevar puesto en tu %s")