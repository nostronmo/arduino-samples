#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("Failed to create socket\n");
        return -1;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(14550); // MAVLink / UDP Telemetry port
    inet_pton(AF_INET, "192.168.1.50", &dest_addr.sin_addr); // Target PC IP

    const char *payload = "Telemetry Packet from ESP32 NuttX\n";

    for (int i = 0; i < 10; i++) {
        sendto(sockfd, payload, strlen(payload), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        printf("Sent telemetry packet %d\n", i + 1);
        sleep(1);
    }

    close(sockfd);
    return 0;
}
