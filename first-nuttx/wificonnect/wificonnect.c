#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// get these from .env file

#define WIFI_SSID     "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASS"

int wifi_connect(void)
{
    char cmd[128];
    printf("[Wi-Fi] Connecting to %s...\n", WIFI_SSID);

    snprintf(cmd, sizeof(cmd), "wapi psk wlan0 %s 3", WIFI_PASSWORD);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "wapi essid wlan0 %s 1", WIFI_SSID);
    system(cmd);

    system("renew wlan0");

    sleep(3);
    return 0;
}
