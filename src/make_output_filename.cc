
static int make_output_filename( const char *_filename_in,
				 const char *appended_str,
				 const char *bit_str,	/* e.g., "16bit" */
				 tstring *filename_out )
{
    tstring filename_in;
    ssize_t pos_dot;

    filename_in.assign(_filename_in);
    pos_dot = filename_in.rfind('.');
    if ( 0 <= pos_dot ) filename_in.copy(0, pos_dot, filename_out);
    else filename_in.copy(filename_out);

    if ( filename_in.findf(".%s.",bit_str) < 0 ) {
	filename_out->appendf(".%s.%s.tiff",bit_str,appended_str);
    }
    else {
	filename_out->appendf(".%s.tiff",appended_str);
    }

    return 0;
}
