
/***************************************************************************
 * service_scan.h -- Routines used for service fingerprinting to determine *
 * what application-level protocol is listening on a given port            *
 * (e.g. snmp, http, ftp, smtp, etc.)                                      *
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
 * o Executes Nmap                                                         *
 * o Integrates/includes/aggregates Nmap into an executable installer      *
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
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the (GPL) terms above, then that      *
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
 * http://www.gnu.org/copyleft/gpl.html .                                  *
 *                                                                         *
 ***************************************************************************/

/* $Id$ */


#include "service_scan.h"
#include "timing.h"
#include "NmapOps.h"
#include "nsock.h"
#if HAVE_OPENSSL
#include <openssl/ssl.h>
#endif

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

#include <algorithm>
#include <list>

// Because this file uses assert()s for some security checking, we can't
// have anyone turning off debugging.
#undef NDEBUG

extern NmapOps o;

// Details on a particular service (open port) we are trying to match
class ServiceNFO {
public:
  ServiceNFO(AllProbes *AP);
  ~ServiceNFO();

  // If a service response to a given probeName, this function adds
  // the resonse the the fingerprint for that service.  The
  // fingerprint can be printed when nothing matches the service.  You
  // can obtain the fingerprint (if any) via getServiceFingerprint();
  void addToServiceFingerprint(const char *probeName, const u8 *resp, 
			       int resplen);

  // Get the service fingerprint.  It is NULL if there is none, such
  // as if there was a match before any other probes were finished (or
  // if no probes gave back data).  Note that this is plain
  // NUL-terminated ASCII data, although the length is optionally
  // available anyway.  This function terminates the service fingerprint
  // with a semi-colon
  const char *getServiceFingerprint(int *flen);

  // Note that the next 2 members are for convenience and are not destroyed w/the ServiceNFO
  Target *target; // the port belongs to this target host
  Port *port; // The Port that this service represents (this copy is taken from inside Target)
  // if a match is found, it is placed here.  Otherwise NULL
  const char *probe_matched;
  // If a match is found, any product/version/info is placed in these
  // 3 strings.  Otherwise the string will be 0 length.
  char product_matched[80];
  char version_matched[80];
  char extrainfo_matched[128];
  enum service_tunnel_type tunnel; /* SERVICE_TUNNEL_NONE, SERVICE_TUNNEL_SSL */
  // This stores our SSL session id, which will help speed up subsequent
  // SSL connections.  It's overwritten each time.  void* is used so we don't
  // need to #ifdef HAVE_OPENSSL all over.  We'll cast later as needed.
  void *ssl_session;
  // if a match was found (see above), this tells whether it was a "soft"
  // or hard match.  It is always false if no match has been found.
  bool softMatchFound;
  // most recent probe executed (or in progress).  If there has been a match 
  // (probe_matched != NULL), this will be the corresponding ServiceProbe.
  ServiceProbe *currentProbe();
  // computes the next probe to test, and ALSO CHANGES currentProbe() to
  // that!  If newresp is true, the old response info will be lost and
  // invalidated.  Otherwise it remains as if it had been received by
  // the current probe (useful after a NULL probe).
  ServiceProbe *nextProbe(bool newresp);
  // Resets the probes back to the first one. One case where this is useful is
  // when SSL is detected -- we redo all probes through SSL.  If freeFP, any
  // service fingerprint is freed too.
  void resetProbes(bool freefp);
  // Number of milliseconds left to complete the present probe, or 0 if
  // the probe is already expired.  Timeval can omitted, it is just there 
  // as an optimization in case you have it handy.
  int currentprobe_timemsleft(const struct timeval *now = NULL);
  enum serviceprobestate probe_state; // defined in portlist.h
  nsock_iod niod; // The IO Descriptor being used in this probe (or NULL)
  u16 portno; // in host byte order
  u8 proto; // IPPROTO_TCP or IPPROTO_UDP
  // The time that the current probe was executed (meaning TCP connection
  // made or first UDP packet sent
  struct timeval currentprobe_exec_time;
  // Append newly-received data to the current response string (if any)
  void appendtocurrentproberesponse(const u8 *respstr, int respstrlen);
  // Get the full current response string.  Note that this pointer is 
  // INVALIDATED if you call appendtocurrentproberesponse() or nextProbe()
  u8 *getcurrentproberesponse(int *respstrlen);
  AllProbes *AP;
          
private:
  // Adds a character to servicefp.  Takes care of word wrapping if
  // neccessary at the given (wrapat) column.  Chars will only be
  // written if there is enough space.  Oherwise it exits.
  void addServiceChar(char c, int wrapat);
  // Like addServiceChar, but for a whole zero-terminated string
  void addServiceString(char *s, int wrapat);
  vector<ServiceProbe *>::iterator current_probe;
  u8 *currentresp;
  int currentresplen;
  char *servicefp;
  int servicefplen;
  int servicefpalloc;
};

// This holds the service information for a group of Targets being service scanned.
class ServiceGroup {
public:
  ServiceGroup(vector<Target *> &Targets, AllProbes *AP);
  ~ServiceGroup();
  list<ServiceNFO *> services_finished; // Services finished (discovered or not)
  list<ServiceNFO *> services_in_progress; // Services currently being probed
  list<ServiceNFO *> services_remaining; // Probes not started yet
  unsigned int ideal_parallelism; // Max (and desired) number of probes out at once.
  ScanProgressMeter *SPM;
  int num_hosts_timedout; // # of hosts timed out during (or before) scan
  private:
};

#define SUBSTARGS_MAX_ARGS 5
#define SUBSTARGS_STRLEN 128
#define SUBSTARGS_ARGTYPE_NONE 0
#define SUBSTARGS_ARGTYPE_STRING 1
#define SUBSTARGS_ARGTYPE_INT 2
struct substargs {
  int num_args; // Total number of arguments found
  char str_args[SUBSTARGS_MAX_ARGS][SUBSTARGS_STRLEN];
  // This is the length of each string arg, since they can contain zeros.
  // The str_args[] are zero-terminated for convenience in the cases where
  // you know they won't contain zero.
  int str_args_len[SUBSTARGS_MAX_ARGS]; 
  int int_args[SUBSTARGS_MAX_ARGS];
  // The type of each argument -- see #define's above.
  int arg_types[SUBSTARGS_MAX_ARGS];
};

/********************   PROTOTYPES *******************/
void servicescan_read_handler(nsock_pool nsp, nsock_event nse, void *mydata);
void servicescan_write_handler(nsock_pool nsp, nsock_event nse, void *mydata);
void servicescan_connect_handler(nsock_pool nsp, nsock_event nse, void *mydata);
void end_svcprobe(nsock_pool nsp, enum serviceprobestate probe_state, ServiceGroup *SG, ServiceNFO *svc, nsock_iod nsi);
int launchSomeServiceProbes(nsock_pool nsp, ServiceGroup *SG);

ServiceProbeMatch::ServiceProbeMatch() {
  deflineno = -1;
  servicename = NULL;
  matchstr = NULL;
  product_template = version_template = info_template = NULL;
  regex_compiled = NULL;
  regex_extra = NULL;
  isInitialized = false;
  matchops_ignorecase = false;
  matchops_dotall = false;
  isSoft = false;
}

ServiceProbeMatch::~ServiceProbeMatch() {
  if (!isInitialized) return;
  if (servicename) free(servicename);
  if (matchstr) free(matchstr);
  if (product_template) free(product_template);
  if (version_template) free(version_template);
  if (info_template) free(info_template);
  matchstrlen = 0;
  if (regex_compiled) pcre_free(regex_compiled);
  if (regex_extra) pcre_free(regex_extra);
  isInitialized = false;
  matchops_anchor = -1;
}

// match text from the nmap-service-probes file.  This must be called
// before you try and do anything with this match.  This function
// should be passed the whole line starting with "match" or
// "softmatch" in nmap-service-probes.  The line number that the text
// is provided so that it can be reported in error messages.  This
// function will abort the program if there is a syntax problem.
void ServiceProbeMatch::InitMatch(const char *matchtext, int lineno) {
  const char *p;
  char delimchar;
  int pcre_compile_ops = 0;
  const char *pcre_errptr = NULL;
  int pcre_erroffset = 0;
  unsigned int tmpbuflen = 0;

  if (isInitialized) fatal("Sorry ... ServiceProbeMatch::InitMatch does not yet support reinitializion");
  if (!matchtext || !*matchtext) 
    fatal("ServiceProbeMatch::InitMatch: no matchtext passed in (line %d of nmap-service-probes)", lineno);
  isInitialized = true;

  deflineno = lineno;
  while(isspace(*matchtext)) matchtext++;

  // first we find whether this is a "soft" or normal match
  if (strncmp(matchtext, "softmatch ", 10) == 0) {
    isSoft = true;
    matchtext += 10;
  } else if (strncmp(matchtext, "match ", 6) == 0) {
    isSoft = false;
    matchtext += 6;
  } else 
    fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes - must begin with \"match\" or \"softmatch\"", lineno);

  // next comes the service name
  p = strchr(matchtext, ' ');
  if (!p) fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes: could not find service name", lineno);

  servicename = (char *) safe_malloc(p - matchtext + 1);
  memcpy(servicename, matchtext, p - matchtext);
  servicename[p - matchtext]  = '\0';

  // The next part is a perl style regular expression specifier, like:
  // m/^220 .*smtp/i Where 'm' means a normal regular expressions is
  // used, the char after m can be anything (within reason, slash in
  // this case) and tells us what delieates the end of the regex.
  // After the delineating character are any single-character
  // options. ('i' means "case insensitive", 's' means that . matches
  // newlines (both are just as in perl)
  matchtext = p;
  while(isspace(*matchtext)) matchtext++;
  if (*matchtext == 'm') {
    if (!*(matchtext+1))
      fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes: matchtext must begin with 'm'", lineno);
    matchtype = SERVICEMATCH_REGEX;
    delimchar = *(++matchtext);
    ++matchtext;
    // find the end of the regex
    p = strchr(matchtext, delimchar);
    if (!p) fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes: could not find end delimiter for regex", lineno);
    matchstrlen = p - matchtext;
    matchstr = (char *) safe_malloc(matchstrlen + 1);
    memcpy(matchstr, matchtext, matchstrlen);
    matchstr[matchstrlen]  = '\0';
    
    matchtext = p + 1; // skip past the delim
    // any options?
    while(*matchtext && !isspace(*matchtext)) {
      if (*matchtext == 'i')
	matchops_ignorecase = true;
      else if (*matchtext == 's')
	matchops_dotall = true;
      else fatal("ServiceProbeMatch::InitMatch: illegal regexp option on line %d of nmap-service-probes", lineno);
      matchtext++;
    }

    // Next we compile and study the regular expression to match
    if (matchops_ignorecase)
      pcre_compile_ops |= PCRE_CASELESS;

    if (matchops_dotall)
      pcre_compile_ops |= PCRE_DOTALL;
    
    regex_compiled = pcre_compile(matchstr, pcre_compile_ops, &pcre_errptr, 
				     &pcre_erroffset, NULL);
    
    if (regex_compiled == NULL)
      fatal("ServiceProbeMatch::InitMatch: illegal regexp on line %d of nmap-service-probes (at regexp offset %d): %s\n", lineno, pcre_erroffset, pcre_errptr);
    
    
    // Now study the regexp for greater efficiency
    regex_extra = pcre_study(regex_compiled, 0, &pcre_errptr);
    if (pcre_errptr != NULL)
      fatal("ServiceProbeMatch::InitMatch: failed to pcre_study regexp on line %d of nmap-service-probes: %s\n", lineno, pcre_errptr);
  } else {
    /* Invalid matchtext */
    fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes: match string must begin with 'm'", lineno);
  }

  /* OK!  Now we look for the optional version-detection
     product/version info in the form v/productname/version/info/
     (where '/' delimiter can be anything) */

  while(isspace(*matchtext)) matchtext++;
  if (isalnum(*matchtext)) {
    if (isSoft)
      fatal("ServiceProbeMatch::InitMatch: illegal trailing garbage on line %d of nmap-service-probes - note that softmatch lines cannot have a version specifier.", lineno);
    if (*matchtext != 'v') 
      fatal("ServiceProbeMatch::InitMatch: illegal trailing garbage (should be a version pattern match?) on line %d of nmap-service-probes", lineno);
    delimchar = *(++matchtext);
    ++matchtext;
    // find the end of the productname
    p = strchr(matchtext, delimchar);
    if (!p) fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes (in the version pattern - productname section)", lineno);
    tmpbuflen = p - matchtext;
    if (tmpbuflen > 0) {
      product_template = (char *) safe_malloc(tmpbuflen + 1);
      memcpy(product_template, matchtext, tmpbuflen);
      product_template[tmpbuflen] = '\0';
    }
    // Now lets go after the version info
    matchtext = p+1;
    p = strchr(matchtext, delimchar);
    if (!p) fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes (in the version pattern - version section)", lineno);
    tmpbuflen = p - matchtext;
    if (tmpbuflen > 0) {
      version_template = (char *) safe_malloc(tmpbuflen + 1);
      memcpy(version_template, matchtext, tmpbuflen);
      version_template[tmpbuflen] = '\0';
    }
    // And finally for the "info"
    matchtext = p+1;
    p = strchr(matchtext, delimchar);
    if (!p) fatal("ServiceProbeMatch::InitMatch: parse error on line %d of nmap-service-probes (in the version pattern - info section)", lineno);
    tmpbuflen = p - matchtext;
    if (tmpbuflen > 0) {
      info_template = (char *) safe_malloc(tmpbuflen + 1);
      memcpy(info_template, matchtext, tmpbuflen);
      info_template[tmpbuflen] = '\0';
    }

    // Insure there is no trailing junk after the version string
    // (usually cased by delimchar accidently being in the
    // product/version/info string).
    p++;
    while(*p && *p != '\r' && *p != '\n') {
      if (!isspace((int) *(unsigned char *)p))  fatal("ServiceProbeMatch::InitMatch: illegal trailing garbage (accidental version delimeter in your v//// string?) on line %d of nmap-service-probes", lineno);
      p++;
    }
  }

  isInitialized = 1;
}

  // If the buf (of length buflen) match the regex in this
  // ServiceProbeMatch, returns the details of the match (service
  // name, version number if applicable, and whether this is a "soft"
  // match.  If the buf doesn't match, the serviceName field in the
  // structure will be NULL.  The MatchDetails sructure returned is
  // only valid until the next time this function is called. The only
  // exception is that the serviceName field can be saved throughought
  // program execution.  If no version matched, that field will be
  // NULL.
const struct MatchDetails *ServiceProbeMatch::testMatch(const u8 *buf, int buflen) {
  int rc;
  int i;
  static char product[80];
  static char version[80];
  static char info[128];
  char *bufc = (char *) buf;
  int ovector[150]; // allows 50 substring matches (including the overall match)
  assert(isInitialized);

  assert (matchtype == SERVICEMATCH_REGEX);

  // Clear out the output struct
  memset(&MD_return, 0, sizeof(MD_return));
  MD_return.isSoft = isSoft;

  rc = pcre_exec(regex_compiled, regex_extra, bufc, buflen, 0, 0, ovector, sizeof(ovector) / sizeof(*ovector));
  if (rc < 0) {
#ifdef PCRE_ERROR_MATCHLIMIT  // earlier PCRE versions lack this
    if (rc == PCRE_ERROR_MATCHLIMIT) {
      if (o.debugging || o.verbose > 1) 
	error("Warning: Hit PCRE_ERROR_MATCHLIMIT when probing for service %s with the regex '%s'", servicename, matchstr);
    } else
#endif // PCRE_ERROR_MATCHLIMIT
      if (rc != PCRE_ERROR_NOMATCH) {
	fatal("Unexpected PCRE error (%d) when probing for service %s with the regex '%s'", rc, servicename, matchstr);
      }
  } else {
    // Yeah!  Match apparently succeeded.
    // Now lets get the version number if available
    i = getVersionStr(buf, buflen, ovector, rc, product, sizeof(product), version, sizeof(version), info, sizeof(info));    
    if (*product) MD_return.product = product;
    if (*version) MD_return.version = version;
    if (*info) MD_return.info = info;
  
    MD_return.serviceName = servicename;
  }

  return &MD_return;
}

// This simple function parses arguments out of a string.  The string
// starts with the first argument.  Each argument can be a string or
// an integer.  Strings must be enclosed in double quotes ("").  Most
// standard C-style escapes are supported.  If this is successful, the
// number of args found is returned, args is filled appropriately, and
// args_end (if non-null) is set to the character after the closing
// ')'.  Otherwise we return -1 and the values of args and args_end
// are undefined.
static int getsubstcommandargs(struct substargs *args, char *args_start, 
			char **args_end) {
  char *p;
  unsigned int len;
  if (!args || !args_start) return -1;

  memset(args, 0, sizeof(*args));

  while(*args_start && *args_start != ')') {
    // Find the next argument.
    while(isspace(*args_start)) args_start++;
    if (*args_start == ')')
      break;
    else if (*args_start == '"') {
      // OK - it is a string
      // Do we have space for another arg?
      if (args->num_args == SUBSTARGS_MAX_ARGS)
	return -1;
      do {
	args_start++;
	if (*args_start == '"' && (*(args_start - 1) != '\\' || *(args_start - 2) == '\\'))
	  break;
	len = args->str_args_len[args->num_args];
	if (len >= SUBSTARGS_STRLEN - 1)
	  return -1;
	args->str_args[args->num_args][len] = *args_start;
	args->str_args_len[args->num_args]++;
      } while(*args_start);
      len = args->str_args_len[args->num_args];
      args->str_args[args->num_args][len] = '\0';
      // Now handle escaped characters and such
      if (!cstring_unescape(args->str_args[args->num_args], &len))
	return -1;
      args->str_args_len[args->num_args] = len;
      args->arg_types[args->num_args] = SUBSTARGS_ARGTYPE_STRING;
      args->num_args++;
      args_start++;
      args_start = strpbrk(args_start, ",)");
      if (!args_start) return -1;
      if (*args_start == ',') args_start++;
    } else {
      // Must be an integer argument
      args->int_args[args->num_args] = (int) strtol(args_start, &p, 0);
      if (p <= args_start) return -1;
      args_start = p;
      args->arg_types[args->num_args] = SUBSTARGS_ARGTYPE_INT;
      args->num_args++;
      args_start = strpbrk(args_start, ",)");
      if (!args_start) return -1;
      if (*args_start == ',') args_start++;
    }
  }

  if (*args_start == ')') args_start++;
  if (args_end) *args_end = args_start;
  return args->num_args;
}

// This function does the actual substitution of a placeholder like $2
// or $U(4) into the given buffer.  It returns the number of chars
// written, or -1 if it fails.  tmplvar is a template variable, such
// as "$U(2)".  We determine the appropriate string representing that,
// and place it in newstr (as long as it doesn't exceed newstrlen).
// We then set *tmplvarend to the character after the
// variable. subject, subjectlen, ovector, and nummatches mean the
// same as in dotmplsubst().
static int substvar(char *tmplvar, char **tmplvarend, char *newstr, 
	     int newstrlen, const u8 *subject, int subjectlen, int *ovector,
	     int nummatches) {

  char substcommand[16];
  char *p = NULL;
  char *p_end;
  int len;
  int subnum = 0;
  int offstart, offend;
  int byteswritten = 0; // for return val
  int rc;
  int i;
  struct substargs command_args;
  // skip the '$'
  if (*tmplvar != '$') return -1;
  tmplvar++;

  if (!isdigit(*tmplvar)) {
    p = strchr(tmplvar, '(');
    if (!p) return -1;
    len = p - tmplvar;
    if (!len || len >= (int) sizeof(substcommand))
      return -1;
    memcpy(substcommand, tmplvar, len);
    substcommand[len] = '\0';
    tmplvar = p+1;
    // Now we grab the arguments.
    rc = getsubstcommandargs(&command_args, tmplvar, &p_end);
    if (rc <= 0) return -1;
    tmplvar = p_end;
  } else {
    substcommand[0] = '\0';
    subnum = *tmplvar - '0';
    tmplvar++;
  }

  if (tmplvarend) *tmplvarend = tmplvar;

  if (!*substcommand) {
    if (subnum > 9 || subnum <= 0) return -1;
    if (subnum >= nummatches) return -1;
    offstart = ovector[subnum * 2];
    offend = ovector[subnum * 2 + 1];
    assert(offstart >= 0 && offstart < subjectlen);
    assert(offend >= 0 && offend <= subjectlen);
    len = offend - offstart;
    // A plain-jane copy
    if (newstrlen <= len - 1)
      return -1;
    memcpy(newstr, subject + offstart, len);
    byteswritten = len;
  } else if (strcmp(substcommand, "P") == 0) {
    if (command_args.arg_types[0] != SUBSTARGS_ARGTYPE_INT)
      return -1;
    subnum = command_args.int_args[0];
    if (subnum > 9 || subnum <= 0) return -1;
    if (subnum >= nummatches) return -1;
    offstart = ovector[subnum * 2];
    offend = ovector[subnum * 2 + 1];
    assert(offstart >= 0 && offstart < subjectlen);
    assert(offend >= 0 && offend <= subjectlen);
    // This filter only includes printable characters.  It is particularly
    // useful for collapsing unicode text that looks like 
    // "W\0O\0R\0K\0G\0R\0O\0U\0P\0"
    for(i=offstart; i < offend; i++)
      if (isprint((int) subject[i])) {
	if (byteswritten >= newstrlen - 1)
	  return -1;
	newstr[byteswritten++] = subject[i];
      }
  } else if (strcmp(substcommand, "SUBST") == 0) {
    char *findstr, *replstr;
    int findstrlen, replstrlen;
   if (command_args.arg_types[0] != SUBSTARGS_ARGTYPE_INT)
      return -1;
    subnum = command_args.int_args[0];
    if (subnum > 9 || subnum <= 0) return -1;
    if (subnum >= nummatches) return -1;
    offstart = ovector[subnum * 2];
    offend = ovector[subnum * 2 + 1];
    assert(offstart >= 0 && offstart < subjectlen);
    assert(offend >= 0 && offend <= subjectlen);
    if (command_args.arg_types[1] != SUBSTARGS_ARGTYPE_STRING ||
	command_args.arg_types[2] != SUBSTARGS_ARGTYPE_STRING)
      return -1;
    findstr = command_args.str_args[1];
    findstrlen = command_args.str_args_len[1];
    replstr = command_args.str_args[2];
    replstrlen = command_args.str_args_len[2];
    for(i=offstart; i < offend; ) {
      if (byteswritten >= newstrlen - 1)
	return -1;
      if (offend - i < findstrlen)
	newstr[byteswritten++] = subject[i++]; // No room for match
      else if (memcmp(subject + i, findstr, findstrlen) != 0)
	newstr[byteswritten++] = subject[i++]; // no match
      else {
	// The find string was found, copy it to newstring
	if (newstrlen - 1 - byteswritten < replstrlen)
	  return -1;
	memcpy(newstr + byteswritten, replstr, replstrlen);
	byteswritten += replstrlen;
	i += findstrlen;
      }
    }
  } else return -1; // Unknown command

  if (byteswritten >= newstrlen) return -1;
  newstr[byteswritten] = '\0';
  return byteswritten;
}



// This function takes a template string (tmpl) which can have
// placeholders in it such as $1 for substring matches in a regexp
// that was run against subject, and subjectlen, with the 'nummatches'
// matches in ovector.  The NUL-terminated newly composted string is
// placed into 'newstr', as long as it doesn't exceed 'newstrlen'
// bytes.  Trailing whitespace and commas are removed.  Returns zero for success
static int dotmplsubst(const u8 *subject, int subjectlen, 
		       int *ovector, int nummatches, char *tmpl, char *newstr,
		       int newstrlen) {
  int newlen;
  char *srcstart=tmpl, *srcend;
  char *dst = newstr;
  char *newstrend = newstr + newstrlen; // Right after the final char

  if (!newstr || !tmpl) return -1;
  if (newstrlen < 3) return -1; // fuck this!
  
  while(*srcstart) {
    // First do any literal text before '$'
    srcend = strchr(srcstart, '$');
    if (!srcend) {
      // Only literal text remain!
      while(*srcstart) {
	if (dst >= newstrend - 1)
	  return -1;
	*dst++ = *srcstart++;
      }
      *dst = '\0';
      while (--dst >= newstr) {
	if (isspace(*dst) || *dst == ',') 
	  *dst = '\0';
	else break;
      }
      return 0;
    } else {
      // Copy the literal text up to the '$', then do the substitution
      newlen = srcend - srcstart;
      if (newlen > 0) {
	if (newstrend - dst <= newlen - 1)
	  return -1;
	memcpy(dst, srcstart, newlen);
	dst += newlen;
      }
      srcstart = srcend;
      newlen = substvar(srcstart, &srcend, dst, newstrend - dst, subject, 
		    subjectlen, ovector, nummatches);
      if (newlen == -1) return -1;
      dst += newlen;
      srcstart = srcend;
    }
  }

  if (dst >= newstrend - 1)
    return -1;
  *dst = '\0';
  while (--dst >= newstr) {
    if (isspace(*dst) || *dst == ',') 
      *dst = '\0';
    else break;
  }
  return 0;

}


// Use the three version templates, and the match data included here
// to put the version info into 'product', 'version', and 'info',
// (as long as the given string sizes are sufficient).  Returns zero
// for success.  If no template is available for product, version,
// and/or info, that string will have zero length after the function
// call (assuming the corresponding length passed in is at least 1)
int ServiceProbeMatch::getVersionStr(const u8 *subject, int subjectlen, 
	    int *ovector, int nummatches, char *product, int productlen,
	    char *version, int versionlen, char *info, int infolen) {

  int rc;
  assert(productlen >= 0 && versionlen >= 0 && infolen >= 0);
  
  if (productlen > 0) *product = '\0';
  if (versionlen > 0) *version = '\0';
  if (infolen > 0) *info = '\0';
  int retval = 0;

  // Now lets get this started!  We begin with the product name
  if (product_template) {
    rc = dotmplsubst(subject, subjectlen, ovector, nummatches, product_template, product, productlen);
    if (rc != 0) {
      error("Warning: Servicescan failed to fill product_template (subjectlen: %d). Too long? Match string was line %d: v/%s/%s/%s", subjectlen, deflineno, (product_template)? product_template : "",
	    (version_template)? version_template : "", (info_template)? info_template : "");
      if (productlen > 0) *product = '\0';
      retval = -1;
    }
  }

  if (version_template) {
    rc = dotmplsubst(subject, subjectlen, ovector, nummatches, version_template, version, versionlen);
    if (rc != 0) {
      error("Warning: Servicescan failed to fill version_template (subjectlen: %d). Too long? Match string was line %d: v/%s/%s/%s", subjectlen, deflineno, (product_template)? product_template : "",
	    (version_template)? version_template : "", (info_template)? info_template : "");
      if (versionlen > 0) *version = '\0';
      retval = -1;
    }
  }

  if (info_template) {
    rc = dotmplsubst(subject, subjectlen, ovector, nummatches, info_template, info, infolen);
    if (rc != 0) {
      error("Warning: Servicescan failed to fill info_template (subjectlen: %d). Too long? Match string was line %d: v/%s/%s/%s", subjectlen, deflineno, (product_template)? product_template : "",
	    (version_template)? version_template : "", (info_template)? info_template : "");
      if (infolen > 0) *info = '\0';
      retval = -1;
    }
  }
  
  return retval;
}


ServiceProbe::ServiceProbe() {
  probename = NULL;
  probestring = NULL;
  totalwaitms = DEFAULT_SERVICEWAITMS;
  probestringlen = 0; probeprotocol = -1;
}

ServiceProbe::~ServiceProbe() {
  vector<ServiceProbeMatch *>::iterator vi;

  if (probename) free(probename);
  if (probestring) free(probestring);

  for(vi = matches.begin(); vi != matches.end(); vi++) {
    delete *vi;
  }
}

void ServiceProbe::setName(const char *name) {
  if (probename) free(probename);
  probename = strdup(name);
}

  // Parses the "probe " line in the nmap-service-probes file.  Pass the rest of the line
  // after "probe ".  The format better be:
  // [TCP|UDP] [probename] q|probetext|
  // Note that the delimiter (|) of the probetext can be anything (within reason)
  // the lineno is requested because this function will bail with an error
  // (giving the line number) if it fails to parse the string.
void ServiceProbe::setProbeDetails(char *pd, int lineno) {
  char *p;
  unsigned int len;
  char delimiter;

  if (!pd || !*pd)
    fatal("Parse error on line %d of nmap-services-probes: no arguments found!", lineno);

  // First the protocol
  if (strncmp(pd, "TCP ", 4) == 0)
      probeprotocol = IPPROTO_TCP;
  else if (strncmp(pd, "UDP ", 4) == 0)
      probeprotocol = IPPROTO_UDP;
  else fatal("Parse error on line %d of nmap-services-probes: invalid protocol", lineno);
  pd += 4;

  // Next the service name
  if (!isalnum(*pd)) fatal("Parse error on line %d of nmap-services-probes - bad probe name", lineno);
  p = strchr(pd, ' ');
  if (!p) fatal("Parse error on line %d of nmap-services-probes - nothing after probe name", lineno);
  len = p - pd;
  probename = (char *) safe_malloc(len + 1);
  memcpy(probename, pd, len);
  probename[len]  = '\0';

  // Now for the probe itself
  pd = p+1;

  if (*pd != 'q') fatal("Parse error on line %d of nmap-services-probes - probe string must begin with 'q'", lineno);
  delimiter = *(++pd);
  p = strchr(++pd, delimiter);
  if (!p) fatal("Parse error on line %d of nmap-services-probes -- no ending delimiter for probe string", lineno);
  *p = '\0';
  if (!cstring_unescape(pd, &len)) {
    fatal("Parse error on line %d of nmap-services-probes: bad probe string escaping", lineno);
  }
  setProbeString((const u8 *)pd, len);
}

void ServiceProbe::setProbeString(const u8 *ps, int stringlen) {
  if (probestringlen) free(probestring);
  probestringlen = stringlen;
  if (stringlen > 0) {
    probestring = (u8 *) safe_malloc(stringlen + 1);
    memcpy(probestring, ps, stringlen);
    probestring[stringlen] = '\0'; // but note that other \0 may be in string
  } else probestring = NULL;
}

void ServiceProbe::setPortVector(vector<u16> *portv, const char *portstr, 
				 int lineno) {
  const char *current_range;
  char *endptr;
  long int rangestart = 0, rangeend = 0;

  current_range = portstr;

  do {
    while(*current_range && isspace(*current_range)) current_range++;
    if (isdigit((int) *current_range)) {
      rangestart = strtol(current_range, &endptr, 10);
      if (rangestart < 0 || rangestart > 65535) {
	fatal("Parse error on line %d of nmap-services-probes: Ports must be between 0 and 65535 inclusive", lineno);
      }
      current_range = endptr;
      while(isspace((int) *current_range)) current_range++;
    } else {
      fatal("Parse error on line %d of nmap-services-probes: An example of proper portlist form is \"21-25,53,80\"", lineno);
    }

    /* Now I have a rangestart, time to go after rangeend */
    if (!*current_range || *current_range == ',') {
      /* Single port specification */
      rangeend = rangestart;
    } else if (*current_range == '-') {
      current_range++;
      if (isdigit((int) *current_range)) {
	rangeend = strtol(current_range, &endptr, 10);
	if (rangeend < 0 || rangeend > 65535 || rangeend < rangestart) {
	  fatal("Parse error on line %d of nmap-services-probes: Ports must be between 0 and 65535 inclusive", lineno);
	}
	current_range = endptr;
      } else {
	fatal("Parse error on line %d of nmap-services-probes: An example of proper portlist form is \"21-25,53,80\"", lineno);
      }
    } else {
      fatal("Parse error on line %d of nmap-services-probes: An example of proper portlist form is \"21-25,53,80\"", lineno);
    }

    /* Now I have a rangestart and a rangeend, so I can add these ports */
    while(rangestart <= rangeend) {
      portv->push_back(rangestart);
      rangestart++;
    }
    
    /* Find the next range */
    while(isspace((int) *current_range)) current_range++;
    if (*current_range && *current_range != ',') {
      fatal("Parse error on line %d of nmap-services-probes: An example of proper portlist form is \"21-25,53,80\"", lineno);
    }
    if (*current_range == ',')
      current_range++;
  } while(current_range && *current_range);
}

  // Takes a string as given in the 'ports '/'sslports ' line of
  // nmap-services-probes.  Pass in the list from the appropriate
  // line.  For 'sslports', tunnel should be specified as
  // SERVICE_TUNNEL_SSL.  Otherwise use SERVICE_TUNNEL_NONE.  The line
  // number is requested because this function will bail with an error
  // (giving the line number) if it fails to parse the string.  Ports
  // are a comma seperated list of prots and ranges
  // (e.g. 53,80,6000-6010).
void ServiceProbe::setProbablePorts(enum service_tunnel_type tunnel,
				    const char *portstr, int lineno) {
  if (tunnel == SERVICE_TUNNEL_NONE)
    setPortVector(&probableports, portstr, lineno);
  else {
    assert(tunnel == SERVICE_TUNNEL_SSL);
    setPortVector(&probablesslports, portstr, lineno);
  }
}

  /* Returns true if the passed in port is on the list of probable
     ports for this probe and tunnel type.  Use a tunnel of
     SERVICE_TUNNEL_SSL or SERVICE_TUNNEL_NONE as appropriate */
bool ServiceProbe::portIsProbable(enum service_tunnel_type tunnel, u16 portno) {
  vector<u16> *portv;

  portv = (tunnel == SERVICE_TUNNEL_SSL)? &probablesslports : &probableports;
  
  if (find(portv->begin(), portv->end(), portno) == portv->end())
    return false;
  return true;
}

 // Returns true if the passed in service name is among those that can
  // be detected by the matches in this probe;
bool ServiceProbe::serviceIsPossible(const char *sname) {
  vector<const char *>::iterator vi;

  for(vi = detectedServices.begin(); vi != detectedServices.end(); vi++) {
    if (strcmp(*vi, sname) == 0)
      return true;
  }
  return false;
}

  // Takes a match line in a probe description and adds it to the
  // list of matches for this probe.  This function should be passed
  // the whole line starting with "match" or "softmatch" in
  // nmap-service-probes.  The line number is requested because this
  // function will bail with an error (giving the line number) if it
  // fails to parse the string.
void ServiceProbe::addMatch(const char *match, int lineno) {
  const char *sname;
  ServiceProbeMatch *newmatch = new ServiceProbeMatch();
  newmatch->InitMatch(match, lineno);
  sname = newmatch->getName();
  if (!serviceIsPossible(sname))
    detectedServices.push_back(sname);
  matches.push_back(newmatch);
}

// Parses the given nmap-service-probes file into the AP class
void parse_nmap_service_probe_file(AllProbes *AP, char *filename) {
  ServiceProbe *newProbe;
  char line[2048];
  int lineno = 0;
  FILE *fp;

  // We better start by opening the file
  fp = fopen(filename, "r");
  if (!fp) 
    fatal("Failed to open nmap-service-probes file %s for reading", filename);

  while(fgets(line, sizeof(line), fp)) {
    lineno++;
    
    if (*line == '\n' || *line == '#')
      continue;
  
  anotherprobe:
  
    if (strncmp(line, "Probe ", 6) != 0)
      fatal("Parse error on line %d of nmap-service-probes file: %s -- line was expected to begin with \"Probe \"", lineno, filename);
    
    newProbe = new ServiceProbe();
    newProbe->setProbeDetails(line + 6, lineno);
    
    // Now we read the rest of the probe info
    while(fgets(line, sizeof(line), fp)) {
      lineno++;
      if (*line == '\n' || *line == '#')
	continue;
      
      if (strncmp(line, "Probe ", 6) == 0) {
	if (newProbe->isNullProbe()) {
	  assert(!AP->nullProbe);
	  AP->nullProbe = newProbe;
	} else {
	  AP->probes.push_back(newProbe);
	}
	goto anotherprobe;
      } else if (strncmp(line, "ports ", 6) == 0) {
	newProbe->setProbablePorts(SERVICE_TUNNEL_NONE, line + 6, lineno);
      } else if (strncmp(line, "sslports ", 9) == 0) {
	newProbe->setProbablePorts(SERVICE_TUNNEL_SSL, line + 9, lineno);
      } else if (strncmp(line, "totalwaitms ", 12) == 0) {
	long waitms = strtol(line + 12, NULL, 10);
	if (waitms < 100 || waitms > 300000)
	  fatal("Error on line %d of nmap-service-probes file (%s): bad totalwaitms value.  Must be between 100 and 300000 milliseconds", lineno, filename);
	newProbe->totalwaitms = waitms;
      } else if (strncmp(line, "match ", 6) == 0 || strncmp(line, "softmatch ", 10) == 0) {
	newProbe->addMatch(line, lineno);
      } else fatal("Parse error on line %d of nmap-service-probes file: %s -- unknown directive", lineno, filename);
    }
  }

  assert(newProbe);
  if (newProbe->isNullProbe()) {
    assert(!AP->nullProbe);
    AP->nullProbe = newProbe;
  } else {
    AP->probes.push_back(newProbe);
  }
  
  fclose(fp);
}

// Parses the nmap-service-probes file, and adds each probe to
// the already-created 'probes' vector.
void parse_nmap_service_probes(AllProbes *AP) {
  char filename[256];

  if (nmap_fetchfile(filename, sizeof(filename), "nmap-service-probes") == -1){
    fatal("Service scan requested but I cannot find nmap-service-probes file.  It should be in %s, ~/.nmap/ or .", NMAPDATADIR);
  }

  parse_nmap_service_probe_file(AP, filename);
}

// If the buf (of length buflen) matches one of the regexes in this
// ServiceProbe, returns the details of the match (service name,
// version number if applicable, and whether this is a "soft" match.
// If the buf doesn't match, the serviceName field in the structure
// will be NULL.  The MatchDetails returned is only valid until the
// next time this function is called.  The only exception is that the
// serviceName field can be saved throughought program execution.  If
// no version matched, that field will be NULL. This function may
// return NULL if there are no match lines at all in this probe.
const struct MatchDetails *ServiceProbe::testMatch(const u8 *buf, int buflen) {
  vector<ServiceProbeMatch *>::iterator vi;
  const struct MatchDetails *MD;

  for(vi = matches.begin(); vi != matches.end(); vi++) {
    MD = (*vi)->testMatch(buf, buflen);
    if (MD->serviceName)
      return MD;
  }

  return NULL;
}

AllProbes::AllProbes() {
  nullProbe = NULL;
}

AllProbes::~AllProbes() {
  vector<ServiceProbe *>::iterator vi;

  // Delete all the ServiceProbe's inside the probes vector
  for(vi = probes.begin(); vi != probes.end(); vi++)
    delete *vi;
}

  // Tries to find the probe in this AllProbes class which have the
  // given name and protocol.  It can return the NULL probe.
ServiceProbe *AllProbes::getProbeByName(const char *name, int proto) {
  vector<ServiceProbe *>::iterator vi;

  if (proto == IPPROTO_TCP && nullProbe && strcmp(nullProbe->getName(), name) == 0)
    return nullProbe;

  for(vi = probes.begin(); vi != probes.end(); vi++) {
    if ((*vi)->getProbeProtocol() == proto &&
	strcmp(name, (*vi)->getName()) == 0)
      return *vi;
  }

  return NULL;
}

ServiceNFO::ServiceNFO(AllProbes *newAP) {
  target = NULL;
  probe_matched = NULL;
  niod = NULL;
  probe_state = PROBESTATE_INITIAL;
  portno = proto = 0;
  AP = newAP;
  currentresp = NULL; 
  currentresplen = 0;
  port = NULL;
  product_matched[0] = version_matched[0] = extrainfo_matched[0] = '\0';
  tunnel = SERVICE_TUNNEL_NONE;
  ssl_session = NULL;
  softMatchFound = false;
  servicefplen = servicefpalloc = 0;
  servicefp = NULL;
  memset(&currentprobe_exec_time, 0, sizeof(currentprobe_exec_time));
}

ServiceNFO::~ServiceNFO() {
  if (currentresp) free(currentresp);
  if (servicefp) free(servicefp);
  servicefp = NULL;
  servicefpalloc = servicefplen = 0;
#if HAVE_OPENSSL
  if (ssl_session)
    SSL_SESSION_free((SSL_SESSION*)ssl_session);
  ssl_session=NULL;
#endif
}

  // Adds a character to servicefp.  Takes care of word wrapping if
  // neccessary at the given (wrapat) column.  Chars will only be
  // written if there is enough space.  Oherwise it exits.
void ServiceNFO::addServiceChar(char c, int wrapat) {

  if (servicefpalloc - servicefplen < 6)
    fatal("ServiceNFO::addServiceChar - out of space for servicefp");

  if (servicefplen % (wrapat+1) == wrapat) {
    // we need to start a new line
    memcpy(servicefp + servicefplen, "\nSF:", 4);
    servicefplen += 4;
  }

  servicefp[servicefplen++] = c;
}

// Like addServiceChar, but for a whole zero-terminated string
void ServiceNFO::addServiceString(char *s, int wrapat) {
  while(*s) 
    addServiceChar(*s++, wrapat);
}

// If a service response to a given probeName, this function adds the
// resonse the the fingerprint for that service.  The fingerprint can
// be printed when nothing matches the service.  You can obtain the
// fingerprint (if any) via getServiceFingerprint();
void ServiceNFO::addToServiceFingerprint(const char *probeName, const u8 *resp, 
					 int resplen) {
  int spaceleft = servicefpalloc - servicefplen;
  int servicewrap=74; // Wrap after 74 chars / line
  int respused = MIN(resplen, (o.debugging)? 1300 : 900); // truncate to reasonable size
  // every char could require \xHH escape, plus there is the matter of 
  // "\nSF:" for each line, plus "%r(probename,probelen,"") Oh, and 
  // the SF-PortXXXX-TCP stuff, etc
  int spaceneeded = respused * 5 + strlen(probeName) + 128;  
  int srcidx;
  struct tm *ltime;
  time_t timep;
  char buf[128];
  int len;

  assert(resplen);
  assert(probeName);

  if (servicefplen > (o.debugging? 10000 : 2200))
    return; // it is large enough.

  if (spaceneeded >= spaceleft) {
    spaceneeded = MAX(spaceneeded, 512); // No point in tiny allocations
    spaceneeded += servicefpalloc;

    servicefp = (char *) safe_realloc(servicefp, spaceneeded);
    servicefpalloc = spaceneeded;
  }
  spaceleft = servicefpalloc - servicefplen;

  if (servicefplen == 0) {
    timep = time(NULL);
    ltime = localtime(&timep);
    servicefplen = snprintf(servicefp, spaceleft, "SF-Port%hu-%s:V=%s%s%%D=%d/%d%%Time=%X%%P=%s", portno, proto2ascii(proto, true), NMAP_VERSION, (tunnel == SERVICE_TUNNEL_SSL)? "%T=SSL" : "", ltime->tm_mon + 1, ltime->tm_mday, (int) timep, NMAP_PLATFORM);
  }

  // Note that we give the total length of the response, even though we 
  // may truncate
  len = snprintf(buf, sizeof(buf), "%%r(%s,%X,\"", probeName, resplen);
  addServiceString(buf, servicewrap);

  // Now for the probe response itself ...
  for(srcidx=0; srcidx < respused; srcidx++) {
    // A run of this can take up to 8 chars: "\n  \x20"
    assert( servicefpalloc - servicefplen > 8);
 
   if (isalnum((int)resp[srcidx]))
      addServiceChar((char) resp[srcidx], servicewrap);
    else if (resp[srcidx] == '\0') {
      /* We need to be careful with this, because if it is followed by
	 an ASCII number, PCRE will treat it differently. */
      if (srcidx + 1 >= respused || !isdigit(resp[srcidx + 1]))
	addServiceString("\\0", servicewrap);
      else addServiceString("\\x00", servicewrap);
    } else if (strchr("\\?\"[]().*+$^|", resp[srcidx])) {
      addServiceChar('\\', servicewrap);
      addServiceChar(resp[srcidx], servicewrap);
    } else if (ispunct((int)resp[srcidx])) {
      addServiceChar((char) resp[srcidx], servicewrap);
    } else if (resp[srcidx] == '\r') {
      addServiceString("\\r", servicewrap);
    } else if (resp[srcidx] == '\n') {
      addServiceString("\\n", servicewrap);
    } else if (resp[srcidx] == '\t') {
      addServiceString("\\t", servicewrap);
    } else {
      addServiceChar('\\', servicewrap);
      addServiceChar('x', servicewrap);
      snprintf(buf, sizeof(buf), "%02x", resp[srcidx]);
      addServiceChar(*buf, servicewrap);
      addServiceChar(*(buf+1), servicewrap);
    }
  }

  addServiceChar('"', servicewrap);
  addServiceChar(')', servicewrap);
  assert(servicefpalloc - servicefplen > 1);
  servicefp[servicefplen] = '\0';
}

// Get the service fingerprint.  It is NULL if there is none, such
// as if there was a match before any other probes were finished (or
// if no probes gave back data).  Note that this is plain
// NUL-terminated ASCII data, although the length is optionally
// available anyway.  This function terminates the service fingerprint
// with a semi-colon
const char *ServiceNFO::getServiceFingerprint(int *flen) {

  if (servicefplen == 0) {
    if (flen) *flen = 0;
    return NULL;
  }

  // Ensure we have enough space for the terminating semi-colon and \0
  if (servicefplen + 2 > servicefpalloc) {
    servicefpalloc = servicefplen + 20;
    servicefp = (char *) safe_realloc(servicefp, servicefpalloc);
  }

  if (flen) *flen = servicefplen + 1;
  // We terminate with a semi-colon, which is never wrapped.
  servicefp[servicefplen] = ';';
  servicefp[servicefplen + 1] = '\0';
  return servicefp;
}

ServiceProbe *ServiceNFO::currentProbe() {
  if (probe_state == PROBESTATE_INITIAL) {
    return nextProbe(true);
  } else if (probe_state == PROBESTATE_NULLPROBE) {
    assert(AP->nullProbe);
    return AP->nullProbe;
  } else if (probe_state == PROBESTATE_MATCHINGPROBES || 
	     probe_state == PROBESTATE_NONMATCHINGPROBES) {
    return *current_probe;
  }
  return NULL;
}

// computes the next probe to test, and ALSO CHANGES currentProbe() to
// that!  If newresp is true, the old response info will be lost and
// invalidated.  Otherwise it remains as if it had been received by
// the current probe (useful after a NULL probe).
ServiceProbe *ServiceNFO::nextProbe(bool newresp) {
bool dropdown = false;

// This invalidates the probe response string if any
 if (newresp) { 
   if (currentresp) free(currentresp);
   currentresp = NULL; currentresplen = 0;
 }

 if (probe_state == PROBESTATE_INITIAL) {
   probe_state = PROBESTATE_NULLPROBE;
   // This is the very first probe -- so we try to use the NULL probe
   // but obviously NULL probe only works with TCP
   if (proto == IPPROTO_TCP && AP->nullProbe)
     return AP->nullProbe;
   
   // No valid NULL probe -- we'll drop to the next state
 }
 
 if (probe_state == PROBESTATE_NULLPROBE) {
   // There can only be one (or zero) NULL probe.  So now we go through the
   // list looking for matching probes
   probe_state = PROBESTATE_MATCHINGPROBES;
   dropdown = true;
   current_probe = AP->probes.begin();
 }

 if (probe_state == PROBESTATE_MATCHINGPROBES) {
   if (!dropdown && current_probe != AP->probes.end()) current_probe++;
   while (current_probe != AP->probes.end()) {
     // For the first run, we only do probes that match this port number
     if ((proto == (*current_probe)->getProbeProtocol()) && 
	 (*current_probe)->portIsProbable(tunnel, portno)) {
       // This appears to be a valid probe.  Let's do it!
       return *current_probe;
     }
     current_probe++;
   }
   // Tried all MATCHINGPROBES -- now we must move to nonmatching
   probe_state = PROBESTATE_NONMATCHINGPROBES;
   dropdown = true;
   current_probe = AP->probes.begin();
 }

 if (probe_state == PROBESTATE_NONMATCHINGPROBES) {
   if (!dropdown && current_probe != AP->probes.end()) current_probe++;
   while (current_probe != AP->probes.end()) {
     // The protocol must be right, it must be a nonmatching port ('cause we did thos),
     // and we better either have no soft match yet, or the soft service match must
     // be available via this probe.
     if ((proto == (*current_probe)->getProbeProtocol()) && 
	 !(*current_probe)->portIsProbable(tunnel, portno) &&
	 (!softMatchFound || (*current_probe)->serviceIsPossible(probe_matched))) {
       // Valid, probe.  Let's do it!
       return *current_probe;
     }
     current_probe++;
   }

   // Tried all NONMATCHINGPROBES -- we're finished
   probe_state = (softMatchFound)? PROBESTATE_FINISHED_SOFTMATCHED : PROBESTATE_FINISHED_NOMATCH;
   return NULL; 
 }

 fatal("ServiceNFO::nextProbe called for probe in state (%d)", (int) probe_state);
 return NULL;
}

  // Resets the probes back to the first one. One case where this is useful is
  // when SSL is detected -- we redo all probes through SSL.  If freeFP, any
  // service fingerprint is freed too.
void ServiceNFO::resetProbes(bool freefp) {

  if (currentresp) free(currentresp);

  if (freefp) {
    if (servicefp) { free(servicefp); servicefp = NULL; }
    servicefplen = servicefpalloc = 0;
  }

  currentresp = NULL; currentresplen = 0;

  probe_state = PROBESTATE_INITIAL;
}


int ServiceNFO::currentprobe_timemsleft(const struct timeval *now) {
  int timeused, timeleft;

  if (now)
    timeused = TIMEVAL_MSEC_SUBTRACT(*now, currentprobe_exec_time);
  else {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeused = TIMEVAL_MSEC_SUBTRACT(tv, currentprobe_exec_time);
  }

  timeleft = currentProbe()->totalwaitms - timeused;
  return (timeleft < 0)? 0 : timeleft;
}

void ServiceNFO::appendtocurrentproberesponse(const u8 *respstr, int respstrlen) {
  currentresp = (u8 *) realloc(currentresp, currentresplen + respstrlen);
  assert(currentresp);
  memcpy(currentresp + currentresplen, respstr, respstrlen);
  currentresplen += respstrlen;
}

// Get the full current response string.  Note that this pointer is 
// INVALIDATED if you call appendtocurrentproberesponse() or nextProbe()
u8 *ServiceNFO::getcurrentproberesponse(int *respstrlen) {
  *respstrlen = currentresplen;
  return currentresp;
}


ServiceGroup::ServiceGroup(vector<Target *> &Targets, AllProbes *AP) {
  unsigned int targetno;
  ServiceNFO *svc;
  Port *nxtport;
  int desired_par;
  struct timeval now;
  num_hosts_timedout = 0;
  gettimeofday(&now, NULL);

  for(targetno = 0 ; targetno < Targets.size(); targetno++) {
    nxtport = NULL;
    if (Targets[targetno]->timedOut(&now)) {
      num_hosts_timedout++;
      continue;
    }
    while((nxtport = Targets[targetno]->ports.nextPort(nxtport, 0, PORT_OPEN,
			      true))) {
      svc = new ServiceNFO(AP);
      svc->target = Targets[targetno];
      svc->portno = nxtport->portno;
      svc->proto = nxtport->proto;
      svc->port = nxtport;
      services_remaining.push_back(svc);
    }
  }

  /* Use a whole new loop for PORT_OPENFILTERED so that we try all the
     known open ports first before bothering with this speculative
     stuff */
  for(targetno = 0 ; targetno < Targets.size(); targetno++) {
    nxtport = NULL;
    if (Targets[targetno]->timedOut(&now)) {
      continue;
    }
    while((nxtport = Targets[targetno]->ports.nextPort(nxtport, 0, 
						       PORT_OPENFILTERED, true))) {
      svc = new ServiceNFO(AP);
      svc->target = Targets[targetno];
      svc->portno = nxtport->portno;
      svc->proto = nxtport->proto;
      svc->port = nxtport;
      services_remaining.push_back(svc);
    }
  }

  SPM = new ScanProgressMeter("Service scan");
  desired_par = 1;
  if (o.timing_level == 3) desired_par = 10;
  if (o.timing_level == 4) desired_par = 15;
  if (o.timing_level >= 5) desired_par = 20;
  // TODO: Come up with better ways to determine ideal_services
  ideal_parallelism = box(o.min_parallelism, o.max_parallelism? o.max_parallelism : 100, desired_par);
}

ServiceGroup::~ServiceGroup() {
  list<ServiceNFO *>::iterator i;

  for(i = services_finished.begin(); i != services_finished.end(); i++)
    delete *i;

  for(i = services_in_progress.begin(); i != services_in_progress.end(); i++)
    delete *i;

  for(i = services_remaining.begin(); i != services_remaining.end(); i++)
    delete *i;

  delete SPM;
}

  // Sends probe text to an open connection.  In the case of a NULL probe, there
  // may be no probe text
  static int send_probe_text(nsock_pool nsp, nsock_iod nsi, ServiceNFO *svc,
			     ServiceProbe *probe) {
    const u8 *probestring;
    int probestringlen;

    assert(probe);
    if (probe->isNullProbe())
      return 0; // No need to send anything for a NULL probe;
    probestring = probe->getProbeString(&probestringlen);
    assert(probestringlen > 0);
    // Now we write the string to the IOD
    nsock_write(nsp, nsi, servicescan_write_handler, svc->currentprobe_timemsleft(), svc,
		(const char *) probestring, probestringlen);
    return 0;
  }

// This simple helper function is used to start the next probe.  If
// the probe exists, execution begins (and the previous one is cleaned
// up if neccessary) .  Otherwise, the service is listed as finished
// and moved to the finished list.  If you pass 'true' for alwaysrestart, a
// new connection will be made even if the previous probe was the NULL probe.
// You would do this, for example, if the other side has closed the connection.
static void startNextProbe(nsock_pool nsp, nsock_iod nsi, ServiceGroup *SG, 
			   ServiceNFO *svc, bool alwaysrestart) {
  bool isInitial = svc->probe_state == PROBESTATE_INITIAL;
  ServiceProbe *probe = svc->currentProbe();
  struct sockaddr_storage ss;
  size_t ss_len;

  if (!alwaysrestart && probe->isNullProbe()) {
    // The difference here is that we can reuse the same (TCP) connection
    // if the last probe was the NULL probe.
    probe = svc->nextProbe(false);
    if (probe) {
      svc->currentprobe_exec_time = *nsock_gettimeofday();
      send_probe_text(nsp, nsi, svc, probe);
      nsock_read(nsp, nsi, servicescan_read_handler, 
		 svc->currentprobe_timemsleft(nsock_gettimeofday()), svc);
    } else {
      // Should only happen if someone has a highly perverse nmap-service-probes
      // file.  Null scan should generally never be the only probe.
      end_svcprobe(nsp, (svc->softMatchFound)? PROBESTATE_FINISHED_SOFTMATCHED : PROBESTATE_FINISHED_NOMATCH, SG, svc, NULL);
    }
  } else {
    // The finisehd probe was not a NULL probe.  So we close the
    // connection, and if further probes are available, we launch the
    // next one.
    if (!isInitial)
      probe = svc->nextProbe(true); // if was initial, currentProbe() returned the right one to execute.
    if (probe) {
      // For a TCP probe, we start by requesting a new connection to the target
      if (svc->proto == IPPROTO_TCP) {
	nsi_delete(nsi, NSOCK_PENDING_SILENT);
	if ((svc->niod = nsi_new(nsp, svc)) == NULL) {
	  fatal("Failed to allocate Nsock I/O descriptor in startNextProbe()");
	}
	svc->target->TargetSockAddr(&ss, &ss_len);
	if (svc->tunnel == SERVICE_TUNNEL_NONE) {
	  nsock_connect_tcp(nsp, svc->niod, servicescan_connect_handler, 
			    DEFAULT_CONNECT_TIMEOUT, svc, 
			    (struct sockaddr *) &ss, ss_len,
			    svc->portno);
	} else {
	  assert(svc->tunnel == SERVICE_TUNNEL_SSL);
	  nsock_connect_ssl(nsp, svc->niod, servicescan_connect_handler, 
			    DEFAULT_CONNECT_SSL_TIMEOUT, svc, 
			    (struct sockaddr *) &ss,
			    ss_len, svc->portno, svc->ssl_session);
	}
      } else {
	assert(svc->proto == IPPROTO_UDP);
	/* Can maintain the same UDP "connection" */
	svc->currentprobe_exec_time = *nsock_gettimeofday();
	send_probe_text(nsp, nsi, svc, probe);
	// Now let us read any results
	nsock_read(nsp, nsi, servicescan_read_handler, 
		   svc->currentprobe_timemsleft(nsock_gettimeofday()), svc);
      }
    } else {
      // No more probes remaining!  Failed to match
      nsi_delete(nsi, NSOCK_PENDING_SILENT);
      end_svcprobe(nsp, (svc->softMatchFound)? PROBESTATE_FINISHED_SOFTMATCHED : PROBESTATE_FINISHED_NOMATCH, SG, svc, NULL);
    }
  }
  return;
}

/* Sometimes the normal service scan will detect a
   tunneling/encryption protocol such as SSL.  Instead of just
   reporting "ssl", we can make an SSL connection and try to determine
   the service that is really sitting behind the SSL.  This function
   will take a service that has just been detected (hard match only),
   and see if we can dig deeper through tunneling.  Nonzero is
   returned if we can do more.  Otherwise 0 is returned and the caller
   should end the service with its successful match.  If the tunnel
   results can be determined with no more effort, 0 is also returned.
   For example, a service that already matched as "ssl/ldap" will be
   chaned to "ldap" with the tunnel being SSL and 0 will be returned.
   That is a special case.
*/

static int scanThroughTunnel(nsock_pool nsp, nsock_iod nsi, ServiceGroup *SG, 
			     ServiceNFO *svc) {

  if (strncmp(svc->probe_matched, "ssl/", 4) == 0) {
    /* The service has been detected without having to make an SSL connection */
    svc->tunnel = SERVICE_TUNNEL_SSL;
    svc->probe_matched += 4;
    return 0;
  }

#ifndef HAVE_OPENSSL
  return 0;
#endif

  if (svc->tunnel != SERVICE_TUNNEL_NONE) {
    // Another tunnel type has already been tried.  Let's not go recursive.
    return 0;
  }

  if (svc->proto != IPPROTO_TCP || 
      !svc->probe_matched || strcmp(svc->probe_matched, "ssl") != 0)
    return 0; // Not SSL

  // Alright!  We are going to start the tests over using SSL
  // printf("DBG: Found SSL service on %s:%hi - starting SSL scan\n", svc->target->NameIP(), svc->portno);
  svc->tunnel = SERVICE_TUNNEL_SSL;
  svc->probe_matched = NULL;
  svc->product_matched[0] = svc->version_matched[0] = svc->extrainfo_matched[0] = '\0';
  svc->softMatchFound = false;
   svc->resetProbes(true);
  startNextProbe(nsp, nsi, SG, svc, true);
  return 1;
}

/* Prints completion estimates and the like when appropriate */
static void considerPrintingStats(ServiceGroup *SG) {
  /* Perhaps this should be made more complex, but I suppose it should be
     good enough for now. */
  if (SG->SPM->mayBePrinted(nsock_gettimeofday())) {
    SG->SPM->printStatsIfNeccessary(SG->services_finished.size() / ((double)SG->services_remaining.size() + SG->services_in_progress.size() + SG->services_finished.size()), nsock_gettimeofday());
  }
}

/* Check if target is done (no more probes remaining for it in service group),
   and responds appropriately if so */
static void handleHostIfDone(ServiceGroup *SG, Target *target) {
  list<ServiceNFO *>::iterator svcI;
  bool found = false;

  for(svcI = SG->services_in_progress.begin(); 
      svcI != SG->services_in_progress.end(); svcI++) {
    if ((*svcI)->target == target) {
      found = true;
      break;
    }
  }

  for(svcI = SG->services_remaining.begin(); 
      !found && svcI != SG->services_remaining.end(); svcI++) {
    if ((*svcI)->target == target) {
      found = true;
      break;
    }
  }

  if (!found) {
    target->stopTimeOutClock(nsock_gettimeofday());
    if (target->timedOut(NULL)) {
      SG->num_hosts_timedout++;
    }
  }
}

// A simple helper function to cancel further work on a service and
// set it to the given probe_state pass NULL for nsi if you don't want
// it to be deleted (for example, if you already have done so).
void end_svcprobe(nsock_pool nsp, enum serviceprobestate probe_state, ServiceGroup *SG, ServiceNFO *svc, nsock_iod nsi) {
  list<ServiceNFO *>::iterator member;
  Target *target = svc->target;

  svc->probe_state = probe_state;
  member = find(SG->services_in_progress.begin(), SG->services_in_progress.end(),
		  svc);
  assert(*member);
  SG->services_in_progress.erase(member);
  SG->services_finished.push_back(svc);

  considerPrintingStats(SG);

  if (nsi) {
    nsi_delete(nsi, NSOCK_PENDING_SILENT);
  }

  handleHostIfDone(SG, target);
  return;
}

void servicescan_connect_handler(nsock_pool nsp, nsock_event nse, void *mydata) {
  nsock_iod nsi = nse_iod(nse);
  enum nse_status status = nse_status(nse);
  enum nse_type type = nse_type(nse);
  ServiceNFO *svc = (ServiceNFO *) mydata;
  ServiceProbe *probe = svc->currentProbe();
  ServiceGroup *SG = (ServiceGroup *) nsp_getud(nsp);

  assert(type == NSE_TYPE_CONNECT || type == NSE_TYPE_CONNECT_SSL);

  if (svc->target->timedOut(nsock_gettimeofday())) {
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
  } else if (status == NSE_STATUS_SUCCESS) {

#if HAVE_OPENSSL
    // Snag our SSL_SESSION from the nsi for use in subsequent connections.
    if (nsi_checkssl(nsi)) {
      if ( svc->ssl_session ) {
	if (svc->ssl_session == (SSL_SESSION *)(nsi_get0_ssl_session(nsi))) {
	  //nada
	} else {
          SSL_SESSION_free((SSL_SESSION*)svc->ssl_session);
          svc->ssl_session = (SSL_SESSION *)(nsi_get1_ssl_session(nsi));
        }
      } else {
        svc->ssl_session = (SSL_SESSION *)(nsi_get1_ssl_session(nsi));
      }
    }
#endif

    // Yeah!  Connection made to the port.  Send the appropriate probe
    // text (if any is needed -- might be NULL probe)
    svc->currentprobe_exec_time = *nsock_gettimeofday();
    send_probe_text(nsp, nsi, svc, probe);
    // Now let us read any results
    nsock_read(nsp, nsi, servicescan_read_handler, svc->currentprobe_timemsleft(nsock_gettimeofday()), svc);
  } else if (status == NSE_STATUS_TIMEOUT || status == NSE_STATUS_ERROR) {
      // This is not good.  The connect() really shouldn't generally
      // be timing out like that.  We'll mark this svc as incomplete
      // and move it to the finished bin.
    if (o.debugging)
      error("Got nsock CONNECT response with status %s - aborting this service", nse_status2str(status));
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
  } else if (status == NSE_STATUS_KILL) {
    /* User probablby specified host_timeout and so the service scan is
       shutting down */
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
    return;
  } else fatal("Unexpected nsock status (%d) returned for connection attempt", (int) status);

  // We may have room for more pr0bes!
  launchSomeServiceProbes(nsp, SG);

  return;
}

void servicescan_write_handler(nsock_pool nsp, nsock_event nse, void *mydata) {
  enum nse_status status = nse_status(nse);
  nsock_iod nsi;
  ServiceNFO *svc = (ServiceNFO *)mydata;
  ServiceGroup *SG;
  int err;

  SG = (ServiceGroup *) nsp_getud(nsp);
  nsi = nse_iod(nse);

  if (svc->target->timedOut(nsock_gettimeofday())) {
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
    return;
  }

  if (status == NSE_STATUS_SUCCESS)
    return;

  if (status == NSE_STATUS_KILL) {
    /* User probablby specified host_timeout and so the service scan is
       shutting down */
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
    return;
  }

  if (status == NSE_STATUS_ERROR) {
	err = nse_errorcode(nse);
	error("Got nsock WRITE error #%d (%s)\n", err, strerror(err));
  }

  // Uh-oh.  Some sort of write failure ... maybe the connection closed
  // on us unexpectedly?
  if (o.debugging) 
    error("Got nsock WRITE response with status %s - aborting this service", nse_status2str(status));
  end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
  
  // We may have room for more pr0bes!
  launchSomeServiceProbes(nsp, SG);
  
  return;
}

/* Called if data is read for a service.  Checks if the service is UDP and
   the port is in state PORT_OPENFILTERED.  If that is the case, the state
   is changed to PORT_OPEN, as only an open port would return a response */
static void adjustPortStateIfNeccessary(ServiceNFO *svc) {
  if (svc->proto != IPPROTO_UDP)
    return;

  if (svc->port->state == PORT_OPENFILTERED) {
    svc->target->ports.addPort(svc->portno, IPPROTO_UDP, NULL, PORT_OPEN);
  }

  return;
}

void servicescan_read_handler(nsock_pool nsp, nsock_event nse, void *mydata) {
  nsock_iod nsi = nse_iod(nse);
  enum nse_status status = nse_status(nse);
  enum nse_type type = nse_type(nse);
  ServiceNFO *svc = (ServiceNFO *) mydata;
  ServiceProbe *probe = svc->currentProbe();
  ServiceGroup *SG = (ServiceGroup *) nsp_getud(nsp);
  const u8 *readstr;
  int readstrlen;
  const struct MatchDetails *MD;
  bool nullprobecheat = false; // We cheated and found a match in the NULL probe to a non-null-probe response

  assert(type == NSE_TYPE_READ);

  if (svc->target->timedOut(nsock_gettimeofday())) {
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
  } else if (status == NSE_STATUS_SUCCESS) {
    // w00p, w00p, we read something back from the port.
    readstr = (u8 *) nse_readbuf(nse, &readstrlen);
    adjustPortStateIfNeccessary(svc); /* A UDP response means PORT_OPENFILTERED is really PORT_OPEN */
    svc->appendtocurrentproberesponse(readstr, readstrlen);
    // now get the full version
    readstr = svc->getcurrentproberesponse(&readstrlen);
    // Now let us try to match it.
    MD = probe->testMatch(readstr, readstrlen);

    // Sometimes a service doesn't respond quickly enough to the NULL
    // scan, even though it would have match.  In that case, Nmap can
    // end up tediously going through every probe without finding a
    // match.  So we test the NULL probe matches if the probe-specific
    // matches fail
    if (!MD && !probe->isNullProbe() && probe->getProbeProtocol() == IPPROTO_TCP && svc->AP->nullProbe) {
      MD = svc->AP->nullProbe->testMatch(readstr, readstrlen);
      nullprobecheat = true;
    }

    if (MD && MD->serviceName) {
      // WOO HOO!!!!!!  MATCHED!  But might be soft
      if (MD->isSoft && svc->probe_matched) {
	if (strcmp(svc->probe_matched, MD->serviceName) != 0)
	  error("WARNING:  service %s:%hi had allready soft-matched %s, but now soft-matched %s; ignoring second value\n", svc->target->NameIP(), svc->portno, svc->probe_matched, MD->serviceName);
	// No error if its the same - that happens frequently.  For
	// example, if we read more data for the same probe response
	// it will probably still match.
      } else {
	if (o.debugging > 1)
	  if (MD->product || MD->version || MD->info)
	    printf("Service scan %smatch: %s:%hi is %s%s.  Version: |%s|%s|%s|\n", nullprobecheat? "NULL-CHEAT " : "", 
		   svc->target->NameIP(), svc->portno, (svc->tunnel == SERVICE_TUNNEL_SSL)? "SSL/" : "", 
		   MD->serviceName, (MD->product)? MD->product : "", (MD->version)? MD->version : "", 
		   (MD->info)? MD->info : "");
	  else
	    printf("Service scan %s%s match: %s:%hi is %s%s\n", nullprobecheat? "NULL-CHEAT " : "", 
		   (MD->isSoft)? "soft" : "hard", svc->target->NameIP(), 
		   svc->portno, (svc->tunnel == SERVICE_TUNNEL_SSL)? "SSL/" : "", MD->serviceName);
	svc->probe_matched = MD->serviceName;
	if (MD->product)
	  Strncpy(svc->product_matched, MD->product, sizeof(svc->product_matched));
	if (MD->version) 
	  Strncpy(svc->version_matched, MD->version, sizeof(svc->version_matched));
	if (MD->info) 
	  Strncpy(svc->extrainfo_matched, MD->info, sizeof(svc->extrainfo_matched));
	svc->softMatchFound = MD->isSoft;
	if (!svc->softMatchFound) {
	  // We might be able to continue scan through a tunnel protocol 
	  // like SSL
	  if (scanThroughTunnel(nsp, nsi, SG, svc) == 0) 
	    end_svcprobe(nsp, PROBESTATE_FINISHED_HARDMATCHED, SG, svc, nsi);
	}
      }
    }

    if (!MD || !MD->serviceName || MD->isSoft) {
      // Didn't match... maybe reading more until timeout will help
      // TODO: For efficiency I should be able to test if enough data
      // has been received rather than always waiting for the reading
      // to timeout.  For now I'll limit it to 4096 bytes just to
      // avoid reading megs from services like chargen.  But better
      // approach is needed.
      if (svc->currentprobe_timemsleft() > 0 && readstrlen < 4096) { 
	nsock_read(nsp, nsi, servicescan_read_handler, svc->currentprobe_timemsleft(), svc);
      } else {
	// Failed -- lets go to the next probe.
	if (readstrlen > 0)
	  svc->addToServiceFingerprint(svc->currentProbe()->getName(), readstr, 
				       readstrlen);
	startNextProbe(nsp, nsi, SG, svc, false);
      }
    }
  } else if (status == NSE_STATUS_TIMEOUT) {
    // Failed to read enough to make a match in the given amount of time.  So we
    // move on to the next probe.  If this was a NULL probe, we can simply
    // send the new probe text immediately.  Otherwise we make a new connection.

    readstr = svc->getcurrentproberesponse(&readstrlen);
    if (readstrlen > 0)
      svc->addToServiceFingerprint(svc->currentProbe()->getName(), readstr, 
				   readstrlen);
    startNextProbe(nsp, nsi, SG, svc, false);
    
  } else if (status == NSE_STATUS_EOF) {
    // The jerk closed on us during read request!
    // If this was during the NULL probe, let's (for now) assume
    // the port is TCP wrapped.  Otherwise, we'll treat it as a nomatch
    readstr = svc->getcurrentproberesponse(&readstrlen);
    if (readstrlen > 0)
      svc->addToServiceFingerprint(svc->currentProbe()->getName(), readstr, 
				   readstrlen);
    if (probe->isNullProbe() && readstrlen == 0) {
      // TODO:  Perhaps should do further verification before making this assumption
      end_svcprobe(nsp, PROBESTATE_FINISHED_TCPWRAPPED, SG, svc, nsi);
    } else {

      // Perhaps this service didn't like the particular probe text.
      // We'll try the next one
      startNextProbe(nsp, nsi, SG, svc, true);
    }
  } else if (status == NSE_STATUS_ERROR) {
    // Errors might happen in some cases ... I'll worry about later
    int err = nse_errorcode(nse);
    switch(err) {
    case ECONNRESET:
    case ECONNREFUSED: // weird to get this on a connected socket (shrug) but 
                       // BSD sometimes gives it
      // Jerk hung up on us.  Probably didn't like our probe.  We treat it as with EOF above.
      if (probe->isNullProbe()) {
	// TODO:  Perhaps should do further verification before making this assumption
	end_svcprobe(nsp, PROBESTATE_FINISHED_TCPWRAPPED, SG, svc, nsi);
      } else {
	// Perhaps this service didn't like the particular probe text.  We'll try the 
	// next one
	startNextProbe(nsp, nsi, SG, svc, true);
      }
      break;
    case EHOSTUNREACH:
      // That is funny.  The port scanner listed the port as open.  Maybe it got unplugged, or firewalled us, or did
      // something else nasty during the scan.  Shrug.  I'll give up on this port
      end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
      break;
#ifndef WIN32
    case EPIPE:
#endif
    case EIO:
      // Usually an SSL error of some sort (those are presently
      // hardcoded to EIO).  I'll just try the next probe.
      startNextProbe(nsp, nsi, SG, svc, true);
      break;
    default:
      fatal("Unexpected error in NSE_TYPE_READ callback.  Error code: %d (%s)", err,
	    strerror(err));
    }
  } else if (status == NSE_STATUS_KILL) {
    /* User probablby specified host_timeout and so the service scan is 
       shutting down */
    end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, nsi);
    return;
  } else {
    fatal("Unexpected status (%d) in NSE_TYPE_READ callback.", (int) status);
  }

  // We may have room for more pr0bes!
  launchSomeServiceProbes(nsp, SG);
  return;
}

// This is passed a completed ServiceGroup which contains the scanning results for every service.
// The function iterates through each finished service and adds the results to Target structure for
// Nmap to output later.

static void processResults(ServiceGroup *SG) {
list<ServiceNFO *>::iterator svc;

 for(svc = SG->services_finished.begin(); svc != SG->services_finished.end(); svc++) {
   if ((*svc)->probe_state == PROBESTATE_FINISHED_HARDMATCHED) {
     (*svc)->port->setServiceProbeResults((*svc)->probe_state, 
					  (*svc)->probe_matched,
					  (*svc)->tunnel,
					  *(*svc)->product_matched? (*svc)->product_matched : NULL, 
					  *(*svc)->version_matched? (*svc)->version_matched : NULL, 
					  *(*svc)->extrainfo_matched? (*svc)->extrainfo_matched : NULL, 
					  NULL);

   } else if ((*svc)->probe_state == PROBESTATE_FINISHED_SOFTMATCHED) {
    (*svc)->port->setServiceProbeResults((*svc)->probe_state, 
					  (*svc)->probe_matched,
					 (*svc)->tunnel,
					  NULL, NULL, NULL, 
					 (*svc)->getServiceFingerprint(NULL));

   }  else if ((*svc)->probe_state == PROBESTATE_FINISHED_NOMATCH) {
     if ((*svc)->getServiceFingerprint(NULL))
       (*svc)->port->setServiceProbeResults((*svc)->probe_state, NULL,
					    (*svc)->tunnel, NULL, NULL, NULL, 
					    (*svc)->getServiceFingerprint(NULL));
   }
 }
}


// This function consults the ServiceGroup to determine whether any
// more probes can be launched at this time.  If so, it determines the
// appropriate ones and then starts them up.
int launchSomeServiceProbes(nsock_pool nsp, ServiceGroup *SG) {
  ServiceNFO *svc;
  ServiceProbe *nextprobe;
  struct sockaddr_storage ss;
  size_t ss_len;

  while (SG->services_in_progress.size() < SG->ideal_parallelism &&
	 !SG->services_remaining.empty()) {
    // Start executing a probe from the new list and move it to in_progress
    svc = SG->services_remaining.front();
    if (svc->target->timedOut(nsock_gettimeofday())) {
      end_svcprobe(nsp, PROBESTATE_INCOMPLETE, SG, svc, NULL);
      continue;
    }
    nextprobe = svc->nextProbe(true);
    // We start by requesting a connection to the target
    if ((svc->niod = nsi_new(nsp, svc)) == NULL) {
      fatal("Failed to allocate Nsock I/O descriptor in launchSomeServiceProbes()");
    }
    if (o.debugging > 1) {
      printf("Starting probes against new service: %s:%hi (%s)\n", svc->target->targetipstr(), svc->portno, proto2ascii(svc->proto));
    }
    svc->target->TargetSockAddr(&ss, &ss_len);
    if (svc->proto == IPPROTO_TCP)
      nsock_connect_tcp(nsp, svc->niod, servicescan_connect_handler, 
			DEFAULT_CONNECT_TIMEOUT, svc, 
			(struct sockaddr *)&ss, ss_len,
			svc->portno);
    else {
      assert(svc->proto == IPPROTO_UDP);
      nsock_connect_udp(nsp, svc->niod, servicescan_connect_handler, 
			svc, (struct sockaddr *) &ss, ss_len,
			svc->portno);
    }
    // Now remove it from the remaining service list
    SG->services_remaining.pop_front();
    // And add it to the in progress list
    SG->services_in_progress.push_back(svc);
  }
  return 0;
}

/* Start the timeout clocks of any targets that have probes.  Assumes
   that this is called before any probes have been launched (so they
   are all in services_remaining */
static void startTimeOutClocks(ServiceGroup *SG) {
  list<ServiceNFO *>::iterator svcI;
  Target *target = NULL;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  for(svcI = SG->services_remaining.begin(); 
      svcI != SG->services_remaining.end(); svcI++) {
    target = (*svcI)->target;
    if (!target->timeOutClockRunning())
      target->startTimeOutClock(&tv);
  }
}

/* Execute a service fingerprinting scan against all open ports of the
   Targets specified. */
int service_scan(vector<Target *> &Targets) {
  // int service_scan(Target *targets[], int num_targets)
  static AllProbes *AP;
  ServiceGroup *SG;
  nsock_pool nsp;
  struct timeval now;
  int timeout;
  enum nsock_loopstatus looprc;
  time_t starttime;
  struct timeval starttv;

  if (Targets.size() == 0)
    return 1;

  if (!AP) {
    AP = new AllProbes();
    parse_nmap_service_probes(AP);
  }


  // Now I convert the targets into a new ServiceGroup
  SG = new ServiceGroup(Targets, AP);
  startTimeOutClocks(SG);

  if (SG->services_remaining.size() == 0) {
    delete SG;
    return 1;
  }
  
  gettimeofday(&starttv, NULL);
  starttime = time(NULL);
  if (o.verbose) {
    char targetstr[128];
    struct tm *tm = localtime(&starttime);
    bool plural = (Targets.size() != 1);
    if (!plural) {
      (*(Targets.begin()))->NameIP(targetstr, sizeof(targetstr));
    } else snprintf(targetstr, sizeof(targetstr), "%d hosts", Targets.size());

    log_write(LOG_STDOUT, "Initiating service scan against %d %s on %s at %02d:%02d\n", 
	      SG->services_remaining.size(), 
	      (SG->services_remaining.size() == 1)? "service" : "services", 
	      targetstr, tm->tm_hour, tm->tm_min);
  }

  // Lets create a nsock pool for managing all the concurrent probes
  // Store the servicegroup in there for availability in callbacks
  if ((nsp = nsp_new(SG)) == NULL) {
    fatal("service_scan() failed to create new nsock pool.");
  }

  if (o.versionTrace()) {
    nsp_settrace(nsp, 5, o.getStartTime());
  }

  launchSomeServiceProbes(nsp, SG);

  // How long do we have befor timing out?
  gettimeofday(&now, NULL);
  timeout = -1;

  // OK!  Lets start our main loop!
  looprc = nsock_loop(nsp, timeout);
  if (looprc == NSOCK_LOOP_ERROR) {
    int err = nsp_geterrorcode(nsp);
    fatal("Unexpected nsock_loop error.  Error code %d (%s)", err, strerror(err));
  }

  nsp_delete(nsp);

  if (o.verbose) {
    gettimeofday(&now, NULL);
    if (SG->num_hosts_timedout == 0)
      log_write(LOG_STDOUT, "The service scan took %.2fs to scan %d %s on %d %s.\n", 
		TIMEVAL_MSEC_SUBTRACT(now, starttv) / 1000.0, 
		SG->services_finished.size(),  
		(SG->services_finished.size() == 1)? "service" : "services", 
		Targets.size(), (Targets.size() == 1)? "host" : "hosts");
    else log_write(LOG_STDOUT, 
		   "Finished service scan in %.2fs, but %d %s timed out.\n", 
		   TIMEVAL_MSEC_SUBTRACT(now, starttv) / 1000.0, 
		   SG->num_hosts_timedout, 
		   (SG->num_hosts_timedout == 1)? "host" : "hosts");
  }

  // Yeah - done with the service scan.  Now I go through the results
  // discovered, store the important info away, and free up everything
  // else.
  processResults(SG);

  delete SG;

  return 0;
}

