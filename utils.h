#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include "config.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "error.h"
#include "nmap.h"
#include "global_structures.h"

#ifndef MAX
#define MAX(x,y) (((x)>(y))?(x):(y))
#endif
#ifndef MIN
#define MIN(x,y) (((x)<(y))?(x):(y))
#endif
#ifndef ABS
#define ABS(x) (((x) >= 0)?(x):(-x)) 
#endif
#ifndef MOD_DIFF
#define MOD_DIFF(a,b) ((unsigned long) (MIN((unsigned long)(a) - (unsigned long ) (b), (unsigned long )(b) - (unsigned long) (a))))
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define NIPQUAD(addr) \
        (((addr) >> 0)  & 0xff), \
        (((addr) >> 8)  & 0xff), \
        (((addr) >> 16) & 0xff), \
        (((addr) >> 24) & 0xff)

#define MAX_PARSE_ARGS 254 /* +1 for integrity checking + 1 for null term */

/* Timeval subtraction in microseconds */
#define TIMEVAL_SUBTRACT(a,b) (((a).tv_sec - (b).tv_sec) * 1000000 + (a).tv_usec - (b).tv_usec)
/* Timeval subtract in milliseconds */
#define TIMEVAL_MSEC_SUBTRACT(a,b) ((((a).tv_sec - (b).tv_sec) * 1000) + ((a).tv_usec - (b).tv_usec) / 1000)
/* Timeval subtract in seconds */
#define TIMEVAL_SEC_SUBTRACT(a,b) ((a).tv_sec - (b).tv_sec + ((a).tv_usec - (b).tv_usec + 500)/1000)


void *safe_malloc(int size);
#ifndef HAVE_STRCASESTR
char *strcasestr(char *haystack, char *pneedle);
#endif
void hdump(unsigned char *packet, unsigned int len);
void lamont_hdump(unsigned char *bp, unsigned int length);
int Strncpy(char *dest, const char *src, size_t n);
int get_random_bytes(void *buf, int numbytes);
int get_random_int();
unsigned short get_random_ushort();
unsigned int get_random_uint();
/* Scramble the contents of an array*/
void genfry(unsigned char *arr, int elem_sz, int num_elem);
void shortfry(unsigned short *arr, int num_elem);
/* Like the perl equivialent -- It removes the terminating newline from string
   IF one exists.  It then returns the POSSIBLY MODIFIED string */
char *chomp(char *string);
ssize_t Write(int fd, const void *buf, size_t count);

unsigned long gcd_ulong(unsigned long a, unsigned long b);
unsigned int gcd_uint(unsigned int a, unsigned int b);
unsigned long gcd_n_ulong(long nvals, unsigned long *val);
unsigned int gcd_n_uint(int nvals, unsigned int *val);

int arg_parse(const char *command, char ***argv);
void arg_parse_free(char **argv);

#ifndef HAVE_USLEEP
#ifdef HAVE_NANOSLEEP
void usleep(unsigned long usec);
#endif
#endif

#ifndef HAVE_STRERROR
char *strerror(int errnum);
#endif


/* mmap() an entire file into the address space.  Returns a pointer
   to the beginning of the file.  The mmap'ed length is returned
   inside the length parameter.  If there is a problem, NULL is
   returned, the value of length is undefined, and errno is set to
   something appropriate.  The user is responsible for doing
   an munmap(ptr, length) when finished with it.  openflags should 
   be O_RDONLY or O_RDWR, or O_WRONLY
*/
char *mmapfile(char *fname, int *length, int openflags);


#endif
