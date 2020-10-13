#ifndef _FILE_IO_H
#define _FILE_IO_H 1

#include <sli/tstring.h>

int remove_redundancy_in_path( sli::tstring *path_p );

int get_dirname( const char *filename, sli::tstring *ret_dir );


#endif	/* _FILE_IO_H */
