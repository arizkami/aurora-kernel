# Aurora PCI/PCIe Bus Driver Makefile

OBJS = pci.o

all: $(OBJS)

pci.o: pci.c
	$(CC) $(CFLAGS) -c pci.c -o pci.o

clean:
	rm -f $(OBJS)

.PHONY: all clean