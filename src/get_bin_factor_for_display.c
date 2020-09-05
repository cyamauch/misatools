
static int get_bin_factor_for_display( size_t img_width, size_t img_height )
{
    int r_depth, r_width, r_height;
    int ret_val = -1;
    if ( ggetdisplayinfo(&r_depth,&r_width,&r_height) < 0 ) {
	goto quit;
    }
    else {
	const int margin = 80;
	int bin_x, bin_y;

	if ( r_depth < 24 ) goto quit;
	
	bin_x = (img_width + margin) / r_width;
	if ( (img_width + margin) %  r_width != 0 ) bin_x ++;
	bin_y = (img_height + margin) / r_height;
	if ( (img_height + margin) % r_height != 0 ) bin_y ++;
	
	if ( bin_x < bin_y ) ret_val = bin_y;
	else ret_val = bin_x;
    }
 quit:
    return ret_val;
}
