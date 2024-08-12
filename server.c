#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MYPORT "42069"  // the port users will be connecting to

void set_recv_timeout(int sockfd, int sec) {
    // set timeout to 2 seconds
    const struct timeval timeout = {sec, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
}

void extract_ip_port(struct sockaddr_storage *sock_addr, char *ip_port) {
    void *addr;
    unsigned short int port_i;
    char ipstr[INET6_ADDRSTRLEN];
    if (sock_addr->ss_family == AF_INET) { // IPv4
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)sock_addr;
        addr = &(ipv4->sin_addr);
        port_i = ntohs(ipv4->sin_port);
        inet_ntop(AF_INET, addr, ipstr, sizeof ipstr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sock_addr;
        addr = &(ipv6->sin6_addr);
        port_i = ntohs(ipv6->sin6_port);
        inet_ntop(AF_INET6, addr, ipstr, sizeof ipstr);
    }

    char port[10];
    sprintf(port, "%d", port_i);
    printf("%s | %d\n\n", port, port_i);

    strcpy(ip_port, ipstr);
    strcat(ip_port, "_");
    strcat(ip_port, port);
}

void wait_addr(int sockfd, struct sockaddr_storage *their_addr, socklen_t *addr_len) {
    //their_addr->ss_family = AF_INET;

    char buf[100];
    int r = recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *)their_addr, addr_len);

    void *addr;
    unsigned short int port;
    char ipstr[INET6_ADDRSTRLEN];
    if (their_addr->ss_family == AF_INET) { // IPv4
        printf("%d\n", their_addr->ss_family);
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)their_addr;
        addr = &(ipv4->sin_addr);
        printf("port: %d\n\n", ipv4->sin_port);
        port = ntohs(ipv4->sin_port);

        printf("portpula: %d\n\n", port);
        inet_ntop(AF_INET, addr, ipstr, sizeof ipstr);
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)their_addr;
        addr = &(ipv6->sin6_addr);
        port = ntohs(ipv6->sin6_port);
        inet_ntop(AF_INET6, addr, ipstr, sizeof ipstr);
    }

    printf("Received: %s\n", buf);
    printf("From IP: %s\n", ipstr);
    printf("Through port: %d\n\n", port);

    sendto(sockfd, "Confirm client", 10, 0, (struct sockaddr *)&addr, *addr_len);
}

int init_udp_server() {
    struct addrinfo hints, *res;
    int sockfd;

    // first, load up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;         // IPv4
    hints.ai_socktype = SOCK_DGRAM;    // UDP
    hints.ai_flags = AI_PASSIVE;       // fill in IP automatically

    getaddrinfo(NULL, MYPORT, &hints, &res);

    // make a socket, bind it, and recvfrom() it:
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);

    // set receive timeout to 2 seconds
    set_recv_timeout(sockfd, 2);

    return sockfd;
}

int main(void) {
    // create UDP server
    int sockfd = init_udp_server();

    // - main loop -
    // whenever a new client sends smth to the server,
    // share its address to the other known peers
    // and also inform them about itself
    struct sockaddr_in peers[10];
    int peer_cnt = 0;
    while (1) {
        printf("Waiting for new clients...\n");

        // check for incoming packets
        char buf[50];
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof addr;
        if (recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *)&addr, &addr_len) == -1)
            continue;

        // get the ip and port of the sender
        char ipstr[50];
        int port;
        port = ntohs(addr.sin_port);
        inet_ntop(AF_INET, &(addr.sin_addr), ipstr, INET_ADDRSTRLEN);
        printf("Found new peer at: %s:%d\n", ipstr, port);

        // send confirmation back to peer
        sendto(sockfd, "<3", 3, 0, (struct sockaddr *)&addr, sizeof addr);

        // discard if peer is already known
        int seen = 0;
        for (int i = 0; i < peer_cnt && !seen; ++i)
            if (memcmp(&(addr.sin_addr), &peers[i].sin_addr, sizeof(struct in_addr)) == 0
                      && addr.sin_port == peers[i].sin_port)
                seen = 1;
        if (seen)
            continue;

        // notify the new peer about the others
        for (int i = 0; i < peer_cnt; ++i)
            sendto(sockfd, &peers[i], sizeof peers[i], 0, (struct sockaddr *)&addr, sizeof addr);

        // notify others about the new peer
        for (int i = 0; i < peer_cnt; ++i)
            sendto(sockfd, &addr, sizeof addr, 0, (struct sockaddr *)&peers[i], sizeof addr);

        // add new peer to the list of known clients
        memcpy(&peers[peer_cnt++], &addr, sizeof(struct sockaddr_in));
    }

    shutdown(sockfd, 2);
    return 0;
}
