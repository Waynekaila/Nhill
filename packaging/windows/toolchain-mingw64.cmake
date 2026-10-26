# Compilation croisée Linux -> Windows 10/11 x86_64 avec MinGW-w64 (paquets Ubuntu/Debian
# g++-mingw-w64-x86-64-posix : modèle de threads POSIX exigé par Qt 6).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER  ${MINGW_PREFIX}-windres)
set(CMAKE_AR           /usr/bin/${MINGW_PREFIX}-ar)
set(CMAKE_RANLIB       /usr/bin/${MINGW_PREFIX}-ranlib)
# Qt pour Windows (construit par build-qt-mingw.sh) et bibliothèques MinGW
set(NHILL_WIN_PREFIX "$ENV{NHILL_WIN_PREFIX}" CACHE PATH "Préfixe de Qt et QtKeychain pour Windows")
set(CMAKE_FIND_ROOT_PATH /usr/${MINGW_PREFIX} ${NHILL_WIN_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
# Exécutables autonomes : libgcc et libstdc++ liés statiquement (pas de DLL MinGW à livrer
# hormis libwinpthread, copiée par le script de déploiement).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
