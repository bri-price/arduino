/*
 * 
 * 		Working Light Wand 
 * 		
 * 		ESP32 dev module or Nano Iot 33
 * 
 */

// Library initialization
#include <FastLED.h>				// Library for the LED Strip

#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>

typedef unsigned char prog_uchar;

#define SERIAL_BAUD 115200
#define LOOP_CYCLES 2000

#define DATA_PIN					D5

// LED Strip
#define NUM_LEDS					144		// Set the number of LEDs the LED Strip

int sabreColours[4][3] = {{210,0,0},{0,200,0},{0,0,180},{220,220,220}};
int sabreColour = 4;

int interruptPressed = 0;						// variable to keep track if user wants to interrupt display session.
int loopCounter = 0;								// count loops as a means to avoid extra presses of keys
int menuItem = 1;								// Variable for current main menu selection
boolean updateScreen = true;			// Added to minimize screen update flicker (Brian Heiland)
byte x;
int g = 0;												// Variable for the Green Value
int b = 0;												// Variable for the Blue Value
int r = 0;												// Variable for the Red Value
int hue = 180;
int hueSpeed = 2;
bool lightOn = false;
bool buttonDown = false;
bool debounce = false;
bool startingUp = false;
bool runningSabre = false;
bool shuttingDown = false;
int numLit = 0;

// ******************************************** INITIALISE COMPONENT OBJECTS  *******************************


// LED Strip
CRGB leds[NUM_LEDS];



void SetupLEDs() {
	LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
	LEDS.setBrightness(84);
}


// Setup loop to get everything ready.	This is only run once at power on or reset
void setup() {
	Serial.begin(SERIAL_BAUD);
	delay(100);
	pinMode(D1, OUTPUT);
	pinMode(D6, INPUT_PULLUP);

	digitalWrite(D1, HIGH);   // turn the LED on (HIGH is the voltage level)
	delay(1000);
	digitalWrite(D1, LOW);   // turn the LED on (HIGH is the voltage level)
	delay(100);
	digitalWrite(D1, HIGH);   // turn the LED on (HIGH is the voltage level)
	delay(1000);
	SetupLEDs();
}

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

	if (runningSabre && lightOn == false && shuttingDown == false) {
		Serial.println("Shutting down");
		shuttingDown = true;
		runningSabre = false;
	}
	
	if (runningSabre == false && lightOn == true && startingUp == false) {
		startingUp = true;
		Serial.println("Starting up");
	}

	if (runningSabre || startingUp || shuttingDown) {
		SendSabre();
	} else {					
		ClearStrip(0);								// after last image is displayed clear the strip
	}

	if (debounce == true) {
		debounce = false;
		delay(250);
	} else {
		delay(10);
	}
}

int NormaliseVal(int v, int mi, int ma, boolean loop) {
	if (v < mi) {
		return (loop) ? ma : mi;
	}
	if (v > ma) {
		return loop ? mi : ma;
	}
	return v;
}

void ClearStrip(int duration) {

	for (int x = 0; x < NUM_LEDS; x++) {
		leds[x] = 0;
	}
	FastLED.show();
	pinMode(DATA_PIN, INPUT_PULLUP);
}

int brightness = 80;
int huePoint = 33;
int startPoint = 36;
float throbVal = 1;
boolean throbDir = true;
int maxBrightness = 100;
int minBrightness = 50;
boolean logBrightness = false;

int FixVal(int v) {
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return v;
}

void HSVtoRGB(float H, float S,float V, int* rgbVals) {

    if (H > 360 || H < 0 || S > 100 || S < 0 || V > 100 || V < 0){
        return;
    }
    float s = S/100;
    float v = V/100;
    float C = s*v;
    float X = C*(1-abs(fmod(H/60.0, 2)-1));
    float m = v-C;
    float r,g,b;
    
    if (H >= 0 && H < 60){
        r = C,g = X,b = 0;
    } else if (H >= 60 && H < 120){
        r = X,g = C,b = 0;
    } else if (H >= 120 && H < 180){
        r = 0,g = C,b = X;
    } else if (H >= 180 && H < 240){
        r = 0,g = X,b = C;
    } else if (H >= 240 && H < 300){
        r = X,g = 0,b = C;
    } else {
        r = C,g = 0,b = X;
    }
    int R = (r + m) * 255;
    int G = (g + m) * 255;
    int B = (b + m) * 255;

    rgbVals[0] = R;
    rgbVals[1] = G;
    rgbVals[2] = B;
}

int SetSabreLine(int displayWidth, int currentLength) {

	int sr,sg,sb;

	if (sabreColour == 4) {
		int rgbVals[3] = {0,0,0};
		HSVtoRGB(hue,100,brightness, &rgbVals[0]);
		sr = rgbVals[0];
		sg = rgbVals[1];
		sb = rgbVals[2];
		hue = hue + hueSpeed;
		if (hue > 359) {
			hue = 0;
		}
	} else {
		sr = sabreColours[sabreColour][0];
		sg = sabreColours[sabreColour][1];
		sb = sabreColours[sabreColour][2];
	}


	for (int i = 0; i < currentLength; i++) {

		int brt = (currentLength * brightness / displayWidth );
	
		r = GammaVal(sr)*(brt *0.01);			// reduced brightness 50% when brightness was changed from 
		g = GammaVal(sg)*(brt *0.01);			// 100 to 99 , by 50% brightness was nearly 0 This formula corrects this.
		b = GammaVal(sb)*(brt *0.01);			// Brian Heiland Revise.	Old formula
		leds[i].setRGB(g,r,b);
	}
	for (int i = currentLength; i < displayWidth; i++) {
		leds[i].setRGB(0,0,0);
	}

	
	return 100;
}

//int maxBrightness = 100;
//int minBrightness = 10;


int GetBrightness(int displayWidth, int b, int i, float throbVal) {

	int brange = (maxBrightness - minBrightness) / 2;
	int quarter = displayWidth / 4;
	float sval =  i * 2 * 3.14159;	// from 0 to 2pi
	sval = sval / quarter;
	// distance = i / maxlength * num pulses ?
	double bval = sin(sval); 			// -1 to +1
	bval = bval * throbVal;
	double bval2 = bval * brange + brange + minBrightness;
	return bval2;
}

void SendSabre() {

	int displayWidth = NUM_LEDS;
	interruptPressed = 0;
	maxBrightness = 100;

	if (runningSabre || startingUp || shuttingDown) {

		if (startingUp) {
			numLit += 1;
			if (numLit >= displayWidth) {
				// if we went over the end, just pop back to the end
				numLit = displayWidth;
				startingUp = false;
				runningSabre = true;
				Serial.println("Running");
				logBrightness = true;
			}
		} else if (shuttingDown) {
			numLit -= 1;
			if (numLit <= 0) {
				numLit = 0;
				shuttingDown = false;
				Serial.println("Off");
			}
		}

		int lad = SetSabreLine(displayWidth, numLit);
		if (runningSabre == false)
			lad = 0;

		FastLED.show();
		delay(lad);
	}
}



const PROGMEM prog_uchar gammaTable[]	= {
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	1,	1,	1,	1,
	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	1,	2,	2,	2,	2,
	2,	2,	2,	2,	2,	3,	3,	3,	3,	3,	3,	3,	3,	4,	4,	4,
	4,	4,	4,	4,	5,	5,	5,	5,	5,	6,	6,	6,	6,	6,	7,	7,
	7,	7,	7,	8,	8,	8,	8,	9,	9,	9,	9,  10, 10, 10, 10, 11,
	11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16,
	16, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22,
	23, 23, 24, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30,
	30, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 37, 37, 38, 38, 39,
	40, 40, 41, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48, 49, 50,
	50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60, 61, 62,
	62, 63, 64, 65, 66, 67, 67, 68, 69, 70, 71, 72, 73, 74, 74, 75,
	76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99, 100,101,102,104,105,106,107,108,
	109,110,111,113,114,115,116,117,118,120,121,122,123,125,126,127
};
 
inline byte GammaVal(byte x) {
	return pgm_read_byte(&gammaTable[x]);
}
