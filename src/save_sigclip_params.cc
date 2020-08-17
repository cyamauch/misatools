static int save_sigclip_params( const char *filename,
				int count_sigma_clip, const int sigma_rgb[],
				bool std_sigma_clip, bool comet_sigma_clip )
{
    int return_status = -1;
    stdstreamio sio, f_out;
    if ( f_out.open("w", filename) < 0 ) {
	sio.eprintf("[ERROR] Cannot write data to %s\n", filename);
	goto quit;
    }
    f_out.printf("%d %d %d %d %d %d\n",
		 count_sigma_clip,
		 sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
		 (int)std_sigma_clip, (int)comet_sigma_clip);
    f_out.close();
    return_status = 0;
 quit:
    return return_status;
}
