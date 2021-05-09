/*

	Use board: "ESP 32 Wrover Module"

*/
#include <AsyncTCP.h>
#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>

#include <EEPROM.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>

#include "Secrets.h"

// optocouplers
#define CAMERA_PIN		18
#define FOCUS_PIN		5
#define FLASH_PIN		19

// solenoid
#define SOLENOID_PIN	13

// joystick
#define JOY_UP_PIN		12
#define JOY_DOWN_PIN	14
#define JOY_LEFT_PIN	27
#define JOY_RIGHT_PIN	26
#define JOY_MID_PIN		25

// OLED uses standard pins for SCL and SDA

// configurable values - saved in eeprom
int dropOneSize = 100;
int dropTwoSize = 100;

int delayBeforeShooting = 1000;
int delayBeforeDrops = 500;
int delayBetweenDrops = 88;
int delayBeforeFlash = 333;
int delayAfterShooting = 300;

int flashDuration = 5;
int screenTimeout = 5;
bool useTwoDrops = true;

#define EEPROM_SIZE 45

// run time values
bool joystickRight = true;
int lastJoyLeft = HIGH;
int lastJoyRight = HIGH;

int screenTimer = 0;
int menuItem = 1;
boolean updateScreen = true;

int delayVal = 0;
int	downwardsDelay = 500;
int	sidewaysDelay = 1000;
int	maxSidewaysDelay = 1000;

double accelerationFactor = 1.3;
int accelerationTimerStart = 10;
int accelerationTimer = 0;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);	 // OLEDs without Reset of the Display

AsyncWebServer server(80);

void InitWiFi() {

	Serial.println("Initialising wifi");
	PrintToOLED(0, 2, F("Connecting wifi "));
	WiFi.mode(WIFI_STA);
	WiFi.begin(SECRET_SSID2 , SECRET_PASS2 );
	Serial.print("  - connecting to WiFi ..");
	int col = 0;
	while (WiFi.status() != WL_CONNECTED) {
		PrintToOLED(col, 3, F("."));
		Serial.print('.');
		delay(1000);
		col = col + 1;
		if (col == 16) {
			col = 0;
			PrintToOLED(0, 3, F("								"));
		}
	}
	Serial.println("");
	Serial.print(F("  - connected on IP:"));
	Serial.println(WiFi.localIP());
	PrintToOLED(0, 3, F("Connected on IP:"));
	PrintToOLED(0, 4, WiFi.localIP());
}

void InitServer() {
	
	Serial.println("Initialising server");
	PrintToOLED(0, 5, F("	Init server	 "));
	server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
		// get the params here

		String paramVal;
		if (request->hasParam("drop1")) {
			paramVal = request->getParam("drop1")->value();
			dropOneSize = paramVal.toInt();
		}
		if (request->hasParam("drop2")) {
			paramVal = request->getParam("drop2")->value();
			dropTwoSize = paramVal.toInt();
		}
		if (request->hasParam("predelay")) {
			paramVal = request->getParam("predelay")->value();
			delayBeforeShooting = paramVal.toInt();
		}
		if (request->hasParam("postdelay")) {
			paramVal = request->getParam("postdelay")->value();
			delayAfterShooting = paramVal.toInt();
		}
		if (request->hasParam("dropdelay")) {
			paramVal = request->getParam("dropdelay")->value();
			delayBeforeDrops = paramVal.toInt();
		}
		if (request->hasParam("betweendelay")) {
			paramVal = request->getParam("betweendelay")->value();
			delayBetweenDrops = paramVal.toInt();
		}
		
		if (request->hasParam("flashdelay")) {
			paramVal = request->getParam("flashdelay")->value();
			delayBeforeFlash = paramVal.toInt();
		}
		if (request->hasParam("flashduration")) {
			paramVal = request->getParam("flashduration")->value();
			flashDuration = paramVal.toInt();
		}
		if (request->hasParam("screentimeout")) {
			paramVal = request->getParam("screentimeout")->value();
			screenTimeout = paramVal.toInt();
		}
		if (request->hasParam("twodrops")) {
			paramVal = request->getParam("screentimeout")->value();
			paramVal.toLowerCase();
			if (paramVal == "true" || paramVal == "yes" || paramVal == "1") {
				useTwoDrops = true;
			} else {
				useTwoDrops = false;
			}
		}

		request->send(200, "text/plain", "OK");
	});
	server.on("/shoot", HTTP_GET, [](AsyncWebServerRequest *request){
		TakeAPhoto();
		request->send(200, "text/plain", "OK");
	});
	
	PrintToOLED(0, 6, F("	Start server	"));
	server.begin();
}

unsigned long eeprom_crc()
{
	const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned long crc = ~0L;

	for (int index = 0; index < 17 ; ++index) {
		byte epv = EEPROM.read(index);
		crc = crc_table[(crc ^ epv) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (epv >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}
	return crc;
}

void InitSettings() {

	Serial.println("Initialising settings");
	EEPROM.begin(EEPROM_SIZE);
	unsigned long calculatedCrc = eeprom_crc();
	unsigned long storedCrc;
	EEPROM.get(37, storedCrc);
	if (storedCrc != calculatedCrc) {
		Serial.println("  - Mismatched CRC, saving default values");
		// they don't match, so let's save the default values
		SaveSettings();
		return;
	}

	Serial.println("  - CRC OK getting saved values");
	EEPROM.get(0, dropOneSize);
	EEPROM.get(4, dropTwoSize);
	EEPROM.get(8, delayBeforeShooting);
	EEPROM.get(12, delayBeforeDrops);
	EEPROM.get(16, delayBetweenDrops);
	EEPROM.get(20, delayBeforeFlash);
	EEPROM.get(24, delayAfterShooting);
	EEPROM.get(28, flashDuration);
	EEPROM.get(32, screenTimeout);
	EEPROM.get(36, useTwoDrops);
}

void SaveSettings() {
	
	Serial.println("Saving settings");
	EEPROM.put(0, dropOneSize);
	EEPROM.put(4, dropTwoSize);
	EEPROM.put(8, delayBeforeShooting);
	EEPROM.put(12, delayBeforeDrops);
	EEPROM.put(16, delayBetweenDrops);
	EEPROM.put(20, delayBeforeFlash);
	EEPROM.put(24, delayAfterShooting);
	EEPROM.put(28, flashDuration);
	EEPROM.put(32, screenTimeout);
	EEPROM.put(36, useTwoDrops);
	EEPROM.commit();
	
	unsigned long calculatedCrc = eeprom_crc();
	// save calculated CRC to eeprom
	EEPROM.put(37, calculatedCrc);
	EEPROM.commit();
}

void InitGPIO() {

	Serial.println("Initialising GPIO pins");

	pinMode(SOLENOID_PIN, OUTPUT);
	digitalWrite(SOLENOID_PIN, LOW);

	pinMode(FOCUS_PIN, OUTPUT);
	digitalWrite(FOCUS_PIN, LOW);

	pinMode(CAMERA_PIN, OUTPUT);
	digitalWrite(CAMERA_PIN, LOW);

	pinMode(FLASH_PIN, OUTPUT);
	digitalWrite(FLASH_PIN, LOW);

	pinMode(JOY_UP_PIN, INPUT_PULLUP);
	digitalWrite(JOY_UP_PIN, HIGH);

	pinMode(JOY_DOWN_PIN, INPUT_PULLUP);
	digitalWrite(JOY_DOWN_PIN, HIGH);
	
	pinMode(JOY_LEFT_PIN, INPUT_PULLUP);
	digitalWrite(JOY_LEFT_PIN, HIGH);
	
	pinMode(JOY_RIGHT_PIN, INPUT_PULLUP);
	digitalWrite(JOY_RIGHT_PIN, HIGH);
	
	pinMode(JOY_MID_PIN, INPUT_PULLUP);
	digitalWrite(JOY_MID_PIN, HIGH);

	
}

void InitSerial() {
	Serial.begin(9600);
	delay(2000);
	Serial.println("");
	Serial.println("Initialised serial");
}
void setup() {

	InitSerial();
	InitSettings();
	InitGPIO();
	InitOLED();
	InitWiFi();
	InitServer();

	delay(1000);
	ResetOLED();
}

void loop() {

	// first, handle updating the screen from the last rotary	use
	if (updateScreen) {
		updateScreen = false;
		PrintToOLED(0, 0, F(" WATER DROPPER "));
		switch (menuItem) {
			case 1:
				PrintToOLED(0, 3, F("Pre-shoot delay "));
				PrintToOLEDFullLine(4, 6, delayBeforeShooting);
				break;
			case 2:
				PrintToOLED(0, 3, F("Pre-drop delay "));
				PrintToOLEDFullLine(4, 6, delayBeforeDrops);
				break;
			case 3:
				PrintToOLED(0, 3, F("First drop size "));
				PrintToOLEDFullLine(4, 6, dropOneSize);
				break;
			case 4:
				PrintToOLED(0, 3, F("Use two drops   "));
				if (useTwoDrops) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case 5:
				PrintToOLED(0, 3, F("Second drop size"));
				PrintToOLEDFullLine(4, 6, dropTwoSize);
				break;
			case 6:
				PrintToOLED(0, 3, F("Inter drop delay"));
				PrintToOLEDFullLine(4, 6, delayBetweenDrops);
				break;
			case 7:
				PrintToOLED(0, 3, F("Pre-flash delay "));
				PrintToOLEDFullLine(4, 6, delayBeforeFlash);
				break;
			case 8:
				PrintToOLED(0, 3, F("Post shoot delay"));
				PrintToOLEDFullLine(4, 6, delayAfterShooting);
				break;
			case 9:
				PrintToOLED(0, 3, F("Screen timeout  "));
				PrintToOLEDFullLine(4, 6, screenTimeout);
				break;
			case 10:
				PrintToOLED(0, 3, F("Save settings ? "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
		}
	}

	// if we've used the joystick, there will be a delay that we want to
	// do AFTER we update the OLED, but only once. This value will get changed
	// every time we use the joystick, as sometimes it will speed up
	if (delayVal > 0) {
		delay(delayVal);
		delayVal = 0;
	}

	int thisJoyUp = digitalRead(JOY_UP_PIN);
	int thisJoyDown = digitalRead(JOY_DOWN_PIN);
	int thisJoyLeft = digitalRead(JOY_LEFT_PIN);
	int thisJoyRight = digitalRead(JOY_RIGHT_PIN);

	if (thisJoyLeft == LOW || thisJoyRight == LOW) {
		if (thisJoyLeft == LOW) {
			Serial.println("joy left");
		} else {
			Serial.println("joy right");
		}
		// if the joystick has moved sideways, we change the setting value

		// joystick has been used, so keep the screen on
		screenTimer = screenTimeout * 1000;

		// also, the screen will need to be updated
		updateScreen = true;					

		joystickRight = (thisJoyRight == LOW);
		if (thisJoyLeft == lastJoyLeft && thisJoyRight == lastJoyRight) {
			// the joystick is the same position as last time, so speed up
			if (sidewaysDelay > 10) {
				sidewaysDelay = sidewaysDelay - 100;
			}
		} else {
			// otherwise, it has changed, so set the delay back to the max
			sidewaysDelay = maxSidewaysDelay;
		}

		// modify the selected item
		switch (menuItem) {
			case 1:
				delayBeforeShooting = NormaliseIntVal(delayBeforeShooting, 10, 0, 5000);
				break;
			case 2:
				delayBeforeDrops = NormaliseIntVal(delayBeforeDrops, 10, 0, 5000);
				break;
			case 3:
				dropOneSize = NormaliseIntVal(dropOneSize, 1, 10, 300);
				break;
			case 4:
				useTwoDrops = !useTwoDrops;
				break;
			case 5:
				dropTwoSize = NormaliseIntVal(dropTwoSize, 1, 10, 300);
				break;
			case 6:
				delayBetweenDrops = NormaliseIntVal(delayBetweenDrops, 1, 10, 2000);
				break;
			case 7:
				delayBeforeFlash = NormaliseIntVal(delayBeforeFlash, 1, 10, 2000);
				break;
			case 8:
				delayAfterShooting = NormaliseIntVal(delayAfterShooting, 1, 0, 5000);
				break;
			case 9:
				screenTimeout = NormaliseIntVal(screenTimeout, 1, 1, 20);
				break;
			case 10:
				SaveSettings();
				PrintToOLEDFullLine(4, 6, "saved");
				updateScreen = false;
				break;
		}
		lastJoyLeft = thisJoyLeft;
		lastJoyRight = thisJoyRight;
		delayVal = sidewaysDelay;

	} else if (thisJoyUp == LOW || thisJoyDown == LOW) {

		// if the joystick has moved vertically, we change which setting we are... setting

		// joystick has been used, so keep the screen on
		screenTimer = screenTimeout * 1000;

		// also, the screen will need to be updated
		updateScreen = true;

		// change the menu item, but skip the second drop settings if
		// we are not using two drops
		if (thisJoyDown == LOW) {
			menuItem++;
			if ((useTwoDrops == false) && (menuItem == 5 || menuItem == 6)) {
				menuItem = 7;
			}
		} else {
			menuItem--;
			if ((useTwoDrops == false) && (menuItem == 5 || menuItem == 6)) {
				menuItem = 4;
			}
		}

		// loop around
		if (menuItem == 0) {
			menuItem = 10;	
		} else if (menuItem == 11) {
			menuItem = 1;
		}
		delayVal = downwardsDelay;

	} else {

		// check if the joystick button has been pressed, to initiate the process

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			TakeAPhoto();
		}
		digitalWrite(CAMERA_PIN, LOW);
		digitalWrite(FOCUS_PIN, LOW);
		digitalWrite(FLASH_PIN, LOW);
		digitalWrite(SOLENOID_PIN, LOW);
	}
}

void InitOLED() {

	Serial.println("Initialising OLED");
	u8x8.begin();
	u8x8.setFont(u8x8_font_chroma48medium8_r);
	PrintToOLED(0, 0, F(" WATER DROPPER "));
	PrintToOLED(0, 1, F("  Initializing  "));
}

void ResetOLED() {
	ClearScreen();
	PrintToOLED(0, 0, F(" WATER DROPPER "));
}

void ClearScreen() {
	u8x8.clearDisplay();
}

void TakeAPhoto() {

	Serial.println("Running photo sequence");
	ClearScreen();													// want the screen off when shooting

	
	delay(delayBeforeShooting);

	Serial.println("Setting camera pin HIGH");
	digitalWrite(CAMERA_PIN, HIGH);									//trigger the camera, which should be in bulb mode
	
	delay(delayBeforeDrops);
	Serial.println("Setting solenoid pin HIGH");
	digitalWrite(SOLENOID_PIN, HIGH);								// open the valve
	
	delay(dropOneSize);												// hold the value open for the specified time (milliseconds) 
	Serial.println("Setting solenoid pin LOW");
	digitalWrite(SOLENOID_PIN, LOW);								// close the valve

	if (useTwoDrops) {
		delay(delayBetweenDrops);									// keeps valve closed for the time between drips
		Serial.println("Setting solenoid pin HIGH");
		digitalWrite(SOLENOID_PIN, HIGH);							// makes the valve open for second drip

		delay(dropTwoSize);											// keeps valve open for ValveOpen time (milliseconds)
		Serial.println("Setting solenoid pin LOW");
		digitalWrite(SOLENOID_PIN, LOW);							// closes the valve
	}

	delay(delayBeforeFlash);										// wait the flash delay time to trigger flash
	Serial.println("Setting flash pin HIGH");
	digitalWrite(FLASH_PIN, HIGH);									// trigger the flash

	delay(flashDuration);											// keep flash trigger pin high long enough to trigger flash
	Serial.println("Setting flash pin LOW");
	digitalWrite(FLASH_PIN, LOW);									// turn off flash trigger

	delay(delayAfterShooting);										// wait the flash delay time to trigger flash
	Serial.println("Setting camera pin LOW");
	digitalWrite(CAMERA_PIN, LOW);									//trigger the camera, which should be in bulb mode

	delay(100);														// keeps the system in a pause mode to avoid false triggers
	Serial.println("Done sequence");

	updateScreen = true;
}


int NormaliseIntVal(int v, int stepamount, int minimum, int maximum) {

	int newValue = v;
	if (joystickRight) {
		newValue += stepamount;
	} else {
		newValue -= stepamount;
	}
	if (newValue < minimum) {
		newValue = minimum;
	} else if (newValue > maximum) {
		newValue = maximum;
	}
	return newValue;
}


void PrintToOLED(int col, int row, int ival) {

	String s = String(ival);
	const char *mes = s.c_str();
	PrintToOLED(col, row, mes);
}

void PrintToOLEDFullLine(int col, int row, int ival) {

	String s = String(ival);
	PrintToOLEDFullLine(col, row, s);
}

void PrintToOLEDFullLine(int col, int row, const char * message) {
	String s = String(message);
	PrintToOLEDFullLine(col, row, s);
}

void PrintToOLED(int col, int row, String	message) {

	const char *mes = message.c_str();
	PrintToOLED(col, row, mes);
}

void PrintToOLED(int col, int row, const char * message) {
	 u8x8.drawString(col, row, message);
}

void PrintToOLEDFullLine(int col, int row, String	message) {

	String blank = "                ";
	int charsAvailable = 16 - col;

	for (int i = 0; i < message.length() && i < charsAvailable; i++) {
		blank.setCharAt(i + col, message.charAt(i));
	}

	const char *mes = blank.c_str();
	PrintToOLED(0, row, mes);
}
