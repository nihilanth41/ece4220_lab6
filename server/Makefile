IDIR=../include
CC=/usr/local/opt/crosstool/arm-linux/gcc-3.3.4-glibc-2.3.2/bin/arm-linux-gcc
#CC=gcc 
CFLAGS=-I$(IDIR) #-I../../includes/realtime/include -I../../includes/src/linux24/include 
#CFLAGS+= -Wall -Wextra -Wshadow -g#-Werror -Wextra -Wshadow

ODIR=obj

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = lab6.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

lab6: $(OBJ)
	$(CC) -o $@.bin $^ $(CFLAGS) $(LDLIBS)

.PHONY: clean

clean:
	rm -rf $(ODIR)/*.o *~ core $(INCDIR)/*~


