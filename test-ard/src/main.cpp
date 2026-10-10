#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>

const int red_led_pin = 12;
const int blue_led_pin = 14;
const int sd_card_pin = 15;
const int button_pin = 13;

const char *WIFI_SSID = "NONE";
const char *WIFI_PASSWORD = "NONE";

const char *MQTT_BROKER = "NONE";
const int MQTT_PORT = 9002;

const char *DEVICE_NAME = "ESP32-TCC-001";

const char *MQTT_INPUT_TOPIC = "simulador/obd";
const char *MQTT_OUTPUT_TOPIC = "telemetria/veicular";

const char *ADMIN_USER = "admin";
const char *ADMIN_PASSWORD = "admin";

bool wifiEnabled = true;
int lastButtonState = HIGH;
bool timeConfigured = false;
bool backlogCleared = false;

unsigned long lastReconnectAttempt = 0;
unsigned long lastDataSendTime = 0;
unsigned long lastObdReadTime = 0;

const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

int latest_velocity = 0;
int latest_rpm = 0;
int latest_temp = 0;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

String getFileName() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "/sensor_data-[NO_DATE].csv";
  }
  char fileName[35];
  strftime(fileName, sizeof(fileName), "/sensor_data-[%Y-%m-%d].csv",
           &timeinfo);
  return String(fileName);
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String(millis());
  }
  char timeString[35];
  strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeString);
}

unsigned long getSavedPointer() {
  File ptrFile = SD.open("/backlog_pointer.txt", FILE_READ);
  if (ptrFile) {
    String val = ptrFile.readString();
    ptrFile.close();
    return val.toInt();
  }
  return 0;
}

void savePointer(unsigned long pos) {
  File ptrFile = SD.open("/backlog_pointer.txt", FILE_WRITE);
  if (ptrFile) {
    ptrFile.print(pos);
    ptrFile.close();
  }
}

int getBacklogCount() {
  String filename = getFileName();
  if (!SD.exists(filename)) {
    return 0;
  }
  File dataFile = SD.open(filename, FILE_READ);
  if (!dataFile) {
    return 0;
  }

  unsigned long startPos = getSavedPointer();
  if (startPos > 0) {
    dataFile.seek(startPos);
  }

  int recordCount = 0;
  while (dataFile.available()) {
    if (dataFile.read() == '\n')
      recordCount++;
  }
  dataFile.close();
  return recordCount;
}

bool sendMqtt(const String &topic, const String &payload, int maxRetries = 3) {
  if (!mqttClient.connected())
    return false;

  digitalWrite(red_led_pin, HIGH);
  for (int i = 1; i <= maxRetries; i++) {
    if (mqttClient.publish(topic.c_str(), payload.c_str())) {
      digitalWrite(red_led_pin, LOW);
      return true;
    }
    delay(100);
  }
  digitalWrite(red_led_pin, LOW);
  return false;
}

void logToSD(String currentTimestamp, int v, int r, int t) {
  digitalWrite(blue_led_pin, HIGH);
  String filename = getFileName();
  File dataFile = SD.open(filename, FILE_APPEND);

  if (dataFile) {
    dataFile.printf("%s,%d,%d,%d\n", currentTimestamp.c_str(), v, r, t);
    dataFile.close();
    Serial.println("[SD CARD] Data logged locally to " + filename);
  } else {
    Serial.println("[ERROR] Failed to open file for appending");
  }
  digitalWrite(blue_led_pin, LOW);
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, MQTT_INPUT_TOPIC) == 0) {
    String message;
    for (int i = 0; i < length; i++) {
      message += (char)payload[i];
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
      if (doc.containsKey("velocidadeObd"))
        latest_velocity = doc["velocidadeObd"];
      if (doc.containsKey("rpm"))
        latest_rpm = doc["rpm"];
      if (doc.containsKey("temperaturaMotor"))
        latest_temp = doc["temperaturaMotor"];

    } else {
      Serial.println("[MQTT ERROR] Failed to parse incoming OBD JSON");
    }
  }
}

bool processBacklog() {
  String filename = getFileName();

  if (!SD.exists(filename)) {
    return true;
  }

  int pendingRecords = getBacklogCount();
  if (pendingRecords == 0) {
    return true;
  }

  Serial.println("[SYSTEM] Found offline data. Uploading backlog...");

  File dataFile = SD.open(filename, FILE_READ);
  if (!dataFile) {
    Serial.println("[ERROR] Could not open backlog file");
    return false;
  }

  unsigned long currentPosition = getSavedPointer();
  if (currentPosition > 0) {
    dataFile.seek(currentPosition);
  }

  bool uploadFinished = true;

  while (dataFile.available()) {
    mqttClient.loop();

    if (!mqttClient.connected()) {
      Serial.println("\n[SYSTEM] Connection lost! Aborting upload...");
      savePointer(currentPosition);
      uploadFinished = false;
      break;
    }

    int currentBtnState = digitalRead(button_pin);
    if (lastButtonState == HIGH && currentBtnState == LOW) {
      Serial.println("\n[SYSTEM] Airplane Mode Activated! Aborting upload...");
      wifiEnabled = false;
      uploadFinished = false;
      savePointer(currentPosition);
      lastButtonState = currentBtnState;
      break;
    }
    lastButtonState = currentBtnState;

    unsigned long lineStartPos = dataFile.position();

    String line = dataFile.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) {
      continue;
    }

    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    int thirdComma = line.indexOf(',', secondComma + 1);

    if (thirdComma == -1) {
      continue;
    }

    String savedTimestamp = line.substring(0, firstComma);
    int savedVel = line.substring(firstComma + 1, secondComma).toInt();
    int savedRpm = line.substring(secondComma + 1, thirdComma).toInt();
    int savedTemp = line.substring(thirdComma + 1).toInt();

    JsonDocument doc;
    doc["pacoteId"] = String(DEVICE_NAME) + "-" + savedTimestamp;
    doc["codigoDispositivo"] = DEVICE_NAME;
    doc["timestamp"] = savedTimestamp;
    doc["velocidadeObd"] = savedVel;
    doc["rpm"] = savedRpm;
    doc["temperaturaMotor"] = savedTemp;
    doc["is_backup"] = true;

    String jsonString;
    serializeJson(doc, jsonString);

    if (sendMqtt(MQTT_OUTPUT_TOPIC, jsonString)) {
      currentPosition = dataFile.position();
      Serial.println("[MQTT] Backlog uploaded: " + jsonString);
    } else {
      savePointer(lineStartPos);
      Serial.println("[MQTT ERROR] Backlog upload failed for this line.");
      uploadFinished = false;
      break;
    }

    delay(20);
  }

  dataFile.close();

  if (uploadFinished) {
    String newFilename = filename;
    newFilename.replace(".csv", "_uploaded_" + String(millis()) + ".csv");

    if (SD.rename(filename.c_str(), newFilename.c_str())) {
      Serial.println("[SYSTEM] Backlog upload complete. File archived to: " +
                     newFilename);

      savePointer(0);

      for (int i = 0; i < 5; i++) {
        digitalWrite(blue_led_pin, HIGH);
        delay(100);
        digitalWrite(blue_led_pin, LOW);
        delay(100);
      }
      return true;
    } else {
      Serial.println("[ERROR] Upload complete, but failed to rename SD file!");
    }
  } else {
    Serial.println("[SYSTEM] Upload paused. SD File kept safe.");
  }

  return false;
}

void handleButton() {
  int currentButtonState = digitalRead(button_pin);
  if (currentButtonState != lastButtonState) {
    delay(50);
    currentButtonState = digitalRead(button_pin);
  }

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    wifiEnabled = !wifiEnabled;
    backlogCleared = false;

    if (wifiEnabled) {
      Serial.println("[BUTTON] Airplane Mode OFF - Starting Wi-Fi");
      WiFi.mode(WIFI_STA);
    } else {
      Serial.println(
          "[BUTTON] Airplane Mode ON - Dropping network, saving to SD...");
      mqttClient.disconnect();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      timeConfigured = false;
    }
  }
  lastButtonState = currentButtonState;
}

void handleNetwork() {
  if (!wifiEnabled) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnectAttempt > 5000) {
      Serial.print("[WIFI] Connecting...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastReconnectAttempt = millis();
      digitalWrite(blue_led_pin, LOW);
    }
  } else {
    digitalWrite(blue_led_pin, HIGH);
    if (!timeConfigured) {
      configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
      timeConfigured = true;
    }

    if (!mqttClient.connected()) {
      if (millis() - lastReconnectAttempt > 5000) {
        Serial.print("[MQTT] Connecting...");
        String clientId = String(DEVICE_NAME) + "-" +
                          String((uint32_t)ESP.getEfuseMac(), HEX);

        if (mqttClient.connect(clientId.c_str(), ADMIN_USER, ADMIN_PASSWORD)) {
          Serial.println(" Connected!");
          mqttClient.subscribe(MQTT_INPUT_TOPIC);
          Serial.println("[MQTT] Subscribed to topic: " + String(MQTT_INPUT_TOPIC));
          processBacklog();
          backlogCleared = true;
        } else {
          Serial.println(" Failed.");
        }
        lastReconnectAttempt = millis();
      }
    } else {
      mqttClient.loop();
    }
  }
}

void updateLEDs() {
  if (wifiEnabled && WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    digitalWrite(red_led_pin, HIGH);
  } else {
    digitalWrite(red_led_pin, LOW);
  }
}

void handleTelemetry() {
  if (millis() - lastDataSendTime >= 250) {
    lastDataSendTime = millis();
    String currentTimestamp = getTimestamp();

    if (wifiEnabled && mqttClient.connected()) {
      if (!backlogCleared) {
        backlogCleared = processBacklog();
      }

      JsonDocument doc;
      doc["pacoteId"] = String(DEVICE_NAME) + "-" + String(millis());
      doc["codigoDispositivo"] = DEVICE_NAME;
      doc["timestamp"] = currentTimestamp;
      doc["velocidadeObd"] = latest_velocity;
      doc["rpm"] = latest_rpm;
      doc["temperaturaMotor"] = latest_temp;
      doc["is_backup"] = false;

      String jsonString;
      serializeJson(doc, jsonString);

      if (sendMqtt(MQTT_OUTPUT_TOPIC, jsonString)) {
        Serial.println("[MQTT] Live data sent: " + jsonString);
      } else {
        Serial.println("[MQTT ERROR] Failed to send live data!");
      }
    } else {
      logToSD(currentTimestamp, latest_velocity, latest_rpm, latest_temp);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("System Initialized");
  pinMode(red_led_pin, OUTPUT);
  pinMode(blue_led_pin, OUTPUT);
  pinMode(button_pin, INPUT);

  digitalWrite(red_led_pin, LOW);
  digitalWrite(blue_led_pin, LOW);

  Serial.print("Initializing SD card...");
  if (!SD.begin(sd_card_pin)) {
    Serial.println(" Card Mount Failed!");
  } else {
    Serial.println(" SD Card initialized.");
  }

  WiFi.mode(WIFI_STA);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(1024);
  mqttClient.setCallback(mqttCallback);
  lastButtonState = digitalRead(button_pin);
}

void loop() {
  handleButton();
  handleNetwork();
  updateLEDs();

  if (millis() - lastObdReadTime >= 100) {
    lastObdReadTime = millis();
  }

  handleTelemetry();
}
