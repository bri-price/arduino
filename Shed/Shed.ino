/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://www.arduino.cc/en/Main/Products

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/Blink
*/

bool lightOn = false;
bool buttonDown = false;
bool debounce = false;

// the setup function runs once when you press reset or power the board
void setup() {
	// initialize digital pin LED_BUILTIN as an output.
	pinMode(D1, OUTPUT);
	pinMode(D6, INPUT_PULLUP);

	digitalWrite(D1, HIGH);   // turn the LED on (HIGH is the voltage level)
	delay(2000);
	digitalWrite(D1, LOW);   // turn the LED on (HIGH is the voltage level)
	delay(500);
	digitalWrite(D1, HIGH);   // turn the LED on (HIGH is the voltage level)
	delay(2000);
}

// the loop function runs over and over again forever
void loop() {
	if (digitalRead(D6) == LOW) {
		// set this flag whenever the button is down
		buttonDown = true;
	} else {
		if (buttonDown == true) {
			// if the button was down, and has now gone up, change the state
			buttonDown = false;
			lightOn = !lightOn;
			debounce = true;
		}
	}

	if (lightOn == true) {
		digitalWrite(D1, HIGH);   // turn the LED on (HIGH is the voltage level)
	} else {
		digitalWrite(D1, LOW);    // turn the LED off by making the voltage LOW
	}
	if (debounce == true) {
		debounce = false;
		delay(250);
	} else {
		delay(10);
	}
}
