#ifndef _LOUPE_FUNCS_H
#define _LOUPE_FUNCS_H 1

#include <unistd.h>
#include <sli/mdarray.h>
#include <eggx.h>
using namespace sli;

const int Loupe_pos_out = -32000;

int update_loupe_buf( const sli::mdarray &src_img_buf,
		      int cen_x, int cen_y, int zoom_factor,
		      sli::mdarray *dest_loupe_buf );

int display_loupe( int win_cmd, int pos_x, int pos_y,
	   int refresh_loupe,
	   const sli::mdarray &img_buf, int tiff_szt,
	   double ev_x, double ev_y,
	   int loupe_zoom, int circle_radius, int contrast_rgb[],
	   int display_ch, int gcfnc, sli::mdarray *loupe_buf_p,
	   int *loupe_x_p, int *loupe_y_p,
	   sli::mdarray_uchar *tmp_buf_loupe_p );

#endif	/* _LOUPE_FUNCS_H */
