/*
 * $ s++ align_rgb.cc -leggx -lX11 -ltiff
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

#include "read_tiff24or48_separate_buffer.h"
#include "write_float_to_tiff24or48.h"
#include "load_display_params.cc"
#include "save_display_params.cc"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   align_rgb.cc
 * @brief  interactive rgb matching tool
 *         8/16-bit images are supported.
 */

const int Font_y_off = 3;
const int Font_margin = 2;
static int Fontsize = 14;
#include "set_fontsize.c"

static int update_image_buffer( int display_type,
     long display_gain_r, long display_gain_b,
     long off_r_x, long off_r_y, long off_b_x, long off_b_y,
     const mdarray &in_img_r, const mdarray &in_img_g, const mdarray &in_img_b,
     mdarray_float *image_rgb_buf,
     mdarray_float *image_gr_buf,
     mdarray_float *image_gb_buf )
{
    int ret_status = -1;

    if ( image_rgb_buf != NULL ) {	/* RGB */
	image_rgb_buf->paste(in_img_r, off_r_x, off_r_y, 0);
	image_rgb_buf->paste(in_img_g, 0, 0,             1);
	image_rgb_buf->paste(in_img_b, off_b_x, off_b_y, 2);
	if ( in_img_r.size_type() == UCHAR_ZT ) (*image_rgb_buf) *= 256.0;
    }
    if ( image_gr_buf != NULL ) {	/* G - R */
	if ( display_type == 0 ) {
	    image_gr_buf->paste(in_img_r, off_r_x, off_r_y);
	}
	else if ( display_type == 1 ) {
	    image_gr_buf->paste(in_img_g);
	}
	else {
	    double gain = display_gain_r / 100.0;
	    image_gr_buf->paste(in_img_g);
	    image_gr_buf->subtract(in_img_r * gain, off_r_x, off_r_y);
	    image_gr_buf->abs();
	    if ( display_type == 3 ) (*image_gr_buf) *= 2.0;
	    if ( display_type == 4 ) (*image_gr_buf) *= 4.0;
	    if ( display_type == 5 ) (*image_gr_buf) *= 8.0;
	}
	if ( in_img_r.size_type() == UCHAR_ZT ) (*image_gr_buf) *= 256.0;
    }
    if ( image_gb_buf != NULL ) {	/* G - B */
	if ( display_type == 0 ) {
	    image_gb_buf->paste(in_img_b, off_b_x, off_b_y);
	}
	else if ( display_type == 1 ) {
	    image_gb_buf->paste(in_img_g);
	}
	else {
	    double gain = display_gain_b / 100.0;
	    image_gb_buf->paste(in_img_g);
	    image_gb_buf->subtract(in_img_b * gain, off_b_x, off_b_y);
	    image_gb_buf->abs();
	    if ( display_type == 3 ) (*image_gb_buf) *= 2.0;
	    if ( display_type == 4 ) (*image_gb_buf) *= 4.0;
	    if ( display_type == 5 ) (*image_gb_buf) *= 8.0;
	}
	if ( in_img_r.size_type() == UCHAR_ZT ) (*image_gb_buf) *= 256.0;
    }

    ret_status = 0;

    return ret_status;
}

/* display 8- or 16-bit RGB or mono image */
static int display_image( int win_image, const mdarray &img_buf,
			  const int contrast_rgb[], int ch_mono,
			  mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    unsigned char *tmp_buf_ptr;
    size_t j, jj, k;
    bool needs_resize_win = false;

    int ret_status = -1;

    if ( img_buf.dim_length() != 3 && img_buf.dim_length() != 2 ) {
        sio.eprintf("[ERROR] img_buf is not RGB data\n");
	return -1;
    }


    /* check size of temporary array buffer */
    if ( tmp_buf->x_length() != img_buf.x_length() * 4 ||
	 tmp_buf->y_length() != img_buf.y_length() ) {
        needs_resize_win = true;
	tmp_buf->resize_2d(img_buf.x_length() * 4 /* ARGB */,
			   img_buf.y_length());
    }
    tmp_buf_ptr = tmp_buf->array_ptr();
    
    if ( needs_resize_win == true ) {
        gresize(win_image, img_buf.x_length(), img_buf.y_length());
    }

    if ( img_buf.size_type() == UCHAR_ZT ) {		/* 8-bit image */

	if ( img_buf.z_length() == 1 ) {		/* mono image */
	    size_t off1=0, off4=0;
	    const unsigned char *img_buf_ptr;
	    img_buf_ptr = (const unsigned char *)img_buf.data_ptr();
	    for ( k=0 ; k < img_buf.y_length() ; k++ ) {
		double v, ct;
		ct = pow(2, contrast_rgb[ch_mono]/Contrast_scale);
		for ( j=0, jj=1 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = img_buf_ptr[off1 + j] * ct + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = v;
		    tmp_buf_ptr[off4 + jj + 1] = v;
		    tmp_buf_ptr[off4 + jj + 2] = v;
		}
		off1 += img_buf.x_length();
		off4 += img_buf.x_length() * 4;
	    }
	}
	else {
	    size_t off1=0, off4=0;
	    const unsigned char *img_buf_ptr_r;
	    const unsigned char *img_buf_ptr_g;
	    const unsigned char *img_buf_ptr_b;
	    img_buf_ptr_r = (const unsigned char *)img_buf.data_ptr(0,0,0);
	    img_buf_ptr_g = (const unsigned char *)img_buf.data_ptr(0,0,1);
	    img_buf_ptr_b = (const unsigned char *)img_buf.data_ptr(0,0,2);
	    for ( k=0 ; k < img_buf.y_length() ; k++ ) {
		double v, ct;
		ct = pow(2, contrast_rgb[0]/Contrast_scale);
		for ( j=0, jj=1 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = img_buf_ptr_r[off1 + j] * ct + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = v;
		}
		ct = pow(2, contrast_rgb[1]/Contrast_scale);
		for ( j=0, jj=2 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = img_buf_ptr_g[off1 + j] * ct + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = v;
		}
		ct = pow(2, contrast_rgb[2]/Contrast_scale);
		for ( j=0, jj=3 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = img_buf_ptr_b[off1 + j] * ct + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = v;
		}
		off1 += img_buf.x_length();
		off4 += img_buf.x_length() * 4;
	    }
	}
    }
    else if ( img_buf.size_type() == FLOAT_ZT ) {	/* 16-bit image */

	if ( img_buf.z_length() == 1 ) {		/* mono image */
	    size_t off1=0, off4=0;
	    const float *img_buf_ptr;
	    img_buf_ptr = (const float *)img_buf.data_ptr();
	    for ( k=0 ; k < img_buf.y_length() ; k++ ) {
		double v, ct;
		ct = pow(2, contrast_rgb[ch_mono]/Contrast_scale);
		for ( j=0, jj=1 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = (img_buf_ptr[off1 + j]/256.0) * ct + 0.5;
		    if ( v < 0.0 ) v = 0.0;
		    else if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = (unsigned char)v;
		    tmp_buf_ptr[off4 + jj + 1] = (unsigned char)v;
		    tmp_buf_ptr[off4 + jj + 2] = (unsigned char)v;
		}
		off1 += img_buf.x_length();
		off4 += img_buf.x_length() * 4;
	    }
	}
	else {
	    size_t off1=0, off4=0;
	    const float *img_buf_ptr_r;
	    const float *img_buf_ptr_g;
	    const float *img_buf_ptr_b;
	    img_buf_ptr_r = (const float *)img_buf.data_ptr(0,0,0);
	    img_buf_ptr_g = (const float *)img_buf.data_ptr(0,0,1);
	    img_buf_ptr_b = (const float *)img_buf.data_ptr(0,0,2);
	    for ( k=0 ; k < img_buf.y_length() ; k++ ) {
		double v, ct;
		ct = pow(2, contrast_rgb[0]/Contrast_scale);
		for ( j=0, jj=1 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = (img_buf_ptr_r[off1 + j]/256.0) * ct + 0.5;
		    if ( v < 0.0 ) v = 0.0;
		    else if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = (unsigned char)v;
		}
		ct = pow(2, contrast_rgb[1]/Contrast_scale);
		for ( j=0, jj=2 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = (img_buf_ptr_g[off1 + j]/256.0) * ct + 0.5;
		    if ( v < 0.0 ) v = 0.0;
		    else if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = (unsigned char)v;
		}
		ct = pow(2, contrast_rgb[2]/Contrast_scale);
		for ( j=0, jj=3 ; j < img_buf.x_length() ; j++, jj+=4 ) {
		    v = (img_buf_ptr_b[off1 + j]/256.0) * ct + 0.5;
		    if ( v < 0.0 ) v = 0.0;
		    else if ( 255.0 < v ) v = 255.0;
		    tmp_buf_ptr[off4 + jj] = (unsigned char)v;
		}
		off1 += img_buf.x_length();
		off4 += img_buf.x_length() * 4;
	    }
	}

    }
    else {
	/* error */
	goto quit;
    }
    
    gputimage(win_image, 0,0,
	      tmp_buf_ptr, img_buf.x_length(), img_buf.y_length(), 0);
    
    ret_status = 0;
 quit:
    return ret_status;
}

static void update_display_gain_rb( const int contrast_rgb[],
				    long *display_gain_r_ret,
				    long *display_gain_b_ret )
{
    double v;
    v = pow(2, contrast_rgb[0]/Contrast_scale) / pow(2, contrast_rgb[1]/Contrast_scale);
    *display_gain_r_ret = (long)(v * 100 + 0.5);
    v = pow(2, contrast_rgb[2]/Contrast_scale) / pow(2, contrast_rgb[1]/Contrast_scale);
    *display_gain_b_ret = (long)(v * 100 + 0.5);
    return;
}


typedef struct _command_list {
    int id;
    const char *menu_string;
} command_list;

const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Target (R and B) [1]"},
#define CMD_DISPLAY_REFERENCE 2
        {CMD_DISPLAY_REFERENCE, "Display Reference (G)    [2]"},
#define CMD_DISPLAY_RESIDUAL1 3
        {CMD_DISPLAY_RESIDUAL1, "Display Residual x1      [3]"},
#define CMD_DISPLAY_RESIDUAL2 4
        {CMD_DISPLAY_RESIDUAL2, "Display Residual x2      [4]"},
#define CMD_DISPLAY_RESIDUAL4 5
        {CMD_DISPLAY_RESIDUAL4, "Display Residual x4      [5]"},
#define CMD_DISPLAY_RESIDUAL8 6
        {CMD_DISPLAY_RESIDUAL8, "Display Residual x8      [6]"},
#define CMD_CONT_PLUS_R 7
        {CMD_CONT_PLUS_R,       "Red Contrast +           [r]"},
#define CMD_CONT_MINUS_R 8
        {CMD_CONT_MINUS_R,      "Red Contrast -           [R]"},
#define CMD_CONT_PLUS_G 9
        {CMD_CONT_PLUS_G,       "Green Contrast +         [g]"},
#define CMD_CONT_MINUS_G 10
        {CMD_CONT_MINUS_G,      "Green Contrast -         [G]"},
#define CMD_CONT_PLUS_B 11
        {CMD_CONT_PLUS_B,       "Blue Contrast +          [b]"},
#define CMD_CONT_MINUS_B 12
        {CMD_CONT_MINUS_B,      "Blue Contrast -          [B]"},
#define CMD_SAVE 13
        {CMD_SAVE,              "Save Aligned Image       [Enter]"},
#define CMD_EXIT 14
        {CMD_EXIT,              "Exit                     [q]"}
};

const size_t N_cmd_list = sizeof(Cmd_list) / sizeof(Cmd_list[0]);

int main( int argc, char *argv[] )
{
    int contrast_rgb[3] = {8, 8, 8};

    stdstreamio sio, f_in;
    tstring filename;
    mdarray in_image_r_buf(UCHAR_ZT,false);
    mdarray in_image_g_buf(UCHAR_ZT,false);
    mdarray in_image_b_buf(UCHAR_ZT,false);
    mdarray_float image_rgb_buf(false);
    mdarray_float image_gr_buf(false);	/* g - r */
    mdarray_float image_gb_buf(false);	/* g - b */
    mdarray_uchar icc_buf(false);
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    size_t width = 0, height = 0;

    int win_rgb, win_gr, win_gb, win_command;
    int win_command_col_height;

    int display_type = 2 /* residual */;	/* flag to display image type */
    long offset_r_x = 0;
    long offset_r_y = 0;
    long offset_b_x = 0;
    long offset_b_y = 0;
    long display_gain_r = 100;
    long display_gain_b = 100;
    
    size_t i;
    
    int return_status = -1;

    if ( argc < 2 ) {
	sio.eprintf("Interactive RGB matching tool\n");
	sio.eprintf("\n");
	sio.eprintf("[USAGE]\n");
	sio.eprintf("$ %s input_filename.tiff\n", argv[0]);
	goto quit;
    }

    load_display_params( "display_1.txt", contrast_rgb );

    update_display_gain_rb( contrast_rgb, &display_gain_r, &display_gain_b );
    
    filename = argv[1];

    if ( f_in.open("r", filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] cannot open file: %s\n", filename.cstr());
	goto quit;
    }
    f_in.close();

    if ( read_tiff24or48_separate_buffer( filename.cstr(),
			  &in_image_r_buf, &in_image_g_buf, &in_image_b_buf,
			  &icc_buf ) < 0 ) {
	sio.eprintf("[ERROR] read_tiff24or48_separate_buffer() failed\n");
	goto quit;
    }
    if ( in_image_r_buf.size_type() == UCHAR_ZT ) {
	sio.printf("found 8-bit RGB image.\n");
    }
    else if ( in_image_r_buf.size_type() == FLOAT_ZT ) {
	sio.printf("found 16-bit RGB image.\n");
    }
    else {
	sio.eprintf("[ERROR] unexpected image type\n");
	goto quit;
    }
    width = in_image_r_buf.x_length();
    height = in_image_r_buf.y_length();
    
    image_rgb_buf.resize_3d(width,height,3);
    image_gr_buf.resize_2d(width,height);
    image_gb_buf.resize_2d(width,height);
    

    /*
     * GRAPHICS
     */
    
    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;

    /* Font selector */
    set_fontsize();

    /* command window */

    //win_command = gopen((Fontsize/2) * 32, Fontsize * (N_cmd_list + 1));
    //winname(win_command, "Command Window");
    //for ( i=0 ; i < N_cmd_list ; i++ ) {
    //    drawstr(win_command, 0, Fontsize*Cmd_list[i].id - Font_y_off,
    //		Fontsize, 0, Cmd_list[i].menu_string);
    //}
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

    win_rgb = gopen(width, height);
    winname(win_rgb, "RGB image");
    
    win_gr = gopen(width, height);
    winname(win_gr, "Display for G and R [Residual]");

    win_gb = gopen(width, height);
    winname(win_gb, "Display for G and B [Residual]");



    /* refresh windows */
    update_image_buffer( display_type,
			 display_gain_r, display_gain_b,
			 offset_r_x, offset_r_y, offset_b_x, offset_b_y,
			 in_image_r_buf, in_image_g_buf, in_image_b_buf,
			 &image_rgb_buf, &image_gr_buf, &image_gb_buf );

    display_image( win_rgb, image_rgb_buf, contrast_rgb, 0, &tmp_buf );
    display_image( win_gr, image_gr_buf, contrast_rgb, 1, &tmp_buf );
    display_image( win_gb, image_gb_buf, contrast_rgb, 1, &tmp_buf );

    
    /*
     * MAIN EVENT LOOP
     */

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        bool update_image = false;
        bool refresh_gr = false;
        bool refresh_gb = false;

	int cmd_id = -1;
	
        /* waiting an event */
        ev_win = eggx_ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);

	if ( ev_type == ButtonPress && ev_win == win_command ) {
	    cmd_id = 1 + ev_y / win_command_col_height;
	}

	if ( ev_type == KeyPress ) {
	    //sio.printf("[%d]\n", ev_btn);
	    if ( ev_btn == '1' ) cmd_id = CMD_DISPLAY_TARGET;
	    else if ( ev_btn == '2' ) cmd_id = CMD_DISPLAY_REFERENCE;
	    else if ( ev_btn == '3' ) cmd_id = CMD_DISPLAY_RESIDUAL1;
	    else if ( ev_btn == '4' ) cmd_id = CMD_DISPLAY_RESIDUAL2;
	    else if ( ev_btn == '5' ) cmd_id = CMD_DISPLAY_RESIDUAL4;
	    else if ( ev_btn == '6' ) cmd_id = CMD_DISPLAY_RESIDUAL8;
	    else if ( ev_btn == 'r' ) cmd_id = CMD_CONT_PLUS_R;
	    else if ( ev_btn == 'R' ) cmd_id = CMD_CONT_MINUS_R;
	    else if ( ev_btn == 'g' ) cmd_id = CMD_CONT_PLUS_G;
	    else if ( ev_btn == 'G' ) cmd_id = CMD_CONT_MINUS_G;
	    else if ( ev_btn == 'b' ) cmd_id = CMD_CONT_PLUS_B;
	    else if ( ev_btn == 'B' ) cmd_id = CMD_CONT_MINUS_B;
	    else if ( ev_btn == 13 ) cmd_id = CMD_SAVE;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn =='q' ) cmd_id = CMD_EXIT;
	    else if ( ev_btn == ' ' ) {
		if ( display_type == 0 ) cmd_id = CMD_DISPLAY_REFERENCE;
		else cmd_id = CMD_DISPLAY_TARGET;
	    }
	}

	if ( cmd_id == CMD_EXIT ) {
	    break;
	}
	else if ( cmd_id == CMD_SAVE ) {
	    tstring out_filename;
	    image_rgb_buf.clean();
	    update_image_buffer( display_type,
				 display_gain_r, display_gain_b,
				 offset_r_x, offset_r_y, offset_b_x, offset_b_y,
				 in_image_r_buf, in_image_g_buf, in_image_b_buf,
				 &image_rgb_buf, NULL, NULL );
	    if ( icc_buf.length() == 0 ) {
		icc_buf.resize_1d(sizeof(Icc_srgb_profile));
		icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
	    }
	    if ( in_image_r_buf.size_type() == UCHAR_ZT ) {
		make_output_filename(filename.cstr(), "aligned_rgb", "8bit",
				     &out_filename);
		sio.printf("Saved [%s]\n", out_filename.cstr());
		image_rgb_buf /= 256.0;
		if ( write_float_to_tiff24or48(image_rgb_buf, icc_buf, NULL, 
				0.0, 255.0, false, out_filename.cstr()) < 0 ) {
		    sio.eprintf("[ERROR] write_float_to_tiff24or48() failed.\n");
		    goto quit;
		}
		update_image = true;
	    }
	    else {
		make_output_filename(filename.cstr(), "aligned_rgb", "16bit",
				     &out_filename);
		sio.printf("Saved [%s]\n", out_filename.cstr());
		if ( write_float_to_tiff24or48(image_rgb_buf, icc_buf, NULL, 
			      0.0, 65535.0, false, out_filename.cstr()) < 0 ) {
		    sio.eprintf("[ERROR] write_float_to_tiff24or48() failed.\n");
		    goto quit;
		}
	    }
	}
	else if ( CMD_DISPLAY_TARGET <= cmd_id &&
		  cmd_id <= CMD_DISPLAY_RESIDUAL8 ) {
	    display_type = cmd_id - CMD_DISPLAY_TARGET;
	    refresh_gr = true;
	    refresh_gb = true;
	}
	else if ( cmd_id == CMD_CONT_PLUS_R ) {
	    contrast_rgb[0] ++;
	    update_display_gain_rb( contrast_rgb,
				    &display_gain_r, &display_gain_b );
	    save_display_params("display_1.txt", contrast_rgb);
	    update_image = true;
	}
	else if ( cmd_id == CMD_CONT_MINUS_R ) {
	    if ( 0 < contrast_rgb[0] ) {
		contrast_rgb[0] --;
		update_display_gain_rb( contrast_rgb,
					&display_gain_r, &display_gain_b );
		save_display_params("display_1.txt", contrast_rgb);
		update_image = true;
	    }
	}
	else if ( cmd_id == CMD_CONT_PLUS_G ) {
	    contrast_rgb[1] ++;
	    update_display_gain_rb( contrast_rgb,
				    &display_gain_r, &display_gain_b );
	    save_display_params("display_1.txt", contrast_rgb);
	    update_image = true;
	}
	else if ( cmd_id == CMD_CONT_MINUS_G ) {
	    if ( 0 < contrast_rgb[1] ) {
		contrast_rgb[1] --;
		update_display_gain_rb( contrast_rgb,
					&display_gain_r, &display_gain_b );
		save_display_params("display_1.txt", contrast_rgb);
		update_image = true;
	    }
	}
	else if ( cmd_id == CMD_CONT_PLUS_B ) {
	    contrast_rgb[2] ++;
	    update_display_gain_rb( contrast_rgb,
				    &display_gain_r, &display_gain_b );
	    save_display_params("display_1.txt", contrast_rgb);
	    update_image = true;
	}
	else if ( cmd_id == CMD_CONT_MINUS_B ) {
	    if ( 0 < contrast_rgb[2] ) {
		contrast_rgb[2] --;
		update_display_gain_rb( contrast_rgb,
					&display_gain_r, &display_gain_b );
		save_display_params("display_1.txt", contrast_rgb);
		update_image = true;
	    }
	}
	else if ( ev_type == KeyPress ) {
#if 0
	    //sio.printf("key = %d\n", ev_btn);
	    if ( ev_btn == '+' || ev_btn == ';' ) {	/* + key */
		if ( ev_win == win_gr ) {
		    display_gain_r += 10;
		    update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    display_gain_b += 10;
		    update_image = true;
		}
	    }
	    else if ( ev_btn == '-' ) {			/* - key */
		if ( ev_win == win_gr ) {
		    display_gain_r -= 10;
		    if ( display_gain_r < 10 ) display_gain_r += 10;
		    else update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    display_gain_b -= 10;
		    if ( display_gain_b < 10 ) display_gain_b += 10;
		    else update_image = true;
		}
	    }
#endif
	    if ( ev_btn == 28 ) {		/* Right key */
		if ( ev_win == win_gr ) {
		    offset_r_x ++;
		    update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    offset_b_x ++;
		    update_image = true;
		}
	    }
	    else if ( ev_btn == 29 ) {	/* Left key */
		if ( ev_win == win_gr ) {
		    offset_r_x --;
		    update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    offset_b_x --;
		    update_image = true;
		}
	    }
	    else if ( ev_btn == 30 ) {	/* Up key */
		if ( ev_win == win_gr ) {
		    offset_r_y --;
		    update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    offset_b_y --;
		    update_image = true;
		}
	    }
	    else if ( ev_btn == 31 ) {	/* Down key */
		if ( ev_win == win_gr ) {
		    offset_r_y ++;
		    update_image = true;
		}
		else if ( ev_win == win_gb ) {
		    offset_b_y ++;
		    update_image = true;
		}
	    }
	}


	if ( update_image == true ) {
	    const char *im_str;
	    update_image_buffer( display_type,
				 display_gain_r, display_gain_b,
				 offset_r_x, offset_r_y, offset_b_x, offset_b_y,
				 in_image_r_buf, in_image_g_buf, in_image_b_buf,
				 &image_rgb_buf, &image_gr_buf, &image_gb_buf );
	    display_image( win_rgb, image_rgb_buf, contrast_rgb, 0, &tmp_buf );
	    im_str = "Residual";
	    if ( display_type == 0 ) {
		display_image(win_gr, image_gr_buf, contrast_rgb, 0, &tmp_buf);
		im_str = "Red";
	    }
	    else {
		display_image(win_gr, image_gr_buf, contrast_rgb, 1, &tmp_buf);
		if ( display_type == 1 ) im_str = "Green";
	    }
	    winname(win_gr, "G and R  offset_r_x = %ld  offset_r_y = %ld  "
		    "[%s]", offset_r_x, offset_r_y, im_str);
	    im_str = "Residual";
	    if ( display_type == 0 ) {
		display_image(win_gb, image_gb_buf, contrast_rgb, 2, &tmp_buf);
		im_str = "Blue";
	    }
	    else {
		display_image(win_gb, image_gb_buf, contrast_rgb, 1, &tmp_buf);
		if ( display_type == 1 ) im_str = "Green";
	    }
	    winname(win_gb, "G and B  offset_b_x = %ld  offset_b_y = %ld  "
		    "[%s]", offset_b_x, offset_b_y, im_str);
	}

	if ( refresh_gr == true ) {
	    const char *im_str = "Residual";
	    update_image_buffer( display_type,
				 display_gain_r, display_gain_b,
				 offset_r_x, offset_r_y, offset_b_x, offset_b_y,
				 in_image_r_buf, in_image_g_buf, in_image_b_buf,
				 NULL, &image_gr_buf, NULL );
	    if ( display_type == 0 ) {
		display_image(win_gr, image_gr_buf, contrast_rgb, 0, &tmp_buf);
		im_str = "Red";
	    }
	    else {
		display_image(win_gr, image_gr_buf, contrast_rgb, 1, &tmp_buf);
		if ( display_type == 1 ) im_str = "Green";
	    }
	    winname(win_gr, "G and R  offset_r_x = %ld  offset_r_y = %ld  "
		    "[%s]", offset_r_x, offset_r_y, im_str);
	}

	if ( refresh_gb == true ) {
	    const char *im_str = "Residual";
	    update_image_buffer( display_type,
				 display_gain_r, display_gain_b,
				 offset_r_x, offset_r_y, offset_b_x, offset_b_y,
				 in_image_r_buf, in_image_g_buf, in_image_b_buf,
				 NULL, NULL, &image_gb_buf );
	    if ( display_type == 0 ) {
		display_image(win_gb, image_gb_buf, contrast_rgb, 2, &tmp_buf);
		im_str = "Blue";
	    }
	    else {
		display_image(win_gb, image_gb_buf, contrast_rgb, 1, &tmp_buf);
		if ( display_type == 1 ) im_str = "Green";
	    }
	    winname(win_gb, "G and B  offset_b_x = %ld  offset_b_y = %ld  "
		    "[%s]", offset_b_x, offset_b_y, im_str);
	}

	
    }

    //ggetch();
    
    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_separate_buffer.cc"
#include "write_float_to_tiff24or48.cc"
