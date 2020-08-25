static int display_image( int win_image, const mdarray &img_buf,
			  int display_ch,	/* 0:RGB 1:R 2:G 3:B */
			  const int contrast_rgb[],
			  mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    unsigned char *tmp_buf_ptr;
    size_t i, j, k;
    int src_ch[3];
    bool needs_resize_win = false;

    if ( img_buf.dim_length() != 3 ) {
        sio.eprintf("[ERROR] img_buf is not RGB data\n");
	return -1;
    }

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

    /* check size of temporary array buffer */
    if ( tmp_buf->x_length() != img_buf.x_length() * 4 ||
	 tmp_buf->y_length() != img_buf.y_length() ) {
        needs_resize_win = true;
	tmp_buf->resize_2d(img_buf.x_length() * 4 /* ARGB */,
			   img_buf.y_length());
    }
    tmp_buf_ptr = tmp_buf->array_ptr();

    if ( img_buf.size_type() == UCHAR_ZT ) {
	const unsigned char *img_buf_ptr_r;
	const unsigned char *img_buf_ptr_g;
	const unsigned char *img_buf_ptr_b;
	size_t off4 = 0;
	size_t off1 = 0;
	img_buf_ptr_r = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[0]);
	img_buf_ptr_g = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[1]);
	img_buf_ptr_b = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[2]);
	for ( i=0 ; i < img_buf.y_length() ; i++ ) {
	    double v, ct;
	    ct = pow(2, contrast_rgb[src_ch[0]]/Contrast_scale);
	    for ( j=0, k=1 ; j < img_buf.x_length() ; j++, k+=4 ) {
		v = img_buf_ptr_r[off1 + j] * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    ct = pow(2, contrast_rgb[src_ch[1]]/Contrast_scale);
	    for ( j=0, k=2 ; j < img_buf.x_length() ; j++, k+=4 ) {
		v = img_buf_ptr_g[off1 + j] * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    ct = pow(2, contrast_rgb[src_ch[2]]/Contrast_scale);
	    for ( j=0, k=3 ; j < img_buf.x_length() ; j++, k+=4 ) {
		v = img_buf_ptr_b[off1 + j] * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    off4 += img_buf.x_length() * 4;
	    off1 += img_buf.x_length();
	}
    }
    else if ( img_buf.size_type() == FLOAT_ZT ) {
	const float *img_buf_ptr_r;
	const float *img_buf_ptr_g;
	const float *img_buf_ptr_b;
	size_t off4 = 0;
	size_t off1 = 0;
	img_buf_ptr_r = (const float *)img_buf.data_ptr(0,0,src_ch[0]);
	img_buf_ptr_g = (const float *)img_buf.data_ptr(0,0,src_ch[1]);
	img_buf_ptr_b = (const float *)img_buf.data_ptr(0,0,src_ch[2]);
	for ( i=0 ; i < img_buf.y_length() ; i++ ) {
	    double v, ct;
	    ct = pow(2, contrast_rgb[src_ch[0]]/Contrast_scale);
	    for ( j=0, k=1 ; j < img_buf.x_length() ; j++, k+=4 ) {
		/* assume unsigned 16-bit data */
		v = (img_buf_ptr_r[off1 + j] / 256) * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    ct = pow(2, contrast_rgb[src_ch[1]]/Contrast_scale);
	    for ( j=0, k=2 ; j < img_buf.x_length() ; j++, k+=4 ) {
		v = (img_buf_ptr_g[off1 + j] / 256) * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    ct = pow(2, contrast_rgb[src_ch[2]]/Contrast_scale);
	    for ( j=0, k=3 ; j < img_buf.x_length() ; j++, k+=4 ) {
		v = (img_buf_ptr_b[off1 + j] / 256) * ct + 0.5;
		if ( 255.0 < v ) v = 255.0;
		tmp_buf_ptr[off4 + k] = (unsigned char)v;
	    }
	    off4 += img_buf.x_length() * 4;
	    off1 += img_buf.x_length();
	}
    }
    else {
        sio.eprintf("[ERROR] type of img_buf is not supported\n");
	return -1;
    }
    
    if ( needs_resize_win == true ) {
        gresize(win_image, img_buf.x_length(), img_buf.y_length());
    }

    gputimage(win_image, 0,0,
	      tmp_buf_ptr, img_buf.x_length(), img_buf.y_length(), 0);
    
    return 0;
}
