#ifndef PORTLIST_H
#define PORTLIST_H

/* struct port stuff */
#define PORT_UNKNOWN 0
#define PORT_CLOSED 1
#define PORT_OPEN 2
#define PORT_FIREWALLED 3
#define PORT_TESTING 4
#define PORT_FRESH 5
#define PORT_UNFIREWALLED 6
#define PORT_HIGHEST_STATE 7 /* ***IMPORTANT -- BUMP THIS UP WHEN STATES ARE 
				ADDED *** */
 
#define CONF_NONE 0
#define CONF_LOW 1
#define CONF_HIGH 2


struct port {
  unsigned short portno;
  unsigned char proto;
  char *owner;
  char *rpc_name;
  int rpc_status; /* RPC_STATUS_UNTESTED means we haven't checked
		    RPC_STATUS_UNKNOWN means the port appears to be RPC
		    but we couldn't find a match
		    RPC_STATUS_GOOD_PROG means rpc_program gives the prog #
		    RPC_STATUS_NOT_RPC means the port doesn't appear to 
		    be RPC */
  unsigned long rpc_program; /* Only valid if rpc_state == RPC_STATUS_GOOD_PROG */
  unsigned int rpc_lowver;
  unsigned int rpc_highver;
  int state; 
  int confidence; /* How sure are we about the state? */

  struct port *next; /* Internal use only -- we sometimes like to link them
			together */
};



typedef struct portlist {
  struct port **udp_ports;
  struct port **tcp_ports;
  int state_counts[PORT_HIGHEST_STATE]; /* How many ports in list are in each
					   state */
  int state_counts_udp[PORT_HIGHEST_STATE];
  int state_counts_tcp[PORT_HIGHEST_STATE];
  int ignored_port_state; /* The state of the port we ignore for output */
  int numports; /* Total number of ports in list in ANY state */
} portlist;

int addport(portlist *plist, unsigned short portno, unsigned short protocol,
	    char *owner, int state);
int deleteport(portlist *plist, unsigned short portno,
	       unsigned short protocol);

/* A function for iterating through the ports.  Give NULL for the
   first "afterthisport".  Then supply the most recent returned port
   for each subsequent call.  When no more matching ports remain, NULL
   will be returned.  To restrict returned ports to just one protocol,
   specify IPPROTO_TCP or IPPROTO_UDP for allowed_protocol.  A 0 for
   allowed_protocol matches either.  allowed_state works in the same
   fashion as allowed_protocol. This function returns ports in numeric
   order from lowest to highest, except that if you ask for both TCP &
   UDP, every TCP port will be returned before we start returning UDP
   ports */
struct port *nextport(portlist *plist, struct port *afterthisport, 
		      int allowed_protocol, int allowed_state);

struct port *lookupport(portlist *ports, unsigned short portno, 
			unsigned short protocol);
/* Decide which port we want to ignore in output (for example, we don't want
 to show closed ports if there are 40,000 of them.) */
void assignignoredportstate(portlist *plist);

/* RECYCLES the port so that it can later be obtained again using 
   make_empty_port */
void free_port(struct port *pt);

struct port *make_empty_port();

/* Empties out a portlist so that it can be reused (or freed).  All the 
   internal structures that must be freed are done so here. */
void resetportlist(portlist *plist);

#endif
