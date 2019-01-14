static int save_display_params( const char *filename, const int contrast_rgb[] )
{
    int return_status = -1;
    stdstreamio sio, f_out;
    if ( f_out.open("w", filename) < 0 ) {
	sio.eprintf("[ERROR] Cannot write data to display.txt\n");
	goto quit;
    }
    f_out.printf("%d %d %d\n",
		 contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
    f_out.close();
    return_status = 0;
 quit:
    return return_status;
}
