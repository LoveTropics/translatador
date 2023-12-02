find_package(JNI REQUIRED)

add_library(translatador-java SHARED "src/translatador.c")

target_link_libraries(translatador-java PRIVATE JNI::JNI)
target_link_libraries(translatador-java PRIVATE translatador)

# Use dummy generator expression to ensure that we don't use the configuration subdirectory on Windows: <https://cmake.org/cmake/help/latest/prop_tgt/LIBRARY_OUTPUT_DIRECTORY.html>
set_target_properties(translatador-java PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/$<0:>
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/$<0:>
)
