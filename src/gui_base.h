#ifndef _GUI_BASE_H
#define _GUI_BASE_H 1

#include <sli/tarray_tstring.h>


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


int get_fontsize();

int set_fontsize();

int get_font_y_off();

int get_font_margin();
    
command_win gopen_command_window( const command_list cmd_list[],
				  int reserved_height );

int get_command_list_info( const command_list cmd_list[],
			   size_t *ret_max_len_menu_string );

int gopen_file_selector( const sli::tarray_tstring &filenames,
			 bool status_field );

int display_file_list( int win_filesel,
		       const sli::tarray_tstring &filenames,
		       int sel_file_id, bool flag_loading,
		       int ref_file_id, const bool flg_saved[] );


#endif	/* _GUI_BASE_H */
