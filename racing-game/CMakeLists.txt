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

# --------------------------------------------------
# Game module target
# --------------------------------------------------
add_library(racing_game MODULE
    src/main.cpp
)

# Expose public headers if needed by the engine
target_include_directories(racing_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------------------------------------------------
# Dependencies: FetchContent for glm
# --------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8  # stable release
)
FetchContent_MakeAvailable(glm)

# --------------------------------------------------
# Output configuration
# --------------------------------------------------
# Ensure engine loads a consistent name: racing_game
set_target_properties(racing_game PROPERTIES
    OUTPUT_NAME "racing-game"  # All platforms
    PREFIX ""                  # Remove "lib" prefix
)

# Platform-specific output directories
set(BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if(WIN32)
    set_target_properties(racing_game PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BIN_DIR}/windows"
    )
elseif(APPLE)
    set_target_properties(racing_game PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/macos"
    )
elseif(UNIX)
    set_target_properties(racing_game PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${BIN_DIR}/linux"
    )
endif()


# add position-independent code for shared libraries
set_target_properties(racing_game PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
