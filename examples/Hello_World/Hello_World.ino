
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Adafruit_PCF8574.h>

#define CS_PIN (3)
#define BUSY_PIN (0)
#define RES_PIN (1)
#define DC_PIN (2)

Adafruit_PCF8574 pcf;

void setup() {

  Serial.begin(115200);

  if (!pcf.begin(0x20, &Wire)) {
    Serial.println("Couldn't find PCF8574");
    while (1);
  }

  GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(/*CS=5*/ CS_PIN, /*DC=*/ DC_PIN, /*RES=*/ RES_PIN, /*BUSY=*/ BUSY_PIN, /*PCF8574=*/ &pcf)); 

  display.init();

  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(1);

  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(10, 10);
  display.print("Hello World!");

  display.display();
}

void loop() {
  
}
