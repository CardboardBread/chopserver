cmake_minimum_required (VERSION 3.10)
project (chopserver)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 99)

add_compile_options(-Werror -Wall)

link_libraries(pthread m)
include_directories(include)

add_executable(chopserver
        src/chopserver.c
        src/chopconn.c
        src/chopconst.c
        src/chopdata.c
        src/chopdebug.c
        src/choppacket.c
        src/chopsocket.c
        src/chopsend.c
        include/hashtable.c)

add_executable(chopclient
        src/chopclient.c
        src/chopconn.c
        src/chopconst.c
        src/chopdata.c
        src/chopdebug.c
        src/choppacket.c
        src/chopsocket.c
        src/chopsend.c
        include/hashtable.c)
