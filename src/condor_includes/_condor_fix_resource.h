#ifndef _RESOURCE_H
#define _RESOURCE_H

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(ULTRIX43)
#	include <time.h>
#endif

#if defined(SUNOS41) || defined(HPUX9) || defined(ULTRIX43)
	typedef int rlim_t;
#endif

#include <sys/resource.h>

#if defined(ULTRIX43)
#if defined(__STDC__) || defined(__cplusplus)
	int getrlimit( int resource, struct rlimit *rlp );
	int setrlimit( int resource, const struct rlimit *rlp );
#else
	int getrlimit();
	int setrlimit();
#endif
#endif /* ULTRIX43 */

#if defined(__cplusplus)
}
#endif

#endif /* _RESOURCE_h */
