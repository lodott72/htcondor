/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#include "condor_common.h"
#include "condor_getmnt.h"

char			*strdup(), *malloc();

/*
 The function getmnt() is Ultrix specific and returns a different
 structure than the similar functions available on other UNIX
 variants.  We simulate "getmnt()" by calling the appropriate platform
 specific library routines.  N.B. we simulate getmnt() only to the
 extent needed specifically by condor.  This is NOT a generally correct
 simulation.
*/

#if defined(ULTRIX42) || defined(ULTRIX43) 

	/* Nothing needed on ULTRIX systems - getmnt() is native*/

#elif defined(OSF1) || defined(Darwin)

	/* BEGIN OSF1 version - use getmntinfo() */

#include <sys/stat.h>
#include <sys/mount.h>


int
getmnt( start, buf, bufsize, mode, path )
int				*start;
struct fs_data	buf[];
unsigned		bufsize;
int				mode;
char			*path;
{
	struct statfs	*data = NULL;
	struct stat	st_buf;
	int		n_entries;
	int		i;

	if( (n_entries=getmntinfo(&data,MNT_NOWAIT)) < 0 ) {
		perror( "getmntinfo" );
		exit( 1 );
	}

	for( i=0; i<n_entries; i++ ) {
		if( stat(data[i].f_mntonname,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = data[i].f_mntonname;
		buf[i].fd_req.path = data[i].f_mntfromname;
	}
	return n_entries;
}

	/* END OSF1 version */

#elif defined(AIX32)
FILE			*setmntent();
struct mntent	*getmntent();
char			*strdup();

getmnt( start, buf, bufsize, mode, path )
int				*start;
struct fs_data	buf[];
unsigned		bufsize;
int				mode;
char			*path;
{
	int				status;
	int				mount_table_size = 1024;
	struct vmount   *ptr;
	struct stat		st_buf;
	int				i;
	int				lim;

	ptr = (struct vmount *)malloc( mount_table_size );
	status = mntctl( MCTL_QUERY, mount_table_size, ptr );

		/* If the mount table we have supplied is too small, then mntctl
		   will return the required size in the first word and return 0.
		*/
	if( status == 0 ) {
		mount_table_size = *(int *)ptr;
		free( ptr );
		ptr = (struct vmount *)malloc( mount_table_size );
		status = mntctl( MCTL_QUERY, mount_table_size, ptr );
	}

	if( status < 0 ) {
		perror( "mntctl" );
		exit( 1 );
	}

		/* "lim" should be the number of entries we can store in the buffer
		   supplied by the calling routine, or the number of entries
		   in the real mount table - whichever is smaller */
	lim = bufsize / sizeof(struct fs_data);
	if( status < lim ) {
		lim = status;
	}

	for( i=0; i < lim; i++ ) {
		add( ptr, &buf[i] );
		ptr = (struct vmount *) ( (char *)ptr + ptr->vmt_length );
	}

	return status;
}


#define OBJECT vmt2dataptr(vm,VMT_OBJECT)
#define STUB vmt2dataptr(vm,VMT_STUB)
#define HOSTNAME vmt2dataptr(vm,VMT_HOSTNAME)
add( vm, ent )
struct vmount	*vm;
struct fs_data	*ent;
{
	char	buf[1024];
	struct stat	st_buf;


	if( vm->vmt_gfstype == MNT_NFS ) {
		sprintf( buf, "%s:%s", HOSTNAME, OBJECT );
	} else {
		strcpy( buf, OBJECT );
	}

	if( stat(STUB,&st_buf) < 0 ) {
		ent->fd_req.dev = 0;
	} else {
		if( vm->vmt_gfstype == MNT_NFS ) {
			ent->fd_req.dev = st_buf.st_vfs;
		} else {
			ent->fd_req.dev = st_buf.st_dev;
		}
	}
	ent->fd_req.devname = strdup(buf);
	ent->fd_req.path = strdup( STUB );
}

#elif defined(Solaris)
	/* Solaris specific change ..dhaval
	6/26 */

/*FILE			*setmntent();*/

getmnt( start, buf, bufsize, mode, path )
int				*start;
struct fs_data	buf[];
unsigned		bufsize;
int				mode;
char			*path;
{
	FILE			*tab;
	int			check;
	struct mnttab		*ent;	
	struct stat		st_buf;
	int				i;
	int				lim;

/*	if( (tab=setmntent("/etc/mnttab","r")) == NULL ) {
		perror( "setmntent" );
		exit( 1 );
	} */

	if( (tab=fopen("/etc/mnttab","r")) == NULL ) {
		perror( "setmntent" );
		exit( 1 );
	} 

	lim = bufsize / sizeof(struct fs_data);
	for( i=0; (i < lim) && (check=getmntent(tab,ent)); i++ ) {
		if( stat(ent->mnt_mountp,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = strdup(ent->mnt_special);
		buf[i].fd_req.path = strdup(ent->mnt_mountp); 
	}
	return i;
}


#else

	/* BEGIN !OSF1, !ULTRIX, !AIX  version - use setmntent() and getmntent()  */

FILE			*setmntent();
struct mntent	*getmntent();

int
getmnt( int* start, struct fs_data buf[], unsigned int bufsize, 
		int	mode, char* path )
{
	FILE			*tab;
	struct mntent	*ent;
	struct stat		st_buf;
	int				i;
	int				lim;

	if( (tab=setmntent("/etc/mtab","r")) == NULL ) {
		perror( "setmntent" );
		exit( 1 );
	}

	lim = bufsize / sizeof(struct fs_data);
	for( i=0; (i < lim) && (ent=getmntent(tab)); i++ ) {
		if( stat(ent->mnt_dir,&st_buf) < 0 ) {
			buf[i].fd_req.dev = 0;
		} else {
			buf[i].fd_req.dev = st_buf.st_dev;
		}
		buf[i].fd_req.devname = strdup(ent->mnt_fsname);
		buf[i].fd_req.path = strdup(ent->mnt_dir); 
	}
	return i;
}

	/* END !OSF1 and !ULTRIX version */

#endif
