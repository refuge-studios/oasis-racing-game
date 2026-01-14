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

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/O2 /Ob2 /DNDEBUG)
else()
    add_compile_options(-O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic)
endif()

# --------------------------------------------------
# Dependencies: FetchContent for glm
# --------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
)
FetchContent_MakeAvailable(glm)

# --------------------------------------------------
# Game module target
# --------------------------------------------------
add_library(racing_game MODULE
    src/main.cpp
)

target_include_directories(racing_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(racing_game PRIVATE glm::glm)

# --------------------------------------------------
# Output configuration
# --------------------------------------------------
set_target_properties(racing_game PROPERTIES
    OUTPUT_NAME "racing-game"
    PREFIX ""  # remove "lib" prefix
)

# Platform-specific output directories
set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
    set_target_properties(racing_game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/windows"
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/windows"
        ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIR}/windows"
    )
elseif(APPLE)
    set_target_properties(racing_game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/macos"
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/macos"
        ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIR}/macos"
    )
elseif(UNIX)
    set_target_properties(racing_game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/linux"
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/linux"
        ARCHIVE_OUTPUT_DIRECTORY "${BIN_DIR}/linux"
    )
endif()

if(NOT MSVC)
    target_compile_options(racing_game PRIVATE -Werror)
endif()

# --------------------------------------------------
# Install rules: install directly into bin/<platform>
# --------------------------------------------------
if(WIN32)
    install(TARGETS racing_game
        RUNTIME DESTINATION .      # put DLL directly in prefix (bin/windows)
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .
    )
elseif(APPLE)
    install(TARGETS racing_game
        RUNTIME DESTINATION .      # put .dylib directly in prefix (bin/macos)
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .
    )
elseif(UNIX)
    install(TARGETS racing_game
        RUNTIME DESTINATION .      # put .so directly in prefix (bin/linux)
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION .
    )
endif()