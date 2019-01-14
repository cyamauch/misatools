/* Adjust cropping parameters */
static void adjust_crop_prms( uint32 width, uint32 height,
			      const long crop_prms[],
			      /* actual crop area */
			      uint32 *x_out_ret, uint32 *y_out_ret,
			      uint32 *width_out_ret, uint32 *height_out_ret )
{
    uint32 x_out, y_out, width_out, height_out;

    if ( crop_prms[2] <= 0 ) width_out = width;
    else width_out = crop_prms[2];
    if ( width < width_out ) width_out = width;

    if ( crop_prms[3] <= 0 ) height_out = height;
    else height_out = crop_prms[3];
    if ( height < height_out ) height_out = height;
    
    if ( crop_prms[0] < 0 ) {	/* center crop */
        x_out = (width - width_out) / 2;
    }
    else {			/* crop with position */
        x_out = crop_prms[0];
	if ( width <= x_out ) x_out = width - 1;
	if ( width < x_out + width_out ) width_out = width - x_out;
    }

    if ( crop_prms[1] < 0 ) {	/* center crop */
        y_out = (height - height_out) / 2;
    }
    else {			/* crop with position */
        y_out = crop_prms[1];
	if ( height <= y_out ) y_out = height - 1;
	if ( height < y_out + height_out ) height_out = height - y_out;
    }
    
    *x_out_ret = x_out;
    *y_out_ret = y_out;
    *width_out_ret = width_out;
    *height_out_ret = height_out;
    
    return;
}
