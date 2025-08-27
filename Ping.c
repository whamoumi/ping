#include "ft_ping.h"

struct info ping_info = {1, 0, 0.0, 0.0, 0.0, NULL, 0.0, 0.0, 0.0, 0.0, 0.0};

unsigned int checksum(void *b, int len) 
{
    unsigned int *buf = b;
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
    for (int i = 0; i < 10; i++) { 
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

void sig_int(int sig)
{
    struct timeval end_time;
    (void)sig;
    gettimeofday(&end_time, NULL);
    printf("\n--- %s ping statistics ---\n", ping_info.hostname);
    printf("%d packets transmitted, %d received, %.1f%% packet loss, time %.1f ms\n",
           ping_info.sent, ping_info.received,
           ping_info.sent > 0 ? (ping_info.sent - ping_info.received) * 100.0 / ping_info.sent : 0.0,
           (end_time.tv_sec * 1000.0 + end_time.tv_usec / 1000.0) - ping_info.time);
    if (ping_info.received > 0) {
        double avg = ping_info.rtt_sum / ping_info.received;
        double variance = (ping_info.rtt_sum_sq / ping_info.received) - (avg * avg);
        double mdev = variance > 0 ? my_sqrt(variance) : 0.0;
        
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
                ping_info.rtt_min,
                avg,
                ping_info.rtt_max,
                mdev);
    }
    exit(0);
}

int main(int ac, char **av)
{
    int verbose = 0;
    int id = getpid() & 0xFFFF;
    
    if (getuid() != 0) {
        printf("ping: socket: Operation not permitted\n");
        printf("(Try running as root with sudo)\n");
        return 1;
    }
    
    if (ac < 2 || ac > 3)
        return(write(1, "Error\n", 6));
    if (ac == 2 && strcmp(av[1], "-h") == 0)
        return(printf("Usage\n\tping [options] <destination>\nOptions:\n\t<destination>       DNS name or IP4 address\n\t-v      Verbose output\n\t-h      Display this help message\n"));
    if (strcmp(av[1], "-v") == 0 && ac == 3)
        verbose = 1;
    if ((strcmp(av[1], "-v") != 0) && ac == 3)
        return(printf("Give the right options\n"));    
    if (ac == 3)
        ping_info.hostname = av[2];
    else
        ping_info.hostname = av[1]; 
    struct in_addr **addr_list;
    struct sockaddr_in dest;
    struct hostent *host;
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);     
    if (sock == -1)
            return(write(1, "Error socket\n", 13));   
    if (verbose == 1)
        printf("ping: sock4.fd: %d (socktype:SOCK_RAW), hints.ai_family: AF_UNSPEC\n\n", sock);
    if (ac == 2)
    {
        host = gethostbyname(av[1]);
        if (host == NULL)
            return(printf("ping: %s: Name or service not known\n", av[1]));
        addr_list = (struct in_addr **)host->h_addr_list;
        if (addr_list[0] == NULL)
            return(write(1, "Error addr_list\n", 17));
        if (verbose == 1)
            printf("ping: sock4.fd:-1 (socktype:SOCK_RAW), hints.ai_family: AF_UNSPEC\n\n");
        if (verbose == 1)
            printf("ai->ai_family: AF_INET, ai->ai_canonname: '%s'\n", av[1]);
        if (verbose == 1)
            printf("Resolved %s to %s\n", av[1], inet_ntoa(*addr_list[0]));
    }
    else if (ac == 3 && strcmp(av[1], "-v") == 0)
    {
        host = gethostbyname(av[2]);
        if (host == NULL)
            return(printf("ping: %s: Name or service not known\n", av[2]));
        addr_list = (struct in_addr **)host->h_addr_list;
        if (addr_list[0] == NULL)
            return(write(1, "Error addr_list\n", 17));
        if (verbose == 1)
            printf("Resolved %s to %s\n", av[2], inet_ntoa(*addr_list[0]));
    }
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr = *addr_list[0];
    if (verbose == 1)
        printf("ai->ai_family: AF_INET, ai->ai_canonname: '%s'\n", ping_info.hostname);
    printf("PING %s (%s) 56(84) bytes of data.\n", ping_info.hostname, inet_ntoa(*addr_list[0]));
    close(sock);
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    ping_info.time = start_time.tv_sec * 1000.0 + start_time.tv_usec / 1000.0;
    signal(SIGINT, &sig_int);
    while(1)
    {
        sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock == -1)
            return(write(1, "Error socket\n", 13));
        struct timeval timeout;
        struct timeval start, end;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));    
        struct icmphdr *icmp_hdr = malloc(sizeof(struct icmphdr));
        memset(icmp_hdr, 0, sizeof(struct icmphdr));
        icmp_hdr->type = ICMP_ECHO; // 8
        icmp_hdr->code = 0;
        icmp_hdr->un.echo.id = id;
        icmp_hdr->un.echo.sequence = ping_info.sent;
        icmp_hdr->checksum = 0;  // Mettre à 0 avant calcul
        icmp_hdr->checksum = checksum(icmp_hdr, sizeof(struct icmphdr));
        gettimeofday(&start, NULL);
        if (sendto(sock, icmp_hdr, sizeof(struct icmphdr), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
            return(write(1, "Error socket 2 \n", 15));
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
                gettimeofday(&end, NULL);
                double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                             (end.tv_usec - start.tv_usec) / 1000.0;
        
                if (ping_info.rtt_min == 0.0 || ping_info.rtt_min > rtt)
                    ping_info.rtt_min = rtt;
                if (ping_info.rtt_max < rtt)
                    ping_info.rtt_max = rtt;
                ping_info.rtt_sum += rtt;
                ping_info.rtt_sum_sq += rtt * rtt;
                if (verbose == 0)
                    printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n",
                        n, inet_ntoa(r_addr.sin_addr),
                        icmp_hdr1->un.echo.sequence,
                        ip_hdr->ttl, rtt);
                else
                    printf("%d bytes from %s: icmp_seq=%d ident=%d ttl=%d time=%.2f ms\n",
                        n, inet_ntoa(r_addr.sin_addr),
                        icmp_hdr1->un.echo.sequence,
                        id, ip_hdr->ttl, rtt);
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
        free(icmp_hdr);
        free(icmp_hdr1);
        close(sock);
        sleep(1);
        ping_info.sent++;
    }
    return (0);
}
