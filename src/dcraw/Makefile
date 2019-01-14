
DESTDIR =

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

########

OBJS = dcraw raw2deadpixels raw2preview raw2tiff

all:: $(OBJS)

clean::
	rm -f $(OBJS) *.exe

CMD = gcc

dcraw: dcraw.c
	$(CMD) -o dcraw -O4 dcraw.c -lm -DNODEPS

raw2preview: raw2preview.c
	$(CMD) -Wall -o raw2preview -O raw2preview.c -ltiff

raw2tiff: raw2tiff.c
	$(CMD) -Wall -o raw2tiff -O raw2tiff.c -ltiff

raw2deadpixels: raw2deadpixels.c
	$(CMD) -Wall -o raw2deadpixels -O raw2deadpixels.c -lm

install:: $(OBJS)
	sh install-sh -m 755 $(OBJS) $(DESTDIR)$(BINDIR)
