
all: test.ihx cmdtool.ihx

ifeq ($(DEST), rom)
CMDTOOL_JMP_OBJ= jmp_table.rel
CODE_LOC=0
else
CMDTOOL_JMP_OBJ=
CODE_LOC=0x8000
endif

MEM_OPTS= --model-small --stack-auto

COMPILE_OPTS=  $(CPU_OPTS) $(MEM_OPTS) -I .
LINK_OPTS= $(MEM_OPTS)  --code-loc $(CODE_LOC)

TEST_OBJ= test.rel 
test.ihx: $(TEST_OBJ)
	sdcc $(LINK_OPTS) -o $@ $^

CMDTOOL_OBJ= cmdtool.rel ext_io.rel int_io.rel rtc.rel $(CMDTOOL_JMP_OBJ)
cmdtool.ihx: $(CMDTOOL_OBJ)
	sdcc $(LINK_OPTS) -o $@ $^


CPU_OPTS=  -DMICROCONTROLLER_SAB80517 # -DMCS51REG_ENABLE_WARNINGS
%.rel: %.c
	sdcc $(COMPILE_OPTS) -c $<

%.rel: %.asm
	sdas8051 -o -l $<

OBJ = $(TEST_OBJ) $(CMDTOOL_OBJ)
clean:
	-rm $(OBJ) 
	-rm test.ihx cmdtool.ihx
	-rm *.map *.mem *.rst *.lst
