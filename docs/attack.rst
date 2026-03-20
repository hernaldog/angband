==================
Atacando monstruos
==================

Atacar y ser atacado
============================

Atacar es simple en Angband. Si te mueves hacia una criatura, la atacas.
Puedes atacar desde la distancia disparando un proyectil o por medios mágicos
(como apuntar una varita). Las criaturas atacan de la misma manera. Si se mueven
hacia ti, te atacan. Algunas criaturas también pueden lanzar hechizos a
distancia, y otras pueden usar varias armas de aliento (como fuego) contra ti
desde la distancia.

Las criaturas en las paredes no pueden ser atacadas por varitas u otros ataques mágicos
normalmente detenidos por las paredes, ni pueden ser disparadas con arcos y flechas.
Excavar en la pared (usando el comando "excavar" o "alterar") te permitirá
atacar cualquier criatura en la pared con tu arma principal. Esto se aplica
a las criaturas que "atraviesan" las paredes: si "perforan" las paredes, la
pared ya no está allí, y la criatura puede ser objetivo normalmente.

Si estás empuñando un arma, se usa el daño del arma cuando
golpeas a una criatura. De lo contrario, obtienes un solo puñetazo que hace un daño mínimo.

Puedes ``w``\ildir un arma para el combate cuerpo a cuerpo, y también un lanzador
de proyectiles (arco, ballesta o honda). También puedes usar un amuleto (alrededor del
único cuello del personaje), dos anillos (en los dos dedos "anulares",
es decir, el tercer dedo de cada mano: un anillo mágico no funciona cuando
se usa en cualquier otro dedo, ni se pueden usar dos en el mismo dedo), una
fuente de luz y un conjunto completo de armadura: armadura corporal, escudo, casco, guantes,
botas y una capa. Cualquiera o todos estos objetos pueden proporcionar poderes al
personaje en términos de bonificaciones para-golpear, para-dañar, para-clase de armadura o para
otras estadísticas.

Disparar un proyectil (mientras se empuña el lanzador apropiado) es la única forma
de obtener el poder "total" del proyectil. Por supuesto, puedes lanzar una
flecha a un monstruo sin dispararla, pero encontrarás que los efectos no
serán lo que esperabas.

Los impactos y fallos se determinan por la habilidad para golpear versus la clase de armadura. Un impacto
es un golpe que hace algo de daño; un fallo puede de hecho alcanzar un objetivo, pero
no hace ningún daño. Las clases de armadura más altas hacen que sea más difícil hacer daño,
y por lo tanto conducen a más fallos. Los personajes con clases de armadura más altas también
reciben una reducción de daño. Esto no es cierto para los monstruos, cuya CA solo
afecta la dificultad del personaje para golpearlos.

Si deseas ver cuánto daño hará tu arma, puedes
``I``\nspeccionarla. Verás el número de golpes y cuánto daño
harías por ronda, incluyendo información sobre si tu arma daña
otros tipos de monstruos de manera diferente.

Memorias de monstruos
================

Hay cientos de criaturas diferentes en los pozos de Angband, muchas de las
cuales tienen el mismo símbolo de letra y color en la pantalla. La especie
exacta de una criatura se puede descubrir |``l``\ookeando|. También es
muy difícil seguir la pista de las capacidades de varias criaturas.
Afortunadamente, Angband mantiene automáticamente un registro de tus experiencias con una
criatura en particular. Esta característica se llama la memoria de monstruos. Tu
memoria de monstruos recuerda los ataques particulares de cada criatura (ya sea
técnicamente un monstruo o no) que has sufrido, así como recuerda si
has observado que se multiplican o se mueven erráticamente, o sueltan tesoros,
etc. De lo contrario, simplemente tendrías que tomar notas, lo cual es una molestia
innecesaria.

.. |``l``\ookeando| replace:: ``l``\ookeando

Si has matado suficientes de una criatura en particular, o has sufrido suficientes
ataques, recordar la memoria del monstruo también puede proporcionarte información
no disponible de otra manera, como una clase de armadura, dados de vida, tipos de hechizos,
frecuencia de lanzamiento de hechizos o la cantidad de daño por alientos o hechizos.
Estos ataques estarán codificados por colores para informarte si
actualmente resistes un ataque específico. Rojo o naranja significa que no lo resistes,
amarillo significa que lo resistes parcialmente y verde significa que lo resistes o eres
inmune. Si atacas a un monstruo con ataques elementales específicos,
aprenderás si el monstruo resiste ese elemento o si es inmune. Hay
otros medios mágicos para aprender sobre las habilidades de los monstruos que no requieren
que realmente experimentes los ataques.

Esta memoria puede ser utilizada por todos tus personajes; se almacena en un archivo
llamado 'lore.txt' en tu directorio de usuario (~/.angband/Angband en Linux,
lib/user en Windows, Documents/Angband en macOS).

Tu arma
===========

Llevar un arma en tu mochila no te sirve de nada. Debes empuñar un
arma antes de que pueda ser utilizada en una pelea. Se puede mantener un arma secundaria
manteniéndola en la mochila y cambiándola con el arma principal cuando sea
necesario. Esto se usa más a menudo cuando se cambia entre dos armas, cada una de las
cuales proporciona un poder raro que el personaje necesita en dos momentos separados.
Ten en cuenta que una herramienta de excavación solo necesita ser llevada en tu mochila, ya que cuando intentas
excavar, tu mejor herramienta de excavación se usará automáticamente.

.. index::
   single: para-golpear; valores de equipo
   single: para-dañar; valores de equipo

Las armas tienen dos características mágicas principales, su habilidad encantada para
golpear y su habilidad encantada para hacer daño, expresadas como '(+#,+#)'. Un
arma normal sería '(+0,+0)'. Muchas armas en Angband tienen bonificaciones para
golpear y/o para dañar.

.. index::
   single: golpes; límite superior

Angband asume que tu juventud en el entorno hostil cerca de las mazmorras
te ha enseñado los méritos relativos de las diferentes armas, y muestra como
parte de su descripción los dados de daño que definen sus capacidades.
Cualquier encantamiento de daño se suma a la tirada de dados para esa arma. Los dados
utilizados para un arma dada se muestran como 'XdY'. El número ``X`` indica
cuántos dados tirar, y el número ``Y`` indica cuántas caras tienen.
Un arma '2d6' dará así daño de 2 a 12, más cualquier bonificación de daño.
El peso de un arma también es una consideración. Las armas pesadas pueden golpear
más fuerte, pero también son más difíciles de usar. Dependiendo de tu fuerza,
destreza, clase de personaje y peso del arma, puedes obtener ataques más
rápidamente: la alta destreza y fuerza y el bajo peso del arma son los factores
principales. Los guerreros pueden obtener hasta un máximo de 6 ataques por ronda:
los lanzadores de hechizos puros están limitados a solo 4: otras clases pueden obtener hasta 5. Tus ataques
por ronda con un arma se muestran como un decimal, por ejemplo, 2.3 o 3.4, etc.
Las fracciones toman la forma de energía no utilizada que se transfiere a tu
próximo turno.

Las armas de proyectiles, como los arcos, tienen sus características añadidas a las del
proyectil utilizado, si se utiliza la combinación arma/proyectil adecuada, y
luego se aplica el multiplicador del lanzador al daño total, haciendo que las armas
de proyectiles sean muy poderosas con los proyectiles adecuados, especialmente si están
encantados. Al igual que las armas, |``I``\nspeccionar| la munición te dirá cuánto daño
harás con tu lanzador de proyectiles actual.

.. |``I``\nspeccionar| replace:: ``I``\nspeccionar

Finalmente, algunas armas raras tienen habilidades especiales. Estas se llaman armas
de ego, y son temidas por grandes y pequeños. Un arma de ego debe ser empuñada para
recibir el beneficio de sus habilidades. Se debe tener en cuenta que algunos de estos
objetos son considerablemente más poderosos que otros, y generalmente los más
poderosos son los más raros. Algunos objetos tendrán un efecto obvio,
como un aumento en la infravisión o fuerza extra. Estos efectos se notarán
tan pronto como empuñes el objeto. Otros efectos, como la mayoría de las
resistencias, necesitarán ser aprendidos. Puedes aprenderlos ya sea
sufriendo un ataque apropiado o usando medios mágicos de
identificación.

Algunas de las armas de ego más comunes se describen al final de este archivo.

El equipo que no está en la ranura de arma o arma de proyectiles puede afectar tus
ataques. Para el combate cuerpo a cuerpo, los ajustes para golpear, los ajustes
para dañar, el daño elemental extra (llamado "marcas") y el daño extra
debido al tipo de monstruo objetivo (llamado "matanzas") de ese equipo se
aplican y se pueden ver en la hoja de personaje o cuando se inspecciona el arma.
Cuando se lanza un arma o se dispara un proyectil, solo los ajustes para golpear
(y, cuando la :ref:`opción para-dañar es un porcentaje de los dados
<damage-percent-of-dice-option>` está activada, los ajustes para dañar) se
aplican de ese equipo.

.. index::
   single: clase de armadura; efecto en los ataques

Tu Clase de Armadura
================

Tu clase de armadura (o CA) es un número que describe la cantidad y la
calidad de la armadura que se lleva puesta. La clase de armadura generalmente irá de aproximadamente 0 a
200, aunque una armadura excepcionalmente buena puede mejorar incluso esta última cifra.

Cuanto mayor sea tu clase de armadura, más protectora es. Una clase de armadura negativa
en realidad ayudaría a que te golpeen. La armadura te protege de tres maneras.
Primero, hace que sea más difícil que te golpeen para causarte daño. Un golpe sin daño cuenta
como un fallo y se describe como un fallo. Segundo, una buena armadura absorberá
parte del daño que tu personaje habría sufrido por ataques
normales. Tercero, el daño por ácido se reduce usando armadura corporal (pero la
armadura puede ser dañada en su lugar). Es obvio que una clase de armadura alta es
vital para sobrevivir en los niveles más profundos de Angband.

.. index::
   single: clase de armadura; valores de equipo
   single: para-golpear; valores de armadura

Los valores de clase de armadura siempre se muestran entre un conjunto de corchetes,
como '[#]' o '[#,+#]'. El primer valor es la clase de armadura base de la
armadura. El segundo número es la bonificación mágica del objeto, que solo se
muestra si se conoce, y siempre tendrá un signo antes del valor. Estos
pluses pueden determinarse empuñando la armadura en combate y siendo golpeado.
Ten en cuenta que algunos anillos, amuletos y armas también tienen la notación '[+#]',
lo que indica que proporcionan una bonificación de armadura. Muchas piezas de armadura
corporal pesada también tendrán un '(-#)' (entre paréntesis normales) antes del
'[#,+#]', lo que indica que el peso de la armadura disminuye tus
posibilidades de golpear a los monstruos. Esto puede ir desde inexistente para armaduras muy
ligeras hasta '(-8)' para las armaduras más pesadas.

Efectos de estado de monstruos
======================

Encontrarás algunos hechizos y objetos que pueden afectar a los monstruos de formas que
no implican causarles daño directamente. Estos son 'efectos de estado'.
Se enumeran con sus efectos a continuación. Estos efectos de estado
funcionarán en un tipo de monstruo o no; algunos monstruos resisten efectos particulares
pero no todos. Si el mismo atributo es afectado por más de uno de estos
efectos de estado, los efectos sobre ese atributo no se acumulan, a menos que se indique
lo contrario.

Sujetar Monstruo:
  Paraliza a un monstruo hasta que lo golpees o hasta que termine la duración.
  Aumenta la probabilidad de que el jugador obtenga un golpe crítico.
  Duración normal de 3-8 turnos.

Aturdir Monstruo:
  Reduce la precisión y el daño cuerpo a cuerpo del monstruo en un 25%.
  1 de cada 10 posibilidades de que el monstruo pierda el turno.
  Aumenta la probabilidad de que el jugador obtenga un golpe crítico.
  Duración normal de 5-10 turnos.

Confundir Monstruo:
  Los hechizos del monstruo fallan un 50% más a menudo; esto se acumula con los fallos
  aumentados por estar asustado.
  El monstruo tiene al menos un 40% más de probabilidades de fallar el objetivo con hechizos/ataques a distancia.
  Los hechizos de bola, rayo y aliento del monstruo a veces van en la dirección equivocada.
  30% de probabilidad de movimiento errático, más cuando está más confundido.
  Si un movimiento errático intenta atravesar una pared que el monstruo no puede
  atravesar normalmente, el monstruo puede quedar ligeramente aturdido.
  Aumenta la probabilidad de que el jugador obtenga un golpe crítico.
  Duración normal de 5-10 turnos.

Ralentizar Monstruo:
  -2 de velocidad, más si está más ralentizado.
  Duración normal de 10 o más turnos.

Dormir Monstruo:
  Pone a dormir a los monstruos, pero pueden despertarse con bastante facilidad.

Asustar Monstruo:
  El monstruo huirá.
  Los hechizos del monstruo fallan un 20% más a menudo; esto se acumula con los fallos
  aumentados por confusión o desencantamiento.
  Aumenta la probabilidad de que el jugador obtenga un golpe crítico.

Desencantar Monstruo:
  Los hechizos del monstruo fallan un 50% más a menudo; esto se acumula con los fallos
  aumentados por estar asustado.
  Duración normal de 5-10 turnos.


Ataques no cuerpo a cuerpo y resistencias
=================================

El jugador puede en algún momento obtener acceso a ataques no cuerpo a cuerpo, y muchos
monstruos también los tienen. Quizás el más famoso de este tipo de ataque es
el aliento de dragón, pero los monstruos también pueden lanzar hechizos al jugador, y viceversa.
Este daño generalmente no se ve afectado por la clase de armadura y no
necesita una tirada de golpe para golpear al jugador o al monstruo al que se apunta.

Algunos ataques son puramente mágicos: hechizos de ataque que ciegan, confunden, ralentizan,
asustan o paralizan al objetivo. Estos ataques son resistidos por monstruos de
nivel superior (nativos de profundidades de mazmorra más profundas) y personajes con una alta
tirada de salvación: las tiradas de salvación dependen de la clase, el nivel y la sabiduría.
También hay resistencias disponibles para el miedo, la ceguera, la confusión y
el aturdimiento, y el poder de "acción libre" previene la parálisis mágica (el
jugador aún puede ser paralizado por ser "noqueado" en combate cuerpo a cuerpo o por un ataque
de aturdimiento, pero esto es muy raro y se puede prevenir con protección contra
el aturdimiento; la parálisis por desmayo debido al hambre o lanzar un hechizo con
maná insuficiente tampoco se ve afectada por tener acción libre). Hay
monstruos que pueden causar efectos de estado como ceguera, parálisis o
confusión a través de su ataque cuerpo a cuerpo. Dado que esto es un efecto físico y
no mental, el jugador no obtendrá una tirada de salvación. Sin embargo, tener
resistencia a ese efecto evitará el estado negativo en todos los casos.
También se debe tener en cuenta que la mayoría de los monstruos únicos pasan automáticamente sus
tiradas de salvación, y algunos monstruos son naturalmente resistentes a la confusión, el miedo y
el sueño. Algunos monstruos pueden tener hechizos que 'causan heridas' que pueden ser
mortales si tienen éxito, pero no causan daño si se pasa la tirada de salvación.
Algunos ataques cuerpo a cuerpo de monstruos pueden drenar una estadística, al igual que algunas trampas: esto se
previene teniendo esa estadística sostenida. Las estadísticas drenadas son temporales y
se pueden restaurar al ganar un nuevo nivel de personaje o consumiendo objetos raros
que se encuentran en la mazmorra.

Algunos monstruos pueden lanzar hechizos que teletransportan al personaje del jugador. No
hay tirada de salvación, excepto para aquellos que realmente lo teletransportarían hacia arriba o
abajo un nivel de mazmorra. Tener resistencia a nexo también evitará ser
teletransportado de nivel, pero no ayudará contra los ataques de hechizos de teletransporte
normales. El jugador puede teletransportar monstruos de la misma manera, con un hechizo,
varita o vara. Ningún monstruo, ni siquiera el propio Morgoth, puede resistir este
teletransporte. Sin embargo...

Otros ataques suelen estar basados en elementos, incluido el mencionado
ejemplo de aliento de dragón. Muchos monstruos pueden respirar varios ataques o lanzar
hechizos de rayo o bola, y el jugador también puede tener acceso a hechizos de rayo y bola
(o respirar como un dragón, en algunas circunstancias raras). El jugador
y los monstruos pueden ser resistentes a estas formas de ataque: la resistencia se
maneja de manera diferente para el jugador y el monstruo, y para diferentes formas
de ataque.

Los hechizos de rayo golpearán al primer monstruo (o al jugador) en la línea de fuego:
los hechizos de bola pueden centrarse en un objetivo que puede estar escondido detrás de
otros objetivos. Los hechizos de bola y las armas de aliento afectan un área: otros
monstruos atrapados en la explosión sufren daño reducido dependiendo de su distancia
del centro de la explosión. Las armas de aliento son proporcionales a una
fracción de los puntos de vida actuales del monstruo y disminuyen en poder con
la distancia del monstruo, con un límite máximo en el
daño (que es más alto para los ataques más comunes de este tipo, debido al
hecho de que las resistencias también son más fáciles de encontrar). El daño de los hechizos de rayo y bola
se calcula de manera diferente, a menudo (pero no siempre) en relación con el nivel del personaje
o del monstruo.

En el caso de fuego, frío, relámpago, ácido y veneno, si el monstruo tiene
resistencia a un ataque del jugador de este tipo, casi no sufrirá daño.
Si el jugador tiene una o más fuentes permanentes de resistencia, sufrirá
1/3 del daño que normalmente sufriría: si el jugador tiene una fuente temporal
de resistencia (ya sea de poción, hechizo o activación de objeto), esto
también reducirá el daño a 1/3 de su nivel normal, lo que permitirá al
personaje sufrir solo 1/9 del daño si tiene resistencia tanto permanente como
temporal. Tener más de una fuente de resistencia permanente no confiere ninguna
bonificación extra, y usar más de una fuente de resistencia temporal
aumenta solo la duración de la resistencia: en ambos casos, ya sea que la
resistencia esté presente o no. Pero una resistencia permanente y una
temporal son efectivas simultáneamente.

Los ataques elementales también tienen la posibilidad de dañar el equipo empuñado o destruir
objetos en el inventario del personaje. Los ataques de fuego destruyen pergaminos, bastones,
libros de magia y flechas. Los ataques de ácido destruyen pergaminos, bastones, flechas, pernos
y pueden dañar la armadura. Los ataques de electricidad pueden destruir varitas, varas, anillos
y amuletos. Los ataques de frío pueden destruir pociones. Los objetos en tu inventario
tienen una tirada de salvación y no sufren daño si la pasan. Tener resistencia al
elemento hará que un objeto sea menos probable de ser destruido. Los objetos en el
suelo que quedan atrapados en una bola o aliento elemental se destruyen
automáticamente sin una tirada de salvación. Las armas, armaduras y cofres también pueden ser
destruidos si están en el suelo, pero no pueden ser dañados si están
en tu mochila.

El personaje también puede obtener inmunidad al fuego, frío, relámpago y ácido si
tiene la suerte de encontrar alguno de los pocos artefactos que proporcionan estas
inmunidades: la inmunidad significa que no se sufre daño y el equipo del
personaje también está totalmente protegido. Las inmunidades son EXTREMADAMENTE raras.

Algunos objetos usados hacen que el personaje sea vulnerable al fuego, frío, relámpago
o ácido. A menos que el personaje también use un objeto que otorgue inmunidad
al mismo elemento, la vulnerabilidad significa que el personaje sufre
más daño de ese elemento: 1/3 más si el personaje no tiene
una fuente permanente de resistencia al elemento y daño normal si el
personaje tiene una fuente permanente de resistencia al elemento. Además,
un personaje no puede obtener una resistencia temporal a un elemento cuando está presente
una vulnerabilidad a ese elemento.

.. index::
   single: puntos de experiencia; ataques de drenaje

Otro ataque con el que el jugador entrará en contacto con demasiada frecuencia es
la naturaleza escalofriante de los no muertos, que puede drenar la experiencia de vida
del personaje. Algunos monstruos tienen un ataque cuerpo a cuerpo que drena la vida, otros
pueden lanzar hechizos de bola o rayo o, en casos extremos, respirar la fuerza
misma del más allá (abreviado por el juego como "más allá"). Hay dos
poderes que ayudan en este caso: el de "mantener la vida" evitará
muchos drenajes de experiencia (la fracción afectada depende del monstruo,
y puede ser del 95%, 90%, 75% o 50%), y si ocurre el drenaje, reduce la cantidad
de experiencia perdida en un 90%. El de "resistencia a las fuerzas del más allá" proporcionará
resistencia a rayos, bolas y alientos del más allá, reduciendo el daño y
evitando cualquier drenaje de experiencia de esos ataques, pero no tiene efecto en
los "golpes para drenar experiencia" cuerpo a cuerpo. Los monstruos atrapados en la explosión de una bola
o aliento del más allá sufrirán daño proporcional a la distancia desde el centro del
ataque, excepto los no muertos que son totalmente inmunes. El jugador puede encontrar
varitas o varas de Drenar Vida, que son igualmente ineficaces en aquellas criaturas no
muertas que no tienen vida que drenar: sin embargo, el verdadero hechizo de ataque equivalente
del jugador es el hechizo de sacerdote/paladín "Orbe de Drenaje", un hechizo de bola
que hace daño a todos los monstruos, doble daño a los monstruos malvados y es
resistido por ninguno.

Otras formas de ataque son más raras, pero pueden incluir: desencantamiento (tanto en
cuerpo a cuerpo como por aliento de monstruo), caos (aliento o cuerpo a cuerpo, que si no se resiste
hará que el jugador alucine y se confunda, y puede drenar experiencia de vida),
nexo (que puede teletransportar al jugador hacia el monstruo, lejos del
monstruo, hacia arriba o abajo un nivel, o intercambiar dos de las estadísticas
"internas" del jugador), luz y oscuridad (que cegarán a un personaje a menos
que tenga protección contra la ceguera o resistencia a la luz o la oscuridad),
sonido (que aturdirá a un personaje sin resistencia al sonido o protección contra
el aturdimiento), fragmentos de cristal (que cortarán a un personaje no resistente),
inercia (que ralentizará a un personaje independientemente de la acción libre),
gravedad (que parpadeará a un personaje, también aturdiendo y ralentizando),
fuerza (que aturdirá al personaje), plasma (que aturdirá), tiempo (que puede
drenar experiencia independientemente de mantener la vida, o drenar estadísticas independientemente de
los sostenimientos), rayos y bolas de agua (que pueden confundir y aturdir, y hacen
un daño considerable de monstruos de alto nivel), rayos de hielo (que pueden
cortar y aturdir, y dañar pociones) y rayos y bolas de maná (estos últimos
generalmente conocidos como Tormentas de Maná). Los proyectiles mágicos están incluidos en la categoría
"maná", ya sean lanzados por el monstruo o por el jugador.

Además, los objetos en el suelo son especialmente vulnerables a los efectos elementales.
Las pociones en el suelo siempre serán destruidas por el frío, los fragmentos,
el sonido y la fuerza. Los pergaminos, bastones, libros y equipos no metálicos siempre
serán destruidos por el fuego o el plasma. Los pergaminos, bastones y todo el equipo no
de mithril serán destruidos por el ácido. Los anillos, amuletos, varitas y varas serán
destruidos por el relámpago y el plasma. Y finalmente, casi todo será
destruido por una tormenta de maná si se deja en el suelo.

Algunos ataques pueden aturdir o cortar al jugador. Estos pueden ser hechizos o
ataques de aliento (sonido, bolas de agua) o de combate cuerpo a cuerpo. Un personaje aturdido
recibe una penalización para golpear y es mucho más probable que falle un hechizo o
activación. Si un personaje se aturde mucho, puede ser noqueado y
quedar a merced de los enemigos. Un personaje cortado perderá vida lentamente hasta
ser curado ya sea por pociones, hechizos o regeneración natural. Tanto el estado de aturdimiento
como el de corte se muestran en la parte inferior de la pantalla.

Hay resistencias disponibles para caos, desencantamiento, confusión, nexo,
sonido, fragmentos, luz y oscuridad: todas ellas reducirán el daño y
evitarán los efectos secundarios que no sean el daño físico. Con estas resistencias, al
igual que con la resistencia al más allá, el daño es una fracción aleatoria entre 1/2 y 2/3.

Se debe tener en cuenta que no todas estas son realmente vitales para completar
el juego: de hecho, de la lista anterior, solo las resistencias a fuego, frío, ácido,
relámpago, veneno y confusión se consideran realmente vitales, siendo las siguientes más deseables
ceguera, caos y más allá. Algunas formas de ataque no son resistibles, pero afortunadamente estas son raras:
resistir fragmentos evitará todos los demás ataques mágicos que cortan (a saber, rayos de hielo), y la resistencia a la confusión
evitará la confusión por un rayo o bola de agua, pero no hay resistencia
al daño físico causado por los siguientes ataques: inercia, fuerza,
gravedad, plasma, tiempo, hielo, agua, maná. No hay resistencia a ninguno de
los efectos secundarios de un ataque de tiempo, ni de hecho a nada más que los efectos
de aturdimiento de un ataque de gravedad.

Una nota sobre la velocidad
===============

Los monstruos que no se mueven a velocidad normal generalmente se mueven "lentamente" (-10 a
velocidad), "bastante rápido" (+5), "rápido" (+10), "muy rápido" (+20) o
"increíblemente rápido" (+30). (No sorprenderá a nadie que Morgoth sea uno de los
pocos monstruos en la última categoría). Esto se ajusta aún más por el hecho
de que cualquier monstruo no único puede tener un ajuste aleatorio de (-2) a (+2)
a su propia velocidad.

En general, (+10) es exactamente el doble de la velocidad normal, y (-10) exactamente la mitad.
(+20) es aproximadamente tres veces la velocidad normal, pero después de eso hay menos
mejora notable a medida que la velocidad aumenta, por ejemplo, (+30) no es
casi cuatro veces la velocidad normal, y los valores más altos que esto son en gran medida
irrelevantes. El jugador puede encontrar objetos que se pueden usar o empuñar que
proporcionan bonificaciones de velocidad: estos pueden incluir botas de velocidad, anillos de velocidad y
unos pocos artefactos muy raros. Las botas proporcionarán un 1d10 aleatorio de velocidad: los anillos
de velocidad pueden ser más grandes que eso, generalmente lo mejor que el jugador
obtendrá es dos poco más de (+10), pero se han conocido anillos individuales de hasta (+23) de velocidad.

Separado de la cuestión de la velocidad permanente (determinada por los
objetos de velocidad del jugador y la velocidad natural del monstruo) está la de la velocidad
temporal. El jugador puede lanzar un hechizo de auto-apresuramiento, o usar una poción, bastón o
vara de velocidad o usar una activación de artefacto para apresurarse temporalmente: o un
monstruo puede lanzar un hechizo de auto-apresuramiento, o ser afectado por otro monstruo
"gritando pidiendo ayuda" o el jugador leyendo un pergamino de agravación de monstruo.
En todos los casos, se añade (+10) de velocidad temporalmente al monstruo o
jugador afectado. Usar dos o más fuentes de velocidad temporal es acumulativo solo en
duración: no se puede pasar de velocidad normal a (+20) usando una poción y un
hechizo de velocidad. Los hechizos de ralentización temporal (incluyendo monstruos que respiran
inercia o gravedad) se manejan de la misma manera, con exactamente (-10) siendo
restado del jugador o monstruo temporalmente, durante la duración del hechizo o efecto
de aliento.

Armas y armaduras de ego
=====================

Algunas de las armas de ego que podrías encontrar en la mazmorra se enumeran
a continuación. Esto te dará una pequeña muestra de los objetos que se pueden encontrar.
Sin embargo, si deseas descubrir estos objetos por tu cuenta, es posible que no desees
continuar. Las armas de ego se denotan por los siguientes "nombres":

Armas de ego cuerpo a cuerpo:
------------------
(Defensor)
  Un arma mágica que en realidad ayuda al portador a defenderse, aumentando
  así su clase de armadura y protegiéndolo contra el daño
  del fuego, frío, ácido, relámpago y caídas. Esta arma también
  aumentará tu sigilo, te permitirá ver criaturas invisibles, te protegerá de
  la parálisis y te ayudará a regenerar puntos de vida y maná más rápido. Como
  resultado de la capacidad de regeneración, usarás comida algo más rápido
  de lo normal mientras empuñas un arma de este tipo. Estas poderosas armas también
  sostendrán una estadística, aunque esta estadística variará de un arma a otra.

(Vengador Santo)
  Un Vengador Santo es a menudo una de las armas más poderosas. Un Vengador Santo
  aumentará tu sabiduría y tu clase de armadura. Esta arma hará
  daño extra cuando se use contra criaturas malvadas, demoníacas y no muertas, y
  también te dará protección contra el miedo y la capacidad de ver criaturas invisibles.
  Estas armas son básicamente versiones extremadamente poderosas de las Hojas Bendecidas
  y otorgan bonificaciones cuerpo a cuerpo a los sacerdotes y paladines. Estas
  armas, como las armas (Defensor), también sostendrán una estadística aleatoria.

(Bendecida)
  Una hoja bendecida aumentará tu sabiduría. Si eres un sacerdote o paladín,
  empuñar una te otorga bonificaciones para el combate cuerpo a cuerpo. Las hojas bendecidas también tienen
  un poder adicional aleatorio.

Arma de Westernesse
  Un Arma de Westernesse es una de las armas más poderosas. Hace
  daño extra contra orcos, trolls y gigantes, mientras aumenta tu
  fuerza, destreza y constitución. También te permite ver criaturas invisibles
  y te protege de la parálisis. Estas hojas fueron hechas por los
  Dúnedain.

Arma de Ataques Extra
  Un arma de ataques extra permitirá al portador realizar ataques adicionales
  durante cada ronda.

Armas Marcadas Elementales
  Cada uno de los cinco ataques elementales tiene un arma correspondiente que
  hará el triple de su daño base a las criaturas no resistentes a ese elemento y
  otorgará al portador resistencia a ese elemento. (Se debe tener en cuenta que la
  bonificación de daño mágico no se ve afectada por esto: un arma de Llama '(2d6)
  (+5,+6)' hace 6d6+6 de daño por golpe, no 6d6+18, contra criaturas que no
  son resistentes al fuego.) Hay armas de Llama, Escarcha, Relámpago, Ácido y
  Marcas de Veneno.

Armas de Matar enemigos
  Estas armas hacen daño extra contra criaturas de un tipo vulnerable.
  Las armas de Matar Malvados y Matar Animales hacen el doble del daño base, mientras
  que las armas de Matar Orcos, Trolls, Gigantes, Dragones, Demonios y No Muertos hacen el triple del
  daño base. Al igual que con las armas marcadas elementales, la bonificación de daño mágico
  no se ve afectada.

Armas de |*Matar*| enemigos
  Estas armas, además de hacer daño extra a tus enemigos, tienen
  poderes adicionales también. En cada caso, una estadística se aumenta. Las armas
  de |*Matar*| Dragón, Demonio o No Muerto también son más poderosas contra sus
  oponentes, haciendo cinco veces su daño base en lugar de los tres
  normales.

Palas, Picos y Azadas de Excavar
  Estos poderosos excavadores excavarán el granito como si fuera madera,
  y las vetas minerales como si fueran mantequilla. La roca permanente sigue siendo un
  obstáculo infranqueable. Todos están marcados con ácido y triplican su daño
  base contra las criaturas que no son resistentes al ácido.

Lanzadores de proyectiles y munición de ego:
-------------------------------
Lanzadores de Precisión
  Estos lanzadores tienen un número para-golpear antinaturalmente alto, lo que los hace
  extremadamente precisos.

Lanzadores de Poder
  Estos lanzadores hacen una cantidad de daño antinaturalmente alta debido a su alto
  número para-dañar.

Lanzadores de Disparos Extra
  Estos lanzadores permiten al portador disparar más veces por ronda de lo
  normal.

Lanzadores de Poderío Extra
  Estos lanzadores tienen un daño base más alto que los lanzadores fabricados normalmente de
  su tipo. Por ejemplo, un 'Arco Largo de Poderío Extra (x3)(+X,+Y)(+1)'
  es realmente un Arco Largo '(x4)(+X,+Y)' donde '(+X,+Y)' es el estándar
  para-golpear y para-dañar. Como el multiplicador de daño con el arco afecta
  **todo**: el daño base de la flecha, la bonificación de daño mágico tanto en
  el arco como en la flecha, y cualquier bonificación por flechas de matanza o marcadas elementalmente,
  esto lo convierte en un arma poderosa.

Munición de Heridas
  Esta munición, ya sean guijarros, perdigones de hierro, flechas, pernos,
  flechas buscadoras o pernos buscadores, tiene grandes bonificaciones para-golpear y para-dañar.

Munición de Marcas Elementales y Munición de Matar enemigos
  Esto funciona de la misma manera que las armas cuerpo a cuerpo del mismo tipo: doble
  daño para matar malvados y matar animales, triple daño para todas las demás matanzas
  y para todas las marcas elementales. A diferencia de las armas cuerpo a cuerpo, las matanzas y
  las marcas elementales **sí** afectan la bonificación de daño mágico para la munición.

Estos son los tipos más comunes de armas de ego: ten en cuenta que no son los
ÚNICOS objetos de ego disponibles en la mazmorra, puede haber más.

Aparte de estos, hay algunas armas muy raras y bien hechas en la
mazmorra que no necesariamente tienen habilidades especiales. Estas incluyen Hojas
de Caos, Mazas de Disrupción y Guadañas de Corte. También pueden ser
armas de ego como las anteriores. Por ejemplo, una Hoja de Caos (Vengador
Santo) es mucho más poderosa que muchas armas de artefacto.

Algunas piezas de armadura poseerán habilidades especiales denotadas por los siguientes
nombres:

Armaduras y Escudos de ego:
-----------------------
de Resistir Ácido, Relámpago, Fuego o Frío
  Un personaje que use una armadura o escudo con una de esas resistencias sufrirá
  solo 1/3 del daño normal de los ataques que involucren el elemento relevante de
  ácido, relámpago, fuego o frío. Ten en cuenta que las fuentes permanentes múltiples de
  resistencia NO son acumulativas: usar dos no es mejor que usar una.
  Sin embargo, la armadura que proporciona resistencia al ácido no puede ser dañada por
  el ácido, y esta es una buena razón para usar más de una pieza de este tipo de
  armadura.

de Resistencia
  Un personaje que use una armadura con esta capacidad tendrá resistencia al Ácido,
  Frío, Fuego y Relámpago como se explica en cada parte anterior.

Armadura de Élfica
  Es lo mismo que la armadura de Resistencia, solo que generalmente mejor encantada. Te
  hará más sigiloso. Esta armadura también posee una resistencia
  extra, aleatoria de la siguiente lista: veneno, luz, oscuridad,
  nexo, más allá, caos, desencantamiento, sonido y fragmentos.

Túnicas de Permanencia
  Estas túnicas están diseñadas especialmente para magos. Al igual que la armadura
  Élfica, proporcionan resistencia al fuego, frío, ácido y electricidad y
  no pueden ser dañadas por el ácido. Sostienen todas tus estadísticas y te protegen
  de una buena cantidad de todo drenaje de experiencia. También como la armadura Élfica,
  tienen una resistencia aleatoria.

Mallas de Escamas de Dragón
  Estas piezas de armadura extremadamente raras vienen en muchos colores diferentes, cada una
  protegiéndote contra los dragones relevantes. Naturalmente, todas son
  resistentes al daño por ácido. También ocasionalmente te permiten respirar como
  un dragón. Las Mallas de Escamas de Dragón también pueden tener egos.

Yelmos de ego:
----------
Yelmos de Aumento de Estadísticas
  Hay yelmos mágicos que se encuentran en la mazmorra que tienen la capacidad de
  aumentar la inteligencia o la sabiduría del usuario. Además de aumentar la
  estadística relevante, estos yelmos también evitarán que esa estadística sea drenada.

Corona del Mago
  Esta es la gran corona de los magos. El usuario tendrá una inteligencia
  aumentada (y sostenida), y también se le otorgará resistencia contra
  fuego, escarcha, ácido y relámpago. Estos valiosos yelmos también tienen
  un poder aleatorio adicional.

Corona del Poderío
  Esta es la corona de los guerreros. El usuario tendrá una fuerza,
  destreza y constitución aumentadas y sostenidas, y también será inmune
  a cualquier intento de un enemigo de paralizarlo.

Corona del Señorío
  Esta es la gran corona de los sacerdotes. El usuario tendrá una sabiduría
  aumentada y sostenida y estará protegido contra el miedo. Estos yelmos también tienen
  un poder aleatorio adicional.

Yelmo/Corona de Visión
  Este es el gran yelmo o corona de los pícaros. El usuario podrá ver
  criaturas invisibles y tendrá una mayor capacidad para localizar
  trampas. También se rumorea que el usuario de tal yelmo no podrá
  ser cegado.

Yelmo de Infravision
  Este yelmo permite al personaje ver monstruos incluso en la oscuridad total,
  con la capacidad de ver el calor. Ten en cuenta que los libros de hechizos están a la misma
  temperatura que el entorno, por lo que no se pueden leer a menos que haya algo de luz
  real presente.

Yelmo de Luz
  Además de proporcionar una fuente de luz permanente para el usuario, este
  yelmo también proporciona resistencia contra los ataques basados en la luz.

Yelmo/Corona de Telepatía
  Este yelmo o corona otorga al usuario el poder de la telepatía.

Yelmo de Regeneración
  Este yelmo te ayudará a regenerar puntos de vida y maná más rápidamente de lo
  normal, permitiéndote luchar más tiempo antes de necesitar descansar. Usarás
  comida más rápido de lo normal mientras usas este yelmo debido a los
  efectos regenerativos.

Capas de ego:
-----------
Capa de Protección
  Esta capa finamente hecha vendrá con un encantamiento antinaturalmente alto,
  no se ve afectada por ataques basados en elementos y proporciona resistencia contra
  fragmentos.

Capa de Sigilo
  Esta capa aumentará el sigilo del usuario, haciendo que el usuario sea menos
  probable de despertar a los monstruos dormidos.

Capa de Aman
  Estas capas excepcionalmente raras proporcionan un gran sigilo, tienen un
  encantamiento muy alto y una resistencia aleatoria.

Guantes de ego:
-----------
Guantes de Acción Libre
  El usuario de estos guantes será resistente a los ataques de parálisis.

Guantes de Matanza
  Estos guantes aumentarán la capacidad de lucha del usuario al aumentar
  los valores para-golpear y para-dañar del usuario.

Guantes de Agilidad
  Estos guantes aumentarán la destreza del usuario.

Guantes de Poder
  Estos guantes aumentarán la fuerza del usuario, así como los números para-golpear
  y para-dañar del usuario.

Botas de ego:
----------
Botas de Caída Lenta
  Estas botas protegen al usuario de los efectos de pequeñas caídas.

Botas de Sigilo
  Estas botas aumentan el sigilo del usuario, como una Capa de Sigilo.

Botas de Acción Libre
  El usuario de estas botas será resistente a los ataques de parálisis.

Botas de Velocidad
  El usuario de estas botas se volverá antinaturalmente rápido.

Una vez más, estos no son necesariamente los ÚNICOS objetos de ego en la mazmorra,
solo los más comunes.

Aparte de estos, hay algunas armaduras muy raras y bien hechas en la
mazmorra que no necesariamente tienen habilidades especiales. Estas incluyen la Malla de
Adamantita, la Malla de Mithril, la Cota de Malla de Mithril y las Capas Élficas. Las
primeras tres no pueden ser dañadas por el ácido debido a los metales de calidad que
contienen.

Hay rumores de objetos "artefacto" únicos en la mazmorra: armas y
armaduras de todo tipo. Muchos de estos son más poderosos que incluso los
objetos de ego más grandes: algunos son débiles y tienen poco más que un nombre para recomendarlos.

.. |*Slay*| unicode:: *Slay*
.. |*Slay*ing| unicode:: *Slay*ing
