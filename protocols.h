
/***********************************************************************/
/* protocols.h -- Functions relating to the protocol scan and mapping  */
/* between IPproto Number <-> name.                                    */
/*                                                                     */
/***********************************************************************/
/*  The Nmap Security Scanner is (C) 1995-2000 Insecure.Org.  This     */
/*  program is free software; you can redistribute it and/or modify    */
/*  it under the terms of the GNU General Public License as published  */
/*  by the Free Software Foundation; Version 2.  This guarantees your  */
/*  right to use, modify, and redistribute this software under certain */
/*  conditions.  If this license is unacceptable to you,               */
/*  Insecure.Com LLC may be willing to sell alternative licenses       */
/*  (contact sales@insecure.com ).                                     */
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

#ifndef PROTOCOLS_H
#define PROTOCOLS_H

#ifndef WIN32
#include <netdb.h>
#endif
#include "nmap.h"
#include "global_structures.h"
#include "charpool.h"
#include "error.h"
#include "utils.h"

#define PROTOCOL_TABLE_SIZE 256

struct protocol_list {
  struct protoent *protoent;
  struct protocol_list *next;
};

struct protoent *nmap_getprotbynum(int num);
unsigned short *getfastprots(void);
unsigned short *getdefaultprots(void);


#endif
