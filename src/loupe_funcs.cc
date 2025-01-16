#include "loupe_funcs.h"

#include "display_image.h"
#include "gui_base.h"

int update_loupe_buf( const mdarray &src_img_buf,
		      int cen_x, int cen_y, int zoom_factor,
		      mdarray *dest_loupe_buf )
{
    const size_t src_width = src_img_buf.x_length();
    const size_t src_height = src_img_buf.y_length();
    size_t dest_width, dest_height;
    size_t i;
    int szt = src_img_buf.size_type();
    int ret_status = -1;

    if ( dest_loupe_buf == NULL ) goto quit;

    if ( zoom_factor < 3 ) zoom_factor = 3;
    else if ( 127 < zoom_factor ) zoom_factor = 127;
    if ( (zoom_factor % 2) == 0 ) zoom_factor ++;
    
    dest_width = dest_loupe_buf->x_length();
    dest_height = dest_loupe_buf->y_length();

    if ( dest_loupe_buf->size_type() != szt ) {
	dest_loupe_buf->convert(szt);
    }

    if ( szt == FLOAT_ZT ) {
	for ( i=0 ; i < 3 ; i++ ) {
	    size_t j;
	    const float *src_p = (const float *)src_img_buf.data_ptr(0,0,i);
	    for ( j=0 ; j < dest_height ; j++ ) {
		float *dest_p = (float *)dest_loupe_buf->data_ptr(0,j,i);
		int src_y;
		int j_adj = (int)j - (int)dest_height / 2;
		if ( 0 < j_adj ) j_adj += zoom_factor/2;
		else if ( j_adj < 0 ) j_adj -= zoom_factor/2;
		src_y = cen_y + j_adj / zoom_factor;
		size_t k;
		if ( 0 <= src_y && src_y < (int)src_height ) {
		    for ( k=0 ; k < dest_width ; k++ ) {
			int src_x;
			int k_adj = (int)k - (int)dest_width / 2;
			if ( 0 < k_adj ) k_adj += zoom_factor/2;
			else if ( k_adj < 0 ) k_adj -= zoom_factor/2;
			src_x = cen_x + k_adj / zoom_factor;
			if ( 0 <= src_x && src_x < (int)src_width ) 
			    dest_p[k] = src_p[src_width * src_y + src_x];
			else 
			    dest_p[k] = 0;
		    }
		}
		else {
		    for ( k=0 ; k < dest_width ; k++ ) dest_p[k] = 0;
		}
	    }
	}
    }
    else if ( szt == UCHAR_ZT ) {
	for ( i=0 ; i < 3 ; i++ ) {
	    size_t j;
	    const unsigned char *src_p = (const unsigned char *)src_img_buf.data_ptr(0,0,i);
	    for ( j=0 ; j < dest_height ; j++ ) {
		unsigned char *dest_p = (unsigned char *)dest_loupe_buf->data_ptr(0,j,i);
		int src_y;
		int j_adj = (int)j - (int)dest_height / 2;
		if ( 0 < j_adj ) j_adj += zoom_factor/2;
		else if ( j_adj < 0 ) j_adj -= zoom_factor/2;
		src_y = cen_y + j_adj / zoom_factor;
		size_t k;
		if ( 0 <= src_y && src_y < (int)src_height ) {
		    for ( k=0 ; k < dest_width ; k++ ) {
			int src_x;
			int k_adj = (int)k - (int)dest_width / 2;
			if ( 0 < k_adj ) k_adj += zoom_factor/2;
			else if ( k_adj < 0 ) k_adj -= zoom_factor/2;
			src_x = cen_x + k_adj / zoom_factor;
			if ( 0 <= src_x && src_x < (int)src_width ) 
			    dest_p[k] = src_p[src_width * src_y + src_x];
			else 
			    dest_p[k] = 0;
		    }
		}
		else {
		    for ( k=0 ; k < dest_width ; k++ ) dest_p[k] = 0;
		}
	    }
	}
    }

    ret_status = 0;
 quit:
    return ret_status;
}

int display_loupe( int win_cmd, int pos_x, int pos_y,
		   int refresh_loupe,
		   const mdarray &img_buf, int tiff_szt,
		   double ev_x, double ev_y,
		   int loupe_zoom, int circle_radius, int contrast_rgb[],
		   int display_ch, int gcfnc, mdarray *loupe_buf_p,
		   int *loupe_x_p, int *loupe_y_p,
		   mdarray_uchar *tmp_buf_loupe_p )
{
    const int this_y0 = pos_y;
    const int digit_y_pos = get_font_margin() + get_fontsize() - get_font_y_off() + 1;
    const int loupe_y_pos = get_font_margin() + 2*get_fontsize() + get_font_margin() - 1;
    const int cross_x = loupe_buf_p->x_length() / 2;
    const int cross_y = loupe_buf_p->y_length() / 2;
    const int hole = 4;	/* center hole of loupe */
    if ( refresh_loupe == 2 ) {
	*loupe_x_p = (int)ev_x;
	*loupe_y_p = (int)ev_y;
    }
    layer(win_cmd, 0, 2);
    copylayer(win_cmd, 1, 2);
    drawstr(win_cmd, get_font_margin(), this_y0 + digit_y_pos,
	    get_fontsize(), 0, "zoom=%d radius=%d", loupe_zoom, circle_radius);
    if ( *loupe_x_p != Loupe_pos_out ) {
	drawstr(win_cmd,
		get_font_margin() + 19 * (get_fontsize()/2), this_y0 + digit_y_pos,
		get_fontsize(), 0, "x=%d y=%d", *loupe_x_p, *loupe_y_p);
	drawstr(win_cmd,
		get_font_margin(),
		this_y0 + digit_y_pos + get_fontsize(),
		get_fontsize(), 0, "RGB=%5g %5g %5g",
		img_buf.dvalue(*loupe_x_p, *loupe_y_p, 0),
		img_buf.dvalue(*loupe_x_p, *loupe_y_p, 1),
		img_buf.dvalue(*loupe_x_p, *loupe_y_p, 2) );
	update_loupe_buf(img_buf, *loupe_x_p, *loupe_y_p, loupe_zoom,
			 loupe_buf_p);
	newgcfunction(win_cmd, gcfnc);
	// newgcfunction(win_cmd, GXcopy);
	display_image(win_cmd,
		      0, this_y0 + loupe_y_pos,
		      *loupe_buf_p, tiff_szt,
		      1, display_ch, contrast_rgb,
		      false, tmp_buf_loupe_p);
	newgcfunction(win_cmd, GXxor);
	newrgbcolor(win_cmd, 0x00,0xff,0x00);
	drawline(win_cmd, 0, this_y0 + loupe_y_pos + cross_y,
		 cross_x - hole, this_y0 + loupe_y_pos + cross_y);
	drawline(win_cmd, cross_x + hole, this_y0 + loupe_y_pos + cross_y,
		 loupe_buf_p->x_length() - 1, this_y0 + loupe_y_pos + cross_y);
	drawline(win_cmd, cross_x, this_y0 + loupe_y_pos + 0,
		 cross_x, this_y0 + loupe_y_pos + cross_y - hole);
	drawline(win_cmd, cross_x, this_y0 + loupe_y_pos + cross_y + hole,
		 cross_x, this_y0 + loupe_y_pos + loupe_buf_p->y_length() - 1);
	if ( 0 < circle_radius ) {
	    drawcirc(win_cmd, cross_x, this_y0 + loupe_y_pos + cross_y,
		     circle_radius * loupe_zoom, circle_radius * loupe_zoom);
	}
	newgcfunction(win_cmd, GXcopy);
	newrgbcolor(win_cmd, 0xff,0xff,0xff);
    }
    copylayer(win_cmd, 2, 0);

    return 0;
}

