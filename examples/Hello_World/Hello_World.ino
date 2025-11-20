#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Adafruit_PCF8574.h>
#include <Wire.h>

#define CS_PIN (3)
#define BUSY_PIN (12)
#define RES_PIN (1)
#define DC_PIN (2)

Adafruit_PCF8574 pcf;

// Функция для полного отключения WDT
void disableWDT() {
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Отключаем WDT
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // ПОЛНОЕ отключение WDT на системном уровне
  disableWDT();
  
  Serial.println("start - WDT completely disabled");

  // Wire.begin();
  // // delay(100);

  //  SPI.begin();


  if (!pcf.begin(0x20, &Wire)) {
    Serial.println("Couldn't find PCF8574");
    while (1);
  }
  delay(200);
  Serial.println("PCF8574 initialized");

  GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(CS_PIN, DC_PIN, RES_PIN, BUSY_PIN)); 

  Serial.println("1");
  display.init();
  Serial.println("2");

  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(1);

  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(10, 30);
  display.print("Hello World!");

  Serial.println("3");
  
  display.display();

    Serial.println("4");

}

void loop() {
  // Ничего не делаем
  delay(1000);
}