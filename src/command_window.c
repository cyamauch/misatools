
typedef struct _command_list {
    int id;			/* do not set 0 in the list */
    const char *menu_string;
} command_list;

typedef struct _command_win {
    int win_id;
    int width;
    int height;
    int cell_height;
    int reserved_y0;
} command_win;

static command_win gopen_command_window( const command_list cmd_list[],
					 int reserved_height )
{
    command_win cmd_win_ret = {0};
    size_t n_cmd_list;
    int win_command;
    int w_width, w_height;
    int cl_height, res_begin;
    size_t i, max_c;
    
    max_c = 0;
    for ( i=0 ; cmd_list[i].menu_string != NULL ; i++ ) {
	size_t j = 0;
	while ( cmd_list[i].menu_string[j] != '\0' ) j++;
	if ( max_c < j ) max_c = j;
    }
    n_cmd_list = i;
    
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
