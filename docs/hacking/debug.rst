======================================
Descripciones de Comandos de Depuración
======================================

Creación de Objetos
===================

Crear un objeto ``c``
  Muestra un menú para crear cualquier objeto y lo deja caer en el suelo.

Crear un artefacto ``C``
  Muestra un menú para crear cualquier artefacto y lo deja caer en el suelo.

Crear un objeto bueno ``g``
  Solicita la cantidad de objetos a crear y luego genera esa cantidad de
  objetos buenos en los alrededores.

Crear un objeto muy bueno ``v``
  Solicita la cantidad de objetos a crear y luego genera esa cantidad de
  objetos muy buenos ("excelentes") en los alrededores.

Jugar con un objeto ``o``
  Permite modificar un objeto regenerándolo aleatoriamente como un objeto
  normal, bueno o excelente, o editar sus atributos, incluyendo cantidad, tipo
  de ego, presencia o ausencia de maldiciones, valores de combate y
  modificadores. También hay una opción de estadísticas para evaluar qué tan
  probable sería generar objetos peores, mejores o equivalentes del mismo tipo.

Probar tipo ``V``
  Solicita un tval como número entero. Para ese tval, crea un objeto de cada
  sval y lo deja caer en los alrededores. Existe una opción similar con el
  comando ``c``, pero este genera cualquier artefacto instantáneo asociado a
  un tval y selecciona el tval por número en lugar de por nombre.

Detección / Información
=======================

Detectar todo ``d``
  Detecta todas las trampas, puertas, escaleras, tesoros y monstruos cercanos.

Mapeado mágico ``m``
  Mapea la mazmorra cercana.

Aprender sobre objetos ``l``
  Te hace "consciente" de todos los objetos con nivel menor o igual a 100.

Recuerdo de monstruos ``r``
  Te da el recuerdo completo de todos los monstruos o de uno elegido.

Borrar recuerdo ``W``
  Reinicia el recuerdo de todos los monstruos o de uno elegido.

Revelar monstruos ``u``
  Revela todos los monstruos.

Iluminar el nivel con luz mágica ``w``
  Ilumina el nivel entero, como la Poción de Iluminación.

Crear spoilers ``"``
  Permite crear un archivo de spoilers para objetos o monstruos.

Teletransportación
==================

Teletransportar nivel ``j``
  Permite teletransportarte a cualquier nivel de la mazmorra al instante.

Puerta de Fase ``p``
  Te teletransporta hasta 10 espacios de distancia.

Teletransporte ``t``
  Te teletransporta hasta 100 espacios de distancia.

Teletransporte al objetivo ``b``
  Te teletransporta a la casilla objetivo (o cerca de ella, si está ocupada).

Mejora del Personaje
====================

Curar todas las dolencias ``a``
  Elimina todas las maldiciones, restaura todas las estadísticas, xp, hp y sp,
  cura todos los efectos negativos y sacia tu hambre.

Avanzar el personaje ``A``
  Lleva tu personaje al nivel 50, maximiza todas las estadísticas y te da un
  millón de monedas de oro.

Editar personaje ``e``
  Permite especificar tus estadísticas base, xp y oro.

Aumentar experiencia ``x``
  Solicita una cantidad, hasta 9999, para añadir a tu experiencia actual.

Recalcular puntos de vida ``h``
  Recalcula tus puntos de vida.

Monstruos
=========

Invocar monstruo ``n``
  Solicita el nombre o índice entero de un monstruo y luego lo invoca cerca
  de ti.

Invocar monstruo aleatorio ``s``
  Solicita una cantidad y luego invoca ese número de monstruos aleatorios
  cerca de ti.

Eliminar monstruos ``z``
  Solicita una distancia, hasta el rango máximo de visión, y elimina todos
  los monstruos dentro de esa distancia.

Golpear a todos en línea de visión ``H``
  Golpea a todos los monstruos en línea de visión con una gran cantidad de
  daño: 10000.

.. _DebugDungeon:

Mazmorra
========

Crear una trampa ``T``
  Solicita el tipo de trampa a crear y la coloca en tu casilla actual.

Ejecutar un efecto ``E``
  Solicita un tipo de efecto y sus parámetros, y luego ejecuta ese efecto.

Salir sin guardar ``X``
  Sale del juego sin guardar (solicita confirmación primero).

Consultar la mazmorra ``q``
  Ilumina todas las casillas que tengan un flag de casilla determinado
  (ver src/list-square-flags.h).

Consultar terreno ``F``
  Ilumina todas las casillas con un tipo de terreno determinado
  (ver lib/gamedata/terrain.txt).

Recopilar estadísticas ``f`` o ``S``
  Recopila estadísticas sobre monstruos y objetos presentes al generar un
  nivel. Solicita el número de ejecuciones y si se deben explorar niveles en
  profundidad o limpiarlos, y escribe los resultados en el archivo
  'stats.log' en el directorio del usuario. Los comentarios en ese archivo
  ayudarán a interpretar los resultados; para información más detallada, es
  recomendable revisar la implementación de stats_collect() en wiz-stats.c.

Recopilar estadísticas de desconexión ``D``
  Genera varios niveles para recopilar estadísticas sobre con qué frecuencia
  todas las escaleras descendentes son inaccesibles para el jugador, con qué
  frecuencia la ubicación inicial del jugador no es válida, y con qué
  frecuencia un nivel tiene áreas sin bóveda inaccesibles para el jugador.
  Los resultados se escriben en la ventana de mensajes, y los mapas de los
  niveles desconectados o con ubicaciones iniciales inválidas se escriben en
  'disconnect.html' en el directorio del usuario. También recopila estadísticas
  generales sobre la distribución de todos los niveles generados y las escribe
  en 'disconnect_gstat.txt' en el directorio del usuario. Para más detalles
  sobre qué se considera desconectado y qué más se resume sobre la generación
  de niveles, consulta la implementación de disconnect_stats() en wiz-stats.c.

Recopilar estadísticas de pozos ``P``
  Genera varios pozos del tipo de sala que especifiques (foso, nido u otro)
  y calcula un histograma de los tipos de monstruos involucrados. Un resumen
  de los resultados se escribe en la ventana de mensajes. Los resultados por
  nivel y el resumen también se escriben en un archivo.

Hack Nick ``_``
  Mapea las casillas accesibles (por el algoritmo de sonido y olfato) en
  distancias sucesivas desde la casilla del jugador.

Empujar objetos ``>``
  Empuja los objetos fuera de la casilla objetivo como forma de probar
  push_object().

Escribir un mapa del nivel actual ``M``
  Escribe un mapa del nivel actual como archivo HTML.

Miscelánea
==========

Demo de animaciones ``G``
  Muestra los gráficos o caracteres usados para animar los efectos de
  proyección.

Registro de teclas ``L``
  Muestra las pulsaciones de teclas recientes.
