#!/bin/bash

set -euo pipefail

# Build script for JP's BASIC
#
# It's based on R.T.Russell's BBC BASIC for SDL but with minimal wrappers around
# it

BUILD=./build
BBCSDL=./BBCSDL
CC=arm-none-eabi-gcc
CFLAGS="-mcpu=cortex-m0plus -g -Os -I${BBCSDL}/include -DPICO"

rm -rf ${BUILD}
mkdir -p ${BUILD}

echo "Building bbasmb_arm_v6m.c..."
${CC} ${CFLAGS} -c ${BBCSDL}/src/bbasmb_arm_v6m.c -o ${BUILD}/bbasmb_arm_v6m.o
echo "Building bbdata_arm_32.s..."
${CC} ${CFLAGS} -c ${BBCSDL}/src/bbdata_arm_32.s -o ${BUILD}/bbdata_arm_32.o
echo "Building bbeval.c..."
${CC} ${CFLAGS} -c ${BBCSDL}/src/bbeval.c -o ${BUILD}/bbeval.o
echo "Building bbexec.c..."
${CC} ${CFLAGS} -c ${BBCSDL}/src/bbexec.c -o ${BUILD}/bbexec.o -Wno-discarded-qualifiers
echo "Building bbmain.c..."
${CC} ${CFLAGS} -c ${BBCSDL}/src/bbmain.c -o ${BUILD}/bbmain.o
echo "Building main.c..."
${CC} ${CFLAGS} -c src/main.c -o ${BUILD}/main.o

echo "Linking bbcbasic.elf..."
${CC} ${CFLAGS} -o ${BUILD}/bbcbasic.elf \
    ${BUILD}/bbasmb_arm_v6m.o \
    ${BUILD}/bbdata_arm_32.o \
    ${BUILD}/bbeval.o \
    ${BUILD}/bbexec.o \
    ${BUILD}/bbmain.o \
    ${BUILD}/main.o \
    -lm \
    -Wl,-Tlinker.ld
