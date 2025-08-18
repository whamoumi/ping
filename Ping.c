#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#include <stdio.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <stdlib.h>

uint16_t checksum(void *b, int len) {
    uint16_t *buf = b;
    unsigned int sum = 0;
    for (; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(uint8_t *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~sum;
}

int main(int ac, char **av)
{
    if (ac < 2 || ac > 3)
        return(write(1, "Error\n", 6));
    struct hostent *hostinfo = NULL;
    struct in_addr **addr_list;
    hostinfo = gethostbyname(av[1]); 
    addr_list = (struct in_addr **)hostinfo->h_addr_list;
    printf("IP address: %s\n", inet_ntoa(*addr_list[0]));
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr = *addr_list[0];
    // creer le socket pour le protocole ICMP
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1)
        return(write(1, "Error socket\n", 13)); 

    // creer le paquet icmp pour lenvoie du socket     
    struct icmphdr *icmp_hdr = malloc(sizeof(struct icmphdr));
    memset(icmp_hdr, 0, sizeof(struct icmp_hdr));
    icmp_hdr->type = ICMP_ECHO; // 8
    icmp_hdr->code = 0;
    icmp_hdr->un.echo.id = getpid() & 0xFFFF;
    icmp_hdr->un.echo.sequence = 1;
    icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr));

    // envoie de ICMPECHO 
    if (sendto(sock, icmp_hdr, sizeof(struct icmphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
        return(write(1, "Error socket 2 \n", 15));
    close 
}