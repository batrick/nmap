#include "osscan.h"

extern struct ops o;

FingerPrint *get_fingerprint(struct hoststruct *target, struct seq_info *si) {
FingerPrint *FP = NULL, *FPtmp = NULL;
FingerPrint *FPtests[8];
struct AVal *seq_AVs;
int last;
struct ip *ip;
struct tcphdr *tcp;
struct timeval t1,t2;
int i;
struct hostent *myhostent = NULL;
unsigned int localnet, netmask;
pcap_t *pd;
char myname[513];
int rawsd;
int tries = 0;
int newcatches;
int current_port = 0;
int testsleft;
int testno;
int  timeout;
unsigned long sequence_base = rand() + rand();
unsigned int openport;
int bytes;
unsigned int closedport;
struct port *tport;
char *p;
int decoy;
struct bpf_program fcode;
char err0r[PCAP_ERRBUF_SIZE];
char filter[512];
double seq_inc_sum = 0;
unsigned long  seq_avg_inc = 0;
unsigned long seq_gcd = 1;
unsigned long seq_diffs[NUM_SEQ_SAMPLES];

/* Init our raw socket */
 if ((rawsd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0 )
   pfatal("socket trobles in super_scan");
 unblock_socket(rawsd);
 
 /* Do we have a correct source address? */
 if (!target->source_ip.s_addr) {
   if (gethostname(myname, MAXHOSTNAMELEN) != 0 &&
       !((myhostent = gethostbyname(myname))))
     fatal("Your system is messed up.\n");
   memcpy(&target->source_ip, myhostent->h_addr_list[0], sizeof(struct in_addr));
   if (o.debugging || o.verbose)
     printf("We skillfully deduced that your address is %s\n",
	    inet_ntoa(target->source_ip));
 }
 /* Now for the pcap opening nonsense ... */
 /* Note that the snaplen is 152 = 64 byte max IPhdr + 24 byte max link_layer
  * header + 64 byte max TCP header.
  */

if (!(pd = pcap_open_live(target->device, 152,  (o.spoofsource)? 1 : 0, (target->to.timeout + 500)/ 1000, err0r)))
  fatal("pcap_open_live: %s", err0r);

if (o.debugging)
  printf("Wait time is %d\n", (target->to.timeout +500)/1000);

if (pcap_lookupnet(target->device, &localnet, &netmask, err0r) < 0)
  fatal("Failed to lookup device subnet/netmask: %s", err0r);
 p = strdup(inet_ntoa(target->host));

sprintf(filter, "(icmp and dst host %s) or (tcp and src host %s and dst host %s)", inet_ntoa(target->source_ip), p, inet_ntoa(target->source_ip));
 free(p);
 if (o.debugging)
   printf("Packet capture filter: %s\n", filter);
 if (pcap_compile(pd, &fcode, filter, 0, netmask) < 0)
   fatal("Error compiling our pcap filter: %s\n", pcap_geterr(pd));
 if (pcap_setfilter(pd, &fcode) < 0 )
   fatal("Failed to set the pcap filter: %s\n", pcap_geterr(pd));

 /* Lets find an open port to used */
 openport = -1;
 for(tport = target->ports; tport; tport = tport->next) {
   if (tport->state == PORT_OPEN && tport->proto == IPPROTO_TCP &&
       tport->confidence == CONF_HIGH) {   
     openport = tport->portno;
     break;
   } else if  (tport->state == PORT_OPEN && tport->proto == IPPROTO_TCP) {
     openport = tport->portno;
   }
 }
 
closedport = (rand() % 14781) + 30000;

if (o.verbose && openport != -1)
  printf("For OSScan assuming that port %d is open and port %d is closed and neither are firewalled\n", openport, closedport);
 
 /* First we send our initial NUM_SEQ_SAMPLES SYN packets, 50ms apart */
 if (openport != -1) {
   for(i=1; i <= NUM_SEQ_SAMPLES; i++) {
     for(decoy=0; decoy < o.numdecoys; decoy++) {
       send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, o.magic_port+i, 
		    openport, sequence_base + i, 0,TH_BOGUS|TH_SYN, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
       usleep( 5000 + target->to.srtt);
    }
     /*     usleep(25000);*/
   }
   /* Now we collect  the replies */
   si->responses = 0;
   timeout = 0; 
   gettimeofday(&t1,NULL);
   if (o.debugging > 1) printf("TMP1\n");
   while(si->responses < NUM_SEQ_SAMPLES && !timeout) {
     ip = (struct ip*) readip_pcap(pd, &bytes);
     gettimeofday(&t2, NULL);
     if (!ip) { 
       if (TIMEVAL_SUBTRACT(t2,t1) > target->to.timeout)
	 timeout = 1;
       continue; 
     } else if (TIMEVAL_SUBTRACT(t2,t1) > 5 * target->to.timeout) {
       timeout = 1;
     }		  
     if (bytes < (4 * ip->ip_hl) + 4)
       continue;
     if (ip->ip_p == IPPROTO_TCP) {
       tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
       if (ntohs(tcp->th_dport) < o.magic_port || ntohs(tcp->th_dport) - o.magic_port > NUM_SEQ_SAMPLES || ntohs(tcp->th_sport) != openport) {
	 continue;
       }
       if ((tcp->th_flags & TH_RST)) {
	 /*readtcppacket((char *) ip, ntohs(ip->ip_len));*/	 
	 if (si->responses == 0) {	 
	     printf("WARNING:  RST from port %d -- is this port really open?\n", openport);
	     /* We used to quit in this case, but left-overs from a SYN
		scan or lame-ass TCP wrappers can cause this! */
	 } 
	 continue;
      } else if ((tcp->th_flags & (TH_SYN|TH_ACK)) == (TH_SYN|TH_ACK)) {
	/*readtcppacket((char *)ip, ntohs(ip->ip_len));*/
	  si->seqs[si->responses++] = ntohl(tcp->th_seq);
	  if (si->responses > 1) {
	    seq_diffs[si->responses-2] = MOD_DIFF(ntohl(tcp->th_seq), si->seqs[si->responses-2]);
	  }
	if (!FP) {
	  FP = safe_malloc(sizeof(FingerPrint));
	  bzero(FP, sizeof(FingerPrint));
	  FP->name = "T1";
	  FP->results = fingerprint_iptcppacket(ip ,265, ntohl(tcp->th_ack) -1);
	  FP->next = NULL;
	}
      }
     }
   }
   if (o.debugging > 1) printf("TMP2\n");
   if (si->responses >= 4) {
     seq_gcd = get_gcd_n_ulong(si->responses -1, seq_diffs);
     /*     printf("The GCD is %lu\n", seq_gcd);*/
     if (seq_gcd) {     
       for(i=0; i < si->responses - 1; i++)
	 seq_diffs[i] /= seq_gcd;
       for(i=0; i < si->responses - 1; i++) {     
	 if (MOD_DIFF(si->seqs[i+1],si->seqs[i]) > 50000000) {
	   si->class = SEQ_TR;
	   si->index = 9999999;
	   /*	 printf("Target is a TR box\n");*/
	   break;
	 }	
	 seq_avg_inc += seq_diffs[i];
       }
     }
     if (seq_gcd == 0) {
       si->class = SEQ_CONSTANT;
       si->index = 0;
     } else if (seq_gcd % 64000 == 0) {
       si->class = SEQ_64K;
       /*       printf("Target is a 64K box\n");*/
       si->index = 1;
     } else if (seq_gcd % 800 == 0) {
       si->class = SEQ_i800;
       /*       printf("Target is a i800 box\n");*/
       si->index = 10;
     } else if (si->class == SEQ_UNKNOWN) {
       seq_avg_inc = (0.5) + seq_avg_inc / (si->responses - 1);
       /*       printf("seq_avg_inc=%lu\n", seq_avg_inc);*/
       for(i=0; i < si->responses -1; i++)       {     
	 /*	 printf("The difference is %lu\n", seq_diffs[i]);
		 printf("Adding %lu^2=%e", MOD_DIFF(seq_diffs[i], seq_avg_inc), pow(MOD_DIFF(seq_diffs[i], seq_avg_inc), 2));*/
	 seq_inc_sum += pow(MOD_DIFF(seq_diffs[i], seq_avg_inc), 2);
       }
       /*       printf("The sequence sum is %e\n", seq_inc_sum);*/
       seq_inc_sum /= (si->responses - 1);
       si->index = (unsigned long) (0.5 + pow(seq_inc_sum, 0.5));
       /*       printf("The sequence index is %d\n", si->index);*/
       if (si->index < 75) {
	 si->class = SEQ_TD;
	 /*	 printf("Target is a Micro$oft style time dependant box\n");*/
       }
       else {
	 si->class = SEQ_RI;
	 /*	 printf("Target is a random incremental box\n");*/
       }
     }
     FPtmp = safe_malloc(sizeof(FingerPrint));
     bzero(FPtmp, sizeof(FingerPrint));
     if (!FP)
       FPtmp->next = NULL;
     else FPtmp->next = FP;
     FP = FPtmp;
     FP->name = "TSeq";
     seq_AVs = safe_malloc(sizeof(struct AVal) * 3);
     bzero(seq_AVs, sizeof(struct AVal) * 3);
     FP->results = seq_AVs;
     seq_AVs[0].attribute = "Class";
     switch(si->class) {
     case SEQ_CONSTANT:
       strcpy(seq_AVs[0].value, "C");
       seq_AVs[0].next = &seq_AVs[1];
       seq_AVs[1].attribute= "Val";     
       sprintf(seq_AVs[1].value, "%lX", si->seqs[0]);
       break;
     case SEQ_64K:
       strcpy(seq_AVs[0].value, "64K");      
       break;
     case SEQ_i800:
       strcpy(seq_AVs[0].value, "i800");
       break;
     case SEQ_TD:
       strcpy(seq_AVs[0].value, "TD");
       seq_AVs[0].next = &seq_AVs[1];
       seq_AVs[1].attribute= "gcd";     
       sprintf(seq_AVs[1].value, "%lu", seq_gcd);
       seq_AVs[1].next = &seq_AVs[2];
       seq_AVs[2].attribute="SI";
       sprintf(seq_AVs[2].value, "%d", si->index);
       break;
     case SEQ_RI:
       strcpy(seq_AVs[0].value, "RI");
       seq_AVs[0].next = &seq_AVs[1];
       seq_AVs[1].attribute= "gcd";     
       sprintf(seq_AVs[1].value, "%lu", seq_gcd);
       seq_AVs[1].next = &seq_AVs[2];
       seq_AVs[2].attribute="SI";
       sprintf(seq_AVs[2].value, "%d", si->index);
       break;
     case SEQ_TR:
       strcpy(seq_AVs[0].value, "TR");
       break;
     }
   }
   else {
     nmap_log("Insufficient responses for TCP sequencing (%d), OS detection will be less reliable\n", si->responses);
   }
 } else {
   nmap_log("Warning:  No ports found open on this machine, OS detection will be MUCH less reliable\n");
 }
 current_port = o.magic_port + NUM_SEQ_SAMPLES +1;
 
 /* Now lets do the NULL packet technique */
 testsleft = (openport == -1)? 3 : 6;
 FPtmp = NULL;
 bzero(FPtests, sizeof(FPtests));
 tries = 0;
 do { 
   newcatches = 0;
   if (openport != -1) {   
     /* Test 2 */
     if (!FPtests[2])
       for(decoy=0; decoy < o.numdecoys; decoy++) {
	 send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port, 
		      openport, sequence_base, 0,0, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
       }
     /* Test 3 */
     if (!FPtests[3])
       for(decoy=0; decoy < o.numdecoys; decoy++) {
	 send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port +1, 
		      openport, sequence_base, 0,TH_SYN|TH_FIN|TH_URG|TH_PUSH, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
       }
     /* Test 4 */
     if (!FPtests[4])
       for(decoy=0; decoy < o.numdecoys; decoy++) {
	 send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port +2, 
		      openport, sequence_base, 0,TH_ACK, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
       }
   }
   /* Test 5 */
   if (!FPtests[5])
     for(decoy=0; decoy < o.numdecoys; decoy++) {
       send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port +3, 
		    closedport, sequence_base, 0,TH_SYN, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }
     /* Test 6 */
   if (!FPtests[6])
     for(decoy=0; decoy < o.numdecoys; decoy++) {
       send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port +4, 
		    closedport, sequence_base, 0,TH_ACK, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }
     /* Test 7 */
   if (!FPtests[7])
     for(decoy=0; decoy < o.numdecoys; decoy++) {
       send_tcp_raw(rawsd, &o.decoys[decoy], &target->host, current_port +5, 
		    closedport, sequence_base, 0,TH_FIN|TH_PUSH|TH_URG, 0,"\003\003\012\001\002\004\001\011\010\012\077\077\077\077\000\000\000\000\000\000" , 20, NULL, 0);
     }
   gettimeofday(&t1, NULL);
   timeout = 0;
   while(( ip = (struct ip*) readip_pcap(pd, &bytes)) && !timeout) {
     gettimeofday(&t2, NULL);
     if (TIMEVAL_SUBTRACT(t2,t1) > 5 * target->to.timeout) {
       timeout = 1;
     }
     if (bytes < (4 * ip->ip_hl) + 4)
       continue;
     if (ip->ip_p == IPPROTO_TCP) {
       tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
       if (ntohs(tcp->th_dport) < current_port || ntohs(tcp->th_dport) - current_port > 5) {
	 continue;
       }
       testno = ntohs(tcp->th_dport) - current_port + 2;
       if (o.debugging > 1)
	 printf("Got packet for test number %d\n", testno);
       if (FPtests[testno]) continue;
       testsleft--;
       newcatches++;
       FPtests[testno] = safe_malloc(sizeof(FingerPrint));
       bzero(FPtests[testno], sizeof(FingerPrint));
       FPtests[testno]->results = fingerprint_iptcppacket(ip, 255, sequence_base);
       FPtests[testno]->name = (testno == 2)? "T2" : (testno == 3)? "T3" : (testno == 4)? "T4" : (testno == 5)? "T5" : (testno == 6)? "T6" : (testno == 7)? "T7" : "T8";
     }
   }     
   if (o.debugging > 1) printf("TMP3\n");
 } while ( testsleft > 0 && (tries++ < 5 && (newcatches || tries == 1)));
 
for(i=2; i < 8; i++)
  if (!FPtests[i] && ((openport != -1) || i > 4)) {
    /* We create a Resp (response) attribute with value of N (no) because
       it is important here to note whether responses were or were not 
       received */
    FPtests[i] = safe_malloc(sizeof(FingerPrint));
    bzero(FPtests[i], sizeof(FingerPrint));
    seq_AVs = safe_malloc(sizeof(struct AVal));
    seq_AVs->attribute = "Resp";
    strcpy(seq_AVs->value, "N");
    seq_AVs->next = NULL;
    FPtests[i]->results = seq_AVs;
    FPtests[i]->name =  (i == 2)? "T2" : (i == 3)? "T3" : (i == 4)? "T4" : (i == 5)? "T5" : (i == 6)? "T6" : (i == 7)? "T7" : "T8";
  }

 last = 0;
 FPtmp = NULL;
 for(i=2; i < 8 ; i++) {
   if (!FPtests[i]) continue; 
   if (!FPtmp) FPtmp = FPtests[i];
   if (last) {
     FPtests[last]->next = FPtests[i];    
   }
   last = i;
 }
 if (last) FPtests[last]->next = NULL;
 
 if (!FP) FP = FPtmp;
 else {
   for(FPtests[0] = FP; FPtests[0]->next; FPtests[0] = FPtests[0]->next)
     ;
   FPtests[0]->next = FPtmp;	
 }

 close(rawsd);
 pcap_close(pd);
 return FP;
}


struct AVal *fingerprint_iptcppacket(struct ip *ip, int mss, unsigned long syn) {
  struct AVal *AVs;
  int length;
  int opcode;
  char *p,*q;
  struct tcphdr *tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));

  AVs = malloc(6 * sizeof(struct AVal));

  /* Link them together */
  AVs[0].next = &AVs[1];
  AVs[1].next = &AVs[2];
  AVs[2].next = &AVs[3];
  AVs[3].next = &AVs[4];
  AVs[4].next = &AVs[5];
  AVs[5].next = NULL;

  /* First we give the "response" flag to say we did actually receive
     a packet -- this way we won't match a template with Resp=N */
  AVs[0].attribute = "Resp";
  strcpy(AVs[0].value, "Y");


  /* Next we check whether the Don't Fragment bit is set */
  AVs[1].attribute = "DF";
  if(ip->ip_off && 0x4000) {
    strcpy(AVs[1].value,"Y");
  } else strcpy(AVs[1].value, "N");

  /* Now we do the TCP Window size */
  AVs[2].attribute = "W";
  sprintf(AVs[2].value, "%hX", ntohs(tcp->th_win));

  /* Time for the ACK, the codes are:
     S   = same as syn
     S++ = syn + 1
     O   = other
  */
  AVs[3].attribute = "ACK";
  if (ntohl(tcp->th_ack) == syn + 1)
    strcpy(AVs[3].value, "S++");
  else if (ntohl(tcp->th_ack) == syn) 
    strcpy(AVs[3].value, "S");
  else strcpy(AVs[3].value, "O");
    
  /* Now time for the flags ... they must be in this order:
     B = Bogus (64, not a real TCP flag)
     U = Urgent
     A = Acknowledgement
     P = Push
     R = Reset
     S = Synchronize
     F = Final
  */
  AVs[4].attribute = "Flags";
  p = AVs[4].value;
  if (tcp->th_flags & TH_BOG) *p++ = 'B';
  if (tcp->th_flags & TH_URG) *p++ = 'U';
  if (tcp->th_flags & TH_ACK) *p++ = 'A';
  if (tcp->th_flags & TH_PUSH) *p++ = 'P';
  if (tcp->th_flags & TH_RST) *p++ = 'R';
  if (tcp->th_flags & TH_SYN) *p++ = 'S';
  if (tcp->th_flags & TH_FIN) *p++ = 'F';
  *p++ = '\0';

  /* Now for the TCP options ... */
  AVs[5].attribute = "Ops";
  p = AVs[5].value;
  /* Partly swiped from /usr/src/linux/net/ipv4/tcp_input.c in Linux kernel */
  length = (tcp->th_off * 4) - sizeof(struct tcphdr);
  q = ((char *)tcp) + sizeof(struct tcphdr);

  while(length > 0) {
    opcode=*q++;
    length--;
    if (!opcode) {
      *p++ = 'L'; /* End of List */
      break;
    } else if (opcode == 1) {
      *p++ = 'N'; /* No Op */
    } else if (opcode == 2) {
      *p++ = 'M'; /* MSS */
      q++;
      if(ntohs(*((unsigned short *) q)) == mss)
	*p++ = 'E'; /* Echoed */
      q += 2;
      length -= 3;
    } else if (opcode == 3) { /* Window Scale */
      *p++ = 'W';
      q += 2;
      length -= 2;
    } else if (opcode == 8) { /* Timestamp */
      *p++ = 'T';
      q += 9;
      length -= 9;
    }
  }
  *p++ = '\0';
  return AVs;
}

FingerPrint **match_fingerprint(FingerPrint *FP) {
  static FingerPrint *matches[10];
  int max_matches = 6;
  int matches_found = 0;
  FingerPrint *current_os;
  FingerPrint *current_test;
  struct AVal *tst;
  int match = 1;
  int i;

  if (!FP) {
    matches[0] = NULL;
    return matches;
  }

  for(i = 0; o.reference_FPs[i]; i++) {
    current_os = o.reference_FPs[i];
    match = 1;
    for(current_test = current_os; current_test; current_test = current_test->next) 
      {
	tst = gettestbyname(FP, current_test->name);
	if (tst) {
	  match = AVal_match(current_test->results, tst);
	  if (!match) break;
	}
      }
    if (match) {
      /* Yeah, we found a match! */
      if (matches_found >= max_matches -1) {
	matches[0] = NULL;
	return matches;
      }
      matches[matches_found++] = current_os;
    }
  }
  matches[matches_found] = NULL;
  return matches;
}

struct AVal *gettestbyname(FingerPrint *FP, char *name) {

  if (!FP) return NULL;
  do {
    if (!strcmp(FP->name, name))
      return FP->results;
    FP = FP->next;
  } while(FP);
  return NULL;
}

struct AVal *getattrbyname(struct AVal *AV, char *name) {

  if (!AV) return NULL;
  do {
    if (!strcmp(AV->attribute, name))
      return AV;
    AV = AV->next;
  } while(AV);
  return NULL;
}

int AVal_match(struct AVal *reference, struct AVal *fprint) {
  struct AVal *current_ref;
  struct AVal *current_fp;
  unsigned long number;
  unsigned long val;
  char *p, *q;  /* OHHHH YEEEAAAAAHHHH!#!@#$!% */
  char valcpy[512];
  char *endptr;
  int andexp, orexp, expchar, numtrue;

  for(current_ref = reference; current_ref; current_ref = current_ref->next) {
    current_fp = getattrbyname(fprint, current_ref->attribute);    
    if (!current_fp) continue;
    /* OK, we compare an attribute value in  cinrrent_fp->value to a 
     potentially large expression in current_ref->value.  The syntax uses
    < (less than), > (greather than), + (non-zero), | (or), and & (and) 
    No parenthesis are allowed and an expression cannot have | AND & */
    numtrue = andexp = orexp = 0;
    strcpy(valcpy, current_ref->value);
    p = valcpy;
    if (strchr(current_ref->value, '|')) {
      orexp = 1; expchar = '|';
    } else {
      andexp = 1; expchar = '&';
    }
    do {
      q = strchr(p, expchar);
      if (q) *q = '\0';
      if (!strcmp(p, "+")) {
	if (!*current_fp->value) { if (andexp) return 0; }
	else {
	  val = strtol(current_fp->value, &endptr, 16);
	  if (val == 0 || *endptr) { if (andexp) return 0; }
	  else { numtrue++; if (orexp) break; }
	}
      } else if (*p == '<' && isdigit((int) p[1])) {
	if (!*current_fp->value) { if (andexp) return 0; }
	number = strtol(p + 1, &endptr, 16);
	val = strtol(current_fp->value, &endptr, 16);
	if (val >= number || *endptr) { if (andexp) return 0; }
	else { numtrue++; if (orexp) break; }
      } else if (*p == '>' && isdigit((int) p[1])) {
	if (!*current_fp->value) { if (andexp) return 0; }
	number = strtol(p + 1, &endptr, 16);
	val = strtol(current_fp->value, &endptr, 16);
	if (val <= number || *endptr) { if (andexp) return 0; }
	else { numtrue++; if (orexp) break; }
      }
      else {
	if (strcmp(p, current_fp->value))
	  { if (andexp) return 0; }
	else { numtrue++; if (orexp) break; }
      }
      if (q) p = q + 1;
    } while(q);
      if (numtrue == 0) return 0;
    /* Whew, we made it past one Attribute alive , on to the next! */
  }
  return 1;  
}

void freeFingerPrint(FingerPrint *FP) {
FingerPrint *currentFP;
FingerPrint *nextFP;

if (!currentFP) return;

 for(currentFP = FP; currentFP; currentFP = nextFP) {
   nextFP = currentFP->next;
   if (currentFP->results)
     free(currentFP->results);
   free(currentFP);
 }
return;
}


int os_scan(struct hoststruct *target) {
FingerPrint *FPs[3];
FingerPrint **FP_matches[3];
struct seq_info si[3];
int try;
int i;

bzero(si, sizeof(si));

 for(try=0; try < 3; try++) {
  FPs[try] = get_fingerprint(target, &si[try]); 
  FP_matches[try] = match_fingerprint(FPs[try]);
  if (FP_matches[try][0]) 
    break;
  usleep(target->to.timeout);
 }
 if (try == 0) {
   /* Everything is hunky-dory so we do nothing */
 } else if (try == 3) {
   /* Uh-oh, we were NEVER able to match, lets take
      the first fingerprint */
   for(try=1; try < 3; try++) {
     if (o.debugging)
       error("Killing fingerprint #%d (0 based):\n%s", try, 
	     fp2ascii(FPs[try]));
     if (FPs[try]) freeFingerPrint(FPs[try]);
   }
   try = 0;
 } else {
   error("WARNING: OS didn't match until the %d try", try + 1);
   for(i=0; i < try; i++) {
     if (FPs[i]) {
       if (o.debugging)
	 error("Failed match #%d (0-based):\n%s", i, fp2ascii(FPs[i]));
       freeFingerPrint(FPs[i]);
     }
   }
 }

 target->FP = FPs[try];
 target->FP_matches = FP_matches[try];
 memcpy(&(target->seq), &si[try], sizeof(struct seq_info));
 return 1;
}

char *fp2ascii(FingerPrint *FP) {
static char str[2048];
FingerPrint *current;
struct AVal *AV;
char *p = str;

bzero(str, sizeof(str));

if (!FP) return "(None)";

if(*(FP->OS_name)) {
  int len = 0;
  len = sprintf(str, "FingerPrint  %s\n", FP->OS_name);
  p += len;
}

for(current = FP; current ; current = current->next) {
  strcpy(p, current->name);
  p += strlen(current->name);
  *p++='(';
  for(AV = current->results; AV; AV = AV->next) {
    strcpy(p, AV->attribute);
    p += strlen(AV->attribute);
    *p++='=';
    strcpy(p, AV->value);
    p += strlen(AV->value);
    *p++ = '%';
  }
  if(*(p-1) != '(')
    p--; /* Kill the final & */
  *p++ = ')';
  *p++ = '\n';
}
*p = '\0';
return str;
}


unsigned long get_gcd_n_ulong(int numvalues, unsigned long *values) {
  int gcd;
  int i;

  if (numvalues == 0) return 1;
  gcd = values[0];
  for(i=1; i < numvalues; i++)
    gcd = euclid_gcd(gcd, values[i]);

  return gcd;
}

unsigned long euclid_gcd(unsigned long a, unsigned long b) {
  if (a < b) return euclid_gcd(b,a);
  if (!b) return a;
  return euclid_gcd(b, a % b);
}



FingerPrint **parse_fingerprint_reference_file() {
FingerPrint **FPs;
FingerPrint *current;
FILE *fp;
char filename[256];
char line[1024];
int numrecords = 0;
int lineno = 0;
char *p, *q; /* OH YEAH!!!! */

/* If you need more than 2048 fingerprints, tough */
 FPs = safe_malloc(sizeof(FingerPrint *) * 2048); 
 bzero(FPs, sizeof(FingerPrint *) * 2048);


sprintf(filename, "%s/nmap-os-fingerprints", LIBDIR);
fp = fopen(filename, "r");
if (!fp) {
  fp = fopen("nmap-os-fingerprints", "r");
  if (!fp) {
    fatal("OS scan requested but I cannot find nmap_os_fingerprints file.  It must be in %s or your current directory", LIBDIR);
  }
}
 top:
while(fgets(line, sizeof(line), fp)) {  
  lineno++;
  /* Read in a record */
  if (*line == '\n' || *line == '#')
    continue;

 fparse:

  if (strncmp(line, "FingerPrint", 11)) {
    printf("Parse error on line %d of nmap_os_fingerprints file: %s\n", lineno, line);
  }
  p = line + 12;
  while(*p && isspace((int) *p)) p++;
  if (!*p) {
    printf("Parse error on line %d of nmap_os_fingerprints file: %s\n", lineno, line);    
  }
  FPs[numrecords] = safe_malloc(sizeof(FingerPrint));
  q = FPs[numrecords]->OS_name;
  while(*p && *p != '\n' && *p != '#') {
    *q++ = *p++;
  }
  *q = '\0';

  current = FPs[numrecords];
  /* Now we read the fingerprint itself */
  while(fgets(line, sizeof(line), fp)) {
    lineno++;
    if (*line == '#')
      continue;
    if (*line == '\n')
      break;
    if (!strncmp(line, "FingerPrint",11)) {
      goto fparse;
    }
    p = line;
    q = strchr(line, '(');
    if (!q) {
      printf("Parse error on line %d of nmap_os_fingerprints file: %s\n", lineno, line);
      goto top;
    }
    *q = '\0';
    if(current->name) {
      current->next = safe_malloc(sizeof(FingerPrint));
      current = current->next;
      bzero(current, sizeof(FingerPrint));
    }
    current->name = strdup(p);
    p = q+1;
    *q = '(';
    q = strchr(p, ')');
    if (!q) {
      printf("Parse error on line %d of nmap_os_fingerprints file: %s\n", lineno, line);
      goto top;
    }
    *q = '\0';
    current->results = str2AVal(p);
  }
  /* printf("Read in fingerprint:\n%s\n", fp2ascii(FPs[numrecords])); */
  numrecords++;
}
FPs[numrecords] = NULL; 
return FPs;
}

struct AVal *str2AVal(char *str) {
int i = 1;
int count = 1;
char *q = str, *p=str;
struct AVal *AVs;
if (!*str) return NULL;

/* count the AVals */
while((q = strchr(q, '%'))) {
  count++;
  q++;
}

AVs = safe_malloc(count * sizeof(struct AVal));
bzero(AVs, sizeof(struct AVal) * count);
for(i=0; i < count; i++) {
  q = strchr(p, '=');
  if (!q) {
    fatal("Parse error with AVal string (%s) in nmap-os-fingerprints file", str);
  }
  *q = '\0';
  AVs[i].attribute = strdup(p);
  p = q+1;
  if (i != count - 1) {
    q = strchr(p, '%');
    if (!q) {
      fatal("Parse error with AVal string (%s) in nmap-os-fingerprints file", str);
    }
    *q = '\0';
    AVs[i].next = &AVs[i+1];
  }
  strcpy(AVs[i].value, p); 
  p = q + 1;
}
return AVs;
}



