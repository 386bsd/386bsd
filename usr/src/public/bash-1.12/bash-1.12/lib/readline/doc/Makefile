# This makefile for Readline library documentation is in -*- text -*- mode.
# Emacs likes it that way.

DVIOBJ = readline.dvi history.dvi
INFOBJ = readline.info history.info

all: $(DVIOBJ) $(INFOBJ)

readline.dvi: texindex rlman.texinfo rluser.texinfo rltech.texinfo
	tex rlman.texinfo
	./texindex rlman.??
	tex rlman.texinfo
	mv rlman.dvi readline.dvi

history.dvi: texindex hist.texinfo hsuser.texinfo hstech.texinfo
	tex hist.texinfo
	tex hist.texinfo
	mv hist.dvi history.dvi

readline.info: rlman.texinfo rluser.texinfo rltech.texinfo
	makeinfo rlman.texinfo

history.info: hist.texinfo hsuser.texinfo hstech.texinfo
	makeinfo hist.texinfo

texindex: texindex.o
	$(CC) -o $@ $(LDFLAGS) $(CFLAGS) $?
texindex.o: texindex.c

clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.fns *.kys *.tps *.vrs *.o core texindex

squeaky-clean:
	rm -f *.aux *.cp *.fn *.ky *.log *.pg *.toc *.tp *.vr *.cps *.pgs \
	      *.dvi *.info *.info-* *.fns *.kys *.tps *.vrs *.o core texindex
