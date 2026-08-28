#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define PING_PKT_SIZE 64
#define TARGET_IP "15.15.15.15"
#define SOURCE_IP "15.15.15.14"
#define INTERFACE "eth0"  // Change this to your network interface

// Calculate ICMP checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(void) {
    int sockfd;
    struct sockaddr_in dest_addr, source_addr;
    struct icmphdr icmp_hdr;
    char packet[PING_PKT_SIZE];
    int seq_num = 1;
    char cmd[256];
    
    // Configure source IP address on interface
    printf("Configuring IP address %s on interface %s...\n", SOURCE_IP, INTERFACE);
    snprintf(cmd, sizeof(cmd), "ip addr add %s/24 dev %s 2>/dev/null", SOURCE_IP, INTERFACE);
    system(cmd);
    
    snprintf(cmd, sizeof(cmd), "ip link set %s up 2>/dev/null", INTERFACE);
    system(cmd);
    
    sleep(1);  // Wait for interface to be ready
    
    printf("Pinging %s from %s using /bin/ping...\n", TARGET_IP, SOURCE_IP);

    // Send 5 ping packets using /bin/ping
    for (int i = 0; i < 5; i++) {
        printf("Ping #%d:\n", i + 1);
        snprintf(cmd, sizeof(cmd), "/bin/ping -c 1 -I %s %s", INTERFACE, TARGET_IP);
        system(cmd);
    }
    
    // Clean up: remove the IP address
    printf("Cleaning up IP address configuration...\n");
    snprintf(cmd, sizeof(cmd), "ip addr del %s/24 dev %s 2>/dev/null", SOURCE_IP, INTERFACE);
    system(cmd);
    
    printf("Done.\n");
    return 0;
}