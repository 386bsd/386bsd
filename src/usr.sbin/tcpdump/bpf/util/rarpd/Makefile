
DEFINES = -DTFTP_DIR=\"/tftpboot\"
PROG = rarpd

CFLAGS = -O $(DEFINES)
CC = gcc -g

${PROG}: ${PROG}.c
	$(CC) $(CFLAGS) -o $@ $(PROG).c

install: ${PROG}
	install ${PROG} /usr/etc

lint:
	lint ${PROG}.c

clean:
	rm -f *.o core ${PROG}
