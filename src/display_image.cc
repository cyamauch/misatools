static int display_image( int win_image, const mdarray &img_buf,
			  int binning,		/* 1: original scale 2:1/2 */ 
			  int display_ch,	/* 0:RGB 1:R 2:G 3:B */
			  const int contrast_rgb[],
			  mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    unsigned char *tmp_buf_ptr;
    size_t i, j, k, l;
    int src_ch[3];
    double ct[3];
    size_t display_width, display_height;
    size_t bin = binning;
    bool needs_resize_win = false;

    if ( img_buf.dim_length() != 3 ) {
        sio.eprintf("[ERROR] img_buf is not RGB data\n");
	return -1;
    }

    /* determine display width */
    display_width = img_buf.x_length() / bin;
    if ( img_buf.x_length() % bin !=0 ) display_width ++;
    display_height = img_buf.y_length() / bin;
    if ( img_buf.y_length() % bin !=0 ) display_height ++;

    /* determine display ch (R,G,B or RGB) */
    if ( display_ch == 1 ) {
	src_ch[0] = 0;	src_ch[1] = 0;  src_ch[2] = 0;  
    }
    else if ( display_ch == 2 ) {
	src_ch[0] = 1;	src_ch[1] = 1;  src_ch[2] = 1;  
    }
    else if ( display_ch == 3 ) {
	src_ch[0] = 2;	src_ch[1] = 2;  src_ch[2] = 2;  
    }
    else {
	src_ch[0] = 0;	src_ch[1] = 1;  src_ch[2] = 2;  
    }

    /* set contrast parameters */
    for ( i=0 ; i < 3 ; i++ ) {
	ct[i] = pow(2, contrast_rgb[src_ch[i]]/Contrast_scale);
    }
    
    /* check size of temporary array buffer */
    if ( tmp_buf->x_length() != display_width * 4 ||
	 tmp_buf->y_length() != display_height ) {
        needs_resize_win = true;
	tmp_buf->resize_2d(display_width * 4 /* ARGB */,
			   display_height);
    }
    tmp_buf_ptr = tmp_buf->array_ptr();

    if ( img_buf.size_type() == UCHAR_ZT ) {
	const unsigned char *img_buf_ptr[3];
	size_t off1 = 0;
	size_t off4 = 0;
	img_buf_ptr[0] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[0]);
	img_buf_ptr[1] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[1]);
	img_buf_ptr[2] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[2]);
	if ( bin < 2 ) {
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		double v;
		size_t ii;
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const unsigned char *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii+1 ; j < img_buf.x_length() ; j++, k+=4 ) {
			v = img_buf_ptr_rgb[off1 + j] * ct[ii] + 0.5;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    }
		}
		off4 += display_width * 4;
		off1 += img_buf.x_length();
	    }
	}
	else {
	    float *lv_p = NULL;
	    mdarray_float lv(false, &lv_p);
	    size_t ii;
	    lv.resize(display_width * 3);
	    lv = 0.0;
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const unsigned char *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii ; j < img_buf.x_length() ; j++ ) {
			lv_p[k] += img_buf_ptr_rgb[off1 + j];
			if ( (j+1) % bin == 0 ) k += 3;
		    }
		}
		if ( (i+1) % bin == 0 || (i+1) == img_buf.y_length() ) {
		    const double ftr = 1.0 / (double)(bin * bin);
		    double v;
		    for ( j=0, k=0, l=0 ; j < display_width ; j++ ) {
			l++;
			v = lv_p[k] * ftr * ct[0] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
			v = lv_p[k] * ftr * ct[1] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
			v = lv_p[k] * ftr * ct[2] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
		    }
		    off4 += display_width * 4;
		}
		off1 += img_buf.x_length();
	    }
	}
    }
    else if ( img_buf.size_type() == FLOAT_ZT ) {
	const float *img_buf_ptr[3];
	size_t off1 = 0;
	size_t off4 = 0;
	img_buf_ptr[0] = (const float *)img_buf.data_ptr(0,0,src_ch[0]);
	img_buf_ptr[1] = (const float *)img_buf.data_ptr(0,0,src_ch[1]);
	img_buf_ptr[2] = (const float *)img_buf.data_ptr(0,0,src_ch[2]);
	if ( bin < 2 ) {
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		const double ftr = 1.0 / 256.0;
		double v;
		size_t ii;
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const float *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii+1 ; j < img_buf.x_length() ; j++, k+=4 ) {
			/* assume unsigned 16-bit data */
			v = img_buf_ptr_rgb[off1 + j] * ftr * ct[ii] + 0.5;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    }
		}
		off4 += display_width * 4;
		off1 += img_buf.x_length();
	    }
	}
	else {
	    float *lv_p = NULL;
	    mdarray_float lv(false, &lv_p);
	    size_t ii;
	    lv.resize(display_width * 3);
	    lv = 0.0;
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const float *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii ; j < img_buf.x_length() ; j++ ) {
			lv_p[k] += img_buf_ptr_rgb[off1 + j];
			if ( (j+1) % bin == 0 ) k += 3;
		    }
		}
		if ( (i+1) % bin == 0 || (i+1) == img_buf.y_length() ) {
		    const double ftr = 1.0 / (double)(256 * bin * bin);
		    double v;
		    for ( j=0, k=0, l=0 ; j < display_width ; j++ ) {
			l++;
			v = lv_p[k] * ftr * ct[0] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
			v = lv_p[k] * ftr * ct[1] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
			v = lv_p[k] * ftr * ct[2] + 0.5;
			lv_p[k] = 0.0;
			if ( 255.0 < v ) v = 255.0;
			tmp_buf_ptr[off4 + l] = (unsigned char)v;
			k++;  l++;
		    }
		    off4 += display_width * 4;
		}
		off1 += img_buf.x_length();
	    }
	}
    }
    else {
        sio.eprintf("[ERROR] type of img_buf is not supported\n");
	return -1;
    }
    
    if ( needs_resize_win == true ) {
        gresize(win_image, display_width, display_height);
	coordinate(win_image, 0,0, 0.0, 0.0, 1.0/binning, 1.0/binning);
    }

    gputimage(win_image, 0,0,
	      tmp_buf_ptr, display_width, display_height, 0);
    
    return 0;
}
