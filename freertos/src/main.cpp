#define LED_PIN 2
#define SENSOR_PIN 13

TaskHandle_t BlinkTaskHandle = NULL;
TaskHandle_t SensorTaskHandle = NULL;

void vBlinkTask(void *pvParameters) {
    pinMode(LED_PIN, OUTPUT);

    Serial.print("Blink Task running on Core: ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));

        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vSensorTask(void *pvParameters) {
    pinMode(SENSOR_PIN, INPUT);

    Serial.print("Sensor Task running on Core: ");
    Serial.println(xPortGetCoreID());

    for (;;) {
        int sensorVal = analogRead(SENSOR_PIN);
        Serial.printf("[Core %d] Sensor Reading: %d\n", xPortGetCoreID(), sensorVal);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    xTaskCreatePinnedToCore(
        vBlinkTask,          // Task function
        "Blink LED",         // Task name for debugging
        2048,                // Stack size (bytes on ESP32, not words!)
        NULL,                // Parameter to pass
        1,                   // Priority (higher number = higher priority)
        &BlinkTaskHandle,    // Task handle
        0                    // Core ID (0)
    );

    xTaskCreatePinnedToCore(
        vSensorTask,         // Task function
        "Read Sensor",       // Task name
        2048,                // Stack size
        NULL,                // Parameter to pass
        2,                   // Priority (Higher priority than Blink)
        &SensorTaskHandle,   // Task handle
        1                    // Core ID (1)
    );
}

void loop() {
    vTaskDelete(NULL);
}
