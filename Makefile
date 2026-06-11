CXX = gcc
CXXFLAGS = -Wall -Wextra -g
TARGET = procgen
SRC = procgen.c

# =========================
# Kernel module
# =========================
obj-m += producer_consumer.o

KDIR := /usr/src/linux
PWD  := $(shell pwd)

# =========================
# Build everything
# =========================
all: $(TARGET) module

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm -f $(TARGET)
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean module