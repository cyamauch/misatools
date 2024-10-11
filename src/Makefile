
CC      = gcc
CFLAGS  = -O2 -Wall
CDEFS   = 

CCC     = s++
CCFLAGS = -O2 -Wall -msse3 -mssse3
CCDEFS  = -DUSE_SIMD

DESTDIR =

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

########

OBJS = max_memory view_images make_dark make_flat merge_flat proc_images align_center stack_images align_rgb determine_sky pseudo_sky make_sky denoise_images

all:: $(OBJS)

clean::
	rm -f $(OBJS) *.o *.exe

.cc.o:   ; $(CCC) $(CCFLAGS) $(CCDEFS) -c $*.cc

max_memory: max_memory.c
	$(CC) $(CFLAGS) $(CDEFS) max_memory.c -o max_memory

view_images: view_images.cc file_io.o tiff_funcs.o display_image.o gui_base.o loupe_funcs.o
	$(CCC) view_images.cc file_io.o tiff_funcs.o display_image.o gui_base.o loupe_funcs.o -leggx -lX11 -ltiff

make_dark: make_dark.cc tiff_funcs.o
	$(CCC) make_dark.cc tiff_funcs.o -ltiff

make_flat: make_flat.cc tiff_funcs.o
	$(CCC) make_flat.cc tiff_funcs.o -ltiff

merge_flat: merge_flat.cc tiff_funcs.o
	$(CCC) merge_flat.cc tiff_funcs.o -ltiff

proc_images: proc_images.cc tiff_funcs.o
	$(CCC) proc_images.cc tiff_funcs.o -ltiff

align_center: align_center.cc tiff_funcs.o image_funcs.o
	$(CCC) align_center.cc tiff_funcs.o image_funcs.o -ltiff

stack_images: stack_images.cc tiff_funcs.o display_image.o gui_base.o loupe_funcs.o
	$(CCC) stack_images.cc tiff_funcs.o display_image.o gui_base.o loupe_funcs.o -leggx -lX11 -ltiff

align_rgb: align_rgb.cc tiff_funcs.o display_image.o gui_base.o
	$(CCC) align_rgb.cc tiff_funcs.o display_image.o gui_base.o -leggx -lX11 -ltiff

determine_sky: determine_sky.cc tiff_funcs.o display_image.o
	$(CCC) determine_sky.cc tiff_funcs.o display_image.o -leggx -lX11 -ltiff

pseudo_sky:	pseudo_sky.cc tiff_funcs.o display_image.o gui_base.o
	$(CCC) pseudo_sky.cc tiff_funcs.o display_image.o gui_base.o -leggx -lX11 -ltiff

make_sky: make_sky.cc tiff_funcs.o
	$(CCC) make_sky.cc tiff_funcs.o -ltiff

denoise_images:	denoise_images.cc tiff_funcs.o
	$(CCC) denoise_images.cc tiff_funcs.o -ltiff

install:: $(OBJS)
	sh install-sh -m 755 $(OBJS) copy_classified $(DESTDIR)$(BINDIR)
