
/***************************************************************************
 * utils.cc -- Various miscellaneous utility functions which defy          *
 * categorization :)                                                       *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *                                                                         *
 * The Nmap Security Scanner is (C) 1996-2004 Insecure.Com LLC. Nmap       *
 * is also a registered trademark of Insecure.Com LLC.  This program is    *
 * free software; you may redistribute and/or modify it under the          *
 * terms of the GNU General Public License as published by the Free        *
 * Software Foundation; Version 2.  This guarantees your right to use,     *
 * modify, and redistribute this software under certain conditions.  If    *
 * you wish to embed Nmap technology into proprietary software, we may be  *
 * willing to sell alternative licenses (contact sales@insecure.com).      *
 * Many security scanner vendors already license Nmap technology such as  *
 * our remote OS fingerprinting database and code, service/version         *
 * detection system, and port scanning code.                               *
 *                                                                         *
 * Note that the GPL places important restrictions on "derived works", yet *
 * it does not provide a detailed definition of that term.  To avoid       *
 * misunderstandings, we consider an application to constitute a           *
 * "derivative work" for the purpose of this license if it does any of the *
 * following:                                                              *
 * o Integrates source code from Nmap                                      *
 * o Reads or includes Nmap copyrighted data files, such as                *
 *   nmap-os-fingerprints or nmap-service-probes.                          *
 * o Executes Nmap and parses the results (as opposed to typical shell or  *
 *   execution-menu apps, which simply display raw Nmap output and so are  *
 *   not derivative works.)                                                * 
 * o Integrates/includes/aggregates Nmap into a proprietary executable     *
 *   installer, such as those produced by InstallShield.                   *
 * o Links to a library or executes a program that does any of the above   *
 *                                                                         *
 * The term "Nmap" should be taken to also include any portions or derived *
 * works of Nmap.  This list is not exclusive, but is just meant to        *
 * clarify our interpretation of derived works with some common examples.  *
 * These restrictions only apply when you actually redistribute Nmap.  For *
 * example, nothing stops you from writing and selling a proprietary       *
 * front-end to Nmap.  Just distribute it by itself, and point people to   *
 * http://www.insecure.org/nmap/ to download Nmap.                         *
 *                                                                         *
 * We don't consider these to be added restrictions on top of the GPL, but *
 * just a clarification of how we interpret "derived works" as it applies  *
 * to our GPL-licensed Nmap product.  This is similar to the way Linus     *
 * Torvalds has announced his interpretation of how "derived works"        *
 * applies to Linux kernel modules.  Our interpretation refers only to     *
 * Nmap - we don't speak for any other GPL products.                       *
 *                                                                         *
 * If you have any questions about the GPL licensing restrictions on using *
 * Nmap in non-GPL works, we would be happy to help.  As mentioned above,  *
 * we also offer alternative license to integrate Nmap into proprietary    *
 * applications and appliances.  These contracts have been sold to many    *
 * security vendors, and generally include a perpetual license as well as  *
 * providing for priority support and updates as well as helping to fund   *
 * the continued development of Nmap technology.  Please email             *
 * sales@insecure.com for further information.                             *
 *                                                                         *
 * As a special exception to the GPL terms, Insecure.Com LLC grants        *
 * permission to link the code of this program with any version of the     *
 * OpenSSL library which is distributed under a license identical to that  *
 * listed in the included Copying.OpenSSL file, and distribute linked      *
 * combinations including the two. You must obey the GNU GPL in all        *
 * respects for all of the code used other than OpenSSL.  If you modify    *
 * this file, you may extend this exception to your version of the file,   *
 * but you are not obligated to do so.                                     *
 *                                                                         *
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the terms above, then that            *
 * alternative license agreement takes precedence over these comments.     *
 *                                                                         *
 * Source is provided to this software because we believe users have a     *
 * right to know exactly what a program is going to do before they run it. *
 * This also allows you to audit the software for security holes (none     *
 * have been found so far).                                                *
 *                                                                         *
 * Source code also allows you to port Nmap to new platforms, fix bugs,    *
 * and add new features.  You are highly encouraged to send your changes   *
 * to fyodor@insecure.org for possible incorporation into the main         *
 * distribution.  By sending these changes to Fyodor or one the            *
 * Insecure.Org development mailing lists, it is assumed that you are      *
 * offering Fyodor and Insecure.Com LLC the unlimited, non-exclusive right *
 * to reuse, modify, and relicense the code.  Nmap will always be          *
 * available Open Source, but this is important because the inability to   *
 * relicense code has caused devastating problems for other Free Software  *
 * projects (such as KDE and NASM).  We also occasionally relicense the    *
 * code to third parties as discussed above.  If you wish to specify       *
 * special license conditions of your contributions, just say so when you  *
 * send them.                                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * General Public License for more details at                              *
 * http://www.gnu.org/copyleft/gpl.html , or in the COPYING file included  *
 * with Nmap.                                                              *
 *                                                                         *
 ***************************************************************************/

/* $Id$ */

#include "utils.h"
#include "NmapOps.h"

extern NmapOps o;

/* Hex dump */
void hdump(unsigned char *packet, unsigned int len) {
unsigned int i=0, j=0;

printf("Here it is:\n");

for(i=0; i < len; i++){
  j = (unsigned) (packet[i]);
  printf("%-2X ", j);
  if (!((i+1)%16))
    printf("\n");
  else if (!((i+1)%4))
    printf("  ");
}
printf("\n");
}

/* A better version of hdump, from Lamont Granquist.  Modified slightly
   by Fyodor (fyodor@insecure.org) */
void lamont_hdump(char *cp, unsigned int length) {

  /* stolen from tcpdump, then kludged extensively */

  static const char asciify[] = "................................ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~.................................................................................................................................";

  const u_short *sp;
  const u_char *ap;
  unsigned char *bp = (unsigned char *) cp;
  u_int i, j;
  int nshorts, nshorts2;
  int padding;
  
  printf("\n\t");
  padding = 0;
  sp = (u_short *)bp;
  ap = (u_char *)bp;
  nshorts = (u_int) length / sizeof(u_short);
  nshorts2 = (u_int) length / sizeof(u_short);
  i = 0;
  j = 0;
  while(1) {
    while (--nshorts >= 0) {
      printf(" %04x", ntohs(*sp));
      sp++;
      if ((++i % 8) == 0)
        break;
    }
    if (nshorts < 0) {
      if ((length & 1) && (((i-1) % 8) != 0)) {
        printf(" %02x  ", *(u_char *)sp);
        padding++;
      }
      nshorts = (8 - (nshorts2 - nshorts));
      while(--nshorts >= 0) {
        printf("     ");
      }
      if (!padding) printf("     ");
    }
    printf("  ");

    while (--nshorts2 >= 0) {
      printf("%c%c", asciify[*ap], asciify[*(ap+1)]);
      ap += 2;
      if ((++j % 8) == 0) {
        printf("\n\t");
        break;
      }
    }
    if (nshorts2 < 0) {
      if ((length & 1) && (((j-1) % 8) != 0)) {
        printf("%c", asciify[*ap]);
      }
      break;
    }
  }
  if ((length & 1) && (((i-1) % 8) == 0)) {
    printf(" %02x", *(u_char *)sp);
    printf("                                       %c", asciify[*ap]);
  }
  printf("\n");
}

#ifndef HAVE_STRERROR
char *strerror(int errnum) {
  static char buf[1024];
  sprintf(buf, "your system is too old for strerror of errno %d\n", errnum);
  return buf;
}
#endif

/* Like the perl equivialent -- It removes the terminating newline from string
   IF one exists.  It then returns the POSSIBLY MODIFIED string */
char *chomp(char *string) {
  int len = strlen(string);
  if (len && string[len - 1] == '\n') {
    if (len > 1 && string[len - 2] == '\r')
      string[len - 2] = '\0';
    else
      string[len - 1] = '\0';
  }
  return string;
}

/* Compare a canonical option name (e.g. "max-scan-delay") with a
   user-generated option such as "max_scan_delay" and returns 0 if the
   two values are considered equivalant (for example, - and _ are
   considered to be the same), nonzero otherwise. */
int optcmp(const char *a, const char *b) {
  while(*a && *b) {
    if (*a == '_' || *a == '-') {
      if (*b != '_' && *b != '-')
	return 1;
    }
    else if (*a != *b)
      return 1;
    a++; b++;
  }
  if (*a || *b)
    return 1;
  return 0;
}

/* Convert a comma-separated list of ASCII u16-sized numbers into the
   given 'dest' array, which is of total size (meaning sizeof() as
   opposed to numelements) of destsize.  If min_elem and max_elem are
   provided, each number must be within (or equal to) those
   constraints.  The number of numbers stored in 'dest' is returned,
   except that -1 is returned in the case of an error. If -1 is
   returned and errorstr is non-null, *errorstr is filled with a ptr to a
   static string literal describing the error. */

int numberlist2array(char *expr, u16 *dest, int destsize, char **errorstr, u16 min_elem, u16 max_elem) {
  char *current_range;
  char *endptr;
  char *errbogus;
  long val;
  int max_vals = destsize / 2;
  int num_vals_saved = 0;
  current_range = expr;

  if (!errorstr)
    errorstr = &errbogus;

  if (destsize % 2 != 0) {
    *errorstr = "Bogus call to numerlist2array() -- destsize must be a multiple of 2";
    return -1;
  }

  if (!expr || !*expr)
    return 0;

  do {
    if (num_vals_saved == max_vals) {
      *errorstr = "Buffer would overflow -- too many numbers in provided list";
      return -1;
    }
    if( !isdigit((int) *current_range) ) {
      *errorstr = "Alleged number begins with nondigit!  Example of proper form: \"20,80,65532\"";
      return -1;
    }
    val = strtol(current_range, &endptr, 10);
    if( val < min_elem || val > max_elem ) {
      *errorstr = "Number given in list is outside given legal range";
      return -1;
    }
    dest[num_vals_saved++] = (u16) val;
    current_range = endptr;
    while (*current_range == ',' || isspace(*current_range))
      current_range++;
    if (*current_range && !isdigit(*current_range)) {
      *errorstr = "Bogus character in supposed number-list string. Example of proper form: \"20,80,65532\"";
      return -1;
    }
  } while( current_range && *current_range);

  return num_vals_saved;
}

/* Scramble the contents of an array*/
void genfry(unsigned char *arr, int elem_sz, int num_elem) {
int i;
unsigned int pos;
unsigned char *bytes;
unsigned char *cptr;
unsigned short *sptr;
unsigned int *iptr;
unsigned char *tmp;
int bpe;

if (sizeof(unsigned char) != 1)
  fatal("genfry() requires 1 byte chars");

if (num_elem < 2)
  return;

 if (elem_sz == sizeof(unsigned short)) {
   shortfry((unsigned short *)arr, num_elem);
   return;
 }

/* OK, so I am stingy with the random bytes! */
if (num_elem < 256) 
  bpe = sizeof(unsigned char);
else if (num_elem < 65536)
  bpe = sizeof(unsigned short);
else bpe = sizeof(unsigned int);

bytes = (unsigned char *) malloc(bpe * num_elem);
tmp = (unsigned char *) malloc(elem_sz);

get_random_bytes(bytes, bpe * num_elem);
cptr = bytes;
sptr = (unsigned short *)bytes;
iptr = (unsigned int *) bytes;

 for(i=num_elem - 1; i > 0; i--) {
   if (num_elem < 256) {
     pos = *cptr; cptr++;
   }
   else if (num_elem < 65536) {
     pos = *sptr; sptr++;
   } else {
     pos = *iptr; iptr++;
   }
   pos %= i+1;
   memcpy(tmp, arr + elem_sz * i, elem_sz);
   memcpy(arr + elem_sz * i, arr + elem_sz * pos, elem_sz);
   memcpy(arr + elem_sz * pos, tmp, elem_sz);
 }
 free(bytes);
 free(tmp);
}

void shortfry(unsigned short *arr, int num_elem) {
int num;
unsigned short tmp;
int i;

if (num_elem < 2)
  return;
 
 for(i= num_elem - 1; i > 0 ; i--) {
   num = get_random_ushort() % (i + 1);
   if (i == num) continue;
   tmp = arr[i];
   arr[i] = arr[num];
   arr[num] = tmp;
 } 

 return;
}

// Send data to a socket, keep retrying until an error or the full length
// is sent.  Returns -1 if there is an error, or len if the full length was sent.
int Send(int sd, const void *msg, size_t len, int flags) {
  int res;
  unsigned int sentlen = 0;

  do {
    res = send(sd,(char *) msg + sentlen, len - sentlen, 0);
    if (res > 0)
      sentlen += res;
  } while(sentlen < len && (res != -1 || socket_errno() == EINTR));

  return (res < 0)? -1 : (int) len;
}

// Write data to a file descriptor, keep retrying until an error or the full length
// is written.  Returns -1 if there is an error, or len if the full length was sent.
// Note that this does NOT work well on Windows using sockets -- so use Send() above 
// for those.  I don't know if it works with regular files on Windows with files).
ssize_t Write(int fd, const void *buf, size_t count) {
  int res;
  unsigned int len;

  len = 0;
  do {
    res = write(fd,(char *) buf + len,count - len);
    if (res > 0)
      len += res;
  } while(len < count && (res != -1 || socket_errno() == EINTR));

  return (res == -1)? -1 : (int) count;
}


/* gcd_1 and gcd_n_long were sent in by Peter Kosinar <goober@gjh.sk> 
   Not needed for gcd_n_long, just for the case you'd want to have gcd
   for two arguments too. */
unsigned long gcd_ulong(unsigned long a, unsigned long b)
{
  /* Shorter
     while (b) { a%=b; if (!a) return b; b%=a; } */
  
  /* Faster */
  unsigned long c; 
  if (a<b) { c=a; a=b; b=c; }
  while (b) { c=a%b; a=b; b=c; }
  
  /* Common for both */
  return a;
}

unsigned long gcd_n_ulong(long nvals, unsigned long *val)
 {
   unsigned long a,b,c;
   
   if (!nvals) return 1;
   a=*val;
   for (nvals--;nvals;nvals--)
     {
       b=*++val;
       if (a<b) { c=a; a=b; b=c; }
       while (b) { c=a%b; a=b; b=c; }
     }
   return a;
 }

unsigned int gcd_uint(unsigned int a, unsigned int b)
{
  /* Shorter
     while (b) { a%=b; if (!a) return b; b%=a; } */
  
  /* Faster */
  unsigned int c; 
  if (a<b) { c=a; a=b; b=c; }
  while (b) { c=a%b; a=b; b=c; }
  
  /* Common for both */
  return a;
}

unsigned int gcd_n_uint(int nvals, unsigned int *val)
 {
   unsigned int a,b,c;
   
   if (!nvals) return 1;
   a=*val;
   for (nvals--;nvals;nvals--)
     {
       b=*++val;
       if (a<b) { c=a; a=b; b=c; }
       while (b) { c=a%b; a=b; b=c; }
     }
   return a;
 }

/* This function takes a command and the address of an uninitialized
   char ** .  It parses the command (by seperating out whitespace)
   into an argv[] style char **, which it sets the argv parameter to.
   The function returns the number of items filled up in the array
   (argc), or -1 in the case of an error.  This function allocates
   memory for argv and thus it must be freed -- use argv_parse_free()
   for that.  If arg_parse returns <1, then argv does not need to be freed.
   The returned arrays are always terminated with a NULL pointer */
int arg_parse(const char *command, char ***argv) {
  char **myargv = NULL;
  int argc = 0;
  char mycommand[4096];
  char *start, *end;
  char oldend;

  *argv = NULL;
  if (Strncpy(mycommand, command, 4096) == -1) {      
    return -1;
  }
  myargv = (char **) malloc((MAX_PARSE_ARGS + 2) * sizeof(char *));
  memset(myargv, 0, (MAX_PARSE_ARGS+2) * sizeof(char *));
  myargv[0] = (char *) 0x123456; /* Integrity checker */
  myargv++;
  start = mycommand;
  while(start && *start) {
    while(*start && isspace((int) *start))
      start++;
    if (*start == '"') {
      start++;
      end = strchr(start, '"');
    } else if (*start == '\'') {
      start++;
      end = strchr(start, '\'');      
    } else if (!*start) {
      continue;
    } else {
      end = start+1;
      while(*end && !isspace((int) *end)) {      
	end++;
      }
    }
    if (!end) {
      arg_parse_free(myargv);
      return -1;
    }
    if (argc >= MAX_PARSE_ARGS) {
      arg_parse_free(myargv);
      return -1;
    }
    oldend = *end;
    *end = '\0';
    myargv[argc++] = strdup(start);
    if (oldend)
      start = end + 1;
    else start = end;
  }
  myargv[argc+1] = 0;
  *argv = myargv;
  return argc;
}

/* Free an argv allocated inside arg_parse */
void arg_parse_free(char **argv) {
  char **current;
  /* Integrity check */
  argv--;
  assert(argv[0] == (char *) 0x123456);
  current = argv + 1;
  while(*current) {
    free(*current);
    current++;
  }
  free(argv);
}

/* Converts an Nmap time specification string into milliseconds.  If
   the string is a plain non-negative number, it is considered to
   already be in milliseconds and is returned.  If it is a number
   followed by 's' (for seconds), 'm' (minutes), or 'h' (hours), the
   number is converted to milliseconds and returned.  If Nmap cannot
   parse the string, it is returned instead. */
long tval2msecs(char *tspec) {
  long l;
  char *endptr = NULL;
  l = strtol(tspec, &endptr, 10);
  if (l < 0 || !endptr) return -1;
  if (*endptr == '\0') return l;
  if (*endptr == 's' || *endptr == 'S') return l * 1000;
  if ((*endptr == 'm' || *endptr == 'M')) {
    if (*(endptr + 1) == 's' || *(endptr + 1) == 'S') 
      return l;
    return l * 60000;
  }
  if (*endptr == 'h' || *endptr == 'H') return l * 3600000;
  return -1;
}

// A simple function to form a character from 2 hex digits in ASCII form
static unsigned char hex2char(unsigned char a, unsigned char b)
{
  int val;
  if (!isxdigit(a) || !isxdigit(b)) return 0;
  a = tolower(a);
  b = tolower(b);
  if (isdigit(a))
    val = (a - '0') << 4;
  else val = (10 + (a - 'a')) << 4;

  if (isdigit(b))
    val += (b - '0');
  else val += 10 + (b - 'a');

  return (unsigned char) val;
}

/* Convert a string in the format of a roughly C-style string literal
   (e.g. can have \r, \n, \xHH escapes, etc.) into a binary string.
   This is done in-place, and the new (shorter or the same) length is
   stored in newlen.  If parsing fails, NULL is returned, otherwise
   str is returned. */
char *cstring_unescape(char *str, unsigned int *newlen) {
  char *dst = str, *src = str;
  char newchar;

  while(*src) {
    if (*src == '\\' ) {
      src++;
      switch(*src) {
      case '0':
	newchar = '\0'; src++; break;
      case 'a': // Bell (BEL)
	newchar = '\a'; src++; break;	
      case 'b': // Backspace (BS)
	newchar = '\b'; src++; break;	
      case 'f': // Formfeed (FF)
	newchar = '\f'; src++; break;	
      case 'n': // Linefeed/Newline (LF)
	newchar = '\n'; src++; break;	
      case 'r': // Carriage Return (CR)
	newchar = '\r'; src++; break;	
      case 't': // Horizontal Tab (TAB)
	newchar = '\t'; src++; break;	
      case 'v': // Vertical Tab (VT)
	newchar = '\v'; src++; break;	
      case 'x':
	src++;
	if (!*src || !*(src + 1)) return NULL;
	if (!isxdigit(*src) || !isxdigit(*(src + 1))) return NULL;
	newchar = hex2char(*src, *(src + 1));
	src += 2;
	break;
      default:
	if (isalnum(*src))
	  return NULL; // I don't really feel like supporting octals such as \015
	// Other characters I'll just copy as is
	newchar = *src;
	src++;
	break;
      }
      *dst = newchar;
      dst++;
    } else {
      if (dst != src)
	*dst = *src;
      dst++; src++;
    }
  }

  *dst = '\0'; // terminated, but this string can include other \0, so use newlen
  if (newlen) *newlen = dst - str;
  return str;
}

/* mmap() an entire file into the address space.  Returns a pointer
   to the beginning of the file.  The mmap'ed length is returned
   inside the length parameter.  If there is a problem, NULL is
   returned, the value of length is undefined, and errno is set to
   something appropriate.  The user is responsible for doing
   an munmap(ptr, length) when finished with it.  openflags should 
   be O_RDONLY or O_RDWR, or O_WRONLY
*/

#ifndef WIN32
char *mmapfile(char *fname, int *length, int openflags) {
  struct stat st;
  int fd;
  char *fileptr;

  if (!length || !fname) {
    errno = EINVAL;
    return NULL;
  }

  *length = -1;

  if (stat(fname, &st) == -1) {
    errno = ENOENT;
    return NULL;
  }

  fd = open(fname, openflags);
  if (fd == -1) {
    return NULL;
  }

  fileptr = (char *)mmap(0, st.st_size, (openflags == O_RDONLY)? PROT_READ :
			 (openflags == O_RDWR)? (PROT_READ|PROT_WRITE) 
			 : PROT_WRITE, MAP_SHARED, fd, 0);

  close(fd);

#ifdef MAP_FAILED
  if (fileptr == (void *)MAP_FAILED) return NULL;
#else
  if (fileptr == (char *) -1) return NULL;
#endif

  *length = st.st_size;
  return fileptr;
}
#else /* WIN32 */
/* FIXME:  From the looks of it, this function can only handle one mmaped 
   file at a time (note how gmap is used).*/
/* I believe this was written by Ryan Permeh ( ryan@eeye.com) */

static HANDLE gmap = NULL;

char *mmapfile(char *fname, int *length, int openflags)
{
	HANDLE fd;
	DWORD mflags, oflags;
	char *fileptr;

	if (!length || !fname) {
		WSASetLastError(EINVAL);
		return NULL;
	}

 if (openflags == O_RDONLY) {
  oflags = GENERIC_READ;
  mflags = PAGE_READONLY;
  }
 else {
  oflags = GENERIC_READ | GENERIC_WRITE;
  mflags = PAGE_READONLY | PAGE_READWRITE;
 }

 fd = CreateFile (
   fname,
   oflags,                       // open flags
   0,                            // do not share
   NULL,                         // no security
   OPEN_EXISTING,                // open existing
   FILE_ATTRIBUTE_NORMAL,
   NULL);                        // no attr. template
 if (!fd)
  pfatal ("%s(%u): CreateFile()", __FILE__, __LINE__);

 *length = (int) GetFileSize (fd, NULL);

 gmap = CreateFileMapping (fd, NULL, mflags, 0, 0, NULL);
 if (!gmap)
  pfatal ("%s(%u): CreateFileMapping(), file '%s', length %d, mflags %08lX",
    __FILE__, __LINE__, fname, *length, mflags);

 fileptr = (char*) MapViewOfFile (gmap, oflags == GENERIC_READ ? FILE_MAP_READ : FILE_MAP_WRITE,
                                     0, 0, 0);
 if (!fileptr)
  pfatal ("%s(%u): MapViewOfFile()", __FILE__, __LINE__);

 CloseHandle (fd);

 if (o.debugging > 2)
  printf ("mmapfile(): fd %08lX, gmap %08lX, fileptr %08lX, length %d\n",
    (DWORD)fd, (DWORD)gmap, (DWORD)fileptr, *length);

	return fileptr;
}


/* FIXME:  This only works if the file was mapped by mmapfile (and only
   works if the file is the most recently mapped one */
int win32_munmap(char *filestr, int filelen)
{
  if (gmap == 0)
    fatal("win32_munmap: no current mapping !\n");

  FlushViewOfFile(filestr, filelen);
  UnmapViewOfFile(filestr);
  CloseHandle(gmap);
  gmap = NULL;
  return 0;
}

#endif
