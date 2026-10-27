cmake_minimum_required(VERSION 3.22)
project(bazuko_cpp VERSION 1.0.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Use pinned dependency revisions" FORCE)

# Banderas de compilación estrictas
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Opciones de GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Gestión de dependencias externas (GLFW, GLM, ImGui)
find_package(glfw3 3.3 QUIET)
find_package(glm 0.9.9 QUIET)

if(NOT TARGET glfw)
    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4 GIT_SHALLOW TRUE)
endif()

if(NOT TARGET glm::glm)
    FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG 1.0.1 GIT_SHALLOW TRUE)
endif()

FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.8 GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm imgui)

# GLAD (Local)
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# Dear ImGui setup
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw glad)

# Búsqueda/Configuración de WebP (FORZADA para evitar errores de find_path en Windows)
if(WIN32)
    set(LOCAL_WEBP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/win64-deps")
    set(WEBP_INCLUDE_DIR "${LOCAL_WEBP_ROOT}/include")
    
    if(EXISTS "${LOCAL_WEBP_ROOT}/lib/libwebp.lib")
        set(WEBP_LIBRARY "${LOCAL_WEBP_ROOT}/lib/libwebp.lib")
    else()
        set(WEBP_LIBRARY "${LOCAL_WEBP_ROOT}/lib/webp.lib")
    endif()

    add_library(bazuko_webp UNKNOWN IMPORTED)
    set_target_properties(bazuko_webp PROPERTIES IMPORTED_LOCATION "${WEBP_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}")
    set(BAZUKO_WEBP_TARGET bazuko_webp)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WEBP QUIET IMPORTED_TARGET libwebp)
    endif()
    if(TARGET PkgConfig::WEBP)
        set(BAZUKO_WEBP_TARGET PkgConfig::WEBP)
    else()
        find_path(WEBP_INCLUDE_DIR webp/decode.h REQUIRED)
        find_library(WEBP_LIBRARY NAMES webp REQUIRED)
        add_library(bazuko_webp UNKNOWN IMPORTED)
        set_target_properties(bazuko_webp PROPERTIES IMPORTED_LOCATION "${WEBP_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES "${WEBP_INCLUDE_DIR}")
        set(BAZUKO_WEBP_TARGET bazuko_webp)
    endif()
endif()

find_package(Threads REQUIRED)

# Fuentes del Ejecutable
file(GLOB_RECURSE BAZUKO_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(bazuko ${BAZUKO_SOURCES})

# Inclusión directa
target_include_directories(bazuko PRIVATE src third_party/include third_party/glad/include "${CMAKE_CURRENT_SOURCE_DIR}/win64-deps/include")
target_link_libraries(bazuko PRIVATE glfw glm::glm glad imgui Threads::Threads ${BAZUKO_WEBP_TARGET} ${CMAKE_DL_LIBS})

# Post-Build: Copia de recursos
add_custom_command(TARGET bazuko POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:bazuko>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:bazuko>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../public" "$<TARGET_FILE_DIR:bazuko>/public"
    COMMENT "Copiando recursos al directorio binario..."
)

# Copiar DLLs en Windows si aplica
if(WIN32)
    file(GLOB BAZUKO_WEBP_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/win64-deps/bin/*.dll")
    foreach(WEBP_RUNTIME_DLL IN LISTS BAZUKO_WEBP_RUNTIME_DLLS)
        add_custom_command(TARGET bazuko POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WEBP_RUNTIME_DLL}" "$<TARGET_FILE_DIR:bazuko>"
            VERBATIM
        )
    endforeach()
endif()