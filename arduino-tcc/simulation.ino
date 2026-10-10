#include <Arduino.h>
#include <ArduinoJson.h>

const char *DEVICE_NAME;
const char *API_KEY;
const char *SECRET;
const char *MQTT_BROKER;

byte mac[6];
int MQTT_PORT = 1883;

String getTopic() {
  return String("arduino/") + DEVICE_NAME + "/" + API_KEY + "/" + SECRET;
}

#ifdef EPOXY_DUINO
#include <cstdlib>
#include <ctime>

void sendMqtt(String topic, String payload) {
  payload.replace("\"", "\\\"");
  String command = "mosquitto_pub -h " + String(MQTT_BROKER) + " -p " +
                   String(MQTT_PORT) + " -t " + topic + " -m \"" + payload +
                   "\"";

  Serial.print("[SIMULATION] Executing: ");
  Serial.println(command);

  int result = system(command.c_str());

  if (result != 0) {
    Serial.println("[ERROR] Failed to publish. Is 'mosquitto-clients' "
                   "installed inside the container?");
  }
}

#else
#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <PubSubClient.h>

const char *WIFI_SSID = "YOUR_WIFI_NAME";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASS";

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
    if (i < 5)
      Serial.print(":");
  }
  Serial.println();
  client.setServer(MQTT_BROKER, MQTT_PORT);
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
    client.publish(topic.c_str(), payload.c_str());
  }
}
#endif

void setup() {
  Serial.begin(9600);

#ifdef EPOXY_DUINO
  DEVICE_NAME =
      getenv("DEVICE_NAME") ? getenv("DEVICE_NAME") : "unknown_device";
  API_KEY = getenv("API_KEY") ? getenv("API_KEY") : "no_key";
  SECRET = getenv("SECRET") ? getenv("SECRET") : "no_secret";
  MQTT_BROKER = getenv("MQTT_BROKER") ? getenv("MQTT_BROKER") : "mqtt-broker";

  char* env_port = getenv("MQTT_PORT");
  if (env_port != NULL) {
    MQTT_PORT = atoi(env_port);
  }

  long seed = 0;
  for (int i = 0; DEVICE_NAME[i]; i++) {
    seed += DEVICE_NAME[i];
  }
  srand(time(NULL) + seed);

  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = rand() % 256;
  mac[4] = rand() % 256;
  mac[5] = rand() % 256;

#else
  DEVICE_NAME = "sensor-physical";
  API_KEY = "physical-key";
  SECRET = "physical-secret";
  MQTT_BROKER = "mqtt-broker";
  setupHardware();
#endif

  Serial.println("System Initialized");
  Serial.print("Device Name: ");
  Serial.println(DEVICE_NAME);
  Serial.print("Broker: ");
  Serial.println(MQTT_BROKER);
  Serial.print("Port: ");
  Serial.println(MQTT_PORT);
}

void loop() {
  double temp = 25.0 + (rand() % 100) / 10.0;
  double hum = 50.0 + (rand() % 50) / 10.0;
  double volt = 3.3 + (rand() % 10) / 100.0;

  JsonDocument doc;

  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
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
