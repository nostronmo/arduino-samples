#include <Arduino.h>

const int BUTTON_PIN_1 = 19;
const int BUTTON_PIN_2 = 21;

const int LED_PIN_1 = 5;
const int LED_PIN_2 = 18;

void setup()
{
	Serial.begin(115200);
	pinMode(BUTTON_PIN_1, INPUT);
	pinMode(BUTTON_PIN_2, INPUT);
	pinMode(LED_PIN_1, OUTPUT);
	pinMode(LED_PIN_2, OUTPUT);

}

void loop()
{
	int buttonState1 = digitalRead(BUTTON_PIN_1);
	digitalWrite(LED_PIN_1, buttonState1);

	int buttonState2 = digitalRead(BUTTON_PIN_2);
	digitalWrite(LED_PIN_2, buttonState2);

	Serial.print("Button 1: ");
	Serial.print(buttonState1);
	Serial.print(" | Button 2: ");
	Serial.println(buttonState2);

	delay(100);
}
