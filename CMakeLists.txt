cmake_minimum_required(VERSION 3.0)

project(holycc2 LANGUAGES C)

add_executable(holycc2 main.c)

install(TARGETS holycc2 RUNTIME DESTINATION bin)
