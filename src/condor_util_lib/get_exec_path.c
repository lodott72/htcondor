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

/*

  This file defines getExecPath(), which tries to return the full path
  to the file that the current process is executing.  On platforms
  where this information cannot be reliably determined, it returns
  NULL.  On platforms where it is possible to collect this
  information, the full path is stored in a string allocated with
  malloc() that must be free()'ed when the caller is done with it.
*/


/*
  Each platform that supports this functionality uses a completely
  different method for finding it out, so they are implemented in
  seperate helper functions...
*/

#if defined( LINUX )

#include "condor_common.h" 
#include "condor_debug.h" 

char*
linux_getExecPath()
{
	int rval;
	char* full_path;
	char path_buf[MAXPATHLEN];
	rval = readlink( "/proc/self/exe", path_buf, MAXPATHLEN );
	if( rval < 0 ) {
		dprintf( D_ALWAYS,"getExecPath: "
				 "readlink(\"/proc/self/exe\") failed: errno %d (%s)\n",
				 errno, strerror(errno) );
		return NULL;
	}
	if( rval == MAXPATHLEN ) {
		dprintf( D_ALWAYS,"getExecPath: "
				 "unable to find full path from /proc/self/exe\n" );
		return NULL;
	}
		/* Oddly, readlink doesn't terminate the string for you, so
		   we've got to handle that ourselves... */
	path_buf[rval] = '\0';
	full_path = strdup( path_buf );
	return full_path;
}
#endif /* defined(LINUX) */


#if defined( Solaris )

#include "condor_common.h" 
#include "condor_debug.h" 

char*
solaris_getExecPath()
{
	const char* name;
	char* cwd = NULL;
	char* full_path = NULL;
	int size;

	name = getexecname();
	if( name[0] == '/' ) {
		return strdup(name);
	}
		/*
		  if we're still here, we didn't get the full path, and the
		  man pages say we can prepend the output of getcwd() to find
		  the real value...
		*/ 
	cwd = getcwd( NULL, MAXPATHLEN );
	size = strlen(cwd) + strlen(name) + 2;
	full_path = malloc(size);
	snprintf( full_path, size, "%s/%s", cwd, name );
	free( cwd );
	cwd = NULL;
	return full_path;
}
#endif /* defined(Solaris) */


#if defined( WIN32 )

#include "condor_common.h" 
#include "condor_debug.h" 
#include <psapi.h>

char*
win32_getExecPath()
{
	char buf[MAX_PATH];
	HANDLE hProc;
	DWORD result;
	HMODULE hMod;
	DWORD cbNeeded;

	hProc = GetCurrentProcess();

	if ( ! EnumProcessModules( hProc, &hMod, sizeof(hMod), 
		&cbNeeded) )
	{
				
		dprintf( D_ALWAYS,"getExecPath: EnumProcessModules()  failed "
			"(err=%li)\n", GetLastError() );
		
		CloseHandle(hProc);
		return NULL;
	}
		
	result = GetModuleFileNameEx(
		hProc,     /* process handle */
		hMod,      /* handle to module we want to know about */
		buf,       /* buffer to receive name */
		MAX_PATH   /* size of buffer needed */
		);
	
	CloseHandle(hProc);
	
	if ( result == 0 ) {
		
		dprintf( D_ALWAYS,"getExecPath: "
			"unable to find full path from GetModuleFileNameEx() "
			"(err=%li)\n", GetLastError() );
		
		return NULL;
		
	} else {
		return strdup(buf);
	}
}
#endif /* defined(WIN32) */


#if defined( Darwin )

/* We need to include <mach-o/dyld.h> for some prototypes, but
   unfortunately, that header conflicts with our definitions of FALSE
   and TRUE. :( So, we hack around that by defining a special thing
   that tells our condor_constants.h header not to mess with the
   definition of FALSE and TRUE.  It doesn't effect anything in the
   rest of this file, and it's important to get the right stuff from
   the system headers for what we're doing here for OS X.
   Derek Wright <wright@cs.wisc.edu> 2004-05-13
*/

#include <mach-o/dyld.h>
#define _CONDOR_NO_TRUE_FALSE 1
#include "condor_common.h" 
#include "condor_debug.h" 
#include <crt_externs.h>

typedef int (*NSGetExecutablePathProcPtr)(char *buf, size_t *bufsize);

int
darwin_NSGetExecutablePath_getExecPath( char* buf, size_t* bufsize )
{
	return ((NSGetExecutablePathProcPtr)NSAddressOfSymbol(NSLookupAndBindSymbol("__NSGetExecutablePath")))(buf, bufsize);
}

int
darwin_NSGetArgv_getExecPath( char* buf, size_t* bufsize )
{
	int len = MAXPATHLEN;
	int argc = 0;
    char *path;
    char **ptr;

		/* _NSGetArgv() returns a pointer to the argv array.  After
		   all the args, there should be a NULL in the table.  Next
		   comes the environment, and another NULL.  After that, the
		   next entry will be the value of the path given to exec().
		*/
	ptr = (char**)(*(_NSGetArgv()));
		/* Get the value of our original argc. */
	argc = *(_NSGetArgc());
		/* skip over all the args: */
	ptr += argc;
		/* ptr should be pointing at the NULL terminating the args. */
	if( *ptr != NULL ) {
		return -1;
	} else {
			/* skip the NULL */
		ptr += 1;
	}
		/* now, skip the environment, searching for the next NULL. */
	while( *ptr != NULL ) {
		ptr += 1;
	}
		/* ptr should be pointing at the NULL terminating the env. */
	if( *ptr != NULL ) {
		return -1;
	} else { 
			/* skip the NULL */
		ptr += 1;
	}
		/* Now we're finally pointing at the thing we want */
	path = *ptr;
	len = strlen( path ) + 1;
	if( len > *bufsize ) {
		return -1;
	}
	strncpy( buf, path, len );
	*bufsize = len;
	return len;
}


char*
darwin_getExecPath()
{
	char buf[MAXPATHLEN];
	char full_buf[MAXPATHLEN];
	char* path = NULL;
	size_t size = MAXPATHLEN;
	int rval = 0;

    memset( buf, '\0', MAXPATHLEN );
    memset( full_buf, '\0', MAXPATHLEN );

		// Figure out dynamically if we can use the special method or
		// if we have to do it ourselves...
	if( NSIsSymbolNameDefined("__NSGetExecutablePath") ) {
			// We've got the handy function that does all this work
			// for us, so use it.
		rval = darwin_NSGetExecutablePath_getExecPath( buf, &size );
	    if( rval < 0 ) {
			dprintf( D_ALWAYS, "getExecName(): NSGetExecutablePath() "
					 "returned failure (%d): %s (errno %d)\n", 
					 rval, strerror(errno), errno );
			return NULL;
	    }
	} else {
			// No handy function that does it for us, so instead we
			// use a slightly more convoluted method...
		rval = darwin_NSGetArgv_getExecPath( buf, &size );
		if( rval < 0 ) {		
			dprintf( D_ALWAYS, "getExecName(): NSGetArgv() "
					 "returned failure (%d): %s (errno %d)\n", 
					 rval, strerror(errno), errno );
			return NULL;
	    }
	}
		/* 
		   Either way, the resulting path might still have some
		   relative paths in it.  So, convert to a canonicalized path,
		   resolving '/./' and '/../'
		*/
	path = realpath( buf, full_buf );
	if( path ) {
		path = strdup( full_buf );
		return path;
	}
	dprintf( D_ALWAYS,
			 "getExecName(): realpath() returned failure on \"%s\": "
			 "%s (errno %d)\n", full_buf, strerror(errno), errno );
	return NULL;
}
#endif /* defined(Darwin) */

#include "condor_common.h" 

/*
  Now, the public method that just invokes the right platform-specific
  helper (if there is one)
*/
char*
getExecPath( void )
{
	char* rval = NULL;
#if defined( LINUX )
	rval = linux_getExecPath();
#elif defined( Solaris )
	rval= solaris_getExecPath();
#elif defined( Darwin )
	rval = darwin_getExecPath();
#elif defined( WIN32 )
	rval = win32_getExecPath();
#endif
	return rval;
}

