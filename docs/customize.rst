====================
Personalizando el juego
====================

Angband te permite cambiar varios aspectos del juego para adaptarlo a tus gustos. Estos incluyen:

* Opciones - que te permiten cambiar el comportamiento de la interfaz o del juego
* :ref:`Ignorar objetos <ignoring>` y :ref:`inscribir objetos <inscribing>` para cambiar cómo el juego los trata
* `Mostrar información extra en subventanas`_
* `Mapas de teclas`_ - una forma de asignar acciones de uso común a teclas específicas
* `Visuales`_ - permitiéndote cambiar la apariencia de entidades del juego como objetos y monstruos
* `Colores`_ - permitiéndote hacer que un color dado sea más brillante, más oscuro o incluso completamente diferente
* :ref:`Detalles de la interfaz <interface-details>` - dependiendo de qué interfaz del juego uses, estos te dan control sobre la fuente, la colocación de ventanas y el conjunto de gráficos

Excepto por las opciones, que están vinculadas al archivo guardado, y los detalles
de la interfaz, que son manejados por el front-end en lugar del núcleo del juego,
puedes guardar tus preferencias para estos en archivos, que se llaman
`archivos de preferencias de usuario`. Para las opciones, personalízalas usando el comando ``=``
mientras juegas.


.. _user-pref-files:

Archivos de Preferencias de Usuario
===================================

Los archivos de preferencias de usuario son la forma que tiene Angband de guardar y cargar ciertas configuraciones.
Pueden almacenar:

* Apariencias visuales alteradas para las entidades del juego
* Inscripciones para aplicar automáticamente a los objetos
* Mapas de teclas
* Colores alterados
* Configuración de subventanas
* Colores para diferentes tipos de mensajes
* Qué archivos de audio reproducir para diferentes tipos de mensajes

Son archivos de texto simples con un formato fácil de modificar, y el juego tiene
un conjunto de archivos de preferencias preexistentes en la carpeta ``lib/customize/``. Se
recomienda que no los modifiques.

Varios elementos del menú de opciones (``=``) te permiten cargar archivos de preferencias de usuario existentes,
crear nuevos archivos de preferencias de usuario o guardar en un archivo de preferencias de usuario.

Dónde encontrarlos
~~~~~~~~~~~~~~~~~~

En macOS, puedes encontrarlos en tu directorio de usuario, en ``Documents/Angband/``.

En Linux, se almacenarán en ``~/.angband/Angband``.

En Windows puedes encontrarlos en ``lib/user/``.

¿Cómo se cargan?
~~~~~~~~~~~~~~~~

Cuando el juego se inicia, después de haber cargado o creado un personaje, se cargan automáticamente algunos archivos
de preferencias de usuario. Estos son los mencionados anteriormente en la
carpeta ``lib/customize/``, a saber ``pref.prf`` seguido de ``font.prf``. Si tienes
gráficos activados, entonces el juego también cargará algunas configuraciones de
``lib/tiles/``.

Después de que se completen estos, el juego intentará cargar (en orden):

* ``window.prf`` - cargado para todos los personajes
* *raza*.prf - donde *raza* es la raza de tu personaje, algo como
  ``Dwarf.prf``
* *clase*.prf - donde *clase* es la clase de tu personaje, algo como
  ``Paladin.prf``
* *nombre*.prf - donde *nombre* es el nombre de tu personaje, algo como
  ``Balin.prf``

Puedes guardar algunas configuraciones, por ejemplo, mapas de teclas, en el archivo ``Mage.prf``
si solo quieres que se carguen para magos.

También puedes ingresar comandos de preferencias de usuario individuales directamente, usando el comando especial "Introducir un
comando de preferencias de usuario", activado presionando ``"``.

Es posible que tengas que usar el comando de redibujar (``^r``) después de cambiar ciertos
aspectos del juego para permitir que Angband se adapte a tus cambios.

.. _ignoring:

Ignorar objetos
===============

Angband te permite ignorar objetos específicos que ya no quieres ver. Estos objetos se marcan como 'ignorados' y cualquier objeto similar se oculta de la vista. La forma más fácil de ignorar un objeto es con el comando ``k`` (o ``^d``); el objeto se suelta y luego se oculta de la vista. Cuando ignoras un objeto, se te dará la opción de ignorar solo ese objeto, o todos los objetos similares de alguna manera. Si accidentalmente ignoras un objeto o te encuentras en una situación donde quieres ver si un objeto previamente ignorado está disponible cerca, una forma de manejar eso es desactivar el ignorado para todos los objetos con el comando ``K`` (o ``O``), ir al objeto que quieres, designorarlo con el comando ``k`` (o ``^d``), y luego volver a activar el ignorado con el comando ``K`` (o ``O``). Cuando se ha desactivado el ignorado para todos los objetos, verás ``Designorando`` en la línea de estado en la parte inferior de la pantalla.

Todo el sistema de ignorado también se puede acceder desde el menú de opciones (``=``) eligiendo ``i`` para ``Configuración de ignorado de objetos``. Esto permite ver o cambiar la configuración de ignorado para objetos no equipables, y la configuración de calidad y de ego (descritas a continuación) para objetos equipables.

Hay una configuración de calidad para cada tipo de objeto equipable. Ignorar un objeto equipable te preguntará si deseas ignorar todos los objetos de ese tipo con una cierta configuración de calidad, o de un tipo de ego, o ambos.

Las configuraciones de calidad son:

malo
  El arma/armadura tiene CA, para-golpear o para-dañar negativos.

promedio
  El arma/armadura no tiene pluses ni minuses. No es mágico.

bueno
  El arma/armadura tiene CA, para-golpear o para-dañar positivos. Sin embargo, no tiene
  ninguna habilidad especial, marcas, matanzas, aumentos de estadísticas, resistencias

no-artefacto
  Esta configuración solo deja los artefactos sin ignorar.


.. _inscribing:

Inscribir objetos
=================

Las inscripciones son notas que puedes marcar en los objetos usando el comando ``{``. Puedes usar esto para darle comandos al juego sobre el objeto, que se enumeran a continuación. También puedes configurar el juego para inscribir automáticamente ciertos objetos cada vez que los encuentres, usando las pantallas de conocimiento de objetos, a las que se accede mediante ``~``.

Inscribir un objeto con '!!':
	Te alertará cuando el objeto haya terminado de recargarse.

Inscribir un objeto con '=g':
	Esto marca un objeto como 'siempre recoger'. Esto a veces es útil para
	recoger munición después de un tiroteo. Si hay un número
	inmediatamente después de la 'g', entonces la cantidad recogida automáticamente
	estará limitada. Si has inscrito un libro de hechizos con '=g4' y tienes
	cuatro o más copias en tu mochila, no recogerás automáticamente
	más copias cuando tengas la opción 'recoger si está en el inventario'
	activada. Si tienes tres copias en tu mochila con esa inscripción
	y encuentras una pila de dos copias, recogerás automáticamente
	una para que haya cuatro en la mochila.

Inscribir un objeto con ``!`` seguido de una letra de comando o ``*``:
	Esto significa "pregúntame antes de usar este objeto". '!w' significa 'pregúntame antes de
	empuñar', '!d' significa 'pregúntame antes de soltar', y así sucesivamente. Si
	inscribes un objeto con '!*', entonces el juego confirmará cualquier uso de un
	objeto.

	Digamos que inscribiste tu poción de Velocidad con '!q'. Esto te preguntaría
	cuando intentes beberla para ver si realmente quieres hacerlo. Múltiples
	inscripciones '!q' preguntarán múltiples veces.

	De manera similar, usar !v!k!d hace que sea muy difícil que accidentalmente
	lanzares, ignores o dejaras el objeto en el que está inscrito.

	Algunos aventureros inscriben sus Pergaminos de Palabra de Retorno con '!r'
	para no regresar a la mazmorra demasiado pronto.

Inscribir un objeto con ``@``, seguido de una letra de comando, seguido de 0-9:
	Normalmente, cuando seleccionas un objeto de tu inventario, debes ingresar la
	letra que corresponde al objeto. Dado que el orden de tu inventario
	cambia a medida que se añaden y eliminan objetos, esto puede ser molesto. Puedes
	asignar ciertos objetos números cuando usas un comando para que
	dondequiera que estén en tu mochila, puedas usar las mismas pulsaciones de teclas.
	Si tienes varios objetos inscritos con lo mismo, el juego usará
	el primero.

	Por ejemplo, si inscribes un bastón de Curar Heridas Leves con '\@u1',
	puedes referirte a él presionando 1 cuando lo ``u``ses. También podrías
	inscribir una varita de Maravilla con '\@a1', y cuando uses ``a``\, 1
	seleccionaría esa varita.

	Los lanzadores de hechizos deberían inscribir sus libros, para que si los pierden no
	lanzen el hechizo equivocado. Si eres mago y el libro de hechizos para principiantes
	es el primero en tu inventario, lanzar 'maa' lanzará proyectil
	mágico. Pero si pierdes tu libro de hechizos, lanzar 'maa' lanzará el
	primer hechizo en el nuevo libro que esté en la parte superior de tu inventario. Esto
	puede ser un desperdicio en el mejor de los casos y extremadamente peligroso en
	el peor. Al inscribir tus libros de hechizos con '\@m1', '\@m2', etc., si
	pierdes tu primer libro de hechizos e intentas lanzar proyectil mágico
	usando 'm1a', no puedes seleccionar accidentalmente el libro de hechizos equivocado.

	La inscripción '\@v' más número tiene el efecto secundario de colocar el
	objeto en la aljaba si el objeto es bueno para lanzar y la aljaba tiene
	espacio. Entonces, inscribir una lanza con '\@v0' intentará poner la
	lanza en la primera ranura de la aljaba. La munición va automáticamente a
	la aljaba cuando hay espacio, pero puedes inscribirla con '\@f' (para
	el conjunto de teclas original; usa '\@t' para el conjunto de teclas roguelike) más el
	número de ranura deseado, del 0 al 9, si quieres un orden diferente de
	los objetos en la aljaba del que el juego hace por sí mismo. Si
	las inscripciones asignan más de una pila a la misma ranura de la aljaba, la
	pila inscrita que ha estado en el inventario por más tiempo se
        colocará en la ranura.

Inscribir un objeto con ``^``, seguido de una letra de comando:
	Cuando usas un objeto inscrito con ``^``, el juego te pregunta antes de
	hacer esa acción. Podrías inscribir '^>' en un objeto si quieres que te
	recuerde quitártelo antes de bajar escaleras. Si el objeto está en
	tu mochila, el juego no te preguntará.

	Al igual que con ``!``, puedes usar ``*`` para la letra de comando si quieres que el
	juego te pregunte cada turno hagas lo que hagas. ¡Esto puede ser muy
	molesto!


.. _showing-extra-info-in-subwindows:

Mostrar información extra en subventanas
========================================

Además de la ventana principal, puedes crear ventanas adicionales que tengan
información secundaria en ellas. Puedes acceder al menú de subventanas usando
``=`` luego ``w``, donde puedes elegir qué mostrar en qué ventana. El
front-end que uses puede proporcionar una forma de configurar directamente las subventanas
sin usar ese menú de subventanas. Para front-ends que no proporcionan eso,
es posible que tengas que usar los controles del front-end para hacer visible la subventana
después de haber realizado cambios en el menú de subventanas. Mira la
:ref:`sección de Detalles de la interfaz <interface-details>` para ver si describe
el front-end que usas y cómo ese front-end maneja las subventanas.

Hay una variedad de opciones de subventanas y deberías experimentar para ver cuáles son las más útiles para ti.


.. _keymaps:

Mapas de teclas
===============

Puedes configurar mapas de teclas en Angband, que te permiten mapear una sola pulsación de tecla, el disparador, a una serie de pulsaciones de tecla, la acción. Por ejemplo, podrías mapear la tecla F1 a "maa" (las pulsaciones de tecla para lanzar "Proyectil Mágico" como lanzador de hechizos). Esto puede acelerar el acceso a funciones de uso común. Para omitir un mapa de teclas que se ha asignado a una tecla, presiona ``\`` antes de presionar la tecla.

Para configurar mapas de teclas, ve al menú de opciones (``=``) y selecciona "Editar mapas de teclas" (``e``). Allí, puedes verificar si una tecla activa un mapa de teclas: selecciona "Consultar un mapa de teclas" (``c``) y luego presiona la tecla para verificar. También puedes eliminar un mapa de teclas existente: selecciona "Eliminar un mapa de teclas" (``e``) y luego presiona la tecla que activa el mapa de teclas a eliminar. Para agregar un nuevo mapa de teclas (o sobrescribir uno existente), selecciona "Crear un mapa de teclas" (``d``), luego te pedirá la tecla que activa el mapa de teclas. Después de presionar la tecla de activación, se te pedirá la acción del mapa de teclas, la serie de pulsaciones de tecla que se generarán cuando se presione la tecla de activación. Si cometes un error al ingresar las pulsaciones de tecla para la acción, presiona ``Control-u`` para borrar las pulsaciones de tecla ya ingresadas para la acción. Una vez que hayas terminado de ingresar las pulsaciones de tecla para la acción, presiona ``=`` para finalizar la secuencia; luego se te preguntará si deseas mantener el mapa de teclas recién ingresado.

Dentro de la acción de un mapa de teclas, a menudo es útil suprimir temporalmente las indicaciones -más- ya que pueden tragar pulsaciones de tecla del mapa de teclas. Para deshabilitar esas indicaciones desde dentro de la acción, incluye ``(``. Para volver a habilitar las indicaciones, incluye ``)``. Entonces, una acción típica donde podrían ocurrir indicaciones -más- se vería así: ``(`` tus pulsaciones de tecla aquí ``)``.

Las pulsaciones de tecla en la acción se interpretarán en relación con el conjunto de teclas que estés usando actualmente (original o roguelike). El juego recordará qué conjunto de teclas estaba en efecto cuando se creó el mapa de teclas. Entonces, si cambias de conjunto de teclas, los mapas de teclas que solo se definieron para el otro conjunto de teclas no serán visibles. Puedes tener dos mapas de teclas, uno para el conjunto de teclas original y otro para el conjunto de teclas roguelike, vinculados al mismo disparador.

Los mapas de teclas no son recursivos. Si tienes F1 como el disparador de un mapa de teclas, incluir F1 como una pulsación de tecla en la acción de ese u otro mapa de teclas no invocará ese mapa de teclas.

Cualquier cambio que realices en los mapas de teclas desde el menú de opciones solo dura mientras el juego se está ejecutando. Para que afecten a sesiones futuras, guarda los mapas de teclas en un archivo. Hay una opción para hacerlo desde el menú de edición de mapas de teclas. Consulta `Archivos de Preferencias de Usuario`_ para saber cómo el nombre del archivo afecta si el archivo se carga cuando el juego recarga tu personaje.

Ten en cuenta que el juego tiene en cuenta las teclas modificadoras (Mayús, Control, Alt, Meta) que se presionan junto con una tecla. En la mayoría de las plataformas, el juego también distingue entre las teclas del teclado numérico que tienen equivalentes en el teclado principal. Cuando se muestra una pulsación de tecla o se guarda en el archivo de preferencias, los modificadores, si los hay, para la pulsación de tecla se muestran mediante letras de código (S para Mayús, ^ para Control, A para Alt, M para Meta y K para el teclado numérico) entre llaves antes de la pulsación de tecla. Hay dos excepciones a eso: si Control es el único modificador, se mostrará como ^ antes de la pulsación de tecla sin llaves, y si Mayús es el único modificador, a menudo se combinará en la propia pulsación de tecla. Por ejemplo::

	{^S}& = Control-Mayúsculas-&
	{AK}0 = Alt-0 del teclado numérico
	^d    = Control-d
	A     = Mayúsculas-a

Las teclas especiales, como F1, F2 o Tab, se escriben todas entre corchetes [].
Por ejemplo::

	^[F1]     = Control-F1
	{^S}[Tab] = Control-Mayúsculas-Tab

Las teclas especiales incluyen [Escape].

Puede que te resulte más fácil editar los archivos de preferencias directamente para cambiar un mapa de teclas. Los mapas de teclas se escriben en archivos de preferencias como::

	keymap-act:<acción>
	keymap-input:<tipo>:<disparador>

La acción siempre debe ir primero, ```<tipo>``` significa 'tipo de conjunto de teclas', que es 0 para el conjunto de teclas original o 1 para el conjunto de teclas roguelike. Por ejemplo::

	keymap-act:maa
	keymap-input:0:[F1]

Una acción puede tener más de un disparador vinculado a ella teniendo más de una línea keymap-input después de ella y antes de la siguiente línea keymap-act. Una razón para hacer eso sería tener el mapa de teclas funcionando con cualquier conjunto de teclas. Por ejemplo::

	keymap-act:maa
	keymap-input:0:[F1]
	keymap-input:1:[F1]

Angband usa algunos mapas de teclas integrados. Estos son para las teclas de movimiento (están mapeadas a ``;`` más el número, ej. ``5`` -> ``;5``), entre otros. Puedes ver la lista completa en pref.prf, pero no deberían afectarte de ninguna manera.

La acción de un mapa de teclas puede incluir múltiples comandos. Por ejemplo, un sacerdote que ha inscrito el primer libro de hechizos con '@m1' podría tener un mapa de teclas con la acción
'm1dm1f' para lanzar Bendecir y Heroísmo. Dichos mapas de teclas pueden abortar temprano sin completar los comandos restantes si:

* El jugador es :ref:`molestado <disturb-player>` antes del siguiente comando en el mapa de teclas.
* (nuevo desde 4.2.5-460-... en Vanilla) La siguiente tecla a procesar en el mapa de teclas no corresponde a un comando. Ten en cuenta que si la siguiente tecla es un espacio, ESCAPE o la tecla de alerta (ASCII 7), esas corresponden al comando NULL: no hacer nada, con éxito, sin requerir ninguna entrada.
* (nuevo desde 4.2.5-460-... en Vanilla) El siguiente comando tiene un requisito previo que no se cumple. Por ejemplo, disparar un proyectil requiere un lanzador equipado y usar un pergamino requiere poder leer (no ciego, no confundido, no sujeto a amnesia y la casilla actual está vista).
* (nuevo desde 4.2.5-460-... en Vanilla) Un escaneo de los objetos equipados encontró una inscripción que requiere una confirmación (ya sea '^*' para confirmar cualquier acción o '^' seguido de la tecla del siguiente comando) y esa confirmación fue denegada.

Desde 4.2.5-455-... en Vanilla, si la primera tecla en la acción de un mapa de teclas es ESCAPE, el disparador del mapa de teclas puede salir de un mensaje de dirección, un objetivo, un objeto, un hechizo, una maldición o un efecto. No saldrá de un mensaje de cadena (por ejemplo, al inscribir un objeto o agregar una nota) ni de mensajes de tecla (por ejemplo, indicaciones sí/no, el símbolo para desterrar monstruos, el símbolo para identificar con ``/``, o la tecla especificada para ``^`` para pasar junto con el modificador de control). El resto de la acción del mapa de teclas no se usará después de salir del mensaje. Como ejemplo, un mago tiene F1 como el disparador de la acción '[ESCAPE]m1a'. Si el juego está solicitando una dirección, ese mago puede presionar F1 para salir del mensaje, pero el hechizo Proyectil Mágico no se lanzará. El mago tendría que presionar F1 de nuevo para que se lance el hechizo.

Colores
=======

El submenú "Interactuar con colores" de opciones (``=``, luego ``c``) te permite cambiar cómo se muestran los diferentes colores. Dependiendo del tipo de computadora que tengas, esto puede o no tener ningún efecto.

La interfaz es bastante torpe. Puedes moverte a través de los colores usando ``n`` para 'siguiente color' y ``N`` para 'color anterior'. Luego, las letras mayúsculas y minúsculas ``r``, ``g`` y ``b`` te permitirán ajustar el color. Luego puedes guardar los resultados en un archivo de preferencias de usuario.


Visuales
========

Puedes cambiar cómo se muestran varias entidades del juego usando el editor visual. Este editor es parte de los menús de conocimiento (``~``). Cuando estás viendo una entidad particular, por ejemplo, un monstruo, si puedes editar sus visuales, se mencionará en el mensaje en la parte inferior de la pantalla.

Si estás en modo gráfico, podrás seleccionar un nuevo gráfico para la entidad. Si no lo estás, solo podrás cambiar sus colores.

Una vez que hayas realizado ediciones, puedes guardarlas desde el menú de opciones (``=``). Presiona ``v`` para 'guardar visuales' y elige lo que quieres guardar.

.. _interface-details:

Detalles de la interfaz
=======================

Algunos aspectos de cómo se presenta el juego, notablemente la fuente, la colocación de ventanas
y el conjunto de gráficos, son controlados por el front-end, en lugar del núcleo
del juego en sí. Cada front-end tiene su propio mecanismo para establecer esos
detalles y registrarlos entre sesiones de juego. A continuación se presentan breves descripciones
de lo que puedes configurar con los front-ends estándar de `Windows`_, `X11`_, `SDL`_,
`SDL2`_ y `Mac`_.

Windows
~~~~~~~

Con el front-end de Windows, el juego, por defecto, muestra varias de las
subventanas y usa los gráficos de David Gervais para mostrar el mapa.
Puedes cerrar una subventana con el control de cierre estándar en la
esquina superior derecha de la ventana. Cerrar la ventana principal con el control estándar hace
que el juego guarde su estado actual y luego salga. Puedes volver a abrir o también
cerrar una subventana a través del menú "Visibilidad", la primera entrada en el menú
"Ventana" para la ventana principal. Para mover una ventana, usa el procedimiento estándar:
coloca el puntero del mouse en la barra de título de la ventana y luego haz clic y arrastra
el mouse para cambiar la posición de la ventana. Haz clic y arrastra en los bordes o
esquinas de una ventana para cambiar su tamaño. Para seleccionar la fuente para una ventana, usa
el menú "Fuente", la segunda entrada en el menú "Ventana" para la ventana principal.

La entrada "Opciones de Terminal" en el menú "Ventana" para la ventana principal es un acceso directo
para acceder al método del núcleo del juego para seleccionar el contenido de las subventanas.
Puedes leer más sobre eso en `Mostrar información extra en subventanas`_. La
opción "Restablecer diseño" reorganizará las ventanas para que se ajusten al tamaño actual y
tendrá un resultado similar al que obtendrías al reiniciar la interfaz de Windows
sin una configuración preestablecida.

La entrada "Pantalla extraña" en el menú "Ventana" permite activar o desactivar
un algoritmo de visualización de texto alternativo para cada ventana. Eso se agregó para
compatibilidad con Windows Vista y posteriores. La configuración predeterminada, activado,
probablemente debería usarse, a menos que la visualización de texto esté distorsionada en tu sistema y la
configuración desactivada permita que el texto se muestre correctamente.

Las opciones "Aumentar ancho del gráfico" y "Disminuir ancho del gráfico" en el menú "Ventana",
te permiten incrementar o disminuir, en un píxel, el ancho de las columnas en una
ventana. Las opciones "Aumentar alto del gráfico" y "Disminuir alto del gráfico" son
similares pero funcionan con la altura de las filas. Para la ventana principal, podrías
usar la entrada "Tamaño de gráfico de la terminal 0" como una alternativa a esas para establecer
el ancho de las columnas y la altura de las filas a ciertas combinaciones o para
coincidir con el ancho y la altura de la fuente, que es el valor predeterminado. Cuando
la opción "Habilitar gráficos agradables" está activada (está en el menú "Opciones" para la ventana
principal), las entradas "Aumentar ancho del gráfico", "Disminuir ancho del gráfico",
"Aumentar alto del gráfico", "Disminuir alto del gráfico" y "Tamaño de gráfico de la terminal 0"
no tendrán efecto ya que el ancho de columna y la altura de fila se establecen
automáticamente cuando esa opción está activada.

Para cambiar si se usan gráficos, usa el menú "Gráficos", la primera
entrada en el menú "Opciones" para la ventana principal. La opción "Ninguno" en el
menú "Gráficos" deshabilitará los gráficos y usará texto para el mapa. La
siguiente sección en ese menú te permite seleccionar uno de los conjuntos de
gráficos. Activar la opción "Habilitar gráficos agradables" en el menú "Gráficos"
es un atajo para establecer automáticamente tamaños para obtener un resultado de aspecto razonable.
Cuando eso está activado o ya está activado y se cambia el conjunto de gráficos,
el ancho de las columnas ("ancho del gráfico"), la altura de las filas ("alto del gráfico")
y el número de filas y columnas utilizadas para mostrar un gráfico (el
"Multiplicador de gráfico") se ajustarán para funcionar bien con el tamaño de fuente actual y
el tamaño nativo de los gráficos. Puedes ajustar manualmente el número de
filas y columnas utilizadas para mostrar un gráfico con la entrada "Multiplicador de gráfico"
en el menú "Gráficos". Dado que las fuentes típicas a menudo tienen el doble de alto que de ancho,
los multiplicadores donde el primer valor, para el ancho, es el doble del segundo, a menudo
funcionan mejor con los gráficos que son nativamente cuadrados (los originales,
los de Adam Bolt, los de David Gervais y las dos versiones de los gráficos de Shockbolt).
Los gráficos de Nomad son de 8 x 16 y, por lo tanto, generalmente funcionan mejor con multiplicadores que usan el
mismo valor para ambas dimensiones.

Cuando sales del juego, la configuración actual para la interfaz de Windows se
guarda como ``angband.INI`` en el directorio que contiene el ejecutable. Esa
configuración se recargará automáticamente la próxima vez que inicies la interfaz de
Windows.

X11
~~~

Con el front-end X11, el número de ventanas abiertas se establece mediante la opción '-n'
en la línea de comandos, es decir, ejecutar ``./angband -mx11 -- -n4`` abrirá la
ventana principal y las subventanas uno a tres si el ejecutable está en el
directorio de trabajo actual. Para controlar la fuente, la ubicación y el tamaño utilizados para
cada una de las ventanas, establece variables de entorno antes de ejecutar Angband. Esas
variables de entorno para la ventana 'z', donde 'z' es un entero entre 0 (la
ventana principal) y 7 son:

* ANGBAND_X11_FONT_z contiene el nombre de la fuente a usar para la ventana
* ANGBAND_X11_AT_X_z contiene la coordenada horizontal (cero es el extremo izquierdo) para la esquina superior izquierda de la ventana
* ANGBAND_X11_AT_Y_z contiene la coordenada vertical (cero es el extremo superior) para la esquina superior izquierda de la ventana
* ANGBAND_X11_COLS_z contiene el número de columnas a mostrar en la ventana
* ANGBAND_X11_ROWS_z contiene el número de filas a mostrar en la ventana

SDL
~~~

Con el front-end SDL, la ventana principal y cualquier subventana se muestran dentro
de la ventana rectangular de la aplicación. En la parte superior de la ventana de la aplicación
hay una línea de estado. Dentro de esa línea de estado, los elementos resaltados en amarillo son
botones que se pueden presionar para iniciar una acción. De izquierda a derecha son:

* El número de versión de la aplicación: presionarlo muestra un cuadro de diálogo de información sobre la aplicación
* El terminal actualmente seleccionado: presionarlo muestra un menú para seleccionar el terminal actual; también puedes hacer que un terminal sea el actual haciendo clic en la barra de título del terminal si es visible
* Si el terminal actual es visible o no: presionarlo para cualquier terminal que no sea la ventana principal te permitirá mostrar u ocultar ese terminal
* La fuente para el terminal actual: presionarla muestra un menú para elegir la fuente para el terminal
* Opciones: muestra un cuadro de diálogo para seleccionar opciones globales, incluidas las del conjunto de gráficos utilizado y si el modo de pantalla completa está activado
* Salir: para guardar el juego y salir

Para mover una ventana de terminal, haz clic en su barra de título y luego arrastra el mouse.
Para cambiar el tamaño de una ventana de terminal, coloca el puntero del mouse sobre la esquina inferior derecha.
Eso debería hacer que aparezca un cuadrado azul, luego haz clic y arrastra para
cambiar el tamaño del terminal.

Para cambiar el conjunto de gráficos utilizado al mostrar el mapa del juego, presiona
el botón Opciones en la barra de estado. Luego, en el cuadro de diálogo que aparece, presiona
uno de los botones rojos que aparecen a la derecha de la etiqueta,
"Gráficos disponibles:". El último de esos botones, etiquetado "Ninguno", selecciona
texto como método para mostrar el mapa. Tu elección del conjunto de gráficos
no surte efecto hasta que presiones el botón rojo etiquetado "OK" en la
parte inferior del cuadro de diálogo.

Cuando sales del juego, la configuración actual para la interfaz SDL se guarda
como ``sdlinit.txt`` en el mismo directorio que se usa para los archivos de preferencias, consulta
`Archivos de Preferencias de Usuario`_ para más detalles. Esa configuración se recargará automáticamente
la próxima vez que inicies la interfaz SDL.

SDL2
~~~~

Con el front-end SDL2, la aplicación tiene una ventana que puede contener la
ventana principal y cualquiera de las subventanas. La aplicación también puede tener hasta
tres ventanas adicionales que pueden contener cualquiera de las subventanas. Una subventana
no puede aparecer en más de una de esas ventanas de la aplicación: añadir una
subventana a una ventana la elimina automáticamente de la otra ventana, si la hay,
que la tenía. Las partes no utilizadas de una ventana de la aplicación están embaldosadas con
repeticiones del logotipo del juego.

Cada una de las ventanas de la aplicación tiene una barra de menú en la parte superior. La entrada "Menú"
en el extremo izquierdo de la barra de menú tiene el menú principal para controlar
aspectos de la interfaz SDL2.

Junto a "Menú", hay una serie de etiquetas de una sola letra que actúan como conmutadores para las
ventanas de terminal que se muestran en la ventana de la aplicación. Haz clic en uno para cambiarlo
entre activado (dibujado con un rectángulo relleno al lado) y desactivado (dibujado con un
rectángulo vacío al lado). No es posible desactivar la ventana principal
que se muestra en la ventana de la aplicación principal.

Al final de la barra de menú hay dos botones de alternancia etiquetados "Mover" y "Tamaño".
Cuando el rectángulo junto a "Mover" está vacío, las posiciones de las subventanas
están fijas. Hacer clic en "Mover" en ese estado lo activará, desactivará
"Tamaño", deshabilitará la entrada al núcleo del juego y hará que los clics y arrastres dentro de
la pantalla cambien las posiciones de esas ventanas. Hacer clic en "Mover" cuando está
activado lo desactivará y restaurará el paso de entrada al núcleo del juego. Cuando el
rectángulo junto a "Tamaño" está vacío, el tamaño de las subventanas está fijo.
Hacer clic en "Tamaño" en ese estado lo activará, desactivará "Mover", deshabilitará la entrada
al núcleo del juego y hará que los clics y arrastres dentro de las ventanas mostradas
cambien los tamaños de esas subventanas. Hacer clic en "Tamaño" cuando está activado lo
desactivará y restaurará el paso de entrada al núcleo del juego.

Dentro de "Menú", las primeras entradas controlan las propiedades de cada una de las
ventanas de terminal mostradas dentro de esa ventana de la aplicación. Para la ventana principal,
puedes establecer la fuente, el conjunto de gráficos, si la ventana se muestra con bordes
o no, y si la ventana se mostrará sobre las otras ventanas.
Para las subventanas, puedes establecer la fuente, el propósito (que es un atajo para
habilitar el contenido de la subventana como se describe en
`Mostrar información extra en subventanas`_), la opacidad ("alfa") de la ventana,
si la ventana se muestra con bordes o no, y si la ventana se mostrará
sobre las otras ventanas.

Debajo de las entradas para las ventanas de terminal contenidas, hay una entrada,
"Pantalla completa" para alternar el modo de pantalla completa para esa ventana de la aplicación. Esa
entrada mostrará un rectángulo relleno al final de la entrada cuando el modo de pantalla completa
esté activado y un rectángulo vacío cuando el modo de pantalla completa esté desactivado.

En la ventana de la aplicación principal que contiene la ventana principal, hay una
entrada, "Enviar modificador de teclado numérico", después de eso, para si las pulsaciones de tecla del
teclado numérico se enviarán al juego con el modificador de teclado numérico activado. Esa
entrada tendrá un rectángulo vacío al final cuando el modificador no se envía
y un rectángulo relleno cuando el modificador se envía. Enviar el modificador permite
que algunos mapas de teclas predefinidos funcionen, por ejemplo, mayúsculas con 8 del teclado
numérico para correr hacia el norte, a costa de problemas de compatibilidad con algunos diseños
de teclado que difieren del diseño de teclado inglés estándar para el cual las teclas
normales tienen equivalentes en el teclado numérico.
https://github.com/angband/angband/issues/4522 tiene un ejemplo de los
problemas que se pueden evitar no enviando el modificador de teclado numérico.

Debajo de "Enviar modificador de teclado numérico" en el "Menú" de la ventana de la aplicación principal está
"Atajos de menú...". Eso te permite establecer una pulsación de tecla para transferir el control
al menú de una ventana. Por defecto, no se definen tales pulsaciones de tecla. Eso evita
conflictos potenciales con cualquier mapa de teclas que puedas tener. Mientras estás en los menús,
las pulsaciones de tecla se pueden usar para la navegación. Las teclas de movimiento horizontales y verticales
del juego funcionarán para moverse entre los controles, al igual que Tab (para ir al
control "siguiente") y Mayúsculas-Tab (para ir al control anterior). Enter
activará un elemento del menú si se puede activar. Intentar descender más profundamente
en los menús con las teclas de movimiento del juego también activará si un elemento del menú
está tan profundo como se puede llegar. Debajo de "Atajos de menú..." está "Ventanas": usa
eso para mostrar u ocultar una de las ventanas de aplicación adicionales.

Las tres últimas entradas en "Menú" son "Acerca de" para mostrar un cuadro de diálogo de información
sobre el juego, "Detalles de SDL" para mostrar un cuadro de diálogo de información
con detalles de diagnóstico sobre SDL2 y "Salir" para guardar el juego y salir.

Cuando sales del juego, la configuración actual para la interfaz SDL se guarda
como ``sdl2init.txt`` en el mismo directorio que se usa para los archivos de preferencias, consulta
`Archivos de Preferencias de Usuario`_ para más detalles. Esa configuración se recargará automáticamente
la próxima vez que inicies la interfaz SDL2.

Mac
~~~

Con el front-end específico de Mac, puedes usar los mecanismos estándar de Apple para
controlar la colocación de ventanas: haz clic y arrastra en la barra de título de una ventana para moverla,
haz clic y arrastra en el borde o esquina de una ventana para cambiar sus dimensiones,
y haz clic en el botón rojo en la esquina superior izquierda de una subventana para cerrarla.
Para volver a abrir una subventana que cerraste, usa el menú Ventana de la barra de menú de Mac
mientras el juego es la aplicación activa y selecciona la entrada cerca de la
parte inferior de ese menú que corresponde a la subventana que deseas ver. Para que la entrada de una
subventana esté habilitada en el menú Ventana, esa subventana debe estar configurada
para mostrar al menos una categoría de información: consulta
`Mostrar información extra en subventanas`_ para más detalles.

Para cambiar la fuente de una ventana, haz clic en la barra de título de la ventana y selecciona
"Editar fuente" en el menú Configuración de la barra de menú de Mac. Eso abrirá un
cuadro de diálogo que muestra la familia, tipo de letra y tamaño para la fuente actual.
Cambiar la selección de cualquiera de ellos cambiará la fuente en la ventana.

Si el mapa del juego se muestra como texto o como gráficos se puede configurar
seleccionando Configuración de la barra de menú de Mac mientras el juego es la aplicación
activa y luego eligiendo una de las entradas en la opción Gráficos.
Elegir "ASCII clásico" mostrará el mapa como texto. Cualquiera de las otras opciones
usará alguna forma de gráficos para mostrar el mapa. Si deseas
ajustar cómo se escalan los gráficos para que coincidan con la fuente seleccionada
actualmente en la ventana principal, usa el menú 'Tamaño de gráfico' en el menú Configuración. La
entrada 100% en el menú 'Tamaño de gráfico' hará que un gráfico se muestre lo más
cerca posible de su resolución nativa. La entrada 200% hará que un
gráfico se muestre lo más cerca posible del doble del ancho y alto nativos del gráfico.

Cuando sales del juego, la configuración específica de Mac actual se guarda y se
recargará automáticamente cuando reinicies. La configuración se almacena en
``Library/Preferences/org.rephial.angband.plist`` dentro de tu directorio de usuario.
Si sospechas que esa configuración se ha corrompido de alguna manera o deseas
comenzar de nuevo desde la configuración predeterminada, sal del juego si se está ejecutando, abre una
ventana de Terminal (es decir, selecciona 'Ir->Utilidades->Terminal' de los menús del Finder),
y, en esa ventana de Terminal, ejecuta esto::

	defaults delete org.rephial.angband

para borrar el contenido del archivo de preferencias y cualquier preferencia en caché que
pueda permanecer en la memoria.