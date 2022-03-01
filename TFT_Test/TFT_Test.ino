/* Map XPT2046 input to ILI9341 320x240 raster */
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <XPT2046_Touchscreen.h> /* https://github.com/PaulStoffregen/XPT2046_Touchscreen */

#define TFT_DC             4
#define TFT_CS              5
#define TFT_RST            22


#define _sclk              18
#define _mosi              23
#define _miso              19

#define TFT_BACKLIGHT_PIN  15 /* -via transistor- */
#define TOUCH_CS_PIN       14
#define TOUCH_IRQ_PIN      27

#define TFT_NORMAL          1
#define TFT_UPSIDEDOWN      3

const uint8_t TFT_ORIENTATION = TFT_NORMAL;

Adafruit_ILI9341 tft = Adafruit_ILI9341( TFT_CS, TFT_DC, TFT_RST );

XPT2046_Touchscreen touch( TOUCH_CS_PIN, TOUCH_IRQ_PIN );

void setup() {
  Serial.begin( 115200 );
  SPI.begin( _sclk, _miso, _mosi );
  SPI.setFrequency( 60000000 );

  tft.begin( 10000000, SPI );

  tft.setRotation( TFT_ORIENTATION );
  tft.fillScreen( ILI9341_BLACK );
  tft.setTextColor( ILI9341_GREEN, ILI9341_BLACK );

  pinMode( TFT_BACKLIGHT_PIN, OUTPUT );
  digitalWrite( TFT_BACKLIGHT_PIN, HIGH );

  touch.begin();
  Serial.println( "Touch screen ready." );
}


TS_Point rawLocation;

void loop() {
    while ( touch.touched() )
    {
      rawLocation = touch.getPoint();
      /*
        tft.setCursor( 100, 150 );
        tft.print( "X = " );
        tft.print( rawLocation.x );
        tft.setCursor(100, 180);
        tft.print( "Y = " );
        tft.print( rawLocation.y );

        Serial.print("x = ");
        Serial.print(rawLocation.x);
        Serial.print(", y = ");
        Serial.print(rawLocation.y);
        Serial.print(", z = ");
        Serial.println(rawLocation.z);
      */
      if ( TFT_ORIENTATION == TFT_UPSIDEDOWN )
      {
        tft.drawPixel( map( rawLocation.x, 340, 3900, 0, 320 ),
                       map( rawLocation.y, 200, 3850, 0, 240 ),
                       ILI9341_GREEN );
      }
      if ( TFT_ORIENTATION == TFT_NORMAL )
      {
        tft.drawPixel( map( rawLocation.x, 340, 3900, 320, 0 ),
                       map( rawLocation.y, 200, 3850, 240, 0 ),
                       ILI9341_GREEN );
      }
    }
  //tft.fillScreen(ILI9341_BLACK);
}
