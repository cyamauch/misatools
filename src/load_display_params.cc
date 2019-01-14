static int load_display_params( const char *filename, int contrast_rgb[] )
{
    int return_status = -1;
    stdstreamio sio, f_in;
    tstring line;
    if ( f_in.open("r", filename) < 0 ) {
	goto quit;
    }
    line = f_in.getline();
    line.trim();
    if ( 0 < line.length() ) {
	tarray_tstring elms;
	elms.split(line.cstr()," ",false);
	if ( elms.length() == 3 ) {
	    contrast_rgb[0] = elms[0].atoi();
	    if ( contrast_rgb[0] < 0 ) contrast_rgb[0] = 0;
	    contrast_rgb[1] = elms[1].atoi();
	    if ( contrast_rgb[1] < 0 ) contrast_rgb[1] = 0;
	    contrast_rgb[2] = elms[2].atoi();
	    if ( contrast_rgb[2] < 0 ) contrast_rgb[2] = 0;
	}
    }
    f_in.close();
    return_status = 0;
 quit:
    return return_status;
}
