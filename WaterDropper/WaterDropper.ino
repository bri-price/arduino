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

// configurable values
int dropOneSize = 100;
int dropTwoSize = 100;
int delayBeforeShooting = 1000;
int delayAfterShooting = 300;
int delayBetweenDrops = 88;
int delayBeforeFlash = 333;
int flashDuration = 5;					// 5
int minimumRange = 10;
int maximumRange = 90;
int screenTimeout = 5;
bool useTwoDrops = true;


#define EEPROM_SIZE 21



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
double rotaryAccelerator = 1;
int accelerationTimerStart = 10;
int accelerationTimer = 0;

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);	 // OLEDs without Reset of the Display

AsyncWebServer server(80);

// Initialize WiFi
void InitWiFi() {
	PrintToLCD(0, 2, F("Connecting wifi "));
	WiFi.mode(WIFI_STA);
	WiFi.begin(SECRET_SSID2 , SECRET_PASS2 );
	Serial.print("Connecting to WiFi ..");
	int col = 0;
	while (WiFi.status() != WL_CONNECTED) {
		PrintToLCD(col, 3, F("."));
		Serial.print('.');
		delay(1000);
		col = col + 1;
		if (col == 16) {
			col = 0;
			PrintToLCD(0, 3, F("								"));
		}
	}
	PrintToLCD(0, 3, F("Connected on IP:"));
	PrintToLCD(0, 4, WiFi.localIP());
}

void InitServer() {
	
	PrintToLCD(0, 5, F("  Init server   "));
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
	
		if (request->hasParam("minrange")) {
			paramVal = request->getParam("minrange")->value();
			minimumRange = paramVal.toInt();
		}
		if (request->hasParam("maxrange")) {
			paramVal = request->getParam("maxrange")->value();
			maximumRange = paramVal.toInt();
		}
	
	 
		request->send(200, "text/plain", "OK");
	});
	server.on("/fire", HTTP_GET, [](AsyncWebServerRequest *request){
		TakeAPhoto();
		request->send(200, "text/plain", "OK");
	});
	server.on("/test/camera", HTTP_GET, [](AsyncWebServerRequest *request){
		TestCamera();
		request->send(200, "text/plain", "OK");
	});
	server.on("/test/flash", HTTP_GET, [](AsyncWebServerRequest *request){
		TestFlash();
		request->send(200, "text/plain", "OK");
	});
	server.on("/test/solenoid", HTTP_GET, [](AsyncWebServerRequest *request){
		TestSolenoid();
		request->send(200, "text/plain", "OK");
	});
	PrintToLCD(0, 6, F("	Start server	"));
	server.begin();
}

unsigned long eeprom_crc()
{

  const unsigned long crc_table[16] =
  {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = 0 ; index < EEPROM.length() - 4  ; ++index)
  {
    crc = crc_table[(crc ^ EEPROM.read(index)) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM.read(index) >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

/*
 * int dropOneSize = 100;
int dropTwoSize = 100;
int delayBeforeShooting = 1000;
int delayAfterShooting = 300;
int delayBetweenDrops = 88;
int delayBeforeFlash = 333;
int flashDuration = 5;					// 5
int screenTimeout = 5;
bool useTwoDrops = true;
 */

/*
int readIntFromEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + EEPROM.read(address + 1);
}

int readBoolFromEEPROM(int address)
{
  return (EEPROM.read(address) > 0);
}
*/

void InitVariables() {
	
	EEPROM.begin(EEPROM_SIZE);

	unsigned long calculatedCrc = eeprom_crc();
  Serial.print("calculated CRC32 of EEPROM data: 0x");
  Serial.println(calculatedCrc, HEX);

	unsigned long storedCrc;
  EEPROM.get(EEPROM.length() - 4, storedCrc);
  Serial.print("stored CRC32 of EEPROM data: 0x");
  Serial.println(storedCrc, HEX);
  if (storedCrc != calculatedCrc) {
  	// they don't match, so let's save the default values
  	UpdateVariables();
  	return;
  }

  // crc is correct, so read the values.
  /*
	dropOneSize = readIntFromEEPROM(0);
	dropTwoSize = readIntFromEEPROM(2);
	delayBeforeShooting = readIntFromEEPROM(4);
	delayAfterShooting = readIntFromEEPROM(6);
	delayBetweenDrops = readIntFromEEPROM(8);
	delayBeforeFlash = readIntFromEEPROM(10);
	flashDuration = readIntFromEEPROM(12);
	screenTimeout = readIntFromEEPROM(14);
	useTwoDrops = readBoolFromEEPROM(16);
	*/
	
	EEPROM.get(0, dropOneSize);
	EEPROM.get(2, dropTwoSize);
	EEPROM.get(4, delayBeforeShooting);
	EEPROM.get(6, delayAfterShooting);
	EEPROM.get(8, delayBetweenDrops);
	EEPROM.get(10, delayBeforeFlash);
	EEPROM.get(12, flashDuration);
	EEPROM.get(14, screenTimeout);
	EEPROM.get(16, useTwoDrops);
}

/*
void writeIntToEEPROM(int address, int number)
{ 
	EEPROM.write(address, number >> 8);
	EEPROM.write(address + 1, number & 0xFF);
}

void writeBoolToEEPROM(int address, bool b)
{
	if (b) {
		EEPROM.write(address, 1);
	} else {
		EEPROM.write(address, 0);
	}
}
*/

void UpdateVariables() {
	
	EEPROM.put(0, dropOneSize);
	EEPROM.put(2, dropTwoSize);
	EEPROM.put(4, delayBeforeShooting);
	EEPROM.put(6, delayAfterShooting);
	EEPROM.put(8, delayBetweenDrops);
	EEPROM.put(10, delayBeforeFlash);
	EEPROM.put(12, flashDuration);
	EEPROM.put(14, screenTimeout);
	EEPROM.put(16, useTwoDrops);

	
	unsigned long calculatedCrc = eeprom_crc();
	Serial.print("calculated CRC32 of EEPROM data: 0x");
	Serial.println(calculatedCrc, HEX);

	// save calculated CRC to eeprom
	EEPROM.put(EEPROM.length() - 4, calculatedCrc);
//	EEPROM.commit();
}


void setup() {

	Serial.begin(9600);

	InitVariables();

	pinMode(SOLENOID_PIN, OUTPUT);
	digitalWrite(SOLENOID_PIN, LOW);

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

	SetupLCD();
	InitWiFi();
	InitServer();
	Serial.println("Ready");
	PrintToLCD(0, 7, F("		 Ready			"));

	delay(2000);
	InitLCD();
}

void loop() {

	// first, handle updating the screen from the last rotary	use
	if (updateScreen) {
		updateScreen = false;
		PrintToLCD(0, 1, F(" WATER	DROPPER "));
		switch (menuItem) {
			case 1:
				PrintToLCD(0, 3, F("Pre-shoot delay "));
				PrintToLCDFullLine(4, 6, delayBeforeShooting);
				break;
			case 2:
				PrintToLCD(0, 3, F("First drop size "));
				PrintToLCDFullLine(4, 6, dropOneSize);
				break;
			case 3:
				PrintToLCD(0, 3, F("Use two drops	 "));
				if (useTwoDrops) {
					PrintToLCDFullLine(4, 6, F("Yes"));
				} else {
					PrintToLCDFullLine(4, 6, F("No ")); 
				} 
				break;
			case 4:
				PrintToLCD(0, 3, F("Second drop size"));
				PrintToLCDFullLine(4, 6, dropTwoSize);
				break;
			case 5:
				PrintToLCD(0, 3, F("Inter drop delay"));
				PrintToLCDFullLine(4, 6, delayBetweenDrops);
				break;
			case 6:
				PrintToLCD(0, 3, F("Pre-flash delay "));
				PrintToLCDFullLine(4, 6, delayBeforeFlash);
				break;
			case 7:
				PrintToLCD(0, 3, F("Post shoot delay"));
				PrintToLCDFullLine(4, 6, delayAfterShooting);
				break;
			case 8:
				PrintToLCD(0, 3, F("Min sensor range"));
				PrintToLCDFullLine(4, 6, minimumRange);
				break;
			case 9:
				PrintToLCD(0, 3, F("Max sensor range"));
				PrintToLCDFullLine(4, 6, maximumRange);
				break;
			case 10:
				PrintToLCD(0, 3, F("Screen timeout	"));
				PrintToLCDFullLine(4, 6, screenTimeout);
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


	/*
	 * Comment this back if you need to debug the joystick
	if (newju == LOW) {
		Serial.println("Joystick UP");
	}
	if (newjd == LOW) {
		Serial.println("Joystick DOWN");
	}
	if (newjl == LOW) {
		Serial.println("Joystick LEFT");
	}
	if (newjr == LOW) {
		Serial.println("Joystick RIGHT");
	}
	*/

	if (thisJoyLeft == LOW || thisJoyRight == LOW) {

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
				dropOneSize = NormaliseIntVal(dropOneSize, 1, 10, 300);
				break;
			case 3:
				useTwoDrops = !useTwoDrops;
				break;
			case 4:
				dropTwoSize = NormaliseIntVal(dropTwoSize, 1, 10, 300);
				break;
			case 5:
				delayBetweenDrops = NormaliseIntVal(delayBetweenDrops, 1, 10, 2000);
				break;
			case 6:
				delayBeforeFlash = NormaliseIntVal(delayBeforeFlash, 1, 10, 2000);
				break;
			case 7:
				delayAfterShooting = NormaliseIntVal(delayAfterShooting, 1, 0, 5000);
				break;
			case 8:
				minimumRange = NormaliseIntVal(minimumRange, 1, 10, 120);
				break;
			case 9:
				maximumRange = NormaliseIntVal(maximumRange, 1, 10, 120);
				break;
			case 10:
				screenTimeout = NormaliseIntVal(screenTimeout, 1, 1, 20);
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
			if ((useTwoDrops == false) && (menuItem == 4 || menuItem == 5)) {
				menuItem = 6;
			}
		} else {
			menuItem--;
			if ((useTwoDrops == false) && (menuItem == 4 || menuItem == 5)) {
				menuItem = 3;
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
			Serial.println("Button pressed");
			UpdateVariables();
			TakeAPhoto();
		}
		digitalWrite(CAMERA_PIN, LOW);
		digitalWrite(FLASH_PIN, LOW);
		digitalWrite(SOLENOID_PIN, LOW);
	}
}

void SetupLCD() {

	u8x8.begin();
	u8x8.setFont(u8x8_font_chroma48medium8_r);
	PrintToLCD(0, 0, F(" WATER  DROPPER "));
	PrintToLCD(0, 1, F("  Initializing  "));
}

void InitLCD() {
	ClearScreen();
	PrintToLCD(0, 0, F(" WATER  DROPPER "));
}

void ClearScreen() {
	u8x8.clearDisplay();
}

void TestCamera() {
	InitLCD();
	PrintToLCD(2, 2, F("TEST CAMERA"));
	PrintToLCD(7, 3, F("ON"));
	Serial.println("Setting camera pin HIGH");
	digitalWrite(CAMERA_PIN, HIGH);		 //trigger the camera, which should be in bulb mode
	delay(2000);
	digitalWrite(CAMERA_PIN, LOW);		 //trigger the camera, which should be in bulb mode
	PrintToLCD(7, 3, F("OFF"));
	delay(2000);
	InitLCD();
	updateScreen = true;
}

void TestFlash() {
	InitLCD();
	PrintToLCD(2, 2, F("TEST FLASH"));
	digitalWrite(FLASH_PIN, HIGH);		 //trigger the camera, which should be in bulb mode
	delay(5);
	digitalWrite(FLASH_PIN, LOW);		 //trigger the camera, which should be in bulb mode
	PrintToLCD(6, 3, F("DONE"));
	delay(2000);
	InitLCD();
	updateScreen = true;
}

void TestSolenoid() {
	InitLCD();
	PrintToLCD(1, 2, F("TEST SOLENOID"));
	PrintToLCD(7, 3, F("ON"));
	digitalWrite(SOLENOID_PIN, HIGH);		 //trigger the camera, which should be in bulb mode
	delay(2000);
	digitalWrite(SOLENOID_PIN, LOW);		 //trigger the camera, which should be in bulb mode
	PrintToLCD(7, 3, F("OFF"));
	delay(2000);
	InitLCD();
	updateScreen = true;
}

void TakeAPhoto() {

	ClearScreen();																	// want the screen off when shooting

	Serial.println("Running photo sequence");
	
	delay(delayBeforeShooting);

	Serial.println("Setting camera pin HIGH");
	digitalWrite(CAMERA_PIN, HIGH);									//trigger the camera, which should be in bulb mode

	delay(delayAfterShooting);
	
	Serial.println("Setting solenoid pin HIGH");
	digitalWrite(SOLENOID_PIN, HIGH);								// open the valve
	delay(dropOneSize);															// hold the value open for the specified time (milliseconds) 
	Serial.println("Setting solenoid pin LOW");
	digitalWrite(SOLENOID_PIN, LOW);								// close the valve

	if (useTwoDrops) {
		delay(delayBetweenDrops);											// keeps valve closed for the time between drips
		Serial.println("Setting solenoid pin HIGH");
		digitalWrite(SOLENOID_PIN, HIGH);							// makes the valve open for second drip
		delay(dropTwoSize);														// keeps valve open for ValveOpen time (milliseconds)
		Serial.println("Setting solenoid pin LOW");
		digitalWrite(SOLENOID_PIN, LOW);							// closes the valve
	}

	delay(delayBeforeFlash);												// wait the flash delay time to trigger flash
	
	Serial.println("Setting flash pin HIGH");
	digitalWrite(FLASH_PIN, HIGH);									// trigger the flash
	delay(flashDuration);														// keep flash trigger pin high long enough to trigger flash
	Serial.println("Setting flash pin LOW");
	digitalWrite(FLASH_PIN, LOW);										// turn off flash trigger

	delay(2000);																		// keeps the system in a pause mode to avoid false triggers
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


void PrintToLCD(int col, int row, int ival) {

	String s = String(ival);
	const char *mes = s.c_str();
	PrintToLCD(col, row, mes);
}

void PrintToLCDFullLine(int col, int row, int ival) {

	String s = String(ival);
	PrintToLCDFullLine(col, row, s);
}

void PrintToLCDFullLine(int col, int row, const char * message) {
	String s = String(message);
	PrintToLCDFullLine(col, row, s);
}

void PrintToLCD(int col, int row, String	message) {

	const char *mes = message.c_str();
	PrintToLCD(col, row, mes);
}

void PrintToLCD(int col, int row, const char * message) {
	 u8x8.drawString(col, row, message);
}

void PrintToLCDFullLine(int col, int row, String	message) {

	String blank = "                ";
	int charsAvailable = 16 - col;

	for (int i = 0; i < message.length() && i < charsAvailable; i++) {
		blank.setCharAt(i + col, message.charAt(i));
	}

	const char *mes = blank.c_str();
	PrintToLCD(0, row, mes);
}
