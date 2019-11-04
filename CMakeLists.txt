cmake_minimum_required (VERSION 2.6)
project (chopserver)
add_executable(chopserver chopserver.c chopconn.c choppacket.c chopdebug.c chopconst.c chopsocket.c)
add_executable(chopclient chopclient.c chopconn.c choppacket.c chopdebug.c chopconst.c chopsocket.c)
