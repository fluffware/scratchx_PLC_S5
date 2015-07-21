
OBJ= bootloader.rel 
firmware.ihx: $(OBJ)
	sdcc -o $@ $<
CPU_DEFS=  -DMICROCONTROLLER_SAB80517 # -DMCS51REG_ENABLE_WARNINGS
MEM_DEFS= --model-small --stack-auto
%.rel: %.c
	sdcc  $(CPU_DEFS) $(MEM_DEFS) -c $<


clean:
	-rm $(OBJ) 
	-rm firmware.ihx 
	-rm *.map *.mem *.rst *.lst

