void setup() {
  Serial.begin(115200);
  delay(1000); // give me time to bring up serial monitor
  Serial.println("ESP32 Touch Test");
}

bool touching = false;

void loop() {
	int touchVal = touchRead(T0);
	if (touchVal < 40) {
		if (!touching) {
			Serial.print("Touch detected: ");
			Serial.println(touchVal);
		}
		touching = true;
	} else {
		if (touching) {
			Serial.print("Touch released: ");
			Serial.println(touchVal);
		}
		touching = false;
	}
	delay(50);
}
