# Converter
the implementation of a USD to OBJ converter tool for the Caldera map data set

## Build
### Requirements (tested versions)
* Windows 11
* C++ compiler (msvc 19.50)
* CMake (v4.2.0)
* GPU (NVIDIA 4070)
* a working build of OpenUSD framework (clone from https://github.com/PixarAnimationStudios/OpenUSD and follow installation guide; after build is complete, set environment variables as instructed in terminal)
* a local clone of the Caldera map data set (clone from https://github.com/Activision/caldera)

### CMake with default build tools
* git clone --recurse-submodules https://github.com/Ragnaro0k/converter
* cd converter
* cmake -B build
* cmake --build build --config Release
* cmake --install build --config Release 
* cd install/bin
* converter.exe

### Usage
refer to user_manual.pdf



