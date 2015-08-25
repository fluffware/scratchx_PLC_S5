
all: cmdtool.ihx

ifeq ($(DEST), rom)
CMDTOOL_DEST_OBJ= jmp_table.rel rom_interrupt.rel
CODE_LOC=0
DEST_DEFS=-DDEST_ROM
else
CMDTOOL_DEST_OBJ= 
CODE_LOC=0x8000
DEST_DEFS=-DDEST_RAM
endif

VPATH=../common

MEM_OPTS= --model-small --stack-auto

COMPILE_OPTS=  $(CPU_OPTS) $(MEM_OPTS) -I . -I ../common -I ../contiki $(DEST_DEFS)
LINK_OPTS= $(MEM_OPTS)  --code-loc $(CODE_LOC)


CMDTOOL_OBJ= cmdtool.rel ext_io.rel int_io.rel rtc.rel zmodem.rel crc.rel ihex.rel $(CMDTOOL_DEST_OBJ)
cmdtool.ihx: $(CMDTOOL_OBJ)
	sdcc $(LINK_OPTS) -o $@ $^


CPU_OPTS=  -DMICROCONTROLLER_SAB80517 # -DMCS51REG_ENABLE_WARNINGS
%.rel: %.c
	sdcc $(COMPILE_OPTS) -c $<

%.rel: %.asm
	sdas8051 -o -l $<

OBJ = $(CMDTOOL_OBJ)
clean:
	-rm $(OBJ) 
	-rm cmdtool.ihx
	-rm *.map *.mem *.rst *.lst *.sym *.lk

