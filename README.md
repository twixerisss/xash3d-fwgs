# Xash3D FWGS Engine (Wii port) <img align="right" width="128" height="128" src="https://github.com/FWGS/xash3d-fwgs/raw/master/game_launch/icon-xash-material.png" alt="Xash3D FWGS icon" />

Xash3D ([pronounced](https://ipa-reader.com/?text=ks%C9%91%CA%82) `[ksɑʂ]`) FWGS Es un motor de juego diseñado para brindar compatibilidad con Half-Life Engine y ampliarlo, además de ofrecer a los desarrolladores de juegos un flujo de trabajo conocido. Esta es la versión para Wii/Gamecube del motor.

Xash3D FWGS is a heavily modified fork of an original [Xash3D Engine](https://www.moddb.com/engines/xash3d-engine) by Unkle Mike.

## Instalación y Ejecución 

0) Renombrar `xash.dol` a `boot.dol` una vez compilado.
1) Mueve el archivo `boot.dol a algún directorio dentro de la carpeta `apps`.
2) Pega tu copia legal `valve` Dentro de la carpeta `xash3d` que deberás crear en la raíz de tu SD.
3) Ejecútalo por medio de Homebrew Channel.

## Instrucciones de la Build
La versión para Wii actualmente usa cmake para compilar sus binarios. Se integrará en waf en algún momento.

## De preferencia, compile este motor por medio de una distribución Linux, es más rápido y no presenta problemas de enlace

**NOTE: NUNCA USE LOS ARCHIVOS ZIP DE GitHub. GitHub no incluye las dependencias externas que estamos utilizando!**

### Prerrequisitos

*  Instala CMake
*  Instala [devkitPro](https://devkitpro.org/wiki/Getting_Started)
*  Instala devkitPPC y las siguientes librerías faltantes
 `sudo (dkp-)pacman -S wii-dev wii-sdl2 wii-opengx ppc-bzip2 ppc-freetype ppc-zlib`
*  Crea una dirección de desarrollo(Ubicada en un almacenamiento el cual no tenga espacios en su nombre, al momento de compilar no se suele encontrar archivos por este problema)
*  Clona los siguientes repositorios en el mismo directorio.
git clone --recursive https://github.com/MintFerret/xash3d-fwgs
git clone --recursive https://github.com/MintFerret/mainui_cpp
git clone --recursive https://github.com/MintFerret/hlsdk-portable
```

### Building
1) Configura build `cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="/opt/devkitpro/cmake/Wii.cmake"`
2) Compila `make -C build`

This will build:
- the filesystem
-  hlsdk (game libraries)
-  mainui
-  the engine itself

### Nota: 
-Este es un proyecto en desarrollo. 
-Es posible muy probable toparse con errores e inestabilidad 

### Créditos 
- Uncle Mike for the original Xash3D Engine
- FWGS team for Xash3D FWGS fork
- mardy for the SDL2 port and OpenGX
- devkitPro team

