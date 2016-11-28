#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <MovingAverageFilter.h>
#include <TimeLib.h>
#include <NtpClientLib.h>


#define WIFI_SSID "Henry's Living Room 2.4GHz"
#define WIFI_PASSWD "13913954971"

bool initialized = false;
bool ntpSynchronized = false;
int temperature = INT32_MAX;
MovingAverageFilter temperatureMA(6);
int humidity = INT32_MAX;
MovingAverageFilter humidityMA(6);
bool blink = false;

Adafruit_SSD1306 screen;
DHT dht11(D7, DHT11);

void updateTemperature()
{
    int temp = dht11.readTemperature();
    Serial.print("Temperature: ");
    Serial.print(temp);
    Serial.print(" / ");
    if (temperature == INT32_MAX)
    {
        for (int i = 0; i < 6; i++)
        {
            temperature = temperatureMA.process(temp);
        }
    }
    else
    {
        if (temp < 50)
        {
            temperature = temperatureMA.process(temp);
        }
    }
    Serial.println(temperature);
}

void updateHumidity()
{
    int hum = dht11.readHumidity();
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.print(" / ");
    if (humidity == INT32_MAX)
    {
        for (int i = 0; i < 6; i++)
        {
            humidity = humidityMA.process(hum);
        }
    }
    else
    {
        humidity = humidityMA.process(hum);
    }
    Serial.println(humidity);
}

void setupNTP()
{
    NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
        if (error)
        {
            Serial.print("Time Sync error: ");
            if (error == noResponse)
            {
                Serial.println("NTP server not reachable");
            }
            else if (error == invalidAddress)
            {
                Serial.println("Invalid NTP server address");
            }
        }
        else
        {
            ntpSynchronized = true;
        }
    });
    NTP.begin("time.apple.com", 8, false);
    NTP.setInterval(30 * 60);
}

void onSTAGotIP(WiFiEventStationModeGotIP ipInfo)
{
	Serial.printf("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
	setupNTP();
}
void setupWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    WiFi.onStationModeGotIP(onSTAGotIP);
}

void setupScreen()
{
    screen.begin(SSD1306_SWITCHCAPVCC, 0x3c);
    screen.setTextColor(WHITE);
    screen.clearDisplay();
    screen.display();
}

void setup()
{
    Serial.begin(9600);

    setupWiFi();
    setupNTP();
    setupScreen();

    dht11.begin();
}

void update()
{
    if (!initialized  || millis() / 1000 % 20 == 0)
    {
        Serial.println("Updating temperature and humidity...");
        updateTemperature();
        updateHumidity();
    }
    initialized = true;
}

void render()
{
    const int PADDING_LEFT = 5;
    screen.clearDisplay();

    // Time
    screen.setCursor(PADDING_LEFT, 0);
    screen.setTextSize(4);
    String timeString =  NTP.getTimeStr();
    screen.print(timeString.substring(0, 2));
    blink = !blink;
    if (blink)
    {
        screen.print(":");
    }
    else
    {
        screen.print(" ");
    }
    screen.print(timeString.substring(3, 5));

    // Temperature
    screen.setCursor(PADDING_LEFT, 40);
    screen.setTextSize(1);
    screen.println("TEMP");
    screen.setCursor(PADDING_LEFT, 50);
    screen.setTextSize(2);
    screen.println(temperature);

    // Humidity
    screen.setCursor(PADDING_LEFT + 38, 40);
    screen.setTextSize(1);
    screen.println("HUM");
    screen.setCursor(PADDING_LEFT + 36, 50);
    screen.setTextSize(2);
    screen.println(humidity);

    screen.display();
}

void loop()
{
    update();
    render();
    delay(500);
}
