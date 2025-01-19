project("translatador-example")

add_executable(translatador-example EXCLUDE_FROM_ALL "simple.c")
target_link_libraries(translatador-example PRIVATE translatador)
