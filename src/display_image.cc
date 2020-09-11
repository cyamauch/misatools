#include "test_simd.h"

#if defined(_SSE3_IS_OK)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

#if defined(_SSSE3_IS_OK)
#include <xmmintrin.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#endif


static int display_image( int win_image, const mdarray &img_buf,
			  int binning,		/* 1: original scale 2:1/2 */ 
			  int display_ch,	/* 0:RGB 1:R 2:G 3:B */
			  const int contrast_rgb[],
			  bool needs_resize_win,
			  mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    unsigned char *tmp_buf_ptr;
    size_t i, j, k, l;
    int src_ch[3];
    double ct[3];
    size_t display_width, display_height;
    size_t n_img_buf_ch;
    size_t bin = binning;

    if ( img_buf.dim_length() != 3 && img_buf.dim_length() != 2 ) {
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
	tmp_buf->resize_2d(display_width * 4 /* ARGB */,
			   display_height);
    }
    tmp_buf_ptr = tmp_buf->array_ptr();

    n_img_buf_ch = img_buf.z_length();

    //sio.eprintf("[DEBUG] n_img_buf_ch = %zd\n",n_img_buf_ch);
    
    if ( img_buf.size_type() == UCHAR_ZT ) {
	const unsigned char *img_buf_ptr[3];
	size_t off1 = 0;
	size_t off4 = 0;
	if ( n_img_buf_ch == 3 ) {
	    img_buf_ptr[0] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[0]);
	    img_buf_ptr[1] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[1]);
	    img_buf_ptr[2] = (const unsigned char *)img_buf.data_ptr(0,0,src_ch[2]);
	}
	else {
	    img_buf_ptr[0] = (const unsigned char *)img_buf.data_ptr();
	    img_buf_ptr[1] = (const unsigned char *)img_buf.data_ptr();
	    img_buf_ptr[2] = (const unsigned char *)img_buf.data_ptr();
	}
	if ( bin < 2 ) {
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		double v;
		size_t ii;
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const unsigned char *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii+1 ; j < img_buf.x_length() ; j++, k+=4 ) {
			v = img_buf_ptr_rgb[off1 + j] * ct[ii] + 0.5;
			if ( 255.0 < v ) v = 255.0;
			else if ( v < 0 ) v = 255.0;
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
	    lv.resize(display_width * n_img_buf_ch);
	    lv.clean();
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		size_t ii;
		for ( ii=0 ; ii < n_img_buf_ch ; ii++ ) {
		    const unsigned char *img_buf_ptr_rgb = img_buf_ptr[ii];
		    k = ii * display_width;	/* offset */
		    for ( j=0 ; j < img_buf.x_length() ; j++ ) {
			lv_p[k] += img_buf_ptr_rgb[off1 + j];
			if ( (j+1) % bin == 0 ) k++;
		    }
		}
		if ( (i+1) % bin == 0 || (i+1) == img_buf.y_length() ) {
		    const double ftr = 1.0 / (double)(bin * bin);
		    double v;
		    if ( n_img_buf_ch == 3 ) {	/* RGB */
			float *lv_p_r = lv_p + 0;
			float *lv_p_g = lv_p + display_width;
			float *lv_p_b = lv_p + 2 * display_width;
			for ( j=0, l=0 ; j < display_width ; j++ ) {
			          l++;
			    v = lv_p_r[j] * ftr * ct[0] + 0.5;
			    lv_p_r[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p_g[j] * ftr * ct[1] + 0.5;
			    lv_p_g[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p_b[j] * ftr * ct[2] + 0.5;
			    lv_p_b[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			}
		    }
		    else {			/* MONO */
			for ( j=0, l=0 ; j < display_width ; j++ ) {
			          l++;
			    v = lv_p[j] * ftr * ct[0] + 0.5;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p[j] * ftr * ct[1] + 0.5;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p[j] * ftr * ct[2] + 0.5;
			    lv_p[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			}
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
	if ( n_img_buf_ch == 3 ) {
	    img_buf_ptr[0] = (const float *)img_buf.data_ptr(0,0,src_ch[0]);
	    img_buf_ptr[1] = (const float *)img_buf.data_ptr(0,0,src_ch[1]);
	    img_buf_ptr[2] = (const float *)img_buf.data_ptr(0,0,src_ch[2]);
	}
	else {
	    img_buf_ptr[0] = (const float *)img_buf.data_ptr();
	    img_buf_ptr[1] = (const float *)img_buf.data_ptr();
	    img_buf_ptr[2] = (const float *)img_buf.data_ptr();
	}
	if ( bin < 2 ) {
	    /* assume unsigned 16-bit data */
	    const double ftr = 1.0 / 256.0;
#if defined(_SSSE3_IS_OK)
	    const __m128i sfl_red_ix =
	    /*                    right  to  left !                      */
	    /*            B  G  R  A  B  G  R  A  B  G  R  A  B  G  R  A */
	    _mm_set_epi8(-1,-1,12,-1,-1,-1, 8,-1,-1,-1, 4,-1,-1,-1, 0,-1);
	    const __m128i sfl_green_ix =
	    /*            B  G  R  A  B  G  R  A  B  G  R  A  B  G  R  A */
	    _mm_set_epi8(-1,12,-1,-1,-1, 8,-1,-1,-1, 4,-1,-1,-1, 0,-1,-1);
	    const __m128i sfl_blue_ix =
	    /*            B  G  R  A  B  G  R  A  B  G  R  A  B  G  R  A */
	    _mm_set_epi8(12,-1,-1,-1, 8,-1,-1,-1, 4,-1,-1,-1, 0,-1,-1,-1);
	    const __m128 f0 = _mm_set1_ps((float)(ftr * ct[0]));
	    const __m128 f1 = _mm_set1_ps((float)(ftr * ct[1]));
	    const __m128 f2 = _mm_set1_ps((float)(ftr * ct[2]));
	    const __m128 c255 = _mm_set1_ps((float)255.0);
	    const __m128 c000 = _mm_set1_ps((float)0.0);
	    const size_t step_sse16 = 16 / sizeof(float);	/* 4 */
	    const size_t n_sse16 = img_buf.x_length() / step_sse16;
	    const size_t nel_sse16 = step_sse16 * n_sse16;
	    /* _mm_cvtps works as *pseudo* round() */
	    const unsigned int mxcsr = _mm_getcsr();
	    _mm_setcsr(mxcsr & 0xffff9fffU);
#endif
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		double v;
#if 1
		j = 0;	/* src */
		k = 0;	/* dest */
#if defined(_SSSE3_IS_OK)
		__m128 ps0, m0;
		__m128i si0, si1, si2;
		for ( ; j < nel_sse16 ; j += step_sse16, k += 16 ) {
		    /* red */
		    ps0 = _mm_loadu_ps( img_buf_ptr[0] + off1 + j );
		    ps0 = _mm_mul_ps(ps0, f0);
		    m0 = _mm_or_ps(_mm_cmpgt_ps(ps0, c255),_mm_cmplt_ps(ps0, c000));
		    si0 = _mm_cvtps_epi32(ps0);
		    si0 = _mm_or_si128(si0, (__m128i)m0);
		    si0 = _mm_shuffle_epi8(si0, sfl_red_ix);
		    /* green */
		    ps0 = _mm_loadu_ps( img_buf_ptr[1] + off1 + j );
		    ps0 = _mm_mul_ps(ps0, f1);
		    m0 = _mm_or_ps(_mm_cmpgt_ps(ps0, c255),_mm_cmplt_ps(ps0, c000));
		    si1 = _mm_cvtps_epi32(ps0);
		    si1 = _mm_or_si128(si1, (__m128i)m0);
		    si1 = _mm_shuffle_epi8(si1, sfl_green_ix);
		    si0 = _mm_or_si128(si0, si1);
		    /* blue */
		    ps0 = _mm_loadu_ps( img_buf_ptr[2] + off1 + j );
		    ps0 = _mm_mul_ps(ps0, f2);
		    m0 = _mm_or_ps(_mm_cmpgt_ps(ps0, c255),_mm_cmplt_ps(ps0, c000));
		    si2 = _mm_cvtps_epi32(ps0);
		    si2 = _mm_or_si128(si2, (__m128i)m0);
		    si2 = _mm_shuffle_epi8(si2, sfl_blue_ix);
		    si0 = _mm_or_si128(si0, si2);
		    /* store! */
		    _mm_storeu_si128((__m128i *)(tmp_buf_ptr + off4 + k), si0);
		}
#endif
		for ( ; j < img_buf.x_length() ; j++ ) {
		    k++;
		    v = img_buf_ptr[0][off1 + j] * ftr * ct[0] + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    else if ( v < 0 ) v = 255.0;
		    tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    k++;
		    v = img_buf_ptr[1][off1 + j] * ftr * ct[1] + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    else if ( v < 0 ) v = 255.0;
		    tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    k++;
		    v = img_buf_ptr[2][off1 + j] * ftr * ct[2] + 0.5;
		    if ( 255.0 < v ) v = 255.0;
		    else if ( v < 0 ) v = 255.0;
		    tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    k++;
		}
#else
		size_t ii;
		for ( ii=0 ; ii < 3 ; ii++ ) {
		    const float *img_buf_ptr_rgb = img_buf_ptr[ii];
		    for ( j=0, k=ii+1 ; j < img_buf.x_length() ; j++, k+=4 ) {
			v = img_buf_ptr_rgb[off1 + j] * ftr * ct[ii] + 0.5;
			if ( 255.0 < v ) v = 255.0;
			else if ( v < 0 ) v = 255.0;
			tmp_buf_ptr[off4 + k] = (unsigned char)v;
		    }
		}
#endif
		off4 += display_width * 4;
		off1 += img_buf.x_length();
	    }
#if defined(_SSSE3_IS_OK)
	    _mm_setcsr(mxcsr);
#endif
	}
	else {
	    float *lv_p = NULL;
	    mdarray_float lv(false, &lv_p);
	    lv.resize(display_width * n_img_buf_ch);
	    lv.clean();
	    for ( i=0 ; i < img_buf.y_length() ; i++ ) {
		size_t ii;
		for ( ii=0 ; ii < n_img_buf_ch ; ii++ ) {
		    const float *img_buf_ptr_rgb = img_buf_ptr[ii];
		    size_t j_start = 0;
		    k = ii * display_width;	/* offset */
#if defined(_SSE3_IS_OK)
		    if ( bin == 2 ) {
			size_t n_sse = img_buf.x_length() / 8;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1;
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    /* */
			    r0 = _mm_hadd_ps( r0, r1 );
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 8 * n_sse;
		    }
		    else if ( bin == 3 ) {
			size_t n_sse = img_buf.x_length() / 12;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			uint32_t msk0[4] = {0xffffffffUL,0xffffffffUL,0xffffffffUL,0};
			uint32_t msk1[4] = {0,0xffffffffUL,0xffffffffUL,0xffffffffUL};
			__m128 msk_l = _mm_loadu_ps( (float *)msk0 );
			__m128 msk_r = _mm_loadu_ps( (float *)msk1 );
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1, r2;
			    /*
			     *  p_src     abc  def   ghi  jkl
			     *    V
			     *   xmm      abc0 def0 0ghi 0jkl
			     */
			    r0 = _mm_loadu_ps( p_src );
			    r0 = _mm_and_ps(r0, msk_l);
			    p_src += 3;
			    r1 = _mm_loadu_ps( p_src );
			    r1 = _mm_and_ps(r1, msk_l);
			    p_src += 3;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    r1 = _mm_loadu_ps( p_src - 1 );
			    r1 = _mm_and_ps(r1, msk_r);
			    p_src += 3;
			    r2 = _mm_loadu_ps( p_src - 1 );
			    r2 = _mm_and_ps(r2, msk_r);
			    p_src += 3;
			    r1 = _mm_hadd_ps( r1, r2 );	/* result */
			    /* */
			    r0 = _mm_hadd_ps( r0, r1 );
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 12 * n_sse;
		    }
		    else if ( bin == 4 ) {
			size_t n_sse = img_buf.x_length() / 16;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1, r2;
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_hadd_ps( r1, r2 );	/* result */
			    /* */
			    r0 = _mm_hadd_ps( r0, r1 );
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 16 * n_sse;
		    }
		    else if ( bin == 6 ) {
			size_t n_sse = img_buf.x_length() / 24;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			float tmp_work[12] __attribute__((aligned(16)));
			//uint32_t msk0[4] = {0xffffffffUL,0xffffffffUL,0xffffffffUL,0};
			//uint32_t msk1[4] = {0,0xffffffffUL,0xffffffffUL,0xffffffffUL};
			//__m128 msk_l = _mm_loadu_ps( (float *)msk0 );
			//__m128 msk_r = _mm_loadu_ps( (float *)msk1 );
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1, r2, r3;
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_hadd_ps( r0, r1 );	/* result */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r3 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r3 = _mm_hadd_ps( r1, r3 );	/* result */
			    /* result: r2,r3,r0 */
			    _mm_store_ps(tmp_work + 0, r2);
			    _mm_store_ps(tmp_work + 4, r0);
			    _mm_store_ps(tmp_work + 8, r3);
#if 1
			    tmp_work[0] = tmp_work[0] + tmp_work[1] + tmp_work[2];
			    tmp_work[1] = tmp_work[3] + tmp_work[4] + tmp_work[5];
			    tmp_work[2] = tmp_work[6] + tmp_work[7] + tmp_work[8];
			    tmp_work[3] = tmp_work[9] + tmp_work[10] + tmp_work[11];
			    /* */
			    r0 = _mm_load_ps( tmp_work );	/* result3 */
#else
			    /*
			     *  This code is NOT fast!
			     *
			     *             |-r2-|       |-r3-|
			     *  tmp_work   abc  def   ghi  jkl
			     *     V
			     *    xmm      abc0 def0 0ghi 0jkl
			     *
			     */
			    //r2 = _mm_load_ps( tmp_work + 0 );
			    r2 = _mm_and_ps(r2, msk_l);
			    r1 = _mm_loadu_ps( tmp_work + 3 );
			    r1 = _mm_and_ps(r1, msk_l);
			    r0 = _mm_hadd_ps( r2, r1 );	/* result2 */
			    r1 = _mm_loadu_ps( tmp_work + 5 );
			    r1 = _mm_and_ps(r1, msk_r);
			    //r3 = _mm_load_ps( tmp_work + 8 );
			    r3 = _mm_and_ps(r3, msk_r);
			    r1 = _mm_hadd_ps( r1, r3 );	/* result2 */
			    r0 = _mm_hadd_ps( r0, r1 ); /* result3 */ 
#endif
			    /* */
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 24 * n_sse;
		    }
		    else if ( bin == 8 ) {
			size_t n_sse = img_buf.x_length() / 32;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1, r2, r3;
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_hadd_ps( r1, r2 );	/* result */
			    r3 = _mm_hadd_ps( r0, r1 );	/* result2 */
			    /* */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_hadd_ps( r1, r2 );	/* result */
			    r2 = _mm_hadd_ps( r0, r1 );	/* result2 */
			    /* */
			    r0 = _mm_hadd_ps( r3, r2 );
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 32 * n_sse;
		    }
		    else if ( bin == 10 ) {
			size_t n_sse = img_buf.x_length() / 40;
			const float *p_src = img_buf_ptr_rgb + off1;
			float *p_dst = lv_p + k;
			float tmp_work[20] __attribute__((aligned(16)));
			for ( j=0 ; j < n_sse ; j++ ) {
			    __m128 r0, r1, r2, r3;
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_hadd_ps( r0, r1 );	/* result */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    _mm_store_ps(tmp_work + 0, r2);
			    _mm_store_ps(tmp_work + 4, r0);
			    /* */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r2 = _mm_hadd_ps( r0, r1 );	/* result */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r3 = _mm_hadd_ps( r0, r1 );	/* result */
			    r0 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r1 = _mm_loadu_ps( p_src );
			    p_src += 4;
			    r0 = _mm_hadd_ps( r0, r1 );	/* result */
			    _mm_store_ps(tmp_work + 8, r2);
			    _mm_store_ps(tmp_work + 12, r3);
			    _mm_store_ps(tmp_work + 16, r0);
			    /* */
			    tmp_work[0] = tmp_work[0] + tmp_work[1]
				     + tmp_work[2] + tmp_work[3] + tmp_work[4];
			    tmp_work[1] = tmp_work[5] + tmp_work[6]
				     + tmp_work[7] + tmp_work[8] + tmp_work[9];
			    tmp_work[2] = tmp_work[10] + tmp_work[11]
				     + tmp_work[12] + tmp_work[13] + tmp_work[14];
			    tmp_work[3] = tmp_work[15] + tmp_work[16]
				     + tmp_work[17]  + tmp_work[18] + tmp_work[19];
			    /* */
			    r0 = _mm_load_ps( tmp_work );
			    r1 = _mm_loadu_ps( p_dst );
			    r0 = _mm_add_ps( r0, r1 );
			    _mm_storeu_ps(p_dst, r0);
			    p_dst += 4;
			}
			k += 4 * n_sse;
			j_start = 40 * n_sse;
		    }
#endif
		    for ( j=j_start ; j < img_buf.x_length() ; j++ ) {
			lv_p[k] += img_buf_ptr_rgb[off1 + j];
			if ( (j+1) % bin == 0 ) k++;
		    }
		}
		if ( (i+1) % bin == 0 || (i+1) == img_buf.y_length() ) {
		    const double ftr = 1.0 / (double)(256 * bin * bin);
		    double v;
		    if ( n_img_buf_ch == 3 ) {	/* RGB */
			float *lv_p_r = lv_p + 0;
			float *lv_p_g = lv_p + display_width;
			float *lv_p_b = lv_p + 2 * display_width;
			for ( j=0, l=0 ; j < display_width ; j++ ) {
			          l++;
			    v = lv_p_r[j] * ftr * ct[0] + 0.5;
			    lv_p_r[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p_g[j] * ftr * ct[1] + 0.5;
			    lv_p_g[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p_b[j] * ftr * ct[2] + 0.5;
			    lv_p_b[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			}
		    }
		    else {			/* MONO */
			for ( j=0, l=0 ; j < display_width ; j++ ) {
			          l++;
			    v = lv_p[j] * ftr * ct[0] + 0.5;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p[j] * ftr * ct[1] + 0.5;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			    v = lv_p[j] * ftr * ct[2] + 0.5;
			    lv_p[j] = 0.0;
			    if ( 255.0 < v ) v = 255.0;
			    else if ( v < 0 ) v = 255.0;
			    tmp_buf_ptr[off4 + l] = (unsigned char)v;
			          l++;
			}
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
