
static int make_output_filename( const char *_filename_in,
		 const char *appended_str,
		 const char *bit_str,	/* "8bit" , "16bit" or "float" */
		 tstring *filename_out )
{
    const char *str_8[4] = {".8bit.","_8bit.",".8bit_","_8bit_"};
    const char *str_16[4] = {".16bit.","_16bit.",".16bit_","_16bit_"};
    const char *str_fl[4] = {".float.","_float.",".float_","_float_"};
    stdstreamio sio;
    tstring filename_in, bit_str_in;
    ssize_t pos_dot, ix_8, ix_16, ix_fl, ix_right_most, ix;
    size_t i;
    bool flag_need_bit_str = true;

    bit_str_in.assign(bit_str);
    filename_in.assign(_filename_in);
    pos_dot = filename_in.rfind('.');
    if ( 0 <= pos_dot ) filename_in.copy(0, pos_dot, filename_out);
    else filename_in.copy(filename_out);

    ix_right_most = -1;
    ix_8 = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_8[i]);
	if ( ix_8 < ix ) ix_8 = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    ix_16 = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_16[i]);
	if ( ix_16 < ix ) ix_16 = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    ix_fl = -1;
    for ( i=0 ; i < 4 ; i++ ) {
	ix = filename_in.rfind(str_fl[i]);
	if ( ix_fl < ix ) ix_fl = ix;
	if ( ix_right_most < ix ) ix_right_most = ix;
    }

    if ( bit_str_in == "8bit" ) {
	if ( 0 <= ix_8 && ix_8 == ix_right_most ) flag_need_bit_str = false;
    }
    else if ( bit_str_in == "16bit" ) {
	if ( 0 <= ix_16 && ix_16 == ix_right_most ) flag_need_bit_str = false;
    }
    else if ( bit_str_in == "float" ) {
	if ( 0 <= ix_fl && ix_fl == ix_right_most ) flag_need_bit_str = false;
    }
    else {
	sio.eprintf("[WARNING] unexpected bit_str: '%s'\n", bit_str);
    }
    
    if ( flag_need_bit_str == true ) {
	filename_out->appendf(".%s.%s.tiff",appended_str,bit_str);
    }
    else {
	filename_out->appendf(".%s.tiff",appended_str);
    }
    
    return 0;
}
