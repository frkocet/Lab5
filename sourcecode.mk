SHELL=cmd
CC=c51
COMPORT = $(shell type COMPORT.inc)
OBJS=sourcecode.obj

sourcecode.hex: $(OBJS)
	$(CC) $(OBJS)
	@echo Done!
	
sourcecode.obj: sourcecode.c
	$(CC) -c sourcecode.c

clean:
	@del $(OBJS) *.asm *.lkr *.lst *.map *.hex *.map 2> nul

LoadFlash:
	@Taskkill /IM putty.exe /F 2>NUL | wait 500
	EFM8_prog.exe -ft230 -r sourcecode.hex
	cmd /c start putty -serial $(COMPORT) -sercfg 115200,8,n,1,N

putty:
	@Taskkill /IM putty.exe /F 2>NUL | wait 500
	cmd /c start putty -serial $(COMPORT) -sercfg 115200,8,n,1,N

make: $(OBJS)
	$(CC) $(OBJS)
	@Taskkill /IM putty.exe /F 2>NUL | wait 500
	EFM8_prog.exe -ft230 -r sourcecode.hex
	cmd /c start putty -serial $(COMPORT) -sercfg 115200,8,n,1,N

explorer:
	cmd /c start explorer .
