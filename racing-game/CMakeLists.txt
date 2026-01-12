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

find_package(glm REQUIRED)

# Dependencies
target_link_libraries(racing_game
    PRIVATE
        glm::glm
)

# --------------------------------------------------
# Output configuration
# --------------------------------------------------
set_target_properties(racing_game PROPERTIES
    OUTPUT_NAME "game_module" # engine loads a consistent name
    PREFIX ""                 # no lib prefix (libgame_module.so → game_module.so)
)