=================
Modificando Angband
=================

Angband no solo es un gran juego por derecho propio, sino que también es muy fácil de modificar.
Gran parte de los detalles del juego están contenidos en archivos de datos de texto. Estos pueden
cambiarse usando nada más que un editor de texto para un cambio inmediato en la forma en que
funciona el juego.

Estos archivos de datos están en lib/gamedata.

Cada archivo tiene un encabezado que describe las líneas que componen las entradas del
archivo, y en su mayor parte esto dejará claro lo que se debe hacer para
realizar cambios en los archivos. A continuación se muestra una breve descripción de cada uno de los archivos.

Aquellos que quieran cambiar el juego más de lo que permite simplemente variar los
archivos de datos necesitarán el código fuente. Debajo de las descripciones de los archivos de datos hay una
breve discusión sobre por dónde empezar en tal empresa.


Los archivos de datos
======================

constants.txt
  Este archivo contiene valores del juego como la capacidad de carga de objetos, el rango visual
  y las dimensiones del nivel de mazmorra y la ciudad.

object_base.txt
  Este archivo contiene los nombres y propiedades comunes de las clases básicas de
  objetos: pergamino, espada, anillo, etc. Todos los objetos tienen una base de objeto.
  A cada base de objeto se le asigna un 'tval', un índice numérico. Los tvals están
  definidos en src/list-tvals.h. Aunque es posible añadir nuevas bases de objeto,
  es poco probable que hagan mucho sin cambios más profundos en el juego.

object.txt
  Este archivo contiene los nombres, propiedades y descripción de todos los tipos
  de objetos que aparecen en Angband. Se pueden añadir fácilmente nuevos tipos de objetos
  a este archivo, o editar los existentes. Cada objeto definido por este archivo tiene una
  base de objeto, y también se le asigna otro índice numérico llamado 'sval'.
  Un par tval-sval identifica completamente un objeto; dado que el tval y el sval
  se guardan en los archivos de guardado, eliminar o añadir objetos probablemente hará
  que los archivos de guardado existentes sean inutilizables.

ego_item.txt
  Este archivo contiene los nombres, propiedades y descripción de los objetos de ego, que
  son armas y armaduras mejoradas mágicamente. Se pueden añadir o eliminar nuevos objetos de ego
  a voluntad, aunque eliminar o cambiar uno con una instancia actualmente
  en el juego podría causar problemas.

artifact.txt
  Este archivo contiene los nombres, propiedades y descripción de los artefactos, que
  son objetos únicos; solo se generará uno de cada. Si estás
  considerando cambios importantes, los nuevos artefactos son uno de los signos más visibles de
  un cambio de tema. Independientemente, los nuevos artefactos son fáciles y divertidos de diseñar.

names.txt
  Este archivo contiene listas de palabras que se usan para generar nombres para
  nombres de personajes aleatorios, artefactos aleatorios y pergaminos. De nuevo, en el caso
  de un cambio de tema, esta es una buena manera de mostrar el nuevo tema.

activation.txt
  Las activaciones se usan para artefactos y algunos objetos normales, y podrían
  usarse para objetos de ego (aunque actualmente ninguno lo hace). Algunos artefactos estándar
  de artifact.txt tienen activaciones, y los artefactos aleatorios pueden tener cualquier
  activación de este archivo elegida para ellos. Las activaciones pueden estar compuestas de
  cualquier efecto (ver list-effects.h y effect-handler-*algo*.c).

flavor.txt
  A objetos como pociones y varitas se les asigna un sabor por tipo de objeto,
  diferente en cada partida. Debe haber al menos tantos sabores para cada
  base de objeto con sabor como objetos con esa base.

monster_base.txt
  Las bases de monstruos son el equivalente a las bases de objeto para monstruos: clases de monstruo
  como orco, troll o vampiro. Este archivo contiene las propiedades comunes a
  todos los monstruos en cada una de estas clases.

monster.txt
  Esto contiene los detalles de todas las razas de monstruos, cada una de las cuales tendrá sus
  propiedades de base de monstruo más otras adicionales. Algunos monstruos son únicos, y
  una vez muertos nunca reaparecerán.

monster_spell.txt
  Todos los hechizos que pueden ser lanzados por monstruos (y a los que se hace referencia en las
  líneas 'spells:' en monster.txt) se definen en este archivo. Al igual que con
  las activaciones, los hechizos de monstruo se construyen a partir de efectos.

pain.txt
  Este archivo contiene los diversos mensajes que se dan para describir cómo un
  monstruo responde a un ataque.

pit.txt
  Los niveles de mazmorra pueden contener fosos: salas llenas de una selección particular de
  monstruos. Este archivo define estas selecciones. También se pueden usar, por
  ejemplo, para generar niveles de mazmorra parciales o completos con monstruos temáticos.

class.txt
  Este archivo define completamente cómo funcionan las clases de jugador, incluyendo todos los detalles
  de los hechizos lanzables. Hay algunas propiedades específicas de clase codificadas,
  a las que se hace referencia a través de las líneas 'flags:', y aparecen en
  list-player-flags.h.

p_race.txt
  Este archivo define todas las características de las razas de jugador. El código específico de raza se
  maneja como para las clases.

body.txt
  Cada raza de jugador tiene un cuerpo, que define qué equipo pueden usar.
  Actualmente solo hay un cuerpo, que todas las razas usan, pero esto se puede cambiar
  fácilmente para obtener un efecto significativo.

history.txt
  Este archivo es para crear los antecedentes del jugador que se encuentran en la pantalla
  de personaje. Si se introduce una nueva raza, será necesario añadir una selección de información
  de antecedentes para ella.

hints.txt
  Esto es simplemente una lista de consejos generales que los tenderos darán
  a sus clientes.

quest.txt
  Este archivo define los monstruos de misión (Sauron y Morgoth) y dónde
  aparecen. Actualmente esto no se puede cambiar fácilmente, ya que todavía hay
  aspectos de las misiones codificados.

shape.txt
  Define formas alternativas que el jugador puede asumir a través de hechizos u objetos
  mágicos. Tal hechizo u objeto mágico incluiría
  "SHAPECHANGE:*nombre de la forma a asumir*" en su lista de efectos.

terrain.txt
  Este archivo define el tipo de terreno que puede aparecer en Angband y sus
  propiedades. El terreno actual se puede cambiar (con efectos posiblemente grandes),
  pero eliminarlo sin cambios en el código probablemente romperá el juego. Añadir
  nuevo terreno no tendrá efecto por sí mismo, porque no hay ningún mecanismo
  para que aparezca.

trap.txt
  Esto define todas las trampas de suelo, cerraduras de puertas, telarañas, señuelos de jugador y
  glifos de protección. Las trampas que pueden aparecer en cofres se definen en otro lugar,
  chest_trap.txt. Los efectos reales de las trampas aparecen en list-effects.h y
  effect-handler-*algo*.c.

chest_trap.txt
  Esto define las trampas y cerraduras que pueden aparecer en los cofres. Las trampas de suelo
  se definen en trap.txt. Los efectos reales de las trampas aparecen en list-effects.h
  y effect-handler-*algo*.c.

room_template.txt
  Esta es una lista de plantillas para salas con formas interesantes que aparecen en la
  mazmorra. Estas se pueden cambiar fácilmente y se pueden añadir nuevas.

vault.txt
  Similar a room_template.txt, esto maneja las bóvedas, que son salas muy peligrosas
  y lucrativas.

visuals.txt
  Configura las secuencias de colores utilizadas por los monstruos con la bandera ATTR_FLICKER.

dungeon_profile.txt
  Este archivo contiene detalles bastante técnicos sobre los diferentes tipos de
  nivel de mazmorra que se pueden generar. Las rutinas de generación reales están en
  gen-cave.c; la información aquí consiste en parámetros para generar
  niveles individuales, y para la frecuencia con la que aparecen los tipos de nivel dados.

world.txt
  Esto define cómo se enlazan los niveles de la mazmorra. Es muy un
  esqueleto. Si lo que quieres es muy parecido a Angband con una sola mazmorra y un
  número fijo de niveles enlazados secuencialmente, entonces todo lo que cambiarías aquí
  son los nombres y asegurarte de que haya configuración para cada nivel hasta
  uno menos de lo que se establece en world:max-depth en constants.txt. Cualquier
  otra cosa probablemente requiere cambios en struct level en game-world.h,
  en la generación de niveles, y en cómo el jugador interactúa con el terreno (escaleras
  en Angband) que enlaza los niveles. Dependiendo de qué aspectos de ese diseño de mundo
  quieras que sean configurables, el contenido de world.txt y cómo se analiza
  en init.c probablemente no se parecerá en nada a lo que hay en Angband.

store.txt
  Esto detalla a los dueños de las tiendas y su relativa generosidad.

blow_effects.txt
  Esto define los efectos sobre el jugador causados por los ataques de monstruos. Los ataques
  de monstruo más simples solo infligen daño, pero otros pueden afectar el estado,
  las estadísticas o el inventario del jugador.

blow_methods.txt
  Esto detalla las diferentes formas en que los monstruos pueden atacar (golpear, arañar, etc.). Afecta
  a los mensajes que recibe el jugador, y también si el golpe puede aturdir
  o cortar al jugador.

brand.txt
  Esto detalla cómo funcionan las marcas elementales de las armas.

slay.txt
  Esto detalla cómo las armas pueden ser más efectivas contra ciertos monstruos.

curse.txt
  Este archivo contiene todas las diferentes maldiciones que se pueden aplicar a los objetos.
  Incluye a qué tipo de objeto se pueden aplicar, los efectos aleatorios que
  pueden causar, y cómo cambian las propiedades de un objeto.

object_property.txt
  Este archivo proporciona detalles sobre qué propiedades puede tener un objeto (aparte de
  combate básico y clase de armadura). Cada propiedad tiene un código que se utiliza
  en el juego para referirse a esa propiedad de alguna manera. Esto significa que no es
  posible añadir nuevas propiedades a este archivo y esperar que tengan algún efecto,
  pero es posible cambiar cómo funcionan las propiedades existentes.

player_property.txt
  Configura las propiedades que el jugador puede obtener de la raza, clase o
  forma del jugador. Algunas de ellas no pueden provenir de otras fuentes y están vinculadas a
  entradas en list-player-flags.h. Otras se superponen con lo que el jugador puede
  obtener del equipo y están vinculadas a entradas en list-object-flags.h o
  list-elements.h. Establece los nombres y descripciones utilizados por las
  pantallas de creación y el comando Ver habilidades, ``S``. Configura enlaces a la columna
  en el panel de resistencias de la pantalla de personaje y a la línea en la
  comparación de equipo que resume el estado del jugador.

player_timed.txt
  Este archivo define algunas de las propiedades de los efectos temporizados (como la velocidad y la
  confusión) que pueden aplicarse al jugador. Contiene principalmente los mensajes
  sobre cambios en estos efectos, vincula un efecto temporizado a una resistencia o
  bandera de objeto, y especifica los atributos del jugador que previenen los efectos.
  Para añadir nuevos efectos temporizados o cambiar la forma en que funcionan los existentes más allá
  de lo que se puede especificar en player_timed.txt, tendrás que alterar
  src/list-player-timed.h y probablemente otros archivos, y recompilar el juego.

projection.txt
  Este archivo contiene gran parte de la información definitoria sobre las proyecciones:
  efectos que pueden ser producidos a distancia por el jugador o los monstruos, y que
  afectan al jugador, monstruos, objetos y/o terreno. En particular, este
  archivo define detalles de los efectos de los ataques elementales (como fuego o
  fragmentos) y la efectividad de la resistencia correspondiente del jugador. Las nuevas
  proyecciones deben ser incluidas en src/list-elements.h (para ataques elementales)
  o incluidas en src/list-projections.h (para todas las demás proyecciones),
  y el código para implementar sus efectos debe ser puesto en otros archivos fuente:
  src/project-obj.c para efectos en objetos, y otros archivos con nombres similares.

realm.txt
  Esto contiene una pequeña cantidad de información sobre los cuatro reinos mágicos
  actuales.

summon.txt
  Esto contiene definiciones para los tipos de monstruos que pueden ser invocados.
  Añadir un nuevo tipo de invocación aún no es posible, porque los hechizos de invocación están
  codificados en src/list-mon-spells.h.

ui_entry.txt
  Define entradas que se mostrarán en la segunda parte de la hoja
  de personaje y en la comparación de equipo del menú de conocimiento. Puedes modificar
  propiedades en object_property.txt y project_property.txt para vincularlas a
  esas entradas. La intención es hacer posible añadir o eliminar una propiedad
  sin tener que actualizar ui-player.c o ui-equipcmp.c además de los
  cambios necesarios para que esa propiedad afecte al juego principal.

ui_entry_base.txt
  Proporciona plantillas para usar con ui_entry.txt.

ui_entry_renderer.txt
  Define técnicas, referenciadas en ui_entry.txt, para representar una propiedad en
  la hoja de personaje o en la comparación de equipo. Si bien es posible añadir
  algo que simplemente use diferentes paletas de símbolos o colores que
  uno de los renderizadores actuales, las técnicas de renderizado básicas están codificadas
  en list-ui-entry-renderers.h.

ui_knowledge.txt
  Maneja algo de configuración de los menús de conocimiento, concretamente el diseño de
  las categorías de monstruos.

Haciendo Conjuntos de Gráficos (Tilesets)
==========================================

Puedes crear nuevos conjuntos de gráficos para Angband o personalizar los existentes. En
esta sección profundizaremos en cómo se definen los tilesets y describiremos cómo configurar
uno desde cero. Primero, enumeraremos los pasos requeridos y luego
desglosaremos cada paso en detalle.

1. Crear un directorio para contener los archivos del tileset: (ej. ``lib/tiles/mitileset``)
2. Registrar el tileset en ``lib/tiles/list.txt``
3. Crear una imagen de mapa de bits vacía lo suficientemente grande para contener tu tileset
4. Guardar la imagen de mapa de bits vacía en tu carpeta de tileset
5. Crear uno o más archivos ``.prf`` para informar a Angband cómo usar tu tileset
6. Crear un Makefile en tu carpeta de tileset

Primero necesitas crear un directorio para contener los archivos de tu tileset. Pon el
directorio en lib/tiles y elige un nombre para el directorio que esté en minúsculas
y que generalmente coincida con la convención de nomenclatura de los otros tilesets que veas
allí. Una vez que se haya creado el directorio, el siguiente paso es decidir qué tan grandes
serán los mosaicos en píxeles y luego crear una imagen PNG en blanco lo suficientemente grande para
contener todos los mosaicos (asegúrate de habilitar la transparencia alfa). Como ejemplo,
el tileset de Shockbolt usa mosaicos de 64x64 píxeles. También usa la bandera especial
de combinación alfa para poder usar mosaicos de doble altura (64x128) para monstruos grandes o altos.
Sus dimensiones son 8192x2048, pero el tileset no está completamente
lleno. Se pueden añadir más mosaicos sin aumentar el tamaño de la imagen a medida que se añadan
nuevos objetos en futuras versiones de Angband. Esto debe tenerse en cuenta, ya que
empaquetar tu tileset en el tamaño de imagen más pequeño posible puede no ser la solución
más fácil de mantener. Asegúrate de nombrar el archivo de imagen según el tamaño del mosaico,
por ejemplo 64x64.png. Usa el tamaño base incluso si estás habilitando mosaicos de doble altura.

El único archivo que necesitarás editar fuera del directorio de tu tileset es
lib/tiles/list.txt. list.txt contiene un registro de qué tilesets cargar, así
como algo de información sobre el tamaño de los mosaicos y cualquier bandera especial a
establecer. El formato del archivo está documentado en el encabezado de list.txt. Específicamente,
definirás el nombre del tileset, qué directorio contiene los
archivos del tileset, qué tan grandes son los mosaicos en píxeles (ej. 64x64), el nombre del
archivo de preferencias principal para el tileset y algunas banderas adicionales que tienen que ver
con la combinación alfa. No todos los tilesets necesitan establecer banderas adicionales.

Ahora que la configuración básica está completa, necesitas decirle a Angband cómo interpretar
la imagen de tu tileset. Necesitas mapear cada mosaico en tu imagen a un elemento
específico en el juego para que Angband sepa qué mosaicos mostrar para qué
caracteres ASCII. Este proceso se puede hacer de forma incremental porque Angband
seguirá mostrando los símbolos de caracteres predeterminados en el juego para los objetos que
aún no se han mapeado. Esto es especialmente útil para verificar que tu tileset
se haya configurado correctamente antes de comenzar a mapear las cosas en serio. También
significa que si se añaden nuevos objetos al juego que no has mapeado en
tu tileset, el juego seguirá siendo jugable con tu tileset, aunque el
carácter ASCII mostrado pueda parecer incongruente con tu estilo. El mapeo de
mosaicos a elementos del juego se realiza en archivos de texto llamados archivos de preferencia que tienen
la extensión '.prf'.

Lo primero que hay que entender sobre el mapeo de elementos del juego en archivos de preferencia
es que todo lo que se puede mostrar en el juego tiene un nombre, o en el caso
de los sabores, un número de ID. Los nombres para cada tipo de cosa se pueden consultar
desde los archivos de datos mencionados anteriormente. La siguiente tabla es una referencia rápida
sobre dónde encontrar nombres de cosas y cómo formar ID correctamente para referenciarlas.

============= ================== ====================
Tipo          Archivo de Datos   Ejemplo
============= ================== ====================
Terreno       terrain.txt        ``feat:FLOOR``
Trampa        trap.txt           ``trap:pit``
Objeto        object.txt         ``object:light``
Monstruo      monster.txt        ``monster:Kobold``
Efecto Hechizo monster_spell.txt  ``GF:METEOR``
Jugador       <ver abajo>        ``monster:<player>``
============= ================== ====================

Las imágenes del jugador se referencian de manera diferente a otros tipos de objetos. Usan
una sintaxis de consulta especial que verifica qué clase es el jugador, así
como el género, para determinar qué imagen mostrar. La consulta para
seleccionar qué mosaico mostrar para una montaraz elfa hembra sería::

  ?:[AND [EQU $CLASS Ranger] [EQU $RACE Elf]  [EQU $GENDER Female] ]

Aquí, la consulta está verificando si el jugador es una Semielfa hembra y
usaría la asignación en la siguiente línea del archivo de preferencia solo si esto es
verdadero.

Algunos tipos de objetos, como el terreno, pueden usar diferentes mosaicos según su
estado. En el caso del terreno, puede tener diferentes imágenes para cuando
está iluminado por una antorcha o está oscuro. Estos se seleccionan añadiendo otro
signo de dos puntos y un especificador al nombre. Por ejemplo, este sería el nombre de una
escalera ascendente iluminada por una antorcha::

  feat:LESS:torch

Es posible especificar que se use el mismo mosaico para todos los estados posibles de una
característica del terreno usando un asterisco. Este ejemplo identifica cualquier casilla de
terreno desconocida (una casilla que el jugador no ha iluminado o visto de otra manera)::

  feat:NONE:*

Dado el nombre completo de un objeto, lo último que hay que hacer es especificar qué mosaico
del tileset usar. Las ubicaciones de los mosaicos se dan en un sistema de coordenadas usando
pares de números hexadecimales. Las coordenadas comienzan desde 0x80:0x80 y se
incrementan a partir de ahí. Los pares se traducen directamente al píxel superior e izquierdo
del mosaico correspondiente del archivo gráfico, por lo que el píxel superior izquierdo
del primer mosaico en la parte superior izquierda del archivo gráfico se especificaría como
0x80:0x80 (el píxel en x:0 y:0). El siguiente mosaico inmediatamente a la derecha de
ese sería 0x80:0x81. La hoja de mosaicos se divide en filas y columnas
basadas en el tamaño de mosaico que especificaste en list.txt. Así que, dado un tamaño de mosaico de 64x64
píxeles, el mosaico en 0x80:0x81 se ubicaría en el archivo gráfico en el píxel
x:64 y:0. Recuerda, las coordenadas en los archivos de preferencia están en hexadecimal,
por lo que el siguiente número después de 0x89 sería 0x8A. El siguiente número después de 0x8F sería
0x90 y así sucesivamente. Para mapear un objeto a tu tileset, añadirás una línea completa
al archivo por objeto. Este ejemplo mapea el mosaico en 0x81:0x81 a la
característica del terreno 'vena de cuarzo' cuando la vena de cuarzo está iluminada por la luz de una antorcha::

  feat:QUARTZ:torch:0x81:0x81

Antes de continuar, es recomendable mapear un solo objeto en tu
archivo de preferencia, luego iniciar el juego, seleccionar tu tileset y asegurarte de que ves
tu mosaico mapeado en el juego. Si esto funcionó, entonces estás listo para diseñar y mapear
el resto de tus mosaicos. Un ejemplo rápido sería mapear un mosaico para tu casa
en la ciudad a la primera posición de mosaico en tu archivo gráfico::

  feat:HOME:*:0x80:0x80

Es posible tener más de un archivo de preferencia usando una especie de sintaxis de
inclusión que hace que también se lean otros archivos de preferencia referenciados desde tu archivo
de preferencia principal. También es posible poner comentarios en tus archivos de
preferencia para ayudarte a llevar un registro de dónde están mapeados diferentes tipos de
objetos. Cualquier texto en una línea después de un símbolo ``#`` se ignora. Los mosaicos
de Shockbolt hacen un gran uso de esto y definen un conjunto de mapeos bien organizado usando
tres archivos con comentarios para cada sección lógica de objetos a mapear::

  # Esto es un comentario
  %:other-stuff.prf  # Carga otro archivo de preferencia

El último paso a seguir es asegurarse de que tu tileset se empaquetará con
Angband cuando se compile para su distribución y que se pueda instalar
junto con los otros tilesets. Para hacer esto, necesitarás añadir un archivo llamado
'Makefile' a tu directorio de tileset. Copia y pega un Makefile existente de
uno de los otros directorios de tilesets y actualiza las líneas DATA y PACKAGE para
que coincidan con los nombres de archivo que elegiste para tu tileset.

Una vez que tengas un tileset funcional y una comprensión práctica de cómo se gestionan
y organizan los tilesets, sería una buena idea estudiar el tileset de Shockbolt
y seguir los ejemplos allí para producir un tileset de alta calidad
del que estés orgulloso de compartir con otros.

Cambios más grandes
====================

Si cambiar los archivos de datos no es suficiente para ti, necesitarás cambiar el código
real del juego y recompilarlo. El primer lugar para buscar es en los archivos
de datos compilados, algunos de los cuales ya se han mencionado:

=====================  =========================  =========================
list-dun-profiles.h    list-mon-temp-flags.h      list-rooms.h
list-effects.h         list-mon-timed.h           list-room-flags.h
list-elements.h        list-object-flags.h        list-square-flags.h
list-equip-slots.h     list-object-modifiers.h    list-stats.h
list-history-types.h   list-options.h             list-terrain.h
list-ignore-types.h    list-origins.h             list-terrain-flags.h
list-kind-flags.h      list-parser-errors.h       list-trap-flags.h
list-message.h         list-player-flags.h        list-tvals.h
list-mon-message.h     list-player-timed.h        list-ui-entry-renderers.h
list-mon-race-flags.h  list-projections.h
list-mon-spells.h      list-randart-properties.h
=====================  =========================  =========================

Más allá de esto, tendrás que tener algo de conocimiento del lenguaje de programación
C, y puedes comenzar a hacer cambios en la forma en que el juego funciona o aparece.
Muchas personas han hecho esto: hay más de 100 variantes de Angband:
https://nickmcconnell.github.io/AngbandPlus/
Si llegas a este punto, lo mejor que puedes hacer es discutir tus ideas en
los foros de Angband en https://live/angband.live/forums/. Las personas allí
suelen estar ansiosas por escuchar nuevas ideas y formas de jugar.