cmake_minimum_required(VERSION 3.16)
project(citygen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB CITYGEN_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(citygen ${CITYGEN_SOURCES})
target_include_directories(citygen PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(citygen PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Keep the binary in a predictable spot for tests and tooling.
set_target_properties(citygen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}
)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${cfg} cfg_upper)
        set_target_properties(citygen PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} ${PROJECT_SOURCE_DIR}
        )
    endforeach()
endif()

# Provide compile_commands.json at the source root for clangd and other tools.
add_custom_target(copy_compile_commands ALL
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${PROJECT_SOURCE_DIR}/compile_commands.json
)

include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
enable_testing()
add_test(
    NAME python_tests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.test_citygen
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
