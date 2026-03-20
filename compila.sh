cd c:
cd c:/juegos/angband-src-esp/src

echo "Eliminando carpeta game"
shopt -s extglob
rm -rf game/!(lib)
rm -rf game/lib/!(user)

echo "Eliminando carpeta CMakeFiles"
rm -rf CMakeFiles

echo "Compilando..."
cmake -G Ninja -DSUPPORT_WINDOWS_FRONTEND=ON \
    -DSUPPORT_STATIC_LINKING=ON \
    ..
ninja

echo "Compilacion terminada..."

echo "Abriendo juego C:/juegos/angband-src-esp/src/game/angband.exe"

C:/juegos/angband-src-esp/src/game/angband.exe