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
    double rtt_total;

};

struct info ping_info = {1, 0, 0.0, 0.0, 0.0, NULL, 0.0, 0.0, 0.0, 0.0};

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

double my_sqrt(double x)
{
    if (x <= 0)
        return 0;
    double guess = x / 2.0;
    for (int i = 0; i < 10; i++) {  // 10 itérations suffisent pour une bonne précision
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

void sig_int(int sig)
{
    printf("\n--- %s ping statistics ---\n", ping_info.hostname);
    printf("%d packets transmitted, %d received, %.1f%% packet loss\n",
           ping_info.sent, ping_info.received,
           (ping_info.sent - ping_info.received) * 100.0 / ping_info.sent);
    if (ping_info.received > 0) {
        // a revoir 
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
                ping_info.rtt_min,
                ping_info.rtt_sum / ping_info.received,
                ping_info.rtt_max,
                my_sqrt(ping_info.rtt_sum / ping_info.received - ping_info.rtt_min));
    }
    exit(0);
}

int main(int ac, char **av)
{
    int verbose = 0;
    if (ac < 2 || ac > 3)
        return(write(1, "Error\n", 6));
    if (strcmp(av[1], "-v" == 0 && ac == 3)
        verbose = 1;
    if (strcmp(av[1], "-v" != 0 && ac == 3)
        return(printf("Give the right options\n"));
    struct in_addr **addr_list;
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    if (inet_aton(av[1], &dest.sin_addr) == 0)
        return(write(1, "Give a valid IP address\n", 24));
    ping_info.hostname = av[1];
    if (verbose == 1)
        printf("Ecrire tous les informations que l'on as avec verbose\n");
    printf("PING %s 56(84) bytes of data.\n", av[1]);
    while(1)
    {
        struct timeval start, end;
        // creer le socket pour le protocole ICMP
        int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock == -1)
            return(write(1, "Error socket\n", 13)); 
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        // creer le paquet icmp pour lenvoie du socket     
        struct icmphdr *icmp_hdr = malloc(sizeof(struct icmphdr));
        memset(icmp_hdr, 0, sizeof(struct icmphdr));
        icmp_hdr->type = ICMP_ECHO; // 8
        icmp_hdr->code = 0;
        icmp_hdr->un.echo.id = getpid() & 0xFFFF;
        icmp_hdr->un.echo.sequence = ping_info.sent;
        icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr));
        
        // envoie de ICMPECHO 
        gettimeofday(&start, NULL);
        if (sendto(sock, icmp_hdr, sizeof(struct icmphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
            return(write(1, "Error socket 2 \n", 15));
        
        //reception de ICMP Echo Reply
        struct sockaddr_in r_addr;
        char buffer[1024];
        socklen_t addr_len = sizeof(r_addr);
        struct icmphdr *icmp_hdr1 = malloc(sizeof(struct icmphdr));
        int n;
        n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&r_addr, &addr_len);
        
        struct iphdr *ip_hdr = (struct iphdr *)buffer;
        int ip_header_len = ip_hdr->ihl * 4;
        memcpy(icmp_hdr1, buffer + ip_header_len, sizeof(struct icmphdr));
        if (n > 0) {
            if (icmp_hdr1->type == ICMP_ECHOREPLY) {
                // calcul RTT comme avant
                gettimeofday(&end, NULL);
                double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                             (end.tv_usec - start.tv_usec) / 1000.0;
        
                if (ping_info.rtt_min == 0.0 || ping_info.rtt_min > rtt)
                    ping_info.rtt_min = rtt;
                if (ping_info.rtt_max < rtt)
                    ping_info.rtt_max = rtt;
                ping_info.rtt_sum += rtt;
                ping_info.rtt_total += 1;
        
                printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n",
                       n, inet_ntoa(r_addr.sin_addr),
                       icmp_hdr1->un.echo.sequence,
                       ip_hdr->ttl, rtt);
                ping_info.received++;
            }
            else if (icmp_hdr1->type == ICMP_DEST_UNREACH  && verbose == 1) {
                printf("From %s icmp_seq=%d Destination Unreachable (code=%d)\n",
                       inet_ntoa(r_addr.sin_addr), ping_info.sent, icmp_hdr1->code);
            }
            else if (icmp_hdr1->type == ICMP_TIME_EXCEEDED && verbose == 1) {
                printf("From %s icmp_seq=%d Time to live exceeded\n",
                       inet_ntoa(r_addr.sin_addr), ping_info.sent);
            }
            else if (icmp_hdr1->type == ICMP_REDIRECT  && verbose == 1) {
                printf("From %s icmp_seq=%d Redirect (code=%d)\n",
                       inet_ntoa(r_addr.sin_addr), ping_info.sent, icmp_hdr1->code);
            }
            else {
                printf("From %s: ICMP type=%d code=%d (non géré)\n",
                       inet_ntoa(r_addr.sin_addr), icmp_hdr1->type, icmp_hdr1->code);
            }
        }
        signal(SIGINT, &sig_int);
        free(icmp_hdr);
        free(icmp_hdr1);
        close(sock);
        sleep(1);
        ping_info.sent++;
    }
    return (0);
}
