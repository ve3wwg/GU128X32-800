CC	= gcc
CXX	= g++
STD	= -std=c++11
OPTS	= -Wall
DBG	= -O0 -g
INCL	= -I. -Iuguif/.
CFLAGS	= $(OPTS) $(DBG) $(INCL)
CXXFLAGS = $(STD) $(OPTS) $(DBG) $(INCL)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $*.o

.cpp.o:
	$(CXX) -c $(CFLAGS) $< -o $*.o

ugui.o:	CFLAGS += -Wno-parentheses

OBJS	= gp.o vfd.o

all:	libuguif.a $(OBJS)
	$(CXX) $(OBJS) -L. -luguif -o vfd
	sudo chown root ./vfd
	sudo chmod u+s ./vfd

libuguif.a: uguif/ugui.o
	ar cr libuguif.a uguif/ugui.o

uguif/ugui.o: CFLAGS += -Wno-parentheses

clean:
	rm -f *.o core .errs.t uguif/*.o

clobber: clean
	rm -f vfd *.a
