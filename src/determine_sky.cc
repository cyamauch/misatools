/*
 * $ s++ determine_sky.cc -leggx -lX11 -ltiff
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

const double Contrast_scale = 4.0;

#include "read_tiff24or48.h"
#include "write_tiff24or48.h"
#include "load_display_params.cc"
#include "display_image.cc"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   determine_sky.cc
 * @brief  interactive sky subtraction tool
 *         8/16-bit images are supported.
 */

int main( int argc, char *argv[] )
{
    int contrast_rgb[3] = {8, 8, 8};

    stdstreamio sio, f_in;
    tstring filename;
    tstring filename_out;
    tstring tmp_str;

    mdarray image_buf(UCHAR_ZT,false);
    mdarray_uchar icc_buf(false);
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    mdarray_float stat_buf(false);
    float *const *const *stat_buf_ptr;
    size_t width = 0, height = 0;
    long new_sky_level = 0;
    size_t i, j, k;
    int arg_cnt;
    
    int step_count;
    double obj_x, obj_y, obj_r, sky_r;
    double sky_lv[3]; 
    
    int win_image;
    
    int return_status = -1;


    if ( argc < 2 ) {
	sio.eprintf("Interactive sky subtraction tool\n");
	sio.eprintf("\n");
	sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-l new_sky_level(0...255)] input_filename.tiff\n", argv[0]);
	sio.eprintf("Note: default new sky level is 0.\n");
	goto quit;
    }

    load_display_params( "display_1.txt", contrast_rgb );

    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	tmp_str = argv[arg_cnt];
	if ( tmp_str == "-l" ) {
	    arg_cnt ++;
	    tmp_str = argv[arg_cnt];
	    new_sky_level = tmp_str.atol();
	    if ( new_sky_level < 0 ) {
		sio.eprintf("[WARNING] new_sky_level is set to 0\n");
		new_sky_level = 0;
	    }
	    else if ( 255 < new_sky_level ) {
		sio.eprintf("[WARNING] new_sky_level is set to 255\n");
		new_sky_level = 255;
	    }
	    arg_cnt ++;
	}
	else break;
    }

    sio.printf("Applying new sky level = %ld\n", new_sky_level);
    
    filename = argv[arg_cnt];

    if ( f_in.open("r", filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] cannot open file: %s\n", filename.cstr());
	goto quit;
    }
    f_in.close();

    if ( read_tiff24or48( filename.cstr(), &image_buf, &icc_buf ) < 0 ) {
	sio.eprintf("[ERROR] read_tiff24or48() failed\n");
	goto quit;
    }
    if ( image_buf.size_type() == UCHAR_ZT ) {
	sio.printf("found 8-bit RGB image.\n");
    }
    else if ( image_buf.size_type() == FLOAT_ZT ) {
	sio.printf("found 16-bit RGB image.\n");
    }
    else {
	sio.eprintf("[ERROR] unexpected image type\n");
	goto quit;
    }
    width = image_buf.x_length();
    height = image_buf.y_length();
    
    win_image = gopen(width, height);
    winname(win_image, "RGB image");
    
    display_image(win_image, image_buf, 1, 0, contrast_rgb, &tmp_buf);


    /* set drawing mode */
    newgcfunction(win_image, GXxor);
    newcolor(win_image, "green");
    
    /*
     * 1st EVENT LOOP
     */
    obj_x = -32000;
    obj_y = -32000;
    obj_r = 65535;
    sky_r = 65535;
    step_count = 0;
    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        ev_win = eggx_ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);
	if ( ev_win == win_image ) {
	    if ( ev_type == KeyPress ) {
		if ( ev_btn == 27 || ev_btn == 'q' ) goto quit;
	    }
	    if ( step_count == 0 ) {
		if ( ev_type == MotionNotify ) {
		    /* draw large cross */
		    drawline(win_image, 0, obj_y, width, obj_y);
		    drawline(win_image, obj_x, 0, obj_x, height);
		    obj_x = ev_x;
		    obj_y = ev_y;
		    drawline(win_image, 0, obj_y, width, obj_y);
		    drawline(win_image, obj_x, 0, obj_x, height);
		}
		else if ( ev_type == ButtonPress ) {
		    if ( 0 <= obj_x && obj_x < width &&
			 0 <= obj_y && obj_y < height ) {
			step_count ++;
		    }
		}
	    }
	    else if ( step_count == 1 ) {
		if ( ev_type == MotionNotify ) {
		    double ev_r;
		    /* draw object circle */
		    ev_r = sqrt(pow(obj_x-ev_x,2) + pow(obj_y-ev_y,2));
		    drawcirc(win_image, obj_x, obj_y, obj_r, obj_r);
		    obj_r = ev_r;
		    drawcirc(win_image, obj_x, obj_y, obj_r, obj_r);
		}
		else if ( ev_type == ButtonPress ) {
		    if ( obj_r < width && obj_r < height ) {
			step_count ++;
		    }
		}
	    }
	    else if ( step_count == 2 ) {
		if ( ev_type == MotionNotify ) {
		    double ev_r;
		    /* draw object circle */
		    ev_r = sqrt(pow(obj_x-ev_x,2) + pow(obj_y-ev_y,2));
		    drawcirc(win_image, obj_x, obj_y, sky_r, sky_r);
		    sky_r = ev_r;
		    drawcirc(win_image, obj_x, obj_y, sky_r, sky_r);
		}
		else if ( ev_type == ButtonPress ) {
		    if ( obj_r < sky_r && sky_r < width && sky_r < height ) {
			break;
		    }
		}
	    }
	}
	
    }

    //image_buf.crop(0, obj_x - sky_r, 2 * sky_r);
    //image_buf.crop(1, obj_y - sky_r, 2 * sky_r);
    
    /* start statistics */
    stat_buf.resize_3d(sky_r * 2, sky_r *2, 3);
    stat_buf = (float)NAN;
    stat_buf.paste(image_buf, - obj_x + sky_r, - obj_y + sky_r);
    if ( image_buf.size_type() == UCHAR_ZT ) {
	stat_buf *= 256.0;
    }
    
    /* get 3-d array ptr */
    stat_buf_ptr = stat_buf.array_ptr_3d(true);

    for ( i=0 ; i < stat_buf.y_length() ; i++ ) {
	for ( j=0 ; j < stat_buf.x_length() ; j++ ) {
	    double r;
	    r = sqrt(pow(j - sky_r,2)+pow(i - sky_r,2));
	    if ( r < obj_r || sky_r < r ) {
		stat_buf_ptr[0][i][j] = NAN;
		stat_buf_ptr[1][i][j] = NAN;
		stat_buf_ptr[2][i][j] = NAN;
	    }
	}
    }
    
    //gclr(win_image);
    //display_image(win_image, stat_buf, 1, 0, contrast_rgb, &tmp_buf);

    sky_lv[0] = md_median(stat_buf.sectionf("*,*,0"));
    sky_lv[1] = md_median(stat_buf.sectionf("*,*,1"));
    sky_lv[2] = md_median(stat_buf.sectionf("*,*,2"));
    sio.printf("sky_level(r,g,b) = %g,%g,%g\n",
	       sky_lv[0],sky_lv[1],sky_lv[2]);

    if ( image_buf.size_type() == UCHAR_ZT ) {

	unsigned char *const *const *image_buf_ptr;
	/* get 3-d array ptr */
	image_buf_ptr =
	    (unsigned char *const *const *)image_buf.data_ptr_3d(true);

	for ( k=0 ; k < image_buf.z_length() ; k++ ) {
	    for ( i=0 ; i < image_buf.y_length() ; i++ ) {
		for ( j=0 ; j < image_buf.x_length() ; j++ ) {
		    double v = image_buf_ptr[k][i][j] * 256.0;
		    v -= sky_lv[k];
		    v += new_sky_level;
		    v /= 256.0;
		    v += 0.5;
		    if ( v < 0 ) v = 0;
		    else if ( 255 < v ) v = 255;
		    image_buf_ptr[k][i][j] = (unsigned char)v;
		}
	    }
	}

    }
    else if ( image_buf.size_type() == FLOAT_ZT ) {

	float *const *const *image_buf_ptr;
	/* get 3-d array ptr */
	image_buf_ptr =
	    (float *const *const *)image_buf.data_ptr_3d(true);

	for ( k=0 ; k < image_buf.z_length() ; k++ ) {
	    for ( i=0 ; i < image_buf.y_length() ; i++ ) {
		for ( j=0 ; j < image_buf.x_length() ; j++ ) {
		    double v = image_buf_ptr[k][i][j];
		    v -= sky_lv[k];
		    v += new_sky_level;
		    v += 0.5;
		    if ( v < 0 ) v = 0;
		    else if ( 65535 < v ) v = 65535;
		    image_buf_ptr[k][i][j] = v;
		}
	    }
	}

    }
    else {
	sio.eprintf("[FATAL] unexpected error\n");
	goto quit;
    }
    
    /* create new filename */
    if ( image_buf.bytes() == 1 ) {
	make_output_filename(filename.cstr(), "sky_lv", "8bit",
			     &filename_out);
    }
    else {
	make_output_filename(filename.cstr(), "sky_lv", "16bit",
			     &filename_out);
    }

    sio.printf("Saved [%s]\n", filename_out.cstr());

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    if ( write_tiff24or48( image_buf, icc_buf, filename_out.cstr()) < 0 ) {
	sio.eprintf("[ERROR] write_tiff24or48() failed\n");
	goto quit;
    }

    //ggetch();
    
    return_status = 0;
 quit:
    return return_status;
}


#include "read_tiff24or48.cc"
#include "write_tiff24or48.cc"
