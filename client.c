#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

// #define SERVER_IP "192.168.0.27"
#define SERVER_IP "78.96.223.72"
#define SERVER_PORT "42069"

void connect_to_server(struct sockaddr_in *addr) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int gai_ret = -1;
    while (gai_ret != 0)
        gai_ret = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &res);

    memcpy(addr, res->ai_addr, sizeof *addr);
}

void notify_server(int sockfd, struct sockaddr_in *addr) {
    int ret = sendto(sockfd, "<3", 3, 0, (struct sockaddr *)addr, sizeof *addr);
    if (ret == -1)
        printf("Could not connect to server: %s\n", strerror(errno));
}

int wait_confirm(int sockfd) {
    char buf[20];
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof addr;
    if (recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *)&addr, &addrlen) == -1)
        return -1;

    char ip[50];
    inet_ntop(AF_INET, &(addr.sin_addr), ip, INET_ADDRSTRLEN);

    return strcmp(SERVER_IP, ip) == 0 ? 0 : -1;
}

void set_recv_timeout(int sockfd, int sec) {
    // set timeout to 2 seconds
    const struct timeval timeout = {sec, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
}

int get_pk(int sockfd, char *buf, char *ipstr, int *port) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof addr;

    if (recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr*)&addr, &addr_len) == -1)
        return -1;

    *port = ntohs(addr.sin_port);
    inet_ntop(AF_INET, &(addr.sin_addr), ipstr, INET_ADDRSTRLEN);

    return 0;
}

int holepunch(int sockfd, struct sockaddr_in *addr) {
    for (int i = 0; i < 10; ++i)
        if (sendto(sockfd, "punch", 6, 0, (struct sockaddr *)addr, sizeof *addr) == -1)
            return -1;
    return 0;
}

int main(void) {
    // create socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // connect to the server
    struct sockaddr_in svaddr;
    connect_to_server(&svaddr);

    // set receive timeout to 2 seconds
    set_recv_timeout(sockfd, 2);

    // - main loop -
    // either receive peer info from server or messages from other clients
    int status = 0, peer_cnt = 0;
    struct sockaddr_in peers[10];
    while (1) {
        if (status == 0) {
            // send short msg to server
            notify_server(sockfd, &svaddr);
            printf("Sent notification to server\n");
            status = 1;
        } else if (status == 1) {
            // check for server confirmation
            if (wait_confirm(sockfd) != -1) {
                printf("Received server confirmation\n");
                status = 2;
            } else
                status = 0;
        } else {
            // check for incoming packets from server/peers
            printf("Checking for incoming packets...\n");
            struct sockaddr_in p;
            char buf[50], ipstr[50];
            int port;
            if (get_pk(sockfd, buf, ipstr, &port) == -1)
                continue;

            // check if packet is coming from server
            if (strcmp(ipstr, SERVER_IP) == 0 && port == atoi(SERVER_PORT)) {
                // check if peer (from packet data) is already known
                int seen = 0;
                struct sockaddr_in *new_peer = (struct sockaddr_in *)buf;
                for (int i = 0; i < peer_cnt && !seen; ++i)
                    if (memcmp(&(new_peer->sin_addr), &peers[i].sin_addr, sizeof(struct in_addr)) == 0
                        && new_peer->sin_port == peers[i].sin_port)
                        seen = 1;
                if (seen)
                    continue;

                // add new peer data to the list    
                memcpy(&peers[peer_cnt++], new_peer, sizeof(struct sockaddr_in));

                // get printable ip and port format
                port = ntohs(new_peer->sin_port);
                inet_ntop(AF_INET, &(new_peer->sin_addr), ipstr, INET_ADDRSTRLEN);
                printf("Found peer at %s:%d\n", ipstr, port);

                // hole-punch a connection to the new peer
                printf("Trying to hole-punch it...\n");
                if (holepunch(sockfd, new_peer) == -1)
                    printf("Failed to send UDP packet to new peer: %s\n", strerror(errno));
            } else {
                // packet (maybe) coming from another peer
                printf("Received message from %s:%d\n -> ", ipstr, port);
                printf("%s\n", buf);
            }
        }
    }

    return 0;
}
