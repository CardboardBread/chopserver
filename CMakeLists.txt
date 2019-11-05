cmake_minimum_required (VERSION 2.6)
project (chopserver)
add_executable(chopserver chopserver.c chopconn.c chopconst.c chopdata.c chopdebug.c choppacket.c chopsocket.c)
add_executable(chopclient chopclient.c chopconn.c chopconst.c chopdata.c chopdebug.c choppacket.c chopsocket.c)
