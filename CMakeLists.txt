cmake_minimum_required(VERSION 3.15)

project("translatador")

option(USE_WHATLANG "Compile with language detection support via whatlang-rs" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Make sure that we have pulled submodules (https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html)
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if (GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if (NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif ()
    endif ()
endif ()

# Ensure that the project can be embedded in a shared library (e.g. for bindings)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(extern)

if (USE_WHATLANG)
    add_definitions(-DUSE_WHATLANG=1)
endif ()

add_library(translatador STATIC src/translatador.cpp src/tokenization.cpp)

# We don't use these ourselves - but we include Marian headers, so we need to pass them down
target_compile_definitions(translatador PUBLIC USE_SSE2 WASM_COMPATIBLE_SOURCE)
target_include_directories(translatador PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(translatador PRIVATE marian ssplit)
if (USE_WHATLANG)
    target_link_libraries(translatador PRIVATE whatlang)
endif ()

add_subdirectory(bindings)
add_subdirectory(examples)
