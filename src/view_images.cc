/*
 * $ s++ view_images.cc -leggx -lX11 -ltiff
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

#include "read_tiff24or48.h"
#include "get_bin_factor_for_display.c"
#include "load_display_params.cc"
#include "save_display_params.cc"
//#include "make_output_filename.cc"
//#include "icc_srgb_profile.c"

/**
 * @file   view_images.cc
 * @brief  TIFF image viewer.
 *         8/16-bit integer and 32-bit float images are supported.
 */

const int Font_y_off = 3;
const int Font_margin = 2;
static int Fontsize = 14;
#include "set_fontsize.c"
#include "command_window.c"
#include "display_file_list.cc"
#include "display_image.cc"
const int Loupe_width = 200;

static bool test_file( const char *file )
{
    stdstreamio f_in;
    tstring filename = file;
    ssize_t len_file = (ssize_t)filename.length();
    bool is_tiff_name = false;
    bool ret_value = false;

    if ( file == NULL ) goto quit;
    
    if ( filename.rfind(".tif") + 4 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".tiff") + 5 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".TIF") + 4 == len_file ) is_tiff_name = true;
    else if ( filename.rfind(".TIFF") + 5 == len_file ) is_tiff_name = true;

    if ( is_tiff_name == true ) {
	if ( f_in.open("r", file) == 0 ) {
	    f_in.close();
	    ret_value =  true;
	}
    }

 quit:
    return ret_value;
}

static int get_dirname( const char *filename, tstring *ret_dir )
{
    ssize_t ix;
    int ret = -1;

    if ( filename == NULL ) goto quit;
    if ( ret_dir == NULL ) goto quit;

    *ret_dir = filename;

    ix = ret_dir->rfind('/');

    if ( 0 <= ix ) {
	ret_dir->resize(ix);
	if ( ret_dir->length() == 0 ) {
	    ret_dir->assign(".");
	}
    }
    else {
	ret_dir->assign(".");
    }

    ret = 0;

 quit:
    return ret;
}

static int perform_aphoto( int win_image, const mdarray &img_buf, int tiff_szt,
			   int display_bin, int display_ch, int display_type,
			   const int contrast_rgb[], mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    mdarray_float stat_buf(false);
    float *const *const *stat_buf_ptr;
    mdarray_float stat_buf_1d(false);
    size_t width, height, cnt_obj_pixels, cnt_sky_pixels;
    double obj_x, obj_y, obj_r, sky_r;
    int step_count;
    bool flag_drawed = false;
    size_t i, j;
    double sky_lv[3];
    double obj_cnt[3];
    int ret_status = -1;

    newgcfunction(win_image, GXxor);
    newcolor(win_image, "green");

    width = img_buf.x_length();
    height = img_buf.y_length();

    obj_x = -32000;
    obj_y = -32000;
    obj_r = 65535;
    sky_r = 65535;
    step_count = 0;

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;
	bool refresh_image = false;

	ev_win = ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);
	if ( ev_win == win_image ) {
	    if ( ev_type == KeyPress ) {
		if ( ev_btn == 27 || ev_btn == 'q' ) {
		    goto quit;
		}
		else if ( ev_btn == 8 /* backspace */ ) {
		    if ( 0 < step_count ) {
			step_count --;
			if ( step_count == 0 ) obj_r = 65535;
			else if ( step_count == 1 ) sky_r = 65535;
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
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
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
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
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
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
		    if ( obj_r < sky_r && sky_r < width && sky_r < height ) {
			break;
		    }
		}
	    }
	}

	if ( refresh_image == true ) {
	    if ( display_type == 1 ) {
		newgcfunction(win_image, GXcopyInverted);
	    }
	    else {
		newgcfunction(win_image, GXcopy);
	    }
	    display_image(win_image, 0, 0, img_buf, tiff_szt,
			display_bin, display_ch, contrast_rgb, false, tmp_buf);
	    newgcfunction(win_image, GXxor);
	    flag_drawed = false;

	    if ( 0 < step_count ) {
		drawline(win_image, 0, obj_y, width, obj_y);
		drawline(win_image, obj_x, 0, obj_x, height);
	    }
	    if ( 1 < step_count ) {
		    drawcirc(win_image, obj_x, obj_y, obj_r, obj_r);
	    }
	}
    }

    newgcfunction(win_image, GXcopy);
    newcolor(win_image, "white");

    /*
     * start statistics
     */
    stat_buf.resize_3d(sky_r * 2, sky_r *2, 3);
    stat_buf_1d.resize_2d(stat_buf.x_length(), stat_buf.y_length());
    /* get 3-d array ptr */
    stat_buf_ptr = stat_buf.array_ptr_3d(true);

    /* sky */
    stat_buf = (float)NAN;
    stat_buf.paste(img_buf, - obj_x + sky_r, - obj_y + sky_r);

    cnt_sky_pixels = 0;
    for ( i=0 ; i < stat_buf.y_length() ; i++ ) {
	for ( j=0 ; j < stat_buf.x_length() ; j++ ) {
	    double r;
	    r = sqrt(pow(j - sky_r,2)+pow(i - sky_r,2));
	    if ( r < obj_r || sky_r < r ) {
		stat_buf_ptr[0][i][j] = NAN;
		stat_buf_ptr[1][i][j] = NAN;
		stat_buf_ptr[2][i][j] = NAN;
	    }
	    else {
		cnt_sky_pixels ++;
	    }
	}
    }

    for ( i=0 ; i < 3 ; i++ ) {
	stat_buf.copy(&stat_buf_1d,
		      0, stat_buf.x_length(), 0, stat_buf.y_length(), i, 1);
	sky_lv[i] = md_median(stat_buf_1d);
    }

    /* object */
    stat_buf = (float)NAN;
    stat_buf.paste(img_buf, - obj_x + sky_r, - obj_y + sky_r);

    cnt_obj_pixels = 0;
    for ( i=0 ; i < stat_buf.y_length() ; i++ ) {
	for ( j=0 ; j < stat_buf.x_length() ; j++ ) {
	    double r;
	    r = sqrt(pow(j - sky_r,2)+pow(i - sky_r,2));
	    if ( obj_r < r ) {
		stat_buf_ptr[0][i][j] = NAN;
		stat_buf_ptr[1][i][j] = NAN;
		stat_buf_ptr[2][i][j] = NAN;
	    }
	    else {
		cnt_obj_pixels ++;
	    }
	}
    }

#if 0	/* test! */
    newgcfunction(win_image, GXcopy);
    display_image(win_image, 0, 0, stat_buf, tiff_szt, display_bin, display_ch,
		  contrast_rgb, false, tmp_buf);
    winname(win_image, "TEST!");
    ggetch();
#endif

    for ( i=0 ; i < 3 ; i++ ) {
	stat_buf.copy(&stat_buf_1d,
		      0, stat_buf.x_length(), 0, stat_buf.y_length(), i, 1);
	obj_cnt[i] = md_total(stat_buf_1d);
    }

    /* display */
    sio.printf("** Aperture Photometry **\n");
    sio.printf("obj_x                     = %g\n",obj_x);
    sio.printf("obj_y                     = %g\n",obj_y);
    sio.printf("obj_r                     = %g\n",obj_r);
    sio.printf("sky_r                     = %g\n",sky_r);
    sio.printf("sky_area                  = %zu pixels\n",cnt_sky_pixels);
    sio.printf("obj_area                  = %zu pixels\n",cnt_obj_pixels);
    sio.printf("[Red]\n");
    sio.printf("sky_median                = %g\n",sky_lv[0]);
    sio.printf("obj-area_count            = %g\n",obj_cnt[0]);
    sio.printf("obj_count(sky-subtracted) = %g\n",
	       obj_cnt[0] - (cnt_obj_pixels * sky_lv[0]));
    sio.printf("[Green]\n");
    sio.printf("sky_median                = %g\n",sky_lv[1]);
    sio.printf("obj-area_count            = %g\n",obj_cnt[1]);
    sio.printf("obj_count(sky-subtracted) = %g\n",
	       obj_cnt[1] - (cnt_obj_pixels * sky_lv[1]));
    sio.printf("[Blue]\n");
    sio.printf("sky_median                = %g\n",sky_lv[2]);
    sio.printf("obj-area_count            = %g\n",obj_cnt[2]);
    sio.printf("obj_count(sky-subtracted) = %g\n",
	       obj_cnt[2] - (cnt_obj_pixels * sky_lv[2]));
    sio.printf("-------------------------\n");

    ret_status = 0;

 quit:
    return ret_status;
}

const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Normal            [1]"},
#define CMD_DISPLAY_INVERT 2
        {CMD_DISPLAY_INVERT,    "Display Invert            [2]"},
#define CMD_DISPLAY_RGB 3
        {CMD_DISPLAY_RGB,       "Display RGB               [c]"},
#define CMD_DISPLAY_R 4
        {CMD_DISPLAY_R,         "Display Red               [c]"},
#define CMD_DISPLAY_G 5
        {CMD_DISPLAY_G,         "Display Green             [c]"},
#define CMD_DISPLAY_B 6
        {CMD_DISPLAY_B,         "Display Blue              [c]"},
#define CMD_ZOOM 7
        {CMD_ZOOM,              "Zoom +/-                  [+][-]"},
#define CMD_CONT_RGB 8
        {CMD_CONT_RGB,          "RGB Contrast +/-          [<][>]"},
#define CMD_CONT_R 9
        {CMD_CONT_R,            "Red Contrast +/-          [r][R]"},
#define CMD_CONT_G 10
        {CMD_CONT_G,            "Green Contrast +/-        [g][G]"},
#define CMD_CONT_B 11
        {CMD_CONT_B,            "Blue Contrast +/-         [b][B]"},
#define CMD_IMSTAT 12
        {CMD_IMSTAT,            "Image statistics          [s]"},
#define CMD_APHOTO 13
        {CMD_APHOTO,            "Aperture Photometry       [p]"},
#define CMD_AUTO_ZOOM 14
        {CMD_AUTO_ZOOM,         "Auto zoom on/off for loading [z]"},
#define CMD_DITHER 15
        {CMD_DITHER,            "Dither on/off for saving  [d]"},
#define CMD_SAVE_8BIT 16
        {CMD_SAVE_8BIT,         "Save as 8-bit TIFF"},
#define CMD_SAVE_16BIT 17
        {CMD_SAVE_16BIT,        "Save as 16-bit TIFF"},
#define CMD_SAVE_FLOAT 18
        {CMD_SAVE_FLOAT,        "Save as 32-bit float TIFF"},
#define CMD_EXIT 19
        {CMD_EXIT,              "Exit                      [q]"},
        {0, NULL}		/* EOL */
};

int main( int argc, char *argv[] )
{
    const char *conf_file_display = "display_0.txt";

    stdstreamio sio, f_in;
    pipestreamio p_in;
    tarray_tstring filenames;
    tstring filename_1st;
    tstring dirname;
    size_t maxlen_filename;
    int sel_file_id = 0;
   
    command_win command_win_rec;
    int win_filesel, win_image;

    mdarray img_buf(UCHAR_ZT,false);	/* buffer for target */
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    int tiff_szt = 0;
    
    int display_type = 0;		/* flag to display image type */
    int display_ch = 0;			/* 0=RGB 1=R 2=G 3=B */
    int display_bin = 1;		/* binning factor for display */
    int contrast_rgb[3] = {8, 8, 8};	/* contrast for display */

    bool flag_auto_zoom = true;
    bool flag_dither = true;

    const char *names_ch[] = {"RGB", "Red", "Green", "Blue"};
    
    size_t i;
    int arg_cnt;

    int return_status = -1;

    load_display_params(conf_file_display, contrast_rgb);

    i=0;
    for ( arg_cnt=1 ; arg_cnt < argc ; arg_cnt++ ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( 0 ) {
	    /* NOP */
	}
	else {
	    if ( test_file( argv[arg_cnt] ) == true ) {
		ssize_t ix;
		filenames[i] = argv[arg_cnt];
		while ( filenames[i].find("./") == 0 ) {
		    filenames[i].erase(0,2);
		}
		while ( 0 < (ix=filenames[i].find("/./")) ) {
		    filenames[i].erase(ix,2);
		}
		if ( filename_1st.length() == 0 ) {
		    filename_1st = filenames[i];
		}
		i++;
	    }
	}
    }

    if ( filenames.length() <= 1 ) {
	if ( 0 < filename_1st.length() ) {
	    get_dirname(filename_1st.cstr(), &dirname);
	}
	else dirname = ".";
	//sio.eprintf("[INFO] dirname = '%s'\n",dirname.cstr());
	/* */
	if ( p_in.openf("r",
			"/bin/ls %s | grep -i -e '[.]tiff$' -e '[.]tif$'",
			dirname.cstr()) < 0 ) {
	    sio.eprintf("[ERROR] cannot open pipe.\n");
	    goto quit;
	}
	i = 0;
	while ( 1 ) {
	    const char *v = p_in.getline();
	    if ( v == NULL ) break;
	    if ( dirname != "." ) {
		filenames[i].printf("%s/%s",dirname.cstr(),v);
	    }
	    else {
		filenames[i] = v;
	    }
	    filenames[i].trim();
	    if ( filenames[i] == filename_1st ) sel_file_id = i;
	    i++;
	}
	//filenames.dprint();
	p_in.close();
    }

    /* get maxlen_filename */
    maxlen_filename = 0;
    for ( i=0 ; i < filenames.length() ; i++ ) {
	if ( maxlen_filename < filenames[i].length() ) {
	    maxlen_filename = filenames[i].length();
	}
    }

    if ( maxlen_filename == 0 ) {
        sio.eprintf("[ERROR] cannot find TIFF files.\n");
	goto quit;
    }

    
    /*
     * GRAPHICS
     */
    
    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;

    /* Font selector */
    set_fontsize();
    
    /* command window */
    
    command_win_rec = gopen_command_window( Cmd_list,
	   Font_margin + 2*Fontsize + Font_margin + Loupe_width + Font_margin);

    
    /* file selector */
    
    win_filesel = gopen((Fontsize/2) * (maxlen_filename + 2),
			Fontsize * (filenames.length()));
    gsetbgcolor(win_filesel,"#404040");
    gclr(win_filesel);
    winname(win_filesel, "File Selector");
    
    display_file_list(win_filesel, filenames, sel_file_id, false, -1, NULL);

    /* image viewer */

    sio.printf("Open: %s\n", filenames[sel_file_id].cstr());
    if ( read_tiff24or48(filenames[sel_file_id].cstr(),
			 &img_buf, &tiff_szt, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48() failed\n");
	goto quit;
    }

    display_bin = get_bin_factor_for_display(img_buf.x_length(),
					     img_buf.y_length(), true);
    if ( display_bin < 0 ) {
        sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
		    "bad display depth\n");
	goto quit;
    }

    win_image = gopen(img_buf.x_length() / display_bin,
		      img_buf.y_length() / display_bin);
    
    /* display reference image */
    display_image(win_image, 0, 0, img_buf, tiff_szt,
		  display_bin, display_ch, contrast_rgb, true, &tmp_buf);

    winname(win_image, "Imave Viewer  "
	    "zoom = 1/%d  contrast = ( %d, %d, %d )  "
	    "auto_zoom = %d  dither = %d",
	    display_bin,
	    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
	    (int)flag_auto_zoom, (int)flag_dither);

    
    /*
     * MAIN EVENT LOOP
     */

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        bool refresh_list = false;
        int refresh_image = 0;		/* 1:display only  2:both */
	bool refresh_loupe = false;
	bool refresh_winsize = false;
        bool refresh_winname = false;

	int f_id = -1;
	int cmd_id = -1;
	bool flag_file_selector = false;
	
        /* waiting an event */
        ev_win = ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);

	/*
	 *  Check file selector
	 */
	if ( ev_type == ButtonPress && ev_btn == 1 &&
	     ev_win == win_filesel ) {
	    flag_file_selector = true;
	    f_id = ev_y / Fontsize;
	}
	else if ( ev_type == KeyPress && ev_btn == 6 /* PageDown */ ) {
	    flag_file_selector = true;
	    f_id = sel_file_id + 1;
	}
	else if ( ev_type == KeyPress && ev_btn == 2 /* PageUp */ ) {
	    flag_file_selector = true;
	    f_id = sel_file_id - 1;
	}
	
	if ( f_id != sel_file_id &&
	     0 <= f_id && (size_t)f_id < filenames.length() ) {

	    display_file_list(win_filesel, filenames, f_id, true, -1, NULL);
	    
	    sio.printf("Open: %s\n", filenames[f_id].cstr());
		    
	    if ( read_tiff24or48(filenames[f_id].cstr(), 
				 &img_buf, &tiff_szt, NULL, NULL) < 0 ) {
	        sio.eprintf("[ERROR] read_tiff24or48() failed\n");
		sel_file_id = -1;
	    }
	    else {

		sel_file_id = f_id;
		//sio.printf("%ld\n", sel_file_id);

		if ( flag_auto_zoom == true ) {
		    display_bin = get_bin_factor_for_display(img_buf.x_length(),
						     img_buf.y_length(), true);
		}
		if ( display_bin < 0 ) {
		    sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
				"bad display depth\n");
		    goto quit;
		}

		refresh_image = 2;
		refresh_winsize = true;
		refresh_list = true;

	    }
	}

	/*
	 *  Check command window
	 */
	
	if ( flag_file_selector == true ) {
	    /* NOP */
	}
	else if ( ev_type == ButtonPress && 1 <= ev_btn && ev_btn <= 3 &&
		  ev_win == command_win_rec.win_id ) {
	    cmd_id = 1 + ev_y / command_win_rec.cell_height;
	}
	else if ( ev_type == KeyPress ) {
	    //sio.printf("[%d]\n", ev_btn);
	    if ( ev_btn == '1' ) cmd_id = CMD_DISPLAY_TARGET;
	    else if ( ev_btn == '2' ) cmd_id = CMD_DISPLAY_INVERT;
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
	    else if ( ev_btn == 's' ) cmd_id = CMD_IMSTAT;
	    else if ( ev_btn == 'p' ) cmd_id = CMD_APHOTO;
	    else if ( ev_btn == 'z' ) cmd_id = CMD_AUTO_ZOOM;
	    else if ( ev_btn == 'd' ) cmd_id = CMD_DITHER;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn == 'q' ) cmd_id = CMD_EXIT;
	}

	/*
	 *  Handle cmd_id
	 */
	
	if ( cmd_id == CMD_EXIT ) {
	    break;
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
	    if ( 1 < display_bin ) {
		if ( display_bin <= 4 ) display_bin --;
		else display_bin -= 2;
		refresh_image = 1;
		refresh_winsize = true;
	    }
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 3 ) {
	    if ( display_bin < 10 ) {
		if ( display_bin < 4 ) display_bin ++;
		else display_bin += 2;
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
	else if ( cmd_id == CMD_AUTO_ZOOM ) {
	    if ( flag_auto_zoom == true ) flag_auto_zoom = false;
	    else flag_auto_zoom = true;
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_DITHER ) {
	    if ( flag_dither == true ) flag_dither = false;
	    else flag_dither = true;
	    refresh_winname = true;
	}

	/*
	 * Only when *SELECTED*
	 */

	if ( 0 <= sel_file_id ) {
	  
	    if ( CMD_DISPLAY_TARGET <= cmd_id &&
		 cmd_id <= CMD_DISPLAY_INVERT ) {
		display_type = cmd_id - CMD_DISPLAY_TARGET;
		refresh_image = 2;
	    }
	    else if ( cmd_id == CMD_IMSTAT ) {
		mdarray tmpim(img_buf.size_type(),false);
		size_t ch;
		tmpim.resize_2d(img_buf.x_length(), img_buf.y_length());
		sio.printf("*** Image Statistics ***\n");
		sio.printf("bps     = %d\n",tiff_szt);
		sio.printf("width   = %zu\n",img_buf.x_length());
		sio.printf("height  = %zu\n",img_buf.y_length());
		for ( ch=0 ; ch < 3 ; ch++ ) {
		    double v;
		    if ( ch==0 ) sio.printf("[Red]\n");
		    else if ( ch==1 ) sio.printf("[Green]\n");
		    else sio.printf("[Blue]\n");
		    img_buf.copy(&tmpim,
			  0, img_buf.x_length(), 0, img_buf.y_length(), 0, ch);
		    v = md_total(tmpim);
		    sio.printf("total   = %g\n",v);
		    v = md_median(tmpim);
		    sio.printf("median  = %g\n",v);
		    v = md_mean(tmpim);
		    sio.printf("mean    = %g\n",v);
		    v = md_stddev(tmpim);
		    sio.printf("stddev  = %g\n",v);
		    v = md_min(tmpim);
		    sio.printf("min     = %g\n",v);
		    v = md_max(tmpim);
		    sio.printf("max     = %g\n",v);
		}
		sio.printf("-------------------------\n");
	    }
	    else if ( cmd_id == CMD_APHOTO ) {
		perform_aphoto( win_image, img_buf, tiff_szt,
				display_bin, display_ch,
				display_type, contrast_rgb, &tmp_buf );
		refresh_image = 1;
	    }
	    /* */
	    else if ( ev_win == win_image &&
		      ( ev_type == MotionNotify || 
			ev_type == EnterNotify || ev_type == LeaveNotify ) ) {
                if ( ev_type == LeaveNotify ) {
                    ev_x = -32000;  ev_y = -32000;
                }
		layer(command_win_rec.win_id, 0, 2);
		copylayer(command_win_rec.win_id, 1, 2);
		if ( 0 <= ev_x && 0 <= ev_y ) {
		    drawstr(command_win_rec.win_id,
			Font_margin,
			command_win_rec.reserved_y0 + Font_margin + Fontsize,
			Fontsize, 0, "x=%g y=%g", ev_x, ev_y);
		    drawstr(command_win_rec.win_id,
			Font_margin,
			command_win_rec.reserved_y0 + Font_margin + 2*Fontsize,
			Fontsize, 0, "RGB=%5g %5g %5g",
			img_buf.dvalue((ssize_t)ev_x, (ssize_t)ev_y, 0),
			img_buf.dvalue((ssize_t)ev_x, (ssize_t)ev_y, 1),
			img_buf.dvalue((ssize_t)ev_x, (ssize_t)ev_y, 2) );
		}
		copylayer(command_win_rec.win_id, 2, 0);
		refresh_loupe = true;
	    }
	    else if ( ev_type == KeyPress ) {
		if ( ev_btn == ' ' ) {
		    if ( display_type == 0 ) display_type = 1;
		    else display_type = 0;
		    refresh_image = 2;
		}
	    }
	}

	/* Update window */
	    
	if ( refresh_image != 0 ) {
	    if ( display_type == 1 ) {
		newgcfunction(win_image, GXcopyInverted);
	    }
	    else {
		newgcfunction(win_image, GXcopy);
	    }
	    //
	    display_image(win_image, 0, 0, img_buf, tiff_szt,
			  display_bin, display_ch, contrast_rgb,
			  refresh_winsize, &tmp_buf);
	    winname(win_image, "Image Viewer  "
		    "channel = %s  zoom = 1/%d  contrast = ( %d, %d, %d )  "
		    "auto_zoom = %d  dither = %d  ",
		    names_ch[display_ch], display_bin,
		    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
		    (int)flag_auto_zoom, (int)flag_dither);
	}

	if ( refresh_winname == true ) {
	    winname(win_image, "Image Viewer  "
		    "channel = %s  zoom = 1/%d  contrast = ( %d, %d, %d )  "
		    "auto_zoom = %d  dither = %d  ",
		    names_ch[display_ch], display_bin,
		    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
		    (int)flag_auto_zoom, (int)flag_dither);
	}

	if ( refresh_list == true ) {

	    display_file_list(win_filesel, filenames, sel_file_id, false,
			      -1, NULL);

	}

    }
    //ggetch();

    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48.cc"
