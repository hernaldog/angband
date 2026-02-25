/**
 * \file list-options.h
 * \brief opciones
 *
 * Actualmente, si hay más de 21 de cualquier tipo de opción, las últimas
 * serán ignoradas.
 * Las opciones de trampa deben ser seguidas por las opciones de puntuación correspondientes.
 */

/* nombre                 descripción
tipo      normal */
OP(none,                  "",
SPECIAL, false)
OP(rogue_like_commands,   "Usar teclas estilo roguelike",
INTERFACE, false)
OP(autoexplore_commands,  "Usar comandos de autoexploración",
INTERFACE, false)
OP(use_sound,             "Usar sonido",
INTERFACE, false)
OP(show_damage,           "Mostrar el daño que el jugador inflige",
INTERFACE, false)
OP(use_old_target,        "Usar el objetivo antiguo por defecto",
INTERFACE, false)
OP(pickup_always,         "Recoger objetos siempre",
INTERFACE, false)
OP(pickup_inven,          "Recoger obj siempre que coincidan con el inventario",
INTERFACE, true)
OP(show_flavors,          "Mostrar sabores en las descripciones de objetos",
INTERFACE, false)
OP(show_target,           "Resaltar el objetivo con el cursor",
INTERFACE, true)
OP(highlight_player,      "Resaltar al jugador con el cursor entre turnos",
INTERFACE, false)
OP(disturb_near,          "Molestar cuando un monstruo visible se mueve",
INTERFACE, true)
OP(solid_walls,           "Mostrar paredes como bloques sólidos",
INTERFACE, false)
OP(hybrid_walls,          "Mostrar paredes con fondo sombreado",
INTERFACE, false)
OP(view_yellow_light,     "Color: Iluminar la luz de antorcha en amarillo",
INTERFACE, false)
OP(animate_flicker,       "Color: Hacer brillar las cosas multicolores",
INTERFACE, false)
OP(center_player,         "Centrar el mapa continuamente",
INTERFACE, false)
OP(purple_uniques,        "Color: Mostrar monstruos únicos en púrpura",
INTERFACE, false)
OP(auto_more,             "Limpiar automáticamente los avisos '-más-'",
INTERFACE, false)
OP(hp_changes_color,      "Color: Color del jugador indica % de puntos de golpe",
INTERFACE, true)
OP(mouse_movement,        "Permitir clics del ratón para mover al jugador",
INTERFACE, true)
OP(notify_recharge,       "Notificar al recargar objeto",
INTERFACE, false)
OP(effective_speed,       "Mostrar velocidad efectiva como multiplicador",
INTERFACE, false)
OP(cheat_hear,            "Trampa: Espiar la creación de monstruos",
CHEAT, false)
OP(score_hear,            "Puntuación: Espiar la creación de monstruos",
SCORE, false)
OP(cheat_room,            "Trampa: Espiar la creación de mazmorras",
CHEAT, false)
OP(score_room,            "Puntuación: Espiar la creación de mazmorras",
SCORE, false)
OP(cheat_xtra,            "Trampa: Espiar otra cosa",
CHEAT, false)
OP(score_xtra,            "Puntuación: Espiar otra cosa",
SCORE, false)
OP(cheat_live,            "Trampa: Permitir evitar la muerte",
CHEAT, false)
OP(score_live,            "Puntuación: Permitir evitar la muerte",
SCORE, false)
OP(birth_randarts,        "Generar nuevos artefactos aleatorios",
BIRTH, false)
OP(birth_connect_stairs,  "Generar escaleras conectadas",
BIRTH, true)
OP(birth_force_descend,   "Forzar descenso del jugador (sin escal arriba)",
BIRTH, false)
OP(birth_no_recall,       "Palabra de Retorno no tiene efecto",
BIRTH, false)
OP(birth_no_artifacts,    "Restringir la creación de artefactos",
BIRTH, false)
OP(birth_stacking,        "Apilar objetos en el suelo",
BIRTH, true)
OP(birth_lose_arts,       "Perder artefactos al salir del nivel",
BIRTH, false)
OP(birth_feelings,        "Mostrar sensaciones de nivel",
BIRTH, true)
OP(birth_no_selling,      "Aumentar caída de oro pero sin venta",
BIRTH, true)
OP(birth_start_kit,       "Comenzar con un kit de equipo útil",
BIRTH, true)
OP(birth_ai_learn,        "Los monstruos aprenden de sus errores",
BIRTH, true)
OP(birth_know_runes,      "Conocer todas las runas al nacer",
BIRTH, false)
OP(birth_know_flavors,    "Conocer todos los sabores al nacer",
BIRTH, false)
OP(birth_levels_persist,  "Niveles persistentes (experimental)",
BIRTH, false)
OP(birth_percent_damage,  "Para-dañar es un porcent de dados (experimental)",
BIRTH, false)