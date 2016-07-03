/*
 * define path names
 *
 * $Id: pathnames.h,v 1.3 1994/04/20 00:11:32 paulus Exp $
 */

#if defined(STREAMS) || defined(ultrix)
#define _PATH_PIDFILE 	"/etc/ppp"
#else
#define _PATH_PIDFILE 	"/var/run"
#endif

#define _PATH_UPAPFILE 	"/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	"/etc/ppp/chap-secrets"
#define _PATH_SYSOPTIONS "/etc/ppp/options"
#define _PATH_IPUP	"/etc/ppp/ip-up"
#define _PATH_IPDOWN	"/etc/ppp/ip-down"
