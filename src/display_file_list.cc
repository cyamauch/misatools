static int display_file_list( int win_filesel,
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
