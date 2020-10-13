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

#include "file_io.h"
#include "tiff_funcs.h"
#include "display_image.h"
#include "gui_base.h"
#include "loupe_funcs.h"

using namespace sli;


/**
 * @file   view_images.cc
 * @brief  TIFF image viewer.
 *         8/16-bit integer and 32-bit float images are supported.
 */


typedef struct _imstat {
    double total[3];
    double median[3];
    double mean[3];
    double stddev[3];
    double min[3];
    double max[3];
} imstat;

typedef struct _aphoto {
    double obj_x;
    double obj_y;
    double obj_r;
    double sky_r;
    double obj_area;
    double sky_area;
    double sky_median[3];
    double sky_stddev[3];
    double obj_area_cnt[3];
    double obj_cnt[3];
} aphoto;

static int init_aphoto( aphoto *rec_p )
{
    rec_p->obj_x = -32000;
    rec_p->obj_y = -32000;
    rec_p->obj_r = -1;
    rec_p->sky_r = -1;
    rec_p->obj_area = 0;
    rec_p->sky_area = 0;
    rec_p->sky_median[0] = NAN;
    rec_p->sky_median[1] = NAN;
    rec_p->sky_median[2] = NAN;
    rec_p->sky_stddev[0] = NAN;
    rec_p->sky_stddev[1] = NAN;
    rec_p->sky_stddev[2] = NAN;
    rec_p->obj_area_cnt[0] = NAN;
    rec_p->obj_area_cnt[1] = NAN;
    rec_p->obj_area_cnt[2] = NAN;
    rec_p->obj_cnt[0] = NAN;
    rec_p->obj_cnt[1] = NAN;
    rec_p->obj_cnt[2] = NAN;

    return 0;
}

static int perform_aphoto( const mdarray &img_buf, int tiff_szt,
			   aphoto *rec_p )
{
    stdstreamio sio;
    mdarray_float stat_buf(false);
    float *const *const *stat_buf_ptr;
    mdarray_float stat_buf_1d(false);
    size_t cnt_obj_pixels, cnt_sky_pixels;
    double obj_x = rec_p->obj_x;
    double obj_y = rec_p->obj_y;
    double obj_r = rec_p->obj_r;
    double sky_r = rec_p->sky_r;
    size_t i, j;
    int ret_status = -1;

    /*
     * start statistics
     */
    stat_buf.resize_3d(sky_r * 2, sky_r * 2, 3);
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
	rec_p->sky_median[i] = md_median(stat_buf_1d);
	rec_p->sky_stddev[i] = md_stddev(stat_buf_1d);
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

    rec_p->obj_area = cnt_obj_pixels;
    rec_p->sky_area = cnt_sky_pixels;

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
	rec_p->obj_area_cnt[i] = md_total(stat_buf_1d);
	rec_p->obj_cnt[i] = rec_p->obj_area_cnt[i] - (cnt_obj_pixels * rec_p->sky_median[i]);
    }

    /* display */
    sio.printf("** Aperture Photometry **\n");
    sio.printf("obj_x                     = %g\n",obj_x);
    sio.printf("obj_y                     = %g\n",obj_y);
    sio.printf("obj_r                     = %g\n",obj_r);
    sio.printf("sky_r                     = %g\n",sky_r);
    sio.printf("obj_area                  = %zu pixels\n",cnt_obj_pixels);
    sio.printf("sky_area                  = %zu pixels\n",cnt_sky_pixels);
    sio.printf("[Red]\n");
    sio.printf("sky_median                = %g\n",rec_p->sky_median[0]);
    sio.printf("sky_stddev                = %g\n",rec_p->sky_stddev[0]);
    sio.printf("obj-area_count            = %g\n",rec_p->obj_area_cnt[0]);
    sio.printf("obj_count(sky-subtracted) = %g\n",rec_p->obj_cnt[0]);
    sio.printf("[Green]\n");
    sio.printf("sky_median                = %g\n",rec_p->sky_median[1]);
    sio.printf("sky_stddev                = %g\n",rec_p->sky_stddev[1]);
    sio.printf("obj-area_count            = %g\n",rec_p->obj_area_cnt[1]);
    sio.printf("obj_count(sky-subtracted) = %g\n",rec_p->obj_cnt[1]);
    sio.printf("[Blue]\n");
    sio.printf("sky_median                = %g\n",rec_p->sky_median[2]);
    sio.printf("sky_stddev                = %g\n",rec_p->sky_stddev[2]);
    sio.printf("obj-area_count            = %g\n",rec_p->obj_area_cnt[2]);
    sio.printf("obj_count(sky-subtracted) = %g\n",rec_p->obj_cnt[2]);
    sio.printf("-------------------------\n");

    ret_status = 0;

    return ret_status;
}

static void draw_lines_for_aphoto( int win, size_t width, size_t height,
				   const aphoto &aphoto_rec, int step )
{
    if ( 0 < step ) {
	drawline(win, 0, aphoto_rec.obj_y, width, aphoto_rec.obj_y);
	drawline(win, aphoto_rec.obj_x, 0, aphoto_rec.obj_x, height);
	if ( 0 < aphoto_rec.obj_r ) {
	    drawcirc(win, aphoto_rec.obj_x, aphoto_rec.obj_y,
		 aphoto_rec.obj_r, aphoto_rec.obj_r);
	    drawstr(win,
		    aphoto_rec.obj_x + get_font_margin() + aphoto_rec.obj_r,
		    aphoto_rec.obj_y - get_font_margin(),
		    get_fontsize(), 0, "r=%.2f", aphoto_rec.obj_r);
	}
	if ( 0 < aphoto_rec.sky_r ) {
	    drawcirc(win, aphoto_rec.obj_x, aphoto_rec.obj_y,
		     aphoto_rec.sky_r, aphoto_rec.sky_r);
	    drawstr(win,
		    aphoto_rec.obj_x + get_font_margin(),
		    aphoto_rec.obj_y - get_font_margin() - aphoto_rec.sky_r,
		    get_fontsize(), 0, "r=%.2f", aphoto_rec.sky_r);
	}
    }
    return;
}

static void append_aphoto_log( const char *filename, int tiff_szt,
			       const aphoto &aphoto_rec,
			       tarray_tstring *aphoto_log_p )
{
    tstring log;
    
    if ( aphoto_log_p->length() < 1 ) {
	log.printf("#filename,bps,"
		   "obj_x,obj_y,obj_r,sky_r,obj_area,sky_area,"
		   "sky_median[R],sky_median[G],sky_median[B],"
		   "sky_stddev[R],sky_stddev[G],sky_stddev[B],"
		   "obj_cnt[R],obj_cnt[G],obj_cnt[B]");
	aphoto_log_p->append(log, 1);
    }
    log.printf("%c%s%c,%d,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g",
	       34,filename,34, tiff_szt,
	       aphoto_rec.obj_x, aphoto_rec.obj_y,
	       aphoto_rec.obj_r, aphoto_rec.sky_r,
	       aphoto_rec.obj_area, aphoto_rec.sky_area,
	       aphoto_rec.sky_median[0],
	       aphoto_rec.sky_median[1],
	       aphoto_rec.sky_median[2],
	       aphoto_rec.sky_stddev[0],
	       aphoto_rec.sky_stddev[1],
	       aphoto_rec.sky_stddev[2],
	       aphoto_rec.obj_cnt[0],
	       aphoto_rec.obj_cnt[1],
	       aphoto_rec.obj_cnt[2]);
    aphoto_log_p->append(log, 1);

    return;
}


const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Normal                 [1]"},
#define CMD_DISPLAY_INVERT 2
        {CMD_DISPLAY_INVERT,    "Display Invert                 [2]"},
#define CMD_DISPLAY_RGB 3
        {CMD_DISPLAY_RGB,       "Display RGB                    [c]"},
#define CMD_DISPLAY_R 4
        {CMD_DISPLAY_R,         "Display Red                    [c]"},
#define CMD_DISPLAY_G 5
        {CMD_DISPLAY_G,         "Display Green                  [c]"},
#define CMD_DISPLAY_B 6
        {CMD_DISPLAY_B,         "Display Blue                   [c]"},
#define CMD_ZOOM 7
        {CMD_ZOOM,              "Zoom +/-                       [+][-]"},
#define CMD_CONT_RGB 8
        {CMD_CONT_RGB,          "RGB Contrast +/-               [<][>]"},
#define CMD_CONT_R 9
        {CMD_CONT_R,            "Red Contrast +/-               [r][R]"},
#define CMD_CONT_G 10
        {CMD_CONT_G,            "Green Contrast +/-             [g][G]"},
#define CMD_CONT_B 11
        {CMD_CONT_B,            "Blue Contrast +/-              [b][B]"},
#define CMD_LOUPE_ZOOM 12
        {CMD_LOUPE_ZOOM,        "Zoom factor of loupe +/-       [l][L]"},
#define CMD_IMSTAT 13
        {CMD_IMSTAT,            "Image statistics               [s]"},
#define CMD_APHOTO 14
        {CMD_APHOTO,            "Aperture Photometry / Reuse Ap [p][P]"},
#define CMD_SAVE_SLOG 15
        {CMD_SAVE_SLOG,         "Save Log of Statistics         [Enter]"},
#define CMD_AUTO_ZOOM 16
        {CMD_AUTO_ZOOM,         "Auto zoom on/off for loading   [z]"},
#define CMD_DITHER 17
        {CMD_DITHER,            "Dither on/off for saving       [d]"},
#define CMD_SAVE_8BIT 18
        {CMD_SAVE_8BIT,         "Save as 8-bit TIFF"},
#define CMD_SAVE_16BIT 19
        {CMD_SAVE_16BIT,        "Save as 16-bit TIFF"},
#define CMD_SAVE_FLOAT 20
        {CMD_SAVE_FLOAT,        "Save as 32-bit float TIFF"},
#define CMD_EXIT 21
        {CMD_EXIT,              "Exit                          [q][ESC]"},
        {0, NULL}		/* EOL */
};

int main( int argc, char *argv[] )
{
    const char *conf_file_display = "display_0.txt";

    stdstreamio sio, f_in, f_out;
    pipestreamio p_in;
    tarray_tstring filenames;
    tstring filename_1st;
    tstring dirname;
    int sel_file_id = 0;
   
    command_win command_win_rec;
    size_t max_len_menu_string;
    int win_filesel, win_image;

    mdarray img_buf(UCHAR_ZT,false);	/* buffer for target */
    mdarray loupe_buf(UCHAR_ZT,false);	/* buffer for loupe */
    mdarray_uchar tmp_buf_img(false);	/* tmp buffer for displaying */
    mdarray_uchar tmp_buf_loupe(false);	/* tmp buffer for displaying */
    int tiff_szt = 0;
    
    int display_type = 0;		/* flag to display image type */
    int display_ch = 0;			/* 0=RGB 1=R 2=G 3=B */
    int display_bin = 1;		/* binning factor for display */
    int contrast_rgb[3] = {8, 8, 8};	/* contrast for display */

    int loupe_height = 220;
    int loupe_zoom = 7;
    int loupe_x = Loupe_pos_out;
    int loupe_y = Loupe_pos_out;

    imstat imstat_rec;
    tarray_tstring imstat_log;

    bool flag_drawed;
    int aphoto_step;
    aphoto aphoto_tmp, aphoto_rec;
    tarray_tstring aphoto_log;

    tstring log_filename;

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
	    if ( test_tiff_file( argv[arg_cnt] ) == true ) {
		filenames[i] = argv[arg_cnt];
		remove_redundancy_in_path(&(filenames[i]));
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


    
    /*
     * GRAPHICS
     */
    
    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;

    /* Font selector */
    set_fontsize();

    /* command window */
    
    get_command_list_info(Cmd_list, &max_len_menu_string);
    loupe_height = (get_fontsize() / 2) * max_len_menu_string;

    command_win_rec = gopen_command_window( Cmd_list,
      get_font_margin() + 2*get_fontsize() + get_font_margin() + loupe_height);

    loupe_buf.resize_3d(command_win_rec.width, loupe_height, 3);
    
    /* file selector */
    
    win_filesel = gopen_file_selector(filenames, false);
    if ( win_filesel < 0 ) {
        sio.eprintf("[ERROR] gopen_file_selector() failed.\n");
	goto quit;
    }
    
    display_file_list(win_filesel, filenames, sel_file_id, false, -1, NULL);

    /* image viewer */

    sio.printf("Open: %s\n", filenames[sel_file_id].cstr());
    if ( load_tiff(filenames[sel_file_id].cstr(),
			 &img_buf, &tiff_szt, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] load_tiff() failed\n");
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
		  display_bin, display_ch, contrast_rgb, true, &tmp_buf_img);

    winname(win_image, "Imave Viewer  "
	    "zoom = %3.2f  contrast = ( %d, %d, %d )  "
	    "auto_zoom = %d  dither = %d",
	    (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
	    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
	    (int)flag_auto_zoom, (int)flag_dither);

    
    /*
     * MAIN EVENT LOOP
     */

    init_aphoto( &aphoto_tmp );
    init_aphoto( &aphoto_rec );
    aphoto_step = 0;
    
    flag_drawed = false;

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        bool refresh_list = false;
        int refresh_image = 0;		/* 1:display only  2:both */
	int refresh_loupe = 0;
	bool refresh_winsize = false;
        bool refresh_winname = false;
	
	int f_id = -1;
	int cmd_id = -1;
	
        /* waiting an event */
        ev_win = ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);

	/*
	 *  Check command window
	 */
	
	if ( ev_type == ButtonPress && 1 <= ev_btn && ev_btn <= 3 &&
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
	    else if ( ev_btn == 'l' ) {
		cmd_id = CMD_LOUPE_ZOOM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'L' ) {
		cmd_id = CMD_LOUPE_ZOOM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 's' ) cmd_id = CMD_IMSTAT;
	    else if ( ev_btn == 'p' ) {
		cmd_id = CMD_APHOTO;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'P' ) {
		cmd_id = CMD_APHOTO;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 13 /* Enter */ ) cmd_id = CMD_SAVE_SLOG;
	    else if ( ev_btn == 'z' ) cmd_id = CMD_AUTO_ZOOM;
	    else if ( ev_btn == 'd' ) cmd_id = CMD_DITHER;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn == 'q' ) {
		/* do not exit when aperture photometry */
		if ( aphoto_step < 1 ) cmd_id = CMD_EXIT;
	    }
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
	    if ( minus_binning_with_limit(&display_bin,
		  img_buf.x_length(), img_buf.y_length()) == true ) {
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
	else if ( cmd_id == CMD_LOUPE_ZOOM && ev_btn == 1 ) {
	    if ( loupe_zoom < loupe_height / 2 ) {
		loupe_zoom += 2;
		refresh_loupe = 1;
	    }
	}
	else if ( cmd_id == CMD_LOUPE_ZOOM && ev_btn == 3 ) {
	    if ( 4 < loupe_zoom ) {
		loupe_zoom -= 2;
		refresh_loupe = 1;
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
		tstring log;
		size_t ch;
		tmpim.resize_2d(img_buf.x_length(), img_buf.y_length());
		sio.printf("*** Image Statistics ***\n");
		sio.printf("bps     = %d\n",tiff_szt);
		sio.printf("width   = %zu\n",img_buf.x_length());
		sio.printf("height  = %zu\n",img_buf.y_length());
		for ( ch=0 ; ch < 3 ; ch++ ) {
		    if ( ch==0 ) sio.printf("[Red]\n");
		    else if ( ch==1 ) sio.printf("[Green]\n");
		    else sio.printf("[Blue]\n");
		    img_buf.copy(&tmpim,
			  0, img_buf.x_length(), 0, img_buf.y_length(), ch, 1);
		    imstat_rec.total[ch] = md_total(tmpim);
		    sio.printf("total   = %g\n",imstat_rec.total[ch]);
		    imstat_rec.median[ch] = md_median(tmpim);
		    sio.printf("median  = %g\n",imstat_rec.median[ch]);
		    imstat_rec.mean[ch] = md_mean(tmpim);
		    sio.printf("mean    = %g\n",imstat_rec.mean[ch]);
		    imstat_rec.stddev[ch] = md_stddev(tmpim);
		    sio.printf("stddev  = %g\n",imstat_rec.stddev[ch]);
		    imstat_rec.min[ch] = md_min(tmpim);
		    sio.printf("min     = %g\n",imstat_rec.min[ch]);
		    imstat_rec.max[ch] = md_max(tmpim);
		    sio.printf("max     = %g\n",imstat_rec.max[ch]);
		}
		sio.printf("-------------------------\n");
		/* */
		if ( imstat_log.length() < 1 ) {
		    log.printf("#filename,bps,width,height,"
			       "total[R],total[G],total[B],"
			       "median[R],median[G],median[B],"
			       "mean[R],mean[G],mean[B],"
			       "stddev[R],stddev[G],stddev[B],"
			       "min[R],min[G],min[B],"
			       "max[R],max[G],max[B]");
		    imstat_log.append(log, 1);
		}
		log.printf("%c%s%c,%d,%zu,%zu,%g,%g,%g,%g,%g,%g,%g,%g,%g,"
			   "%g,%g,%g,%g,%g,%g,%g,%g,%g",
			   34,filenames[sel_file_id].cstr(),34, tiff_szt,
			   img_buf.x_length(), img_buf.y_length(),
			   imstat_rec.total[0],
			   imstat_rec.total[1],
			   imstat_rec.total[2],
			   imstat_rec.median[0],
			   imstat_rec.median[1],
			   imstat_rec.median[2],
			   imstat_rec.mean[0],
			   imstat_rec.mean[1],
			   imstat_rec.mean[2],
			   imstat_rec.stddev[0],
			   imstat_rec.stddev[1],
			   imstat_rec.stddev[2],
			   imstat_rec.min[0],
			   imstat_rec.min[1],
			   imstat_rec.min[2],
			   imstat_rec.max[0],
			   imstat_rec.max[1],
			   imstat_rec.max[2]);
		imstat_log.append(log, 1);
	    }
	    else if ( cmd_id == CMD_APHOTO && ev_btn == 1 ) {
		/* New aperture photometory */
		init_aphoto( &aphoto_tmp );
		aphoto_step = 1;
		refresh_image = 1;
	    }
	    else if ( cmd_id == CMD_APHOTO && ev_btn == 3 ) {
		/* photometory reusing previous aperture */
		if ( 0 < aphoto_rec.obj_r &&
		     aphoto_rec.obj_r < aphoto_rec.sky_r ) {
		    aphoto_tmp = aphoto_rec;
		     aphoto_step = 1;
		     refresh_image = 1;
		}
	    }
	    else if ( cmd_id == CMD_SAVE_SLOG ) {
		if ( 0 < imstat_log.length() ||
		     0 < aphoto_log.length() ) {
		    size_t ix = 0;
		    if ( log_filename.length() < 1 ) {
			while ( 1 ) {
			    log_filename.printf("statistics-log_%zu.csv",ix);
			    if ( f_in.open("r", log_filename.cstr()) < 0 ) break;
			    else f_in.close();
			    ix ++;
			}
		    }
		    sio.printf("Saved: '%s'\n",log_filename.cstr());
		    if ( f_out.open("w", log_filename.cstr()) < 0 ) {
			sio.eprintf("[ERROR] cannot write: '%s'\n",
				    log_filename.cstr());
		    }
		    else {
			if ( 0 < imstat_log.length() ) {
			    f_out.printf("#Image-Statistics\r\n");
			    for ( ix=0 ; ix < imstat_log.length() ; ix++ ) {
				f_out.printf("%s\r\n", imstat_log[ix].cstr());
			    }
			}
			if ( 0 < aphoto_log.length() ) {
			    f_out.printf("#Aperture-Photometry\r\n");
			    for ( ix=0 ; ix < aphoto_log.length() ; ix++ ) {
				f_out.printf("%s\r\n", aphoto_log[ix].cstr());
			    }
			}
			f_out.close();
		    }
		}
	    }
	    /* */
	    else if ( ev_win == win_image &&
		      ( ev_type == MotionNotify || 
			ev_type == EnterNotify || ev_type == LeaveNotify ) ) {
                if ( ev_type == LeaveNotify ) {
                    ev_x = Loupe_pos_out;
		    ev_y = Loupe_pos_out;
                }
		cmd_id = 0;
		refresh_loupe = 2;
	    }
	    else if ( ev_type == KeyPress ) {
		if ( ev_btn == ' ' ) {
		    if ( display_type == 0 ) display_type = 1;
		    else display_type = 0;
		    cmd_id = 0;
		    refresh_image = 2;
		}
	    }

	    /* Aperture Photometry */
	    if ( 1 <= cmd_id ) {
		/* NOP */
	    }
	    else if ( 0 < aphoto_step && ev_type == KeyPress &&
		      /* backspace, ESC, or 'q' */
		      ( ev_btn == 8 || ev_btn == 27 || ev_btn == 'q' ) ) {
		/* cancel */
		aphoto_step = 0;
		cmd_id = 0;
		refresh_image = 1;
	    }
	    else if ( 0 < aphoto_step &&
		      ev_type == ButtonPress && ev_btn == 3 /* right btn */ ) {
		/* cancel */
		aphoto_step = 0;
		cmd_id = 0;
		refresh_image = 1;
	    }
	    else if ( aphoto_step == 1 && ev_win == win_image ) {
		cmd_id = 0;
		if ( ev_type == MotionNotify ) {
		    newgcfunction(win_image, GXxor);
		    newrgbcolor(win_image, 0x00,0xff,0x00);
		    /* draw large cross */
		    if ( flag_drawed == true ) {
			draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    }
		    aphoto_tmp.obj_x = ev_x;
		    aphoto_tmp.obj_y = ev_y;
		    draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    newgcfunction(win_image, GXcopy);
		    flag_drawed = true;
		}
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
		    if ( 0 <= aphoto_tmp.obj_x &&
			 aphoto_tmp.obj_x < img_buf.x_length() &&
			 0 <= aphoto_tmp.obj_y &&
			 aphoto_tmp.obj_y < img_buf.y_length() ) {
			/* */
			if ( 0 < aphoto_tmp.obj_r &&	/* right btn */
			     aphoto_tmp.obj_r < aphoto_tmp.sky_r ) {
			    /* perform */
			    perform_aphoto(img_buf, tiff_szt, &aphoto_tmp);
			    /* */
			    append_aphoto_log( filenames[sel_file_id].cstr(),
					   tiff_szt, aphoto_tmp, &aphoto_log );
			    aphoto_rec = aphoto_tmp;
			    /* */
			    aphoto_step = 0;
			    refresh_image = 1;
			}
			else {				/* left btn */
			    aphoto_step ++;
			}
		    }
		}
	    }
	    else if ( aphoto_step == 2 && ev_win == win_image ) {
		cmd_id = 0;
		if ( ev_type == MotionNotify ) {
		    double ev_r;
		    newgcfunction(win_image, GXxor);
		    newrgbcolor(win_image, 0x00,0xff,0x00);
		    /* draw object circle */
		    ev_r = sqrt(pow(aphoto_tmp.obj_x-ev_x,2) +
				pow(aphoto_tmp.obj_y-ev_y,2));
		    if ( flag_drawed == true ) {
			draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    }
		    aphoto_tmp.obj_r = ev_r;
		    draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    newgcfunction(win_image, GXcopy);
		    flag_drawed = true;
		}
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
		    if ( aphoto_tmp.obj_r < img_buf.x_length() &&
			 aphoto_tmp.obj_r < img_buf.y_length() ) {
			aphoto_step ++;
		    }
		}
	    }
	    else if ( aphoto_step == 3 && ev_win == win_image ) {
		cmd_id = 0;
		if ( ev_type == MotionNotify ) {
		    double ev_r;
		    newgcfunction(win_image, GXxor);
		    newrgbcolor(win_image, 0x00,0xff,0x00);
		    /* draw object circle */
		    ev_r = sqrt(pow(aphoto_tmp.obj_x-ev_x,2) +
				pow(aphoto_tmp.obj_y-ev_y,2));
		    if ( flag_drawed == true ) {
			draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    }
		    aphoto_tmp.sky_r = ev_r;
		    draw_lines_for_aphoto(win_image,
				      img_buf.x_length(), img_buf.y_length(),
				      aphoto_tmp, aphoto_step);
		    newgcfunction(win_image, GXcopy);
		    flag_drawed = true;
		}
		else if ( ev_type == ButtonPress && ev_btn == 1 ) {
		    if ( aphoto_tmp.obj_r < aphoto_tmp.sky_r &&
			 aphoto_tmp.sky_r < img_buf.x_length() &&
			 aphoto_tmp.sky_r < img_buf.y_length() ) {
			/* perform */
			perform_aphoto(img_buf, tiff_szt, &aphoto_tmp);
			/* */
			append_aphoto_log( filenames[sel_file_id].cstr(),
					   tiff_szt, aphoto_tmp, &aphoto_log );
			aphoto_rec = aphoto_tmp;
			/* */
			aphoto_step = 0;
			refresh_image = 1;
		    }
		}
	    }

	}

	/*
	 *  Check file selector
	 */
	if ( 0 <= cmd_id ) {
	    /* NOP */
	}
	else if ( ev_type == ButtonPress && ev_btn == 1 &&
	     ev_win == win_filesel ) {
	    f_id = ev_y / get_fontsize();
	}
	else if ( ev_type == KeyPress && ev_btn == 6 /* PageDown */ ) {
	    f_id = sel_file_id + 1;
	}
	else if ( ev_type == KeyPress && ev_btn == 2 /* PageUp */ ) {
	    f_id = sel_file_id - 1;
	}
	else if ( ev_type == KeyPress && ev_btn == 31 /* Down key */ ) {
	    f_id = sel_file_id + 1;
	}
	else if ( ev_type == KeyPress && ev_btn == 30 /* Up key */ ) {
	    f_id = sel_file_id - 1;
	}
	
	if ( f_id != sel_file_id &&
	     0 <= f_id && (size_t)f_id < filenames.length() ) {

	    display_file_list(win_filesel, filenames, f_id, true, -1, NULL);
	    
	    sio.printf("Open: %s\n", filenames[f_id].cstr());
		    
	    if ( load_tiff(filenames[f_id].cstr(), 
				 &img_buf, &tiff_szt, NULL, NULL) < 0 ) {
	        sio.eprintf("[ERROR] load_tiff() failed\n");
		sel_file_id = -1;
	    }
	    else {

		sel_file_id = f_id;
		//sio.printf("%ld\n", sel_file_id);

		if ( flag_auto_zoom == true ) {
		    display_bin = get_bin_factor_for_display(img_buf.x_length(),
						     img_buf.y_length(), true);
		    if ( display_bin < 0 ) {
			sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
				    "bad display depth\n");
			goto quit;
		    }
		}
		else {
		    fix_zoom_factor(&display_bin,
				    img_buf.x_length(), img_buf.y_length());
		}

		refresh_image = 2;
		refresh_winsize = true;
		refresh_list = true;

	    }
	}

	/* Update window */

	if ( refresh_image != 0 ) {
	    if ( display_type == 1 ) newgcfunction(win_image, GXcopyInverted);
	    else newgcfunction(win_image, GXcopy);
	    //
	    display_image(win_image, 0, 0, img_buf, tiff_szt,
			  display_bin, display_ch, contrast_rgb,
			  refresh_winsize, &tmp_buf_img);
	    winname(win_image, "Image Viewer  "
		    "channel = %s  zoom = %3.2f  contrast = ( %d, %d, %d )  "
		    "auto_zoom = %d  dither = %d  ",
		    names_ch[display_ch], (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
		    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
		    (int)flag_auto_zoom, (int)flag_dither);
	    flag_drawed = false;
	}

	if ( refresh_winname == true ) {
	    winname(win_image, "Image Viewer  "
		    "channel = %s  zoom = %3.2f  contrast = ( %d, %d, %d )  "
		    "auto_zoom = %d  dither = %d  ",
		    names_ch[display_ch], (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
		    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
		    (int)flag_auto_zoom, (int)flag_dither);
	}

	if ( refresh_loupe != 0 || refresh_image != 0 ) {
	    int gcfnc;
	    if (display_type == 1) gcfnc = GXcopyInverted;
	    else gcfnc = GXcopy;
	    display_loupe( command_win_rec.win_id,
			   0, command_win_rec.reserved_y0,
			   refresh_loupe, img_buf, tiff_szt, ev_x, ev_y,
			   loupe_zoom, contrast_rgb, display_ch,
			   gcfnc, &loupe_buf, 
			   &loupe_x, &loupe_y, &tmp_buf_loupe);
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

