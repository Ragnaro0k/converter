
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
add_executable(converter "converter.cpp" "converter.h" "meshes.cpp" "meshes.h")

add_custom_command(TARGET converter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/external/PythonScript/usd_loader.py
        $<TARGET_FILE_DIR:converter>
)

install(FILES
    ${CMAKE_SOURCE_DIR}/external/PythonScript/usd_loader.py
    DESTINATION bin
)

target_compile_definitions(converter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET converter PROPERTY CXX_STANDARD 20)
endif()

add_subdirectory(
    ../external/glfw
    external/glfw-build
)

add_subdirectory(
    ../external/glm
    external/glfm-build
)

add_subdirectory(
    ../external/pybind11
    external/pybind11-build
)

add_library(glad ../external/glad/src/glad.c)


set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui)

add_library(imgui
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
target_link_libraries(converter PRIVATE glfw)
target_link_libraries(converter PRIVATE glm::glm)
target_link_libraries(converter PRIVATE glad)
target_link_libraries(converter PRIVATE Python3::Python)
target_link_libraries(converter PRIVATE pybind11::embed)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_include_directories(glad PUBLIC ../external/glad/include)
target_link_libraries(imgui PUBLIC glfw)
target_link_libraries(converter PRIVATE glfw opengl32 imgui)
