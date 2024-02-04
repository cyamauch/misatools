
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
	$(CMD) -O4 dcraw.c -o dcraw -lm -DNODEPS

raw2preview: raw2preview.c
	$(CMD) -O -Wall -DUSE_SIMD raw2preview.c -o raw2preview -ltiff 

raw2tiff: raw2tiff.c
	$(CMD) -O -Wall -DUSE_SIMD raw2tiff.c -o raw2tiff -ltiff

raw2deadpixels: raw2deadpixels.c
	$(CMD) -O -Wall raw2deadpixels.c -o raw2deadpixels -lm

install:: $(OBJS)
	sh install-sh -m 755 $(OBJS) $(DESTDIR)$(BINDIR)
