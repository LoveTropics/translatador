set(USE_WASM_COMPATIBLE_SOURCE ON CACHE BOOL "Use WASM compatible source")
set(COMPILE_CUDA OFF CACHE BOOL "Compile GPU version")
set(USE_SENTENCEPIECE ON CACHE BOOL "Download and compile SentencePiece")
set(USE_STATIC_LIBS ON CACHE BOOL "Link statically against non-system libs")
set(SSPLIT_COMPILE_LIBRARY_ONLY ON CACHE BOOL "Do not compile ssplit tests")
set(COMPILE_LIBRARY_ONLY ON CACHE BOOL "Build only the Marian library and exclude all executables.")
set(USE_MKL OFF CACHE BOOL "Compile with MKL support")
# We don't expect users to dynamically link PCRE2
set(SSPLIT_USE_INTERNAL_PCRE2 ON CACHE BOOL "Use internal PCRE2 instead of system PCRE2")

add_subdirectory(marian-dev EXCLUDE_FROM_ALL)
add_subdirectory(ssplit-cpp EXCLUDE_FROM_ALL)

# Marian & ssplit aren't using target_include_directories, so they don't propagate correctly to dependents
get_property(MARIAN_INCLUDES DIRECTORY marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
get_property(SSPLIT_INCLUDES DIRECTORY ssplit-cpp/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(marian PUBLIC ${MARIAN_INCLUDES})
target_include_directories(ssplit PUBLIC ${SSPLIT_INCLUDES})

get_property(MARIAN_COMPILE_DEFINITIONS DIRECTORY marian-dev PROPERTY COMPILE_DEFINITIONS)
get_property(MARIAN_COMPILE_OPTIONS DIRECTORY marian-dev PROPERTY COMPILE_OPTIONS)
target_compile_definitions(marian PUBLIC ${MARIAN_COMPILE_DEFINITIONS})
target_compile_options(marian PUBLIC ${MARIAN_COMPILE_OPTIONS})

get_directory_property(MARIAN_CMAKE_C_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_C_FLAGS)
get_directory_property(MARIAN_CMAKE_CXX_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_CXX_FLAGS)
set(CMAKE_C_FLAGS ${MARIAN_CMAKE_C_FLAGS} PARENT_SCOPE)
set(CMAKE_CXX_FLAGS ${MARIAN_CMAKE_CXX_FLAGS} PARENT_SCOPE)

if (USE_WHATLANG)
    add_subdirectory(whatlang-ffi)
endif ()
