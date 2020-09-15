cmake_minimum_required(VERSION 3.0)

project(holycc2 LANGUAGES C)

set(sources "")
list(APPEND sources "main.c")
list(APPEND sources "linkedList.c")
list(APPEND sources "hashTable.c")
list(APPEND sources "linkedList.c")
list(APPEND sources "str.c")
list(APPEND sources "graph.c")

set(tests "")
list(APPEND tests "tests/tests.c")
list(APPEND tests "tests/linkedListTests.c")
list(APPEND tests "tests/strTests.c")

add_executable(holycc2 ${sources} ${tests})
target_include_directories(holycc2 PRIVATE ${CMAKE_SOURCE_DIR})
target_link_Libraries(holycc2 dill)
install(TARGETS holycc2 RUNTIME DESTINATION bin)
