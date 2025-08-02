CPU=attiny84
TARGET=tn84a2
OWL_PATH=/home/martin/CLionProjects/senzor/owl

BOOTSYM = DEVL$(ID)

SRC = main.c main.S common.S sender.S encrypt.S am2302.S rx.c
F_CPU=8000000

all: main.hex

main.elf: $(SRC) config.h
	avr-gcc -g -DF_CPU=$(F_CPU)UL -mmcu=attiny84 -Os -o $@ $(SRC)
	avr-objdump -d $@ >main.dump
	avr-size $@

%.hex: %.elf
	avr-objcopy -O ihex $< $@

deploy: main.hex
	# switch to bootloader
	echo -n "BOOT" | ncat 192.168.15.197 9998
	
	sleep 6

	# do flash update
	cat main.hex | ../../uploader/build/uploader -f

clean:
	rm -f *.elf *.hex *.bin *.owl *.o *.dump
