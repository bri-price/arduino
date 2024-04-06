
// OLED uses standard pins for SCL and SDA
//U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);	 // OLEDs without Reset of the Display

// Run a web server on port 80


bool updateScreen = true;
int delayVal = 0;

bool connectedToWifi = false;


bool whiteOnly = true;
