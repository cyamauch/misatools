/*
 * $ s++ view_images.cc -leggx -lX11 -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/pipestreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
//#include <sli/mdarray_statistics.h>
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

#include "display_file_list.cc"
#include "display_image.cc"


typedef struct _command_list {
    int id;
    const char *menu_string;
} command_list;

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
#define CMD_AUTO_ZOOM 12
        {CMD_AUTO_ZOOM,         "Auto zoom on/off for loading [z]"},
#define CMD_DITHER 13
        {CMD_DITHER,            "Dither on/off for saving  [d]"},
#define CMD_SAVE_8BIT 14
        {CMD_SAVE_8BIT,         "Save as 8-bit TIFF"},
#define CMD_SAVE_16BIT 15
        {CMD_SAVE_16BIT,        "Save as 16-bit TIFF"},
#define CMD_SAVE_FLOAT 16
        {CMD_SAVE_FLOAT,        "Save as 32-bit float TIFF"},
#define CMD_EXIT 17
        {CMD_EXIT,              "Exit                      [q]"}
};

const size_t N_cmd_list = sizeof(Cmd_list) / sizeof(Cmd_list[0]);

int main( int argc, char *argv[] )
{
    const char *conf_file_display = "display_0.txt";

    stdstreamio sio, f_in;
    pipestreamio p_in;
    tarray_tstring filenames;
    tstring filename_1st;
    size_t maxlen_filename;
    int sel_file_id = 0;
    
    int win_command, win_filesel, win_image;
    int win_command_col_height;
    
    mdarray img_buf(false);		/* buffer for target */
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    int tiff_sztype = 0;
    
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

    for ( arg_cnt=1 ; arg_cnt < argc ; arg_cnt++ ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( 0 ) {
	    /* NOP */
	}
	else {
	    filename_1st = argv[arg_cnt];
	}
    }
    
    load_display_params(conf_file_display, contrast_rgb);

    if ( p_in.open("r", "/bin/ls | grep -i -e '[.]tiff$' -e '[.]tif$'") < 0 ) {
        sio.eprintf("[ERROR] cannot open pipe.\n");
	goto quit;
    }
    i = 0;
    while ( 1 ) {
        const char *v = p_in.getline();
	if ( v == NULL ) break;
	filenames[i] = v;
	filenames[i].trim();
	if ( filenames[i] == filename_1st ) sel_file_id = i;
	i++;
    }
    //filenames.dprint();
    p_in.close();

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

    {
	const int w_width = 33 * (Fontsize/2) + Font_margin * 2;
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
			 &img_buf, &tiff_sztype, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48() failed\n");
	goto quit;
    }

    /* for float */
    if ( tiff_sztype < 0 ) img_buf *= 65536.0;

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
    display_image(win_image, img_buf,
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
	bool refresh_winsize = false;
        bool refresh_winname = false;

	int f_id = -1;
	int cmd_id = -1;
	bool flag_file_selector = false;
	
        /* waiting an event */
        ev_win = eggx_ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);

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
				 &img_buf, &tiff_sztype, NULL, NULL) < 0 ) {
	        sio.eprintf("[ERROR] read_tiff24or48() failed\n");
		sel_file_id = -1;
	    }
	    else {

		sel_file_id = f_id;
		//sio.printf("%ld\n", sel_file_id);

		/* for float */
		if ( tiff_sztype < 0 ) img_buf *= 65536.0;
		
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
		  ev_win == win_command ) {
	    cmd_id = 1 + ev_y / win_command_col_height;
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
	    display_image(win_image, img_buf, display_bin, display_ch,
			  contrast_rgb, refresh_winsize, &tmp_buf);
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
