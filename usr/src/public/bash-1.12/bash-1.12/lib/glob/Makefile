# Makefile for glob source is in -*- text -*- mode.

CFLAGS = -g
RM = rm -f
OBJECTS = glob.o tilde.o fnmatch.o

# Here is a rule for making .o files from .c files that does not
# force the type of the machine (like -M_MACHINE) into the flags.
.c.o:
	$(RM) $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $*.c

libglob.a: $(OBJECTS)
	$(RM) -f $@
	$(AR) clq $@ $(OBJECTS)
	-if [ -f "$(RANLIB)" ]; then $(RANLIB) $@; fi

tilde-test: tilde.c
	$(CC) $(CFLAGS) -o tilde-test -DTEST tilde.c

fnmatch.o: fnmatch.h

clean:
	$(RM) *.o *.a
