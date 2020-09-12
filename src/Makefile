

DESTDIR =

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

########

OBJS = max_memory view_images make_dark make_flat merge_flat proc_images align_center stack_images align_rgb determine_sky pseudo_sky make_sky denoise_images

all:: $(OBJS)

clean::
	rm -f $(OBJS) *.exe

CMD = s++ -msse3 -mssse3 -DUSE_SIMD

max_memory: max_memory.c
	gcc -Wall -O max_memory.c -o max_memory

view_images: view_images.cc read_tiff24or48.cc display_image.cc
	$(CMD) view_images.cc -leggx -lX11 -ltiff

make_dark: make_dark.cc read_tiff24or48.cc write_tiff24or48.cc
	$(CMD) make_dark.cc -ltiff

make_flat: make_flat.cc read_tiff24or48_to_float.cc write_float_to_tiff24or48.cc
	$(CMD) make_flat.cc -ltiff

merge_flat: merge_flat.cc read_tiff24or48_to_float.cc write_float_to_tiff24or48.cc
	$(CMD) merge_flat.cc -ltiff

proc_images: proc_images.cc read_tiff24or48_to_float.cc write_float_to_tiff24or48.cc
	$(CMD) proc_images.cc -ltiff

align_center: align_center.cc read_tiff24or48.cc write_tiff24or48.cc
	$(CMD) align_center.cc -ltiff

stack_images: stack_images.cc read_tiff24or48_to_float.cc write_float_to_tiff48.cc display_image.cc
	$(CMD) stack_images.cc -leggx -lX11 -ltiff

align_rgb: align_rgb.cc read_tiff24or48_separate_buffer.cc write_float_to_tiff24or48.cc display_image.cc
	$(CMD) align_rgb.cc -leggx -lX11 -ltiff

determine_sky: determine_sky.cc display_image.cc read_tiff24or48.cc write_tiff24or48.cc
	$(CMD) determine_sky.cc -leggx -lX11 -ltiff

pseudo_sky:	pseudo_sky.cc read_tiff24or48_to_float.cc write_float_to_tiff48.cc display_image.cc
	$(CMD) pseudo_sky.cc -leggx -lX11 -ltiff

make_sky: make_sky.cc read_tiff24or48.cc write_tiff24or48.cc
	$(CMD) make_sky.cc -ltiff

denoise_images:	denoise_images.cc read_tiff24or48_to_float.cc write_float_to_tiff24or48.cc
	$(CMD) denoise_images.cc -ltiff

install:: $(OBJS)
	sh install-sh -m 755 $(OBJS) copy_classified $(DESTDIR)$(BINDIR)
