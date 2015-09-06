
all: scratch_server.ihx

ifeq ($(DEST), rom)
SCRATCH_SERVER_DEST_OBJ= 
CODE_LOC=0
DEST_DEFS=-DDEST_ROM
else
SCRATCH_SERVER_DEST_OBJ= 
CODE_LOC=0x8000
DEST_DEFS=-DDEST_RAM
endif

VPATH=../common

MEM_OPTS= --model-small --stack-auto

COMPILE_OPTS=  $(CPU_OPTS) $(MEM_OPTS) -I . -I ../common -I ../contiki -I ../../include $(DEST_DEFS)
LINK_OPTS= $(MEM_OPTS)  --code-loc $(CODE_LOC)


SCRATCH_SERVER_OBJ= scratch_server.rel ext_io.rel int_io.rel rtc.rel serial.rel adc.rel dac.rel crc8.rel $(SCRATCH_SERVER_DEST_OBJ)
scratch_server.ihx: $(SCRATCH_SERVER_OBJ)
	sdcc $(LINK_OPTS) -o $@ $^


CPU_OPTS=  -DMICROCONTROLLER_SAB80517 # -DMCS51REG_ENABLE_WARNINGS
%.rel: %.c
	sdcc $(COMPILE_OPTS) -c $<

%.rel: %.asm
	sdas8051 -o -l $<

OBJ = $(SCRATCH_SERVER_OBJ)
clean:
	-rm $(OBJ) 
	-rm scratch_server.ihx
	-rm *.map *.mem *.rst *.lst *.sym *.lk

