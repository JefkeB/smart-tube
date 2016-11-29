#include <ESP8266WiFi.h>
#include <Wire.h>

#include <JsonListener.h>
#include <MovingAverageFilter.h>
#include <OLEDDisplayUi.h>
#include <SSD1306Wire.h>
#include <Ticker.h>
#include <TimeClient.h>
#include <WundergroundClient.h>

#include "fonts.h"
#include "images.h"


// WIFI
const char* WIFI_SSID = "Henry's Living Room 2.4GHz";
const char* WIFI_PWD = "13913954971";


// Setup
const int UPDATE_INTERVAL_SECS = 60 * 60; // Update every 60 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D2;
const int SDC_PIN = D1;

// TimeClient Settings
const float UTC_OFFSET = 8;

// Wunderground Settings
const boolean IS_METRIC = true;
const String WUNDERGRROUND_API_KEY = "2d4a4e7587426081";
const String WUNDERGRROUND_LANGUAGE = "EN";
const String WUNDERGROUND_COUNTRY = "CN";
const String WUNDERGROUND_CITY = "Nanjing";

// Initialize the oled display for address 0x3c
SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi   ui( &display );

/***************************
   End Settings
 **************************/

TimeClient timeClient(UTC_OFFSET);

WundergroundClient wunderground(IS_METRIC);

// This flag changed in the ticker function every 60 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";
long lastTemperatureUpdate = 0;

Ticker ticker;

// Indoor
int temperature = INT32_MAX;
// Use MovingAverage to caculate the mean value of the temperature in the last 3 minutes.
const int TEMPERATURE_MA_POINT_COUNT = 3 * 60 / 10;
MovingAverageFilter temperatureMA(TEMPERATURE_MA_POINT_COUNT);

// Declaring prototypes
void drawProgress(OLEDDisplay *display, int percentage, String label);
void updateTemperature();
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawIndoor };
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Initialize dispaly
  display.init();
  display.clear();
  display.flipScreenVertically();
  display.display();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  WiFi.begin(WIFI_SSID, WIFI_PWD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.flipScreenVertically();
    display.display();

    counter++;
  }

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  ui.setIndicatorPosition(BOTTOM);

  ui.setIndicatorDirection(LEFT_RIGHT);

  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  ui.setTimePerTransition(200);
  ui.setTimePerFrame(8000);

  ui.init();

  Serial.println("");

  updateData(&display);

  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);

}

void loop() {

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0)
  {
    delay(remainingTimeBudget);
  }

  if (lastTemperatureUpdate == 0 || millis() - lastTemperatureUpdate > 10 * 1000)
  {
    // Update temperature every 10 seconds.
    updateTemperature();
  }
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->flipScreenVertically();
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  timeClient.updateTime();
  drawProgress(display, 30, "Updating conditions...");
  wunderground.updateConditions(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 50, "Updating forecasts...");
  wunderground.updateForecast(WUNDERGRROUND_API_KEY, WUNDERGRROUND_LANGUAGE, WUNDERGROUND_COUNTRY, WUNDERGROUND_CITY);
  drawProgress(display, 80, "Updating temperature...");
  updateTemperature();
  lastUpdate = timeClient.getFormattedTime();
  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(100);
}

void updateTemperature()
{
  int value = analogRead(A0);
  float temp = (float)value / 1023 * 3.3 * 100;
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print(" / ");
  if (temperature == INT32_MAX)
  {
    for (int i = 0; i < TEMPERATURE_MA_POINT_COUNT; i++)
    {
      temperature = round(temperatureMA.process(temp));
    }
  }
  else
  {
    temperature = round(temperatureMA.process(temp));
  }
  lastTemperatureUpdate = millis();
  Serial.println(temperature);
}


void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = wunderground.getDate();
  int textWidth = display->getStringWidth(date);
  display->drawString(64 + x, 5 + y, date);
  display->setFont(ArialMT_Plain_24);
  String time = timeClient.getFormattedTime();
  textWidth = display->getStringWidth(time);
  display->drawString(64 + x, 15 + y, time);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(60 + x, 5 + y, wunderground.getWeatherText());

  display->setFont(ArialMT_Plain_24);
  String temp = wunderground.getCurrentTemp() + "°C";
  display->drawString(60 + x, 15 + y, temp);
  int tempWidth = display->getStringWidth(temp);

  display->setFont(Meteocons_Plain_42);
  String weatherIcon = wunderground.getTodayIcon();
  int weatherIconWidth = display->getStringWidth(weatherIcon);
  display->drawString(32 + x - weatherIconWidth / 2, 05 + y, weatherIcon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 2);
  drawForecastDetails(display, x + 88, y, 4);
}

void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 5 + y, "Indoor");
  display->setFont(ArialMT_Plain_24);
  if (temperature != INT32_MAX)
  {
    display->drawString(64 + x, 15 + y, String(temperature) + " °C");
  }
  else
  {
    display->drawString(64 + x, 15 + y, "-- °C");
  }
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String day = wunderground.getForecastTitle(dayIndex).substring(0, 3);
  day.toUpperCase();
  display->drawString(x + 20, y, day);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, wunderground.getForecastIcon(dayIndex));

  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, wunderground.getForecastLowTemp(dayIndex) + "|" + wunderground.getForecastHighTemp(dayIndex));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  String time = timeClient.getFormattedTime().substring(0, 5);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, time);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  if (temperature != INT32_MAX)
  {
    display->drawString(128, 54, String(temperature) + " °C");
  }
  else
  {
    display->drawString(128, 54, "-- °C");
  }
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
