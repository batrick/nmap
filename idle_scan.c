
/***********************************************************************/
/* idle_scan.c -- Includes the function specific to "Idle Scan"        */
/* support (-sI).  This is an extraordinarily cool scan type that      */
/* can allow for completely blind scanning (eg no packets sent to the  */
/* target from your own IP address) and can also be used to penetrate  */
/* firewalls and scope out router ACLs.  This is one of the "advanced" */
/* scans meant for epxerienced Nmap users.                             */
/*                                                                     */
/***********************************************************************/
/*  The Nmap Security Scanner is (C) 1995-2001 Insecure.Com LLC. This  */
/*  program is free software; you can redistribute it and/or modify    */
/*  it under the terms of the GNU General Public License as published  */
/*  by the Free Software Foundation; Version 2.  This guarantees your  */
/*  right to use, modify, and redistribute this software under certain */
/*  conditions.  If this license is unacceptable to you, we may be     */
/*  willing to sell alternative licenses (contact sales@insecure.com). */
/*                                                                     */
/*  If you received these files with a written license agreement       */
/*  stating terms other than the (GPL) terms above, then that          */
/*  alternative license agreement takes precendence over this comment. */
/*                                                                     */
/*  Source is provided to this software because we believe users have  */
/*  a right to know exactly what a program is going to do before they  */
/*  run it.  This also allows you to audit the software for security   */
/*  holes (none have been found so far).                               */
/*                                                                     */
/*  Source code also allows you to port Nmap to new platforms, fix     */
/*  bugs, and add new features.  You are highly encouraged to send     */
/*  your changes to fyodor@insecure.org for possible incorporation     */
/*  into the main distribution.  By sending these changes to Fyodor or */
/*  one the insecure.org development mailing lists, it is assumed that */
/*  you are offering Fyodor the unlimited, non-exclusive right to      */
/*  reuse, modify, and relicense the code.  This is important because  */
/*  the inability to relicense code has caused devastating problems    */
/*  for other Free Software projects (such as KDE and NASM).  Nmap     */
/*  will always be available Open Source.  If you wish to specify      */
/*  special license conditions of your contributions, just say so      */
/*  when you send them.                                                */
/*                                                                     */
/*  This program is distributed in the hope that it will be useful,    */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of     */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU  */
/*  General Public License for more details (                          */
/*  http://www.gnu.org/copyleft/gpl.html ).                            */
/*                                                                     */
/***********************************************************************/

/* $Id$ */

#include "idle_scan.h"
#include "scan_engine.h"
#include "timing.h"
#include "osscan.h"
#include "nmap.h"

#include <stdio.h>

extern struct ops o;

/*  predefined filters -- I need to kill these globals at some point. */
extern unsigned long flt_dsthost, flt_srchost, flt_baseport;


struct idle_proxy_info {
  struct hoststruct host; /* contains name, IP, source IP, timing info, etc. */
  int seqclass; /* IPID sequence class (IPID_SEQ_* defined in nmap.h) */
  u16 latestid; /* The most recent IPID we have received from the proxy */
  u16 probe_port; /* The port we use for probing IPID infoz */
  u16 max_groupsz; /* We won't test groups larger than this ... */
  double current_groupsz; /* Current group size being used ... depends on
                          conditions ... won't be higher than
                          max_groupsz */
  int senddelay; /* Delay between sending pr0be SYN packets to target
                    (in microseconds) */
  int max_senddelay; /* Maximum time we are allowed to wait between
                        sending pr0bes (when we send a bunch in a row.
                        In microseconds. */

  pcap_t *pd; /* A Pcap descriptor which (starting in
                 initialize_idleproxy) listens for TCP packets from
                 the probe_port of the proxy box */
  int rawsd; /* Socket descriptor for sending probe packets to the proxy */
};


/* Sends an IPID probe to the proxy machine and returns the IPID.
   This function handles retransmissions, and returns -1 if it fails.
   Proxy timing is adjusted, but proxy->latestid is NOT ADJUSTED --
   you'll have to do that yourself.   Probes_sent is set to the number
   of probe packets sent during execution */
int ipid_proxy_probe(struct idle_proxy_info *proxy, int *probes_sent,
		     int *probes_rcvd) {
  struct timeval tv_end;
  int tries = 0;
  int trynum;
  int sent=0, rcvd=0;
  int maxtries = 3; /* The maximum number of tries before we give up */
  struct timeval tv_sent[3];
  int ipid = -1;
  int to_usec;
  int bytes;
  int timedout = 0;
  int base_port;
  struct ip *ip;
  struct tcphdr *tcp;
  static u32 seq_base = 0;
  static u32 ack = 0;
  static int packet_send_count = 0; /* Total # of probes sent by this program -- to ensure that our sequence # always changes */

  if (o.magic_port_set)
    base_port = o.magic_port;
  else base_port = o.magic_port + get_random_u8();

  if (seq_base == 0) seq_base = get_random_u32();
  if (!ack) ack = get_random_u32();


  do {
    timedout = 0;
    gettimeofday(&tv_sent[tries], NULL);

    /* Time to send the pr0be!*/
    send_tcp_raw(proxy->rawsd, &(proxy->host.source_ip), &(proxy->host.host), 
		 base_port + tries , proxy->probe_port, 
		 seq_base + (packet_send_count++ * 500) + 1, ack, 
		 TH_SYN|TH_ACK, 0, 
		 NULL, 0, NULL, 0);
    sent++;
    tries++;

    /* Now it is time to wait for the response ... */
    to_usec = proxy->host.to.timeout;
    gettimeofday(&tv_end, NULL);
    while((ipid == -1 || sent > rcvd) && to_usec >= 0) {

      to_usec = proxy->host.to.timeout - TIMEVAL_SUBTRACT(tv_end, tv_sent[tries-1]);
    
      ip = (struct ip *) readip_pcap(proxy->pd, &bytes, to_usec);      
      gettimeofday(&tv_end, NULL);
      if (ip) {
	if (bytes < ( 4 * ip->ip_hl) + 14U)
	  continue;

	if (ip->ip_p == IPPROTO_TCP) {

	  tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
	  if (ntohs(tcp->th_dport) < base_port || ntohs(tcp->th_dport) - base_port >= tries  || ntohs(tcp->th_sport) != proxy->probe_port || ((tcp->th_flags & TH_RST) == 0)) {
	    if (ntohs(tcp->th_dport) > o.magic_port && ntohs(tcp->th_dport) < (o.magic_port + 260)) {
	      if (o.debugging) {
		error("Received IPID zombie probe response which probably came from an earlier prober instance ... increasing rttvar from %d to %d", 
		      proxy->host.to.rttvar, (int) (proxy->host.to.rttvar * 1.2));
	      }
	      proxy->host.to.rttvar *= 1.2;
	      rcvd++;
	    }
	    else if (o.debugging > 1) {
	      error("Received unexpected response packet from %s during ipid zombie probing:", inet_ntoa(ip->ip_src));
	      readtcppacket((char *)ip,BSDUFIX(ip->ip_len));
	    }
	    continue;
	  }
	  
	  trynum = ntohs(tcp->th_dport) - base_port;
	  rcvd++;

	  ipid = ntohs(ip->ip_id);
	  adjust_timeouts2(&(tv_sent[trynum]), &tv_end, &(proxy->host.to));
	}
      }
    }
  } while(ipid == -1 && tries < maxtries);

  if (probes_sent) *probes_sent = sent;
  if (probes_rcvd) *probes_rcvd = rcvd;

  return ipid;
}


/* Returns the number of increments between an early IPID and a later
   one, assuming the given IPID Sequencing class.  Returns -1 if the
   distance cannot be determined */

int ipid_distance(int seqclass , u16 startid, u16 endid) {
  if (seqclass == IPID_SEQ_INCR)
    return endid - startid;
  
  if (seqclass == IPID_SEQ_BROKEN_INCR) {
    /* Convert to network byte order */
    startid = (startid >> 8) + ((startid & 0xFF) << 8);
    endid = (endid >> 8) + ((endid & 0xFF) << 8);
    return endid - startid;
  }

  return -1;

}


/* takes a proxy name/IP, resolves it if neccessary, tests it for IPID
   suitability, and fills out an idle_proxy_info structure.  If the
   proxy is determined to be unsuitable, the function whines and exits
   the program */
#define NUM_IPID_PROBES 6
void initialize_idleproxy(struct idle_proxy_info *proxy, char *proxyName,
			  struct in_addr *first_target) {
  int probes_sent = 0, probes_returned = 0;
  int hardtimeout = 9000000; /* Generally don't wait more than 9 secs total */
  int bytes, to_usec;
  int timedout = 0;
  char *p, *q;
  char *endptr = NULL;
  int seq_response_num;
  char *dev;
  int newipid;
  int i;
  char filter[512]; /* Libpcap filter string */
  char name[MAXHOSTNAMELEN + 1];
  u32 sequence_base;
  u32 ack = 0;
  struct timeval probe_send_times[NUM_IPID_PROBES], tmptv;
  u16 lastipid = 0;
  struct ip *ip;
  struct tcphdr *tcp;
  int distance;
  u16 ipids[NUM_IPID_PROBES]; 
  u8 probe_returned[NUM_IPID_PROBES];
  assert(proxy);
  assert(proxyName);

  ack = get_random_u32();

  for(i=0; i < NUM_IPID_PROBES; i++) probe_returned[i] = 0;

  bzero(proxy, sizeof(*proxy));
  proxy->host.to.srtt = -1;
  proxy->host.to.rttvar = -1;
  proxy->host.to.timeout = o.initial_rtt_timeout * 1000;

  proxy->max_groupsz = (o.max_parallelism)? o.max_parallelism : 100;
  proxy->max_senddelay = 100000;

  Strncpy(name, proxyName, sizeof(name));
  q = strchr(name, ':');
  if (q) {
    *q++ = '\0';
    proxy->probe_port = strtoul(q, &endptr, 10);
    if (*q==0 || !endptr || *endptr != '\0' || !proxy->probe_port) {
      fatal("Invalid port number given in IPID zombie specification: %s", proxyName);
    }
  } else proxy->probe_port = o.tcp_probe_port;

  proxy->host.name = strdup(name);

  if (resolve(name, &(proxy->host.host)) == 0) {
    fatal("Could not resolve idlescan zombie host: %s", name);
  }

  /* Lets figure out the appropriate source address to use when sending
     the pr0bez */
  if (o.source && o.source->s_addr) {
    proxy->host.source_ip.s_addr = o.source->s_addr;
    Strncpy(proxy->host.device, o.device, sizeof(proxy->host.device));
  } else {
    dev = routethrough(&(proxy->host.host), &(proxy->host.source_ip));  
    if (!dev) fatal("Unable to find appropriate source address and device interface to use when sending packets to %s", proxyName);
    Strncpy(proxy->host.device, dev, sizeof(proxy->host.device));
  }
  /* Now lets send some probes to check IPID algorithm ... */
  /* First we need a raw socket ... */
  if ((proxy->rawsd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0 )
    pfatal("socket trobles in get_fingerprint");
  unblock_socket(proxy->rawsd);
  broadcast_socket(proxy->rawsd);


/* Now for the pcap opening nonsense ... */
 /* Note that the snaplen is 152 = 64 byte max IPhdr + 24 byte max link_layer
  * header + 64 byte max TCP header. */
  proxy->pd = my_pcap_open_live(proxy->host.device, 152,  (o.spoofsource)? 1 : 0, 50);

  p = strdup(inet_ntoa(proxy->host.host));
  q = strdup(inet_ntoa(proxy->host.source_ip));
  snprintf(filter, sizeof(filter), "tcp and src host %s and dst host %s and src port %hi", p, q, proxy->probe_port);
 free(p); 
 free(q);
 set_pcap_filter(&(proxy->host), proxy->pd, flt_icmptcp, filter);
/* Windows nonsense -- I am not sure why this is needed, but I should
   get rid of it at sometime */

 flt_srchost = proxy->host.source_ip.s_addr;
 flt_dsthost = proxy->host.host.s_addr;

 sequence_base = get_random_u32();

 /* Yahoo!  It is finally time to send our pr0beZ! */

  while(probes_sent < NUM_IPID_PROBES) {
    if (o.scan_delay) enforce_scan_delay(NULL);
    else if (probes_sent) usleep(30000);

    /* TH_SYN|TH_ACK is what the proxy will really be receiving from
       the target, and is more likely to get through firewalls.  But
       TH_SYN allows us to get a nonzero ACK back so we can associate
       a response with the exact request for timing purposes.  So I
       think I'll use TH_SYN, although it is a tough call. */
    /* We can't use decoys 'cause that would screw up the IPIDs */
    send_tcp_raw(proxy->rawsd, &(proxy->host.source_ip), &(proxy->host.host), 
		 o.magic_port + probes_sent + 1, proxy->probe_port, 
		 sequence_base + probes_sent + 1, 0, TH_SYN|TH_ACK, 
		 ack, NULL, 0, NULL, 0);
    gettimeofday(&probe_send_times[probes_sent], NULL);
    probes_sent++;

    /* Time to collect any replies */
    while(probes_returned < probes_sent && !timedout) {

      to_usec = (probes_sent == NUM_IPID_PROBES)? hardtimeout : 1000;
      ip = (struct ip *) readip_pcap(proxy->pd, &bytes, to_usec);

      gettimeofday(&tmptv, NULL);
      
      if (!ip) {
	if (probes_sent < NUM_IPID_PROBES)
	  break;
	if (TIMEVAL_SUBTRACT(tmptv, probe_send_times[probes_sent - 1]) >= hardtimeout) {
	  timedout = 1;
	}
	continue;
      } else if (TIMEVAL_SUBTRACT(tmptv, probe_send_times[probes_sent - 1]) >=
		 hardtimeout)  {      
	timedout = 1;
      }

      if (lastipid != 0 && ip->ip_id == lastipid) {
	continue; /* probably a duplicate */
      }
      lastipid = ip->ip_id;

      if (bytes < ( 4 * ip->ip_hl) + 14U)
	continue;

      if (ip->ip_p == IPPROTO_TCP) {
	/*       readtcppacket((char *) ip, ntohs(ip->ip_len));  */
	tcp = ((struct tcphdr *) (((char *) ip) + 4 * ip->ip_hl));
	if (ntohs(tcp->th_dport) < (o.magic_port+1) || ntohs(tcp->th_dport) - o.magic_port > NUM_IPID_PROBES  || ntohs(tcp->th_sport) != proxy->probe_port || ((tcp->th_flags & TH_RST) == 0)) {
	  if (o.debugging > 1) error("Received unexpected response packet from %s during initial ipid zombie testing", inet_ntoa(ip->ip_src));
	  continue;
	}
	
	seq_response_num = probes_returned;

	/* The stuff below only works when we send SYN packets instead of
	   SYN|ACK, but then are slightly less stealthy and have less chance
	   of sneaking through the firewall.  Plus SYN|ACK is what they will
	   be receiving back from the target */
	/*	seq_response_num = (ntohl(tcp->th_ack) - 2 - sequence_base);
		if (seq_response_num < 0 || seq_response_num >= probes_sent) {
		if (o.debugging) {
		error("Unable to associate IPID proxy probe response with sent packet (received ack: %lX; sequence base: %lX. Packet:", ntohl(tcp->th_ack), sequence_base);
		readtcppacket((char *)ip,BSDUFIX(ip->ip_len));
		}
		seq_response_num = probes_returned;
		}
	*/
	probes_returned++;
	ipids[seq_response_num] = (u16) ntohs(ip->ip_id);
	probe_returned[seq_response_num] = 1;
	adjust_timeouts2(&probe_send_times[seq_response_num], &(tmptv), &(proxy->host.to));
      }
    }
  }

  /* Yeah!  We're done sending/receiving probes ... now lets ensure all of our responses are adjacent in the array */
  for(i=0,probes_returned=0; i < NUM_IPID_PROBES; i++) {
    if (probe_returned[i]) {    
      if (i > probes_returned)
	ipids[probes_returned] = ipids[i];
      probes_returned++;
    }
  }

  proxy->seqclass = ipid_sequence(probes_returned, ipids, 0);
  switch(proxy->seqclass) {
  case IPID_SEQ_INCR:
  case IPID_SEQ_BROKEN_INCR:
    log_write(LOG_NORMAL|LOG_SKID|LOG_STDOUT, "Idlescan using zombie %s (%s:%hi); Class: %s\n", proxy->host.name, inet_ntoa(proxy->host.host), proxy->probe_port, ipidclass2ascii(proxy->seqclass));
    break;
  default:
    fatal("Idlescan zombie %s (%s) port %hi cannot be used because IPID sequencability class is: %s.  Try another proxy.", proxy->host.name, inet_ntoa(proxy->host.host), proxy->probe_port, ipidclass2ascii(proxy->seqclass));
  }

  proxy->latestid = ipids[probes_returned - 1];
  proxy->current_groupsz = MIN(proxy->max_groupsz, 30);

  if (probes_returned < NUM_IPID_PROBES) {
    /* Yikes!  We're already losing packets ... clamp down a bit ... */
    if (o.debugging)
      error("idlescan initial zombie qualification test: %d probes sent, only %d returned", NUM_IPID_PROBES, probes_returned);
    proxy->current_groupsz = MIN(12, proxy->max_groupsz);
    proxy->senddelay += 5000;
  }

  /* OK, through experimentation I have found that some hosts *cough*
   Solaris APPEAR to use simple IPID incrementing, but in reality they
   assign a new IPID base to each host which connects with them.  This
   is actually a good idea on several fronts, but it totally
   frustrates our efforts (which rely on side-channel IPID info
   leaking to different hosts).  The good news is that we can easily
   detect the problem by sending some spoofed packets "from" the first
   target to the zombie and then probing to verify that the proxy IPID
   changed.  This will also catch the case where the Nmap user is
   behind an egress filter or other measure that prevents this sort of
   sp00fery */
  if (first_target) {  
    for (probes_sent = 0; probes_sent < 4; probes_sent++) {  
      if (probes_sent) usleep(50000);
      send_tcp_raw(proxy->rawsd, first_target, &(proxy->host.host), 
		   o.magic_port, proxy->probe_port, 
		   sequence_base + probes_sent + 1, 0, TH_SYN|TH_ACK, 
		   ack, NULL, 0, NULL, 0);

    }

    /* Sleep a little while to give packets time to reach their destination */
    usleep(300000);
    newipid = ipid_proxy_probe(proxy, NULL, NULL);
    if (newipid == -1)
      newipid = ipid_proxy_probe(proxy, NULL, NULL); /* OK, we'll give it one more try */

    if (newipid < 0) fatal("Your IPID Zombie (%s; %s) is behaving strangely -- suddenly cannot obtain IPID", proxy->host.name, inet_ntoa(proxy->host.host));
      
    distance = ipid_distance(proxy->seqclass, proxy->latestid, newipid);
    if (distance <= 0) {
      fatal("Your IPID Zombie (%s; %s) is behaving strangely -- suddenly cannot obtain valid IPID distance.", proxy->host.name, inet_ntoa(proxy->host.host));
    } else if (distance == 1) {
      fatal("Even though your Zombie (%s; %s) appears to be vulnerable to IPID sequence prediction (class: %s), our attempts have failed.  There generally means that either the Zombie uses a separate IPID base for each host (like Solaris), or because you cannot spoof IP packets (perhaps your ISP has enabled egress filtering to prevent IP spoofing), or maybe the target network recognizes the packet source as bogus and drops them", proxy->host.name, inet_ntoa(proxy->host.host), ipidclass2ascii(proxy->seqclass));
    }
    if (o.debugging && distance != 5) {
      error("WARNING: IPID spoofing test sent 4 packets and expected a distance of 5, but instead got %d", distance);
    }
    proxy->latestid = newipid;
  }
  
}




/* Adjust timing parameters up or down given that an idlescan found a
   count of 'testcount' while the 'realcount' is as given.  If the
   testcount was correct, timing is made more aggressive, while it is
   slowed down in the case of an error */
void adjust_idle_timing(struct idle_proxy_info *proxy, 
			struct hoststruct *target, int testcount, 
			int realcount) {

  static int notidlewarning = 0;

  if (o.debugging > 1)
    log_write(LOG_STDOUT, 
	  "adjust_idle_timing: tested/true %d/%d -- old grpsz/delay: %f/%d ",
	  testcount, realcount, proxy->current_groupsz, proxy->senddelay);
  else if (o.debugging && testcount != realcount) {
    error("adjust_idle_timing: testcount: %d  realcount: %d -- old grpsz/delay: %f/%d", testcount, realcount, proxy->current_groupsz, proxy->senddelay);
  }

    if (testcount < realcount) {
      /* We must have missed a port -- our probe could have been
	 dropped, the response to proxy could have been dropped, or we
	 didn't wait long enough before probing the proxy IPID.  The
	 third case is covered elsewhere in the scan, so we worry most
	 about the first two.  The solution is to decrease our group
	 size and add a sending delay */

      proxy->current_groupsz *= 0.80; /* packets could be dropped because
					too many sent at once */
      proxy->current_groupsz = MAX(proxy->current_groupsz, 1);
      proxy->senddelay += 10000;
      proxy->senddelay = MIN(proxy->max_senddelay, proxy->senddelay);
       /* No group size should be greater than .5s of send delays */
      proxy->current_groupsz = MAX(2, MIN(proxy->current_groupsz, 500000 / (proxy->senddelay + 1)));

    } else if (testcount > realcount) {
      /* Perhaps the proxy host is not really idle ... */
      /* I guess all I can do is decrease the group size, so that if the proxy is not really idle, at least we may be able to scan cnunks more quickly in between outside packets */
      proxy->current_groupsz *= 0.8;
      proxy->current_groupsz = MAX(proxy->current_groupsz, 2);

      if (!notidlewarning && o.verbose) {
	notidlewarning = 1;
	error("WARNING: Idlescan has erroneously detected phantom ports -- is the proxy %s (%s) really idle?", proxy->host.name, inet_ntoa(proxy->host.host));
      }
    } else {
      /* W00p We got a perfect match.  That means we get a slight increase
	 in allowed group size and we can lightly decrease the senddelay */

      proxy->senddelay *= 0.9;
      proxy->current_groupsz = MIN(proxy->current_groupsz * 1.1, 500000 / (proxy->senddelay + 1));
      proxy->current_groupsz = MIN(proxy->max_groupsz, proxy->current_groupsz);

    }
    if (o.debugging > 1)
      log_write(LOG_STDOUT, "-> %f/%d\n", proxy->current_groupsz, proxy->senddelay);
}


/* OK, now this is the hardcore idlescan function which actually does
   the testing (most of the other cruft in this file is just
   coordination, preparation, etc).  This function simply uses the
   Idlescan technique to try and count the number of open ports in the
   given port array.  The sent_time and rcv_time are filled in with
   the times that the probe packet & response were sent/received.
   They can be NULL if you don't want to use them.  The purpose is for
   timing adjustments if the numbers turn out to be accurate */

int idlescan_countopen2(struct idle_proxy_info *proxy, 
			struct hoststruct *target, u16 *ports, int numports,
			struct timeval *sent_time, struct timeval *rcv_time) 
{

#if 0 /* Testing code */
  int i;
  for(i=0; i < numports; i++)
    if (ports[i] == 22)
      return 1;
  return 0;
#endif

  int openports;
  int tries;
  int proxyprobes_sent = 0; /* diff. from tries 'cause sometimes we 
			       skip tries */
  int proxyprobes_rcvd = 0; /* To determine if packets were dr0pped */
  int sent, rcvd;
  int ipid_dist;
  struct timeval start, end, latestchange, now;
  struct timeval probe_times[4];
  int pr0be;
  static u32 seq = 0;
  int newipid;
  int sleeptime;
  int lasttry = 0;
  int dotry3 = 0;

  if (seq == 0) seq = get_random_u32();

  bzero(&end, sizeof(end));
  bzero(&latestchange, sizeof(latestchange));
  gettimeofday(&start, NULL);
  if (sent_time) bzero(sent_time, sizeof(*sent_time));
  if (rcv_time) bzero(rcv_time, sizeof(*rcv_time));

  /* I start by sending out the SYN pr0bez */
  for(pr0be = 0; pr0be < numports; pr0be++) {
    if (o.scan_delay) enforce_scan_delay(NULL);
    else if (proxy->senddelay && pr0be > 0) usleep(proxy->senddelay);

    /* Maybe I should involve decoys in the picture at some point --
       but doing it the straightforward way (using the same decoys as
       we use in probing the proxy box is risky.  I'll have to think
       about this more. */

    send_tcp_raw(proxy->rawsd, &(proxy->host.host), &target->host, 
		 proxy->probe_port, ports[pr0be], seq, 0, TH_SYN, 0, NULL, 0, 
		 o.extra_payload, o.extra_payload_length);
  }
  gettimeofday(&end, NULL);


  openports = -1;
  tries = 0;
  /* CHANGEME: I d'nt think that MAX() line works right */
  TIMEVAL_MSEC_ADD(probe_times[0], start, MAX(50, (target->to.srtt * 3/4) / 1000));
  TIMEVAL_MSEC_ADD(probe_times[1], start, target->to.srtt / 1000 );
  TIMEVAL_MSEC_ADD(probe_times[2], end, MAX(75, (target->to.srtt + 
						   target->to.rttvar) / 1000));
  TIMEVAL_MSEC_ADD(probe_times[3], end, MIN(4000, (target->to.srtt + 
						     (target->to.rttvar << 2 )) / 1000));

  do {
    if (tries == 2) dotry3 = (get_random_u8() > 200);
    if (tries == 3 && !dotry3)
      break; /* We usually want to skip the long-wait test */
    if (tries == 3 || (tries == 2 && !dotry3))
      lasttry = 1;

    gettimeofday(&now, NULL);
    sleeptime = TIMEVAL_SUBTRACT(probe_times[tries], now);
    if (!lasttry && proxyprobes_sent > 0 && sleeptime < 50000)
      continue; /* No point going again so soon */

    if (tries == 0 && sleeptime < 500)
      sleeptime = 500;
    if (o.debugging > 1) error("In preparation for idlescan probe try #%d, sleeping for %d usecs", tries, sleeptime);
    if (sleeptime > 0)
      usleep(sleeptime);

    newipid = ipid_proxy_probe(proxy, &sent, &rcvd);
    proxyprobes_sent += sent;
    proxyprobes_rcvd += rcvd;

    if (newipid > 0) {
      ipid_dist = ipid_distance(proxy->seqclass, proxy->latestid, newipid);
      /* I used to only do this if ipid_sit >= proxyprobes_sent, but I'd
	 rather have a negative number in that case */
      if (ipid_dist < proxyprobes_sent) {
	if (o.debugging) 
           error("idlescan_countopen2: Must have lost a sent packet because ipid_dist is %d while proxyprobes_sent is %d.", ipid_dist, proxyprobes_sent);
	/* I no longer whack timing here ... done at bottom */
      }
      ipid_dist -= proxyprobes_sent;
      if (ipid_dist > openports) {
	openports = ipid_dist;
	gettimeofday(&latestchange, NULL);
      } else if (ipid_dist < openports && ipid_dist >= 0) {
	/* Uh-oh.  Perhaps I dropped a packet this time */
	if (o.debugging > 1) {
	  error("idlescan_countopen2: Counted %d open ports in try #%d, but counted %d earlier ... probably a proxy_probe problem", ipid_dist, tries, openports);
	}	
	/* I no longer whack timing here ... done at bottom */
      }
    }
    
    if (openports > numports || (numports <= 2 && (openports == numports))) 
      break;    
  } while(tries++ < 3);

  if (proxyprobes_sent > proxyprobes_rcvd) {
    /* Uh-oh.  It looks like we lost at least one proxy probe packet */
    if (o.debugging) {
      error("idlescan_countopen2: Sent %d probes; only %d responses.  Slowing scan.", proxyprobes_sent, proxyprobes_rcvd);
    }
    proxy->senddelay += 5000;
    proxy->senddelay = MIN(proxy->max_senddelay, proxy->senddelay);
    /* No group size should be greater than .5s of send delays */
    proxy->current_groupsz = MAX(2, MIN(proxy->current_groupsz, 500000 / (proxy->senddelay+1)));
  } else {
    /* Yeah, we got as many responses as we sent probes.  This calls for a 
       very light timing acceleration ... */
    proxy->senddelay *= 0.95;
    proxy->current_groupsz = MAX(2, MIN(proxy->current_groupsz, 500000 / (proxy->senddelay+1)));
  }

  if ((openports > 0) && (openports <= numports)) {
    /* Yeah, we found open ports... lets adjust the timing ... */
    if (o.debugging > 2) error("idlescan_countopen2:  found %d open ports (out of %d) in %d usecs", openports, numports, TIMEVAL_SUBTRACT(latestchange, start));
    if (sent_time) *sent_time = start;
    if (rcv_time) *rcv_time = latestchange;
  }
  if (newipid > 0) proxy->latestid = newipid;
  return openports;
}



/* The job of this function is to use the Idlescan technique to count
   the number of open ports in the given list.  Under the covers, this
   function just farms out the hard work to another function */
int idlescan_countopen(struct idle_proxy_info *proxy, 
		       struct hoststruct *target, u16 *ports, int numports,
		       struct timeval *sent_time, struct timeval *rcv_time) {
  int tries = 0;
  int openports;

  do {
    openports = idlescan_countopen2(proxy, target, ports, numports, sent_time,
				    rcv_time);
    tries++;
    if (tries == 6 || (openports >= 0 && openports <= numports))
      break;
    
    if (o.debugging) {
      error("idlescan_countopen: In try #%d, counted %d open ports out of %d.  Retrying", tries, openports, numports);
    }
    /* Sleep for a little while -- maybe proxy host had brief birst of 
       traffic or similar problem */
    sleep(tries * tries);
    if (tries == 5)
      sleep(45); /* We're gonna give up if this fails, so we will be a bit
		    patient */
  } while(1);

  if (openports < 0 || openports > numports ) {
    /* Oh f*ck!!!! */
    fatal("Idlescan is unable to obtain meaningful results from proxy %s (%s).  I'm sorry it didn't work out.", proxy->host.name, inet_ntoa(proxy->host.host));
  }

  if (o.debugging > 2) error("idlescan_countopen: %d ports found open out of %d, starting with %hi", openports, numports, ports[0]);

  return openports;
}

/* Recursively Idlescans scans a group of ports using a depth-first
   divide-and-conquer strategy to find the open one(s) */

int idle_treescan(struct idle_proxy_info *proxy, struct hoststruct *target,
		 u16 *ports, int numports, int expectedopen) {

  int firstHalfSz = (numports + 1)/2;
  int secondHalfSz = numports - firstHalfSz;
  int flatcount1, flatcount2;
  int deepcount1 = -1, deepcount2 = -1;
  struct timeval sentTime1, rcvTime1, sentTime2, rcvTime2;
  int retrycount = -1, retry2 = -1;
  int totalfound = 0;
  /* Scan the first half of the range */

  if (o.debugging > 1) {  
    error("idle_treescan: Called against %s with %d ports, starting with %hi. expectedopen: %d", inet_ntoa(target->host), numports, ports[0], expectedopen);
    error("IDLESCAN TIMING: grpsz: %.3f delay: %d srtt: %d rttvar: %d\n",
	  proxy->current_groupsz, proxy->senddelay, target->to.srtt,
	  target->to.rttvar);
  }

  flatcount1 = idlescan_countopen(proxy, target, ports, firstHalfSz, 
				  &sentTime1, &rcvTime1);
  

  
  if (firstHalfSz > 1 && flatcount1 > 0) {
    /* A port appears open!  We dig down deeper to find it ... */
    deepcount1 = idle_treescan(proxy, target, ports, firstHalfSz, flatcount1);
    /* Now we assume deepcount1 is right, and adjust timing if flatcount1 was
       wrong */
    adjust_idle_timing(proxy, target, flatcount1, deepcount1);
  }

  /* I guess we had better do the second half too ... */

  flatcount2 = idlescan_countopen(proxy, target, ports + firstHalfSz, 
				  secondHalfSz, &sentTime2, &rcvTime2);
  
  if ((secondHalfSz) > 1 && flatcount2 > 0) {
    /* A port appears open!  We dig down deeper to find it ... */
    deepcount2 = idle_treescan(proxy, target, ports + firstHalfSz, 
			       secondHalfSz, flatcount2);
    /* Now we assume deepcount1 is right, and adjust timing if flatcount1 was
       wrong */
    adjust_idle_timing(proxy, target, flatcount2, deepcount2);
  }

  totalfound = (deepcount1 == -1)? flatcount1 : deepcount1;
  totalfound += (deepcount2 == -1)? flatcount2 : deepcount2;

  if ((flatcount1 + flatcount2 == totalfound) && 
      (expectedopen == totalfound || expectedopen == -1)) {
    
    if (flatcount1 > 0) {    
      if (o.debugging > 1) {
	error("Adjusting timing -- idlescan_countopen correctly found %d open ports (out of %d, starting with %hi)", flatcount1, firstHalfSz, ports[0]);
      }
      adjust_timeouts2(&sentTime1, &rcvTime1, &(target->to));
    }
    
    if (flatcount2 > 0) {    
      if (o.debugging > 2) {
	error("Adjusting timing -- idlescan_countopen correctly found %d open ports (out of %d, starting with %hi)", flatcount2, secondHalfSz, 
	      ports[firstHalfSz]);
      }
      adjust_timeouts2(&sentTime2, &rcvTime2, &(target->to));
    }
  }
  
  if (totalfound != expectedopen) {  
    if (deepcount1 == -1) {
      retrycount = idlescan_countopen(proxy, target, ports, firstHalfSz, NULL,
				      NULL);
      if (retrycount != flatcount1) {      
	/* We have to do a deep count if new ports were found and
	   there are more than 1 total */
	if (firstHalfSz > 1 && retrycount > 0) {	
	  retry2 = retrycount;
	  retrycount = idle_treescan(proxy, target, ports, firstHalfSz, 
				     retrycount);
	  adjust_idle_timing(proxy, target, retry2, retrycount);
	} else {
	  if (o.debugging)
	    error("Adjusting timing because my first scan of %d ports, starting with %hi found %d open, while second scan yielded %d", firstHalfSz, ports[0], flatcount1, retrycount);
	  adjust_idle_timing(proxy, target, flatcount1, retrycount);
	}
	totalfound += retrycount - flatcount1;
	flatcount1 = retrycount;

	/* If our first count erroneously found and added an open port,
	   we must delete it */
	if (firstHalfSz == 1 && flatcount1 == 1 && retrycount == 0)
	  deleteport(&target->ports, ports[0], IPPROTO_TCP);

      }
    }
    
    if (deepcount2 == -1) {
      retrycount = idlescan_countopen(proxy, target, ports + firstHalfSz, 
				      secondHalfSz, NULL, NULL);
      if (retrycount != flatcount2) {
	if (secondHalfSz > 1 && retrycount > 0) {	
	  retry2 = retrycount;
	  retrycount = idle_treescan(proxy, target, ports + firstHalfSz, 
				     secondHalfSz, retrycount);
	  adjust_idle_timing(proxy, target, retry2, retrycount);
	} else {
	  if (o.debugging)
	    error("Adjusting timing because my first scan of %d ports, starting with %hi found %d open, while second scan yeilded %d", secondHalfSz, ports[firstHalfSz], flatcount2, retrycount);
	  adjust_idle_timing(proxy, target, flatcount2, retrycount);
	}

	totalfound += retrycount - flatcount2;
	flatcount2 = retrycount;

	/* If our first count erroneously found and added an open port,
	   we must delete it */
	if (secondHalfSz == 1 && flatcount2 == 1 && retrycount == 0)
	  deleteport(&target->ports, ports[firstHalfSz], IPPROTO_TCP);


      }
    }
  }

  if (firstHalfSz == 1 && flatcount1 == 1) 
    addport(&target->ports, ports[0], IPPROTO_TCP, NULL, PORT_OPEN);
  
  if ((secondHalfSz == 1) && flatcount2 == 1) 
    addport(&target->ports, ports[firstHalfSz], IPPROTO_TCP, NULL, PORT_OPEN);
  return totalfound;

}



/* The very top-level idle scan function -- scans the given target
   host using the given proxy -- the proxy is cached so that you can keep
   calling this function with different targets */
void idle_scan(struct hoststruct *target, u16 *portarray, char *proxyName) {

  static char lastproxy[MAXHOSTNAMELEN + 1] = ""; /* The proxy used in any previous call */
  static struct idle_proxy_info proxy;
  int groupsz;
  int portidx = 0; /* Used for splitting the port array into chunks */
  int portsleft;
  time_t starttime;
  
  if (!proxyName) fatal("Idlescan requires a proxy host");

  if (*lastproxy && strcmp(proxyName, lastproxy))
    fatal("idle_scan(): You are not allowed to change proxies midstream.  Sorry");
  assert(target);



  /* If this is the first call,  */
  if (!*lastproxy) {
    initialize_idleproxy(&proxy, proxyName, &(target->host));
  }

  if (o.debugging || o.verbose) {  
    log_write(LOG_STDOUT, "Initiating Idlescan against %s (%s)\n", target->name, inet_ntoa(target->host));
  }
  starttime = time(NULL);

  /* If we don't have timing infoz for the new target, we'll use values 
     derived from the proxy */
  if (target->to.srtt == -1 && target->to.rttvar == -1) {
    target->to.srtt = 2 * proxy.host.to.srtt;
    target->to.rttvar = MAX(10000, MIN(target->to.srtt, 2000000));
    target->to.timeout = target->to.srtt + (target->to.rttvar << 2);
  }

  /* Now I guess it is time to let the scanning begin!  Since Idle
     scan is sort of tree structured (we scan a group and then divide
     it up and drill down in subscans of the group), we split the port
     space into smaller groups and then call a recursive
     divide-and-counquer function to find the open ports */
  while(portidx < o.numports) {
    portsleft = o.numports - portidx;
    /* current_groupsz is doubled below because idle_subscan cuts in half */
    groupsz = MIN(portsleft, (int) (proxy.current_groupsz * 2));
    idle_treescan(&proxy, target, portarray + portidx, groupsz, -1);
    portidx += groupsz;
  }


  if (o.verbose) {
    long timediff = time(NULL) - starttime;
    log_write(LOG_STDOUT, "The Idlescan took %ld %s to scan %d ports.\n", 
	      timediff, (timediff == 1)? "second" : "seconds", o.numports);
  }

  /* Now we go through the ports which were not determined were scanned
     but not determined to be open, and add them in the "closed" state */
  for(portidx = 0; portidx < o.numports; portidx++) {
    if (lookupport(&target->ports, portarray[portidx], IPPROTO_TCP) == NULL) {
      addport(&target->ports, portarray[portidx], IPPROTO_TCP, NULL,
	      PORT_CLOSED);
    }
  }

  return;
}
