/*
 * $ s++ pseudo_sky.cc -leggx -lX11 -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/pipestreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_math.h>
#include <sli/mdarray_statistics.h>
#include <eggx.h>
#include <unistd.h>
using namespace sli;

const double Contrast_scale = 2.0;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff48.h"
#include "write_float_to_tiff.h"
#include "display_image.cc"
#include "load_display_params.cc"
#include "save_display_params.cc"
#include "get_bin_factor_for_display.c"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   pseudo_images.cc
 * @brief  interactive tool to create pseudo sky image
 *         8/16-bit integer and 32-bit float images are supported.
 */

const int Font_y_off = 3;
const int Font_margin = 2;
static int Fontsize = 14;
#include "set_fontsize.c"
#include "command_window.c"

const int Max_skypoint_box_size = 255;
static int Skypoint_box_size = 31;

typedef struct _sky_point {
    long x;
    long y;
    double red;
    double green;
    double blue;
    int sticky;
} sky_point;

/* for qsort() arg */
static int compar_fnc( const void *_a, const void *_b )
{
    const sky_point *a = (const sky_point *)_a;
    const sky_point *b = (const sky_point *)_b;
    /* sort sky_point list order by x => y */
    if ( a->x < b->x ) return -1;
    else if ( b->x < a->x ) return 1;
    else if ( a->y < b->y ) return -1;
    else if ( b->y < a->y ) return 1;
    else return 0;
}

/* sort Sky Point List order by x => y */
static int sort_sky_point_list( mdarray *sky_point_list_p )
{
    sky_point *target = (sky_point *)(sky_point_list_p->data_ptr());
    size_t n_list = sky_point_list_p->length();
    
    /* sort */
    qsort((void *)target, n_list, sizeof(*target), &compar_fnc);

    return 0;
}

/* search a sky point in sky_point_list exactly */
static long search_sky_point( const mdarray_float &target_img_buf,
			      const mdarray &sky_point_list,
			      long p_x, long p_y )
{
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    size_t i;
    long return_idx = -1;

    /* Search ... */
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	if ( p_x == p_list[i].x && p_y == p_list[i].y ) {
	    return_idx = i;
	    break;
	}
    }
    
    return return_idx;
}


/* fix (x,y) of point at near edge of an image [box] */
static int fix_points_box( const mdarray_float &target_img_buf,
				 long point_x, long point_y,
				 long *actual_x_ret, long *actual_y_ret )
{
    const long offset = Skypoint_box_size / 2;
    const long x_len = target_img_buf.x_length();
    const long y_len = target_img_buf.y_length();
    
    /* for edge ... */
    if ( point_x < offset ) point_x = offset;
    else if ( x_len < point_x + offset + 1 ) point_x = x_len - 1 - offset;
    
    if ( point_y < offset ) point_y = offset;
    else if ( y_len < point_y + offset + 1 ) point_y = y_len - 1 - offset;

    if ( actual_x_ret != NULL ) *actual_x_ret = point_x;
    if ( actual_y_ret != NULL ) *actual_y_ret = point_y;
    
    return 0;
}

/* search a sky point in sky_point_list around (ev_x, ev_y) */
static long search_sky_point_box( const mdarray_float &target_img_buf,
			      const mdarray &sky_point_list,
			      long ev_x, long ev_y )
{
    const long offset = Skypoint_box_size / 2;
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    size_t i;
    double distance = 65536;
    long return_idx = -1;
    
    /* Search ... */
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	long point_x = p_list[i].x;
	long point_y = p_list[i].y;
	fix_points_box( target_img_buf, point_x, point_y,
			  &point_x, &point_y );
	if ( point_x - offset <= ev_x && ev_x <= point_x + offset &&
	     point_y - offset <= ev_y && ev_y <= point_y + offset ) {
	    double dt =
		sqrt( (p_list[i].x - ev_x)*(p_list[i].x - ev_x) +
		      (p_list[i].y - ev_y)*(p_list[i].y - ev_y) );
	    if ( dt < distance ) {
		distance = dt;
		return_idx = i;
	    }
	}
    }
    
    return return_idx;
}

/* get median at a point [box] */
static int get_box_median( const mdarray_float &target_img_buf,
			   long point_x, long point_y,
			   double median_rgb[] )
{
    const long offset = Skypoint_box_size / 2;
    mdarray_float sample(false);
    int return_status = -1;

    sample.resize_2d(Skypoint_box_size, Skypoint_box_size);

    fix_points_box( target_img_buf, point_x, point_y,
		    &point_x, &point_y );
    
    /* R */
    target_img_buf.copy(&sample,
	       point_x - offset, Skypoint_box_size,  point_y - offset, Skypoint_box_size,  0, 1);
    if ( median_rgb != NULL ) median_rgb[0] = md_median(sample);
    //sample.dprint();

    /* G */
    target_img_buf.copy(&sample,
	       point_x - offset, Skypoint_box_size,  point_y - offset, Skypoint_box_size,  1, 1);
    if ( median_rgb != NULL ) median_rgb[1] = md_median(sample);
    //sample.dprint();

    /* B */
    //sample.paste(target_img_buf,
    //		offset - point_x, offset - point_y, -2);
    //sample.dprint();
    target_img_buf.copy(&sample,
	       point_x - offset, Skypoint_box_size,  point_y - offset, Skypoint_box_size,  2, 1);
    //sample.dprint();
    if ( median_rgb != NULL ) median_rgb[2] = md_median(sample);
    
    return_status = 0;

    return return_status;
}

static int delete_sky_point( mdarray *sky_point_list_p,
			     long point_idx, bool v_all )
{
    //stdstreamio sio;

    sky_point *p;

    int return_status = -1;

    p = (sky_point *)sky_point_list_p->data_ptr();

    if ( point_idx < 0 ) goto quit;

    if ( v_all == false ) {
	if ( p[point_idx].sticky == 0 ) {
	    sky_point_list_p->erase(point_idx,1);
	    p = (sky_point *)sky_point_list_p->data_ptr();
	    return_status = 0;
	}
    }
    else {
	size_t i;
	long pos_x = p[point_idx].x;
	size_t len = sky_point_list_p->length();
	//sio.eprintf("[DEBUG] pos_x=%ld\n", pos_x);
	for ( i=0 ; i < len ; i++ ) {
	    //sio.eprintf("[DEBUG] p[%ld].x=%ld st=%d\n", (long)i, p[i].x, p[i].sticky);
	    if ( p[i].x == pos_x && 1 < p[i].sticky ) {
		goto quit;		/* cannot delete */
	    }
	}
	for ( i=len ; 0 < i ; ) {
	    i--;
	    if ( p[i].x == pos_x ) {
		sky_point_list_p->erase(i,1);
		p = (sky_point *)sky_point_list_p->data_ptr();
		return_status = 0;
	    }
	}
	
    }

 quit:
    return return_status;
}

static int append_sky_point( const mdarray_float &img,
			     const mdarray_float &sky, bool use_img,
			     long x, long y, int sticky,
			     mdarray *sky_point_list_p )
{
    sky_point *p;
    size_t len = sky_point_list_p->length();
    double val_rgb[3];

    if ( use_img == true ) {
	get_box_median(img, x, y, val_rgb);
    }
    else if ( 0 < sky.length() && sky.length() == img.length() ) {
	val_rgb[0] = sky.dvalue(x,y,0);
	val_rgb[1] = sky.dvalue(x,y,1);
	val_rgb[2] = sky.dvalue(x,y,2);
    }
    else {
	get_box_median(img, x, y, val_rgb);
    }
    
    sky_point_list_p->resize(len + 1);
    p = (sky_point *)sky_point_list_p->data_ptr();

    p[len].x = x;
    p[len].y = y;
    p[len].red   = val_rgb[0];
    p[len].green = val_rgb[1];
    p[len].blue  = val_rgb[2];
    p[len].sticky = sticky;

    return 0;
}

static int update_sky_point_val_pm( int rgb_ch,
				 long point_idx, int skylv_incdecl, bool plus,
				 mdarray *sky_point_list_p )
{
    sky_point *p_list = (sky_point *)(sky_point_list_p->data_ptr());
    long len = sky_point_list_p->length();

    if ( 0 <= point_idx && point_idx < len ) {
	double pm;
	
	if ( plus == true ) pm = pow(2,skylv_incdecl);
	else pm = -pow(2,skylv_incdecl);
    
	if ( rgb_ch == 0 ) {
	    p_list[point_idx].red += pm;
	    if ( p_list[point_idx].red < 0.0 ) p_list[point_idx].red = 0.0;
	    p_list[point_idx].green += pm;
	    if ( p_list[point_idx].green < 0.0 ) p_list[point_idx].green = 0.0;
	    p_list[point_idx].blue += pm;
	    if ( p_list[point_idx].blue < 0.0 ) p_list[point_idx].blue = 0.0;
	}
	else if ( rgb_ch == 1 ) {
	    p_list[point_idx].red += pm;
	    if ( p_list[point_idx].red < 0.0 ) p_list[point_idx].red = 0.0;
	}
	else if ( rgb_ch == 2 ) {
	    p_list[point_idx].green += pm;
	    if ( p_list[point_idx].green < 0.0 ) p_list[point_idx].green = 0.0;
	}
	else if ( rgb_ch == 3 ) {
	    p_list[point_idx].blue += pm;
	    if ( p_list[point_idx].blue < 0.0 ) p_list[point_idx].blue = 0.0;
	}

	return 0;
    }
    else {
	return -1;
    }

}

static int update_sky_point_val( int rgb_ch,
				 long point_idx, double val[],
				 mdarray *sky_point_list_p )
{
    sky_point *p_list = (sky_point *)(sky_point_list_p->data_ptr());
    long len = sky_point_list_p->length();

    if ( 0 <= point_idx && point_idx < len ) {
    
	if ( rgb_ch == 0 ) {
	    p_list[point_idx].red = val[0];
	    p_list[point_idx].green = val[1];
	    p_list[point_idx].blue = val[2];
	}
	else if ( rgb_ch == 1 ) {
	    p_list[point_idx].red = val[0];
	}
	else if ( rgb_ch == 2 ) {
	    p_list[point_idx].green = val[1];
	}
	else if ( rgb_ch == 3 ) {
	    p_list[point_idx].blue = val[2];
	}

	return 0;
    }
    else {
	return -1;
    }

}

static int draw_sky_points( int win_image,
			    const mdarray_float &target_img_buf,
			    const mdarray &sky_point_list, long selected )
{
    const long offset = Skypoint_box_size / 2;
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    stdstreamio sio;
    size_t i;

    newcolor(win_image, "green");
   
    /* Search ... */
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	long point_x = p_list[i].x;
	long point_y = p_list[i].y;
	//sio.printf("x,y = %ld,%ld\n",p_list[i].x,p_list[i].y);
	fix_points_box( target_img_buf, point_x, point_y,
			  &point_x, &point_y );
	if ( (long)i == selected ) {
	    newcolor(win_image, "red");
	    drawrect(win_image, point_x - offset, point_y - offset,
		     2 * offset + 1, 2 * offset + 1);
	    newcolor(win_image, "green");
	}
	else {
	    drawrect(win_image, point_x - offset, point_y - offset,
		     2 * offset + 1, 2 * offset + 1);
	}
#if 1
	if ( 0 < i && p_list[i-1].x == p_list[i].x ) {
	    newlinestyle(win_image, LineOnOffDash);
	    drawline(win_image, p_list[i-1].x, p_list[i-1].y,
		     p_list[i].x, p_list[i].y);
	    newlinestyle(win_image, LineSolid);
	    /*
	    sio.printf("  x,y,x,y = %ld,%ld,%ld,%ld\n",
		       p_list[i-1].x, p_list[i-1].y,
		       p_list[i].x, p_list[i].y);
	    */
	}
#endif
    }
    
    return 0;
}

/* Least-squares method (quadratic):  a + b*x + c*x^2 */
void minsq_quadratic( int n, double x[], double y[],
		      double *a, double *b, double *c )
{
    int i ;
    double sx=0, sy=0, sxx=0, sxy=0 ;
    double sxxy=0, sxxx=0, sxxxx=0, k ;

    for ( i=0 ; i < n ; i++ ) {
	sx += x[i] ;
	sy += y[i] ;
	sxx += x[i]*x[i] ;
	sxy += x[i]*y[i] ;
	sxxy += x[i]*x[i]*y[i] ;
	sxxx += pow(x[i],3) ;
	sxxxx += pow(x[i],4) ;
    }

    k = (n*sxx-sx*sx)/(sx*sxx-n*sxxx) ;
    (*c) = n*sxy-sx*sy+n*k*sxxy-k*sy*sxx ;
    (*c) /= (k*n*sxxxx-k*sxx*sxx+n*sxxx-sxx*sx) ;
    (*b) = n*sxxy-(*c)*(n*sxxxx-sxx*sxx)-sxx*sy ;
    (*b) /= (n*sxxx-sxx*sx) ;
    (*a) = (sy-(*b)*sx-(*c)*sxx)/n ;

    return ;
}

static int draw_vertical_linear( const sky_point *p_list,
				 size_t idx_begin, size_t n_section_v,
				 mdarray_float *sky_img_buf_p )
{
    long img_width = sky_img_buf_p->x_length();
    size_t ii, i = idx_begin;
    
    for ( ii=0 ; ii < n_section_v ; ii++ ) {
	double p_list_rgb[3];
	double p_list_rgb_1[3];
	long y_df, j, jj;
	size_t ch;
	p_list_rgb[0] = p_list[i].red;
	p_list_rgb[1] = p_list[i].green;
	p_list_rgb[2] = p_list[i].blue;
	p_list_rgb_1[0] = p_list[i+1].red;
	p_list_rgb_1[1] = p_list[i+1].green;
	p_list_rgb_1[2] = p_list[i+1].blue;
	/* */
	y_df = p_list[i+1].y - p_list[i].y;
	for ( ch=0 ; ch < 3 ; ch++ ) {	/* r,g,b */
	    float *img_ptr_rgb = sky_img_buf_p->array_ptr(0,0,ch);
	    double d;
	    d = (p_list_rgb_1[ch] - p_list_rgb[ch]) / y_df;
	    for ( j=p_list[i].y, jj=0 ; j < p_list[i+1].y + 1 ; j++, jj++ ) {
		img_ptr_rgb[ p_list[i].x + img_width * j ]
		    = p_list_rgb[ch] + d * jj;
	    }
	}
	i++;
    }

    return 0;
}

static int draw_vertical_quadratic( const sky_point *p_list,
				    size_t idx_begin, size_t n_section_v,
				    mdarray_float *sky_img_buf_p )
{
    long img_width = sky_img_buf_p->x_length();
    size_t ii, i = idx_begin;
    
    for ( ii=0 ; ii + 1 < n_section_v ; ii++ ) {
	double p_list_rgb[3][3];
	long j, last = 0;
	size_t ch;
	if ( ii + 2 == n_section_v ) last = 1;
	p_list_rgb[0][0] = p_list[i].red;
	p_list_rgb[0][1] = p_list[i+1].red;
	p_list_rgb[0][2] = p_list[i+2].red;
	p_list_rgb[1][0] = p_list[i].green;
	p_list_rgb[1][1] = p_list[i+1].green;
	p_list_rgb[1][2] = p_list[i+2].green;
	p_list_rgb[2][0] = p_list[i].blue;
	p_list_rgb[2][1] = p_list[i+1].blue;
	p_list_rgb[2][2] = p_list[i+2].blue;
	/* */
	for ( ch=0 ; ch < 3 ; ch++ ) {	/* r,g,b */
	    float *img_ptr_rgb = sky_img_buf_p->array_ptr(0,0,ch);
	    double x[3] = { (double)(p_list[i].y),
		(double)(p_list[i+1].y), (double)(p_list[i+2].y) };
	    double a=0, b=0, c=0;
	    minsq_quadratic(3, x, p_list_rgb[ch], &a, &b, &c);
	    if ( ii == 0 ) {
		for ( j=p_list[i].y ; j < p_list[i+1].y ; j++ ) {
		    img_ptr_rgb[ p_list[i].x + img_width * j ] = 0.0;
		}
	    }
	    for ( j=p_list[i].y ; j < p_list[i+1].y ; j++ ) {
		img_ptr_rgb[ p_list[i].x + img_width * j ]
		    += a + b * j + c * j * j;
	    }
	    if ( ii != 0 ) {			    /* average */
		for ( j=p_list[i].y ; j < p_list[i+1].y ; j++ ) {
		    img_ptr_rgb[ p_list[i].x + img_width * j ] *= 0.5;
		}
	    }
	    for ( j=p_list[i+1].y ; j < p_list[i+2].y + last ; j++ ) {
		img_ptr_rgb[ p_list[i].x + img_width * j ]
		    = a + b * j + c * j * j;
	    }
	}
	i++;
    }

    return 0;
}

static int draw_horizontal_linear( const sky_point *p_list,
				   size_t list_len,
				   mdarray_float *sky_img_buf_p )
{
    long img_width = sky_img_buf_p->x_length();
    long img_height = sky_img_buf_p->y_length();
    size_t ch, i;
    
    for ( ch=0 ; ch < 3 ; ch++ ) {	/* r,g,b */
	float *img_ptr_rgb = sky_img_buf_p->array_ptr(0,0,ch);
	for ( i=0 ; i + 1 < list_len ; i++ ) {
	    if ( p_list[i].x != p_list[i+1].x ) {
		long x0 = p_list[i].x;
		long x1 = p_list[i+1].x;
		long x_df = x1 - x0;
		long j, k, kk;
		for ( j=0 ; j < img_height ; j++ ) {
		    double d, v0, v1;
		    v0 = img_ptr_rgb[ x0 + img_width * j ];
		    v1 = img_ptr_rgb[ x1 + img_width * j ];
		    d = (v1 - v0) / x_df;
		    for ( k=x0, kk=0 ; k < x1 + 1 ; k++, kk++ ) {
			img_ptr_rgb[ k + img_width * j ] = v0 + d * kk;
		    }
		}
	    }
	}
    }

    return 0;
}

static int draw_horizontal_quadratic( const sky_point *p_list,
				      size_t list_len, size_t n_section_h,
				      mdarray_float *sky_img_buf_p )
{
    long img_width = sky_img_buf_p->x_length();
    long img_height = sky_img_buf_p->y_length();
    long x[n_section_h + 1];
    size_t ch, i, j;
    //stdstreamio sio;
    
    j = 0;
    x[j] = p_list[0].x;
    //sio.printf("[DEBUG] x[] = %ld\n", x[j]);
    j++;
    for ( i=0 ; i + 1 < list_len ; i++ ) {
	if ( p_list[i].x != p_list[i+1].x ) {
	    if ( j < n_section_h + 1 ) {
		x[j] = p_list[i+1].x;
		//sio.printf("[DEBUG] x[] = %ld\n", x[j]);
		j++;
	    }
	}
    }

    for ( ch=0 ; ch < 3 ; ch++ ) {	/* r,g,b */
	float *img_ptr_rgb = sky_img_buf_p->array_ptr(0,0,ch);
	for ( i=0 ; i + 1 < n_section_h ; i++ ) {
	    long j, k;
	    double a=0, b=0, c=0;
	    double x0[3] = {(double)x[i+0], (double)x[i+1], (double)x[i+2]};
	    double v[3];
	    long last = 0;
	    if ( i + 2 == n_section_h ) last = 1;
	    for ( j=0 ; j < img_height ; j++ ) {
		v[0] = img_ptr_rgb[ x[i+0] + img_width * j ];
		v[1] = img_ptr_rgb[ x[i+1] + img_width * j ];
		v[2] = img_ptr_rgb[ x[i+2] + img_width * j ];
		minsq_quadratic(3, x0, v, &a, &b, &c);
		if ( i == 0 ) {
		    for ( k=x[i+0] ; k < x[i+1] ; k++ ) {
			img_ptr_rgb[ k + img_width * j ] = 0.0;
		    }
		}
		for ( k=x[i+0] ; k < x[i+1] ; k++ ) {
		    img_ptr_rgb[ k + img_width * j ] += a + b * k + c * k * k;
		}
		if ( i != 0 ) {				/* average */
		    for ( k=x[i+0] ; k < x[i+1] ; k++ ) {
			img_ptr_rgb[ k + img_width * j ] *= 0.5;
		    }
		}
		for ( k=x[i+1] ; k < x[i+2] + last ; k++ ) {
		    img_ptr_rgb[ k + img_width * j ] = a + b * k + c * k * k;
		}
	    }
	}
    }

    return 0;
}


static int construct_sky_image( const mdarray &sky_point_list,
				mdarray_float *sky_img_buf_p )
{
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    size_t list_len = sky_point_list.length();
    size_t n_section_h, i;
    //stdstreamio sio;

    int return_status = -1;
    
    if ( list_len < 2 ) goto quit;

    /* vertical */
    for ( i=0 ; i + 1 < list_len ; i++ ) {
	size_t n_section_v, ii;
	/* */
	n_section_v = 0;
	if ( i == 0 || p_list[i].x != p_list[i-1].x ) {
	    for ( ii=i ; ii+1 < list_len ; ii++ ) {
		if ( p_list[ii].x == p_list[ii+1].x ) n_section_v ++;
		else break;
	    }
	    //sio.printf("[DEBUG] n_section_v = %ld\n", (long)n_section_v);
	    /* */
	    if ( n_section_v == 1 ) {
		draw_vertical_linear(p_list, i, n_section_v, sky_img_buf_p);
	    } else {
		draw_vertical_quadratic(p_list, i, n_section_v, sky_img_buf_p);
	    }
	}
    }

    /* horizontal */
    n_section_h = 0;
    for ( i=0 ; i + 1 < list_len ; i++ ) {
	if ( p_list[i].x != p_list[i+1].x ) {
	    n_section_h ++;
	}
    }
    //sio.printf("[DEBUG] n_section_h = %ld\n", (long)n_section_h);

    if ( n_section_h == 1 ) {
	draw_horizontal_linear( p_list, list_len, sky_img_buf_p );
    }
    else {
	draw_horizontal_quadratic( p_list, list_len, n_section_h,
				   sky_img_buf_p );
    }
    
    return_status = 0;
 quit:
    return return_status;
}

static int get_params_filename( const char *target_filename,
				tstring *ret_filename )
{
    size_t i, ix = 0;
    for ( i=0 ; target_filename[i] != '\0' ; i++ ) {
	if ( target_filename[i] == '.' ) ix = i;
    }
    if ( ix != 0 ) {
	ret_filename->assign(target_filename,ix);
	ret_filename->append(".pseudo-sky.txt");
    }
    else {
	ret_filename->printf("%s.pseudo-sky.txt",target_filename);
    }
    return 0;
}


const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Target            [1]"},
#define CMD_DISPLAY_SKY 2
        {CMD_DISPLAY_SKY,       "Display Pseudo Sky        [2]"},
#define CMD_DISPLAY_RESIDUAL1 3
        {CMD_DISPLAY_RESIDUAL1, "Display Residual x1       [3]"},
#define CMD_DISPLAY_RESIDUAL2 4
        {CMD_DISPLAY_RESIDUAL2, "Display Residual x2       [4]"},
#define CMD_DISPLAY_RESIDUAL4 5
        {CMD_DISPLAY_RESIDUAL4, "Display Residual x4       [5]"},
#define CMD_DISPLAY_RESIDUAL8 6
        {CMD_DISPLAY_RESIDUAL8, "Display Residual x8       [6]"},
#define CMD_DISPLAY_RGB 7
        {CMD_DISPLAY_RGB,       "Display RGB               [c]"},
#define CMD_DISPLAY_R 8
        {CMD_DISPLAY_R,         "Display Red               [c]"},
#define CMD_DISPLAY_G 9
        {CMD_DISPLAY_G,         "Display Green             [c]"},
#define CMD_DISPLAY_B 10
        {CMD_DISPLAY_B,         "Display Blue              [c]"},
#define CMD_ZOOM 11
        {CMD_ZOOM,              "Zoom +/-                  [+][-]"},
#define CMD_CONT_RGB 12
        {CMD_CONT_RGB,          "RGB Contrast +/-          [<][>]"},
#define CMD_CONT_R 13
        {CMD_CONT_R,            "Red Contrast +/-          [r][R]"},
#define CMD_CONT_G 14
        {CMD_CONT_G,            "Green Contrast +/-        [g][G]"},
#define CMD_CONT_B 15
        {CMD_CONT_B,            "Blue Contrast +/-         [b][B]"},
#define CMD_DISPLAY_RES_TYPE 16
        {CMD_DISPLAY_RES_TYPE,  "Type of residual          [t]"},
#define CMD_DISPLAY_POINTS 17
        {CMD_DISPLAY_POINTS,    "Display points on/off     [ ]"},
#define CMD_POINTSZ_PM 18
        {CMD_POINTSZ_PM,        "Box size of points +/-    [p][P]"},
#define CMD_DEL_POINT 19
        {CMD_DEL_POINT,         "Delete a point          [x][Del]"},
#define CMD_DEL_POINTS_V 20
        {CMD_DEL_POINTS_V,      "Delete all points on a line  [X]"},
#define CMD_SKYLV_PM 21
        {CMD_SKYLV_PM,          "Sky-level +/- at a point  [s][S]"},
#define CMD_SKYLV_INCDECL_PM 22
        {CMD_SKYLV_INCDECL_PM,  "Sky-level inc/decl +/-    [i][I]"},
#define CMD_SAVE_PARAMS 23
        {CMD_SAVE_PARAMS,       "Save Pseudo-Sky Params    [Enter]"},
#define CMD_DITHER 24
        {CMD_DITHER,            "Dither on/off for saving  [d]"},
#define CMD_SAVE_SKY 25
        {CMD_SAVE_SKY,          "Save Pseudo-Sky image"},
#define CMD_SAVE_IMAGE 26
        {CMD_SAVE_IMAGE,        "Save Pseudo-Sky-Subtracted image"},
#define CMD_EXIT 27
        {CMD_EXIT,              "Exit                      [q]"},
        {0, NULL}		/* EOL */
};

int main( int argc, char *argv[] )
{
    const char *conf_file_display = "display_2.txt";

    stdstreamio sio, f_in;
    tstring target_filename;
    
    command_win command_win_rec;
    int win_image;

    mdarray_float target_img_buf(false);	/* buffer for target image */
    mdarray_float sky_img_buf(false);		/* buffer for pseudo sky */
    mdarray_uchar tmp_buf(false);		/* tmp buffer for displaying */
    mdarray_uchar icc_buf(false);

    mdarray_float img_display(false);	/* buffer for displaying image */

    sky_point *sky_point_ptr;
    mdarray sky_point_list(sizeof(sky_point),false, /* pts to constract sky */
			   (void *)(&sky_point_ptr));

    int display_type = 0;		/* flag for display image type */
    int display_res_type = 0;		/* flag for residual */
    int display_ch = 0;			/* 0=RGB 1=R 2=G 3=B */
    int display_bin = 1;		/* binning factor for display */
    int contrast_rgb[3] = {8, 8, 8};	/* contrast for display */
    bool flag_display_points = true;
    int skylv_incdecl = 7;	/* pow(2,skylv_incdecl) */

    bool flag_dither = true;

    const char *names_ch[] = {"RGB", "Red", "Green", "Blue"};
    const char *names_res_type[] = {"Subtract(abs)", "Subtract(simple)",
				    "Bias", "Minus area(x16)"};

    long selected_point_idx = -1;
    
    int return_status = -1;

    if ( argc < 2 ) {
        sio.eprintf("Interactive tool to create pseudo sky image\n");
	sio.eprintf("\n");
	sio.eprintf("[INFO] Specify target frame\n");
	goto quit;
    }
    target_filename = argv[1];
    

    /*
     * GRAPHICS
     */
    
    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;

    /* Font selector */
    set_fontsize();

    /* command window */

    command_win_rec = gopen_command_window( Cmd_list, 0 );
    
    /* image viewer */

    sio.printf("Open: %s\n", target_filename.cstr());
    if ( read_tiff24or48_to_float(target_filename.cstr(), 65536.0,
				  &target_img_buf, NULL, &icc_buf, NULL) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    display_bin = get_bin_factor_for_display(target_img_buf.x_length(),
					     target_img_buf.y_length(), true);
    if ( display_bin < 0 ) {
        sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
		    "bad display depth\n");
	goto quit;
    }

    load_display_params(conf_file_display, contrast_rgb);
    
    /* set size of box on sky point */
    Skypoint_box_size =
	(target_img_buf.x_length() + target_img_buf.y_length()) / 170;
    if ( Skypoint_box_size % 2 == 0 ) Skypoint_box_size ++;
    if ( Skypoint_box_size < 3 ) Skypoint_box_size = 3;

    sio.printf("[INFO] image width = %ld  height = %ld\n",
	       (long)(target_img_buf.x_length()),
	       (long)(target_img_buf.y_length()));
    sio.printf("[INFO] Skypoint_box_size = %d\n",Skypoint_box_size);

    {
	tarray_tstring arr;
	tstring in_filename, line;
	size_t i;
	get_params_filename(target_filename.cstr(),
			    &in_filename);
	if ( f_in.open("r", in_filename.cstr()) < 0 ) {

	    /* default: determine sky point of 4-corner */
	    append_sky_point( target_img_buf, sky_img_buf, true,
			      0, 0,
			      2, &sky_point_list );
	    append_sky_point( target_img_buf, sky_img_buf, true,
			      target_img_buf.x_length() - 1, 0,
			      2, &sky_point_list );
	    append_sky_point( target_img_buf, sky_img_buf, true,
			      0, target_img_buf.y_length() - 1,
			      2, &sky_point_list );
	    append_sky_point( target_img_buf, sky_img_buf, true,
	      target_img_buf.x_length() - 1, target_img_buf.y_length() - 1,
	      2, &sky_point_list );

	}
	else {
	    i=0;
	    while ( (line=f_in.getline()) != NULL ) {
		arr.split( line.cstr(), " ", false);
		if ( arr.length() == 6 ) {
		    sky_point_list.resizeby(+1);
		    sky_point_ptr[i].x = arr[0].atol();
		    sky_point_ptr[i].y = arr[1].atol();
		    sky_point_ptr[i].red = arr[2].atof();
		    sky_point_ptr[i].green = arr[3].atof();
		    sky_point_ptr[i].blue = arr[4].atof();
		    sky_point_ptr[i].sticky = arr[5].atoi();
		    i++;
		}
	    }
	    f_in.close();
	    sio.printf("Loaded: [%s]\n", in_filename.cstr());
	}
    }
    
    sort_sky_point_list(&sky_point_list);
    
    /*
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	sio.eprintf("[DEBUG] x=%ld y=%ld R=%g G=%g B=%g\n",
	  sky_point_ptr[i].x, sky_point_ptr[i].y,
	  sky_point_ptr[i].red, sky_point_ptr[i].green, sky_point_ptr[i].blue);
    }
    */
    
    win_image = gopen(target_img_buf.x_length() / display_bin,
		      target_img_buf.y_length() / display_bin);

    /* display reference image */
    display_image(win_image, 0, 0, target_img_buf, 2,
		  display_bin, display_ch, contrast_rgb, true, &tmp_buf);

    winname(win_image, "Imave Viewer  "
	"zoom = %3.2f  contrast = ( %d, %d, %d )  dither = %d",
	(double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
	contrast_rgb[0], contrast_rgb[1], contrast_rgb[2], (int)flag_dither);

    /* allocate buffer */
    sky_img_buf.resize(target_img_buf);

    /* make pseudo sky image */
    construct_sky_image(sky_point_list, &sky_img_buf);
	    
    /* drawing sky points */
    draw_sky_points( win_image, target_img_buf, sky_point_list, -1 );
   

    /*
     * MAIN EVENT LOOP
     */

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        int refresh_image = 0;		/* 1:display only  2:both */
	bool refresh_graphics = false;
	bool refresh_winsize = false;
        bool refresh_winname = false;
        bool refresh_sky = false;
	
	int cmd_id = -1;

        /* waiting an event */
        ev_win = eggx_ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);

	if ( ev_type == ButtonPress && 1 <= ev_btn && ev_btn <= 3 &&
	     ev_win == command_win_rec.win_id ) {
	    cmd_id = 1 + ev_y / command_win_rec.cell_height;
	}

	if ( ev_type == KeyPress ) {
	    //sio.printf("[%d]\n", ev_btn);
	    if ( ev_btn == '1' ) cmd_id = CMD_DISPLAY_TARGET;
	    else if ( ev_btn == '2' ) cmd_id = CMD_DISPLAY_SKY;
	    else if ( ev_btn == '3' ) cmd_id = CMD_DISPLAY_RESIDUAL1;
	    else if ( ev_btn == '4' ) cmd_id = CMD_DISPLAY_RESIDUAL2;
	    else if ( ev_btn == '5' ) cmd_id = CMD_DISPLAY_RESIDUAL4;
	    else if ( ev_btn == '6' ) cmd_id = CMD_DISPLAY_RESIDUAL8;
	    else if ( ev_btn == 'c' ) {
	        /* rotate RGB/R/G/B */
	        if ( display_ch == 0 ) cmd_id = CMD_DISPLAY_R;
		else if ( display_ch == 1 ) cmd_id = CMD_DISPLAY_G;
		else if ( display_ch == 2 ) cmd_id = CMD_DISPLAY_B;
		else cmd_id = CMD_DISPLAY_RGB;
	    }
	    else if ( ev_btn == '+' || ev_btn == ';' ) {
		cmd_id = CMD_ZOOM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == '-' ) {
		cmd_id = CMD_ZOOM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == '>' || ev_btn == '.' ) {
		cmd_id = CMD_CONT_RGB;
		ev_btn = 1;
	    }
	    else if ( ev_btn == '<' || ev_btn == ',' ) {
		cmd_id = CMD_CONT_RGB;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'r' ) {
		cmd_id = CMD_CONT_R;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'R' ) {
		cmd_id = CMD_CONT_R;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'g' ) {
		cmd_id = CMD_CONT_G;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'G' ) {
		cmd_id = CMD_CONT_G;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'b' ) {
		cmd_id = CMD_CONT_B;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'B' ) {
		cmd_id = CMD_CONT_B;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 't' ) cmd_id = CMD_DISPLAY_RES_TYPE;
	    else if ( ev_btn == ' ' ) cmd_id = CMD_DISPLAY_POINTS;
	    else if ( ev_btn == 'p' ) {
		cmd_id = CMD_POINTSZ_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'P' ) {
		cmd_id = CMD_POINTSZ_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 127 ) cmd_id = CMD_DEL_POINT;
	    else if ( ev_btn == 'x' ) cmd_id = CMD_DEL_POINT;
	    else if ( ev_btn == 'X' ) cmd_id = CMD_DEL_POINTS_V;
	    else if ( ev_btn == 's' ) {
		cmd_id = CMD_SKYLV_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'S' ) {
		cmd_id = CMD_SKYLV_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'i' ) {
		cmd_id = CMD_SKYLV_INCDECL_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'I' ) {
		cmd_id = CMD_SKYLV_INCDECL_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'd' ) cmd_id = CMD_DITHER;
	    else if ( ev_btn == 13 ) cmd_id = CMD_SAVE_PARAMS;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn == 'q' ) cmd_id = CMD_EXIT;
	}
	
	if ( cmd_id == CMD_EXIT ) {
	    break;
	}
	else if ( cmd_id == CMD_SAVE_SKY ) {
	    tstring out_filename;
	    /* save using float */
	    make_output_filename(target_filename.cstr(), "pseudo-sky",
			 "float", &out_filename);
	    sio.printf("Writing '%s' ...\n", out_filename.cstr());
	    if ( write_float_to_tiff(sky_img_buf, icc_buf, NULL,
				     65536.0, out_filename.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff() failed.\n");
	    }
	    /* save using 16-bit */
	    make_output_filename(target_filename.cstr(), "pseudo-sky",
			 "16bit", &out_filename);
	    sio.printf("Writing '%s' ", out_filename.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    if ( write_float_to_tiff48(sky_img_buf, icc_buf, NULL,
		        0.0, 65535.0, flag_dither, out_filename.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff48() failed.\n");
	    }
	}
	else if ( cmd_id == CMD_SAVE_IMAGE ) {
	    mdarray_float result_img_buf(false);
	    tstring out_filename;
	    double min_v;
	    result_img_buf.resize(target_img_buf);
	    result_img_buf.paste(target_img_buf);
	    result_img_buf -= sky_img_buf;
	    /* save using float */
	    make_output_filename(target_filename.cstr(),
				 "pseudo-sky-subtracted",
				 "float", &out_filename);
	    sio.printf("Writing '%s' ...\n", out_filename.cstr());
	    if ( write_float_to_tiff(result_img_buf, icc_buf, NULL,
				     65536, out_filename.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff() failed.\n");
	    }
	    /* save using 16-bit */
	    make_output_filename(target_filename.cstr(),
				 "pseudo-sky-subtracted",
				 "16bit", &out_filename);
	    sio.printf("Writing '%s' ", out_filename.cstr());
	    if ( flag_dither == true ) sio.printf("using dither ...\n");
	    else sio.printf("NOT using dither ...\n");
	    /* */
	    min_v = md_min(result_img_buf);
	    if ( min_v < 0.0 ){
		result_img_buf -= min_v;
		sio.printf("[INFO] abs(min_v = %g) will be used as softbias\n",
			   min_v);
	    }
	    /* */
	    sio.printf("[INFO] scale will be changed\n");
	    /* */
	    if ( write_float_to_tiff48(result_img_buf, icc_buf, NULL,
			    0.0, 0.0, flag_dither, out_filename.cstr()) < 0 ) {
		sio.eprintf("[ERROR] write_float_to_tiff48() failed.\n");
	    }
	}

	else if ( CMD_DISPLAY_TARGET <= cmd_id &&
		  cmd_id <= CMD_DISPLAY_RESIDUAL8 ) {
	    display_type = cmd_id - CMD_DISPLAY_TARGET;
	    refresh_image = 2;
	}
	else if ( cmd_id == CMD_DISPLAY_RGB ) {
	    display_ch = 0;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_R ) {
	    display_ch = 1;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_G ) {
	    display_ch = 2;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_B ) {
	    display_ch = 3;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 1 ) {
	    if ( minus_binning_with_limit(&display_bin,
		  target_img_buf.x_length(), target_img_buf.y_length()) == true ) {
		refresh_image = 1;
		refresh_winsize = true;
	    }
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 3 ) {
	    if ( plus_binning_with_limit(&display_bin) == true ) {
		refresh_image = 1;
		refresh_winsize = true;
	    }
	}
	else if ( cmd_id == CMD_CONT_RGB && ev_btn == 1 ) {
	    contrast_rgb[0] ++;
	    contrast_rgb[1] ++;
	    contrast_rgb[2] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_RGB && ev_btn == 3 ) {
	    bool changed = false;
	    if ( 0 < contrast_rgb[0] ) {
	        contrast_rgb[0] --;  changed = true;
	    }
	    if ( 0 < contrast_rgb[1] ) {
	        contrast_rgb[1] --;  changed = true;
	    }
	    if ( 0 < contrast_rgb[2] ) {
	        contrast_rgb[2] --;  changed = true;
	    }
	    if ( changed == true ) {
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 1 ) {
	    contrast_rgb[0] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[0] ) {
	        contrast_rgb[0] --;
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 1 ) {
	    contrast_rgb[1] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[1] ) {
	      contrast_rgb[1] --;
	      save_display_params(conf_file_display, contrast_rgb);
	      refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 1 ) {
	    contrast_rgb[2] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[2] ) {
	        contrast_rgb[2] --;
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_DISPLAY_RES_TYPE ) {
	    if ( 3 <= display_res_type ) display_res_type = 0;
	    else display_res_type ++;
	    refresh_image = 2;
	}
	else if ( cmd_id == CMD_DISPLAY_POINTS ) {
	    if ( flag_display_points == false ) {
		flag_display_points = true;
		refresh_graphics = true;
	    }
	    else {
		flag_display_points = false;
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_POINTSZ_PM && ev_btn == 1 ) {
	    if ( Skypoint_box_size < Max_skypoint_box_size ) {
		Skypoint_box_size += 2;
		refresh_image = 1;
		refresh_winname = true;
	    }
	}
	else if ( cmd_id == CMD_POINTSZ_PM && ev_btn == 3 ) {
	    if ( 3 < Skypoint_box_size ) {
		Skypoint_box_size -= 2;
		refresh_image = 1;
		refresh_winname = true;
	    }
	}
	else if ( cmd_id == CMD_DEL_POINT ) {
	    if ( 0 <= selected_point_idx ) {
		if ( delete_sky_point(&sky_point_list,
				      selected_point_idx, false) == 0 ) {
		    selected_point_idx = -1;
		    refresh_sky = true;
		    refresh_image = 2;
		    flag_display_points = true;
		}
	    }
	}
	else if ( cmd_id == CMD_DEL_POINTS_V ) {
	    if ( 0 <= selected_point_idx ) {
		if ( delete_sky_point(&sky_point_list,
				      selected_point_idx, true) == 0 ) {
		    selected_point_idx = -1;
		    refresh_sky = true;
		    refresh_image = 2;
		    flag_display_points = true;
		}
	    }
	}
	else if ( cmd_id == CMD_SKYLV_PM && ev_btn == 1 ) {
	    if ( 0 <= selected_point_idx ) {
		update_sky_point_val_pm(display_ch,
					selected_point_idx, skylv_incdecl,
					true, &sky_point_list);
		refresh_sky = true;
		refresh_image = 2;
	    }
	}
	else if ( cmd_id == CMD_SKYLV_PM && ev_btn == 3 ) {
	    if ( 0 <= selected_point_idx ) {
		update_sky_point_val_pm(display_ch,
					selected_point_idx, skylv_incdecl,
					false, &sky_point_list);
		refresh_sky = true;
		refresh_image = 2;
	    }
	}
	else if ( cmd_id == CMD_SKYLV_INCDECL_PM && ev_btn == 1 ) {
	    if ( skylv_incdecl < 8 ) skylv_incdecl ++;
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SKYLV_INCDECL_PM && ev_btn == 3 ) {
	    if ( 0 < skylv_incdecl ) skylv_incdecl --;
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SAVE_PARAMS ) {
	    stdstreamio f_out;
	    tstring out_filename;
	    size_t i;
	    get_params_filename(target_filename.cstr(),
				&out_filename);
	    if ( f_out.open("w", out_filename.cstr()) < 0 ) {
		sio.eprintf("[ERROR] Cannot write params to '%s'.\n",
			    out_filename.cstr());
	    }
	    else {
		for ( i=0 ; i < sky_point_list.length() ; i++ ) {
		    f_out.printf("%ld %ld %g %g %g %d\n",
				 sky_point_ptr[i].x,
				 sky_point_ptr[i].y,
				 sky_point_ptr[i].red,
				 sky_point_ptr[i].green,
				 sky_point_ptr[i].blue,
				 sky_point_ptr[i].sticky);
		}
		f_out.close();
		sio.printf("Saved: [%s]\n", out_filename.cstr());
	    }
	}
	else if ( cmd_id == CMD_DITHER ) {
	    if ( flag_dither == true ) flag_dither = false;
	    else flag_dither = true;
	    refresh_winname = true;
	}

	else if ( ev_type == ButtonPress ) {
	    if ( ev_win == win_image &&
		 ( 1 <= ev_btn && ev_btn <= 3 ) /* left|center|right */ ) {
#if 0
		long actual_x = 0, actual_y = 0;
		double median_rgb[3];
#endif
		long point_idx;

		point_idx = search_sky_point_box(
					target_img_buf, sky_point_list,
					ev_x, ev_y );
		//sio.eprintf("point_idx = %ld\n", point_idx);

		if ( 0 <= point_idx ) {
		    /* found */
		    if ( point_idx == selected_point_idx ) {
			if ( ev_btn == 1 ) {
			    update_sky_point_val_pm(display_ch,
						    point_idx, skylv_incdecl,
						    true, &sky_point_list);
			}
			else if ( ev_btn == 3 ) {	/* right btn */
			    update_sky_point_val_pm(display_ch,
						    point_idx, skylv_incdecl,
						    false, &sky_point_list);
			}
			else if ( ev_btn == 2 ) {	/* center btn */
			    /* get median */
			    double median_rgb[3];
			    get_box_median(target_img_buf,
					   sky_point_ptr[point_idx].x,
					   sky_point_ptr[point_idx].y,
					   median_rgb);
			    update_sky_point_val(display_ch,
						 point_idx, median_rgb,
						 &sky_point_list);
			}
			refresh_sky = true;
			refresh_image = 2;
		    }
		    else {
			selected_point_idx = point_idx;
			refresh_graphics = true;
			flag_display_points = true;
		    }
		}
		else {
		    /* not found ... register new points */
		    long p_idx;
		    bool flag_use_img = false;
		    if ( ev_btn == 3 || ev_btn == 2 ) {
			flag_use_img = true;	/* right btn || center btn */
		    }
		    /* search nearest existing vertical line */
		    p_idx = search_sky_point_box(
					target_img_buf, sky_point_list,
					ev_x, 0 );
		    if ( 0 <= p_idx ) {			/* found existing */
			/* overwrite ev_x */
			ev_x = sky_point_ptr[p_idx].x;
			/* register */
			append_sky_point( target_img_buf, sky_img_buf,
					  flag_use_img, 
					  ev_x, ev_y,
					  0, &sky_point_list );
		    }
		    else {				/* not found existing */
			/* register */
			append_sky_point( target_img_buf, sky_img_buf, false,
					  ev_x, 0,
					  1, &sky_point_list );
			append_sky_point( target_img_buf, sky_img_buf,
					  flag_use_img, 
					  ev_x, ev_y,
					  0, &sky_point_list );
			append_sky_point( target_img_buf, sky_img_buf, false,
					  ev_x, target_img_buf.y_length() - 1,
					  1, &sky_point_list );
		    }
		    sort_sky_point_list(&sky_point_list);
		    /* */
		    selected_point_idx = search_sky_point(
					  target_img_buf, sky_point_list,
					  ev_x, ev_y);
		    refresh_sky = true;
		    refresh_image = 2;
		    flag_display_points = true;
		}
#if 0		
		fix_points_box(target_img_buf, ev_x, ev_y,
			       &actual_x, &actual_y);
		get_box_median(target_img_buf, actual_x, actual_y, median_rgb);
	        //offset_x = ev_x - (img_buf.x_length() / 2);
		sio.eprintf("ev_x, ev_y = %g, %g   ac_x, ac_y = %ld, %ld\n",
			    ev_x, ev_y, actual_x, actual_y);
		sio.eprintf("median_rgb = %g, %g, %g\n",
			    median_rgb[0], median_rgb[1], median_rgb[2]);
	        //offset_y = ev_y - (img_buf.y_length() / 2);
#endif
	    }
	}
	else if ( ev_type == KeyPress ) {
	    //sio.printf("key = %d\n", ev_btn);
	    /* move a point using cursor keys */
	    if ( ev_btn == 28 ) {		/* Right key */
	        refresh_image = 2;
	    }
	    else if ( ev_btn == 29 ) {		/* Left key */
	        refresh_image = 2;
	    }
	    else if ( ev_btn == 30 ) {		/* Up key */
	        refresh_image = 2;
	    }
	    else if ( ev_btn == 31 ) {		/* Down key */
	        refresh_image = 2;
	    }
	}

	/* Update window */
	
	if ( refresh_sky == true ) {
	    construct_sky_image(sky_point_list, &sky_img_buf);
	}
	
	if ( refresh_image != 0 ) {
	    if ( 2 <= display_type ) {		/* Residual */
		double min_v;
		tstring str_min_v = "";
		if ( 1 < refresh_image ) {
		    img_display.resize(target_img_buf);
		    img_display.paste(target_img_buf);
		    img_display -= sky_img_buf;
		    if ( display_res_type == 0 ) {	/* abs */
			img_display.abs();
		    }
		    else if ( display_res_type == 2 ) {	/* bias */
			min_v = md_min(img_display);
			if ( min_v < 0.0 ){
			    img_display -= min_v;
			}
			str_min_v.printf("min_v = %g  ", min_v);
		    }
		    else if ( display_res_type == 3 ) {	/* minus-only */
			img_display -= fabs(img_display);
			img_display *= (-8.0);
		    }
		    if ( display_type == 3 ) img_display *= 2.0;
		    else if ( display_type == 4 ) img_display *= 4.0;
		    else if ( display_type == 5 ) img_display *= 8.0;
		}
		display_image(win_image, 0, 0, img_display, 2,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Residual [%s]  %s"
			"channel = %s  zoom = %3.2f  "
			"contrast = ( %d, %d, %d )  ",
			names_res_type[display_res_type], str_min_v.cstr(),
			names_ch[display_ch],
			(double)((display_bin < 0) ? -display_bin : 1.0/display_bin), 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    else if ( display_type == 1 ) {	/* Sky */
		display_image(win_image, 0, 0, sky_img_buf, 2,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Pseudo Sky  channel = %s  zoom = %3.2f  "
			"contrast = ( %d, %d, %d )  ",
			names_ch[display_ch],
			(double)((display_bin < 0) ? -display_bin : 1.0/display_bin), 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    else {				/* Target */
		display_image(win_image, 0, 0, target_img_buf, 2,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Target  channel = %s  zoom = %3.2f  "
			"contrast = ( %d, %d, %d )  ",
			names_ch[display_ch],
			(double)((display_bin < 0) ? -display_bin : 1.0/display_bin), 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    refresh_graphics = true;
	}

	if ( flag_display_points == true && refresh_graphics == true ) {
	    /* drawing sky points */
	    draw_sky_points( win_image, target_img_buf, sky_point_list,
			     selected_point_idx );
	}

	if ( refresh_winname == true ) {
	    winname(win_image,
		    "sky-level_box = %d  sky-level_inc/decl = %g  dither = %d",
		    Skypoint_box_size, pow(2,skylv_incdecl), (int)flag_dither);
	}


    }
    
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff48.cc"
#include "write_float_to_tiff.cc"
