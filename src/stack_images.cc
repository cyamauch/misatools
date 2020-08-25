/*
 * $ s++ stack_images.cc -leggx -lX11
 */
#include <sli/stdstreamio.h>
#include <sli/pipestreamio.h>
#include <sli/tstring.h>
#include <sli/tarray_tstring.h>
#include <sli/mdarray.h>
#include <sli/mdarray_statistics.h>
#include <eggx.h>
#include <unistd.h>
using namespace sli;

const double Contrast_scale = 2.0;

#include "read_tiff24or48_to_float.h"
#include "write_float_to_tiff48.h"
#include "display_image.cc"
#include "load_display_params.cc"
#include "save_display_params.cc"
#include "load_sigclip_params.cc"
#include "save_sigclip_params.cc"
#include "make_output_filename.cc"
#include "icc_srgb_profile.c"

/**
 * @file   stack_images.cc
 * @brief  interactive xy matching and stacking tool
 *         8/16-bit images are supported.
 */

const char *Refframe_conffile = "refframe.txt";

const int Font_y_off = 3;
const int Font_margin = 2;
static int Fontsize = 14;
#include "set_fontsize.c"

static int display_file_list( int win_filesel,
			      const tarray_tstring &filenames,
			      const mdarray_bool &flg_saved,
			      int ref_file_id,
			      int sel_file_id )
{
    size_t i;

    gclr(win_filesel);
    
    /* displaying file list */
    newcolor(win_filesel, "white");
    
    for ( i=0 ; i < filenames.length() ; i++ ) {
        bool changed_color = false;
	if ( 0 <= ref_file_id && i == (size_t)ref_file_id ) {
	    newcolor(win_filesel, "red");
	    changed_color = true;
	    drawstr(win_filesel,
		    (Fontsize/2)*2, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		    "REF");
	}
	else if ( 0 <= sel_file_id && i == (size_t)sel_file_id ) {
	    newcolor(win_filesel, "green");
	    changed_color = true;
	}

	if ( flg_saved[i] == true ) {
	    drawstr(win_filesel,
		    (Fontsize/2)*2, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		    "SAVED");
	}
	
        drawstr(win_filesel,
		(Fontsize/2)*8, Fontsize*(i+1) - Font_y_off, Fontsize, 0,
		"%s",filenames[i].cstr());

        if ( changed_color == true ) {
	    newcolor(win_filesel, "white");
	}
    }

    return 0;
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


static int do_stack_and_save( const tarray_tstring &filenames,
			      int ref_file_id, const mdarray_bool &flg_saved,
			      int count_sigma_clip, const int sigma_rgb[], 
			      bool skylv_sigma_clip, bool comet_sigma_clip, 
			      bool flag_dither,
			      bool flag_preview, const int contrast_rgb[],
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

    sio.printf("Sigma-Clipping: [count=%d,  value=( %d, %d, %d ),  sky-level=%d,  comet=%d]  "
    	       "Dither=%d\n",
	       count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
	       (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);

    /* load reference ... needs for ICC data */
    sio.printf("Stacking [%s]\n", filenames[ref_file_id].cstr());
    if ( read_tiff24or48_to_float(filenames[ref_file_id].cstr(),
				  &img_buf, &icc_buf, NULL, NULL) < 0 ) {
	sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
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
    }

    /* paste 1st image */
    stacked_buf1_sum.paste(img_buf);
    stacked_buf1_sum2.paste(img_buf * img_buf);
    
    winname(win_image, "Stacking ...");
    if ( flag_preview == true ) {
        display_image(win_image, img_buf, contrast_rgb, tmp_buf);
    }

    n_plus = 0;
    for ( i=0 ; i < filenames.length() ; i++ ) {
        if ( flg_saved[i] == true ) n_plus ++;
    }
    
    ii = 2;
    for ( i=0 ; i < filenames.length() ; i++ ) {
        if ( flg_saved[i] == true ) {
	    sio.printf("Stacking [%s]\n", filenames[i].cstr());
	    if ( read_tiff24or48_to_float(filenames[i].cstr(),
					  &img_buf, NULL, NULL, NULL) < 0 ) {
		sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	    }
	    else {
		//double max_val;
		long offset_x = 0, offset_y = 0;
		if ( read_offset_file(filenames, i,
				      &offset_x, &offset_y) < 0 ) {
		    sio.eprintf("[ERROR] read_offset_file() failed.\n");
		}

		stacked_buf1_sum.add(img_buf, offset_x, offset_y, 0); /* STACK! */
		stacked_buf1_sum2.add(img_buf * img_buf, offset_x, offset_y, 0); /* STACK pow() */

		if ( flag_preview == true || ii == 1 + n_plus ) {
		    //max_val = md_max(stacked_buf1_sum);
		    //img_buf.paste(stacked_buf1_sum * (65535.0 / max_val));
		    img_buf.paste(stacked_buf1_sum);
		    img_buf *= (1.0/(double)ii);
		    /* display stacked image */
		    display_image(win_image, img_buf, contrast_rgb, tmp_buf);
		}

		winname(win_image, "Stacking %zd/%zd", ii, (size_t)(1+n_plus));

		ii ++;
	    }
	}
    }
    count_buf1 = (short)(1+n_plus);

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

	sio.printf("*** Sigma-Clipping loop-count = %d / %d ***\n", cnt + 1, count_sigma_clip);

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
	    img_tmp_buf = (*stacked_buf1_sum_ptr) / (*count_buf1_ptr);
	    av_median[0] = md_median( img_tmp_buf.sectionf("*,*,0") );
	    av_median[1] = md_median( img_tmp_buf.sectionf("*,*,1") );
	    av_median[2] = md_median( img_tmp_buf.sectionf("*,*,2") );
	    sio.printf("Median of averaged image = (%g, %g, %g)\n",
		       av_median[0], av_median[1], av_median[2]);
	}

	/* clear buffer for new result */
	(*stacked_buf0_sum_ptr) = 0.0;
	(*stacked_buf0_sum2_ptr) = 0.0;
	(*count_buf0_ptr) = 0;

	/*
	 *  main of sigma-clipping
	 */
	ii = 1;
	for ( i=0 ; i < filenames.length() ; i++ ) {
	    if ( (int)i == ref_file_id || flg_saved[i] == true ) {
		sio.printf("Stacking with Sigma-Clipping [%s]\n", filenames[i].cstr());
		if ( read_tiff24or48_to_float(filenames[i].cstr(),
					      &img_buf, NULL, NULL, NULL) < 0 ) {
		    sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		}
	        else {
		    unsigned char *p5 = flag_buf.array_ptr();
		    size_t j;
		    double target_median[3] = {1.0, 1.0, 1.0};
		    //double max_val;
		    long offset_x = 0, offset_y = 0;

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

		    img_tmp_buf = 0.0;
		    img_tmp_buf.add(img_buf, offset_x, offset_y, 0);
		    size_t len_xy = img_tmp_buf.x_length() * img_tmp_buf.y_length();

		    flag_buf = 0;

		    if ( comet_sigma_clip == true && final_loop == true && (int)i == ref_file_id ) {
			/* Stack a reference image without sigma-clipping (when last loop) */
			(*count_buf0_ptr) += (short)1;
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

		    stacked_buf0_sum_ptr->add(img_tmp_buf);		/* STACK! */
		    stacked_buf0_sum2_ptr->add(img_tmp_buf * img_tmp_buf);

		    if ( flag_preview == true || ii == 1 + n_plus ) {
			//max_val = md_max(stacked_buf0_sum);
			//img_buf.paste(stacked_buf0_sum * (65535.0 / max_val));
			img_buf.paste((*stacked_buf0_sum_ptr));
			img_buf /= (*count_buf0_ptr);
			/* display stacked image */
			display_image(win_image, img_buf, contrast_rgb, tmp_buf);
		    }

		    winname(win_image, "Stacking with Sigma-Clipping %zd/%zd", ii, (size_t)(1+n_plus));

		    ii ++;
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
    img_tmp_buf.init(false);
    flag_buf.init(false);
    stacked_buf0_sum2.init(false);
    count_buf0.init(false);
    stacked_buf1_sum2.init(false);
    count_buf1.init(false);
    img_buf.init(false);

    /* display */
    winname(win_image, "Done Stacking %zd frames", (size_t)(1+n_plus));
    appended_str.printf("+%zdframes_stacked", n_plus);
    make_output_filename(filenames[ref_file_id].cstr(), appended_str.cstr(),
			 "16bit", &out_filename);
    //out_filename.printf("%s+%zdframes_stacked_16bit.tiff",
    //			filenames[ref_file_id].cstr(), n_plus);

    sio.printf("Writing '%s' ", out_filename.cstr());
    if ( flag_dither == true ) sio.printf("using dither ...\n");
    else sio.printf("NOT using dither ...\n");
    if ( write_float_to_tiff48(*stacked_buf_result_ptr, 0.0, 0.0, flag_dither, 
			       icc_buf, out_filename.cstr()) < 0 ) {
	sio.eprintf("[ERROR] write_float_to_tiff48() failed.\n");
	goto quit;
    }
    
    ret_status = 0;
 quit:
    return ret_status;
}


typedef struct _command_list {
    int id;
    const char *menu_string;
} command_list;

const command_list Cmd_list[] = {
#define CMD_DISPLAY_TARGET 1
        {CMD_DISPLAY_TARGET,    "Display Target           [1]"},
#define CMD_DISPLAY_REFERENCE 2
        {CMD_DISPLAY_REFERENCE, "Display Reference        [2]"},
#define CMD_DISPLAY_RESIDUAL1 3
        {CMD_DISPLAY_RESIDUAL1, "Display Residual x1      [3]"},
#define CMD_DISPLAY_RESIDUAL2 4
        {CMD_DISPLAY_RESIDUAL2, "Display Residual x2      [4]"},
#define CMD_DISPLAY_RESIDUAL4 5
        {CMD_DISPLAY_RESIDUAL4, "Display Residual x4      [5]"},
#define CMD_DISPLAY_RESIDUAL8 6
        {CMD_DISPLAY_RESIDUAL8, "Display Residual x8      [6]"},
#define CMD_CONT_PLUS_R 7
        {CMD_CONT_PLUS_R,       "Red Contrast +           [r]"},
#define CMD_CONT_MINUS_R 8
        {CMD_CONT_MINUS_R,      "Red Contrast -           [R]"},
#define CMD_CONT_PLUS_G 9
        {CMD_CONT_PLUS_G,       "Green Contrast +         [g]"},
#define CMD_CONT_MINUS_G 10
        {CMD_CONT_MINUS_G,      "Green Contrast -         [G]"},
#define CMD_CONT_PLUS_B 11
        {CMD_CONT_PLUS_B,       "Blue Contrast +          [b]"},
#define CMD_CONT_MINUS_B 12
        {CMD_CONT_MINUS_B,      "Blue Contrast -          [B]"},
#define CMD_SAVE 13
        {CMD_SAVE,              "Save Offset Info         [Enter]"},
#define CMD_DELETE 14
        {CMD_DELETE,            "Delete Offset Info       [Del]"},
#define CMD_CLR_OFF 15
        {CMD_CLR_OFF,           "Clear Currnt Offset      [0]"},
#define CMD_SIGCLIP_CNT_PLUS 16
        {CMD_SIGCLIP_CNT_PLUS,  "Count Sigma-Clip +       [C]"},
#define CMD_SIGCLIP_CNT_MINUS 17
        {CMD_SIGCLIP_CNT_MINUS, "Count Sigma-Clip -       [c]"},
#define CMD_SIGCLIP_PLUS 18
        {CMD_SIGCLIP_PLUS,      "Val of Sigma-Clip +      [v]"},
#define CMD_SIGCLIP_MINUS 19
        {CMD_SIGCLIP_MINUS,     "Val of Sigma-Clip -      [V]"},
#define CMD_SIGCLIP_SKYLV 20
        {CMD_SIGCLIP_SKYLV,     "Sky-lv Sigma-Clip on/off [s]"},
#define CMD_SIGCLIP_COMET 21
        {CMD_SIGCLIP_COMET,     "Comet Sigma-Clip on/off  [m]"},
#define CMD_DITHER 22
        {CMD_DITHER,            "Dither on/off for saving [d]"},
#define CMD_STACK 23
        {CMD_STACK,             "Start Stacking"},
#define CMD_STACK_SILENT 24
        {CMD_STACK_SILENT,      "Start Stacking without preview"},
#define CMD_EXIT 25
        {CMD_EXIT,              "Exit                     [q]"}
};

const size_t N_cmd_list = sizeof(Cmd_list) / sizeof(Cmd_list[0]);

int main( int argc, char *argv[] )
{
    int contrast_rgb[3] = {8, 8, 8};

    stdstreamio sio, f_in;
    pipestreamio p_in;
    tstring refframe, filename_frames;
    tarray_tstring filenames;
    mdarray_bool flg_saved(false);
    int ref_file_id = -1;
    int sel_file_id = -1;
    
    int win_command, win_filesel, win_image;
    int win_command_col_height;
    
    mdarray_float ref_img_buf(false); /* buffer for reference image */
    mdarray_float img_buf(false);	/* buffer for target */
    mdarray_uchar tmp_buf(false);	/* tmp buffer for displaying */

    //mdarray_float img_residual(false);	/* buffer for residual image */
    mdarray_float img_display(false);	/* buffer for displaying image */
    int display_type = 1;		/* flag to display image type */
    long offset_x = 0;
    long offset_y = 0;

    int count_sigma_clip = 0;
    bool skylv_sigma_clip = true;
    bool comet_sigma_clip = false;
    int sigma_rgb[3] = {20, 20, 20};

    bool flag_dither = true;

    size_t i;
    int arg_cnt;

    int return_status = -1;

    load_display_params("display_0.txt", contrast_rgb);
    load_sigclip_params("sigclip_0.txt",
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
	    goto quit;
	}
	refframe = f_in.getline();
	refframe.trim();
	f_in.close();
    }

    sio.printf("refframe = [%s]\n", refframe.cstr());

    filename_frames = refframe;
    filename_frames.regreplace("[0-9]+","*",false);

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

    {
	const int w_width = 32 * (Fontsize/2) + Font_margin * 2;
	const int c_height = (Fontsize + Font_margin * 2);
	win_command = gopen(w_width, c_height * (N_cmd_list));
	gsetbgcolor(win_command,"#606060");
	gclr(win_command);
	winname(win_command, "Command Window");
	for ( i=0 ; i < N_cmd_list ; i++ ) {
	    newrgbcolor(win_command,0x80,0x80,0x80);
	    drawline(win_command,
		     0, c_height * (Cmd_list[i].id - 1),
		     w_width, c_height * (Cmd_list[i].id - 1));
	    newrgbcolor(win_command,0x40,0x40,0x40);
	    drawline(win_command,
		     0, c_height * (Cmd_list[i].id) - 1,
		     w_width, c_height * (Cmd_list[i].id) - 1);
	    newrgbcolor(win_command,0xff,0xff,0xff);
	    drawstr(win_command,
		    Font_margin, c_height * Cmd_list[i].id - Font_y_off,
		    Fontsize, 0, Cmd_list[i].menu_string);
	}
	win_command_col_height = c_height;
    }
    
    /* file selector */
    
    win_filesel = gopen((Fontsize/2) * (refframe.length() + 6 + 4),
			Fontsize * (filenames.length()));
    gsetbgcolor(win_filesel,"#404040");
    gclr(win_filesel);
    winname(win_filesel, "File Selector");
    
    display_file_list(win_filesel, filenames, flg_saved, ref_file_id, -1);

    /* image viewer */

    win_image = gopen(600,400);
    winname(win_image, "Imave Viewer  "
	    "Contrast = ( %d, %d, %d )  "
	    "Sigma-Clipping: [count=%d,  value=( %d, %d, %d ),  sky-level=%d,  comet=%d]  "
	    "Dither=%d",
	    contrast_rgb[0], contrast_rgb[1], contrast_rgb[2],
	    count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
	    (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);

    if ( read_tiff24or48_to_float(filenames[ref_file_id].cstr(),
				  &img_buf, NULL, NULL, NULL) < 0 ) {
        sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
	goto quit;
    }

    /* display reference image */
    display_image(win_image, img_buf, contrast_rgb, &tmp_buf);

    ref_img_buf.resize(img_buf);
    ref_img_buf.paste(img_buf);

    
    /*
     * MAIN EVENT LOOP
     */

    while ( 1 ) {

	int ev_win, ev_type, ev_btn;	/* for event handling */
	double ev_x, ev_y;

        bool refresh_list = false;
        bool refresh_image = false;
        bool refresh_winname = false;
	bool save_offset = false;
	bool delete_offset = false;

	int f_id = -1;
	int cmd_id = -1;
	
        /* waiting an event */
        ev_win = eggx_ggetxpress(&ev_type,&ev_btn,&ev_x,&ev_y);

	if ( ev_type == ButtonPress && ev_win == win_command ) {
	    cmd_id = 1 + ev_y / win_command_col_height;
	}

	if ( ev_type == KeyPress ) {
	    //sio.printf("[%d]\n", ev_btn);
	    if ( ev_btn == '1' ) cmd_id = CMD_DISPLAY_TARGET;
	    else if ( ev_btn == '2' ) cmd_id = CMD_DISPLAY_REFERENCE;
	    else if ( ev_btn == '3' ) cmd_id = CMD_DISPLAY_RESIDUAL1;
	    else if ( ev_btn == '4' ) cmd_id = CMD_DISPLAY_RESIDUAL2;
	    else if ( ev_btn == '5' ) cmd_id = CMD_DISPLAY_RESIDUAL4;
	    else if ( ev_btn == '6' ) cmd_id = CMD_DISPLAY_RESIDUAL8;
	    else if ( ev_btn == 'r' ) cmd_id = CMD_CONT_PLUS_R;
	    else if ( ev_btn == 'R' ) cmd_id = CMD_CONT_MINUS_R;
	    else if ( ev_btn == 'g' ) cmd_id = CMD_CONT_PLUS_G;
	    else if ( ev_btn == 'G' ) cmd_id = CMD_CONT_MINUS_G;
	    else if ( ev_btn == 'b' ) cmd_id = CMD_CONT_PLUS_B;
	    else if ( ev_btn == 'B' ) cmd_id = CMD_CONT_MINUS_B;
	    else if ( ev_btn == 'c' ) cmd_id = CMD_SIGCLIP_CNT_PLUS;
	    else if ( ev_btn == 'C' ) cmd_id = CMD_SIGCLIP_CNT_MINUS;
	    else if ( ev_btn == 'v' ) cmd_id = CMD_SIGCLIP_PLUS;
	    else if ( ev_btn == 'V' ) cmd_id = CMD_SIGCLIP_MINUS;
	    else if ( ev_btn == 's' ) cmd_id = CMD_SIGCLIP_SKYLV;
	    else if ( ev_btn == 'm' ) cmd_id = CMD_SIGCLIP_COMET;
	    else if ( ev_btn == 'd' ) cmd_id = CMD_DITHER;
	    else if ( ev_btn == 13 ) cmd_id = CMD_SAVE;
	    else if ( ev_btn == 127 ) cmd_id = CMD_DELETE;
	    else if ( ev_btn == '0' ) cmd_id = CMD_CLR_OFF;
	    /* ESC key or 'q' */
	    else if ( ev_btn == 27 || ev_btn == 'q' ) cmd_id = CMD_EXIT;
	}

	if ( cmd_id == CMD_EXIT ) {
	    break;
	}
	else if ( cmd_id == CMD_STACK || cmd_id == CMD_STACK_SILENT ) {
	    /* preview on/off */
	    bool flag_preview = true;
	    if ( cmd_id == CMD_STACK_SILENT ) flag_preview = false;
	    /* save memory ... */
	    img_display.init(false);
	    img_buf.init(false);
	    if ( do_stack_and_save( filenames, ref_file_id, flg_saved,
				    count_sigma_clip, sigma_rgb, 
				    skylv_sigma_clip, comet_sigma_clip, 
				    flag_dither,
				    flag_preview, contrast_rgb,
				    win_image, &tmp_buf ) < 0 ) {
	        sio.printf("[ERROR] do_stack_and_save() failed\n");
	    }
	    if ( 0 <= sel_file_id ) {
		f_id = sel_file_id;	/* to reload */
	    }
	    else {
		img_buf.resize(ref_img_buf);
		img_buf.paste(ref_img_buf);
	    }
	    //sel_file_id = -1;
	    //refresh_list = true;
	}
	else if ( ev_type == ButtonPress && ev_win == win_filesel ) {
	    f_id = ev_y / Fontsize;
	}
	else if ( ev_type == KeyPress && ev_btn == 6 /* PageDown */ ) {
	    f_id = sel_file_id + 1;
	    if ( f_id == ref_file_id ) f_id ++;
	}
	else if ( ev_type == KeyPress && ev_btn == 2 /* PageUp */ ) {
	    f_id = sel_file_id - 1;
	    if ( f_id == ref_file_id ) f_id --;
	}
	
	if ( f_id != ref_file_id &&
	     0 <= f_id && (size_t)f_id < filenames.length() ) {

	    sio.printf("Open: %s\n", filenames[f_id].cstr());
	    img_display.init(false);	/* save memory ... */
		    
	    if ( read_tiff24or48_to_float(filenames[f_id].cstr(),
					  &img_buf, NULL, NULL, NULL) < 0 ) {
	        sio.eprintf("[ERROR] read_tiff24or48_to_float() failed\n");
		sel_file_id = -1;
	    }
	    else {
		if ( cmd_id == CMD_STACK || cmd_id == CMD_STACK_SILENT ) {
		    /* NOP */
		}
		else {
		    /* OK */
		    //img_display.resize(img_buf);
		    //img_residual.resize(img_buf);
		    sel_file_id = f_id;
		    //offset_x = 0;
		    //offset_y = 0;
		    //flg_saved.dprint();
		    //sio.printf("%ld\n", sel_file_id);
		    if ( flg_saved[sel_file_id] == true ) {

			if ( read_offset_file(filenames, sel_file_id,
					      &offset_x, &offset_y) < 0 ) {
			    sio.eprintf("[ERROR] read_offset_file() failed.\n");
			}

		    }
		    if ( display_type == 1 ) display_type = 0;
		    refresh_image = true;
		    refresh_list = true;
		}
	    }
	}

	else {

	    if ( cmd_id == CMD_CONT_PLUS_R ) {
		contrast_rgb[0] ++;
		save_display_params("display_0.txt", contrast_rgb);
		refresh_image = true;
	    }
	    else if ( cmd_id == CMD_CONT_MINUS_R ) {
		if ( 0 < contrast_rgb[0] ) {
		    contrast_rgb[0] --;
		    save_display_params("display_0.txt", contrast_rgb);
		    refresh_image = true;
		}
	    }
	    else if ( cmd_id == CMD_CONT_PLUS_G ) {
		contrast_rgb[1] ++;
		save_display_params("display_0.txt", contrast_rgb);
		refresh_image = true;
	    }
	    else if ( cmd_id == CMD_CONT_MINUS_G ) {
		if ( 0 < contrast_rgb[1] ) {
		    contrast_rgb[1] --;
		    save_display_params("display_0.txt", contrast_rgb);
		    refresh_image = true;
		}
	    }
	    else if ( cmd_id == CMD_CONT_PLUS_B ) {
		contrast_rgb[2] ++;
		save_display_params("display_0.txt", contrast_rgb);
		refresh_image = true;
	    }
	    else if ( cmd_id == CMD_CONT_MINUS_B ) {
		if ( 0 < contrast_rgb[2] ) {
		    contrast_rgb[2] --;
		    save_display_params("display_0.txt", contrast_rgb);
		    refresh_image = true;
		}
	    }
	    else if ( cmd_id == CMD_SIGCLIP_CNT_PLUS ) {
		count_sigma_clip ++;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_SIGCLIP_CNT_MINUS ) {
		if ( 0 < count_sigma_clip ) count_sigma_clip --;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_SIGCLIP_PLUS ) {
		sigma_rgb[0] ++;
		sigma_rgb[1] ++;
		sigma_rgb[2] ++;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_SIGCLIP_MINUS ) {
		if ( 0 < sigma_rgb[0] ) sigma_rgb[0] --;
		if ( 0 < sigma_rgb[1] ) sigma_rgb[1] --;
		if ( 0 < sigma_rgb[2] ) sigma_rgb[2] --;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_SIGCLIP_SKYLV ) {
		if ( skylv_sigma_clip == true ) skylv_sigma_clip = false;
		else skylv_sigma_clip = true;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_SIGCLIP_COMET ) {
		if ( comet_sigma_clip == true ) comet_sigma_clip = false;
		else comet_sigma_clip = true;
		save_sigclip_params("sigclip_0.txt", count_sigma_clip, sigma_rgb, skylv_sigma_clip, comet_sigma_clip);
		refresh_winname = true;
	    }
	    else if ( cmd_id == CMD_DITHER ) {
		if ( flag_dither == true ) flag_dither = false;
		else flag_dither = true;
		refresh_winname = true;
	    }

	    if ( 0 <= sel_file_id ) {
	  
		if ( CMD_DISPLAY_TARGET <= cmd_id &&
		     cmd_id <= CMD_DISPLAY_RESIDUAL8 ) {
		    display_type = cmd_id - CMD_DISPLAY_TARGET;
		    refresh_image = true;
		}
		else if ( cmd_id == CMD_SAVE ) {
		    save_offset = true;
		    refresh_list = true;
		}
		else if ( cmd_id == CMD_DELETE ) {
		    delete_offset = true;
		    refresh_list = true;
		}
		else if ( cmd_id == CMD_CLR_OFF ) {
		    offset_x = 0;
		    offset_y = 0;
		    refresh_image = true;
		}
		else if ( ev_type == ButtonPress ) {
		    if ( ev_win == win_image && ev_btn == 3 /* right btn */ ) {
			offset_x = ev_x - (img_buf.x_length() / 2);
			offset_y = ev_y - (img_buf.y_length() / 2);
			refresh_image = true;
		    }
		}
		else if ( ev_type == KeyPress ) {
		    //sio.printf("key = %d\n", ev_btn);
		    if ( ev_btn == 28 ) {		/* Right key */
			offset_x ++;
			refresh_image = true;
		    }
		    else if ( ev_btn == 29 ) {	/* Left key */
			offset_x --;
			refresh_image = true;
		    }
		    else if ( ev_btn == 30 ) {	/* Up key */
			offset_y --;
			refresh_image = true;
		    }
		    else if ( ev_btn == 31 ) {	/* Down key */
			offset_y ++;
			refresh_image = true;
		    }
		    else if ( ev_btn == ' ' ) {
			if ( display_type == 0 ) display_type = 1;
			else display_type = 0;
			refresh_image = true;
		    }
		}
	    }
	}

	if ( delete_offset == true || save_offset == true ) {
	  
	    stdstreamio f_out;
	    tstring offset_file;
	    get_offset_filename(filenames[sel_file_id].cstr(),
				&offset_file);
	    //sio.printf("[%s]\n", offset_file.cstr());

	    if ( delete_offset == true ) {
	        unlink(offset_file.cstr());
		flg_saved[sel_file_id] = false;
		sio.printf("Deleted: [%s]\n", offset_file.cstr());
	    }

	    if ( save_offset == true ) {
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

	}

	/* Update window */
	    
	if ( refresh_image == true ) {
	    if ( 2 <= display_type ) {	/* Residual */
	        //double res_total;
		img_display.resize(ref_img_buf);
		img_display.paste(ref_img_buf);
		img_display.subtract(img_buf, offset_x, offset_y, 0);
		img_display.abs();
		if ( display_type == 3 ) img_display *= 2.0;
		else if ( display_type == 4 ) img_display *= 4.0;
		else if ( display_type == 5 ) img_display *= 8.0;
		//res_total = md_total(img_residual);
		//img_display.swap(img_residual);
		//img_residual.init(false);
		display_image(win_image, img_display, contrast_rgb, &tmp_buf);
		winname(win_image, "Residual offset=(%ld,%ld)",
			offset_x, offset_y);
		//img_display.init(false);
	    }
	    else if ( display_type == 1 ) {	/* Reference */
		img_display.resize(ref_img_buf);
	        img_display.paste(ref_img_buf);
		display_image(win_image, img_display, contrast_rgb, &tmp_buf);
		winname(win_image, "Reference");
		//img_display.init(false);
	    }
	    else {
		img_display.resize(img_buf);
	        img_display.paste(img_buf, offset_x, offset_y, 0);
		display_image(win_image, img_display, contrast_rgb, &tmp_buf);
		winname(win_image, "Target offset=(%ld,%ld)",
			offset_x, offset_y);
		//img_display.init(false);
	    }
	}

	if ( refresh_winname == true ) {
	    winname(win_image, "Sigma-Clipping: [count=%d,  value=( %d, %d, %d ),  sky-level=%d,  comet=%d]  "
	    	    "Dither=%d",
		    count_sigma_clip, sigma_rgb[0], sigma_rgb[1], sigma_rgb[2],
		    (int)skylv_sigma_clip, (int)comet_sigma_clip, (int)flag_dither);
	}

	if ( refresh_list == true ) {

	    display_file_list(win_filesel, filenames, flg_saved,
			      ref_file_id, sel_file_id);

	}

    }
    //ggetch();

    return_status = 0;
 quit:
    return return_status;
}

#include "read_tiff24or48_to_float.cc"
#include "write_float_to_tiff48.cc"
