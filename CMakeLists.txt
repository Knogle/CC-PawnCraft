cmake_minimum_required(VERSION 3.20)
project(ccpawn_native C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(PAWN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pawn")

set(PAWNCC_SOURCES
    ${PAWN_ROOT}/compiler/sc1.c ${PAWN_ROOT}/compiler/sc2.c
    ${PAWN_ROOT}/compiler/sc3.c ${PAWN_ROOT}/compiler/sc4.c
    ${PAWN_ROOT}/compiler/sc5.c ${PAWN_ROOT}/compiler/sc6.c
    ${PAWN_ROOT}/compiler/sc7.c ${PAWN_ROOT}/compiler/scexpand.c
    ${PAWN_ROOT}/compiler/sci18n.c ${PAWN_ROOT}/compiler/sclist.c
    ${PAWN_ROOT}/compiler/scmemfil.c ${PAWN_ROOT}/compiler/scstate.c
    ${PAWN_ROOT}/compiler/scvars.c ${PAWN_ROOT}/compiler/lstring.c
    ${PAWN_ROOT}/compiler/memfile.c ${PAWN_ROOT}/amx/keeloq.c
    ${PAWN_ROOT}/linux/binreloc.c)

add_executable(ccpawn-pawncc ${PAWNCC_SOURCES})
target_include_directories(ccpawn-pawncc PRIVATE
    ${PAWN_ROOT}/compiler ${PAWN_ROOT}/amx ${PAWN_ROOT}/linux)
target_compile_definitions(ccpawn-pawncc PRIVATE
    ENABLE_BINRELOC _GNU_SOURCE HAVE_UNISTD_H HAVE_STDINT_H HAVE_INTTYPES_H)
target_link_libraries(ccpawn-pawncc PRIVATE m pthread)

add_executable(ccpawn-runner src/native/runner.c src/native/stdlib.c ${PAWN_ROOT}/amx/amx.c)
target_link_libraries(ccpawn-runner PRIVATE m)
target_include_directories(ccpawn-runner PRIVATE ${PAWN_ROOT}/amx ${PAWN_ROOT}/linux)
target_compile_definitions(ccpawn-runner PRIVATE
    AMX_NODYNALOAD AMX_ANSIONLY PAWN_CELL_SIZE=32
    HAVE_UNISTD_H HAVE_STDINT_H HAVE_INTTYPES_H)
target_compile_options(ccpawn-runner PRIVATE -Wall -Wextra -Wpedantic)
