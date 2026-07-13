[English](CONTRIBUTING.md) | [Español](CONTRIBUTING.es.md)
# Contribuir a Angband

Este documento es una guía para contribuir a Angband. Es en gran parte una recopilación de consejos previos de varios autores, actualizada según sea necesario.

## Ofrecer tu contribución a Angband

Cuando hayas corregido un error o implementado una nueva funcionalidad, por favor háznoslo saber. La forma preferida de hacerlo es enviar un pull request en Github.

### Conocimientos generales de git

git es un sistema de control de versiones diseñado para llevar el seguimiento del progreso de una base de código de software. Este consejo asume que estás usando git en la línea de comandos (una terminal en Linux o MacOS, o una herramienta como el git shell de Github para Windows).

Para crear una copia local del repositorio oficial de angband, usa `git clone git://github.com/angband/angband.git`. Pero si quieres participar en el desarrollo, es mejor no hacer esto de inmediato. En su lugar, crea una cuenta en [Github](http://github.com), ve al repositorio oficial angband/angband y haz clic en Fork. Esto creará un nuevo repositorio en `https://github.com/tulogin/angband`. Este es el que deberías clonar localmente usando `git clone git://github.com/tulogin/angband.git`.

Esto creará un repositorio local con varias ramas. Usa `git branch -a` para verlas:

* master
* origin/master
* (ramas de lanzamiento y otras cosas de las que no debes preocuparte)

NO hagas tu trabajo en tu rama master: esto es buscarse problemas. Crea una nueva rama usando `git checkout -b nuevarrama`, y trabaja ahí. Usa `git commit -a` para confirmar tus cambios en tu nueva rama y luego compílalos y pruébalos.

Una vez que hayas probado tus commits a tu satisfacción, puedes compartirlos. Suponiendo que hayas creado una nueva rama y hecho tus cambios como se describió anteriormente, puedes publicar tus cambios al mundo usando `git push origin nuevarrama` - esto hará que tu nueva rama aparezca en Github para que otros la prueben. (Es recomendable, pero no esencial, usar claves ssh para el acceso a Github.)

Mantén tu pensamiento claro: separa tu trabajo en diferentes ramas para diferentes cosas. Crea una rama llamada 'docs' si quieres trabajar en documentación. Crea una llamada 'stores' si quieres hacer cambios en las tiendas. Y así sucesivamente. No hay límite en la cantidad de ramas que puedes tener, y puedes usar `git checkout nombrerrama` para cambiar entre ramas en cualquier momento. (Idealmente deberías confirmar cualquier cambio en la rama actual antes de cambiar de rama, pero a git no le gusta que la gente deshaga cosas, así que lee sobre `git stash` si necesitas cambiar de rama y no quieres confirmar ni perder los cambios actuales.)

### Enviar un pull request

Para enviar un pull request, necesitarás tener una cuenta de Github y un repositorio "forkeado" del repositorio oficial de angband. Llamaremos al repositorio "tufork" y a la rama con la corrección de errores o nueva funcionalidad "turrama". Suponemos que has llamado a tu remoto del repositorio oficial "official".

1. Cuando hayas terminado y probado tu trabajo, publícalo en Github usando `git push origin turrama`. No olvides hacer rebase y solucionar cualquier conflicto antes de hacer push si es necesario (`git fetch official; git rebase official/master`) - incorporar tu trabajo es mucho más fácil si se aplica limpiamente a la rama official/master. Ten en cuenta que debes hacer rebase antes de hacer push, ya que rebase cambia los IDs de los commits - esto no importa en absoluto si el único lugar donde existen es en tu repositorio local.

   * Por favor, asegúrate de que turrama contenga solo los commits que quieres fusionar al repositorio oficial. Si no has mantenido tus ramas separadas y hay commits relacionados con cambios locales u otro trabajo en turrama, las cosas se complicarán. Para solucionar esto, crea una copia de master (`git checkout official/master; git checkout -b turrama2`) y haz cherry-pick de los commits que realmente quieres ofrecer (este método también se puede usar para evitar conflictos de rebase, o al menos aislar qué commits los están causando). Desde turrama2 puedes usar `git rebase -i turrama` como una especie de mecanismo de cherry-pick por lotes: te ofrece una lista de todos los commits que están en turrama pero no en master, y puedes elegir los que quieras añadir. Ten en cuenta que rebase dirá "nothing to do" si turrama se aplica limpiamente (es decir, fusión fast-forward) a master - así que esta es una buena prueba.

2. Ve a tu cuenta de Github en tu navegador y haz clic en tu fork. No olvides hacer clic en la pestaña Branches y asegurarte de que estás viendo la rama correcta (Github siempre mostrará por defecto la rama master). Ten en cuenta que si creaste turrama2 como se describió anteriormente, para ordenarla y ofrecer solo los commits correctos, entonces esta es la que necesitas mirar.

3. Haz clic en el botón "Pull Request", que está arriba a la derecha (justo debajo del cuadro de búsqueda). Si no obtienes una pantalla invitándote a escribir una descripción del pull request, algo salió mal (tal vez no estabas viendo la rama correcta, o tu push no funcionó).

4. Escribe una descripción de lo que estás ofreciendo en el pull request. Por favor incluye detalles de cualquier problema pendiente (por ejemplo, dependencia de otras tareas) o trabajo relacionado adicional que pretendas hacer. Además, por favor incluye el/los número(s) de issue (con un `#` delante) de cualquier [issue](https://github.com/angband/angband/issues) que la solicitud aborde (incluso si es solo parcialmente).

5. Haz clic en el botón "Send Pull Request".

#### Después de haber enviado el pull request

A menudo pensarás en alguna corrección o cambio importante después de haber enviado un pull request. No te preocupes - Github maneja esto de forma muy limpia. Simplemente confirma tus cambios adicionales en la rama desde la que enviaste el pull request (es decir, desde turrama, o desde turrama2 si tuviste que hacer cherry-pick y ordenarla), y vuelve a hacer push. Github añadirá automáticamente esos commits al pull request.

Ten en cuenta, sin embargo, que no puedes hacer rebase después de enviar un pull request. Pero eso realmente no es tu problema - una vez que hayas enviado la solicitud, es trabajo del equipo de desarrollo de Angband revisarla y fusionarla tan pronto como podamos. Si fusionamos otra cosa primero, solucionaremos cualquier conflicto de fusión cuando fusionemos la tuya.

Después de que tu trabajo sea fusionado, por favor no sigas trabajando en esa rama. Incluso si continúas desarrollando la misma funcionalidad, por favor haz fetch de official/master y comienza una nueva rama desde ahí para tu próximo pull request (no hay límite en la cantidad de ramas que puedes crear). Si por alguna razón no quieres hacer esto, entonces debes hacer rebase de tu rama sobre official/master antes de hacer push y ofrecer tu siguiente pull request. Deberías usar rebase en lugar de fusionar desde official/master, para evitar una proliferación de commits de fusión.

Así que el ciclo ideal es:

1. `git fetch official/master`
2. `git checkout official/master`
3. `git checkout -b nuevarrama`
4. ... haz tu trabajo en nuevarrama ...
5. `git fetch official/master` de nuevo (para ver si se ha actualizado mientras trabajabas)
   * (si es así) `git rebase official/master` (y soluciona cualquier conflicto)
6. `git push origin nuevarrama`
7. Ve a Github y abre el pull request
8. Espera a que el pull request sea fusionado (puedes hacer push de más commits mientras esperas)
9. Vuelve al paso 1 y comienza de nuevo

### Consejos generales

No tengas miedo de crear y eliminar muchas ramas. Son totalmente prescindibles, y una nueva rama es un comienzo fresco.

Actualiza siempre tu copia local del repositorio oficial (`git fetch official`) antes de hacer push de tu trabajo. Si es posible, usa `git rebase official/master` para asegurarte de que tus cambios estén encima de los commits más recientes del repositorio oficial - eso hará que sea más fácil fusionarlos, y es mucho más ordenado que fusionar ramas oficiales en tus ramas y que el equipo de desarrollo de Angband las vuelva a fusionar. Si te pone nervioso intentar rebase en una rama con mucho de tu trabajo duro, crea primero una nueva copia de esa rama - así, si el rebase sale terriblemente mal, tu rama original permanece intacta. Sin embargo, no uses rebase si has publicado tu rama en Github, ya que esto perjudicará a cualquiera que la esté siguiendo.

Al enviar pull requests en Github, por favor asegúrate de elegir solo los commits relevantes para esta solicitud - intenta evitar elegir commits de fusión o commits que sean parte de otro pull request.

## Directrices de codificación

Esta sección describe cómo debe verse el código de Angband y su documentación. Es posible que también quieras leer la antigua [guía de seguridad de Angband](/src/doc/security.txt), aunque la configuración de compilación por defecto ya no usa setgid.

### Reglas

* Estilo de llaves K&R, con tabulaciones de cuatro espacios
* Evita líneas de más de 80 caracteres de longitud (no es estricto si hay varias indentaciones, pero idealmente deberían refactorizarse)
* Si una función no toma parámetros, debe declararse como function(void), no simplemente como function().
* Usa const cuando no debas modificar una variable.
* Evita las variables globales como si fueran la peste, ya tenemos demasiadas.
* Usa enums siempre que sea posible en lugar de defines, y nunca uses números mágicos.
* No uses punto flotante.
* El código debe compilar como C89 con tipos int de C99, y no depender de comportamiento indefinido.
* No uses las funciones de cadenas integradas de C, usa las versiones my_ en su lugar (strcpy -> my_strcpy, sprintf -> strnfmt()). Son más seguras.

### Nuestro estilo de indentación es:
* Las llaves de apertura deben estar en una línea separada al comienzo de una función, pero por lo demás deben seguir la sentencia que las requiere ('if', 'do', 'for', etc.)
* Las llaves de cierre deben estar en líneas separadas, excepto cuando van seguidas de 'while' o 'else'
* Espacios alrededor de los operadores matemáticos, de comparación y de asignación ('+', '-', '/', '=', '!=', '==', '>', ...). Sin espacios alrededor de los operadores de incremento/decremento ('++', '--').
* Espacios entre identificadores de C como 'if', 'while' y 'for' y los corchetes de apertura ('if (foo)', 'while (bar)', ...),
* Los bucles `do { } while ();` deben tener una nueva línea después de "do {", y la parte "} while ();" debe estar en la misma línea.
* Sin espacios entre los nombres de funciones y los corchetes, ni entre los corchetes y los argumentos de la función (function(1, 2) en lugar de function ( 1, 2 )).
* Si tienes una sentencia if cuyo código ejecutado condicionalmente es solo una sentencia, no escribas ambas en la misma línea, excepto en el caso de "break" o "continue" en bucles.
* `return` no usa corchetes, `sizeof` sí.
* Usa dos indentaciones cuando una llamada a función/condicional se extienda a lo largo de varias líneas, no espacios.

#### Ejemplo:
```C
    if (fridge) {
        int i = 10;

        if (i > 100) {
            i += randint0(4);
            bar(1, 2);
        } else {
            foo(buf, sizeof(buf), FLAG_UNUSED, FLAG_TIMED,
                    FLAG_DEAD);
        }
      
        do {
            /* Only print even numbers */
            if (i % 2) continue;

            /* Be clever */
            printf("Aha!");
        } while (i--);

        return 5;
    }
```

Escribe código para humanos primero y para la ejecución después. Cuando el código no sea claro, comenta, pero por ejemplo lo siguiente es innecesariamente detallado y perjudica la legibilidad:
```C
    /* Delete the object */
    object_delete(idx);
```

### Módulos de código

* Deberías escribir código como módulos siempre que sea posible, con funciones y variables globales dentro de un módulo con el mismo prefijo, como "macro_".
* Si necesitas inicializar cosas en tu módulo, incluye funciones "init" y "free" y llámalas apropiadamente en lugar de poner cosas específicas del módulo por todas partes.
* Algún día el juego podría no cerrarse cuando termina una partida, y podría permitir cargar otras partidas. Ten esto en cuenta.

### Documentación

Ten cuidado al documentar funciones de usar el siguiente diseño:
```C
    /**
     * Provides an example of a documentation style.
     *
     * The purpose of the function do_something() is explained here, mentioning
     * the name and use of every parameter (e.g. `example`).  It returns TRUE if
     * conditions X or Y are met, and FALSE otherwise.
     *
     * BUG: Brief description of bug. (#12345)
     * TODO: Feature to implement. (#54321)
     */
    bool do_something(void *example)
```
#### Notas adicionales sobre el formato
* Tener la descripción breve separada del resto del comentario significa que Doxygen puede extraerla sin necesidad de etiquetas @brief.
* Las variables deben mencionarse rodeadas de comillas invertidas ('`').
* Las funciones deben mencionarse como function_name() -- ''con'' los paréntesis.
* En las descripciones breves de clases y funciones, usa el tiempo presente (es decir, responde a la pregunta "¿Qué hace esto?" con "Construye / edita / calcula / devuelve...")
* En las descripciones largas, usa la voz pasiva para referirte a las variables (es decir, "Las variables se normalizan." en lugar de "Esto normaliza las variables.")
* Sin preferencia de ortografía UK/US (es decir, se adopta la preferencia del primer comentarista para ese comentario).
* (de "The Elements of Style") "Una oración no debe contener palabras innecesarias, un párrafo ninguna oración innecesaria, por la misma razón que un dibujo no debe tener líneas innecesarias y una máquina ninguna parte innecesaria. Esto no requiere que el escritor haga todas sus oraciones cortas, ni que evite todo detalle y trate sus temas solo en esbozo, sino que cada palabra cuente."
