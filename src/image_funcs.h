#ifndef _IMAGE_FUNCS_H
#define _IMAGE_FUNCS_H 1

#include <unistd.h>
#include <sli/mdarray.h>

int scale_image( int scale, sli::mdarray *img_io );

int bin_image( int factor, sli::mdarray *img_io, int sztype );

#endif	/* _IMAGE_FUNCS_H */
