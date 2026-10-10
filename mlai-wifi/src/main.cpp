#include <Arduino.h>
#include <WiFi.h>

// ⚠️ IMPORTANT: You MUST change these to the actual MAC addresses of your routers!
const String TARGET_BSSID_1 = "aa:bb:cc:dd:ee:01";
const String TARGET_BSSID_2 = "aa:bb:cc:dd:ee:02";

// Changed to strictly 2 Zones
const char *zoneNames[2] = { "Zone_A", "Zone_B" };

// 2x2 Matrix (2 Zones, 2 Access Points)
float zoneCentroids[2][2] = {
    { -100.0, -100.0 },
    { -100.0, -100.0 }
};

bool isTraining = false;
int currentTrainingZone = 0;

const int SAMPLES_NEEDED = 10;
int currentSamples = 0;
float temporarySum[2] = { 0.0, 0.0 };

// --- Functions ---

int predictZone(float currentFeatures[2]) {
    int bestZone = 0;
    float minDistance = 999999.0;

    // Loop changed to strictly 2 zones
    for (int z = 0; z < 2; z++) {
        // Skip if this zone hasn't been calibrated yet
        if (zoneCentroids[z][0] == -100.0) continue;

        float distance = 0;
        for (int i = 0; i < 2; i++) {
            distance += pow(currentFeatures[i] - zoneCentroids[z][i], 2);
        }
        distance = sqrt(distance);

        if (distance < minDistance) {
            minDistance = distance;
            bestZone = z;
        }
    }
    return bestZone;
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.println("\n--- 2-Zone / 2-AP Wi-Fi Fingerprinting ---");
    Serial.println("Send 'A' or 'B' via Serial to calibrate that zone.");
}

void loop() {
    // 1. Check for User Commands
    if (Serial.available() > 0) {
        char cmd = toupper(Serial.read());

        // Only accept A or B now
        if (cmd == 'A' || cmd == 'B') {
            isTraining = true;
            currentSamples = 0;
            temporarySum[0] = 0; temporarySum[1] = 0;

            if (cmd == 'A') currentTrainingZone = 0;
            else if (cmd == 'B') currentTrainingZone = 1;

            Serial.printf("\n>>> STARTED CALIBRATING %s <<<\n", zoneNames[currentTrainingZone]);
        }
    }

    // 2. Scan Wi-Fi Network
    float features[2] = { -100.0, -100.0 };
    int numNetworks = WiFi.scanNetworks();

    if (numNetworks > 0) {
        for (int i = 0; i < numNetworks; ++i) {
            String currentBSSID = WiFi.BSSIDstr(i);
            int32_t rssi = WiFi.RSSI(i);

            if (currentBSSID.equalsIgnoreCase(TARGET_BSSID_1)) features[0] = (float) rssi;
            else if (currentBSSID.equalsIgnoreCase(TARGET_BSSID_2)) features[1] = (float) rssi;
        }
    }

    // --- NEW SAFETY CHECK ---
    if (features[0] == -100.0 && features[1] == -100.0) {
        Serial.println("[WARNING] Neither target Wi-Fi router was found. Did you update TARGET_BSSID_1 and 2?");
    }

    // 3. Process the Data
    if (isTraining) {
        temporarySum[0] += features[0];
        temporarySum[1] += features[1];
        currentSamples++;

        Serial.printf("Collecting sample %d of %d...\n", currentSamples, SAMPLES_NEEDED);

        if (currentSamples >= SAMPLES_NEEDED) {
            zoneCentroids[currentTrainingZone][0] = temporarySum[0] / SAMPLES_NEEDED;
            zoneCentroids[currentTrainingZone][1] = temporarySum[1] / SAMPLES_NEEDED;

            Serial.printf("\nSUCCESS! %s trained. Fingerprint: [%.1f, %.1f]\n",
                    zoneNames[currentTrainingZone],
                    zoneCentroids[currentTrainingZone][0],
                    zoneCentroids[currentTrainingZone][1]);

            isTraining = false;
        }
        delay(500);
    }
    else {
        // Only predict if at least one zone has been trained
        if (zoneCentroids[0][0] != -100.0 || zoneCentroids[1][0] != -100.0) {
            int predictedIndex = predictZone(features);

            Serial.printf("Current Read: [%.1f, %.1f] --> Location: **%s**\n",
                    features[0], features[1], zoneNames[predictedIndex]);
        }
        else {
            Serial.println("Waiting for calibration... Send A or B to train.");
        }
        delay(2000);
    }
}
