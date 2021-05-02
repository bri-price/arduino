/*
 * 
 * 		Working Light Wand 
 * 		
 * 		ESP32 dev module or Nano Iot 33
 * 
 */

// Library initialization
#include <FastLED.h>				// Library for the LED Strip
#include <SD.h>							// Library for the SD Card

#include <Arduino.h>
#include <U8x8lib.h>

#include <SPI.h>
#include <Wire.h>

typedef unsigned char prog_uchar;

#define SERIAL_BAUD 115200
#define LOOP_CYCLES 2000

//#define ESP32
#define NANOIOT
// ******************************************** PIN ASSIGNMENTS  *******************************

#ifdef ESP32
// LED strip data
#define DATA_PIN					32
// SD CS
#define SDSS_PIN					2		
// Joystick
#define JOYU_PIN					12		
#define JOYD_PIN					14
#define JOYL_PIN					27			
#define JOYR_PIN					26			
#define JOYB_PIN					25		
#endif

#ifdef NANOIOT
// LED strip data
#define DATA_PIN					7
// SD CS
#define SDSS_PIN					10		
// Joystick
#define JOYU_PIN					2		
#define JOYD_PIN					3
#define JOYL_PIN					4			
#define JOYR_PIN					5			
#define JOYB_PIN					6		
#endif

// ******************************************** CONFIGURATION DATA  *******************************
// LCD Display
#define BACKLIGHTTIMEOUT			360		// Adjust this to a larger number if you want a longer delay

// LED Strip
#define NUM_LEDS					144		// Set the number of LEDs the LED Strip

// Joystick
#define KEY_NONE					0
#define KEY_UP						1
#define KEY_DOWN					2
#define KEY_LEFT					3
#define KEY_RIGHT					4
#define KEY_BUTTON					5

int sabreColours[4][3] = {{210,0,0},{0,200,0},{0,0,180},{220,220,220}};

// ******************************************** PROGRAM VARIABLES  *******************************

#define	WAND_PIXELSTICK				1
#define WAND_SABRE					2

#define SABRE_SOLID					1
#define SABRE_PULSE					2
#define SABRE_THROB					3

#define CONFIG_FILE		"config.bin"

typedef struct {
	int wandMode;				// = WAND_SABRE;
	int sabreSpeed;				// = 1
	int sabreLength;			// = 0
	int sabreColour;			// = 1
	int sabreMode;				// = SABRE_SOLID;
	int fxSpeed;				// = 1;
	int frameDelay;				// = 15;							// default for the frame delay 
	int frameBlankDelay;		// = 0;						// Default Frame blank delay of 0
	int initDelay;				// = 0;									// Variable for delay between button press and start of light sequence
	int repeat;					// = 0;										// Variable to select auto repeat (until select button is pressed again)
	int repeatDelay;			// = 0;								// Variable for delay between repeats
	int repeatTimes;			// = 1;							// Variable to keep track of number of repeats
	int brightness;				// = 90;								// Variable and default for the Brightness of the strip
	boolean cycleAllImages;		// = false;			//cycle through all images
	boolean imageRightToLeft;	// = false;
	boolean imageUpsideDown;	// = false;
	boolean sabreFade;			// = true;

} CONFIGURATION;

CONFIGURATION config;

int interruptPressed = 0;						// variable to keep track if user wants to interrupt display session.
int loopCounter = 0;								// count loops as a means to avoid extra presses of keys
int menuItem = 1;								// Variable for current main menu selection
boolean updateScreen = true;			// Added to minimize screen update flicker (Brian Heiland)
boolean cycleAllImagesOneshot = false;
int cycleImageCount= 0;
byte x;
int g = 0;												// Variable for the Green Value
int b = 0;												// Variable for the Blue Value
int r = 0;												// Variable for the Red Value

// Backlight
boolean BackLightTimer = false;
int BackLightTemp =	BACKLIGHTTIMEOUT;

// Joystick
int key = KEY_NONE;
int oldkey = KEY_NONE;

// SD Card variables and assignments
File root;
File dataFile;
File configFile;
String m_CurrentFilename = "";
int m_FileIndex = 0;
int m_NumberOfFiles = 0;
String m_FileNames[200];					


// ******************************************** INITIALISE COMPONENT OBJECTS  *******************************

// LED Display
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // OLEDs without Reset of the Display

// LED Strip
CRGB leds[NUM_LEDS];

uint32_t ReadLong() {
	uint32_t retValue;
	byte incomingbyte;
	 
	incomingbyte = ReadByte();
	retValue = (uint32_t)((byte)incomingbyte);
	 
	incomingbyte = ReadByte();
	retValue += (uint32_t)((byte)incomingbyte) << 8;
	 
	incomingbyte = ReadByte();
	retValue += (uint32_t)((byte)incomingbyte) << 16;
	 
	incomingbyte = ReadByte();
	retValue += (uint32_t)((byte)incomingbyte) << 24;
	 
	return retValue;
}

uint16_t ReadInt() {
	byte incomingbyte;
	uint16_t retValue;
	 
	incomingbyte = ReadByte();
	retValue += (uint16_t)((byte)incomingbyte);
	 
	incomingbyte = ReadByte();
	retValue += (uint16_t)((byte)incomingbyte) << 8;
	 
	return retValue;
}
	
int ReadByte() {
	int retbyte = -1;
	while (retbyte < 0) 
		retbyte = dataFile.read();
	return retbyte;
}

int ReadBool() {
	int retbyte = -1;
	while (retbyte < 0) 
		retbyte = dataFile.read();
	return (retbyte > 0);
}

void WriteByte(byte wrtByte) {

	dataFile.write(wrtByte);
}

void writeBool(bool wrtBool) {

	byte wrtByte = 0;
	if (wrtBool) {
		wrtByte = 1;
	}
	dataFile.write(wrtByte);
}

void WriteInt(int wrtInt) {
	byte nextByte = (byte)wrtInt;
	WriteByte(nextByte);
	nextByte = (byte)(wrtInt >> 8);
	WriteByte(nextByte);
}

void WriteLong(int wrtLong) {
	byte nextByte = (byte)wrtLong;
	WriteByte(nextByte);
	nextByte = (byte)(wrtLong >> 8);
	WriteByte(nextByte);
	nextByte = (byte)(wrtLong >> 16);
	WriteByte(nextByte);
	nextByte = (byte)(wrtLong >> 24);
	WriteByte(nextByte);
}


bool InitConfig() {

	config.wandMode = WAND_SABRE;
	config.sabreSpeed = 1;
	config.sabreLength = 0;
	config.sabreColour = 1;
	config.sabreMode = SABRE_SOLID;
	config.fxSpeed = 1;
	config.frameDelay = 15;							// default for the frame delay 
	config.frameBlankDelay = 0;						// Default Frame blank delay of 0
	config.initDelay = 0;									// Variable for delay between button press and start of light sequence
	config.repeat = 0;										// Variable to select auto repeat (until select button is pressed again)
	config.repeatDelay = 0;								// Variable for delay between repeats
	config.repeatTimes = 1;							// Variable to keep track of number of repeats
	config.brightness = 90;								// Variable and default for the Brightness of the strip
	config.cycleAllImages = false;			//cycle through all images
	config.imageRightToLeft = false;
	config.imageUpsideDown = false;
	config.sabreFade = true;

	Serial.print("config.wandMode = ");
	Serial.println(config.wandMode);
	Serial.print("config.sabreSpeed = ");
	Serial.println(config.sabreSpeed);
	Serial.print("config.sabreLength = ");
	Serial.println(config.sabreLength);
	Serial.print("config.sabreColour = ");
	Serial.println(config.sabreColour);
	Serial.print("config.sabreMode = ");
	Serial.println(config.sabreMode);
	Serial.print("config.sabreFade = ");
	Serial.println(config.sabreFade);
	Serial.print("config.fxSpeed = ");
	Serial.println(config.fxSpeed);
	Serial.print("config.frameDelay = ");
	Serial.println(config.frameDelay);
	Serial.print("config.frameBlankDelay = ");
	Serial.println(config.frameBlankDelay);
	Serial.print("config.initDelay = ");
	Serial.println(config.initDelay);
	Serial.print("config.repeat = ");
	Serial.println(config.repeat);
	Serial.print("config.repeatDelay = ");
	Serial.println(config.repeatDelay);
	Serial.print("config.repeatTimes = ");
	Serial.println(config.repeatTimes);
	Serial.print("config.brightness = ");
	Serial.println(config.brightness);
	Serial.print("config.cycleAllImages = ");
	Serial.println(config.cycleAllImages);
	Serial.print("config.imageRightToLeft = ");
	Serial.println(config.imageRightToLeft);
	Serial.print("config.imageUpsideDown = ");
	Serial.println(config.imageUpsideDown);		
}

bool ReadConfig() {


	dataFile = SD.open(CONFIG_FILE);
	if (dataFile) {

		config.wandMode = ReadInt();
		config.sabreSpeed = ReadInt();
		config.sabreLength = ReadInt();
		config.sabreColour = ReadInt();
		config.sabreMode = ReadInt();
		config.fxSpeed = ReadInt();
		config.frameDelay = ReadInt();							// default for the frame delay 
		config.frameBlankDelay = ReadInt();						// Default Frame blank delay of 0
		config.initDelay = ReadInt();									// Variable for delay between button press and start of light sequence
		config.repeat = ReadInt();										// Variable to select auto repeat (until select button is pressed again)
		config.repeatDelay = ReadInt();								// Variable for delay between repeats
		config.repeatTimes = ReadInt();							// Variable to keep track of number of repeats
		config.brightness = ReadInt();								// Variable and default for the Brightness of the strip
		config.cycleAllImages = ReadBool();			//cycle through all images
		config.imageRightToLeft = ReadBool();
		config.imageUpsideDown = ReadBool();
		config.sabreFade = ReadBool();
	
		dataFile.close();

		Serial.print("config.wandMode = ");
		Serial.println(config.wandMode);
		Serial.print("config.sabreSpeed = ");
		Serial.println(config.sabreSpeed);
		Serial.print("config.sabreLength = ");
		Serial.println(config.sabreLength);
		Serial.print("config.sabreColour = ");
		Serial.println(config.sabreColour);
		Serial.print("config.sabreMode = ");
		Serial.println(config.sabreMode);
		Serial.print("config.sabreFade = ");
		Serial.println(config.sabreFade);
		Serial.print("config.fxSpeed = ");
		Serial.println(config.fxSpeed);
		Serial.print("config.frameDelay = ");
		Serial.println(config.frameDelay);
		Serial.print("config.frameBlankDelay = ");
		Serial.println(config.frameBlankDelay);
		Serial.print("config.initDelay = ");
		Serial.println(config.initDelay);
		Serial.print("config.repeat = ");
		Serial.println(config.repeat);
		Serial.print("config.repeatDelay = ");
		Serial.println(config.repeatDelay);
		Serial.print("config.repeatTimes = ");
		Serial.println(config.repeatTimes);
		Serial.print("config.brightness = ");
		Serial.println(config.brightness);
		Serial.print("config.cycleAllImages = ");
		Serial.println(config.cycleAllImages);
		Serial.print("config.imageRightToLeft = ");
		Serial.println(config.imageRightToLeft);
		Serial.print("config.imageUpsideDown = ");
		Serial.println(config.imageUpsideDown);		
	} else {
		Serial.println("Failed to open config.bin for reading");
	}
}

bool WriteConfig() {

	SD.remove(CONFIG_FILE);
	dataFile = SD.open(CONFIG_FILE, FILE_WRITE);
	if (dataFile) {

		WriteInt(config.wandMode);
		WriteInt(config.sabreSpeed);
		WriteInt(config.sabreLength);
		WriteInt(config.sabreColour);
		WriteInt(config.sabreMode);
		WriteInt(config.fxSpeed);
		WriteInt(config.frameDelay);							// default for the frame delay 
		WriteInt(config.frameBlankDelay);						// Default Frame blank delay of 0
		WriteInt(config.initDelay);									// Variable for delay between button press and start of light sequence
		WriteInt(config.repeat);										// Variable to select auto repeat (until select button is pressed again)
		WriteInt(config.repeatDelay);								// Variable for delay between repeats
		WriteInt(config.repeatTimes);							// Variable to keep track of number of repeats
		WriteInt(config.brightness);								// Variable and default for the Brightness of the strip
		writeBool(config.cycleAllImages);			//cycle through all images
		writeBool(config.imageRightToLeft);
		writeBool(config.imageUpsideDown);
		writeBool(config.sabreFade);

		dataFile.close();
		return true;
	}
	Serial.println("Failed to open config.bin for writing");
	return false;
}






/*
void InitFlashStorage() {

	config.frameDelay = frame_delay.read();
	if (config.frameDelay == 0) {
		config.frameDelay = 15;
	}
		
	config.initDelay = init_delay.read();
	repeat = repeat_num.read();
	config.repeatDelay = repeat_delay.read();
	config.repeatTimes = repeat_times.read();
	if (config.repeatTimes == 0) {
		config.repeatTimes = 1;
	}
	brightness = bright_val.read();  // <-- save the age
	if (brightness == 0) {
		brightness = 85;
	}
	
	cycleAllImages = cycle_all.read();
	imageRightToLeft = right_to_left.read();
	imageUpsideDown = upside_down.read();
	
	config.wandMode = wand_mode.read();
}

void UpdateFlashStorage() {
	frame_delay.write(config.frameDelay);
	init_delay.write(config.initDelay);
	repeat_num.write(repeat);
	repeat_delay.write(config.repeatDelay);
	repeat_times.write(config.repeatTimes);
	bright_val.write(brightness);
	cycle_all.write(cycleAllImages);
	right_to_left.write(imageRightToLeft);
	upside_down.write(imageUpsideDown);
	wand_mode.write(config.wandMode);

}
*/

void ClearLCD(void) {
	u8x8.clearDisplay();
	Serial.println("clear lcd");
}

void SetupLCDdisplay() {
	u8x8.begin();
	u8x8.setFont(u8x8_font_chroma48medium8_r);
	PrintToLCD(0, 0, F("-PIXELWAND V1.3-"));
	PrintToLCD(2, 1, F("Initializing"));
	delay(200);	
}

void InitLCD() {
	ClearLCD();
	PrintToLCD(0, 0, F("-PIXELWAND V1.3-"));
}

void SetupJoystick() {
	pinMode(JOYU_PIN, INPUT_PULLUP);
	pinMode(JOYD_PIN, INPUT_PULLUP);
	pinMode(JOYL_PIN, INPUT_PULLUP);
	pinMode(JOYR_PIN, INPUT_PULLUP);
	pinMode(JOYB_PIN, INPUT_PULLUP);
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

void PrintToLCDFullLineCentred(int row, int ival, bool perc = false) {
	String s = String(ival);
	if (perc) {
		s += "%";
	}
	PrintToLCDFullLineCentred(row, s);
}

void PrintToLCDFullLine(int col, int row, const char * message) {
	String s = String(message);
	PrintToLCDFullLine(col, row, s);
}

void PrintToLCDFullLineCentred(int row, const char * message) {
	String s = String(message);
	PrintToLCDFullLineCentred(row, s);
}

void PrintToLCD(int col, int row, String  message) {

	const char *mes = message.c_str();
	PrintToLCD(col, row, mes);
}

void PrintToLCD(int col, int row, const char * message) {
	Serial.println("writing to screen");
	u8x8.drawString(col, row, message);
	Serial.println(message);
}

void PrintToLCDFullLineCentred(int row, String  message) {
	int col = (16 - message.length()) / 2;
	PrintToLCDFullLine(col, row, message);
}

void PrintToLCDFullLine(int col, int row, String  message) {

	String blank = "                ";
	int charsAvailable = 16 - col;

	if (col > 0) {
		Serial.print("Printing ");
		Serial.print(message);
		Serial.print(" to column ");
		Serial.println(col);
	}

	for (int i = 0; i < message.length() && i < charsAvailable; i++) {
		blank.setCharAt(i + col, message.charAt(i));
	}

	if (col > 0) {
		Serial.print("Result is: '");
		Serial.print(blank);
		Serial.println("'");
	}
	const char *mes = blank.c_str();
	PrintToLCD(0, row, mes);
}

void PrintProgressToLCD(int cols, int row) {

	String blank = "----------------";
	if (cols == 0) {
		 blank = "                ";
	}
	for (int i = 0; i < cols; i++) {
		blank.setCharAt(i, ' ');
	}

	const char *mes = blank.c_str();
	PrintToLCD(0, row, mes);
}

void SetupLEDs() {
	LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
	LEDS.setBrightness(84);
}


bool SetupSDcard() {

	int attempts = 0;
	PrintToLCD(0, 2, F("SD init starting"));
	delay(100);
	PrintToLCD(0, 3, F("  Accessing SD  "));
 
	pinMode(SDSS_PIN, OUTPUT);

	delay(100);
	while (!SD.begin(SDSS_PIN)) {
		PrintToLCD(0, 4, F(" SD init failed "));
		attempts += 1;
		if (attempts > 10) {
			PrintToLCD(0, 4, F(" giving up "));
			Serial.println("gave up trying to initialise the SD card");
			return false;
		}
		delay(500);
	}
	PrintToLCD(0, 4, F("  SD init done  "));
	return true;
}

void readSDcard() {

	root = SD.open("/");
	PrintToLCD(0, 5, F(" Scanning files "));
	delay(300);
	GetFileNamesFromSD(root);
	PrintToLCD(0, 6, F("num files: "));
	PrintToLCD(11, 6, m_NumberOfFiles);
	SortFilenames(m_FileNames, m_NumberOfFiles);
	m_CurrentFilename = m_FileNames[0];
	delay(1000);
	PrintToLCDFullLine(0, 6, m_CurrentFilename);
	delay(1000);
	Serial.print("current filename = ");
	Serial.println(m_CurrentFilename);
}

// Setup loop to get everything ready.	This is only run once at power on or reset
void setup() {
	Serial.begin(SERIAL_BAUD);
	delay(500);
	SetupLCDdisplay();
	delay(100);
	SetupLEDs();
	SetupJoystick();
	InitConfig();
	if (SetupSDcard() == true) {
		readSDcard();
		ReadConfig();
	}
	InitLCD();
}

void loop() {


	if (config.wandMode == WAND_PIXELSTICK) {
		if (updateScreen) {
			updateScreen = false;
			switch (menuItem) {
				case 1:
					PrintToLCD(0, 2, F(" 1: Wand Mode   "));
					PrintToLCDFullLineCentred(4, F("Pixel Stick"));
					break;	
				case 2:
					PrintToLCD(0, 2, F(" 2: File Select "));
					PrintToLCDFullLineCentred(4, m_CurrentFilename);
					break;	
				case 3:
					PrintToLCD(0, 2, F(" 3: Brightness  "));
					PrintToLCDFullLineCentred(4, config.brightness, true);
					break;	
				case 4:
					PrintToLCD(0, 2, F(" 4: Init Delay  "));
					PrintToLCDFullLineCentred(4, config.initDelay);	 
					break;	
				case 5:
					PrintToLCD(0, 2, F(" 5: Frame Delay "));
					PrintToLCDFullLineCentred(4, config.frameDelay);	
					break;	
				case 6:		
					PrintToLCD(0, 2, F(" 6: Repeat Times"));
					PrintToLCDFullLineCentred(4, config.repeatTimes);	 
					break;	
				case 7:
					PrintToLCD(0, 2, F(" 7: Repeat Delay"));
					PrintToLCDFullLineCentred(4, config.repeatDelay);	
					break;	
				case 8:
					PrintToLCD(0, 2,F( " 8: Blank Time  "));
					PrintToLCDFullLineCentred(4, config.frameBlankDelay);	
					break;	 
				case 9:
					PrintToLCD(0, 2, F(" 9: Cycle All   "));
					if (config.cycleAllImages) {
						PrintToLCDFullLineCentred(4, F("Yes"));
					} else {
						PrintToLCDFullLineCentred(4, F("No ")); 
					}	
					break;
				case 10:
					PrintToLCD(0, 2, F("10: Orientation "));
					if (config.imageUpsideDown) {
						PrintToLCDFullLineCentred(4, F("Upside Down")); 
					} else {
						PrintToLCDFullLineCentred(4, F("Normal"));
					}	
					break;
			}
		}
	
		loopCounter +=1;
		
		if (interruptPressed >= 3) {					// User requested that display mode be stopped
			delay (250);										// Delay for a bit, since select button is used to abort and restart this is trying to prevent a restart of display
			dataFile.close();								// Close data file if it was left open, should have been closed already, trying to prevent open files
			interruptPressed = 0;						// On next loop we won't delay any longer.
		}
		
		if (loopCounter > LOOP_CYCLES) {	//	This is a delay that is counting loops, Keypad won't be read until 2000 cycles

			int keypress = ReadJoystick(false);		// set keypress to keypad reading
			if (keypress != KEY_NONE) {
	
				// only do these checks if a key pressed
				if (keypress == KEY_BUTTON) {		// The select key was pressed
					Serial.println("key button pressed");
					WriteConfig();

					loopCounter = 0;				// tell the loop counter to not read another keypress until the main loop, loops another 2000 times
					delay((config.initDelay * 1000) +100);
					cycleImageCount = 0;
					do {
						cycleAllImagesOneshot = config.cycleAllImages;
						if (config.cycleAllImages) {
		
							m_CurrentFilename = m_FileNames[cycleImageCount];
							if (m_CurrentFilename == "STARTUP.BMP") {
								cycleImageCount += 1;
								if (cycleImageCount	< m_NumberOfFiles) {
									m_CurrentFilename = m_FileNames[cycleImageCount];
								} else {
									break;
								}
							}
						}
						
						interruptPressed = 0;						// Might need removed Brian Heiland	 
						if (config.repeatTimes > 1) {						// If there are repeats selected
		
							for (int x = config.repeatTimes; x > 0; x--) {	// cycle through repeats
								if (interruptPressed >= 3) {			// Look for interrupt pressed count to be 3 or higher
									interruptPressed = 0;			// set interrupt pressed back to 0 
									cycleAllImagesOneshot = 0;
									break;							// user has pressed interrupt exit the for loop and stop displaying more images
								}
								Serial.println("Sending file");
								SendFile(m_CurrentFilename);		//display file 
		
								if (config.repeatDelay > 0) {				// if there is a repeat delay Make sure strip is clear
									ClearStrip(0);					// Before waiting with stuck pixels
								}
								delay(config.repeatDelay * 100);					// delay for the repeat delay before looping 
							}
						} else {
							interruptPressed = 0;					// User only is displaying image one time 
							SendFile(m_CurrentFilename);			// send the file to display
						}
							
						ClearStrip(0);								// after last image is displayed clear the strip
//						updateScreen = false;						// No need to update screen, lets keep light off, time exposure may still be happening on camera, so no lights from display.
						
						cycleImageCount += 1;
						if (config.repeatDelay >0){						// if there is a repeat delay Make sure strip is clear
							ClearStrip(0);							// Before waiting with stuck pixels
						}
						delay(config.repeatDelay);							// Use repeat delay before shoewing next image 
					} while((cycleAllImagesOneshot) && (cycleImageCount < m_NumberOfFiles));
				} else {
	
					// user has pressed something other than the go button, so start the timer
			
					if (keypress == KEY_LEFT || keypress == KEY_RIGHT) {					// The Left Key was Pressed
						loopCounter = 0;
						int dir = (keypress == KEY_RIGHT) ? 1 : -1;
						updateScreen = true;					// Screen will need updated
						switch (menuItem) {						// Select the Previous File
							case 1:
								config.wandMode = WAND_SABRE;
								break;
							case 2:
								m_FileIndex = NormaliseVal(m_FileIndex + dir, 0, m_NumberOfFiles -1, true);
								DisplayCurrentFilename();
								delay(200);		
								break;
							case 3:									// Adjust Brightness
								config.brightness = NormaliseVal(config.brightness + dir, 0, 100, false);
								break;
							case 4:									// Adjust Initial Delay - 1 second
								config.initDelay = NormaliseVal(config.initDelay + dir, 0, 30, false);
								break;
							case 5:									// Adjust Frame Delay - 1 millisecond 
								config.frameDelay = NormaliseVal(config.frameDelay + dir, 0, 100, false);
								break;
							case 6:									// Adjust Repeat Times - 1
								config.repeatTimes = NormaliseVal(config.repeatTimes + dir, 0, 20, false);
								break;
							case 7:									// Adjust Repeat Delay - 100 milliseconds
								config.repeatDelay = NormaliseVal(config.repeatDelay + dir, 0, 20, false);
								break;
							case 8:
								config.frameBlankDelay = NormaliseVal(config.frameBlankDelay + dir, 0, 20, false);
								break;
							case 9:
								config.cycleAllImages = !config.cycleAllImages;
								break;
							case 10:
								config.imageUpsideDown = !config.imageUpsideDown;
								break;
						}	
					}
			
			
					if (keypress == KEY_UP) {				 // The up key was pressed
						loopCounter = 0;
						updateScreen = true;					// Screen will need updated
						if (menuItem == 1) {
							menuItem = 10;	
						} else {
							menuItem -= 1;
						}
					}
					
					if (keypress == KEY_DOWN) {				 // The down key was pressed
						loopCounter = 0;
						updateScreen = true;					// Screen will need updated
						if (menuItem == 10) {
							menuItem = 1;	
						} else {
							menuItem += 1;
						}
					}
				}			
				delay(50);		// delay after a key pressed
			}
			delay(50);
		}
	} 
	
	if (config.wandMode == WAND_SABRE) {
		if (updateScreen) {
			Serial.println("updating screen");
			updateScreen = false;
			switch (menuItem) {
				case 1:
					PrintToLCD(0, 2, F(" 1: Wand Mode   "));
					PrintToLCDFullLineCentred(4, F("Light Sabre")); 
					break;	
				case 2:
					PrintToLCD(0, 2, F(" 2: Brightness  "));
					PrintToLCDFullLineCentred(4, config.brightness, true);
					break;	
				case 3:
					PrintToLCD(0, 2, F(" 3: Colour      "));
					switch (config.sabreColour) {
						case 0:
							PrintToLCDFullLineCentred(4, F("Red"));
							break;
						case 1:
							PrintToLCDFullLineCentred(4, F("Green"));
							break;
						case 2:
							PrintToLCDFullLineCentred(4, F("Blue"));
							break;
						case 3:
							PrintToLCDFullLineCentred(4, F("White"));
							break;
					}
					break;
				case 4:
					PrintToLCD(0, 2, F(" 4: Speed       "));
					PrintToLCDFullLineCentred(4, config.sabreSpeed);
					break;	
				case 5:
					PrintToLCD(0, 2, F(" 5: Appearance  "));
					switch (config.sabreMode) {
						case SABRE_SOLID:
							PrintToLCDFullLineCentred(4, F("Solid"));
							break;
						case SABRE_PULSE:
							PrintToLCDFullLineCentred(4, F("Pulse"));
							break;
						case SABRE_THROB:
							PrintToLCDFullLineCentred(4, F("Throb"));
							break;
					}
					break;	
				case 6:
					PrintToLCD(0, 2, F(" 6: FX Speed    "));
					PrintToLCDFullLineCentred(4, config.fxSpeed);
					break;	
				case 7:
					PrintToLCD(0, 2, F(" 7: Fade        "));
					if (config.sabreFade) {
						PrintToLCDFullLineCentred(4, F("On"));
					} else {
						PrintToLCDFullLineCentred(4, F("Off"));
					}
					break;	
			}
		}
	
		loopCounter +=1;
		
		if (interruptPressed >= 3) {					// User requested that display mode be stopped
			delay (250);										// Delay for a bit, since select button is used to abort and restart this is trying to prevent a restart of display
			interruptPressed = 0;						// On next loop we won't delay any longer.
		}
		
		if (loopCounter > LOOP_CYCLES) {	//	This is a delay that is counting loops, Keypad won't be read until 2000 cycles
	
			int keypress = ReadJoystick(false);		// set keypress to keypad reading
			if (keypress != KEY_NONE) {
	
				// only do these checks if a key pressed
				if (keypress == KEY_BUTTON) {		// The select key was pressed
					
					Serial.println("key button pressed");
					WriteConfig();
					loopCounter = 0;				// tell the loop counter to not read another keypress until the main loop, loops another 2000 times
					interruptPressed = 0;					// User only is displaying image one time 
					SendSabre();
						
					ClearStrip(0);								// after last image is displayed clear the strip
					
				} else {
	
					// user has pressed something other than the go button, so start the timer
					Serial.print("Button pressed: ");
			
					if (keypress == KEY_LEFT || keypress == KEY_RIGHT) {					// The Left Key was Pressed
						Serial.println("left or right");
						int dir = (keypress == KEY_RIGHT) ? 1 : -1;
						loopCounter = 0;
						updateScreen = true;
						switch (menuItem) {
							case 1:
								config.wandMode = WAND_PIXELSTICK;
								break;
							case 2:
								config.brightness = NormaliseVal(config.brightness + dir, 0, 100, false);
								break;
							case 3:
								config.sabreColour = NormaliseVal(config.sabreColour + dir, 0, 3, true);
								break;
							case 4:
								config.sabreSpeed = NormaliseVal(config.sabreSpeed + dir, 0, 30, false);
								break;
							case 5:
								config.sabreMode = NormaliseVal(config.sabreMode + dir, SABRE_SOLID, SABRE_THROB, true);
								break;
							case 6:
								config.fxSpeed = NormaliseVal(config.fxSpeed + dir, 0, 30, false);
								break;
							case 7:
								config.sabreFade = !config.sabreFade;
								break;
						}	
					}
			
			
					if (keypress == KEY_UP) {				 // The up key was pressed
						Serial.println("up");
						loopCounter = 0;
						updateScreen = true;					// Screen will need updated
						if (menuItem == 1) {
							menuItem = 7;	
						} else {
							menuItem -= 1;
						}
					}
					
					if (keypress == KEY_DOWN) {				 // The down key was pressed
						Serial.println("down");
						loopCounter = 0;
						updateScreen = true;					// Screen will need updated
						if (menuItem == 7) {
							menuItem = 1;	
						} else {
							menuItem += 1;
						}
					}
				}			

				Serial.println("delay after key press");
				
				delay(100);		// delay after a key pressed
			}
			delay(50);
		}
		
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

int ReadJoystick(bool buttonOnly) {

	key = GetJoystickPosition(buttonOnly);							// convert into key press
	if (key != oldkey) {											// if keypress is detected

		delay(50);													// wait for debounce time
		key = GetJoystickPosition(buttonOnly);						// convert into key press
		if (key != oldkey) {					
			oldkey = key;
		}
	}
	return key;
}
	
int GetJoystickPosition(bool buttonOnly) {

	if (digitalRead(JOYB_PIN) == LOW) {
		return KEY_BUTTON;
	}
	if (buttonOnly) {
		return KEY_NONE;
	}
	
	if (digitalRead(JOYU_PIN) == LOW) {
		return KEY_UP;
	}
	
	if (digitalRead(JOYD_PIN) == LOW) {
		return KEY_DOWN;
	}
	
	if (digitalRead(JOYL_PIN) == LOW) {
		return KEY_LEFT;
	}
	
	if (digitalRead(JOYR_PIN) == LOW) {
		return KEY_RIGHT;
	}
	return KEY_NONE;
}


void SendFile(String Filename) {

	char temp[14];
	Filename.toCharArray(temp,14);
	 
	dataFile = SD.open(temp);
	 
	// if the file is available send it to the LED's
	if (dataFile) {
		ReadTheFile();
		dataFile.close();
		if (interruptPressed >= 3) {
			delay (500); // add a delay to prevent select button from starting sequence again.
		}
	}	
	else {
		PrintToLCD(0, 6, F("File read error "));
		delay(1000);
		PrintToLCD(0, 6, F("                "));
		SetupSDcard();
	}
}

void DisplayCurrentFilename() {
	m_CurrentFilename = m_FileNames[m_FileIndex];
	PrintToLCDFullLineCentred(4, m_CurrentFilename);
}

void GetFileNamesFromSD(File dir) {
	int fileCount = 0;
	String CurrentFilename = "";
	while(1) {
		File entry = dir.openNextFile();
		if (!entry) {
			// no more files
			m_NumberOfFiles = fileCount;
			entry.close();
			break;
		} else {
			if (!entry.isDirectory()) {
				CurrentFilename = entry.name();
				if (CurrentFilename.endsWith(".bmp") || CurrentFilename.endsWith(".BMP") ) { //find files with our extension only
					if (!CurrentFilename.startsWith("_")) {
						m_FileNames[fileCount] = entry.name();
						PrintToLCDFullLine(0, 6, entry.name());

						Serial.print("File number ");
						Serial.print(fileCount);
						Serial.print(" set to ");
						Serial.println(m_FileNames[fileCount]);
						fileCount++;
					}
				}
			}
		}
		entry.close();
	}
}
	 
void LatchAndDelay(int dur) {
	FastLED.show();
	delay(dur);
}

void ClearStrip(int duration) {

	for (int x = 0; x < NUM_LEDS; x++) {
		leds[x] = 0;
	}
	FastLED.show();
}


void GetRGBwithGamma() {
	
	b = GammaVal(ReadByte())*(config.brightness *0.01);			// Brian Heiland Revise.	Old formula
	r = GammaVal(ReadByte())*(config.brightness *0.01);			// reduced brightness 50% when brightness was changed from 
	g = GammaVal(ReadByte())*(config.brightness *0.01);			// 100 to 99 , by 50% brightness was nearly 0 This formula corrects this.

}

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

int SetSabreLine(int displayWidth, int currentLength) {

	int sr = sabreColours[config.sabreColour][0];
	int sg = sabreColours[config.sabreColour][1];
	int sb = sabreColours[config.sabreColour][2];

	for (int i = 0; i < currentLength; i++) {

		int brt = config.brightness;
		if (config.sabreFade) {
			brt = (currentLength * brt / displayWidth );
		}
		if (config.sabreMode == SABRE_PULSE) {
			brt = GetBrightness(displayWidth, brt, i + startPoint, 1);
		} else if (config.sabreMode == SABRE_THROB) {
			brt = GetBrightness(displayWidth, brt, i, throbVal);
		}
		
		r = GammaVal(sr)*(brt *0.01);			// reduced brightness 50% when brightness was changed from 
		g = GammaVal(sg)*(brt *0.01);			// 100 to 99 , by 50% brightness was nearly 0 This formula corrects this.
		b = GammaVal(sb)*(brt *0.01);			// Brian Heiland Revise.	Old formula
		leds[i].setRGB(g,r,b);
	}
	for (int i = currentLength; i < displayWidth; i++) {
		leds[i].setRGB(0,0,0);
	}

	if (config.sabreMode == SABRE_PULSE) {
		// I think this should allow us any value below 36 to work OK... // to be continued.. ?
		startPoint -= config.fxSpeed;
		if (startPoint <= 0) {
			startPoint += 36;
		}
	} else if (config.sabreMode == SABRE_THROB) {
		if (throbDir) {
			throbVal -= 0.1;
			if (throbVal <= -1) {
				throbDir = false;
			}
		} else {
			throbVal += 0.1;
			if (throbVal >= 1) {
				throbDir = true;
			}
		}
	}
	if (config.sabreMode == SABRE_SOLID) {
		return 100;
	}
	return 1;
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
	Serial.println("Starting sabre");
	bool startingUp = true;
	bool runningSabre = false;
	bool shuttingDown = false;
	int numLit = 0;
	maxBrightness = 100;
	ClearLCD();

	if (config.sabreSpeed == 0) {
		startingUp = false;
		runningSabre = true;
		numLit = displayWidth;
	}
	
	while (runningSabre || startingUp || shuttingDown) {

		if (runningSabre) {
			int keypress = ReadJoystick(true);				//Read the keypad, each successive read will be column+1 until 3 is reached.
	
			if (keypress == KEY_BUTTON && interruptPressed <= 3) {	// The select key was pressed or the Aux button was pressed
				delay(50);
				interruptPressed += 1;										// user is trying to interrupt display, increment count of frames with button held down.		
				Serial.print("interrupt = ");
				Serial.println(interruptPressed);
			}
			
			if (interruptPressed >= 4) {										// 3 or more frames have passed with button pressed
				runningSabre = false;
				if (config.sabreSpeed == 0) {
					shuttingDown = false;
				} else {
					shuttingDown = true;
				}
				interruptPressed = 0;
			}
		}

		if (startingUp) {
			numLit += config.sabreSpeed;
			if (numLit >= displayWidth) {
				// if we went over the end, just pop back to the end
				numLit = displayWidth;
				startingUp = false;
				runningSabre = true;
				logBrightness = true;
			}
		} else if (shuttingDown) {
			numLit -= config.sabreSpeed;
			if (numLit <= 0) {
				numLit = 0;
				shuttingDown = false;
			}
		}

		int lad = SetSabreLine(displayWidth, numLit);
		if (runningSabre == false)
			lad = 0;
		LatchAndDelay(lad);
	}
	Serial.println("Finished running");
	if (interruptPressed >= 4) {
		delay (500); // add a delay to prevent select button from starting sequence again.
	}	
	interruptPressed = 0;
	updateScreen = true;
	InitLCD();
}


void ReadTheFile() {
	#define MYBMP_BF_TYPE			0x4D42
	#define MYBMP_BF_OFF_BITS		54
	#define MYBMP_BI_SIZE			40
	#define MYBMP_BI_RGB			0L
	#define MYBMP_BI_RLE8			1L
	#define MYBMP_BI_RLE4			2L
	#define MYBMP_BI_BITFIELDS		3L

	uint16_t bmpType = ReadInt();
	uint32_t bmpSize = ReadLong();
	uint16_t bmpReserved1 = ReadInt();
	uint16_t bmpReserved2 = ReadInt();
	uint32_t bmpOffBits = ReadLong();
	bmpOffBits = 54;
	ClearLCD();
	 
	/* Check file header */
	if (bmpType != MYBMP_BF_TYPE || bmpOffBits != MYBMP_BF_OFF_BITS) {
//		ClearLCD();
		PrintToLCD(0, 6, F("  not a bitmap  "));
		delay(1000);
		return;
	}
	
	/* Read info header */
	uint32_t imgSize = ReadLong();
	uint32_t imgWidth = ReadLong();
	uint32_t imgHeight = ReadLong();
	uint16_t imgPlanes = ReadInt();
	uint16_t imgBitCount = ReadInt();
	uint32_t imgCompression = ReadLong();
	uint32_t imgSizeImage = ReadLong();
	uint32_t imgXPelsPerMeter = ReadLong();
	uint32_t imgYPelsPerMeter = ReadLong();
	uint32_t imgClrUsed = ReadLong();
	uint32_t imgClrImportant = ReadLong();
	 
	/* Check info header */
	if (imgSize != MYBMP_BI_SIZE || imgWidth <= 0 || imgHeight <= 0 || imgPlanes != 1 || imgBitCount != 24 || imgCompression != MYBMP_BI_RGB || imgSizeImage == 0 ) {
//		ClearLCD();
		PrintToLCDFullLine(0, 6, F("Unsupported bmp "));
		PrintToLCDFullLine(0, 7, F("(use 24bpp)"));
		delay(1000);
		PrintToLCD(0, 7, F("                "));
		return;
	}
	
	int displayWidth = imgWidth;
	if (imgWidth > NUM_LEDS) {
		displayWidth = NUM_LEDS;			 //only display the number of led's we have
	}
	 
	/* compute the line length */
	uint32_t lineLength = imgWidth * 3;
	if ((lineLength % 4) != 0) {
		lineLength = (lineLength / 4 + 1) * 4;
	}

	// Note:	
	// The x,r,b,g sequence below might need to be changed if your strip is displaying
	// incorrect colors.	Some strips use an x,r,b,g sequence and some use x,r,g,b
	// Change the order if needed to make the colors correct.

	int pc = 0;
	int lastpc = 0;
	
	for (int y = imgHeight; y > 0; y--) {
		
		int bufpos=0; 
		if ((interruptPressed <= 3) && (y <= (imgHeight - 5))) {	 // if the interrupt has not been pressed and we are working on column 5 of the image.	(look for no key presses until into 5th column of image)
			int keypress = ReadJoystick(true);				//Read the keypad, each successive read will be column+1 until 3 is reached.
			if (keypress != KEY_NONE) {
				delay(150);
			}
			if (keypress == KEY_BUTTON) {	// The select key was pressed or the Aux button was pressed
				Serial.println("Interrupt");
				interruptPressed += 1;										// user is trying to interrupt display, increment count of frames with button held down.		
			}
			if (interruptPressed >= 3) {										// 3 or more frames have passed with button pressed
				Serial.println("3 interrupts, clearing strip");
				cycleAllImagesOneshot = 0;
				ClearStrip(0);												 // clear the strip
				break;														 // break out of the y for loop.
			}
		}	 
		
		for (int x=0; x < displayWidth; x++) {								// Loop through the x	values of the image
			uint32_t offset = (MYBMP_BF_OFF_BITS + (((y-1)* lineLength) + (x*3))) ;	
			dataFile.seek(offset);
			GetRGBwithGamma();
			if (config.imageUpsideDown) {
				leds[displayWidth - x].setRGB(r,g,b);
			} else {
				leds[x].setRGB(r,g,b);
			}
		}
		
		LatchAndDelay(config.frameDelay);

		pc = (imgHeight - y + 1) * 16 / imgHeight;
		if (pc != lastpc) {
			lastpc = pc;
			PrintProgressToLCD(pc, 7);
		}

		
		if (config.frameBlankDelay > 0) {
			ClearStrip(0);
			delay(config.frameBlankDelay);
		}
		
		if (interruptPressed >= 3) {										// this code is unreachable clean up and remove when verified not needed Brian Heiland
			dataFile.close();
			break;
		}
	}
	PrintProgressToLCD(0, 7);
	updateScreen = true;
	InitLCD();
}

// Sort the filenames in alphabetical order
void SortFilenames(String *filenames, int n) {
	for (int i = 1; i < n; ++i) {
		String j = filenames[i];
		int k;
		for (k = i - 1; (k >= 0) && (j < filenames[k]); k--) {
			filenames[k + 1] = filenames[k];
		}
		filenames[k + 1] = j;
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
