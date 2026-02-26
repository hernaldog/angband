# Angband 4.2.6 en Español

## Autor
Hernaldo González  - hernaldog@gmail.com

# Status de la traducción
4%

## Bitácora
- 21-02-2026 - Se entiende como compilar en Windows 11 y hacen pruebas de concepto.
- 21-02-2026 - Se inicia traducción de primeros elementos como news.txt o archivos c/h base.

## Motivación
Me entantan los juegos Roguelike clásicos como Moria, Rogue, etc, a la vez, siempre me ha gustado el Señor de los Anillos, y que mejor que este gran juego que uno los dos mundos. 
Lo he jugado un par de veces solo en inglés y siempre he pensado que más gente de habla hispana lo jugaría si estaría en Español. 
Como fui traductor de ROMS de la vieja escena de SNES o NES por los años 2000 en "RomHack Hispanio.org", 
tengo algo de experiencia en traducciones y quise aplicar lo aprendido allí. Espero terminar lo empezado, no es una tarea fácil.

## Sitio base y Git de Angband
- https://github.com/angband/angband
- https://rephial.org Sitio con último release y código fuente para Windows, Linux y Mac
- https://angband.readthedocs.io/en/latest/index.html Manual

## Licencia
GNU GPL v2



## Pasos para la compilación si quieres colaborar

Bajar **MSYS2**  https://www.msys2.org/
Instalarlo sin nada particular, todo Siguiente, Siguiente.

En Windows 11, entrar al acceso directo creado **MSYS2 MINGW64**, es un icono con una M blanca sobre un fondo azul.

### Comandos

Este primero, demora como 5 min

    pacman -S make mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja

Luego:

    pacman -S mingw-w64-x86_64-libpng
    pacman -S mingw-w64-x86_64-ncurses

Después:

    pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image \
        mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-SDL2_mixer

Ir a carpeta **src** dentro del fuente

    cd c:\juegos\Angbandsrc\src

Compilar para Windows Native:

    cmake -G Ninja -DSUPPORT_WINDOWS_FRONTEND=ON \
        -DSUPPORT_STATIC_LINKING=ON \
        ..
    ninja

Si funciona bien dirá esto:

    naldo@shachalaloca MINGW64 /c/juegos/angband-esp/src
    $ cmake -G Ninja -DSUPPORT_WINDOWS_FRONTEND=ON \
        -DSUPPORT_STATIC_LINKING=ON \
        ..
    ninja
    -- Could NOT find Sphinx (missing: SPHINX_EXECUTABLE)
    CMake Warning at CMakeLists.txt:100 (message):
      Disabling SDL2 front end because Windows front end is enabled
    
    
    CMake Warning at CMakeLists.txt:124 (message):
      Disabling SDL2 sound because Windows front end is enabled
    
    
    -- Using system PNG and ZLIB (static)
    --   PNG static libraries    : png16;m;z
    --   PNG static include dirs : C:/msys64/mingw64/include/libpng16;C:/msys64/mingw64/include
    -- Configuring done (0.4s)
    -- Generating done (4.4s)
    -- Build files have been written to: C:/juegos/angband-esp/src
    [216/216] Linking C executable game\angband.exe

Salida del ejecutable en carpeta src/game:
C:\juegos\Angbandsrc\src\game

ahí estará: angband.exe

Instrucción original en Inglés: https://angband.readthedocs.io/en/latest/hacking/compiling.html#using-msys2-with-mingw64

## Probando exe
Descarga el binario (no el código fuente) para Windows Angband-4.2.6 desde https://rephial.org donde dice "Download". 

Descomprimir en C:\Angband-4.2.6

Para probarlo, copia ese **angband.exe** que generaste recién de la compilación de 

    C:\juegos\Angbandsrc\src\game

y pégalo sobre la carpeta donde desomprimiste el ZIP original

    C:\Angband-4.2.6

Así toma las librerías .dll correctas.

## Traduciendo TXT

Hay archivos txt que se pueden traducir de forma directa sin tener que compilar como \lib\screens\news.txt. Esto hace más simple la traducción.

## Traduciendo archivos .c

Una vez traducidos, antes de compilar hay que eliminar la carpeta /src/game generada en una compilación anterior. De lo contrario, no se ve reflejado el cambio.

## Archivo shell script compila.sh

Usa este archivo SH para compilar más facilmente ya que elimina carpeta temporal, compila y además copia el ejecutable a la carpeta del juego original para probar más facilmente. Este archivo quedó subido a GIT.

Para correrlo debes copiar el archivo que está en el fuernte

luego dejarlo en tu ruta local: 

    C:\msys64\home\<tu usuario>

Luego entrar al acceso directo de Windows **MSYS2 MINGW64** (no es necesairo abrirlo como administrador), y adentro solo ejecutar el comando:

    .\compila.sh

Contenido del script shell:

    cd c:
    cd c:/juegos/angband-src-esp/src
    
    echo "Eliminando carpeta game"
    rm -rf game
    
    echo "Eliminando carpeta CMakeFiles"
    rm -rf CMakeFiles
    
    echo "Compilando..."
    cmake -G Ninja -DSUPPORT_WINDOWS_FRONTEND=ON \
        -DSUPPORT_STATIC_LINKING=ON \
        ..
    ninja
    
    echo "Compiando archivo a exe..."
    cp -f /c/juegos/angband-src-esp/src/game/angband.exe /c/juegos/angband-bin-esp/angband.exe
    
    echo "Compilacion terminada..."

## Pendientes de traducción
- Cambios en tabulaciones o largos de frase que se ven mal visualmente como "Selecciona Nuevo" se ve muy a la derecha
- Cambios de lb por kg
- Cambios de ft por mt (infravisión)
- Mejoras en traducciones como con tecla d "Soltar qué objeto", por "¿Qué objeto tirar?" o con k "¿Ingorar que objeto?"

## Detalle de la traducción por archivo

| Archivo                                  | % Avance | TODO                                                 |
| -----------------------------------------| ------   | -----------------------------------------------------|
| lib\screens\news.txt                     | 100      |
| lib\gamedata\player_property.txt         | 100      |
| lib\gamedata\history.txt                 | 100      |
| lib\help\commands.txt                    | 100      |
| lib\help\index.txt                       | 100      |
| lib\help\r_index.txt                     | 100      |
| lib\help\symbols.txt                     | 100      |
| src\main-win.c                           | 100      |
| src\borg\borg-messages.c                 | 100      |
| src\mon-util.c                           | 100      |
| src\cmd-cave.c                           | 100      |
| src\borg\borg-item-val.c                 | 100      |
| src\ui-mon-list.c                        | 100      | Corregir traducción "Puedes ver ningún monstruo.|
| src\ui-knowledge.c                       | 100      |
| src\ui-game.c                            | 100      |
| src\ui-score.c                           | 100      |
| src\list-equip-slots.h                   | 100      |
| src\obj-desc.c                           | 100      |
| src\ui-obj-list.c                        | 100      |
| src\ui-birth.c                           | 100      |
| src\ui-help                              | 100      |
| src\cmd-obj.c                            | 100      |
| src\ui-object.c                          | 100      |
| src\ui-input.c                           | 100      |
| src\player-attack.c                      | 100      |
| src\player-util.c                        | 100      |
| src\ui-command.c                         | 100      |
| src\ui-player.c                          | 100      |
| src\list-options.h                       | 100      |
| src\ui-options.c                         | 100      |  
| src\ui-display.c                         | 100      |  
