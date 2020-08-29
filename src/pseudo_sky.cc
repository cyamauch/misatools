/*
 * $ s++ pseudo_sky.cc -leggx -lX11 -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/pipestreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
#include <eggx.h>
#include <unistd.h>
using namespace sli;

const double Contrast_scale = 2.0;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff48.h"
#include "display_image.cc"
#include "load_display_params.cc"
#include "save_display_params.cc"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   pseudo_images.cc
 * @brief  interactive tool to create pseudo sky image
 *         8/16-bit images are supported.
 */

const int Font_y_off = 3;
const int Font_margin = 2;
static int Fontsize = 14;
#include "set_fontsize.c"

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


#define SZ_BLOCK 15
/* fix (x,y) of point at near edge of an image [15x15] */
static int fix_points_15x15( const mdarray_float &target_img_buf,
				 long point_x, long point_y,
				 long *actual_x_ret, long *actual_y_ret )
{
    const long offset = SZ_BLOCK / 2;
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
static long search_sky_point_15x15( const mdarray_float &target_img_buf,
			      const mdarray &sky_point_list,
			      long ev_x, long ev_y )
{
    const long offset = SZ_BLOCK / 2;
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    size_t i;
    double distance = 65536;
    long return_idx = -1;
    
    /* Search ... */
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	long point_x = p_list[i].x;
	long point_y = p_list[i].y;
	fix_points_15x15( target_img_buf, point_x, point_y,
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

/* get median at a point [15x15] */
static int get_15x15_median( const mdarray_float &target_img_buf,
			   long point_x, long point_y,
			   double median_rgb[] )
{
    const long offset = SZ_BLOCK / 2;
    mdarray_float sample(false);
    int return_status = -1;

    sample.resize_2d(SZ_BLOCK, SZ_BLOCK);

    fix_points_15x15( target_img_buf, point_x, point_y,
		    &point_x, &point_y );
    
    /* R */
    target_img_buf.copy(&sample,
	       point_x - offset, SZ_BLOCK,  point_y - offset, SZ_BLOCK,  0, 1);
    if ( median_rgb != NULL ) median_rgb[0] = md_median(sample);
    //sample.dprint();

    /* G */
    target_img_buf.copy(&sample,
	       point_x - offset, SZ_BLOCK,  point_y - offset, SZ_BLOCK,  1, 1);
    if ( median_rgb != NULL ) median_rgb[1] = md_median(sample);
    //sample.dprint();

    /* B */
    //sample.paste(target_img_buf,
    //		offset - point_x, offset - point_y, -2);
    //sample.dprint();
    target_img_buf.copy(&sample,
	       point_x - offset, SZ_BLOCK,  point_y - offset, SZ_BLOCK,  2, 1);
    //sample.dprint();
    if ( median_rgb != NULL ) median_rgb[2] = md_median(sample);
    
    return_status = 0;

    return return_status;
}

static int delete_sky_point( mdarray *sky_point_list_p,
			     long point_idx, bool v_all )
{
    stdstreamio sio;

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
			     long x, long y, int sticky,
			     mdarray *sky_point_list_p )
{
    sky_point *p;
    size_t len = sky_point_list_p->length();
    double median_rgb[3];

    get_15x15_median(img, x, y, median_rgb);
    
    sky_point_list_p->resize(len + 1);
    p = (sky_point *)sky_point_list_p->data_ptr();

    p[len].x = x;
    p[len].y = y;
    p[len].red   = median_rgb[0];
    p[len].green = median_rgb[1];
    p[len].blue  = median_rgb[2];
    p[len].sticky = sticky;

    return 0;
}

static int update_sky_point_val( int rgb_ch,
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

static int draw_sky_points( int win_image,
			    const mdarray_float &target_img_buf,
			    const mdarray &sky_point_list, long selected )
{
    const long offset = SZ_BLOCK / 2;
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    stdstreamio sio;
    size_t i;

    newcolor(win_image, "green");
   
    /* Search ... */
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	long point_x = p_list[i].x;
	long point_y = p_list[i].y;
	//sio.printf("x,y = %ld,%ld\n",p_list[i].x,p_list[i].y);
	fix_points_15x15( target_img_buf, point_x, point_y,
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

#undef SZ_BLOCK

static int construct_sky_image( const mdarray &sky_point_list,
				mdarray_float *sky_img_buf_p )
{
    const sky_point *p_list = (const sky_point *)(sky_point_list.data_ptr_cs());
    size_t list_len = sky_point_list.length();
    long img_width = sky_img_buf_p->x_length();
    long img_height = sky_img_buf_p->y_length();
    float *img_ptr_red = sky_img_buf_p->array_ptr(0,0,0);
    float *img_ptr_green = sky_img_buf_p->array_ptr(0,0,1);
    float *img_ptr_blue = sky_img_buf_p->array_ptr(0,0,2);
    size_t i;

    int return_status = -1;
    
    if ( list_len < 2 ) goto quit;

    for ( i=1 ; i < list_len ; i++ ) {
	if ( p_list[i-1].x == p_list[i].x ) {
	    double d;
	    long y_df, j, jj;
	    /* drawing vertical */
	    y_df = p_list[i].y - p_list[i-1].y;
	    d = (p_list[i].red - p_list[i-1].red) / y_df;
	    for ( j=p_list[i-1].y, jj=0 ; j < p_list[i].y + 1 ; j++, jj++ ) {
		img_ptr_red[ p_list[i].x + img_width * j ]
		    = p_list[i-1].red + d * jj;
	    }
	    d = (p_list[i].green - p_list[i-1].green) / y_df;
	    for ( j=p_list[i-1].y, jj=0 ; j < p_list[i].y + 1 ; j++, jj++ ) {
		img_ptr_green[ p_list[i].x + img_width * j ]
		    = p_list[i-1].green + d * jj;
	    }
	    d = (p_list[i].blue - p_list[i-1].blue) / y_df;
	    for ( j=p_list[i-1].y, jj=0 ; j < p_list[i].y + 1 ; j++, jj++ ) {
		img_ptr_blue[ p_list[i].x + img_width * j ]
		    = p_list[i-1].blue + d * jj;
	    }
	}
    }

    for ( i=1 ; i < list_len ; i++ ) {
	if ( p_list[i-1].x != p_list[i].x ) {
	    long x0 = p_list[i-1].x;
	    long x1 = p_list[i].x;
	    long x_df = x1 - x0;
	    long j, k, kk;
	    for ( j=0 ; j < img_height ; j++ ) {
		double d, v0, v1;
		v0 = img_ptr_red[ x0 + img_width * j ];
		v1 = img_ptr_red[ x1 + img_width * j ];
		d = (v1 - v0) / x_df;
		for ( k=x0, kk=0 ; k < x1 + 1 ; k++, kk++ ) {
		    img_ptr_red[ k + img_width * j ] = v0 + d * kk;
		}
		/* */
		v0 = img_ptr_green[ x0 + img_width * j ];
		v1 = img_ptr_green[ x1 + img_width * j ];
		d = (v1 - v0) / x_df;
		for ( k=x0, kk=0 ; k < x1 + 1 ; k++, kk++ ) {
		    img_ptr_green[ k + img_width * j ] = v0 + d * kk;
		}
		/* */
		v0 = img_ptr_blue[ x0 + img_width * j ];
		v1 = img_ptr_blue[ x1 + img_width * j ];
		d = (v1 - v0) / x_df;
		for ( k=x0, kk=0 ; k < x1 + 1 ; k++, kk++ ) {
		    img_ptr_blue[ k + img_width * j ] = v0 + d * kk;
		}
	    }
	}
    }
    
    return_status = 0;
 quit:
    return return_status;
}

			   
typedef struct _command_list {
    int id;
    const char *menu_string;
} command_list;

const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Target           [1]"},
#define CMD_DISPLAY_SKY 2
        {CMD_DISPLAY_SKY,       "Display Pseudo Sky       [2]"},
#define CMD_DISPLAY_RESIDUAL1 3
        {CMD_DISPLAY_RESIDUAL1, "Display Residual x1      [3]"},
#define CMD_DISPLAY_RESIDUAL2 4
        {CMD_DISPLAY_RESIDUAL2, "Display Residual x2      [4]"},
#define CMD_DISPLAY_RESIDUAL4 5
        {CMD_DISPLAY_RESIDUAL4, "Display Residual x4      [5]"},
#define CMD_DISPLAY_RESIDUAL8 6
        {CMD_DISPLAY_RESIDUAL8, "Display Residual x8      [6]"},
#define CMD_DISPLAY_RGB 7
        {CMD_DISPLAY_RGB,       "Display RGB              [c]"},
#define CMD_DISPLAY_R 8
        {CMD_DISPLAY_R,         "Display Red              [c]"},
#define CMD_DISPLAY_G 9
        {CMD_DISPLAY_G,         "Display Green            [c]"},
#define CMD_DISPLAY_B 10
        {CMD_DISPLAY_B,         "Display Blue             [c]"},
#define CMD_ZOOM 11
        {CMD_ZOOM,              "Zoom +/-                 [+][-]"},
#define CMD_CONT_R 12
        {CMD_CONT_R,            "Red Contrast +/-         [r][R]"},
#define CMD_CONT_G 13
        {CMD_CONT_G,            "Green Contrast +/-       [g][G]"},
#define CMD_CONT_B 14
        {CMD_CONT_B,            "Blue Contrast +/-        [b][B]"},
#define CMD_DEL_POINT 15
        {CMD_DEL_POINT,         "Delete a point           [Del]"},
#define CMD_DEL_POINTS_V 16
        {CMD_DEL_POINTS_V,      "Delete all points on a line [X]"},
#define CMD_SKYLV_PM 17
        {CMD_SKYLV_PM,          "Sky-level +/- at a point [s][S]"},
#define CMD_SKYLV_INCDECL_PM 18
        {CMD_SKYLV_INCDECL_PM,  "Sky-level inc/decl +/-   [i][I]"},
#define CMD_SAVE_PARAMS 19
        {CMD_SAVE_PARAMS,       "Save Pseudo-Sky Params   [Enter]"},
#define CMD_DITHER 20
        {CMD_DITHER,            "Dither on/off for saving [d]"},
#define CMD_SAVE_SKY 21
        {CMD_SAVE_SKY,          "Save Pseudo-Sky image"},
#define CMD_SAVE_IMAGE 22
        {CMD_SAVE_IMAGE,        "Save Pseudo-Sky-Subtracted image"},
#define CMD_EXIT 23
        {CMD_EXIT,              "Exit                     [q]"}
};

const size_t N_cmd_list = sizeof(Cmd_list) / sizeof(Cmd_list[0]);

int main( int argc, char *argv[] )
{
    stdstreamio sio, f_in;
    tstring target_filename;

    int win_command, win_image;
    int win_command_col_height;

    mdarray_float target_img_buf(false);	/* buffer for target image */
    mdarray_float sky_img_buf(false);		/* buffer for pseudo sky */
    mdarray_float result_img_buf(false);	/* buffer for result image */
    mdarray_uchar tmp_buf(false);		/* tmp buffer for displaying */

    mdarray_float img_display(false);	/* buffer for displaying image */

    sky_point *sky_point_ptr;
    mdarray sky_point_list(sizeof(sky_point),false, /* pts to constract sky */
			   (void *)(&sky_point_ptr));

    int display_type = 0;		/* flag to display image type */
    int display_ch = 0;			/* 0=RGB 1=R 2=G 3=B */
    int display_bin = 1;		/* binning factor for display */
    int contrast_rgb[3] = {8, 8, 8};	/* contrast for display */
    int skylv_incdecl = 8;	/* pow(2,skylv_incdecl) */

    bool flag_dither = true;

    const char *names_ch[] = {"RGB", "Red", "Green", "Blue"};

    long selected_point_idx = -1;
    
    size_t i;
    
    int return_status = -1;

    load_display_params("display_0.txt", contrast_rgb);

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

    {
	const int w_width = 32 * (Fontsize/2) + Font_margin * 2;
	const int c_height = (Fontsize + Font_margin * 2);
	win_command = gopen(w_width, c_height * (N_cmd_list));
	gsetbgcolor(win_command,"#606060");
	gclr(win_command);
	winname(win_command, "Command Window");
	for ( i=0 ; i < N_cmd_list ; i++ ) {
	    newrgbcolor(win_command,0x80,0x80,0x80);
	    drawline(win_command,
		     0, c_height * (Cmd_list[i].id - 1),
		     w_width, c_height * (Cmd_list[i].id - 1));
	    newrgbcolor(win_command,0x40,0x40,0x40);
	    drawline(win_command,
		     0, c_height * (Cmd_list[i].id) - 1,
		     w_width, c_height * (Cmd_list[i].id) - 1);
	    newrgbcolor(win_command,0xff,0xff,0xff);
	    drawstr(win_command,
		    Font_margin, c_height * Cmd_list[i].id - Font_y_off,
		    Fontsize, 0, Cmd_list[i].menu_string);
	}
	win_command_col_height = c_height;
    }
    
    /* image viewer */

    win_image = gopen(600,400);

    if ( read_tiff24or48_to_float(target_filename.cstr(),
				  &target_img_buf, NULL, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }

    /* determine sky point of 4-corner */
    append_sky_point( target_img_buf, 0, 0,
		      2, &sky_point_list );
    append_sky_point( target_img_buf, target_img_buf.x_length() - 1, 0,
		      2, &sky_point_list );
    append_sky_point( target_img_buf, 0, target_img_buf.y_length() - 1,
		      2, &sky_point_list );
    append_sky_point( target_img_buf,
		target_img_buf.x_length() - 1, target_img_buf.y_length() - 1,
		2, &sky_point_list );

    sort_sky_point_list(&sky_point_list);
    
    for ( i=0 ; i < sky_point_list.length() ; i++ ) {
	sio.eprintf("[DEBUG] x=%ld y=%ld R=%g G=%g B=%g\n",
	  sky_point_ptr[i].x, sky_point_ptr[i].y,
	  sky_point_ptr[i].red, sky_point_ptr[i].green, sky_point_ptr[i].blue);
    }
    
    
    /* display reference image */
    display_image(win_image, target_img_buf,
		  display_bin, display_ch, contrast_rgb, &tmp_buf);

    winname(win_image, "Imave Viewer  contrast = ( %d, %d, %d )  dither=%d",
	contrast_rgb[0], contrast_rgb[1], contrast_rgb[2], (int)flag_dither);

    /* allocate buffer */
    sky_img_buf.resize(target_img_buf);
    result_img_buf.resize(target_img_buf);

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
        bool refresh_winname = false;
        bool refresh_sky = false;
	
	int cmd_id = -1;

        /* waiting an event */
        ev_win = eggx_ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);

	if ( ev_type == ButtonPress && ev_win == win_command ) {
	    cmd_id = 1 + ev_y / win_command_col_height;
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
	    else if ( ev_btn == '+' ) {
		cmd_id = CMD_ZOOM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == '-' ) {
		cmd_id = CMD_ZOOM;
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
	}
	else if ( cmd_id == CMD_SAVE_IMAGE ) {
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
	    if ( 1 < display_bin ) display_bin --;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 3 ) {
	    if ( display_bin < 10 ) display_bin ++;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 1 ) {
	    contrast_rgb[0] ++;
	    save_display_params("display_0.txt", contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[0] ) {
	        contrast_rgb[0] --;
		save_display_params("display_0.txt", contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 1 ) {
	    contrast_rgb[1] ++;
	    save_display_params("display_0.txt", contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[1] ) {
	      contrast_rgb[1] --;
	      save_display_params("display_0.txt", contrast_rgb);
	      refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 1 ) {
	    contrast_rgb[2] ++;
	    save_display_params("display_0.txt", contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[2] ) {
	        contrast_rgb[2] --;
		save_display_params("display_0.txt", contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_DEL_POINT ) {
	    if ( 0 <= selected_point_idx ) {
		if ( delete_sky_point(&sky_point_list,
				      selected_point_idx, false) == 0 ) {
		    selected_point_idx = -1;
		    refresh_sky = true;
		    refresh_image = 2;
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
		}
	    }
	}
	else if ( cmd_id == CMD_SKYLV_PM && ev_btn == 1 ) {
	    if ( 0 <= selected_point_idx ) {
		update_sky_point_val(display_ch,
				     selected_point_idx, skylv_incdecl,
				     true, &sky_point_list);
		refresh_sky = true;
		refresh_image = 2;
	    }
	}
	else if ( cmd_id == CMD_SKYLV_PM && ev_btn == 3 ) {
	    if ( 0 <= selected_point_idx ) {
		update_sky_point_val(display_ch,
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
	}
	else if ( cmd_id == CMD_DITHER ) {
	    if ( flag_dither == true ) flag_dither = false;
	    else flag_dither = true;
	    refresh_winname = true;
	}

	else if ( ev_type == ButtonPress ) {
	    if ( ev_win == win_image &&
		 ( ev_btn == 1 || ev_btn == 3 ) /* left|right btn */ ) {
		long actual_x = 0, actual_y = 0;
		double median_rgb[3];
		long point_idx;

		point_idx = search_sky_point_15x15(
					target_img_buf, sky_point_list,
					ev_x, ev_y );
		sio.eprintf("point_idx = %ld\n", point_idx);

		if ( 0 <= point_idx ) {
		    /* found */
		    if ( point_idx == selected_point_idx ) {
			if ( ev_btn == 1 ) {
			    update_sky_point_val(display_ch,
						 point_idx, skylv_incdecl,
						 true, &sky_point_list);
			}
			else {
			    update_sky_point_val(display_ch,
						 point_idx, skylv_incdecl,
						 false, &sky_point_list);
			}
			refresh_sky = true;
			refresh_image = 2;
		    }
		    else {
			selected_point_idx = point_idx;
			refresh_graphics = true;
		    }
		}
		else {
		    /* not found ... register new points */
		    long p_idx;
		    /* search nearest existing vertical line */
		    p_idx = search_sky_point_15x15(
					target_img_buf, sky_point_list,
					ev_x, 0 );
		    if ( 0 <= p_idx ) {			/* found existing */
			/* overwrite ev_x */
			ev_x = sky_point_ptr[p_idx].x;
			/* register */
			append_sky_point( target_img_buf, ev_x, ev_y,
					  0, &sky_point_list );
		    }
		    else {				/* not found existing */
			/* register */
			append_sky_point( target_img_buf, ev_x, 0,
					  1, &sky_point_list );
			append_sky_point( target_img_buf, ev_x, ev_y,
					  0, &sky_point_list );
			append_sky_point( target_img_buf,
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
		}
#if 0		
		fix_points_15x15(target_img_buf, ev_x, ev_y,
			       &actual_x, &actual_y);
		get_15x15_median(target_img_buf, actual_x, actual_y, median_rgb);
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
	    /* toggle target/sky images */
	    else if ( ev_btn == ' ' ) {
	        if ( display_type == 0 ) display_type = 1;
		else display_type = 0;
		refresh_image = 2;
	    }
	}

	/* Update window */
	
	if ( refresh_winname == true ) {
	    winname(win_image, "sky-level_inc/decl=%g  dither=%d",
		    pow(2,skylv_incdecl), (int)flag_dither);
	}

	if ( refresh_sky == true ) {
	    construct_sky_image(sky_point_list, &sky_img_buf);
	}
	
	if ( refresh_image != 0 ) {
	    if ( 2 <= display_type ) {		/* Residual */
		if ( 1 < refresh_image ) {
		    img_display.resize(target_img_buf);
		    img_display.paste(target_img_buf);
		    img_display.subtract(sky_img_buf);
		    img_display.abs();
		    if ( display_type == 3 ) img_display *= 2.0;
		    else if ( display_type == 4 ) img_display *= 4.0;
		    else if ( display_type == 5 ) img_display *= 8.0;
		}
		display_image(win_image, img_display,
			      display_bin, display_ch, contrast_rgb, &tmp_buf);
		winname(win_image, "Residual  channel = %s  zoom = 1/%d  "
			"contrast = ( %d, %d, %d )  ",
			names_ch[display_ch], display_bin, 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    else if ( display_type == 1 ) {	/* Sky */
		display_image(win_image, sky_img_buf,
			      display_bin, display_ch, contrast_rgb, &tmp_buf);
		winname(win_image, "Pseudo Sky  channel = %s  zoom = 1/%d  "
			"contrast = ( %d, %d, %d )  ",
			names_ch[display_ch], display_bin, 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    else {				/* Target */
		display_image(win_image, target_img_buf,
			      display_bin, display_ch, contrast_rgb, &tmp_buf);
		winname(win_image, "Target  channel = %s  zoom = 1/%d  "
			"contrast = ( %d, %d, %d )  ",
			names_ch[display_ch], display_bin, 
			contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    }
	    refresh_graphics = true;
	}

	if ( refresh_graphics == true ) {
	    /* drawing sky points */
	    draw_sky_points( win_image, target_img_buf, sky_point_list,
			     selected_point_idx );
	}

    }
    
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff48.cc"
