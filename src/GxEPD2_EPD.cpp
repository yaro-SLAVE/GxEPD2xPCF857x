// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_EPD.h"

#if defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

GxEPD2_EPD::GxEPD2_EPD(int16_t cs, int16_t dc, int16_t rst, int16_t busy, int16_t busy_level, uint32_t busy_timeout,
                       uint16_t w, uint16_t h, GxEPD2::Panel p, bool c, bool pu, bool fpu) :
  WIDTH(w), HEIGHT(h), panel(p), hasColor(c), hasPartialUpdate(pu), hasFastPartialUpdate(fpu),
  _cs(cs), _dc(dc), _rst(rst), _busy(busy), _busy_level(busy_level), _busy_timeout(busy_timeout), _diag_enabled(false),
  _pSPIx(&SPI), _spi_settings(4000000, MSBFIRST, SPI_MODE0)
{
  _initial_write = true;
  _initial_refresh = true;
  _power_is_on = false;
  _using_partial_mode = false;
  _hibernating = false;
  _init_display_done = false;
  _reset_duration = 10;
  _busy_callback = 0;
  _busy_callback_parameter = 0;
}

void GxEPD2_EPD::init(uint32_t serial_diag_bitrate)
{
  init(serial_diag_bitrate, true, 10, false);
}

void GxEPD2_EPD::init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
{
  _initial_write = initial;
  _initial_refresh = initial;
  _pulldown_rst_mode = pulldown_rst_mode;
  _power_is_on = false;
  _using_partial_mode = false;
  _hibernating = false;
  _init_display_done = false;
  _reset_duration = reset_duration;

  Serial.println("1.1");

  if (serial_diag_bitrate > 0)
  {
    Serial.begin(serial_diag_bitrate);
    _diag_enabled = true;
  }

  Serial.println("1.2");
  if (_cs >= 0)
  {
    pcf.digitalWrite(_cs, HIGH); // preset (less glitch for any analyzer)
    pcf.pinMode(_cs, OUTPUT);
    pcf.digitalWrite(_cs, HIGH); // set (needed e.g. for RP2040)
  }
  Serial.println("1.3");
  _reset();
  _pSPIx->begin(); // may steal _rst pin (Waveshare Pico-ePaper-2.9)
  if (_rst >= 0)
  {
    pcf.digitalWrite(_rst, HIGH); // preset (less glitch for any analyzer)
    pcf.pinMode(_rst, OUTPUT);
    pcf.digitalWrite(_rst, HIGH); // set (needed e.g. for RP2040)
  }
  Serial.println("1.4");
  if (_cs >= 0)
  {
    pcf.digitalWrite(_cs, HIGH); // preset (less glitch for any analyzer)
    pcf.pinMode(_cs, OUTPUT);
    pcf.digitalWrite(_cs, HIGH); // set (needed e.g. for RP2040)
  }
  if (_dc >= 0)
  {
    pcf.digitalWrite(_dc, HIGH); // preset (less glitch for any analyzer)
    pcf.pinMode(_dc, OUTPUT);
    pcf.digitalWrite(_dc, HIGH); // set (needed e.g. for RP2040)
  }
  if (_busy >= 0)
  {
    pinMode(_busy, INPUT);
  }
}

void GxEPD2_EPD::end()
{
  _pSPIx->end();
  if (_cs >= 0) pcf.pinMode(_cs, INPUT);
  if (_dc >= 0) pcf.pinMode(_dc, INPUT);
  if (_rst >= 0) pcf.pinMode(_rst, INPUT);
}

void GxEPD2_EPD::setBusyCallback(void (*busyCallback)(const void*), const void* busy_callback_parameter)
{
  _busy_callback = busyCallback;
  _busy_callback_parameter = busy_callback_parameter;
}

void GxEPD2_EPD::selectSPI(SPIClass& spi, SPISettings spi_settings)
{
  _pSPIx = &spi;
  _spi_settings = spi_settings;
}

void GxEPD2_EPD::_reset()
{
  if (_rst >= 0)
  {
    if (_pulldown_rst_mode)
    {
      pcf.digitalWrite(_rst, LOW);
      pcf.pinMode(_rst, OUTPUT);
      pcf.digitalWrite(_rst, LOW);
      delay(_reset_duration);
      pcf.pinMode(_rst, INPUT_PULLUP);
      delay(_reset_duration > 10 ? _reset_duration : 10);
    }
    else
    {
      pcf.digitalWrite(_rst, HIGH); // NEEDED for Waveshare "clever" reset circuit, power controller before reset pulse, preset (less glitch for any analyzer)
      pcf.pinMode(_rst, OUTPUT);
      pcf.digitalWrite(_rst, HIGH); // NEEDED for Waveshare "clever" reset circuit, power controller before reset pulse, set (needed e.g. for RP2040)
      delay(10); // NEEDED for Waveshare "clever" reset circuit, at least delay(2);
      pcf.digitalWrite(_rst, LOW);
      delay(_reset_duration);
      pcf.digitalWrite(_rst, HIGH);
      delay(_reset_duration > 10 ? _reset_duration : 10);
    }
    _hibernating = false;
  }
}

void GxEPD2_EPD::_waitWhileBusy(const char* comment, uint16_t busy_time)
{
  if (_busy >= 0)
  {
    Serial.print("_waitWhileBusy: waiting");
    if (comment) {
      Serial.print(" for ");
      Serial.print(comment);
    }
    Serial.println();
    
    delay(10);
    
    unsigned long start = millis();
    unsigned long last_print = millis();
    unsigned long last_state_change = millis();
    bool last_busy_state = (digitalRead(_busy) == _busy_level);
    
    while (true)
    {
      bool current_busy_state = (digitalRead(_busy) == _busy_level);
      
      // Если состояние изменилось
      if (current_busy_state != last_busy_state) {
        last_busy_state = current_busy_state;
        last_state_change = millis();
        Serial.printf("Busy state changed to: %s\n", current_busy_state ? "BUSY" : "READY");
      }
      
      // Если не busy - выходим
      if (!current_busy_state) {
        Serial.println("_waitWhileBusy: device ready");
        break;
      }
      
      // Периодический вывод статуса
      if (millis() - last_print > 2000) {
        unsigned long elapsed = (millis() - start) / 1000;
        Serial.printf("Still busy... %lu seconds (pin state: %d)\n", elapsed, digitalRead(_busy));
        last_print = millis();
      }
      
      // Проверяем timeout (в секундах)
      if (millis() - start > (busy_time * 1000UL)) {
        Serial.println("_waitWhileBusy: TIMEOUT - forcing exit!");
        break;
      }
      
      // Проверяем зависание (если состояние не меняется слишком долго)
      if (millis() - last_state_change > 30000) { // 30 секунд без изменений
        Serial.println("_waitWhileBusy: STALL DETECTED - forcing exit!");
        break;
      }
      
      delay(100);
    }
    
    unsigned long total_time = millis() - start;
    Serial.printf("_waitWhileBusy: completed in %.1f seconds\n", total_time / 1000.0);
  }
  else 
  {
    Serial.printf("_waitWhileBusy: no BUSY pin, delaying %d ms\n", busy_time);
    delay(busy_time);
  }
}

void GxEPD2_EPD::_writeCommand(uint8_t c)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_dc >= 0) pcf.digitalWrite(_dc, LOW);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  _pSPIx->transfer(c);
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  if (_dc >= 0) pcf.digitalWrite(_dc, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeData(uint8_t d)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  _pSPIx->transfer(d);
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeData(const uint8_t* data, uint16_t n)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  for (uint16_t i = 0; i < n; i++)
  {
    _pSPIx->transfer(*data++);
  }
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeDataPGM(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  for (uint16_t i = 0; i < n; i++)
  {
    _pSPIx->transfer(pgm_read_byte(&*data++));
  }
  while (fill_with_zeroes > 0)
  {
    _pSPIx->transfer(0x00);
    fill_with_zeroes--;
  }
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeDataPGM_sCS(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes)
{
  _pSPIx->beginTransaction(_spi_settings);
  for (uint8_t i = 0; i < n; i++)
  {
    if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
    _pSPIx->transfer(pgm_read_byte(&*data++));
    if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  }
  while (fill_with_zeroes > 0)
  {
    if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
    _pSPIx->transfer(0x00);
    fill_with_zeroes--;
    if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  }
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeCommandData(const uint8_t* pCommandData, uint8_t datalen)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_dc >= 0) pcf.digitalWrite(_dc, LOW);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  _pSPIx->transfer(*pCommandData++);
  if (_dc >= 0) pcf.digitalWrite(_dc, HIGH);
  for (uint8_t i = 0; i < datalen - 1; i++)  // sub the command
  {
    _pSPIx->transfer(*pCommandData++);
  }
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_writeCommandDataPGM(const uint8_t* pCommandData, uint8_t datalen)
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_dc >= 0) pcf.digitalWrite(_dc, LOW);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
  _pSPIx->transfer(pgm_read_byte(&*pCommandData++));
  if (_dc >= 0) pcf.digitalWrite(_dc, HIGH);
  for (uint8_t i = 0; i < datalen - 1; i++)  // sub the command
  {
    _pSPIx->transfer(pgm_read_byte(&*pCommandData++));
  }
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}

void GxEPD2_EPD::_startTransfer()
{
  _pSPIx->beginTransaction(_spi_settings);
  if (_cs >= 0) pcf.digitalWrite(_cs, LOW);
}

void GxEPD2_EPD::_transfer(uint8_t value)
{
  _pSPIx->transfer(value);
}

void GxEPD2_EPD::_endTransfer()
{
  if (_cs >= 0) pcf.digitalWrite(_cs, HIGH);
  _pSPIx->endTransaction();
}
