=====================
Explorando la Mazmorra
=====================

Después de haber creado tu personaje, comenzarás tu aventura en Angband.
Los símbolos que aparecen en tu pantalla representarán las paredes de la
mazmorra, el suelo, objetos, características y criaturas que acechan. Para
dirigir a tu personaje a través de su aventura, ingresarás comandos de un
solo carácter (consulta
:ref:`la sección de Descripciones de Comandos <command-descriptions>` para
descripciones de muchos de los comandos más útiles; la
:ref:`sección de Jugando el Juego <Playing the game>` y la ayuda dentro del juego,
``?``, tienen una lista completa pero solo con las descripciones más breves para
cada comando).

.. index::
   single: símbolos del mapa

Símbolos en Tu Mapa
===================

Los símbolos en tu mapa se pueden dividir en tres categorías: Características de
la mazmorra como paredes, suelo, puertas y trampas; Objetos que se pueden
recoger como tesoros, armas, dispositivos mágicos, etc.; y criaturas
que pueden o no moverse por la mazmorra, pero en su mayoría son dañinas para el
bienestar de tu personaje.

Algunos símbolos se usan para representar más de un tipo de entidad, y algunos
símbolos se usan para representar entidades en más de una categoría. El símbolo
"@" (por defecto) se usa para representar al personaje.

No será necesario recordar todos los símbolos y sus significados.
El comando "barra" (``/``) identificará cualquier carácter que aparezca en tu
mapa (consulta 'commands.txt'), y hay un menú completo de terreno,
objetos, monstruos, etc. usando el comando "tilde" (``~``).

Ten en cuenta que puedes usar un "archivo de preferencias de usuario" para cambiar
cualquiera de estos símbolos por algo con lo que te sientas más cómodo.


Características que no bloquean la línea de visión
--------------------------------------------------

===== =========================    =====  ==================================
``.``   Un espacio de suelo        ``1``    Entrada a la Tienda General
``.``   Una trampa (oculta)        ``2``    Entrada a la Armería
``^``   Una trampa (conocida)      ``3``    Entrada al Herrero de Armas
``;``   Un glifo de protección     ``4``    Entrada al Librero
``'``   Una puerta abierta         ``5``    Entrada a la Tienda de Alquimia
``'``   Una puerta rota            ``6``    Entrada a la Tienda Mágica
``<``   Una escalera hacia arriba  ``7``    Entrada al Mercado Negro
``>``   Una escalera hacia abajo   ``8``    Entrada a tu Hogar
``#``   Un charco de lava
===== =========================    =====  ==================================

Características que bloquean la línea de visión
-----------------------------------------------

===== =========================    =====  ==================================
``#``   Una puerta secreta         ``#``    Una pared
``+``   Una puerta cerrada         ``%``    Una veta mineral
``+``   Una puerta bloqueada       ``*``    Una veta mineral con tesoro
``:``   Un montón de escombros     ``:``    Un montón de escombros transitables
===== =========================    =====  ==================================

Objetos
-------

=====  =============================    =====  =============================
``!``    Una poción (o frasco)          ``/``    Un arma de asta
``?``    Un pergamino (o libro)         ``|``    Un arma de filo
``,``    Una seta (o comida)            ``\``    Un arma de mango
``-``    Una varita o bastón            ``}``    Una honda, arco o ballesta
``_``    Un bastón                      ``{``    Una bala, flecha o virote
``=``    Un anillo                      ``(``    Armadura blanda
``"``    Un amuleto                     ``[``    Armadura dura
``$``    Oro o gemas                    ``]``    Armadura diversa
``~``    Luces, Herramientas, Cofres, etc  ``)``    Un escudo
``&``    Múltiples objetos
=====  =============================    =====  =============================

Monstruos
---------

=====   ===================   =====  ====================================
``$``     Monedas Rastreras    ``,``    Parche de Setas
``a``     Hormiga Gigante      ``A``    Ainu
``b``     Murciélago Gigante   ``B``    Ave
``c``     Ciempiés Gigante     ``C``    Canino (Perro)
``d``     Dragón               ``D``    Dragón Ancestral
``e``     Ojo Flotante         ``E``    Elemental
``f``     Felino (Gato)        ``F``    Libélula
``g``     Golem                ``G``    Fantasma
``h``     Humanoide            ``H``    Híbrido
``i``     Cosa Repugnante      ``I``    Insecto
``j``     Gelatina             ``J``    Serpiente
``k``     Kobold               ``K``    Escarabajo Asesino
``l``     Árbol/Ent            ``L``    Liche
``m``     Moho                 ``M``    Hidra de Múltiples Cabezas
``n``     Naga                 ``N``    (no usado)
``o``     Orco                 ``O``    Ogro
``p``     Humano "persona"     ``P``    Gigante "persona"
``q``     Cuadrúpedo           ``Q``    Quylthulg (Montículo de Carne Pulsante)
``r``     Roedor               ``R``    Reptil/Anfibio
``s``     Esqueleto            ``S``    Araña/Escorpión/Garrapata
``t``     Aldeano              ``T``    Troll
``u``     Demonio Menor        ``U``    Demonio Mayor
``v``     Vórtice              ``V``    Vampiro
``w``     Gusano o Masa de Gusanos  ``W``    Espectro/Espíritu
``x``     (no usado)           ``X``    Xorn/Xaren
``y``     Yeek                 ``Y``    Yeti
``z``     Zombi/Momia          ``Z``    Sabueso Céfiro
=====   ===================   =====  ====================================

El Nivel de la Ciudad
=====================

El nivel de la ciudad es donde comenzarás tu aventura. La ciudad consta de
ocho edificios (cada uno con una entrada), algunos aldeanos y una muralla que
rodea la ciudad y puede contener corrientes de lava. La primera vez que estés
en la ciudad será de día, pero ten en cuenta que el sol sale y se pone (de
manera bastante instantánea) a medida que pasa el tiempo.

Aldeanos
========

La ciudad contiene muchos tipos diferentes de personas. Hay golfillos
callejeros, niños pequeños que acosarán a un aventurero por dinero y parecen
salir de la nada cuando se emocionan. Los idiotas llorones son una molestia
constante, pero no son dañinos. Los borrachos públicos deambulan por la ciudad
cantando y no representan una amenaza para nadie. Los pícaros astutos merodean
buscando una víctima probable para atracar. Y, finalmente, ninguna ciudad
estaría completa sin una turba de guerreros medio borrachos, que se ofenden o se
molestan solo por diversión. (Se supone que hay otras personas en la ciudad,
pero no están representadas en la pantalla ya que no interactúan con el
jugador de ninguna manera.)

La mayoría de los aldeanos deben ser evitados por la mayor distancia posible
cuando deambulas de una tienda a otra. Sin embargo, estallarán peleas, así que
estate preparado. Dado que tu personaje creció en este mundo de intriga, no se
otorga experiencia por matar a los habitantes de la ciudad, aunque puedes
adquirir tesoros.

Edificios de la Ciudad
======================

Tu personaje comenzará su aventura con algunos suministros básicos y algo de
oro extra con el que comprar más suministros en las tiendas de la ciudad. Si
eliges jugar con la opción de kit inicial activada (activada por defecto), tu
personaje comenzará con más objetos pero con menos oro.

Puedes entrar a cualquier tienda abierta para comprar objetos del tipo
apropiado. El precio que el tendero pide depende del precio del objeto.
Por defecto, las tiendas no compran objetos al jugador. Si eliges jugar con la
opción de no vender desactivada (activada por defecto), comprarán, pero cada
tendero tiene una cantidad máxima que está dispuesto a ofrecer por cualquier
objeto, independientemente de cuánto valga realmente.

Una vez dentro de una tienda, verás el nombre y la raza del dueño de la tienda,
el nombre de la tienda, la cantidad máxima de dinero que el dueño de la tienda
pagará por cualquier objeto y el inventario de la tienda, listado junto con los
precios.

Puedes presionar ``?`` para obtener una lista parcial de los comandos
disponibles. Ten en cuenta que muchos de los comandos que funcionan en la
mazmorra también funcionan en las tiendas, pero algunos no, especialmente
aquellos que implican "usar" objetos.

Las tiendas no siempre tienen todo en stock. A medida que avanza el juego,
pueden obtener nuevos objetos, así que compruébalas de vez en cuando. Las
tiendas reabastecen después de 10000 turnos de juego, pero el inventario nunca
cambiará mientras estés en la ciudad, incluso si guardas la partida y regresas.
Debes estar en la mazmorra para que la tienda reabastezca. Además, si les
vendes un objeto, puede ser vendido a un cliente mientras estás de aventura,
así que no esperes poder recuperar todo lo que has vendido. Si tienes mucho oro
de sobra, puedes comprar todos los objetos de una tienda, lo que inducirá al
dueño a sacar nuevo stock y tal vez incluso a jubilarse.

Los dueños de las tiendas no aceptarán objetos dañinos o inútiles conocidos. Si
un objeto no está identificado, un dueño de tienda lo aceptará (pagando algo de
oro si la venta está activada), identificará el objeto y luego lo añadirá al
stock de la tienda si es bueno o lo tirará. Puedes usar esta función para
aprender sabores de objetos o las propiedades mágicas, llamadas "runas", de los
objetos que se pueden equipar.

.. index::
   single: tienda general
   seealso: tienda; tienda general

La Tienda General (``1``)
  La Tienda General vende alimentos, algo de ropa, antorchas, aceite, palas y
  picos. Todos estos objetos y algunos otros pueden venderse de vuelta a la
  tienda general por dinero. La tienda general reabastece como cualquier tienda,
  pero los tipos de inventario nunca cambian.

.. index::
   single: armería
   seealso: tienda; armería

La Armería (``2``)
  La Armería es donde se forja la armadura de la ciudad. Todo tipo de equipo de
  protección se puede comprar y vender aquí. Cuanto más profundo avances en la
  mazmorra, más exótico será el equipo que encontrarás en la armería.
  Sin embargo, algunos tipos de armadura nunca aparecerán aquí a menos que
  los vendas.

.. index::
   single: herrero de armas
   seealso: tienda; herrero de armas

La Tienda del Herrero de Armas (``3``)
  La Tienda del Herrero de Armas es donde se forjan las armas de la ciudad. Se
  pueden comprar y vender aquí armas de mano y de proyectil, junto con flechas,
  virote y balas. Al igual que con la armería, no todos los tipos de armas
  estarán en stock aquí, a menos que el jugador las venda primero a la tienda.

.. index::
   single: librero
   seealso: tienda; librero

El Librero (``4``)
  El Librero tiene suministros de los libros más simples que necesitan los
  usuarios de magia y comprará los libros más avanzados que se pueden encontrar
  en la mazmorra.

.. index::
   single: tienda de alquimia
   seealso: tienda; tienda de alquimia

La Tienda de Alquimia (``5``)
  La Tienda de Alquimia se ocupa de todo tipo de pociones y pergaminos.

.. index::
   single: tienda de magia
   seealso: tienda; tienda de magia

La Tienda de Magia (``6``)
  La Tienda de Magia se ocupa de todo tipo de anillos, varitas, amuletos y
  bastones. Dar (o vender cuando la venta está activada) una varita o bastón
  útil sin cargas a la Tienda de Magia es una forma conveniente de recargar
  el objeto, si tienes suficiente oro para recomprarlo.

.. index::
   single: mercado negro
   seealso: tienda; mercado negro

El Mercado Negro (``7``)
  El Mercado Negro venderá y comprará cualquier cosa a precios abusivos.
  Sin embargo, ocasionalmente tiene objetos **muy** buenos. Con la excepción
  de los artefactos, cualquier objeto encontrado en la mazmorra puede aparecer
  en el mercado negro.

.. index::
   single: hogar
   seealso: tienda; hogar

Tu Hogar (``8``)
  Esta es tu casa donde puedes almacenar objetos que no puedes llevar en tus
  viajes o que necesitarás más adelante.

Dentro de la Mazmorra
=====================

Una vez que tu personaje esté adecuadamente abastecido de comida, luz, armadura
y armas, estará listo para entrar en la mazmorra. Muévete sobre el símbolo ``>``
y usa el comando "Bajar" (``>``).

Tu personaje entrará en un laberinto de escaleras interconectadas y finalmente
llegará a algún lugar del primer nivel de la mazmorra. Cada nivel de la
mazmorra tiene cincuenta pies de altura (por lo que el nivel de mazmorra
"Lev 1" a menudo se llama "50 ft") y está dividido en regiones rectangulares
(grandes) (varias veces más grandes que la pantalla) por roca permanente. Una
vez que dejas un nivel por una escalera, nunca volverás a encontrar tu camino
de regreso a esa región de ese nivel, pero hay un número infinito de otras
regiones a la misma "profundidad" que puedes explorar más tarde. Los monstruos,
por supuesto, pueden usar las escaleras, y puedes encontrarlos nuevamente, pero
no te perseguirán escaleras arriba o abajo.

En la mazmorra, hay muchas cosas que encontrar, pero tu personaje debe
sobrevivir a muchos encuentros horribles y desafiantes para encontrar el tesoro
que hay por allí.

Hay dos fuentes de luz una vez dentro de la mazmorra. Luz permanente que ha sido
colocada mágicamente dentro de las habitaciones y una fuente de luz llevada por
el jugador. Si ninguna está presente, el personaje no podrá ver.
Esto afectará la búsqueda, abrir cerraduras, desarmar trampas, leer
pergaminos, lanzar hechizos, examinar libros, etc. Así que ten mucho cuidado de
no quedarte sin luz!

Un personaje debe empuñar una antorcha o lámpara para proporcionar su propia
luz. Una antorcha o lámpara quema combustible a medida que se usa, y una vez
que se queda sin combustible, deja de proporcionar luz. Se te advertirá a
medida que la luz se acerque a este punto. Puedes usar el comando "Combustible"
(``F``) para reabastecer tu linterna (con frascos de aceite), y es una buena
idea llevar antorchas o frascos de aceite adicionales, según corresponda. Hay
rumores de objetos de poder excepcional que brillan con su propia luz
interminable.

Estos dos últimos párrafos se aplican a la mayoría de las clases, pero no a los
nigromantes. A los nigromantes no les gusta la luz y se envuelven en la
oscuridad. Generalmente es mejor que no lleven luz, pero tampoco obtienen
ninguna de las bonificaciones que pueden provenir de fuentes de luz mágicas.

Objetos Encontrados en la Mazmorra
==================================

Las minas están llenas de objetos que esperan ser recogidos y usados. ¿Cómo
llegaron allí? Bueno, la fuente principal de objetos útiles son todos los
aventureros imprudentes que se adentraron en la mazmorra antes que tú. Son
asesinados y las criaturas útiles esparcen los diversos tesoros por toda la
mazmorra.

Varios objetos pueden ocupar una ubicación determinada en el suelo, que puede
o no contener también una criatura. Sin embargo, las puertas, los escombros,
las trampas y las escaleras no pueden coexistir con los objetos. Como se indica
a continuación, cualquier objeto puede ser en realidad una "pila" de hasta 40
objetos idénticos. Con las opciones predeterminadas, varias pilas pueden estar
en la misma cuadrícula formando un "montón".

Recoges objetos moviéndote sobre ellos. Puedes llevar hasta 23 objetos
diferentes en tu mochila mientras usas y empuñas hasta 12 otros. Aunque estás
limitado a 23 objetos diferentes, cada objeto puede ser en realidad una "pila"
de hasta 40 objetos similares. Si te |``q``uitas| un objeto, irá a tu mochila
si hay espacio: si no hay espacio en tu mochila, caerá al suelo, así que ten
cuidado al cambiar un arma empuñada o una pieza de armadura usada por otra
cuando tu mochila está llena.

.. |``q``uitas| replace:: ``q``\uitas

Sin embargo, estás limitado en la cantidad total de peso que puedes llevar.
Si superas este valor, te vuelves más lento, lo que facilita que los monstruos
te persigan. Ten en cuenta que no hay un límite superior de cuánto puedes
llevar, si no te importa ser lento. Tu "límite" de peso está determinado por tu
fuerza.

Muchos objetos encontrados en la mazmorra tienen comandos especiales para su
uso. Las varitas deben ser Apuntadas, los bastones deben ser Usados, los
pergaminos deben ser Leídos y las pociones deben ser Bebidas. En general, no
solo puedes usar objetos de tu mochila, sino también objetos en el suelo, si
estás parado sobre ellos. Al principio del juego, a todos los objetos se les
asigna un "sabor" aleatorio. Por ejemplo, las pociones de 'curar heridas leves'
podrían ser 'pociones rojas'. Si nunca has usado, vendido o comprado una de
estas pociones, solo verás el sabor. Puedes aprender qué tipo de objeto es
vendiéndolo a una tienda o usándolo (aprender mediante el uso de varitas,
bastones y cañas puede requerir algo de trabajo, ya que es posible que
necesites el objetivo correcto o algo apropiado cerca para identificar lo que
hizo el dispositivo). Por último, los objetos en las tiendas cuyo sabor aún no
has identificado se etiquetarán como '{no visto}'.

Los cofres son objetos complejos que contienen trampas, cerraduras y
posiblemente tesoros u otros objetos dentro una vez abiertos. Muchos de los
comandos que se aplican a trampas o puertas también se aplican a los cofres y,
como las trampas y puertas, estos comandos no funcionan si llevas el cofre.

Se discutirá aquí un objeto en particular. El pergamino de "Palabra de
Retorno" se puede encontrar dentro de la mazmorra o comprar en el alquimista
de la ciudad. Todas las clases comienzan con uno de estos pergaminos en su
inventario. Actúa de dos maneras, dependiendo de tu ubicación actual. Si se lee
dentro de la mazmorra, te teletransportará de vuelta a la ciudad. Si se lee en
la ciudad, te teletransportará de vuelta al nivel más profundo de la mazmorra
en el que tu personaje haya estado anteriormente. Esto hace que el pergamino
sea muy útil para regresar a los niveles más profundos de Angband. Una vez que
se ha leído el pergamino, tarda un tiempo en actuar, así que no esperes que te
salve en una crisis. Durante este tiempo, aparecerá la palabra 'recuerdo' en la
parte inferior de la pantalla, debajo de la mazmorra. Leer un segundo pergamino
antes de que el primero surta efecto cancelará la acción.

Puedes "inscribir" cualquier objeto con una inscripción textual de tu elección.
Estas inscripciones no tienen límite de longitud, aunque puede que no puedas
ver la inscripción completa en el objeto. El juego otorga un significado
especial a las inscripciones que contienen cualquier texto de la forma '@#',
'@x#', '!!', '!x', '!*', '^x', '^*' o '=g`; consulta :ref:`la sección sobre
inscripciones <inscribing>` para más detalles.

El juego proporciona algunas inscripciones "falsas" para ayudarte a llevar un
registro de tus pertenencias. Las armas, armaduras y joyas que tienen
propiedades que aún no conoces obtendrán una etiqueta '{??}'. Las armas,
armaduras y joyas con una maldición conocida obtendrán una etiqueta
'{maldito}'. Las varitas, bastones y cañas pueden obtener una etiqueta
'{probado}' después de su uso, particularmente si tienen un efecto sobre un
monstruo y se probaron en ausencia de monstruos. Las varitas o bastones no
identificados sin cargas restantes tendrán una etiqueta '{vacío}'. Los objetos
que están marcados como ignorados tendrán una etiqueta '{ignorado}'. Si un
objeto tiene más de una de estas etiquetas o también tiene una inscripción que
proporcionaste, todas las etiquetas se mostrarán en un solo conjunto de
corchetes, separadas por comas, y la inscripción que proporcionaste, si la
hay, será la primera. Por ejemplo, podrías ver '{vacío, probado}' en una varita
o bastón no identificado o '{maldito, ??}' en una pieza de armadura con una o
más maldiciones conocidas y una o más propiedades mágicas no identificadas.

Se rumorea que se pueden encontrar anillos de poder y libros de hechizos
extraños más profundamente en la mazmorra...

Y, por último, una advertencia final: no todos los objetos son lo que parecen.
La línea entre la comida sabrosa y una seta venenosa es muy fina, y a veces un
cofre lleno de tesoro desarrollará dientes en su tapa y te morderá la mano...

Objetos Malditos
================

Algunos objetos, a menudo objetos de gran poder, han sido maldecidos. Hay
muchas maldiciones en el juego y pueden aparecer en cualquier objeto que se
pueda usar. Las maldiciones pueden tener un efecto negativo (o a veces
positivo) en las propiedades de un objeto o causar que ocurran cosas malas al
jugador al azar.

Puedes optar por usar el objeto a pesar de sus maldiciones o intentar
desmaldecirlo usando magia. Una advertencia: el intento fallido de desmaldecir
hace que el objeto se vuelva frágil, y un objeto frágil puede ser destruido en
futuros intentos de eliminar la maldición. Depende de ti equilibrar los
riesgos y recompensas en tu uso de objetos malditos.

Minería
=======

Parte del tesoro dentro de la mazmorra solo se puede encontrar extrayéndolo de
las paredes. Existen muchas vetas ricas en cada nivel, pero deben ser
encontradas y extraídas. Las vetas de cuarzo son las más ricas, produciendo la
mayor cantidad de metales y gemas, pero las vetas de magma también tendrán
algunos tesoros escondidos dentro.

La minería es bastante difícil sin un pico o una pala. Los picos y las palas
tienen una habilidad mágica adicional expresada como '(+#)'. Cuanto mayor sea
el número, mejor será la habilidad de excavación mágica de la herramienta. Un
pico o una pala también tienen pluses para golpear y dañar, y se pueden usar
como arma porque, de hecho, lo son.

Cuando se localiza una veta de cuarzo o magma, el personaje puede empuñar su
pico o pala y comenzar a excavar una sección. Cuando se elimina esa sección,
puede localizar otra sección de la veta y comenzar el proceso nuevamente. Dado
que la roca de granito es mucho más difícil de excavar, es mucho más rápido
seguir la veta exactamente y excavar alrededor del granito. Eventualmente, se
vuelve más fácil simplemente matar monstruos y descubrir objetos en la
mazmorra para vender, que caminar excavando en busca de tesoros. Pero, al
principio, las vetas minerales pueden ser una fuente maravillosa de tesoro
fácil.

Si el personaje tiene un pergamino, bastón o hechizo de localización de
tesoros, puede localizar inmediatamente todos los yacimientos de tesoro dentro
de una veta que se muestran en la pantalla. Esto hace que la minería sea mucho
más fácil y rentable.

Ten en cuenta que un personaje con alta fuerza y/o un arma pesada no necesita
una pala/pico para excavar, pero incluso el personaje más fuerte se
beneficiará de un pico si intenta excavar a través de una pared de granito.

A veces es posible que un personaje quede atrapado dentro de la mazmorra al
usar varios hechizos y objetos mágicos, encontrar el tipo correcto de trampa o
entrar en la mazmorra (ya sea por palabra de retorno o por una escalera cuando
se juega con escaleras desconectadas) y encontrarse en una parte que no tiene
una ruta transitable hacia una escalera. Por lo tanto, puede ser una buena
idea llevar siempre algún tipo de herramienta de excavación, incluso cuando no
planeas excavar en busca de tesoros.

Hay rumores de ciertas salas increíblemente rentables enterradas en lo
profundo de la mazmorra y completamente rodeadas de roca permanente y paredes
de granito, que requieren un implemento de excavación o medios mágicos para
entrar. Los mismos rumores implican que estas salas están custodiadas por
monstruos increíblemente poderosos, ¡así que ten cuidado!

Trampas
=======

Hay muchas trampas ubicadas en la mazmorra de peligro variable. Estas trampas
están ocultas a la vista y se activan solo cuando tu personaje camina sobre
ellas. Si has encontrado una trampa, puedes intentar |``D``esarmarla|, pero el
fracaso puede significar activarla. Las trampas pueden ser peligros físicos
como pozos o runas mágicas o inscripciones que causarán un efecto cuando se
activen. Tu personaje puede ser mejor desarmando uno de estos tipos de trampas
que el otro.

.. |``D``esarmarla| replace:: ``D``\esarmarla

Todos los personajes tienen la posibilidad de notar trampas cuando aparecen por
primera vez en la vista (dependiendo de la habilidad de búsqueda). Algunos
jugadores también tendrán acceso a medios mágicos para detectar todas las
trampas dentro de un cierto radio. Si usas tal hechizo o dispositivo y aún
estás dentro del área que verificó, habrá una etiqueta 'DTrap' en verde en la
parte inferior de la pantalla, debajo del mapa de la mazmorra. Si estás en el
límite de un área donde se realizó una detección de trampas (el cuadrado actual
se vio afectado por un efecto de detección de trampas pero uno o más de los
cuatro vecinos en las direcciones cardinales no lo estaban), habrá una etiqueta
'DTrap' en amarillo en la parte inferior de la pantalla.

Algunos monstruos tienen la capacidad de crear nuevas trampas en el nivel que
pueden estar ocultas, incluso si el jugador está en una zona detectada. La
detección solo encuentra las trampas que existen en el momento de la detección,
no te informa sobre nuevas que se hayan creado desde entonces.

Escaleras, Puertas Secretas, Pasajes y Habitaciones
===================================================

Las escaleras son la forma de llegar más profundo o salir de la mazmorra. Los
símbolos para las escaleras arriba y abajo son los mismos que los comandos para
usarlas. Un ``<`` representa una escalera arriba y un ``>`` representa una
escalera abajo. Debes mover a tu personaje sobre la escalera antes de poder
usarla.

La mayoría de los niveles tienen al menos una escalera arriba y al menos dos
escaleras abajo. Puede que tengas problemas para encontrar algunas puertas
secretas bien ocultas, o puede que tengas que excavar a través de obstrucciones
para llegar a ellas, pero siempre puedes encontrar las escaleras si buscas lo
suficiente. Las escaleras, como la roca permanente y las entradas de las
tiendas, no pueden ser destruidas por ningún medio.

Muchas puertas secretas se utilizan dentro de la mazmorra para confundir y
desmoralizar a los aventureros lo suficientemente imprudentes como para entrar,
aunque todas las puertas secretas se pueden descubrir al pisar junto a ellas.
Las puertas secretas a veces ocultan habitaciones o corredores, o incluso
secciones enteras de ese nivel de la mazmorra. A veces simplemente ocultan
pequeños armarios vacíos o incluso callejones sin salida. Las puertas secretas
siempre parecen paredes de granito, al igual que las trampas siempre parecen
suelos normales.

Las criaturas en la mazmorra generalmente conocen y usan estas puertas
secretas, y a menudo se puede contar con que las dejen abiertas detrás de
ellas cuando pasan.

.. index::
   single: sensaciones de nivel

Sensaciones de nivel y objeto
=============================

A menos que hayas desactivado la opción de obtener sensaciones, recibirás un
mensaje al entrar en una mazmorra que te dará una sensación general de lo
peligroso que es ese nivel.

Los posibles mensajes son:

===   =========================================
 1    "Este parece un lugar tranquilo y apacible"
 2    "Este parece un lugar dócil y protegido"
 3    "Este lugar parece razonablemente seguro"
 4    "Este lugar no parece demasiado arriesgado"
 5    "Te sientes nervioso por este lugar"
 6    "Te sientes ansioso por este lugar"
 7    "Este lugar parece terriblemente peligroso"
 8    "Este lugar parece homicida"
 9    "Los presagios de muerte acechan en este lugar"
===   =========================================

Esta sensación depende solo de los monstruos presentes en la mazmorra cuando
entras por primera vez. No se reducirá a una sensación más segura a medida que
matas monstruos, ni aumentará si aparecen nuevos invocados.
Esta sensación también depende de tu profundidad actual en la mazmorra. Una
mazmorra por la que te sientes nervioso a 2000' es mucho más peligrosa que una
homicida a 50'.

Una vez que hayas explorado una cierta cantidad de la mazmorra, también
obtendrás una sensación sobre lo buenos que son los objetos que yacen en el
suelo de la mazmorra.

Los posibles mensajes son:

===   =========================================
 1    "aquí no hay más que telarañas."
 2    "aquí solo hay restos de basura."
 3    "no hay muchos tesoros aquí."
 4    "puede que no haya mucho interesante aquí."
 5    "puede que haya algo valioso aquí."
 6    "hay buenos tesoros aquí."
 7    "hay muy buenos tesoros aquí."
 8    "hay excelentes tesoros aquí."
 9    "hay tesoros magníficos aquí."
 $    "¡sientes un objeto de poder maravilloso!"
===   =========================================

El último mensaje indica que hay un artefacto presente y solo es posible si la
opción de perder artefactos está activada (si esa opción está desactivada, un
artefacto garantizará una sensación de 5 o mejor).

Puedes revisar tu sensación de nivel en cualquier momento usando el comando ^f.
También puedes consultarla verificando el indicador LF: en la parte inferior
izquierda de la pantalla. El primer número después de él es la sensación de
nivel y el segundo es la sensación de objeto. El segundo será ? si necesitas
explorar más antes de obtener una sensación sobre el valor de los tesoros
presentes en la mazmorra.

.. index::
   single: ganar

Ganando el Juego
================

Si tu personaje ha matado a Sauron (una tarea difícil), que vive en el nivel
99 (4950') de la mazmorra, aparecerá una escalera mágica que te permitirá
finalmente llegar al nivel 100. Morgoth acecha en este nivel de su mazmorra,
y no podrás bajar de su nivel hasta que lo hayas matado.
Intenta evitar deambular por el nivel 100 a menos que estés listo para él,
ya que tiene la costumbre de venir hacia ti a través de la mazmorra, con el
Poderoso Martillo 'Grond' en la mano, para matarte por tu insolencia.

Si realmente sobrevives al intento de matar a Morgoth, recibirás el estado de
GANADOR. Puedes continuar explorando, e incluso puedes guardar la partida y
jugar más tarde, pero dado que has derrotado a la criatura más fuerte que
existe, realmente no tiene mucho sentido. A menos que desees escuchar los
rumores de un poderoso anillo enterrado en algún lugar de la mazmorra, o una
armadura de escamas de dragón que resiste todo...

Cuando estés listo para retirarte, presiona la tecla ``Q`` para que tu
personaje sea ingresado en la lista de puntajes altos como ganador. Ten en
cuenta que hasta que te retires, aún puedes ser asesinado, por lo que es
posible que quieras retirarte antes de encontrarte con otra horda de demonios
mayores.

.. index::
   single: morir

Sobre la Muerte y el Morir
==========================

Si tu personaje cae por debajo de 0 puntos de golpe, ha muerto y no puede ser
restaurado (con la excepción de que los caballeros negros pueden caer por
debajo de cero puntos de golpe en algunas circunstancias y vivir para contarlo).
Se mostrará una lápida con información sobre tu personaje. También se te
permite obtener un registro de tu personaje y todo tu equipo (identificado) ya
sea en la pantalla o en un archivo.

Tu personaje dejará atrás un archivo de guardado reducido, que contiene solo
tus elecciones de opciones. Se puede restaurar, en cuyo caso se genera un nuevo
personaje exactamente como si el archivo no estuviera allí.

Hay varias formas de "engañar" a la muerte (incluyendo el uso de una "opción de
trampa" especial) cuando de otro modo ocurriría. Esto sanará completamente a tu
personaje, lo devolverá a la ciudad y lo marcará de varias maneras como un
personaje que ha engañado a la muerte. Engañar a la muerte, como usar
cualquiera de las "opciones de trampa", evitará que tu personaje aparezca en la
lista de puntajes altos.