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
