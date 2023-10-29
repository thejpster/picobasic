VPATH = ../src ../BBCSDL/src ../BBCSDL/include
CXX = arm-none-eabi-gcc -Wall -g -fshort-enums -I ../BBCSDL/include -mcpu=cortex-m0plus -DPICO

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o main.o

BINARY = bbcbasic.elf

.PHONY: all rebuild clean debug

all: $(BINARY)

clean:
	@rm -f $(OBJ) $(BINARY)

rebuild: clean all

run: $(BINARY)
	qemu-system-arm -machine mps3-an547 -cpu cortex-m55 -semihosting -serial stdio -kernel $<

debug: $(BINARY)
	qemu-system-arm -machine mps3-an547 -cpu cortex-m55 -semihosting -serial stdio -s -S -kernel $<

main.o: main.c
	$(CXX) -Os -c $< -o $@

bbmain.o: bbmain.c
	$(CXX) -O2 -c $< -o $@

bbexec.o: bbexec.c
	$(CXX) -O2 -c -Wno-discarded-qualifiers $< -o $@

bbeval.o: bbeval.c
	$(CXX) -O2 -Wno-array-bounds -c $< -o $@

bbasmb.o: bbasmb_arm_v6m.c
	$(CXX) -O2 -c $< -o $@

bbdata.o: bbdata_arm_32.s
	$(CXX) -c $< -o $@

$(BINARY): $(OBJ)
	$(CXX) $^ -L. -lm -Wl,-T../linker.ld -nostartfiles -o $@
