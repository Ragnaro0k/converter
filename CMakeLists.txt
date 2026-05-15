# CMakeList.txt: Soubor projektu CMake na nejvyšší úrovni, proveďte na tomto
# místě globální konfiguraci a zahrňte podprojekty.
#
cmake_minimum_required (VERSION 3.10)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("converter")

add_subdirectory ("converter")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/../install" CACHE PATH "Install prefix" FORCE)
endif()

install(TARGETS converter
    RUNTIME DESTINATION bin
)