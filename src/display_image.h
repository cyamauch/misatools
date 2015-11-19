#ifndef _DISPLAY_IMAGE_H
#define _DISPLAY_IMAGE_H 1


#include <unistd.h>
#include <sli/mdarray.h>

const double Contrast_scale = 2.0;

int display_image( int win_image, double disp_x, double disp_y,
		   const sli::mdarray &img_buf,
		   int tiff_sztype,
		   int binning,		/* 1: original scale  2:1/2 -2:2x */ 
		   int display_ch,	/* 0:RGB 1:R 2:G 3:B */
		   const int contrast_rgb[],
		   bool needs_resize_win,
		   sli::mdarray_uchar *tmp_buf );

int get_bin_factor_for_display( size_t img_width, size_t img_height,
				bool fast_only );

bool fix_zoom_factor( int *display_bin_p, size_t img_width, size_t img_height );

bool minus_binning_with_limit( int *display_bin_p, size_t img_width, size_t img_height );

int plus_binning_with_limit( int *display_bin_p );

int load_display_params( const char *filename, int contrast_rgb[] );

int save_display_params( const char *filename, const int contrast_rgb[] );


#endif	/* DISPLAY_IMAGE_H */
