#ifndef KCP_UTIL_H_
#define KCP_UTIL_H_
#include <unistd.h>
#include <sys/time.h>
#include <iostream>
/* get system time */
static inline void itimeofday(long *sec, long *usec)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
}

/* get clock in millisecond 64 */
static inline int64_t iclock64()
{
	long s, u;
	int64_t value;
	itimeofday(&s, &u);
	value = ((int64_t)s) * 1000 + (u / 1000);
	return value;
}
#endif