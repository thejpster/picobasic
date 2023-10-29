VPATH = ../src ../BBCSDL/src ../BBCSDL/include
CXX = arm-none-eabi-gcc -Wall -I ../BBCSDL/include -mcpu=cortex-m0plus -DPICO

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o main.o

BINARY = bbcbasic.elf

all: $(BINARY)

.PHONY: all rebuild clean

clean:
	@rm $(OBJ) $(BINARY)

rebuild: clean all

run: $(BINARY)
	qemu-system-arm -machine mps3-an547 -cpu cortex-m55 -semihosting -serial stdio -kernel $<

main.o: main.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 $< -o $@

bbasmb.o: bbasmb_arm_v6m.c BBC.h
	$(CXX) -c -Os $< -o $@

bbdata.o: bbdata_arm_32.s
	$(CXX) -c $< -o $@

$(BINARY): $(OBJ)
	$(CXX) $^ -L. -lm -Wl,-T../linker.ld -o $@
