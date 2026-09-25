cmake_minimum_required(VERSION 3.20)
project(Chip8)

# C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generates compile_commands.json for clangd / editor IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Remove the need to copy and paste assets in debug builds
option(PRODUCTION_BUILD
    "Enable Production Build"
    OFF
)

# Access all cpp files in src
# Always do std::string(RESOURCES_PATH) + "textures/unit.png"
file(GLOB_RECURSE SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# For packages installed by vcpkg
find_package(SFML CONFIG REQUIRED COMPONENTS System Window Graphics Network Audio)

# For intellisense
target_include_directories(${PROJECT_NAME}
    PRIVATE include
)

# For linking library
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    SFML::System
    SFML::Window
    SFML::Graphics
    SFML::Network
    SFML::Audio
)

# Asset path
if(PRODUCTION_BUILD)
    # Copy assets to release

    message(STATUS
        "Production Build Enabled"
    )

    target_compile_definitions(
        ${PROJECT_NAME}
        PRIVATE
        RESOURCES_PATH="./assets/"
        PRODUCTION_BUILD=1
        DEVELOPMENT_BUILD=0
    )

    # Copy assets beside executable
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND}
        -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )

else()
    # Directly refer to assets in root

    message(STATUS
        "Development Build Enabled"
    )

    target_compile_definitions(
        ${PROJECT_NAME}
        PRIVATE
        RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/"
        PRODUCTION_BUILD=0
        DEVELOPMENT_BUILD=1
    )

endif()