#include <M5EPD.h>
#include "WiFiInfo.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#define LGFX_M5PAPER
#include <LovyanGFX.hpp>
#include "THPIcons.c"
#include "WeatherIcons.c"
#include "time.h"
#include <Timezone.h>
#include <TimeLib.h>

// Change to your timezone
#define timezone -4
#define FONT_SIZE_LARGE 1.8
#define FONT_SIZE_MEDIUM 1.0
#define FONT_SIZE_SMALL 0.7
#define WIDTH   540
#define HEIGHT  960

// Icon Sprites

LGFX gfx;
//LGFX_Sprite THPIcons(&gfx);
//LGFX_Sprite WeatherIcons(&gfx);
LGFX_Sprite SRSSIcons(&gfx);

String currentTime;

//time_t t;
//time_t local;
//struct tm *tm;
//static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240}; //Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};  //Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);

float indoorTemp, outdoorTemp, indoorHumidity, outdoorHumidity;
time_t localSunriseTime, localSunsetTime;

void setup(void)
{
  constexpr uint_fast16_t WIFI_CONNECT_RETRY_MAX = 10;
  M5.begin(false, true, true, true, true);
  delay(100);
  M5.RTC.begin();
  delay(10);
  WiFi.begin(WiFiInfo::SSID, WiFiInfo::PASS);

  gfx.init();
  gfx.setEpdMode(epd_mode_t::epd_fast);
  // 540 x 960
  gfx.setRotation(0);
  gfx.setFont(&fonts::lgfxJapanMinchoP_40);
  gfx.setTextSize(FONT_SIZE_SMALL);

  gfx.startWrite();
  gfx.fillScreen(TFT_WHITE);
  for (int cnt_retry = 0; cnt_retry < WIFI_CONNECT_RETRY_MAX && !WiFi.isConnected(); cnt_retry++)
    delay(500);

  if (!WiFi.isConnected())
  {

    gfx.println("Failed to connect to a Wi-Fi network");
    gfx.endWrite();
    gfx.waitDisplay();
    M5.shutdown(10 * 60); // Sleep 10 minutes
  }
  gfx.endWrite();
  gfx.waitDisplay();
  /*
  setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 1);
  tzset();
  configTime(0, 0, "0.ca.pool.ntp.org", "1.ca.pool.ntp.org", "2.ca.pool.ntp.org");
  delay(250);

  // Make sure we have the time fetched before proceeding
  // https://github.com/esp8266/Arduino/issues/4749
  while(time(nullptr) <= 100000)
    delay(100);
*/
}

time_t bigstringToTime(String myString)
{
  tmElements_t myTm;
  //2021-09-17T10:12:33.033177+00:00
  //012345678901234567890234567890

  myTm.Year = (uint8_t) (myString.substring(0, 4).toInt() - 1970);
  myTm.Month = (uint8_t) myString.substring(5, 7).toInt();
  myTm.Day = (uint8_t) myString.substring(8, 10).toInt();

  myTm.Hour = (uint8_t) myString.substring(11, 13).toInt();
  myTm.Minute = (uint8_t) myString.substring(14, 16).toInt();
  myTm.Second = (uint8_t) myString.substring(17, 19).toInt();

  return makeTime(myTm);
}

void fetchHA()
{
  HTTPClient haRequest;

  haRequest.begin("http://192.168.1.33:8123/api/states/sensor.outdoor_temperature");
  haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  int httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
      outdoorTemp = (float)(root["state"]);
    else
      outdoorTemp = -99.9;
  }
  haRequest.end();

  haRequest.begin("http://192.168.1.33:8123/api/states/sensor.home_temperature");
  haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
      indoorTemp = (float)(root["state"]);
    else
      indoorTemp = -99.9;
  }
  haRequest.end();

  haRequest.begin("http://192.168.1.33:8123/api/states/sensor.home_humidity");
    haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
      indoorHumidity = (float)(root["state"]);
    else
      indoorHumidity = -99.9;
  }
  haRequest.end();

  haRequest.begin("http://192.168.1.33:8123/api/states/sensor.outdoor_humidity");
  haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
      outdoorHumidity = (float)(root["state"]);
    else
      outdoorHumidity = -99.9;
  }
  haRequest.end();

  haRequest.begin("http://192.168.1.33:8123/api/states/sun.sun");
  haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
    {
      String UTCSunrise((const char *)root["attributes"]["next_rising"]);
      String UTCSunset((const char *)root["attributes"]["next_setting"]);

      localSunriseTime = myTZ.toLocal(bigstringToTime(UTCSunrise));
      localSunsetTime = myTZ.toLocal(bigstringToTime(UTCSunset));

      tmElements_t tm;
      breakTime(localSunriseTime, tm);

    }
  }
  haRequest.end();

  // Current time
  haRequest.begin("http://192.168.1.33:8123/api/states/sensor.time");
  haRequest.addHeader("Authorization", "Bearer " + TOKEN);
  httpCode = haRequest.GET();

  if (httpCode > 0)
  {
    String payload = haRequest.getString();
    //Serial.println(payload);
    DynamicJsonBuffer jsonBuffer(512);
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success())
      currentTime = String((const char *)root["state"]);
    else
      currentTime = String("unknown");
  }
  haRequest.end();
}

void showDateTime(void)
{
  //char message[32];
  
  gfx.setTextSize(FONT_SIZE_SMALL);
  gfx.setTextDatum(textdatum_t::top_center);
  //tmElements_t localTimeElements;
  //time_t localTime = myTZ.toLocal(time(nullptr));

  //breakTime(localTime, localTimeElements);

  //sprintf(message,"%2d:%02d\r\n", localTimeElements.Hour, localTimeElements.Minute);
  gfx.drawString(currentTime, WIDTH / 2, 0);
  gfx.setTextDatum(textdatum_t::top_left);
}

void showSunriseSunset(uint_fast16_t offset_x, uint_fast16_t offset_y)
{
  // Text Size
  gfx.setTextSize(FONT_SIZE_MEDIUM);

  // Sunrise
  SRSSIcons.createSprite(64, 64);
  SRSSIcons.setSwapBytes(true);
  SRSSIcons.fillSprite(WHITE);
  SRSSIcons.pushImage(0, 0, 64, 64, (uint16_t *)SUNRISE64x64);
  SRSSIcons.pushSprite(offset_x, offset_y);

  gfx.setCursor(offset_x + 75, offset_y + 15);
  gfx.printf("%2d:%02d\r\n", hour(localSunriseTime), minute(localSunriseTime));

  // Sunset
  SRSSIcons.createSprite(64, 64);
  SRSSIcons.setSwapBytes(true);
  SRSSIcons.fillSprite(WHITE);
  SRSSIcons.pushImage(0, 0, 64, 64, (uint16_t *)SUNSET64x64);
  SRSSIcons.pushSprite(offset_x + 230, offset_y + 5);

  gfx.setCursor(offset_x + 305, offset_y + 15);
  gfx.printf("%2d:%02d\r\n", hour(localSunsetTime), minute(localSunsetTime));
}

void showBatteryLevel(uint_fast16_t x, uint_fast16_t y)
{
  // Battery Information

  gfx.setTextSize(FONT_SIZE_SMALL);
  //gfx.setCursor(x + 465, y);
  gfx.setCursor(x, y);
  //gfx.printf("BAT: ");
  uint32_t vol = M5.getBatteryVoltage();
  if (vol < 3300)
  {
    vol = 3300;
  }
  else if (vol > 4350)
  {
    vol = 4350;
  }
  float battery = (float)(vol - 3300) / (float)(4350 - 3300);
  if (battery <= 0.01)
  {
    battery = 0.01;
  }
  if (battery > 1)
  {
    battery = 1;
  }
  uint8_t percentage = battery * 100;
  gfx.printf("%d%%\r\n", percentage);
}

void loop(void)
{
  char message[64];

  gfx.startWrite();
  gfx.fillScreen(TFT_WHITE);
  //gfx.fillRect(0.57 * M5PAPER_WIDTH, 0, 3, M5PAPER_HEIGHT, TFT_BLACK); // 960*0.57 = 547.2

  fetchHA();

  showDateTime();
  gfx.setTextSize(1.3);
  gfx.setTextDatum(textdatum_t::middle_center);
  
  // Labels
  gfx.drawString("Outdoor", WIDTH / 2, 105);
  gfx.drawString("Indoor", WIDTH / 2, 500);

  // Temps
  gfx.setTextSize(FONT_SIZE_LARGE * 2.2);

  sprintf(message, "%2.1f°", outdoorTemp);
  gfx.drawString(message, WIDTH / 2, 230);
  sprintf(message, "%2.1f°", indoorTemp);
  gfx.drawString(message, WIDTH / 2, 620);

// Humidity
  gfx.setTextSize(FONT_SIZE_MEDIUM * 2);

  sprintf(message, "%2.0f%%", outdoorHumidity);
  gfx.drawString(message, WIDTH / 2, 350);
  sprintf(message, "%2.0f%%", indoorHumidity);
  gfx.drawString(message, WIDTH / 2, 740);
    


  gfx.setTextDatum(textdatum_t::top_left);

  showSunriseSunset(55, 885);
  showBatteryLevel(0, 0);

  // Centering aids for layout
  //gfx.drawLine(0, HEIGHT / 2, WIDTH, HEIGHT / 2, BLACK);
  //gfx.drawLine(WIDTH / 2, 0, WIDTH / 2, HEIGHT, BLACK);
  
  gfx.clearClipRect();
  gfx.endWrite();
  gfx.waitDisplay();
  //delay(500);

  int minutesToSleep = 30;
  // If we are charging, this will be ignored
  // Shutdown is time in seconds
  M5.shutdown(minutesToSleep * 60);

  // If we are on battery, the call above succeeded, and this will not be reached
  // Just delay 1 minute, then update again
  delay(5 * 60 * 1000);
}
