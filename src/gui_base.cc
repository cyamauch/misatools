#include "gui_base.h"

#include <eggx.h>

using namespace sli;

static int Font_y_off = 3;
static int Font_margin = 2;
static int Fontsize = 14;


int get_fontsize()
{
    return Fontsize;
}

int set_fontsize()
{
    int win, i;
    int ev_win, ev_type, ev_btn;	/* for event handling */
    double ev_x, ev_y;
    win = gopen(36 * 4, 24 * 2 + 2);
    gsetbgcolor(win,"#606060");
    gclr(win);
    drawstr(win, 0,24-1,24, 0, " Font Size?");
    for ( i = 0 ; i < 4 ; i++ ) {
	newrgbcolor(win,0x40,0x40,0x40);
	drawrect(win, 36*i,24, 35,24);
	newrgbcolor(win,0x80,0x80,0x80);
	drawline(win, 36*i,24, 36*i+34,24);
	drawline(win, 36*i,24, 36*i,24+24);
    }
    newrgbcolor(win,0xff,0xff,0xff);
    drawstr(win, 6+ 36*0,48-1,24, 0, "14");
    drawstr(win, 6+ 36*1,48-1,24, 0, "16");
    drawstr(win, 6+ 36*2,48-1,24, 0, "20");
    drawstr(win, 6+ 36*3,48-1,24, 0, "24");
    ev_win = ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);
    if ( ev_win == win && ev_type == ButtonPress ) {
	if ( 0 <= ev_x && ev_x < 36*1 ) Fontsize = 14;
	else if ( 36*1 <= ev_x && ev_x < 36*2 ) Fontsize = 16;
	else if ( 36*2 <= ev_x && ev_x < 36*3 ) Fontsize = 20;
	else if ( 36*3 <= ev_x && ev_x < 364 ) Fontsize = 24;
    }
    gclose(win);

    return Fontsize;
}

int get_font_y_off()
{
    return Font_y_off;
}

int get_font_margin()
{
    return Font_margin;
}

command_win gopen_command_window( const command_list cmd_list[],
				  int reserved_height )
{
    command_win cmd_win_ret = {0};
    size_t n_cmd_list, max_c = 0;
    int win_command;
    int w_width, w_height;
    int cl_height, res_begin;
    size_t i;
    
    n_cmd_list = get_command_list_info(cmd_list, &max_c);
    
    w_width = max_c * (Fontsize/2) + Font_margin * 2;
    cl_height = (Fontsize + Font_margin * 2);
    res_begin = cl_height * (n_cmd_list);
    //if ( 0 < reserved_height ) res_begin += cl_height/2;
    w_height = res_begin + reserved_height;
    
    win_command = gopen(w_width, w_height);

    layer(win_command, 0, 1);

    gsetbgcolor(win_command,"#202020");
    gclr(win_command);

    newrgbcolor(win_command,0x60,0x60,0x60);
    fillrect(win_command, 0, 0, w_width, res_begin);
    
    winname(win_command, "Command Window");
    for ( i=0 ; i < n_cmd_list ; i++ ) {
	newrgbcolor(win_command,0x80,0x80,0x80);
	drawline(win_command,
		 0, cl_height * (cmd_list[i].id - 1),
		 w_width, cl_height * (cmd_list[i].id - 1));
	newrgbcolor(win_command,0x40,0x40,0x40);
	drawline(win_command,
		 0, cl_height * (cmd_list[i].id) - 1,
		 w_width, cl_height * (cmd_list[i].id) - 1);
	newrgbcolor(win_command,0xff,0xff,0xff);
	drawstr(win_command,
		Font_margin, cl_height * cmd_list[i].id - Font_y_off,
		Fontsize, 0, cmd_list[i].menu_string);
    }

    copylayer(win_command, 1, 0);
    layer(win_command, 0, 0);
    
    cmd_win_ret.win_id = win_command;
    cmd_win_ret.width = w_width;
    cmd_win_ret.height = w_height;
    cmd_win_ret.cell_height = cl_height;
    cmd_win_ret.reserved_y0 = res_begin;
    
    return cmd_win_ret;
}

int get_command_list_info( const command_list cmd_list[],
			   size_t *ret_max_len_menu_string )
{
    size_t n_cmd_list = 0;
    size_t i, max_c;

    if ( cmd_list == NULL ) goto quit;
	
    max_c = 0;
    for ( i=0 ; cmd_list[i].menu_string != NULL ; i++ ) {
	size_t j = 0;
	while ( cmd_list[i].menu_string[j] != '\0' ) j++;
	if ( max_c < j ) max_c = j;
    }
    n_cmd_list = i;

    if ( ret_max_len_menu_string != NULL ) *ret_max_len_menu_string = max_c;
    
 quit:
    return n_cmd_list;
}

int gopen_file_selector( const sli::tarray_tstring &filenames,
			 bool status_field )
{
    int win_filesel = -1;
    size_t maxlen_filename, i;

    /* get maxlen_filename */
    maxlen_filename = 0;
    for ( i=0 ; i < filenames.length() ; i++ ) {
	if ( maxlen_filename < filenames[i].length() ) {
	    maxlen_filename = filenames[i].length();
	}
    }
    if ( maxlen_filename == 0 ) {
	goto quit;
    }
    
    if ( status_field == true ) {
	win_filesel = gopen((get_fontsize()/2) * (maxlen_filename + 6 + 4),
			get_fontsize() * (filenames.length()));
    }
    else {
	win_filesel = gopen((get_fontsize()/2) * (maxlen_filename + 2),
			get_fontsize() * (filenames.length()));
    }
    gsetbgcolor(win_filesel,"#404040");
    gclr(win_filesel);
    winname(win_filesel, "File Selector");
    
 quit:
    return win_filesel;
}

int display_file_list( int win_filesel,
		       const tarray_tstring &filenames,
		       int sel_file_id, bool flag_loading,
		       int ref_file_id, const bool flg_saved[] )
{
    size_t i;
    int xpos_filename = (Fontsize/2)*1;

    if ( flg_saved != NULL ) xpos_filename = (Fontsize/2)*8;

    layer(win_filesel, 0, 1);
    
    gclr(win_filesel);
    
    /* displaying file list */
    newcolor(win_filesel, "white");
    
    for ( i=0 ; i < filenames.length() ; i++ ) {
        bool changed_color = false;
	if ( 0 <= ref_file_id && i == (size_t)ref_file_id ) {
	    newcolor(win_filesel, "red");
	    changed_color = true;
	    drawstr(win_filesel,
		    (Fontsize/2)*2, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		    "REF");
	}
	else if ( 0 <= sel_file_id && i == (size_t)sel_file_id ) {
	    newcolor(win_filesel, "green");
	    if ( flag_loading == true ) {
		fillrect(win_filesel,
			 xpos_filename, (int)(Fontsize*i)-1,
			 (Fontsize/2) * (filenames[i].length() + 0),
			 Fontsize+1);
		newcolor(win_filesel, "black");
	    }
	    changed_color = true;
	}

        drawstr(win_filesel,
		xpos_filename, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		"%s",filenames[i].cstr());

	if ( flg_saved != NULL ) {
	    if ( flg_saved[i] == true ) {
		if ( changed_color == true ) {
		    newcolor(win_filesel, "green");
		}
		drawstr(win_filesel,
		    (Fontsize/2)*2, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		    "SAVED");
	    }
	}

	
        if ( changed_color == true ) {
	    newcolor(win_filesel, "white");
	}
    }

    copylayer(win_filesel, 1, 0);
    
    return 0;
}
