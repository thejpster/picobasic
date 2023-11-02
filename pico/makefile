VPATH = ../src ../BBCSDL/src ../BBCSDL/include
CXX = arm-none-eabi-gcc -Wall -O1 -g -fshort-enums -I ../BBCSDL/include -mcpu=cortex-m0plus -DNEOTRON

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
	$(CXX) -c $< -o $@

bbmain.o: bbmain.c
	$(CXX) -c $< -o $@

bbexec.o: bbexec.c
	$(CXX) -c -Wno-discarded-qualifiers $< -o $@

bbeval.o: bbeval.c
	$(CXX) -Wno-array-bounds -c $< -o $@

bbasmb.o: bbasmb_arm_v6m.c
	$(CXX) -c $< -o $@

bbdata.o: bbdata_arm_32.s
	$(CXX) -c $< -o $@

$(BINARY): $(OBJ)
	$(CXX) $^ -L. -lm -Wl,-T../linker.ld -nostartfiles -o $@
