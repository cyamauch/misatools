#include "file_io.h"

using namespace sli;

/* e.g. ././boo//foo => boo/foo */
int remove_redundancy_in_path( tstring *path_p )
{
    ssize_t ix;
    int ret = -1;

    if ( path_p == NULL ) goto quit;
    
    while ( (*path_p).find("./") == 0 ) {
	(*path_p).erase(0,2);
    }
    while ( 0 < (ix=(*path_p).find("/./")) ) {
	(*path_p).erase(ix,2);
    }
    while ( 0 < (ix=(*path_p).find("//")) ) {
	(*path_p).erase(ix,1);
    }
    
    ret = 0;

 quit:
    return ret;
}

/* 'dirname' command */
int get_dirname( const char *filename, tstring *ret_dir )
{
    ssize_t ix;
    int ret = -1;

    if ( filename == NULL ) goto quit;
    if ( ret_dir == NULL ) goto quit;

    *ret_dir = filename;

    ix = ret_dir->rfind('/');

    if ( 0 <= ix ) {
	ret_dir->resize(ix);
	if ( ret_dir->length() == 0 ) {
	    ret_dir->assign(".");
	}
	else {
	    remove_redundancy_in_path(ret_dir);
	}
    }
    else {
	ret_dir->assign(".");
    }

    ret = 0;

 quit:
    return ret;
}

