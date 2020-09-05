
static int write_float_to_tiff48( const mdarray_float &img_buf_in,
				  const mdarray_uchar &icc_buf_in,
				  const float camera_calibration1[],
				  double min_val, double max_val,
				  bool dither,
				  const char *filename_out );
