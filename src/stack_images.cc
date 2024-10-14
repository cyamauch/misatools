/*
 * $ s++ stack_images.cc -leggx -lX11 -ltiff
 */
#include <sli/stdstreamio.h>
#include <sli/pipestreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
#include <eggx.h>
#include <unistd.h>

#include "tiff_funcs.h"
#include "display_image.h"
#include "gui_base.h"
#include "loupe_funcs.h"

using namespace sli;


/**
 * @file   stack_images.cc
 * @brief  interactive xy matching and stacking tool.
 *         8/16-bit integer and 32-bit float images are supported.
 */

const char *Refframe_conffile = "refframe.txt";


static int load_sigclip_params( const char *filename,
			int *n_comp_dark_synth_p,
			int *count_sigma_clip_p, int sigma_rgb[], 
			bool *skylv_sigma_clip_p, bool *comet_sigma_clip_p )
{
    int return_status = -1;
    stdstreamio sio, f_in;
    tstring line;
    if ( f_in.open("r", filename) < 0 ) {
	goto quit;
    }
    while ( (line = f_in.getline()) != NULL ) {
	line.trim();
	if ( 0 < line.length() ) {
	    tarray_tstring elms;
	/*
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
	    if ( v != 0 ) *skylv_sigma_clip_p = true;
	    else *skylv_sigma_clip_p = false;

	    v = elms[5].atoi();
	    if ( v != 0 ) *comet_sigma_clip_p = true;
	    else *comet_sigma_clip_p = false;
	}
	*/
	    elms.split(line.cstr()," =",false);
	    if ( elms[0].strcmp("n_comp_dark_synth") == 0 ) {
		int v = elms[1].atoi();
		if ( v != 0 ) *n_comp_dark_synth_p = true;
		else *n_comp_dark_synth_p = false;
	    }
	    else if ( elms[0].strcmp("count_sigma_clip") == 0 ) {
		int v = elms[1].atoi();
		if ( v != 0 ) *comet_sigma_clip_p = true;
		else *comet_sigma_clip_p = false;
	    }
	    else if ( elms[0].strcmp("skylv_sigma_clip") == 0 ) {
		int v = elms[1].atoi();
		if ( v != 0 ) *skylv_sigma_clip_p = true;
		else *skylv_sigma_clip_p = false;
	    }
	    else if ( elms[0].strcmp("comet_sigma_clip") == 0 ) {
		int v = elms[1].atoi();
		if ( v != 0 ) *comet_sigma_clip_p = true;
		else *comet_sigma_clip_p = false;
	    }
	    else if ( elms[0].strcmp("sigma_r") == 0 ) {
		sigma_rgb[0] = elms[1].atoi();
		if ( sigma_rgb[0] < 0 ) sigma_rgb[0] = 0;
	    }
	    else if ( elms[0].strcmp("sigma_g") == 0 ) {
		sigma_rgb[1] = elms[1].atoi();
		if ( sigma_rgb[1] < 0 ) sigma_rgb[1] = 0;
	    }
	    else if ( elms[0].strcmp("sigma_b") == 0 ) {
		sigma_rgb[2] = elms[1].atoi();
		if ( sigma_rgb[2] < 0 ) sigma_rgb[2] = 0;
	    }
	}
    }
    f_in.close();
    return_status = 0;
 quit:
    return return_status;
}

static int get_offset_filename( const char *target_filename,
				tstring *ret_filename )
{
    size_t i, ix = 0;
    for ( i=0 ; target_filename[i] != '\0' ; i++ ) {
	if ( target_filename[i] == '.' ) ix = i;
    }
    if ( ix != 0 ) {
	ret_filename->assign(target_filename,ix);
	ret_filename->append(".offset.txt");
    }
    else {
	ret_filename->printf("%s.offset.txt",target_filename);
    }
    return 0;
}

static int save_sigclip_params( const char *filename,
				int n_comp_dark_synth,
				int count_sigma_clip, const int sigma_rgb[],
				bool skylv_sigma_clip, bool comet_sigma_clip )
{
    int return_status = -1;
    stdstreamio sio, f_out;
    if ( f_out.open("w", filename) < 0 ) {
	sio.eprintf("[ERROR] Cannot write data to %s\n", filename);
	goto quit;
    }
    /*
    f_out.printf("%d %d %d %d %d %d\n",
		 count_sigma_clip,
		 sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
		 (int)skylv_sigma_clip, (int)comet_sigma_clip);
    */
    f_out.printf("n_comp_dark_synth = %d\n",n_comp_dark_synth);
    f_out.printf("count_sigma_clip = %d\n",count_sigma_clip);
    f_out.printf("skylv_sigma_clip = %d\n",(int)skylv_sigma_clip);
    f_out.printf("comet_sigma_clip = %d\n",(int)comet_sigma_clip);
    f_out.printf("sigma_r = %d\n",sigma_rgb[0]);
    f_out.printf("sigma_g = %d\n",sigma_rgb[1]);
    f_out.printf("sigma_b = %d\n",sigma_rgb[2]);

    f_out.close();
    return_status = 0;
 quit:
    return return_status;
}

static int read_offset_file( const tarray_tstring &filenames, int sel_file_id,
			     long *ret_offset_x, long *ret_offset_y )
{
    stdstreamio f_in;
    tstring offset_file;
    long offset_x, offset_y;
    int ret_status = -1;
        
    get_offset_filename(filenames[sel_file_id].cstr(),
			&offset_file);
    //offset_file.dprint();
    if ( f_in.open("r", offset_file.cstr()) == 0 ) {
        tstring v = f_in.getline();
	tarray_tstring offs;
	offs.split(v," \n",false);
	//offs.dprint();
	offset_x = offs[0].atol();
	offset_y = offs[1].atol();
	f_in.close();
    }
    else {
        goto quit;
    }

    if ( ret_offset_x != NULL ) *ret_offset_x = offset_x;
    if ( ret_offset_y != NULL ) *ret_offset_y = offset_y;
    
    ret_status = 0;
 quit:
    return ret_status;
}

/*
 * Experimental code:
 *
 * The current file is compared with the file n ahead,
 * dark composite is performed, and it is returned.
 *
 * The green channel is used for brightness comparisons.
 *
 */
static bool load_tiff_into_float_and_compare(
		      const tarray_tstring &filenames,
		      const mdarray_bool &flg_saved,
		      long idx_ref_img, long idx_img,
		      long offset_idx_compared, bool skylv_sigma_clip,
		      mdarray_float *ret_img_buf )
{
    stdstreamio sio;
    bool load_tiff_ok = false;
    if ( load_tiff_into_float(filenames[idx_img].cstr(), 65536.0,
			      ret_img_buf, NULL, NULL, NULL) < 0 ) {
	sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
    }
    else {
	mdarray_float img_buf_1(false);
	mdarray_float img_compared_buf(false);
	long idx_compared;
	if ( offset_idx_compared == 0 ) {
	    load_tiff_ok = true;
	    goto quit;
	}
	if ( idx_img + offset_idx_compared < 0 ) {
	    goto quit;
	}
	if ( idx_img + offset_idx_compared < 0 ) {
	    goto quit;
	}
	idx_compared = idx_img + offset_idx_compared;
	/* */
	if ( idx_compared < (long)filenames.length() &&
	     flg_saved[idx_compared] == true ) {
	    if ( load_tiff_into_float(
			filenames[idx_compared].cstr(), 65536.0,
			&img_buf_1, NULL, NULL, NULL) < 0 ) {
		sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
	    }
	    else {
		long offset_x_0 = 0, offset_y_0 = 0;
		long offset_x_1 = 0, offset_y_1 = 0;
		if ( idx_img != idx_ref_img &&
		     read_offset_file(filenames, idx_img,
				      &offset_x_0, &offset_y_0) < 0 ) {
		    sio.eprintf("[ERROR] read_offset_file() failed.\n");
		}
		if ( idx_compared != idx_ref_img &&
		     read_offset_file(filenames, idx_compared,
				      &offset_x_1, &offset_y_1) < 0 ) {
		    sio.eprintf("[ERROR] read_offset_file() failed.\n");
		}
		img_compared_buf = (*ret_img_buf);		/* copy */

		img_compared_buf.subtract(img_buf_1,	/* subtract */
					  offset_x_1 - offset_x_0,
					  offset_y_1 - offset_y_0,
					  0);
		{
		    const float *p_cmp = img_compared_buf.array_ptr(0,0,1 /* green */);
		    size_t j,k, xylen;

		    double median_0[3] = {0.0, 0.0, 0.0};
		    double median_1[3] = {0.0, 0.0, 0.0};

		    if ( skylv_sigma_clip == true ) {
			mdarray_float img_tmp_buf_1d(false);

			ret_img_buf->copy(&img_tmp_buf_1d,
				      0, ret_img_buf->x_length(),
				      0, ret_img_buf->y_length(),
				      0, 1);
			median_0[0] = md_median(img_tmp_buf_1d);
			ret_img_buf->copy(&img_tmp_buf_1d,
				      0, ret_img_buf->x_length(),
				      0, ret_img_buf->y_length(),
				      1, 1);
			median_0[1] = md_median(img_tmp_buf_1d);
			ret_img_buf->copy(&img_tmp_buf_1d,
				      0, ret_img_buf->x_length(),
				      0, ret_img_buf->y_length(),
				      2, 1);
			median_0[2] = md_median(img_tmp_buf_1d);

			img_buf_1.copy(&img_tmp_buf_1d,
				   0, img_buf_1.x_length(),
				   0, img_buf_1.y_length(),
				   0, 1);
			median_1[0] = md_median(img_tmp_buf_1d);
			img_buf_1.copy(&img_tmp_buf_1d,
				     0, img_buf_1.x_length(),
				     0, img_buf_1.y_length(),
				     1, 1);
			median_1[1] = md_median(img_tmp_buf_1d);
			img_buf_1.copy(&img_tmp_buf_1d,
				     0, img_buf_1.x_length(),
				     0, img_buf_1.y_length(),
				     2, 1);
			median_1[2] = md_median(img_tmp_buf_1d);
		    }

		    xylen = ret_img_buf->x_length() * ret_img_buf->y_length();

		    /* compared GREEN ch */
		    for ( j=0 ; j < 3 ; j++ ) {
			float *p0 = ret_img_buf->array_ptr(0,0,j);
			const float *p1 = img_compared_buf.array_ptr(0,0,j);
			double diff = - median_0[j] + median_1[j];
			double diff_cmp = - median_0[1] + median_1[1];	/* GREEN */
			for ( k=0 ; k < xylen ; k++ ) {
			    if ( 0 < (p_cmp[k] + diff_cmp) ) {
				p0[k] = - (p1[k] - p0[k]);
				p0[k] -= diff;
			    }
			}
		    }
		}
		load_tiff_ok = true;
	    }
	}
    }

 quit:
    return load_tiff_ok;
}

static int do_stack_and_save( const tarray_tstring &filenames,
			      int ref_file_id, const mdarray_bool &flg_saved,
			      int n_comp_dark_synth,
			      int count_sigma_clip, const int sigma_rgb[], 
			      bool skylv_sigma_clip, bool comet_sigma_clip, 
			      bool flag_dither, bool flag_preview,
			      int display_bin, int display_ch, 
			      const int contrast_rgb[],
			      int win_image, mdarray_uchar *tmp_buf )
{
    stdstreamio sio;
    mdarray_float stacked_buf0_sum(false);
    mdarray_float stacked_buf0_sum2(false);
    mdarray_float stacked_buf1_sum(false);
    mdarray_float stacked_buf1_sum2(false);
    mdarray_float *stacked_buf_result_ptr = NULL;
    mdarray_short count_buf0(false);
    mdarray_short count_buf1(false);
    mdarray_short *count_buf_result_ptr = NULL;
    mdarray_uchar flag_buf(false);
    mdarray_float img_buf(false);
    mdarray_float img_tmp_buf(false);
    mdarray_float img_tmp_buf_1d(false);
    mdarray_uchar icc_buf(false);
    size_t i, ii, n_plus;
    tstring appended_str;
    tstring out_filename;
    int cnt;
    
    int ret_status = -1;
    
    if ( filenames.length() == 0 ) goto quit;
    if ( flg_saved.length() == 0 ) goto quit;
    if ( ref_file_id < 0 ) goto quit;
    if ( filenames.length() <= (size_t)ref_file_id ) goto quit;

    /* determine on/off of sigma-clip */
    if ( sigma_rgb[0] <= 0 && sigma_rgb[1] <= 0 && sigma_rgb[2] <= 0 ) {
	count_sigma_clip = 0;
    }

    sio.printf("sigma-clipping: [N_iterations=%d,  value=(%d,%d,%d),  sky-level=%d,  comet=%d]  "
	       "dither=%d\n",
	       count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
	       (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);

    /* load reference ... needs for ICC data */
    sio.printf("Stacking [%s]\n", filenames[ref_file_id].cstr());

    if ( load_tiff_into_float(filenames[ref_file_id].cstr(), 65536.0,
			      &img_buf, NULL, &icc_buf, NULL) < 0 ) {
	sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
	goto quit;
    }

    if ( load_tiff_into_float_and_compare(
			filenames, flg_saved, ref_file_id, ref_file_id,
			n_comp_dark_synth, skylv_sigma_clip,
			&img_buf ) == false ) {
	sio.eprintf("[ERROR] load_tiff_into_float_and_compare() failed\n");
	goto quit;
    }

    if ( icc_buf.length() == 0 ) {
	icc_buf.resize_1d(sizeof(Icc_srgb_profile));
	icc_buf.put_elements(Icc_srgb_profile,sizeof(Icc_srgb_profile));
    }

    /* allocate memory */
    stacked_buf1_sum.resize(img_buf);
    count_buf1.resize(img_buf);
    stacked_buf1_sum2.resize(img_buf);

    if ( 0 < count_sigma_clip ) {
	stacked_buf0_sum.resize(img_buf);
	count_buf0.resize(img_buf);
	stacked_buf0_sum2.resize(img_buf);
	flag_buf.resize_2d(img_buf.x_length(), img_buf.y_length());
	img_tmp_buf.resize(img_buf);
	img_tmp_buf_1d.resize_2d(img_buf.x_length(), img_buf.y_length());
    }

    /* paste 1st image */
    stacked_buf1_sum = img_buf;
    stacked_buf1_sum2 = img_buf;
    stacked_buf1_sum2 *= img_buf;
    
    winname(win_image, "Stacking ...");
    if ( flag_preview == true ) {
        display_image(win_image, 0, 0, img_buf, 2,
		      display_bin, display_ch, contrast_rgb, false, tmp_buf);
    }

    n_plus = 0;
    for ( i=0 ; i < filenames.length() ; i++ ) {
        if ( flg_saved[i] == true ) n_plus ++;
    }
    
    ii = 1;
    for ( i=0 ; i < filenames.length() ; i++ ) {
        if ( flg_saved[i] == true ) {
	    bool load_tiff_ok;

	    load_tiff_ok = load_tiff_into_float_and_compare(
			filenames, flg_saved, ref_file_id, i,
			n_comp_dark_synth, skylv_sigma_clip,
			&img_buf );

	    if ( load_tiff_ok == true ) {
		//double max_val;
		long offset_x = 0, offset_y = 0;

		ii ++;

		sio.printf("Stacking [%s]\n", filenames[i].cstr());

		if ( read_offset_file(filenames, i,
				      &offset_x, &offset_y) < 0 ) {
		    sio.eprintf("[ERROR] read_offset_file() failed.\n");
		}

		stacked_buf1_sum.add(img_buf, offset_x, offset_y, 0); /* STACK! */
		stacked_buf1_sum2.add(img_buf * img_buf, offset_x, offset_y, 0); /* STACK pow() */

		if ( flag_preview == true ) {
		    //max_val = md_max(stacked_buf1_sum);
		    //img_buf.paste(stacked_buf1_sum * (65535.0 / max_val));
		    img_buf = stacked_buf1_sum;
		    img_buf *= (1.0/(double)(ii));
		    /* display stacked image */
		    display_image(win_image, 0, 0, img_buf, 2,
			display_bin, display_ch, contrast_rgb, false, tmp_buf);
		}

		winname(win_image, "Stacking %zd/%zd", ii, (size_t)(1+n_plus));

	    }
	}
    }

    n_plus = ii - 1;

    count_buf1 = (int)(1 + n_plus);

    /* display stacked image */
    img_buf = stacked_buf1_sum;
    img_buf *= (1.0/(double)ii);
    display_image(win_image, 0, 0, img_buf, 2,
		  display_bin, display_ch, contrast_rgb, false, tmp_buf);

    stacked_buf_result_ptr = &stacked_buf1_sum;
    count_buf_result_ptr = &count_buf1;


    /*
     *  Perform Sigma-Clipping ...
     */

    for ( cnt=0 ; cnt < count_sigma_clip ; cnt++ ) {

	mdarray_float *stacked_buf0_sum_ptr;
	mdarray_float *stacked_buf0_sum2_ptr;
	mdarray_short *count_buf0_ptr;

	mdarray_float *stacked_buf1_sum_ptr;
	mdarray_float *stacked_buf1_sum2_ptr;
	mdarray_short *count_buf1_ptr;

	double av_median[3] = {1.0, 1.0, 1.0};

	bool final_loop = false;

	if ( cnt + 1 == count_sigma_clip ) final_loop = true;

	sio.printf("*** Sigma-Clipping count of iterations = %d / %d ***\n", cnt + 1, count_sigma_clip);

	/* swap buffer pointer */
	if ( (cnt % 2) == 0 ) {
	    stacked_buf0_sum_ptr = &stacked_buf0_sum;
	    stacked_buf0_sum2_ptr = &stacked_buf0_sum2;
	    count_buf0_ptr = &count_buf0;
	    stacked_buf1_sum_ptr = &stacked_buf1_sum;
	    stacked_buf1_sum2_ptr = &stacked_buf1_sum2;
	    count_buf1_ptr = &count_buf1;
	}
	else {
	    stacked_buf1_sum_ptr = &stacked_buf0_sum;
	    stacked_buf1_sum2_ptr = &stacked_buf0_sum2;
	    count_buf1_ptr = &count_buf0;
	    stacked_buf0_sum_ptr = &stacked_buf1_sum;
	    stacked_buf0_sum2_ptr = &stacked_buf1_sum2;
	    count_buf0_ptr = &count_buf1;
	}

	if ( skylv_sigma_clip == true ) {
	    /* get median(R,G,B) of averaged image */
	    img_tmp_buf = (*stacked_buf1_sum_ptr);
	    img_tmp_buf /= (*count_buf1_ptr);
	    img_tmp_buf.copy(&img_tmp_buf_1d,
		0, img_tmp_buf.x_length(), 0, img_tmp_buf.y_length(), 0, 1);
	    av_median[0] = md_median(img_tmp_buf_1d);
	    img_tmp_buf.copy(&img_tmp_buf_1d,
		0, img_tmp_buf.x_length(), 0, img_tmp_buf.y_length(), 1, 1);
	    av_median[1] = md_median(img_tmp_buf_1d);
	    img_tmp_buf.copy(&img_tmp_buf_1d,
		0, img_tmp_buf.x_length(), 0, img_tmp_buf.y_length(), 2, 1);
	    av_median[2] = md_median(img_tmp_buf_1d);
	    sio.printf("Median of averaged image = (%g, %g, %g)\n",
		       av_median[0], av_median[1], av_median[2]);
	}

	/* clear buffer for new result */
	stacked_buf0_sum_ptr->clean();
	stacked_buf0_sum2_ptr->clean();
	count_buf0_ptr->clean();

	/*
	 *  main of sigma-clipping
	 */
	ii = 0;
	for ( i=0 ; i < filenames.length() ; i++ ) {
	    if ( (int)i == ref_file_id || flg_saved[i] == true ) {
		bool load_tiff_ok = false;

		load_tiff_ok = load_tiff_into_float_and_compare(
			filenames, flg_saved, ref_file_id, i,
			n_comp_dark_synth, skylv_sigma_clip,
			&img_buf );

		/*
		if ( load_tiff_into_float(filenames[i].cstr(), 65536.0,
					  &img_buf, NULL, NULL, NULL) < 0 ) {
		    sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
		}
		else {
		    load_tiff_ok = true;
		}
		*/
	        if ( load_tiff_ok == true ) {
		    unsigned char *p5 = flag_buf.array_ptr();
		    size_t j;
		    double target_median[3] = {1.0, 1.0, 1.0};
		    //double max_val;
		    long offset_x = 0, offset_y = 0;

		    ii ++;

		    sio.printf("Stacking with Sigma-Clipping [%s]\n", filenames[i].cstr());

		    if ( (int)i != ref_file_id ) {
			if ( read_offset_file(filenames, i,
				              &offset_x, &offset_y) < 0 ) {
			    sio.eprintf("[ERROR] read_offset_file() failed.\n");
			}
		    }

		    if ( skylv_sigma_clip == true ) {
			/* get median(R,G,B) of target image */
			target_median[0] = md_median(img_buf.sectionf("*,*,0"));
			target_median[1] = md_median(img_buf.sectionf("*,*,1"));
			target_median[2] = md_median(img_buf.sectionf("*,*,2"));
			sio.printf("Median of target image = (%g, %g, %g)\n",
				   target_median[0], target_median[1], target_median[2]);
		    }

		    img_tmp_buf.clean();
		    img_tmp_buf.add(img_buf, offset_x, offset_y, 0);
		    size_t len_xy = img_tmp_buf.x_length() * img_tmp_buf.y_length();

		    flag_buf = 0;

		    if ( comet_sigma_clip == true && final_loop == true && (int)i == ref_file_id ) {
			/* Stack a reference image without sigma-clipping (when last loop) */
			(*count_buf0_ptr) += (int)1;
		    }
		    else {
			for ( j=0 ; j < 3 ; j++ ) {
			    size_t k;
			    const float *p0 = stacked_buf1_sum_ptr->array_ptr_cs(0,0,j);
			    const float *p1 = stacked_buf1_sum2_ptr->array_ptr_cs(0,0,j);
			    const short *p2 = count_buf1_ptr->array_ptr_cs(0,0,j);
			    const float *p3 = img_tmp_buf.array_ptr_cs(0,0,j);

			    if ( 0 < sigma_rgb[j] ) {
			        double sigma_factor_limit = sigma_rgb[j] / 10.0;
			        for ( k=0 ; k < len_xy ; k++ ) {
				    double sum  = p0[k];
				    double mean = sum / (double)(p2[k]);
				    double sum2 = p1[k];
				    double sigma = sqrt( (sum2 - 2 * mean * sum + mean * mean * p2[k])
					                 / (double)(p2[k] - 1) );
				    /* apply *standardized* pixel value */
				    //double pix_val = p3[k] * (av_median[j] / target_median[j]);
				    /* apply *sky-level-adjusted* pixel value */
				    double pix_val = p3[k] + (av_median[j] - target_median[j]);
				    if ( sigma_factor_limit * sigma < fabs(mean - pix_val) ) {
				        p5[k] ++;		/* mark unused pixels */
				    }
			        }
			    }
		        }
		        for ( j=0 ; j < 3 ; j++ ) {
			    float *p3 = img_tmp_buf.array_ptr(0,0,j);
			    short *p4 = count_buf0_ptr->array_ptr(0,0,j);
			    size_t k;
			    for ( k=0 ; k < len_xy ; k++ ) {
			        if ( 0 < p5[k] ) {
				    p3[k] = 0.0;	/* unused pixels */
			        }
			        else {
				    p4[k] ++;
			        }
			    }
		        }
		    }

		    (*stacked_buf0_sum_ptr) += img_tmp_buf;		/* STACK! */
		    img_tmp_buf *= img_tmp_buf;
		    (*stacked_buf0_sum2_ptr) += img_tmp_buf; /* img_tmp_buf^2 */

		    if ( flag_preview == true || ii == 1 + n_plus ) {
			//max_val = md_max(stacked_buf0_sum);
			//img_buf.paste(stacked_buf0_sum * (65535.0 / max_val));
			img_buf = (*stacked_buf0_sum_ptr);
			img_buf /= (*count_buf0_ptr);
			/* display stacked image */
			display_image(win_image, 0, 0, img_buf, 2,
		         display_bin, display_ch, contrast_rgb, false, tmp_buf);
		    }

		    winname(win_image, "Stacking with sigma-clipping %zd/%zd", ii, (size_t)(1+n_plus));

		}
	    }
	}
	stacked_buf_result_ptr = stacked_buf0_sum_ptr;
	count_buf_result_ptr = count_buf0_ptr;
	sio.printf("Median of pixel-count = %g frames\n", md_median(*(count_buf0_ptr)));
    }


    /* get final averaged image */
    (*stacked_buf_result_ptr) /= (*count_buf_result_ptr);

    /* free memory */
    img_tmp_buf_1d.init(false);
    img_tmp_buf.init(false);
    flag_buf.init(false);
    stacked_buf0_sum2.init(false);
    count_buf0.init(false);
    stacked_buf1_sum2.init(false);
    count_buf1.init(false);
    img_buf.init(false);

    /* display */
    winname(win_image, "Done stacking %zd frames", (size_t)(1+n_plus));

    /* save */
#if 1
    appended_str.printf("+%zdframes_stacked", n_plus);

    /* save using float */
    make_tiff_filename(filenames[ref_file_id].cstr(), appended_str.cstr(),
		       "float", &out_filename);
    sio.printf("Writing '%s' ...\n", out_filename.cstr());
    if ( save_float_to_tiff(*stacked_buf_result_ptr, icc_buf, NULL, 
			    65536.0, out_filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] save_float_to_tiff() failed.\n");
	goto quit;
    }
    
    /* save using 16-bit */
    make_tiff_filename(filenames[ref_file_id].cstr(), appended_str.cstr(),
		       "16bit", &out_filename);
    sio.printf("Writing '%s' ", out_filename.cstr());
    if ( flag_dither == true ) sio.printf("using dither ...\n");
    else sio.printf("NOT using dither ...\n");
    sio.printf("[INFO] scale will be changed\n");
    if ( save_float_to_tiff48(*stacked_buf_result_ptr, icc_buf, NULL,
			    0.0, 0.0, flag_dither, out_filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] save_float_to_tiff48() failed.\n");
	goto quit;
    }
#endif
    
    ret_status = 0;
 quit:
    return ret_status;
}


const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Target            [1]"},
#define CMD_DISPLAY_REFERENCE 2
        {CMD_DISPLAY_REFERENCE, "Display Reference         [2]"},
#define CMD_DISPLAY_RESIDUAL1 3
        {CMD_DISPLAY_RESIDUAL1, "Display Residual x1       [3]"},
#define CMD_DISPLAY_RESIDUAL2 4
        {CMD_DISPLAY_RESIDUAL2, "Display Residual x2       [4]"},
#define CMD_DISPLAY_RESIDUAL4 5
        {CMD_DISPLAY_RESIDUAL4, "Display Residual x4       [5]"},
#define CMD_DISPLAY_RESIDUAL8 6
        {CMD_DISPLAY_RESIDUAL8, "Display Residual x8       [6]"},
#define CMD_DISPLAY_RGB 7
        {CMD_DISPLAY_RGB,       "Display RGB               [c]"},
#define CMD_DISPLAY_R 8
        {CMD_DISPLAY_R,         "Display Red               [c]"},
#define CMD_DISPLAY_G 9
        {CMD_DISPLAY_G,         "Display Green             [c]"},
#define CMD_DISPLAY_B 10
        {CMD_DISPLAY_B,         "Display Blue              [c]"},
#define CMD_ZOOM 11
        {CMD_ZOOM,              "Zoom +/-                  [+][-]"},
#define CMD_CONT_RGB 12
        {CMD_CONT_RGB,          "RGB Contrast +/-          [<][>]"},
#define CMD_CONT_R 13
        {CMD_CONT_R,            "Red Contrast +/-          [r][R]"},
#define CMD_CONT_G 14
        {CMD_CONT_G,            "Green Contrast +/-        [g][G]"},
#define CMD_CONT_B 15
        {CMD_CONT_B,            "Blue Contrast +/-         [b][B]"},
#define CMD_LOUPE_ZOOM 16
        {CMD_LOUPE_ZOOM,        "Zoom factor of loupe +/-  [l][L]"},
#define CMD_SAVE 17
        {CMD_SAVE,              "Save Offset Info          [Enter]"},
#define CMD_SAVE_ALL 18
        {CMD_SAVE_ALL,          "Save Offset for All files [A]"},
#define CMD_DELETE 19
        {CMD_DELETE,            "Delete Offset Info        [Del]"},
#define CMD_CLR_OFF 20
        {CMD_CLR_OFF,           "Clear Currnt Offset       [0]"},
#define CMD_DISPLAY_PARAMS 21
        {CMD_DISPLAY_PARAMS,    "Display parameters        [p]"},
#define CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM 22
        {CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM,    "N comp Dark Synthesis +/- [y][Y]"},
#define CMD_SIGCLIP_CNT_PM 23
        {CMD_SIGCLIP_CNT_PM,    "N iterations Sig-Clip +/- [i][I]"},
#define CMD_SIGCLIP_PM 24
        {CMD_SIGCLIP_PM,        "Val of Sigma-Clip +/-     [v][V]"},
#define CMD_SIGCLIP_SKYLV 25
        {CMD_SIGCLIP_SKYLV,     "Sky-lv Sigma-Clip on/off  [s]"},
#define CMD_SIGCLIP_COMET 26
        {CMD_SIGCLIP_COMET,     "Comet Sigma-Clip on/off   [m]"},
#define CMD_DITHER 27
        {CMD_DITHER,            "Dither on/off for saving  [d]"},
#define CMD_STACK 28
        {CMD_STACK,             "Start Stacking"},
#define CMD_STACK_SILENT 29
        {CMD_STACK_SILENT,      "Start Stacking without preview"},
#define CMD_EXIT 30
        {CMD_EXIT,              "Exit                     [q][ESC]"},
        {0, NULL}		/* EOL */
};

int main( int argc, char *argv[] )
{
    const char *conf_file_display = "display_1.txt";
    const char *conf_file_sigclip = "sigclip_1.txt";

    stdstreamio sio, f_in;
    pipestreamio p_in;
    tstring refframe, filename_frames;
    tarray_tstring filenames;
    mdarray_bool flg_saved(false);
    int ref_file_id = -1;
    int sel_file_id = -1;
    
    command_win command_win_rec;
    size_t max_len_menu_string;
    int win_filesel, win_image;
    size_t len_framecount, pos_framecount;
    
    mdarray_float ref_img_buf(false);	/* buffer for reference image */
    mdarray_float img_buf(false);	/* buffer for target */
    mdarray loupe_buf(UCHAR_ZT,false);	/* buffer for loupe */
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */
    mdarray_uchar tmp_buf_loupe(false);	/* tmp buffer for displaying */

    mdarray_float img_display(false);	/* buffer for displaying image */

    int display_type = 1;		/* flag to display image type */
    int display_ch = 0;			/* 0=RGB 1=R 2=G 3=B */
    int display_bin = 1;		/* binning factor for display */
    int contrast_rgb[3] = {8, 8, 8};	/* contrast for display */

    int loupe_height = 220;
    int loupe_zoom = 7;
    int loupe_x = Loupe_pos_out;
    int loupe_y = Loupe_pos_out;

    int n_comp_dark_synth = 0;
    int count_sigma_clip = 0;
    bool skylv_sigma_clip = true;
    bool comet_sigma_clip = false;
    int sigma_rgb[3] = {20, 20, 20};

    bool flag_dither = true;

    const char *names_ch[] = {"RGB", "Red", "Green", "Blue"};
    
    long offset_x = 0;
    long offset_y = 0;

    size_t i;
    int arg_cnt;

    int return_status = -1;

    load_display_params(conf_file_display, contrast_rgb);
    load_sigclip_params(conf_file_sigclip, &n_comp_dark_synth,
			&count_sigma_clip, sigma_rgb, &skylv_sigma_clip, &comet_sigma_clip);

    for ( arg_cnt=1 ; arg_cnt < argc ; arg_cnt++ ) {
	tstring argstr;
	argstr = argv[arg_cnt];
	if ( argstr == "-r" ) {
	    arg_cnt ++;
	    refframe = argv[arg_cnt];
	}
    }

    if ( refframe.length() < 1 ) {
	if ( f_in.open("r", Refframe_conffile) < 0 ) {
	    sio.eprintf("Interactive xy matching and stacking tool\n");
	    sio.eprintf("\n");
	    sio.eprintf("[INFO] Please write filename of reference frame in %s.\n",
			Refframe_conffile);
	    sio.eprintf("--------------- example1 ----------------\n");
	    sio.eprintf("20240101-??????_FRAME_????.tiff\n");
	    sio.eprintf("20240101-221234_FRAME_0001.tiff\n");
	    sio.eprintf("--------------- example2 ----------------\n");
	    sio.eprintf("FRAME_0001.tiff\n");
	    sio.eprintf("-----------------------------------------\n");
	    goto quit;
	}
	refframe = f_in.getline();			/* 1st line */
	refframe.trim();
	if ( 0 <= refframe.strchr('?') || 0 <= refframe.strchr('*') ) {
	    filename_frames = refframe;
	    refframe = f_in.getline();			/* 2nd line */
	    refframe.trim();
	}

	f_in.close();
    }

    sio.printf("refframe = [%s]\n", refframe.cstr());

    if ( filename_frames.length() < 1 ) {
    
	filename_frames = refframe;

	/* Search frame number from right-side of filename */
	pos_framecount = filename_frames.strrspn("[^0-9]");
	//sio.eprintf("[DEBUG] pos_framecount = %d.\n",(int)pos_framecount);
	if ( 0 < pos_framecount &&
	     pos_framecount < filename_frames.length()  ) {
	    len_framecount =
	 filename_frames.strrspn(filename_frames.length() - pos_framecount - 1,
				 "[0-9]");
	    //sio.eprintf("[DEBUG] len_framecount = %d.\n",(int)len_framecount);
	    if ( 0 < len_framecount ) {
		filename_frames.replace(
		    filename_frames.length() - pos_framecount - len_framecount,
		    len_framecount ,'?', len_framecount);
	    }
	}
    }

    sio.printf("filename_frames = [%s]\n", filename_frames.cstr());

    if ( p_in.openf("r", "/bin/ls %s", filename_frames.cstr()) < 0 ) {
        sio.eprintf("[ERROR] cannot open pipe.\n");
	goto quit;
    }
    i = 0;
    while ( 1 ) {
        const char *v = p_in.getline();
	if ( v == NULL ) break;
	filenames[i] = v;
	filenames[i].trim();
	i++;
    }
    flg_saved.resize_1d(filenames.length());
    //filenames.dprint();
    p_in.close();

    /* get ref_file_id */
    for ( i=0 ; i < filenames.length() ; i++ ) {
	if ( filenames[i] == refframe ) {
	    ref_file_id = i;
	}
	else {
	    tstring offset_file;
	    get_offset_filename(filenames[i].cstr(),
				&offset_file);
	    if ( f_in.open("r", offset_file.cstr()) == 0 ) {
	        flg_saved[i] = true;
		f_in.close();
	    }
	}
    }

    if ( ref_file_id < 0 ) {
        sio.eprintf("[ERROR] ref_file_id is not set.\n");
	goto quit;
    }

    
    /*
     * GRAPHICS
     */
    
    gsetinitialattributes(DISABLE, BOTTOMLEFTORIGIN) ;

    /* Font selector */
    set_fontsize();
    
    /* command window */
    
    get_command_list_info(Cmd_list, &max_len_menu_string);
    loupe_height = (get_fontsize() / 2) * max_len_menu_string;

    command_win_rec = gopen_command_window( Cmd_list,
      get_font_margin() + 2*get_fontsize() + get_font_margin() + loupe_height);

    loupe_buf.resize_3d(command_win_rec.width, loupe_height, 3);
    
    /* file selector */
    
    win_filesel = gopen_file_selector(filenames, true);
    if ( win_filesel < 0 ) {
        sio.eprintf("[ERROR] gopen_file_selector() failed.\n");
	goto quit;
    }
    
    display_file_list( win_filesel, filenames, -1, false,
		       ref_file_id, flg_saved.array_ptr() );

    /* image viewer */

    sio.printf("Open: %s\n", filenames[ref_file_id].cstr());
    if ( load_tiff_into_float(filenames[ref_file_id].cstr(), 65536.0,
			      &ref_img_buf, NULL, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
	goto quit;
    }
    /* for loupe */
    img_display.resize(ref_img_buf);
    img_display.paste(ref_img_buf);

    display_bin = get_bin_factor_for_display(ref_img_buf.x_length(),
					     ref_img_buf.y_length(), true);
    if ( display_bin < 0 ) {
        sio.eprintf("[ERROR] get_bin_factor_for_display() failed: "
		    "bad display depth\n");
	goto quit;
    }

    win_image = gopen(ref_img_buf.x_length() / display_bin,
		      ref_img_buf.y_length() / display_bin);
    
    /* display reference image */
    display_image(win_image, 0, 0, ref_img_buf, 2,
		  display_bin, display_ch, contrast_rgb, true, &tmp_buf);

    winname(win_image, 
	    "zoom=%3.2f  contrast=(%d,%d,%d)  "
	    "N-comp_dark_synth=%d  "
	    "sigma-clipping: [N_iterations=%d,  value=(%d,%d,%d),  sky-level=%d,  comet=%d]  "
	    "dither=%d",
	    (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
	    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
	    (int)n_comp_dark_synth, 
	    count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
	    (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);
    
    sio.printf("Initial Parameters:\n"
	    " N-comp_dark_synth=%d\n"
	    " sigma-clipping: [N_iterations=%d,  value=(%d,%d,%d),  sky-level=%d,  comet=%d]\n"
	    " dither=%d\n",
	    (int)n_comp_dark_synth, 
	    count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
	    (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);
    
    /*
     * MAIN EVENT LOOP
     */

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        bool refresh_list = false;
        int refresh_image = 0;		/* 1:display only  2:both */
	int refresh_loupe = 0;
	bool refresh_winsize = false;
        bool refresh_winname = false;
	int save_offset = 0;
	bool delete_offset = false;

	int f_id = -1;
	int cmd_id = -1;
	
        /* waiting an event */
        ev_win = eggx_ggetevent(&ev_type,&ev_btn,&ev_x,&ev_y);

	/*
	 *  Check command window
	 */
	
	if ( ev_type == ButtonPress && 1 <= ev_btn && ev_btn <= 3 &&
	     ev_win == command_win_rec.win_id ) {
	    cmd_id = 1 + ev_y / command_win_rec.cell_height;
	}
	else if ( ev_type == KeyPress ) {
	    //sio.printf("[%d]\n", ev_btn);
	    if ( ev_btn == '1' ) cmd_id = CMD_DISPLAY_TARGET;
	    else if ( ev_btn == '2' ) cmd_id = CMD_DISPLAY_REFERENCE;
	    else if ( ev_btn == '3' ) cmd_id = CMD_DISPLAY_RESIDUAL1;
	    else if ( ev_btn == '4' ) cmd_id = CMD_DISPLAY_RESIDUAL2;
	    else if ( ev_btn == '5' ) cmd_id = CMD_DISPLAY_RESIDUAL4;
	    else if ( ev_btn == '6' ) cmd_id = CMD_DISPLAY_RESIDUAL8;
	    else if ( ev_btn == 'c' ) {
	        /* rotate RGB/R/G/B */
	        if ( display_ch == 0 ) cmd_id = CMD_DISPLAY_R;
		else if ( display_ch == 1 ) cmd_id = CMD_DISPLAY_G;
		else if ( display_ch == 2 ) cmd_id = CMD_DISPLAY_B;
		else cmd_id = CMD_DISPLAY_RGB;
	    }
	    else if ( ev_btn == '+' || ev_btn == ';' ) {
		cmd_id = CMD_ZOOM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == '-' ) {
		cmd_id = CMD_ZOOM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == '>' || ev_btn == '.' ) {
		cmd_id = CMD_CONT_RGB;
		ev_btn = 1;
	    }
	    else if ( ev_btn == '<' || ev_btn == ',' ) {
		cmd_id = CMD_CONT_RGB;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'r' ) {
		cmd_id = CMD_CONT_R;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'R' ) {
		cmd_id = CMD_CONT_R;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'g' ) {
		cmd_id = CMD_CONT_G;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'G' ) {
		cmd_id = CMD_CONT_G;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'b' ) {
		cmd_id = CMD_CONT_B;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'B' ) {
		cmd_id = CMD_CONT_B;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'l' ) {
		cmd_id = CMD_LOUPE_ZOOM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'L' ) {
		cmd_id = CMD_LOUPE_ZOOM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'p' ) {
		cmd_id = CMD_DISPLAY_PARAMS;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'y' ) {
		cmd_id = CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'Y' ) {
		cmd_id = CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'i' ) {
		cmd_id = CMD_SIGCLIP_CNT_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'I' ) {
		cmd_id = CMD_SIGCLIP_CNT_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 'v' ) {
		cmd_id = CMD_SIGCLIP_PM;
		ev_btn = 1;
	    }
	    else if ( ev_btn == 'V' ) {
		cmd_id = CMD_SIGCLIP_PM;
		ev_btn = 3;
	    }
	    else if ( ev_btn == 's' ) cmd_id = CMD_SIGCLIP_SKYLV;
	    else if ( ev_btn == 'm' ) cmd_id = CMD_SIGCLIP_COMET;
	    else if ( ev_btn == 'd' ) cmd_id = CMD_DITHER;
	    else if ( ev_btn == 13 ) cmd_id = CMD_SAVE;
	    else if ( ev_btn == 'A' ) cmd_id = CMD_SAVE_ALL;
	    else if ( ev_btn == 127 ) cmd_id = CMD_DELETE;
	    else if ( ev_btn == '0' ) cmd_id = CMD_CLR_OFF;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn == 'q' ) cmd_id = CMD_EXIT;
	}

	/*
	 *  Handle cmd_id
	 */
	
	if ( cmd_id == CMD_EXIT ) {
	    break;
	}
	else if ( cmd_id == CMD_SAVE_ALL ) {
	    save_offset = 2;
	    refresh_list = true;
	}
	else if ( cmd_id == CMD_STACK || cmd_id == CMD_STACK_SILENT ) {
	    /* preview on/off */
	    bool flag_preview = true;
	    if ( cmd_id == CMD_STACK_SILENT ) flag_preview = false;
	    /* save memory ... */
	    img_display.init(false);
	    img_buf.init(false);
	    if ( do_stack_and_save( filenames, ref_file_id, flg_saved,
				    n_comp_dark_synth,
				    count_sigma_clip, sigma_rgb, 
				    skylv_sigma_clip, comet_sigma_clip, 
				    flag_dither, flag_preview,
				    display_bin, display_ch, contrast_rgb,
				    win_image, &tmp_buf ) < 0 ) {
	        sio.eprintf("[ERROR] do_stack_and_save() failed\n");
	    }
	    if ( 0 <= sel_file_id ) {
		/* reload */
		if ( load_tiff_into_float(filenames[sel_file_id].cstr(),
				   65536.0, &img_buf, NULL, NULL, NULL) < 0 ) {
		    sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
		    sel_file_id = -1;
		}
	    }
	}
	else if ( cmd_id == CMD_DISPLAY_PARAMS ) {
	    sio.printf("Current Parameters:\n"
		" N-comp_dark_synth=%d\n"
		" sigma-clipping: [N_iterations=%d,  value=(%d,%d,%d),  sky-level=%d,  comet=%d]\n"
		" dither=%d\n",
		(int)n_comp_dark_synth, 
		count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
		(int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_DISPLAY_RGB ) {
	    display_ch = 0;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_R ) {
	    display_ch = 1;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_G ) {
	    display_ch = 2;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_DISPLAY_B ) {
	    display_ch = 3;
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 1 ) {
	    if ( minus_binning_with_limit(&display_bin,
		  ref_img_buf.x_length(), ref_img_buf.y_length()) == true ) {
		refresh_image = 1;
		refresh_winsize = true;
	    }
	}
	else if ( cmd_id == CMD_ZOOM && ev_btn == 3 ) {
	    if ( plus_binning_with_limit(&display_bin) == true ) {
		refresh_image = 1;
		refresh_winsize = true;
	    }
	}
	else if ( cmd_id == CMD_CONT_RGB && ev_btn == 1 ) {
	    contrast_rgb[0] ++;
	    contrast_rgb[1] ++;
	    contrast_rgb[2] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_RGB && ev_btn == 3 ) {
	    bool changed = false;
	    if ( 0 < contrast_rgb[0] ) {
	        contrast_rgb[0] --;  changed = true;
	    }
	    if ( 0 < contrast_rgb[1] ) {
	        contrast_rgb[1] --;  changed = true;
	    }
	    if ( 0 < contrast_rgb[2] ) {
	        contrast_rgb[2] --;  changed = true;
	    }
	    if ( changed == true ) {
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 1 ) {
	    contrast_rgb[0] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_R && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[0] ) {
		contrast_rgb[0] --;
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 1 ) {
	    contrast_rgb[1] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_G && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[1] ) {
		contrast_rgb[1] --;
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 1 ) {
	    contrast_rgb[2] ++;
	    save_display_params(conf_file_display, contrast_rgb);
	    refresh_image = 1;
	}
	else if ( cmd_id == CMD_CONT_B && ev_btn == 3 ) {
	    if ( 0 < contrast_rgb[2] ) {
		contrast_rgb[2] --;
		save_display_params(conf_file_display, contrast_rgb);
		refresh_image = 1;
	    }
	}
	else if ( cmd_id == CMD_LOUPE_ZOOM && ev_btn == 1 ) {
	    if ( loupe_zoom < loupe_height / 2 ) {
		loupe_zoom += 2;
		refresh_loupe = 1;
	    }
	}
	else if ( cmd_id == CMD_LOUPE_ZOOM && ev_btn == 3 ) {
	    if ( 4 < loupe_zoom ) {
		loupe_zoom -= 2;
		refresh_loupe = 1;
	    }
	}
	else if ( cmd_id == CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM && ev_btn == 1 ) {
	    n_comp_dark_synth ++;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_COMPARATIVE_DARK_SYNTHESIS_CNT_PM && ev_btn == 3 ) {
	    if ( 0 < n_comp_dark_synth ) n_comp_dark_synth --;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_CNT_PM && ev_btn == 1 ) {
	    count_sigma_clip ++;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_CNT_PM && ev_btn == 3 ) {
	    if ( 0 < count_sigma_clip ) count_sigma_clip --;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_PM && ev_btn == 1 ) {
	    sigma_rgb[0] ++;
	    sigma_rgb[1] ++;
	    sigma_rgb[2] ++;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_PM && ev_btn == 3 ) {
	    if ( 0 < sigma_rgb[0] ) sigma_rgb[0] --;
	    if ( 0 < sigma_rgb[1] ) sigma_rgb[1] --;
	    if ( 0 < sigma_rgb[2] ) sigma_rgb[2] --;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_SKYLV ) {
	    if ( skylv_sigma_clip == true ) skylv_sigma_clip = false;
	    else skylv_sigma_clip = true;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_SIGCLIP_COMET ) {
	    if ( comet_sigma_clip == true ) comet_sigma_clip = false;
	    else comet_sigma_clip = true;
	    save_sigclip_params(conf_file_sigclip, n_comp_dark_synth, count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
	    refresh_winname = true;
	}
	else if ( cmd_id == CMD_DITHER ) {
	    if ( flag_dither == true ) flag_dither = false;
	    else flag_dither = true;
	    refresh_winname = true;
	}

	if ( ev_win == win_image &&
	     ( ev_type == MotionNotify || 
	       ev_type == EnterNotify || ev_type == LeaveNotify ) ) {
	    if ( ev_type == LeaveNotify ) {
		ev_x = Loupe_pos_out;
		ev_y = Loupe_pos_out;
	    }
	    cmd_id = 0;
	    refresh_loupe = 2;
	}

	/*
	 * Only when *SELECTED*
	 */

	if ( 0 <= sel_file_id ) {
	  
	    if ( CMD_DISPLAY_TARGET <= cmd_id &&
		 cmd_id <= CMD_DISPLAY_RESIDUAL8 ) {
		display_type = cmd_id - CMD_DISPLAY_TARGET;
		refresh_image = 2;
	    }
	    else if ( cmd_id == CMD_SAVE ) {
		save_offset = 1;
		refresh_list = true;
	    }
	    else if ( cmd_id == CMD_DELETE ) {
		delete_offset = true;
		refresh_list = true;
	    }
	    else if ( cmd_id == CMD_CLR_OFF ) {
		offset_x = 0;
		offset_y = 0;
		refresh_image = 2;
	    }
	    else if ( ev_type == ButtonPress ) {
		if ( ev_win == win_image && ev_btn == 3 /* right btn */ ) {
		    offset_x = ev_x - (ref_img_buf.x_length() / 2);
		    offset_y = ev_y - (ref_img_buf.y_length() / 2);
		    cmd_id = 0;
		    refresh_image = 2;
		}
	    }
	    /* */
	    else if ( ev_type == KeyPress ) {
		//sio.printf("key = %d\n", ev_btn);
		if ( ev_btn == 28 ) {		/* Right key */
		    offset_x ++;
		    cmd_id = 0;
		    refresh_image = 2;
		}
		else if ( ev_btn == 29 ) {	/* Left key */
		    offset_x --;
		    cmd_id = 0;
		    refresh_image = 2;
		}
		else if ( ev_btn == 30 && ev_win != win_filesel ) { /* Up */
		    offset_y --;
		    cmd_id = 0;
		    refresh_image = 2;
		}
		else if ( ev_btn == 31 && ev_win != win_filesel ) { /* Down */
		    offset_y ++;
		    cmd_id = 0;
		    refresh_image = 2;
		}
		else if ( ev_btn == ' ' ) {
		    if ( display_type == 0 ) display_type = 1;
		    else display_type = 0;
		    cmd_id = 0;
		    refresh_image = 2;
		}
	    }
	}

	/* Handle offset files */

	if ( delete_offset == true || save_offset != 0 ) {
	  
	    stdstreamio f_out;
	    tstring offset_file;

	    if ( delete_offset == true ) {
		get_offset_filename(filenames[sel_file_id].cstr(),
				    &offset_file);
		//sio.printf("[%s]\n", offset_file.cstr());
	        unlink(offset_file.cstr());
		flg_saved[sel_file_id] = false;
		sio.printf("Deleted: [%s]\n", offset_file.cstr());
	    }

	    if ( save_offset == 1 ) {
		get_offset_filename(filenames[sel_file_id].cstr(),
				    &offset_file);
		//sio.printf("[%s]\n", offset_file.cstr());
	        if ( f_out.open("w", offset_file.cstr()) < 0 ) {
		    sio.eprintf("[ERROR] Cannot save!\n");
		}
		else {
		    f_out.printf("%ld %ld\n", offset_x, offset_y);
		    f_out.close();
		    flg_saved[sel_file_id] = true;
		    sio.printf("Saved: [%s]\n", offset_file.cstr());
		}
	    }
	    else if ( save_offset == 2 ) {	/* save offset to all files */
		size_t j;
		for ( j=0 ; j < filenames.length() ; j++ ) {
		    if ( (int)j != ref_file_id && flg_saved[j] != true ) {
			get_offset_filename(filenames[j].cstr(), &offset_file);
			if ( f_out.open("w", offset_file.cstr()) < 0 ) {
			    sio.eprintf("[ERROR] Cannot save!\n");
			}
			else {
			    f_out.printf("%ld %ld\n", offset_x, offset_y);
			    f_out.close();
			    flg_saved[j] = true;
			    sio.printf("Saved offset (%ld,%ld) for %s\r",
				       offset_x, offset_y, filenames[j].cstr());
			    sio.flush();
			}
		    }
		}
		sio.printf("\n");
	    }

	}

	/*
	 *  Check file selector
	 */
	if ( 0 <= cmd_id ) {
	    /* NOP */
	}
	else if ( ev_type == ButtonPress && ev_btn == 1 &&
	     ev_win == win_filesel ) {
	    f_id = ev_y / get_fontsize();
	}
	else if ( ev_type == KeyPress && ev_btn == 6 /* PageDown */ ) {
	    f_id = sel_file_id + 1;
	    if ( f_id == ref_file_id ) f_id ++;
	}
	else if ( ev_type == KeyPress && ev_btn == 2 /* PageUp */ ) {
	    f_id = sel_file_id - 1;
	    if ( f_id == ref_file_id ) f_id --;
	}
	else if ( ev_type == KeyPress && ev_btn == 31 /* Down key */ ) {
	    f_id = sel_file_id + 1;
	    if ( f_id == ref_file_id ) f_id ++;
	}
	else if ( ev_type == KeyPress && ev_btn == 30 /* Up key */ ) {
	    f_id = sel_file_id - 1;
	    if ( f_id == ref_file_id ) f_id --;
	}
	
	if ( f_id != ref_file_id &&
	     0 <= f_id && (size_t)f_id < filenames.length() ) {

	    display_file_list( win_filesel, filenames, f_id, true,
			       ref_file_id, flg_saved.array_ptr() );
	    
	    sio.printf("Open: %s\n", filenames[f_id].cstr());
	    img_display.init(false);	/* save memory ... */
		    
	    if ( load_tiff_into_float(filenames[f_id].cstr(), 65536.0,
				      &img_buf, NULL, NULL, NULL) < 0 ) {
	        sio.eprintf("[ERROR] load_tiff_into_float() failed\n");
		sel_file_id = -1;
	    }
	    else {

		sel_file_id = f_id;
		//sio.printf("%ld\n", sel_file_id);

		if ( flg_saved[sel_file_id] == true ) {
		    if ( read_offset_file(filenames, sel_file_id,
					  &offset_x, &offset_y) < 0 ) {
			sio.eprintf("[ERROR] read_offset_file() failed.\n");
		    }
		}
		if ( display_type == 1 ) display_type = 0;

		refresh_image = 2;
		refresh_list = true;

	    }
	}

	/* Update window */
	    
	if ( refresh_image != 0 ) {
	    const int tiff_szt = 2;
	    if ( 2 <= display_type ) {	/* Residual */
		if ( 1 < refresh_image || img_display.length() == 0 ) {
		    //double res_total;
		    img_display.resize(ref_img_buf);
		    img_display.paste(ref_img_buf);
		    img_display.subtract(img_buf, offset_x, offset_y, 0);
		    img_display.abs();
		    if ( display_type == 3 ) img_display *= 2.0;
		    else if ( display_type == 4 ) img_display *= 4.0;
		    else if ( display_type == 5 ) img_display *= 8.0;
		}
		display_image(win_image, 0, 0, img_display, tiff_szt,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Residual  offset = ( %ld, %ld )  "
		       "channel = %s  zoom = %3.2f  contrast = ( %d, %d, %d )  ",
		       offset_x, offset_y, names_ch[display_ch],
		       (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
		       contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
		//img_display.init(false);
	    }
	    else if ( display_type == 1 ) {	/* Reference */
		display_image(win_image, 0, 0, ref_img_buf, tiff_szt,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Reference  "
		       "channel = %s  zoom = %3.2f  contrast = ( %d, %d, %d )  ",
		       names_ch[display_ch],
		       (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
		       contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
		//img_display.init(false);
	    }
	    else {
		if ( 1 < refresh_image || img_display.length() == 0 ) {
		    img_display.resize(ref_img_buf);
		    img_display.clean();
		    img_display.paste(img_buf, offset_x, offset_y, 0);
		}
		display_image(win_image, 0, 0, img_display, tiff_szt,
			      display_bin, display_ch,
			      contrast_rgb, refresh_winsize, &tmp_buf);
		winname(win_image, "Target  offset = ( %ld, %ld )  "
		      "channel = %s  zoom = %3.2f  contrast = ( %d, %d, %d )  ",
		      offset_x, offset_y, names_ch[display_ch],
		      (double)((display_bin < 0) ? -display_bin : 1.0/display_bin),
		      contrast_rgb[0], contrast_rgb[1], contrast_rgb[2]);
		//img_display.init(false);
	    }
	}

	if ( refresh_winname == true ) {
	    winname(win_image, "N-comp_dark_synth=%d  sigma-clipping: "
	        "[N_iterations=%d,  value=(%d,%d,%d),  sky-level=%d,  comet=%d]  "
		"dither=%d",
		(int)n_comp_dark_synth, count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
		(int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);
	}

	if ( refresh_loupe != 0 || refresh_image != 0 ) {
	    const int tiff_szt = 2;
	    const int gcfnc = GXcopy;
	    const mdarray_float *disp_img_p;
	    if ( display_type == 1 ) disp_img_p = &ref_img_buf; /* reference */
	    else disp_img_p = &img_display;
	    display_loupe( command_win_rec.win_id,
			   0, command_win_rec.reserved_y0,
			   refresh_loupe, *disp_img_p, tiff_szt, ev_x, ev_y,
			   loupe_zoom, contrast_rgb, display_ch,
			   gcfnc, &loupe_buf, 
			   &loupe_x, &loupe_y, &tmp_buf_loupe);
	}
	
	if ( refresh_list == true ) {

	    display_file_list( win_filesel, filenames, sel_file_id, false,
			       ref_file_id, flg_saved.array_ptr() );

	}

    }
    //ggetch();

    return_status = 0;
 quit:
    return return_status;
}
