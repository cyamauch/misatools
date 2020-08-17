static int load_sigclip_params( const char *filename,
				int *count_sigma_clip_p, int sigma_rgb[], 
				bool *std_sigma_clip_p, bool *comet_sigma_clip_p )
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
	if ( elms.length() == 6 ) {
	    int v;
	    v = elms[0].atoi();
	    if ( v < 0 ) v = 0;
	    *count_sigma_clip_p = v;

	    sigma_rgb[0] = elms[1].atoi();
	    if ( sigma_rgb[0] < 0 ) sigma_rgb[0] = 0;
	    sigma_rgb[1] = elms[2].atoi();
	    if ( sigma_rgb[1] < 0 ) sigma_rgb[1] = 0;
	    sigma_rgb[2] = elms[3].atoi();
	    if ( sigma_rgb[2] < 0 ) sigma_rgb[2] = 0;

	    v = elms[4].atoi();
	    if ( v != 0 ) *std_sigma_clip_p = true;
	    else *std_sigma_clip_p = false;

	    v = elms[5].atoi();
	    if ( v != 0 ) *comet_sigma_clip_p = true;
	    else *comet_sigma_clip_p = false;
	}
    }
    f_in.close();
    return_status = 0;
 quit:
    return return_status;
}
