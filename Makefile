TARGET = simulator

CC = g++
CFLAGS = -pthread -Wall -Wextra -Wsign-conversion -Wpointer-arith -Wcast-qual -Wwrite-strings -Wmissing-prototypes #-Wshadow 
DEBFLAGS = -g

SRCEXTS = .cc
HDREXTS = .h

SRCDIR = .
INCDIR = $(SRCDIR)
BINDIR = .

SRC := $(wildcard $(SRCDIR)/*$(SRCEXTS))
INC := $(wildcard $(INCDIR)/*$(HDREXTS))

.PHONY: all
all: $(BINDIR)/$(TARGET)

.PHONY: debug
debug: CFLAGS += $(DEBFLAGS)
debug: $(BINDIR)/$(TARGET)

.PHONY: clean
clean:
	@rm -f $(BINDIR)/$(TARGET)
	@rm -rf $(BINDIR)/$(TARGET).dSYM

$(BINDIR)/$(TARGET): $(SRC) $(INC)
	@mkdir -p $(BINDIR)
	@$(CC) $(CFLAGS) $(SRC) -o $@