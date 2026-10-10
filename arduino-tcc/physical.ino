#include <Arduino.h>
#include <ArduinoJson.h>

const char *DEVICE_NAME = "NONE";
const char *API_KEY = "NONE";
const char *SECRET = "NONE";
const char *MQTT_BROKER = "NONE";

const char *WIFI_SSID = "NONE";
const char *WIFI_PASSWORD = "NONE";
int MQTT_PORT = 9002;

byte mac[6];

String getTopic() {
  return String("arduino/") + DEVICE_NAME + "/" + API_KEY + "/" + SECRET;
}

#ifdef EPOXY_DUINO
#include <cstdlib>

void sendMqtt(String topic, String payload) {
  payload.replace("\"", "\\\"");

  String command =
    "mosquitto_pub -h " + String(MQTT_BROKER) + " -p " + String(MQTT_PORT) + " -t " + topic + " -m \"" + payload + "\"";

  Serial.print("[SIMULATION] Executing: ");
  Serial.println(command);

  int result = system(command.c_str());

  if (result != 0) {
    Serial.println("[ERROR] Failed to publish. Is 'mosquitto-clients' installed?");
  }
}

#else
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setupHardware() {
  Serial.println("Connecting to Wifi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWifi connected!");
  Serial.print("Ip Address: ");
  Serial.println(WiFi.localIP());

  WiFi.macAddress(mac);
  Serial.print("MAC Address: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setBufferSize(1024);
}

void sendMqtt(String topic, String payload) {
  if (!client.connected()) {
    if (client.connect(DEVICE_NAME)) {
      Serial.println("MQTT Connected");
    } else {
      Serial.print("MQTT Failed, rc=");
      Serial.println(client.state());
      return;
    }
  }

  if (client.connected()) {
    if (client.publish(topic.c_str(), payload.c_str())) {
      Serial.println("[SUCCESS] Data Sent!");
    } else {
      Serial.println("[ERROR] Failed to send. Packet too big?");
    }
  }
}
#endif

void setup() {
  Serial.begin(9600);
  Serial.println("System Initialized");

#ifndef EPOXY_DUINO
  setupHardware();
#else

  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = 0xEF;
  mac[4] = 0xFE;
  mac[5] = 0xED;
#endif
}


void loop() {
  double temp = 25.0 + (rand() % 100) / 10.0;
  double hum = 50.0 + (rand() % 50) / 10.0;
  double volt = 3.3 + (rand() % 10) / 100.0;

  JsonDocument doc;

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2],
          mac[3], mac[4], mac[5]);

  doc["macAddress"] = macStr;
  doc["firmware"] = "1.0.0";
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["voltage"] = volt;
  doc["update"] = "none";
  doc["events"] = "status_ok";
  doc["logs"] = "loop_cycle_complete";

  String jsonString;
  serializeJson(doc, jsonString);

  sendMqtt(getTopic(), jsonString);

#ifndef EPOXY_DUINO
  client.loop();
#endif

  delay(1000);
}
