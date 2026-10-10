#include <Arduino.h>

const int soundSensorPin = 34;

void setup() {
  Serial.begin(115200);
  while (!Serial);
}

void loop() {
  int sensorValue = analogRead(soundSensorPin);
  Serial.println(sensorValue);
}
