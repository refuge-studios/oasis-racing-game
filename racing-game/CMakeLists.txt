cmake_minimum_required(VERSION 3.20)

project(racing_game
    VERSION 0.1.0
    LANGUAGES CXX
)

# --------------------------------------------------
# Language / compiler settings
# --------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/O2 /Ob2)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
endif()

# --------------------------------------------------
# Dependencies
# --------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

# --------------------------------------------------
# Game module
# --------------------------------------------------
add_library(racing_game MODULE
    src/main.cpp
)

target_include_directories(racing_game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(racing_game PRIVATE glm::glm)

set_target_properties(racing_game PROPERTIES
    OUTPUT_NAME "racing-game"
    PREFIX ""
)

# --------------------------------------------------
# Output directories
# --------------------------------------------------
set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
    set(OUT_DIR "${BIN_DIR}/windows")
elseif(APPLE)
    set(OUT_DIR "${BIN_DIR}/macos")
else()
    set(OUT_DIR "${BIN_DIR}/linux")
endif()

# Single-config generators (Ninja, Makefiles)
set_target_properties(racing_game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${OUT_DIR}"
)

# Multi-config generators (Visual Studio, Xcode)
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set_target_properties(racing_game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${OUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${OUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${OUT_DIR}"
    )
endforeach()

if(NOT MSVC)
    target_compile_options(racing_game PRIVATE -Werror)
endif()
