PORT=/dev/ttyUSB0
CPU=attiny84

all: bootloader.hex

bootloader.elf: bootloader.S
	avr-gcc -g -mmcu=attiny84 -O0 -Xassembler --gdwarf-2 -nostartfiles -nodefaultlibs -nostdlib -o bootloader.elf bootloader.S
	avr-objdump -d $@ >$@.dump
	avr-size $@

bootloader.hex: bootloader.elf
#	avr-objcopy -O ihex $< $@

#   Vynechame nulove radky
	avr-objcopy -O ihex --gap-fill=0xFF $< x.tmp 
	cat x.tmp | grep -v '00000000000000000000000000000000000' >$@
	rm x.tmp


arduino: bootloader.hex eeprom.hex
	avrdude -p t84a -c avrisp -P $(PORT) -b 9600 -U lfuse:w:0xe2:m -U hfuse:w:0xdf:m -U efuse:w:0xfe:m
	avrdude -p t84a -c avrisp -P $(PORT) -b 9600 -U flash:w:bootloader.hex:i

clean:
	rm -f *.elf *.hex *.bin *.o *.dump
