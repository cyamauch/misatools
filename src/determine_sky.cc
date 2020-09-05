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

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff48.h"
#include "write_float_to_tiff.h"
#include "load_display_params.cc"
#include "save_display_params.cc"
#include "get_bin_factor_for_display.c"
#include "display_image.cc"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   determine_sky.cc
 * @brief  interactive sky subtraction tool.
 *         8/16-bit integer and 32-bit float images are supported.
 */

int main( int argc, char *argv[] )
{
    int contrast_rgb[3] = {8, 8, 8};

    stdstreamio sio, f_in;
    tstring filename;
    tstring out_filename;
    tstring tmp_str;

    mdarray_float image_buf(false);
    mdarray_uchar icc_buf(false);
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    mdarray_float stat_buf(false);
    float *const *const *stat_buf_ptr;
    size_t width = 0, height = 0;
    long new_sky_level = 0;
    bool flag_dither = true;
    
    int display_bin = 1;		/* binning factor for display */
    int step_count, sztype;
    double obj_x, obj_y, obj_r, sky_r;
    bool flag_drawed = false;
    double sky_lv[3]; 
    int win_image;

    int arg_cnt;
    size_t i, j, k;
    
    int return_status = -1;


    if ( argc < 2 ) {
	sio.eprintf("Interactive sky subtraction tool\n");
	sio.eprintf("\n");
	sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s [-l param] [-t] input_filename.tiff\n", argv[0]);
	sio.eprintf("-l param ... new_sky_level (0...65535)\n");
	sio.eprintf("-t ... If set, dither is not used to output 8/16-bit images\n");
	sio.eprintf("Note: default new sky level is 0.\n");
	goto quit;
    }

    load_display_params( "display_1.txt", contrast_rgb );

    arg_cnt = 1;

    while ( arg_cnt < argc ) {
	tmp_str = argv[arg_cnt];
	if ( tmp_str == "-t" ) {
	    flag_dither = false;
	    arg_cnt ++;
	}
	else if ( tmp_str == "-l" ) {
	    arg_cnt ++;
	    tmp_str = argv[arg_cnt];
	    new_sky_level = tmp_str.atol();
	    if ( new_sky_level < 0 ) {
		sio.eprintf("[WARNING] new_sky_level is set to 0\n");
		new_sky_level = 0;
	    }
	    else if ( 65535 < new_sky_level ) {
		sio.eprintf("[WARNING] new_sky_level is set to 65535\n");
		new_sky_level = 65535;
	    }
	    arg_cnt ++;
	}
	else break;
    }

    sio.printf("[GUI]\n");
    sio.printf("'+' '-' ... zoom\n");
    sio.printf("'<' '>' ... contrast\n");
    sio.printf("'r' 'R' ... red contrast\n");
    sio.printf("'g' 'G' ... green contrast\n");
    sio.printf("'b' 'B' ... blue contrast\n");
    sio.printf("'q' 'ESC' ... exit\n");
    sio.printf("\n");
    
    sio.printf("Applying new sky level = %ld\n", new_sky_level);
    
    filename = argv[arg_cnt];

    if ( f_in.open("r", filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] cannot open file: %s\n", filename.cstr());
	goto quit;
    }
    f_in.close();

    if ( read_tiff24or48_to_float( filename.cstr(), 65536.0,
				   &image_buf, &sztype, &icc_buf, NULL ) < 0 ) {
	sio.eprintf("[ERROR] read_tiff24or48() failed\n");
	goto quit;
    }
    if ( sztype == 1 ) {
	sio.printf("found 8-bit RGB image.\n");
    }
    else if ( sztype == 2 ) {
	sio.printf("found 16-bit RGB image.\n");
    }
    else {
	sio.printf("Found a float(32-bit) RGB image.\n");
    }
    width = image_buf.x_length();
    height = image_buf.y_length();

    display_bin = get_bin_factor_for_display(width, height);
    if ( display_bin < 0 ) {
        sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
		    "bad display depth\n");
	goto quit;
    }

    
    /*
     * GRAPHICS
     */

    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;
    
    win_image = gopen(600, 400);
    winname(win_image, "RGB image");
    
    display_image(win_image, image_buf,
		  display_bin, 0, contrast_rgb, &tmp_buf);

    /* set drawing mode */
    newgcfunction(win_image, GXxor);
    newcolor(win_image, "green");
    

    /*
     * EVENT LOOP
     */
    obj_x = -32000;
    obj_y = -32000;
    obj_r = 65535;
    sky_r = 65535;
    step_count = 0;

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;
	bool refresh_image = false;
	
        ev_win = eggx_ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);
	if ( ev_win == win_image ) {
	    if ( ev_type == KeyPress ) {
		if ( ev_btn == 27 || ev_btn == 'q' ) {
		    goto quit;
		}
		else if ( ev_btn == '+' ) {		/* zoom-in */
		    if ( 1 < display_bin ) {
			display_bin --;
			refresh_image = true;
		    }
		}
		else if ( ev_btn == '-' ) {		/* zoom-out */
		    if ( display_bin < 10 ) {
			display_bin ++;
			refresh_image = true;
		    }
		}
		else if ( ev_btn == 'r' ) {
		    contrast_rgb[0] ++;
		    save_display_params("display_1.txt", contrast_rgb);
		    refresh_image = true;
		}
		else if ( ev_btn == 'R' ) {
		    if ( 0 < contrast_rgb[0] ) {
			contrast_rgb[0] --;
			save_display_params("display_1.txt", contrast_rgb);
			refresh_image = true;
		    }
		}
		else if ( ev_btn == 'g' ) {
		    contrast_rgb[1] ++;
		    save_display_params("display_1.txt", contrast_rgb);
		    refresh_image = true;
		}
		else if ( ev_btn == 'G' ) {
		    if ( 0 < contrast_rgb[1] ) {
			contrast_rgb[1] --;
			save_display_params("display_1.txt", contrast_rgb);
			refresh_image = true;
		    }
		}
		else if ( ev_btn == 'b' ) {
		    contrast_rgb[2] ++;
		    save_display_params("display_1.txt", contrast_rgb);
		    refresh_image = true;
		}
		else if ( ev_btn == 'B' ) {
		    if ( 0 < contrast_rgb[2] ) {
			contrast_rgb[2] --;
			save_display_params("display_1.txt", contrast_rgb);
			refresh_image = true;
		    }
		}
		else if ( ev_btn == '>' ) {
		    contrast_rgb[0] ++;
		    contrast_rgb[1] ++;
		    contrast_rgb[2] ++;
		    save_display_params("display_1.txt", contrast_rgb);
		    refresh_image = true;
		}
		else if ( ev_btn == '<' ) {
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
			save_display_params("display_1.txt", contrast_rgb);
			refresh_image = true;
		    }
		}
	    }
	    else if ( step_count == 0 ) {
		if ( ev_type == MotionNotify ) {
		    /* draw large cross */
		    if ( flag_drawed == true ) {
			drawline(win_image, 0, obj_y, width, obj_y);
			drawline(win_image, obj_x, 0, obj_x, height);
		    }
		    obj_x = ev_x;
		    obj_y = ev_y;
		    drawline(win_image, 0, obj_y, width, obj_y);
		    drawline(win_image, obj_x, 0, obj_x, height);
		    flag_drawed = true;
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
		    if ( flag_drawed == true ) {
			drawcirc(win_image, obj_x, obj_y, obj_r, obj_r);
		    }
		    obj_r = ev_r;
		    drawcirc(win_image, obj_x, obj_y, obj_r, obj_r);
		    flag_drawed = true;
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
		    if ( flag_drawed == true ) {
			drawcirc(win_image, obj_x, obj_y, sky_r, sky_r);
		    }
		    sky_r = ev_r;
		    drawcirc(win_image, obj_x, obj_y, sky_r, sky_r);
		    flag_drawed = true;
		}
		else if ( ev_type == ButtonPress ) {
		    if ( obj_r < sky_r && sky_r < width && sky_r < height ) {
			break;
		    }
		}
	    }
	}

	if ( refresh_image == true ) {
	    newgcfunction(win_image, GXcopy);	/* set normal mode */
	    display_image(win_image, image_buf,
			  display_bin, 0, contrast_rgb, &tmp_buf);
	    winname(win_image,
		    "RGB image  zoom = 1/%d  contrast = ( %d, %d, %d )",
		    display_bin,
		    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
	    newgcfunction(win_image, GXxor);
	    flag_drawed = false;
	}

    }

    //image_buf.crop(0, obj_x - sky_r, 2 * sky_r);
    //image_buf.crop(1, obj_y - sky_r, 2 * sky_r);
    
    /* start statistics */
    stat_buf.resize_3d(sky_r * 2, sky_r *2, 3);
    stat_buf = (float)NAN;
    stat_buf.paste(image_buf, - obj_x + sky_r, - obj_y + sky_r);

#if 0	/* test! */
    newgcfunction(win_image, GXcopy);
    display_image(win_image, stat_buf,
		  display_bin, 0, contrast_rgb, &tmp_buf);
    winname(win_image, "TEST!");
    ggetch();
#endif
    
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

#if 0	/* test! */
    newgcfunction(win_image, GXcopy);
    display_image(win_image, stat_buf,
		  display_bin, 0, contrast_rgb, &tmp_buf);
    winname(win_image, "TEST!");
    ggetch();
#endif

    sky_lv[0] = md_median(stat_buf.sectionf("*,*,0"));
    sky_lv[1] = md_median(stat_buf.sectionf("*,*,1"));
    sky_lv[2] = md_median(stat_buf.sectionf("*,*,2"));
    sio.printf("sky_level(r,g,b) = %g,%g,%g\n",
	       sky_lv[0],sky_lv[1],sky_lv[2]);

    /* subract sky */
    {
	float *const *const *image_buf_ptr;
	/* get 3-d array ptr */
	image_buf_ptr = image_buf.array_ptr_3d(true);

	for ( k=0 ; k < image_buf.z_length() ; k++ ) {
	    for ( i=0 ; i < image_buf.y_length() ; i++ ) {
		for ( j=0 ; j < image_buf.x_length() ; j++ ) {
		    double v = image_buf_ptr[k][i][j];
		    v -= sky_lv[k];
		    v += new_sky_level;
		    if ( v < 0 ) v = 0;
		    else if ( 65535 < v ) v = 65535;
		    image_buf_ptr[k][i][j] = v;
		}
	    }
	}
    }

    /* set icc profile */
    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    /* save using float */
    make_output_filename(filename.cstr(), "sky-lv",
			 "float", &out_filename);
    sio.printf("Writing '%s' ...\n", out_filename.cstr());
    if ( write_float_to_tiff(image_buf, icc_buf, NULL, 
			     65536.0, out_filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] write_float_to_tiff() failed.\n");
	goto quit;
    }
    
    /* save using 16-bit */
    make_output_filename(filename.cstr(), "sky-lv",
			 "16bit", &out_filename);
    sio.printf("Writing '%s' ", out_filename.cstr());
    if ( flag_dither == true ) sio.printf("using dither ...\n");
    else sio.printf("NOT using dither ...\n");
    if ( write_float_to_tiff48(image_buf, icc_buf, NULL,
		       0.0, 65535.0, flag_dither, out_filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] write_float_to_tiff48() failed.\n");
	goto quit;
    }

    //ggetch();
    
    return_status = 0;
 quit:
    return return_status;
}


#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff48.cc"
#include "write_float_to_tiff.cc"
