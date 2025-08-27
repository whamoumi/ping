#ifndef FT_PING_H_H
# define FT_PING_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>  // Pour struct iphdr
#include <arpa/inet.h>
#include <unistd.h> 
#include <netdb.h> 
#include <stdio.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

struct info {
    int sent;
    int received;
    double min;
    double max;
    double total;
    char *hostname;
    double rtt_min;
    double rtt_max;
    double rtt_sum;
    double rtt_sum_sq;
    double time;
};

void            sig_int(int sig);
double          my_sqrt(double x);
unsigned    int checksum(void *b, int len);

#endif
