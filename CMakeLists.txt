cmake_minimum_required(VERSION 3.15)

project("translatador")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# WASM-compatible Bergamot allows us to ship a library without dependency on MKL/CUDA
set(USE_WASM_COMPATIBLE_SOURCE ON)

# Ensure that the project can be embedded in a shared library (e.g. for bindings)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(extern)

set_target_properties(bergamot-translator PROPERTIES GIT_SUBMODULE OFF)

add_library(translatador STATIC "src/translatador.cpp")
target_include_directories(translatador PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(translatador PRIVATE bergamot-translator)
target_link_libraries(translatador PRIVATE whatlang)

add_subdirectory(bindings)
add_subdirectory(examples)
