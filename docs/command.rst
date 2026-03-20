.. _command-descriptions:

====================
Descripciones de Comandos
====================

Las siguientes descripciones de comandos se enumeran como el nombre del comando más la
tecla predeterminada para usarlo. Para aquellos que prefieren el conjunto de teclas "roguelike"
original, el nombre y la tecla del comando roguelike también se muestran si es
diferente. Luego viene una breve descripción del comando, que incluye
información sobre métodos alternativos para especificar el comando en cada
conjunto de teclas, cuando sea necesario.

Algunos comandos usan el "número de repetición" para repetir automáticamente el comando
varias veces, mientras que otros usan el "número de repetición" para especificar una "cantidad"
para el comando, y otros aún lo usan como un "argumento" de algún tipo.

La mayoría de los comandos no consumen "energía" para realizarse, mientras que otros comandos solo consumen
energía cuando hacen que el mundo cambie de alguna manera. Por ejemplo,
intentar leer un pergamino mientras se está ciego no usa energía.

El siguiente comando es muy útil para principiantes,

Listas de comandos ('Enter')
  Esto muestra una pequeña ventana en el medio de la pantalla, en la que
  puedes seleccionar qué comando te gustaría usar navegando. Si deseas
  comenzar a jugar inmediatamente, puedes usar esta opción para navegar por los
  comandos y consultar esta guía cuando necesites más detalles sobre
  comandos específicos.

Comandos de Inventario
==================

Lista de inventario (``i``)
  Muestra una lista de objetos que se llevan pero no están equipados. Puedes llevar
  hasta 23 objetos diferentes, sin contar los de tu equipo. A menudo,
  muchos objetos idénticos pueden "apilarse" en un "montón" que contará como
  un solo objeto. Cada objeto tiene un peso, y si llevas más
  objetos de los que tu fuerza permite, comenzarás a ralentizarte. La
  cantidad de peso que aún puedes llevar sin estar sobrecargado, o la
  cantidad de peso extra que llevas actualmente se muestra en la parte superior
  de la pantalla.

Lista de equipo (``e``)
  Usa este comando para mostrar una lista de los objetos que tu personaje está usando
  actualmente. El cuerpo estándar (que todas las razas tienen actualmente) tiene
  12 ranuras para equipo. Cada ranura de equipo corresponde a una ubicación diferente
  en el cuerpo, y cada una puede contener solo un objeto a la vez, y cada una
  solo puede contener objetos del "tipo" adecuado.
  Para el cuerpo estándar, estas son ARMA (arma), ARCO (lanzador de proyectiles),
  ANILLO (anillo) (dos de estos), AMULETO (amuleto), LUZ (fuente de luz),
  ARMADURA CORPORAL (armadura), CAPA (capa), ESCUDO (escudo), SOMBRERO (casco),
  GUANTES (guantes), BOTAS (botas). Debes estar empuñando/llevando ciertos
  objetos para aprovechar sus poderes especiales.

Lista de aljaba (``|``)
  Muestra el contenido de tu aljaba. Los proyectiles que llevas se
  colocarán automáticamente en tu aljaba si hay espacio. La aljaba tiene
  10 ranuras; también ocupa espacio de inventario: cada 40 proyectiles
  reducirán tu número de ranuras de inventario en 1. Los objetos que son buenos
  para lanzar y han sido :ref:`inscritos para lanzar <inscribing>` (por
  ejemplo, la inscripción incluye ``@v0``) también se colocarán en la
  aljaba si hay espacio. Cada pila de tales objetos ocupa una ranura de aljaba
  y cada 8 de esos objetos reducirán tus ranuras de inventario en 1.

Soltar un objeto (``d``)
  Esto suelta un objeto de tu inventario o equipo al suelo de la mazmorra.
  El objeto permanecerá a tus pies si es posible. Algunos terrenos,
  como puertas y escaleras, y cualquier casilla con una trampa no pueden contener
  objetos, y, en ese caso, el juego intentará colocar el objeto en
  una casilla cercana que pueda contener objetos. Este comando puede tomar una cantidad,
  y consume algo de energía.

Ignorar un objeto (``k``) o Ignorar un objeto ('^d')
  Esto ignora un objeto en tu inventario o en el suelo de la mazmorra. Si el
  montón seleccionado contiene múltiples objetos, puedes especificar una cantidad. Cuando
  se ignora, el juego a veces te preguntará si quieres ignorar solo este
  objeto o todos los demás como él. Si se elige la segunda opción, todos los objetos
  similares en el suelo y en tu inventario serán ignorados. Para ver todos los
  objetos independientemente de si están ignorados, puedes usar ``K`` (``O``
  en el conjunto de teclas roguelike) para alternar la configuración de ignorar.

Usar/Poner equipo (``w``)
  Para usar o poner un objeto de tu inventario, usa este comando. Dado que
  solo un objeto puede estar en cada ranura a la vez, si usas o pones un
  objeto en una ranura que ya está ocupada, el objeto antiguo se quitará primero,
  y de hecho puede ser soltado si no hay espacio para él en tu
  inventario. Este comando consume algo de energía.

Quitar equipo (``t``) o Quitar equipo (``T``)
  Usa este comando para quitar una pieza de equipo y devolverla a tu
  inventario. Ocasionalmente, te encontrarás con un objeto maldito que no se puede
  quitar. Estos objetos normalmente te penalizan de alguna manera y no se pueden
  quitar hasta que se elimine la maldición. Si no hay espacio en tu
  inventario para el objeto, tu mochila se desbordará y soltarás el
  objeto después de quitártelo. También puedes quitar munición de tu aljaba
  con este comando. Este comando consume algo de energía.

Comandos de Movimiento
=================

Moverse (teclas de flecha, teclas numéricas) o (teclas de flecha, teclas numéricas, 'yuhjklbn')
  Esto te hace mover un paso en una dirección dada. Si la casilla a la que
  deseas moverte está ocupada por un monstruo, lo atacarás. Si la
  casilla está ocupada por una puerta, intentarás abrirla. Si la casilla
  está ocupada por una trampa, intentarás desarmarla a menos que seas inmune a las trampas:
  en ese caso, simplemente te mueves allí sin sufrir daño. Anteponer
  este comando con CTRL hará que alteres (ataques, excaves, abras,
  desarmes o cierres) en la dirección apropiada, pero no moverá a tu
  personaje si no hay nada allí para alterar. Estos comandos consumen algo de
  energía.

Caminar (``W``) o Caminar (``-``)
  El comando caminar te permite entrar voluntariamente en una trampa sin intentar
  desarmarla. Este comando puede tomar un número, requiere una dirección y consume
  algo de energía.

Correr (``.``) o Correr (``,``)
  Este comando te moverá en la dirección dada, siguiendo cualquier curva en el
  corredor, hasta que tengas que tomar una "decisión" entre dos direcciones
  o te molesten. Para más información sobre lo que puede molestarte, consulta
  :ref:`Molestar <disturb-player>`. También puedes usar mayúsculas más las teclas de dirección "roguelike"
  (conjunto de teclas roguelike), o mayúsculas más las teclas de dirección "originales"
  en el teclado numérico (ambos conjuntos de teclas, algunas máquinas) para correr en una dirección.
  Este comando puede tomar un argumento, requiere una dirección y consume algo de
  energía.

Subir escaleras (``<``)
  Sube por una escalera ascendente en la que te encuentras. Siempre hay al menos
  una escalera que sube en cada nivel excepto en el nivel de la ciudad (esto
  no significa que sea fácil de encontrar). Subir una escalera te llevará a un
  nuevo nivel de mazmorra a menos que estés a 50 pies (nivel de mazmorra 1), en cuyo
  caso regresarás al nivel de la ciudad. Ten en cuenta que cada vez que abandonas
  un nivel (que no sea la ciudad), nunca lo volverás a encontrar. Esto significa que
  para todos los efectos prácticos, cualquier objeto en ese nivel es destruido. Esto
  incluye artefactos a menos que la opción "Perder artefactos al abandonar el nivel"
  estuviera desactivada cuando se creó tu personaje, en cuyo caso los artefactos
  pueden aparecer de nuevo más tarde. La opción de perder artefactos está desactivada en la
  configuración predeterminada. Este comando consume algo de energía. Si la
  :ref:`Opción de Comandos de Autoexploración <autoexplore-commands-option>` está activada,
  no estás en una escalera ascendente, no estás confundido y no hay monstruos a la vista,
  ``<`` determinará una ruta a la escalera ascendente conocida más cercana (por número de turnos),
  y, si encuentra una ruta, comenzará a seguirla.
  :ref:`Búsqueda de camino <pathfinding-player>` tiene más información sobre cómo se
  calcula la ruta y qué sucede al seguirla.

Bajar escaleras (``>``)
  Desciende por una escalera descendente en la que te encuentras. Siempre hay al menos
  una escalera que baja en cada nivel, excepto en la ciudad, que tiene
  solo una, y los niveles de "misión", que no tienen ninguna hasta que se mata al
  monstruo de la misión. Bajar una escalera te llevará a un nuevo nivel de mazmorra. Consulta
  "Subir Escaleras" para más información. Este comando consume algo de energía. Si la
  :ref:`Opción de Comandos de Autoexploración <autoexplore-commands-option>` está activada,
  no estás en una escalera descendente, no estás confundido y no hay monstruos a la vista,
  ``>`` determinará una ruta a la escalera descendente conocida más cercana (por número de turnos),
  y, si encuentra una ruta, comenzará a seguirla.
  :ref:`Búsqueda de camino <pathfinding-player>` tiene más información sobre cómo se
  calcula la ruta y qué sucede al seguirla.

Ruta a la casilla desconocida más cercana (``p``)
  A menos que la :ref:`Opción de Comandos de Autoexploración <autoexplore-commands-option>`
  esté activada, este comando no hace nada. Cuando esa opción está activada, no estás
  confundido y no hay monstruos a la vista, ``p`` determinará una ruta a
  la casilla transitable conocida más cercana (por turnos) que tenga una casilla vecina desconocida
  y comenzará a seguir esa ruta. Si no existe tal ruta, intentará
  encontrar una ruta a la casilla transitable conocida más cercana que esté junto a una puerta cerrada
  o escombro infranqueable y esa puerta o escombro tenga una casilla vecina desconocida.
  :ref:`Búsqueda de camino <pathfinding-player>` tiene más información sobre cómo se
  calcula la ruta y qué sucede al seguirla.

Comandos de Descanso
================

Quedarse quieto (con recogida) (``,``) o Quedarse quieto (con recogida) (``.``)
  Permanece en la misma casilla durante un movimiento. Si normalmente recoges objetos
  que encuentras, recogerás lo que esté debajo de ti. También puedes
  usar la tecla ``5`` (ambos conjuntos de teclas). Este comando puede tomar un número y
  consume algo de energía.

Recoger objetos (``g``)
  Recoge objetos y oro del suelo debajo de ti. Recoger oro no
  lleva tiempo, y los objetos toman 1/10 de un turno normal cada uno (el costo máximo de tiempo
  es un turno completo). Puedes recoger objetos hasta que el suelo esté vacío o tu
  mochila esté llena.

Descansar (``R``)
  Descansar es mejor para ti que quedarte quieto repetidamente, y se puede indicar
  que se detenga automáticamente después de un cierto tiempo, o cuando se cumplan varias
  condiciones. En cualquier caso, siempre te despiertas cuando algo
  :ref:`molesto sucede <disturb-player>`, o cuando presionas cualquier tecla.
  Para descansar, ingresa el comando Descansar, seguido del número de turnos que deseas
  descansar, o ``*`` para descansar hasta que se recuperen tus puntos de vida y maná, o
  ``&`` para descansar hasta que estés completamente "curado". Este comando puede tomar un
  argumento (usado para el número de turnos para descansar) y consume algo de energía.

Comandos de Alteración
==============

Excavar (``T``) o Excavar ('^t')
  Excavar o minar es un arte muy útil. Hay muchos tipos de roca,
  con diferente dureza, incluyendo roca permanente (permanente), granito
  (muy duro), vetas de cuarzo (duro), vetas de magma (blando) y escombros (muy
  blando). Las vetas de cuarzo y magma pueden mostrarse de una manera especial, y pueden
  contener a veces tesoros, en cuyo caso se mostrarán de una manera diferente.
  Los escombros a veces cubren un objeto pero son fáciles de excavar,
  incluso con las manos desnudas. La habilidad de excavar aumenta con la
  fuerza y el peso del arma. Si tienes una herramienta de excavación en tu mochila,
  el juego la usará automáticamente para excavar. Este comando puede tomar un número,
  requiere una dirección y consume algo de energía.

Abrir una puerta o cofre (``o``)
  Para abrir un objeto como una puerta o cofre, debes usar este comando. Si
  el objeto está cerrado con llave, intentarás abrir la cerradura según tu
  habilidad de desarme. Si abres un cofre con trampas sin desarmar las
  trampas primero, la trampa se activará. Abrir intentará automáticamente
  abrir cualquier cerradura de puerta. Puedes necesitar varios intentos para abrir una puerta o cofre.
  Este comando puede tomar un número, requiere una dirección y consume algo de energía.

Cerrar una puerta (``c``)
  Las criaturas no inteligentes y algunas otras no pueden abrir puertas, por lo que cerrar
  puertas puede ser muy valioso. Además, los monstruos no pueden verte detrás
  de puertas cerradas, por lo que cerrar puertas puede permitirte ganar algo de tiempo sin
  ser atacado. Las puertas rotas no se pueden cerrar. Este comando puede tomar un
  número, requiere una dirección y consume algo de energía.

Desarmar una trampa o cofre, o cerrar con llave una puerta (``D``)
  Puedes intentar desarmar trampas en el suelo o en cofres. Si fallas,
  hay una posibilidad de que te equivoques y la actives. Solo puedes
  desarmar una trampa después de haberla encontrado. El comando también se puede usar para
  cerrar con llave una puerta cerrada, lo que creará un obstáculo para los monstruos. Incluso si
  muchos monstruos pueden abrir la cerradura o derribar la puerta,
  a menudo les llevará algún tiempo. Este comando puede tomar un número, requiere
  una dirección y consume algo de energía.

Alterar (``+``)
  Este comando especial permite el uso de una sola pulsación de tecla para seleccionar cualquiera de
  los comandos "obvios" anteriores (atacar, excavar, derribar, abrir, desarmar),
  y, mediante el uso de mapas de teclas, combinar esta pulsación de tecla con direcciones. En
  general, esto permite el uso de la tecla "control" más la tecla de "dirección"
  adecuada (incluyendo las teclas de dirección roguelike en el modo roguelike)
  como una especie de comando genérico para "alterar la característica del terreno de una casilla
  adyacente". Este comando puede tomar un número, requiere una dirección y
  consume algo de energía.

Robar (``s``)
  Este comando solo está disponible para pícaros y les permite intentar robar
  de un monstruo. Robar funciona mejor cuando el jugador es sigiloso y
  más rápido que el monstruo objetivo, y mejor aún cuando la víctima está dormida.
  Un robo fallido despertará al monstruo; si realmente arruinas el intento, el
  monstruo puede gritar con ira. Este comando requiere una dirección y
  consume algo de energía.

Comandos de Hechizos
=========================

HoJear un libro (``b``) o Examinar un libro (``P``)
  Cada clase tiene libros que puede leer y libros que no; excepto los guerreros,
  que no pueden leer ningún libro. Cuando se usa este comando, todos los hechizos
  contenidos en el libro seleccionado se muestran, junto con información como
  su nivel, la cantidad de maná requerida para lanzarlos, y si sabes o no el hechizo.

Aprender nuevos hechizos (``G``)
  Usa este comando para aprender realmente nuevos hechizos. Cuando puedas aprender
  nuevos hechizos, la palabra "Estudiar" aparecerá en la línea de estado en la parte inferior
  de la pantalla. Si tienes un libro en tu poder, que contenga hechizos
  que puedas aprender, entonces puedes elegir estudiar ese libro. La mayoría de las clases
  pueden elegir qué hechizo estudiar, pero si eres un sacerdote o paladín,
  tus dioses elegirán una oración por ti. Hay cinco libros de cada
  reino, pero las clases híbridas - paladines, pícaros, montaraces y guardias negras - solo
  pueden lanzar desde dos o tres de estos. Los libros de nivel superior normalmente se encuentran
  solo en la mazmorra. Este comando consume algo de energía.

Lanzar un hechizo (``m`` en ambos conjuntos de teclas)
  Para lanzar un hechizo, debes haber aprendido previamente el hechizo y debes tener
  en tu inventario un libro del cual se pueda leer el hechizo. Cada hechizo tiene
  una probabilidad de fallo que comienza siendo bastante grande pero disminuye a medida que
  ganas niveles. Si no tienes suficiente maná para lanzar un hechizo, se te
  pedirá confirmación. Si decides continuar, la probabilidad de
  fallo aumenta considerablemente, y ya sea que el hechizo se lance con éxito o no,
  puedes terminar paralizado durante varios turnos o drenando tu
  constitución. Dado que debes leer el hechizo de un libro, no puedes estar ciego
  o confundido mientras lanzas, y, a menos que seas un nigromante, debe haber
  algo de luz presente. Este comando consume algo de energía.

Comandos de Manipulación de Objetos
============================

Comer algo (``E``)
  Debes comer regularmente para evitar morir de hambre. Hay un medidor de hambre
  en la parte inferior de la pantalla, que dice "Alimentado" y da un porcentaje en
  la mayoría de las circunstancias. Si tienes hambre durante suficiente tiempo, te debilitarás,
  luego comenzarás a desmayarte y, eventualmente, podrías morir de hambre
  (acompañado de mensajes cada vez más alarmantes en tu medidor de hambre).
  También es posible estar "Lleno", lo que te hará moverte lentamente; más
  lentamente cuanto más lleno estés. Puedes usar este comando para comer comida en tu
  inventario. Ten en cuenta que a veces puedes encontrar comida en la mazmorra, pero
  no siempre es sabio comer comida extraña. Este comando consume algo de energía.

Alimentar tu linterna/antorcha (``F``)
  Si estás usando una linterna y tienes frascos de aceite en tu mochila, entonces puedes
  "reabastecerlas" con este comando. Las antorchas y linternas tienen un límite
  en su combustible máximo. En general, dos frascos reabastecerán completamente una linterna.
  Este comando consume algo de energía.

Beber una poción (``q``)
  Usa este comando para beber una poción. Las pociones afectan al jugador de varias
  maneras, pero los efectos no siempre son inmediatamente obvios. Este comando
  consume algo de energía.

Leer un pergamino (``r``)
  Usa este comando para leer un pergamino. Los hechizos de pergamino suelen tener un efecto de área,
  excepto en algunos casos en los que actúan sobre otros objetos. Leer un
  pergamino hace que el pergamino se desintegre a medida que el pergamino hace efecto.
  La mayoría de los pergaminos que solicitan más información se pueden abortar (presionando
  escape), lo que detendrá la lectura del pergamino antes de que se
  desintegre. Este comando consume algo de energía.

Inscribir un objeto (``{``) 
  Este comando inscribe una cadena en un objeto. La inscripción se muestra
  entre llaves después de la descripción del objeto. La inscripción está
  limitada al objeto particular (o pila) y no se transfiere automáticamente
  a todos los objetos similares. Bajo ciertas circunstancias, Angband mostrará inscripciones
  "falsas" en ciertos objetos ('probado', 'vacío') cuando sea apropiado. Estas inscripciones
  "falsas" permanecen todo el tiempo, incluso si el jugador elige agregar una inscripción "real"
  encima de ellas más tarde.

  Además, Angband colocará la inscripción '??' en un objeto por ti
  si el objeto tiene una propiedad (o "runa") que aún no has aprendido.
  Esta inscripción permanecerá hasta que conozcas todas las runas del objeto.

  Un objeto etiquetado como '{vacío}' se encontró sin cargas, y un
  objeto etiquetado como '{probado}' es un objeto "con sabor" que el personaje ha
  usado, pero cuyos efectos son desconocidos. Ciertas inscripciones tienen un significado
  para el juego, consulta '@#', '@x#', '!!', '=g`, '!*', '!x', '^*', y '^x' en la
  :ref:`sección sobre inscripciones <inscribing>`.

Desinscribir un objeto (``}``)
  Este comando elimina la inscripción de un objeto. Este comando no tendrá
  efecto en las inscripciones "falsas" agregadas por el juego mismo.

Alternar ignorar (``K``) o Alternar ignorar (``O``)
  Este comando alternará la configuración de ignorar. Si está activada, todos los objetos ignorados
  quedarán ocultos a la vista. Si está desactivada, se mostrarán todos los objetos independientemente
  de su configuración de ignorar. Consulta la :ref:`sección sobre ignorar objetos <ignoring>`
  para más información.

Comandos de Objetos Mágicos
=======================

Activar un objeto (``A``)
  Has escuchado rumores de armas y armaduras especiales en las profundidades de los Pozos,
  objetos que pueden permitirte respirar fuego como un dragón o iluminar habitaciones con
  solo un pensamiento. Si alguna vez tienes la suerte de encontrar tal objeto,
  este comando te permitirá activar su habilidad especial. Las habilidades especiales
  solo se pueden usar si llevas puesta o empuñas el objeto. Este comando
  consume algo de energía.

Apuntar una varita (``a``) o Usar una varita (``z``)
  Las varitas deben apuntarse en una dirección para ser utilizadas. Las varitas son dispositivos mágicos,
  y por lo tanto hay una posibilidad de que no puedas descubrir cómo usarlas
  si no eres bueno con los dispositivos mágicos. Dispararán un proyectil
  que afecta al primer objeto o criatura encontrada o dispararán un rayo que
  afecta a cualquier cosa en una dirección dada, dependiendo de la varita. Una
  obstrucción como una puerta o pared generalmente detendrá los efectos de viajar
  más lejos. Este comando requiere una dirección y puede usar un
  objetivo. Este comando consume algo de energía.

Usar un bastón (``u``) o Usar un bastón (``Z``)
  Este comando usará un bastón. Un bastón es normalmente muy similar a un
  pergamino, ya que normalmente tienen un efecto de área o afectan a un
  objeto específico. Los bastones son dispositivos mágicos y hay una posibilidad de que
  no puedas descubrir cómo usarlos. Este comando consume algo de energía.

Usar una vara (``z``) o Activar una vara (``a``)
  Las varas son objetos mágicos extremadamente poderosos, que no pueden quemarse ni
  romperse, y que pueden tener efectos similares a bastones o varitas, pero
  a diferencia de los bastones y varitas, no tienen cargas. En cambio, se basan en
  la energía mágica ambiental para recargarse y, por lo tanto, solo pueden
  activarse una vez cada pocos turnos. El tiempo de recarga varía según
  el tipo de vara. Este comando puede requerir una dirección (dependiendo
  del tipo de vara y de si conoces su tipo) y puede usar un
  objetivo. Este comando consume algo de energía.

Armas Arrojadizas y de Proyectiles
============================

Disparar un objeto (``f``) o Disparar un objeto (``t``)
  Este comando te permitirá disparar un proyectil de tu aljaba o
  de tu inventario, siempre que sea la munición adecuada para el arma
  de proyectiles actual que tengas equipada. No puedes disparar un objeto sin un
  arma de proyectiles equipada. La munición disparada tiene una probabilidad de romperse.
  Este comando consume algo de energía.

Disparar munición predeterminada al más cercano (``h``) o ('TAB')
  Si tienes un arma de proyectiles equipada y la munición adecuada en
  tu aljaba, puedes usar este comando para disparar al enemigo visible más cercano.
  Este comando se cancelará si no tienes un lanzador, munición
  o un objetivo visible que esté a alcance. Se usa la primera munición del tipo correcto
  encontrada en la aljaba. Este comando consume algo de energía.

Lanzar un objeto (``v``)
  Puedes lanzar cualquier objeto que lleve tu personaje. Dependiendo del
  peso, puede viajar a través de la habitación o caer a tu lado. Solo un
  objeto de una pila se lanzará a la vez. Ten en cuenta que lanzar un objeto
  a menudo hará que se rompa, ¡así que ten cuidado! Si lanzas algo a una
  criatura, tus posibilidades de golpearla están determinadas por tus pluses para
  golpear, tu habilidad para lanzar y los pluses del objeto para golpear. Algunas
  armas están especialmente diseñadas para ser lanzadas. Una vez que la
  criatura es golpeada, el objeto puede o no causarle daño.
  Ten en cuenta que los frascos de aceite causarán algo de daño de fuego a un monstruo al impactar.
  Lanzar, al igual que disparar, requiere una dirección. El modo de apuntado (consulta el siguiente
  comando) se puede invocar con ``*`` en el mensaje '¿Dirección?'. Este comando
  consume algo de energía.

Modo de Apuntado (``*``)
  Esto te permitirá cambiar o borrar el objetivo actual. Ese objetivo
  se puede recordar cuando otros comandos soliciten un objetivo. Cuando el
  objetivo actual es un monstruo, el estado de ese monstruo se rastrea en la
  barra lateral. Para más detalles sobre la interfaz de apuntado que
  usa este comando, consulta :ref:`Apuntado <targeting>`.

Comandos de Observación
================

Mapa de pantalla completa (``M``)
  Este comando mostrará un mapa de toda la mazmorra, reducido por un factor
  de nueve, en la pantalla. Solo las características principales de la mazmorra serán visibles
  debido a la escala, por lo que incluso algunos objetos importantes pueden no aparecer en
  el mapa. Esto es particularmente útil para localizar dónde están las escaleras
  en relación con tu posición actual, o para identificar áreas inexploradas de
  la mazmorra.

Localizar jugador en el mapa (``L``) o Dónde está el jugador (``W``)
  Este comando te permite desplazarte por tu mapa, mirando todos los sectores de
  nivel actual de la mazmorra, hasta que presiones escape, en cuyo punto el mapa
  se volverá a centrar en el jugador si es necesario. Para desplazarte por el mapa,
  simplemente presiona cualquiera de las teclas de "dirección". La línea superior mostrará la
  ubicación del sector y el desplazamiento desde tu sector actual.

Mirar alrededor (``l``) o Examinar cosas (``x``)
  Este comando se usa para mirar alrededor de los monstruos cercanos (para determinar
  su tipo y salud) y objetos (para determinar su tipo). También se
  usa para saber si un monstruo está actualmente dentro de una pared y qué hay
  debajo del jugador. También podrías usarlo para establecer el objetivo actual, pero,
  cuando quieras apuntar a un monstruo, el comando de apuntado, ``*``, será
  más útil. Para más información sobre la interfaz de apuntado que
  usa este comando, consulta :ref:`Apuntado <targeting>`.

Inspeccionar un objeto (``I``)
  Este comando te permite inspeccionar un objeto. Esto te dirá cosas sobre
  los poderes especiales del objeto, así como información de ataque para
  armas. También te dirá qué resistencias o habilidades has notado
  para el objeto y si aún no has identificado completamente todas las
  propiedades.

Listar monstruos visibles (``[``)
  Este comando enumera todos los monstruos que son visibles para ti, indicando cuántos
  hay de cada tipo. También te dice si están dormidos
  y dónde están (relativo a ti).

Listar objetos visibles (``]``)
  Este comando enumera todos los objetos que son visibles para ti, indicando cuántos
  hay de cada uno y dónde están en el nivel en relación con tu ubicación
  actual.

Comandos de Mensajes
================

Repetir sensación de nivel ('^f')
  Repite la sensación sobre los monstruos en el nivel de mazmorra que obtuviste
  cuando entraste por primera vez al nivel. Si has explorado suficiente del
  nivel, también obtendrás una sensación sobre lo buenos que son los tesoros.

Ver mensajes anteriores ('^p')
  Este comando te muestra todos los mensajes recientes. Puedes desplazarte a través
  de ellos o salir con ESCAPE.

Tomar notas (``:``)
  Este comando te permite tomar notas, que luego aparecerán en tu
  lista de mensajes y en tu historial de personaje (con el prefijo "Nota:").

Comandos de Estado del Juego
====================

Descripción del Personaje (``C``)
  Muestra una descripción completa de tu personaje, que incluye tus niveles
  de habilidad, tus estadísticas actuales y potenciales, y otra información diversa.
  Desde esta pantalla, puedes cambiar tu nombre o usar el comando de descripción de
  archivo de personaje para guardar el estado de tu personaje en un archivo. Ese comando
  guarda información adicional, que incluye tu historial, tu inventario
  y el contenido de tu casa. El comando para cambiar el modo cambia
  lo que se muestra de un lado a otro entre la vista original y una que muestra
  cómo tu equipo actual y las características innatas del jugador afectan
  ciertos atributos. Los símbolos predeterminados utilizados dentro de esa vista son '.' para
  nada equipado o ningún efecto conocido en el atributo, '?' si tu personaje
  no sabe si hay un efecto en el atributo, '+' si tu personaje
  sabe que hay un efecto positivo en el atributo, '-' si tu personaje
  sabe que hay un efecto negativo en el atributo, '!' si un efecto temporizado
  afecta positivamente el atributo, o '=' si un efecto temporizado afecta negativamente
  el atributo. Para las resistencias elementales (el bloque de atributos en el
  extremo izquierdo), también son posibles '*', para indicar una inmunidad, y '~' para indicar que algo
  proporciona tanto un '+' como un '-'. El color de la etiqueta para
  el atributo indicará la suma de las diferentes fuentes para tu
  personaje. Esos colores predeterminados son: pizarra para cuando tu personaje no
  conoce la runa asociada con ese atributo, blanco si no hay ningún
  efecto combinado (excluyendo efectos temporizados) conocido por el personaje, azul
  claro si el efecto combinado conocido (excluyendo efectos temporizados) es positivo, rojo
  si el efecto combinado conocido (excluyendo efectos temporizados) es negativo, y
  verde si el efecto combinado conocido es una inmunidad elemental.

Consultar conocimiento (``~``)
  Este comando te permite preguntar sobre el conocimiento que posee tu
  personaje. La información que puedes consultar es:

  objetos
    Mostrará qué objetos conoce tu personaje. Para cada
    tipo de objeto, te permite cambiar si está ignorado o no,
    la representación de ese tipo en la pantalla o la inscripción
    aplicada automáticamente a todos los objetos de ese tipo. Algunos tipos de
    objetos tu personaje los conocerá desde el principio del juego.
    Otros vienen en "sabores", y tu personaje debe determinar el efecto
    de cada "sabor" una vez para cada tipo de objeto. Para un tipo de objeto
    con un "sabor" conocido, también podrás mostrar un resumen de
    lo que el objeto puede hacer.

  runas
    Mostrará las "runas", propiedades de objetos encantados, que tu
    personaje conoce. Te permite cambiar la inscripción que
    se añade automáticamente a un objeto que tiene la runa. Una vez que tu
    personaje identifica una "runa" en un objeto, reconocerá
    esa propiedad en otros objetos.

  artefactos
    Mostrará todos los artefactos que tu personaje ha encontrado. Normalmente,
    una vez que un artefacto es "generado" y "perdido", nunca se puede volver a encontrar,
    y se volverá "conocido" para el jugador. Con la opción "Perder artefactos al
    abandonar el nivel" desactivada, un artefacto nunca puede ser "perdido" hasta
    que es "conocido" para el jugador. En cualquier caso, cualquier artefacto "conocido" no
    en posesión del jugador nunca volverá a ser "generado".

  objetos de ego
    Mostrará los "egos" que tu personaje ha encontrado. Cada "ego" es
    una colección de encantamientos que pueden aparecer en un objeto. Los "Egos"
    a menudo están restringidos a solo unos pocos tipos específicos de objetos.

  monstruos
    Muestra los tipos de monstruos que tus personajes actuales o anteriores han
    encontrado. Para cada tipo de monstruo, te permite cambiar su
    representación en la pantalla. Algunos monstruos son "únicos" que solo se
    pueden matar una vez por juego. Para un "único" que tus personajes actuales o
    anteriores han encontrado, esto mostrará si ese "único" sigue vivo en este juego.

  características
    Muestra los tipos de casillas de mapa que pueden aparecer en el juego. Para cada
    tipo, te permite cambiar su representación en la pantalla y cómo
    esa representación cambia dependiendo de la cantidad de luz presente.

  trampas
    Muestra los tipos de trampas que pueden aparecer en el juego. Para cada tipo,
    te permite cambiar su representación en la pantalla y cómo esa
    representación cambia dependiendo de la cantidad de luz presente.

  efectos de cambio de forma
    Proporciona una descripción más detallada de las "formas", efectos mágicos
    de algunos hechizos y algunos objetos que cambian la forma del cuerpo de tu
    personaje.

  tiendas y hogar
    Cada una de estas mostrará el contenido de la tienda correspondiente
    o del hogar de tu jugador en el momento en que tu personaje visitó por última vez la
    ciudad. Si tu personaje está actualmente en la ciudad, lo que se muestra aquí
    será el contenido actual.

  salón de la fama
    Muestra una lista de personajes actuales y pasados, ordenados por lo lejos que
    progresaron.

  historial del personaje
    Muestra un resumen de lo que ha hecho tu personaje actual.

  comparación de equipo
    Esto muestra un resumen de las propiedades conocidas de los objetos equipables
    a los que tu personaje tiene acceso, ya sea que estén actualmente equipados,
    en la mochila de tu personaje, en el suelo en la ubicación actual de tu
    personaje o en una tienda. Cerca de la parte superior de la pantalla hay una línea, que comienza
    con "@", que resume el estado de tu personaje dado su equipo actual.
    Cada línea después de esa corresponde a un objeto, ordenado
    por qué ranura de equipo puede ocupar. El primer carácter de cada una de esas
    líneas es la representación de ese objeto como aparecería en el mapa si
    estuviera en el suelo. Después de eso hay un solo carácter, "e" para equipado,
    "p" para mochila, "f" para suelo, "h" para hogar y "s" para tienda, que
    indica dónde está el objeto. El resto de la línea resume las
    propiedades del objeto, con una propiedad por columna. En la vista
    predeterminada, esas propiedades son las resistencias, banderas y modificadores presentes
    en el objeto; aparecen en el mismo orden (de izquierda a derecha) que aparecen
    (de arriba a abajo y luego de izquierda a derecha) en la segunda parte de la descripción
    del personaje. Puedes alternar entre esa vista y una
    que muestra el efecto de cada objeto en las estadísticas clave de tu personaje
    presionando 'v'. Puedes usar 'c' para alternar qué objetos, según
    su ubicación, se incluyen en la pantalla. El valor predeterminado es mostrar
    solo los objetos que están equipados, en la mochila, en el suelo en la ubicación actual
    de tu personaje y en el hogar. Las otras opciones son:
    mostrar solo los objetos en tiendas distintas del hogar, mostrar todos los objetos,
    o mostrar solo aquellos que están equipados o en la mochila. Hay algunos
    comandos adicionales, especialmente para filtrar qué objetos se muestran según
    una propiedad particular y para mostrar los detalles sobre uno o
    dos objetos. Para ver cuáles son esos comandos adicionales, usa la tecla '?'
    para mostrar la ayuda en el juego para la comparación de equipo.

Comandos de Guardado y Salida
===========================

Guardar y Salir ('Ctrl-x')
  Para guardar tu juego para que puedas volver a él más tarde, usa este comando.
  Los archivos guardados también se generarán (con suerte) si el juego falla debido a
  un error del sistema. Después de morir, puedes usar tu archivo guardado para jugar de nuevo
  con las mismas opciones, etc.

Guardar ('Ctrl-s')
  Este comando guarda el juego pero no sale de Angband. Usa esto con frecuencia
  si eres paranoico acerca de que tu computadora falle (o se corte la luz)
  mientras juegas.

Retirarse (``Q``)
  Retira a tu personaje y sale de Angband. Se te pedirá que confirmes que
  realmente quieres hacer esto, y luego se te pedirá que verifiques esa elección. Lo
  único que se puede hacer con el archivo guardado de un personaje retirado es
  comenzar el juego desde el principio. Tendrás la opción de reutilizar
  las mismas opciones y elecciones de nacimiento que el personaje retirado cuando lo hagas.

Comandos de Archivos de Preferencias del Usuario
=======================

Interactuar con opciones (``=``)
  Te permite interactuar con las opciones. Ten en cuenta que usar las opciones de "trampa"
  puede marcar tu archivo guardado como no apto para la lista de puntuaciones altas. Las
  opciones de "ventana" te permiten especificar qué se debe dibujar en cualquiera de las
  subventanas especiales (no disponibles en todas las plataformas). Consulta los archivos de ayuda
  para :doc:`personalización <customize>` y :doc:`opciones <option>` para más
  información. También puedes interactuar con los mapas de teclas en este menú.

Interactuar con mapas de teclas - submenú de opciones
  Te permite interactuar con los mapas de teclas. Puedes cargar o guardar mapas de teclas desde
  archivos de preferencias de usuario, o definir mapas de teclas.

Interactuar con elementos visuales - submenú de opciones
  Te permite interactuar con elementos visuales. Puedes cargar o guardar elementos visuales desde
  archivos de preferencias de usuario, o modificar las asignaciones attr/char para los monstruos,
  objetos y características del terreno. Debes usar el comando "redibujar" ('^r')
  para redibujar el mapa después de cambiar las asignaciones attr/char. NOTA: Generalmente es
  más fácil modificar los elementos visuales a través de los menús de "conocimiento".

Interactuar con colores - submenú de opciones
  Permite al usuario interactuar con los colores. Este comando solo funciona en algunos
  sistemas. NOTA: Se usa comúnmente para aclarar el color 'Gris Oscuro'
  (ej. Arañas de Cueva) en pantallas con configuraciones alfa deficientes.

Comandos de Ayuda
=============

Ayuda (``?``)
  Muestra el sistema de ayuda en línea de Angband. Ten en cuenta que los archivos de ayuda son
  solo archivos de texto en un formato particular, y que otros archivos de ayuda pueden estar
  disponibles en la Red. En particular, hay una variedad de archivos de revelación
  que no vienen con la distribución estándar. Consulta el lugar de donde obtuviste
  Angband o pregunta en los foros de Angband, angband.live/forums/, sobre ellos.

Identificar Símbolo (``/``)
  Usa este comando para averiguar qué representa un carácter. Por ejemplo,
  presionando '/.', puedes averiguar que el símbolo ``.`` representa un
  espacio de suelo. Cuando se usa con un símbolo que representa criaturas, este
  comando te dirá solo qué clase de criatura representa el símbolo,
  no te dará información específica sobre una criatura que puedas ver. Para obtener
  eso, usa el comando Mirar.

  Hay tres símbolos especiales que puedes usar con el comando Identificar Símbolo
  para acceder a partes específicas de tu memoria de monstruos. Escribir
  'Ctrl-a' cuando se te pida un símbolo recordará detalles sobre todos los
  monstruos, escribir 'Ctrl-u' recordará detalles sobre todos los monstruos
  únicos y escribir 'Ctrl-n' recordará detalles sobre todos los monstruos no
  únicos.

  Si el carácter representa una criatura, se te pregunta si quieres
  recordar detalles. Si respondes que sí, la información sobre las criaturas que
  has encontrado con ese símbolo se muestra en la ventana de Recuerdo si
  está disponible, o en la pantalla si no. También puedes responder ``k`` para ver la
  lista ordenada por número de muertes, o ``p`` para ver la lista ordenada por
  nivel de mazmorra en el que normalmente se encuentra el monstruo. Presionar 'ESCAPE' en
  cualquier punto saldrá de este comando.

Versión del Juego (``V``)
  Este comando te dirá qué versión de Angband estás usando. Para
  más información, consulta el archivo de ayuda 'version.txt'.

Comandos Extra
==============

Alternar Ventana de Elección ('^e')
  Alterna la pantalla en cualquier subventana (si está disponible) que
  muestre tu inventario o equipo.

Redibujar Pantalla ('^r')
  Este comando se adapta a varios cambios en las opciones globales y redibuja todas
  las ventanas. Normalmente solo es necesario en situaciones anormales,
  como después de cambiar las asignaciones attr/char visuales, o habilitar
  el modo "gráficos".

Guardar captura de pantalla (|``)``|)
  Este comando guarda una "instantánea" de la pantalla actual en un archivo,
  incluyendo información de color codificada. El comando tiene dos variantes:

  - html, adecuado para ver en un navegador web.
  - html incrustado para foro vBulletin, adecuado para pegar en
    foros web como https://angband.live/forums/.

Teclas Especiales
=============

Ciertas teclas especiales pueden ser interceptadas por el sistema operativo o la máquina
anfitriona, causando resultados inesperados. En general, estas teclas especiales son
teclas de control y, a menudo, puedes deshabilitar sus efectos especiales.

Si estás jugando en un sistema UNIX o similar, entonces Ctrl-c interrumpirá
Angband. La segunda y tercera interrupción inducirán un timbre de advertencia, y la
cuarta inducirá tanto un timbre de advertencia como un mensaje especial, ya que la
quinta saldrá sin guardar (si Angband se compiló sin la opción
SETGID que coloca los archivos guardados en una ubicación compartida para todos los usuarios)
o matará a tu personaje (si Angband se compiló con la opción SETGID).
Además, 'Ctrl-z' suspenderá el juego y te devolverá al shell de comandos
original, hasta que reanudes el juego con el comando 'fg'. Las teclas 'Ctrl-\\',
'Ctrl-d' y 'Ctrl-s' no deberían ser interceptadas.

A menudo es posible especificar "teclas de control" sin presionar realmente
la tecla de control, escribiendo un acento circunflejo (``^``) seguido de la tecla. Esto es
útil para especificar comandos de tecla de control que podrían ser capturados por el
sistema operativo como se explicó anteriormente.

Presionar barra invertida (``\``) antes de un comando omitirá todos los mapas de teclas, y
la siguiente pulsación de tecla se interpretará como una tecla de "comando subyacente",
a menos que sea un acento circunflejo (``^``), en cuyo caso la pulsación de tecla después de esa se
convertirá en una tecla de control y se interpretará como un comando en el conjunto de teclas
subyacente de angband. Por ejemplo, la secuencia ``\`` + ``.`` + ``6`` siempre
significará "correr hacia el este", incluso si la tecla ``.`` ha sido mapeada a un comando
subyacente diferente.

Las teclas ``0``, ``^`` y ``\`` tienen un significado especial cuando se ingresan en
el indicador de comandos, y no hay una manera "útil" de especificar ninguna de ellas como
un "comando subyacente", lo cual está bien, ya que no tendrían efecto.

Para muchas solicitudes de entrada o consultas, el carácter especial ESCAPE abortará
el comando. Las indicaciones '[s/n]' pueden responderse con ``s`` o ``n``, o
'ESCAPE'. Las indicaciones de mensaje '-más-' pueden borrarse (después de leer
el mensaje mostrado) presionando 'ESCAPE', 'ESPACIO', 'RETORNO',
'AVANCE DE LÍNEA', o con cualquier pulsación de tecla, si la opción "mensajes rápidos" está
activada.

.. _command-counts:

Conteos de Comandos
==============

Algunos comandos se pueden ejecutar un número fijo de veces anteponiéndoles
un número. Los comandos contados se ejecutarán hasta que expire el número, hasta
que escribas cualquier carácter, o hasta que
:ref:`algo significativo suceda <disturb-player>`, como ser atacado.
Por lo tanto, un comando contado no funciona para atacar a otra criatura. Mientras
el comando se repite, el número de veces que quedan por repetir parpadeará
en la línea en la parte inferior de la pantalla.

Para dar un número a un comando, escribe ``0``, el número de repetición y luego el
comando. Si quieres dar un comando de movimiento y estás usando el
conjunto de comandos original (donde los comandos de movimiento son dígitos), presiona espacio
después del número y se te pedirá el comando.

Los comandos contados son muy útiles para comandos que consumen tiempo, ya que
se terminan automáticamente al tener éxito, o si eres atacado. También puedes
terminar cualquier comando contado (o descanso o carrera) escribiendo cualquier
carácter. Este carácter se ignora, pero es más seguro usar 'ESPACIO'
o 'ESCAPE', que siempre se ignoran como comandos en caso de que escribas el
comando justo después de que expire el número.

.. |``)``| replace:: ``)``