# Aurora Human Interface Device (HID) Driver Makefile

OBJS = hid.o hid_asm.o

all: $(OBJS)

hid.o: hid.c
	$(CC) $(CFLAGS) -c hid.c -o hid.o

hid_asm.o: hid_asm.S
	$(AS) $(ASFLAGS) -c hid_asm.S -o hid_asm.o

clean:
	rm -f $(OBJS)

.PHONY: all clean
