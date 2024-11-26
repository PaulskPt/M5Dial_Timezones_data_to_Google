/*
*  M5Dial_Timezones_M5Echo_RFID_data_to_Google.ino
*  Test sketch for M5Stack M5Dial with 240 x 240 pixels
*  This sketch:
*  - displays in sequence seven different timezones;
*  - uses SNTP automatic polling.
*  - sends sync time and other data to a Google Sheets spreadsheet (via a Google Apps Script)
*  by @PaulskPt (Github) 2024-10-05. Last update; 2024-11-22.
*  License: MIT
*  ToDo: solve a leakage of total 5.8 kBytes over 24 entries in the spreadsheet (24 x 5 minutes).
*        The averas leakage/loss over 6 x 24 entries: 246 bytes.
*/
#include <Arduino.h>
#include <M5Dial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
//#include <freertos/queue.h>
#include <freertos/semphr.h>
//#include <M5Unified.h>
#include <M5GFX.h>
//#include <BlockNot.h> // Use instead of blocking delay(nnn)
#include <esp_sntp.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TimeLib.h>

#include <stdlib.h>   // for putenv
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
//#include <time.h>
#include <DateTime.h> // See: /Arduino/libraries/ESPDateTime/src
#include "secret.h"
// Following 8 includes needed for creating, changing and using map time_zones
#include <map>
#include <memory>
#include <array>
#include <string>
#include <tuple>
// #include <iomanip>  // For setFill and setW
#include <sstream>  // Used in intToHex() (line 728)
//#include <ctime>    // Used in time_sync_notification_cb()

//namespace {  // anonymous namespace (also known as an unnamed namespace)

// SNTP Polling interval set to: 900000 mSec = 15 minutes in milliseconds (15 seconds is the minimum),
// See: https://github.com/espressif/esp-idf/blob/master/components/lwip/apps/sntp/sntp.c
// 300000 dec = 0x493E0 = 20 bits  ( 5 minutes)
// 900000 dec = 0xDBBA0 = 20 bits  (15 minutes)

volatile bool sntpBusy = false;
volatile bool handle_requestBusy = false;
SemaphoreHandle_t mutex;

const char* serverName = SECRET_GOOGLE_APPS_SCRIPT_URL;
char sServerShortName[30];

#define MIN_FREE_HEAP 200000 // minimum free heap threshold

#define NTP_SERVER1   SECRET_NTP_SERVER_1 // for example: "0.pool.ntp.org"

#ifdef CONFIG_LWIP_SNTP_UPDATE_DELAY   // Found in: Component config > LWIP > SNTP
#undef CONFIG_LWIP_SNTP_UPDATE_DELAY
#endif

// 15U * 60U * 1000U = 15 minutes in milliseconds
#define CONFIG_LWIP_SNTP_UPDATE_DELAY (5 * 60 * 1000)  // 5 minutes

// 4-PIN connector type HY2.0-4P
#define PORT_B_GROVE_OUT_PIN 2
#define PORT_B_GROVE_IN_PIN  1
// Set the HOLD pin to a high level (1) during program initialization to maintain the power supply.
// Otherwise the device will enter the sleep state again. (docs.m5stack.com/en/core/M5Dial)
#define PWR_HOLD_OUT_PIN 46  


#define HOURS 24
 // Array to store time sync times for the past 24 hours time_t
time_t SyncTimes[HOURS][3];
#define arrIDX      0
#define arrSYNCTIME 1
#define arrDIFF_T   2

int SyncTimesIndex = 0; // just for test. It should be 0

// bool my_debug = false;
bool lStart = true;
bool display_on = true;
bool sync_notification_triggered_at_start = false;
bool sync_time = false;
bool buttonPressed = false;
bool i_am_asleep = false;
bool syncIdxRollover = false;
time_t last_sync_epoch = 0; // see: time_sync_notification_cb()
time_t time_sync_epoch_at_start = 0;
bool lastTouchState = false;
unsigned long lastDebounceTime = 0;
unsigned int freeHeap = 0;

typedef struct http_request_data_t {
  char datetimeStr[26];
  char syncTimeStr[11];
  char diffTimeStr[6];
  char indexStr[3];
  char displayStr[2];
  char freeheapStr[7];
} http_request_data;

static constexpr const char gtxt2[] PROGMEM = "xPortGetFreeHeapSize(): ";

static char http_request_data_pool[1][sizeof(http_request_data)]; // create a static pool with enough size

// M5Dial screen 1.28 Inch 240x240px. Display device: GC9A01
// M5Dial touch driver: FT3267

char elem_zone[30];
char my_tz_code[30];
bool TimeToChangeZone = false;

static constexpr const char tz[] PROGMEM = "TZ";  // see: setTimezone() and initTime()
int zone_idx; // Will be incremented in loop()
static constexpr const int nr_of_zones = SECRET_NTP_NR_OF_ZONES[0] - '0';  // Assuming SECRET_NTP_NR_OF_ZONES is defined as a string

std::map<int, std::tuple<std::string, std::string>> zones_map;

//} // end of namespace

void create_maps() {
  
  for (int i = 0; i < nr_of_zones; ++i) {
    // Building variable names dynamically isn't directly possible, so you might want to define arrays instead
    switch (i) {
      case 0:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE0, SECRET_NTP_TIMEZONE0_CODE);
        break;
      case 1:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE1, SECRET_NTP_TIMEZONE1_CODE);
        break;
      case 2:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE2, SECRET_NTP_TIMEZONE2_CODE);
        break;
      case 3:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE3, SECRET_NTP_TIMEZONE3_CODE);
        break;
      case 4:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE4, SECRET_NTP_TIMEZONE4_CODE);
        break;
      case 5:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE5, SECRET_NTP_TIMEZONE5_CODE);
        break;
      case 6:
        zones_map[i] = std::make_tuple(SECRET_NTP_TIMEZONE6, SECRET_NTP_TIMEZONE6_CODE);
        break;
      default:
        break;
    }             
  }
}

// This function is called only once from setup()
// to set the global variable sServerShortName
void createServerShortName(void) {
  const char* sSearch = "/macros";
  const char *sResult = strstr(serverName, sSearch);
  if (sResult) { 
    int index = sResult - serverName;
    // Extract the substring from the start of haystack to index-1
    char sSubStr[index + 1]; 
    // Create an array to hold the substring 
    strncpy(sSubStr, serverName, index); 
    // Copy the characters 
    sSubStr[index] = '\0';
    strcpy(sServerShortName,sSubStr);
  }
}

// Called from handle_request()
void clr_sync_times(void) 
{
  // Clear all
  for (int i=0; i < HOURS; i++)
  {
    SyncTimes[i][arrIDX] = 0;
    SyncTimes[i][arrSYNCTIME] = 0;
    SyncTimes[i][arrDIFF_T] = 0;
  }
  Serial.println(F("SyncTimes array cleared: "));
}

// Show or remove NTP Time Sync notification on the middle of the top of the display
void ntp_sync_notification_txt(bool show) {
  int dw = M5Dial.Display.width();
  if (show) {
    // Only send command to make sound by the M5 Atom Echo when the display is on,
    //   because when the user has put the display off, he/she probably wants to go to bed/sleep.
    //   In that case we don't want nightly sounds!
    if (display_on) {
      send_cmd_to_AtomEcho(); // Send a digital signal to the Atom Echo to produce a beep
    }
    M5Dial.Display.setCursor(dw/2-25, 20);
    M5Dial.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5Dial.Display.print("TS");
    delay(500);
    M5Dial.Display.setCursor(dw/2-25, 20);      // Try to overwrite in black instead of wiping the whole top area
    M5Dial.Display.setTextColor(TFT_BLACK, TFT_BLACK);
    M5Dial.Display.print("TS");
    M5Dial.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  } else 
    M5Dial.Display.fillRect(0, 0, dw-1, 55, TFT_BLACK);
}

// Code suggested by MS CoPilot
// The sntp callback function
// @param tv Time received from SNTP server.
void time_sync_notification_cb(struct timeval *tv) {
  // I saw in the Monitor output that it happened that the sketch was inside handle_request(),
  // awaiting response to the http request, that this function was called.
  // to prevent this I added the following 2 lines.
  if (handle_requestBusy)
    return;
  static constexpr const char txt0[] PROGMEM = "time_sync_notification_cb(): ";
  static constexpr const char txt1[] PROGMEM = "flag sntpBusy "; 
  if (!lStart) {
    xSemaphoreTake(mutex, portMAX_DELAY); // Take the mutex
    sntpBusy = true; // Set the flag
  }
  // Perform SNTP-related tasks 
  // Get the current time  (very important!)
  time_t currentTime = time(nullptr);
  // Convert time_t to GMT struct tm
  struct tm* gmtTime = gmtime(&currentTime);
  uint16_t diff_t;

  // Set the starting epoch time if not set, only when lStart is true
  if (lStart && (time_sync_epoch_at_start == 0) && (currentTime > 0)) {
    time_sync_epoch_at_start = currentTime;  // Set only once!
  }
  // Set the last sync epoch time if not set
  if ((last_sync_epoch == 0) && (currentTime > 0)) {
    last_sync_epoch = currentTime;
  }
  if (currentTime > 0) {
    diff_t = currentTime - last_sync_epoch;
    last_sync_epoch = currentTime;
    #define CONFIG_LWIP_SNTP_UPDATE_DELAY_IN_SECONDS   CONFIG_LWIP_SNTP_UPDATE_DELAY / 1000

    // 300 or more = 5 or more minutes --- 900 or more seconds = 15 or more minutes
    
    // The two next if () blocks prevent that there will be double data lines in the spreadsheet or index jumps.
    if (!lStart && (diff_t < CONFIG_LWIP_SNTP_UPDATE_DELAY_IN_SECONDS)) { 
      sync_time = false;
      sntpBusy = false; // Reset the flag
      xSemaphoreGive(mutex);
      return;
    }
    if ((diff_t >= CONFIG_LWIP_SNTP_UPDATE_DELAY_IN_SECONDS) || lStart) {  
      sync_time = true; // See loop initTime
    }
    if (sync_time == true)
    {
      // If index is 0 check if the sync time at index 23 is not zero

      if (SyncTimesIndex == 0) {
        time_t syncTime23 = SyncTimes[HOURS-1][arrSYNCTIME];  // sync time of record with index 23
        if (syncTime23 == 0)
          SyncTimes[SyncTimesIndex][arrDIFF_T] = 0;  // set arrDIFF_T
        else if (syncTime23 > 0)
          SyncTimes[SyncTimesIndex][arrDIFF_T] = currentTime - syncTime23;
      }
      else if ((SyncTimesIndex > 0) && (SyncTimesIndex < HOURS)) {
        // Set arrDIFF_T with subtract of the sync time from the previous record from currentTime
        SyncTimes[SyncTimesIndex][arrDIFF_T] = currentTime - SyncTimes[SyncTimesIndex-1][arrSYNCTIME];
      }
      // Set arrIDX
      SyncTimes[SyncTimesIndex][arrIDX] = SyncTimesIndex;
      // Set arrSYNCTIME
      SyncTimes[SyncTimesIndex][arrSYNCTIME] = currentTime;

      SyncTimesIndex = SyncTimesIndex + 1;  // increase the index for the next (starts at 0)
      if (SyncTimesIndex == HOURS) {  // range 0-23
        syncIdxRollover = true;  // index will be reset in function prep_vals()
      }
      ntp_sync_notification_txt(true);
    }
  }
  if (!lStart) {
    sntpBusy = false; // Reset the flag
    xSemaphoreGive(mutex); // Give the mutex
  }
}
// End of code suggested by MS CoPilot

void esp_sntp_initialize() {
  Serial.println(F("initializing SNTP"));
   if (esp_sntp_enabled()) { 
    esp_sntp_stop();  // prevent initialization error
  }
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED); // or: (SNTP_SYNC_MODE_SMOOTH);  // Added by @PaulskPt
  esp_sntp_setservername(0, NTP_SERVER1);
  esp_sntp_set_sync_interval(CONFIG_LWIP_SNTP_UPDATE_DELAY);
  esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb); // Set the notification callback function
  esp_sntp_init();
  // check the set sync_interval
  uint32_t rcvd_sync_interval_secs = esp_sntp_get_sync_interval();
}

void setTimezone(void) {
  char elem_zone_code[50];
  strcpy(elem_zone_code, std::get<1>(zones_map[zone_idx]).c_str());
  setenv(tz, elem_zone_code, 1); // tz is a global var
  tzset();
}

bool initTime(void) {
  bool ret = false;
  strcpy(elem_zone, std::get<0>(zones_map[zone_idx]).c_str());
  strcpy(my_tz_code, getenv(tz)); // tz and my_tz_code are global vars
  static constexpr const char server2[] PROGMEM = "1.pool.ntp.org";
  static constexpr const char server3[] PROGMEM = "2.pool.ntp.org";
  configTzTime(my_tz_code, NTP_SERVER1, server2, server3);
  uint8_t try_cnt = 0;
  struct tm my_timeinfo;
  while (!getLocalTime(&my_timeinfo, 1000)) {
      try_cnt++;
      if (try_cnt > 10) {
        Serial.println(F("initTime(): unable to get local time. Exiting function."));
        return ret;
      }
      delay(1000);
  }

  if (my_timeinfo.tm_sec  != 0 || my_timeinfo.tm_min  != 0 || my_timeinfo.tm_hour  != 0 || 
      my_timeinfo.tm_mday != 0 || my_timeinfo.tm_mon  != 0 || my_timeinfo.tm_year  != 0 || 
      my_timeinfo.tm_wday != 0 || my_timeinfo.tm_yday != 0 || my_timeinfo.tm_isdst != 0) {
      setTimezone();
      ret = true;
  }
  return ret;
}

bool set_RTC(void) {
  bool ret = false;
  struct tm my_timeinfo;
  if(!getLocalTime(&my_timeinfo)) return ret;
  if (my_timeinfo.tm_year + 1900 > 1900) {
    //                            YYYY  MM  DD      hh  mm  ss
    //M5Dial.Rtc.setDateTime( { { 2021, 12, 31 }, { 12, 34, 56 } } );
    M5Dial.Rtc.setDateTime( {{my_timeinfo.tm_year + 1900, my_timeinfo.tm_mon + 1, 
        my_timeinfo.tm_mday}, {my_timeinfo.tm_hour, my_timeinfo.tm_min, my_timeinfo.tm_sec}} );
    ret = true;
  }
  return ret;
}

// This function uses local var my_timeinfo to display date and time data.
// The function also displays my_timezone info.
// It also calls functions ck_touch() and ck_Btn() four times 
// to increase a "catch" a display touch or a BtnA keypress.
void disp_data(void) {
  struct tm my_timeinfo;
  if (!getLocalTime(&my_timeinfo)) return;
  strncpy(elem_zone, std::get<0>(zones_map[zone_idx]).c_str(), sizeof(elem_zone) - 1);  // elem_zone is a global var
  elem_zone[sizeof(elem_zone) - 1] = '\0'; // Ensure null termination

  char part1[20], part2[20], part3[20], part4[20];
  char copiedString[50], copiedString2[50];

  memset(part1, 0, sizeof(part1));
  memset(part2, 0, sizeof(part2));
  memset(part3, 0, sizeof(part3));
  memset(part4, 0, sizeof(part4));
  memset(copiedString, 0, sizeof(copiedString));
  memset(copiedString2, 0, sizeof(copiedString2));
  
  char *index1 = strchr(elem_zone, '/');  // index to the 1st occurrance of a forward slash (e.g.: Europe/Lisbon)
  char *index2 = nullptr; 
  char *index3 = strchr(elem_zone, '_'); // index to the occurrance of an underscore character (e.g.: Sao_Paulo)
  int disp_data_view_delay = 1000;
  static constexpr const char txt1[] PROGMEM = "disp_data(): BtnA pressed";
  strncpy(copiedString, elem_zone, sizeof(copiedString) - 1);
  copiedString[sizeof(copiedString) - 1] = '\0'; // Ensure null termination
  // Check if index1 is valid and within bounds
  if (index1 != nullptr) {
    size_t idx1_pos = index1 - elem_zone;
    if (idx1_pos < sizeof(copiedString)) {
      strncpy(part1, copiedString, idx1_pos);
      part1[idx1_pos] = '\0';
    }
    strncpy(copiedString2, index1 + 1, sizeof(copiedString2) - 1);
    copiedString2[sizeof(copiedString2) - 1] = '\0'; // Ensure null termination
    if (index3 != nullptr) {
      // Replace underscores with spaces in copiedString2
      for (int i = 0; i < sizeof(copiedString2); i++) {
        if (copiedString2[i] == '_') {
          copiedString2[i] = ' ';
        }
      }
    }
    
    index2 = strchr(copiedString2, '/'); 
    if (index2 != nullptr) {
      size_t idx2_pos = index2 - copiedString2;
      if (idx2_pos < sizeof(copiedString2)) {
          strncpy(part3, copiedString2, idx2_pos);  // part3, e.g.: "Kentucky"
          part3[idx2_pos] = '\0';
      }
      strncpy(part4, index2 + 1, sizeof(part4) - 1);  // part4, e.g.: "Louisville"
      part4[sizeof(part4) - 1] = '\0'; // Ensure null termination
    } else {
      strncpy(part2, copiedString2, sizeof(part2) - 1);
      part2[sizeof(part2) - 1] = '\0'; // Ensure null termination
    }
  }
  M5Dial.update();
  if (ck_touch() > 0) return;
  ck_BtnA();
  if (buttonPressed) return;

  // --------------- 1st view ---------------
  M5Dial.Display.clear(); // M5Dial.Display.clear(TFT_BLACK); ;
  M5Dial.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  
  if (index1 != nullptr && index2 != nullptr) {
      M5Dial.Display.setCursor(50, 65);
      M5Dial.Display.print(part1);
      M5Dial.Display.setCursor(50, 118);
      M5Dial.Display.print(part3);
      M5Dial.Display.setCursor(50, 170);
      M5Dial.Display.print(part4);
  } else if (index1 != nullptr) {
      M5Dial.Display.setCursor(50, 65);
      M5Dial.Display.print(part1);
      M5Dial.Display.setCursor(50, 120);
      M5Dial.Display.print(part2);
  } else {
      M5Dial.Display.setCursor(50, 65);
      M5Dial.Display.print(copiedString);
  }
  delay(disp_data_view_delay);

  M5Dial.update();
  ck_BtnA();
  if (ck_touch() > 0) return;
  if (buttonPressed) return;
  
  // --------------- 2nd view ---------------
  M5Dial.Display.clear();
  M5Dial.Display.setCursor(50, 65);
  M5Dial.Display.print("Zone");
  M5Dial.Display.setCursor(50, 120);
  M5Dial.Display.print(&my_timeinfo, "%Z %z");
  delay(disp_data_view_delay);

  M5Dial.update();
  ck_BtnA();
  if (ck_touch() > 0) return;
  if (buttonPressed) return;

  // --------------- 3rd view ---------------
  M5Dial.Display.clear();
  M5Dial.Display.setCursor(50, 65);
  M5Dial.Display.print(&my_timeinfo, "%A");
  M5Dial.Display.setCursor(50, 118);
  M5Dial.Display.print(&my_timeinfo, "%B %d");
  M5Dial.Display.setCursor(50, 170);
  M5Dial.Display.print(&my_timeinfo, "%Y");
  delay(disp_data_view_delay);

  M5Dial.update();
  ck_BtnA();
  if (ck_touch() > 0) return;
  if (buttonPressed) return;

  // --------------- 4th view ---------------
  M5Dial.Display.clear();
  M5Dial.Display.setCursor(40, 65);
  M5Dial.Display.print(&my_timeinfo, "%H:%M:%S local");
  M5Dial.Display.setCursor(50, 120);

  if (index2 != nullptr) {
      M5Dial.Display.printf("in %s\n", part4);
  } else if (index1 != nullptr) {
      M5Dial.Display.printf("in %s\n", part2);
  }
  delay(disp_data_view_delay);
}

void disp_msg(String str, bool clr_at_end_func = false) {
  M5Dial.Display.clear();
  M5Dial.Display.setBrightness(200);  // Make more brightness than normal
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(TFT_NAVY); // (BLUE);
  M5Dial.Display.drawString(str, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
  delay(6000);
  if (clr_at_end_func)
  {
    M5Dial.Display.clear();
    M5Dial.Display.setBrightness(50); // Restore brightness to normal
  }
}

bool connect_WiFi(void) {
  #define WIFI_SSID     SECRET_SSID
  #define WIFI_PASSWORD SECRET_PASS
  static constexpr const char txt0[] PROGMEM = "WiFi ";
  bool ret = false;
  WiFi.begin( WIFI_SSID, WIFI_PASSWORD );

  for (int i = 20; i && WiFi.status() != WL_CONNECTED; --i)
    delay(500);
  
  if (WiFi.status() == WL_CONNECTED) {
    ret = true;
    static constexpr const char txt1[] PROGMEM = "Connected";
    Serial.printf("\r\n%s%s\n", txt0, txt1);
  }
  else {
    static constexpr const char txt2[] PROGMEM = "connection failed.";
    Serial.printf("%s%s\n", txt0, txt2);
  }
  return ret;
}

// ToDo: to be changed
void force_sntp_sync(void )
{
  ;
}

// Example function to clear the static pool
void clear_http_request_data_pool() {
    memset(http_request_data_pool, 0, sizeof(http_request_data_pool));
}

void formatTime(char* buffer, int value) {
  size_t bufferSize = sizeof(buffer);
  if (value < 10) {
      snprintf(buffer, bufferSize, "0%d", value);
  } else {
      snprintf(buffer, bufferSize, "%d", value);
  }
}

// Code suggested by MS Copilot (to kill memory leak)
char* prep_vals(void) {
  static char isoTime[26];
  time_t currentTime;

  // The index has been increased in time_sync_notification_cb() 
  // after filling the data to the SyncTimes array.
  int idx = SyncTimesIndex - 1;

  if (SyncTimes[idx][arrSYNCTIME] > 0)  // sync time
    currentTime = SyncTimes[idx][arrSYNCTIME];
  else
    currentTime = time(nullptr); // "seconds since 00:00:00 GMT, Jan 1, 1970"

  struct tm* gmtTime = gmtime(&currentTime);

  // Format time components
  char yy[5], mo[3], dd[3], hh[3], mi[3], ss[3];
  snprintf(yy, sizeof(yy), "%d", gmtTime->tm_year + 1900);
  formatTime(mo, gmtTime->tm_mon + 1);
  formatTime(dd, gmtTime->tm_mday);
  formatTime(hh, gmtTime->tm_hour);
  formatTime(mi, gmtTime->tm_min);
  formatTime(ss, gmtTime->tm_sec);

  // Combined format
  snprintf(isoTime, sizeof(isoTime), "%s-%s-%sT%s:%s:%sZ", yy, mo, dd, hh, mi, ss);
  return isoTime;
}

bool handle_request(void) {
  static constexpr const char txt0[] PROGMEM = "handle_request(): ";
  static constexpr const char txt1[] PROGMEM = "flag handle_requestBusy ";

  if (!lStart) {
    xSemaphoreTake(mutex, portMAX_DELAY); // Take the mutex
    handle_requestBusy = true; // Set the flag
  }
  
  const char* apikey = SECRET_GOOGLE_API_KEY;
  bool ret = false; // return value
  // The index has been increased in time_sync_notification_cb() 
  // after adding the data to the SyncTimes array.
  int idx = SyncTimesIndex - 1;

  // Example usage
  http_request_data requestData;
  strcpy(requestData.datetimeStr, prep_vals());
  snprintf(requestData.syncTimeStr, sizeof(requestData.syncTimeStr), "%ld", SyncTimes[idx][arrSYNCTIME]);
  snprintf(requestData.diffTimeStr, sizeof(requestData.diffTimeStr), "%ld", SyncTimes[idx][arrDIFF_T]);
  snprintf(requestData.indexStr,    sizeof(requestData.indexStr),    "%d",  SyncTimes[idx][arrIDX]);
  snprintf(requestData.displayStr,  sizeof(requestData.displayStr),  "%d",  (display_on == true) ? 1 : 0);
  snprintf(requestData.freeheapStr, sizeof(requestData.freeheapStr), "%d",  freeHeap); // global var. Was: ESP.getFreeHeap());

  // Copy the structure to the pool
  memcpy( http_request_data_pool[0], &requestData, sizeof(requestData));

  if (syncIdxRollover) {
    clr_sync_times();
    SyncTimesIndex = 0; // reset the index
    syncIdxRollover = false;
  }
  
  // Make a POST request to a Google Apps Script
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(txt0);
    Serial.println(F("WiFi not connected. Exiting function"));
    return ret;
  }
  else {
    HTTPClient http;
    http.begin(serverName); // Specify the URL

    // We don't print the deployment URL to the Google Apps Script
    // We only print the serverName. It was extracted by a call in setup().
    Serial.print(F("serverName = \"")); // + Google Apps Script deployment url =\n\t\""));
    Serial.print(sServerShortName); // Print the first part of the serverName
    Serial.println("\"");

    http.addHeader("Content-Type", "application/json"); // Specify content-type header

    http_request_data* requestDataPtr = (http_request_data*)(http_request_data_pool[0]);

    /* For request parameters see. https://developers.google.com/apps-script/guides/web */
    char jsonData[256];
    snprintf(jsonData, sizeof(jsonData),
      "{\"date\":\"%s\",\"sync_time\":%s,\"diff_t\":%s,\"index\":%s,\"display\":%s,\"f_heap\":%s,\"id\":\"M5Dial\"}",
                 //s0,                s1,           s2,          s3,            s4,           s5);
      requestDataPtr->datetimeStr,   // s0
      requestDataPtr->syncTimeStr,   // s1
      requestDataPtr->diffTimeStr,   // s2
      requestDataPtr->indexStr,      // s3
      requestDataPtr->displayStr,    // s4
      requestDataPtr->freeheapStr);  // s5
    char httpRequestData[512];
    snprintf(httpRequestData, sizeof(httpRequestData), "{\"key\":\"%s\", \"data\":%s}", apikey, jsonData);
    Serial.print(F("Going to post a http request with these data: \""));
    Serial.printf("{\"data\":\"%s\"}\n", jsonData);
    
    //  The char buffer jsonData above, was received by the Google Scripts script as: 
    //  "postData":{"contents":"\"{date\":\"1999-11-30T1:47:4Z\",\"sync_time\":\"1732200607\",\"diff_t\":\"302\",\"index\":\"0\",
    //    \"display"\":\"1\",\"f_heap\":\"263048\",\"id\":\"M5Dial\"} 
    //  See the exported log in: <Drive>:\Dropbox\<User>\Hardware\M5Stack\M5Stack_Dial\Arduino\M5Dial_Timezones\logs\Google_Scrips_BetterLog_Logger.xlsx
    
    // Give the mutex, temporarily, otherwise http.POST cannot do its work
    xSemaphoreGive(mutex); 

    int httpResponseCode = http.POST(httpRequestData);

    xSemaphoreTake(mutex, portMAX_DELAY); // Take the mutex (back on)

    if (httpResponseCode > 0) {
      ret = true;
      Serial.print(F("HTTP Response code: "));
      Serial.println(String(httpResponseCode));
      if (httpResponseCode != 302) { // 302 = temporary redirection.
        String response = http.getString();
        Serial.print(F("Response: "));
        Serial.println(response);
      }
      sync_time = false; // reset flag
    } else {
      Serial.print(F("Error: "));
      Serial.print(String(httpResponseCode));
      Serial.println(F(" on HTTP request"));
    }
    // = StatusCode 302 = Redirection. We don't follow the redirection. 
    //   The data sent will arrive in the Google Sheet
    if (httpResponseCode == 200 || httpResponseCode == 302)  
      ret = true;
    
    http.end(); // Free resources
    requestDataPtr = nullptr;
    clear_http_request_data_pool();

  }
  if (!lStart) {
    handle_requestBusy = false; // Reset the flag
    xSemaphoreGive(mutex); // Give the mutex
  }
  return ret;
}

// end of code suggested by MS Copilot

void ck_BtnA() {
  bool ret = false;
  if (M5Dial.BtnA.wasPressed() || M5Dial.BtnA.wasHold()) { // 100 mSecs
    Serial.println(F("BtnA was pressed or was hold"));
    buttonPressed = true;
  }
  else
    buttonPressed = false;
}

unsigned int ck_touch(void) {
  unsigned long debounceDelay = 50; // 50 milliseconds debounce delay
  bool touchState = false;
  unsigned int ck_touch_cnt = 0;
  auto t = M5Dial.Touch.getDetail();
  bool currentTouchState = t.state; // M5.Touch.ispressed();

  if (currentTouchState != lastTouchState)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentTouchState != touchState)
    {
      touchState = currentTouchState;
      if (touchState)
      {
        ck_touch_cnt++;  // increase global var
        // The maximum value of an unsigned int is 4294967295
        if (ck_touch_cnt >= (4294967295 - 1))
          ck_touch_cnt = 4294967295 - 1; // keep it below the maximum. Prevent an overflow
      }
    }
  }
  lastTouchState = currentTouchState;
  return ck_touch_cnt;
}

std::string intToHex(int value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

bool ck_RFID(void) {
  #define MY_RFID_TAG_HEX SECRET_MY_RFID_TAG_NR_HEX
  bool lCardIsOK = false;
  M5Dial.Rfid.begin();
  delay(1000);
  if (M5Dial.Rfid.PICC_IsNewCardPresent() &&
      M5Dial.Rfid.PICC_ReadCardSerial()) {
    M5Dial.Display.clear();
    uint8_t piccType = M5Dial.Rfid.PICC_GetType(M5Dial.Rfid.uid.sak);
    // Check is the PICC of Classic MIFARE type
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        M5Dial.Display.clear(); // M5Dial.Display.fillScreen(TFT_BLACK); 
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextColor(TFT_RED);
        M5Dial.Display.drawString("card not", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 - 50);
        M5Dial.Display.drawString("supported", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
        return false;
    }
    std::string uid = "";
    std::string sMyRFID(MY_RFID_TAG_HEX);
    int le = sMyRFID.length();

    for (byte i = 0; i < M5Dial.Rfid.uid.size; i++) {    // Output the stored UID data.
      std::string hexString1 = intToHex(M5Dial.Rfid.uid.uidByte[i]);
      uid += hexString1;
    }
    bool lCkEq = true;
    // Check if the presented RFID card has the same ID as that of the ID in secret.h
    for (int i = 0; i < le; i++) {
      if (uid[i] != sMyRFID[i]) {
        lCkEq = false; // RFID Card not OK
        break;
      }
    }
    lCardIsOK = lCkEq;
  }
  M5Dial.Rfid.PICC_HaltA();
  M5Dial.Rfid.PCD_StopCrypto1();
  return lCardIsOK;
}

int calc_x_offset(const char* t, int ch_width_in_px) {
  int le = strlen(t);
  int char_space = 1;
  int ret = (M5Dial.Display.width() - ((le * ch_width_in_px) + ((le -1) * char_space) )) / 2;
  return (ret < 0) ? 0 : ret;
}

void start_scrn(void) {
  static constexpr const char* txt[] PROGMEM = {"TIMEZONES", "by Paulus", "Github", "@PaulskPt"};
  static constexpr int char_width_in_pixels[] PROGMEM = {16, 12, 12, 14};
  static constexpr const int vert2[] PROGMEM = {0, 60, 90, 120, 150}; 
  int x = 0;
  M5Dial.Display.clear(); // M5Dial.Display.clear(TFT_BLACK); ;
  M5Dial.Display.setTextColor(TFT_RED, TFT_BLACK);
  for (int i = 0; i < 4; ++i) {
    x = calc_x_offset(txt[i], char_width_in_pixels[i]);
    M5Dial.Display.setCursor(x, vert2[i + 1]);
    M5Dial.Display.println(txt[i]);
  }
  //delay(5000);
  M5Dial.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
}

void send_cmd_to_AtomEcho(void) {
  digitalWrite(PORT_B_GROVE_IN_PIN, LOW);
  digitalWrite(PORT_B_GROVE_OUT_PIN, HIGH);
  delay(100);
  digitalWrite(PORT_B_GROVE_OUT_PIN, LOW);
  delay(100); 
}

void force_sntp_sync(void) { 
  // sntp_init(); 
  timeval tv; 
  tv.tv_sec = 0; 
  tv.tv_usec = 0; 
  sntp_sync_time(&tv); // Force immediate synchronization if supported 
  Serial.println(F("SNTP synchronization requested"));
  static constexpr const char txt[] PROGMEM = "Sync forced";
  disp_msg(txt, true);
}

void setup(void) {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5Dial.begin(cfg, false, true);
  M5Dial.Power.begin();
  // Initialize the mutex
  mutex = xSemaphoreCreateMutex();
  // Pin settings for communication with M5Dial to receive commands from M5Dial
  // commands to start a beep.
  pinMode(PORT_B_GROVE_OUT_PIN, OUTPUT);
  digitalWrite(PORT_B_GROVE_OUT_PIN, LOW); // Turn Idle the output pin
  pinMode(PORT_B_GROVE_IN_PIN, INPUT);
  digitalWrite(PORT_B_GROVE_IN_PIN, LOW); // Turn Idle the input pin

  Serial.begin(115200);
  // Extract the first part of global variable
  // serverName and copy it to the global variable sServerShortName
  createServerShortName();

  static constexpr const char txt2[] PROGMEM = "M5Stack M5Dial Timezones, M5Atom Echo, RFID and data to Google test.";
  Serial.print("\n\n");
  Serial.println(txt2);
  M5Dial.Display.init();
  M5Dial.Display.setBrightness(50);  // 0-255
  M5Dial.Display.setRotation(0);
  M5Dial.Display.clear(); // M5Dial.Display.fillScreen(TFT_BLACK); 
  M5Dial.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
  M5Dial.Display.setColorDepth(1); // mono color
  M5Dial.Display.setFont(&fonts::FreeSans12pt7b);
  M5Dial.Display.setTextWrap(false);
  M5Dial.Display.setTextSize(1);

  start_scrn();
  create_maps();  // creeate zones_map
  delay(1000);
  // Try to establish WiFi connection. If so, Initialize NTP,
  if (connect_WiFi()) {
    // See: https://docs.espressif.com/projects/esp-idf/en/v5.0.2/esp32/api-reference/system/system_time.html#sntp-time-synchronization
    //  See also: https://docs.espressif.com/projects/esp-idf/en/v5.0.2/esp32/api-reference/kconfig.html#config-lwip-sntp-update-delay
    //  CONFIG_LWIP_SNTP_UPDATE_DELAY
    //  This option allows you to set the time update period via SNTP. Default is 1 hour.
    //  Must not be below 15 seconds by specification. (SNTPv4 RFC 4330 enforces a minimum update time of 15 seconds).
    //  Range:
    //  from 15000 to 4294967295
    //
    //  Default value:
    //  3600000
    //
    //  See: https://github.com/espressif/esp-idf/blob/v5.0.2/components/lwip/include/apps/esp_sntp.h
    //  SNTP sync status
    //      typedef enum {
    //        SNTP_SYNC_STATUS_RESET,         // Reset status.
    //        SNTP_SYNC_STATUS_COMPLETED,     // Time is synchronized.
    //        SNTP_SYNC_STATUS_IN_PROGRESS,   // Smooth time sync in progress.
    //      } sntp_sync_status_t;
    esp_sntp_initialize();  // name sntp_init() results in compilor error "multiple definitions sntp_init()"
    int status = esp_sntp_get_sync_status();
    zone_idx = 0; // needed to set here. Is needed in setTimezone()
    setTimezone();
  }
  M5Dial.Display.clear(); // M5Dial.Display.clear(TFT_BLACK);
}

void loop() {
  static constexpr const char txt0[] PROGMEM = "loop(): ";
  static constexpr const char txt1[] PROGMEM = "running on core ";
  static constexpr const char txt2[] PROGMEM = "SNTP sync interval: ";
  static constexpr const char txt3[] PROGMEM = "Error: Buffer size is not sufficient to hold the combined strings.";
  Serial.print(txt1);
  Serial.println(xPortGetCoreID());
  Serial.printf("%s%lu minutes\n", txt2, CONFIG_LWIP_SNTP_UPDATE_DELAY / 60000);
  const char *txts[] PROGMEM = {
  "Waking up!",     //  0
  "asleep!",        //  1
  "Reset...",       //  2
  "Bye...",         //  3
  "Going ",         //  4
  "display state",  //  5
  " changed to: ",  //  6
  "RTC updated ",   //  7
  "with SNTP ",     //  8 
  "datetime",       //  9
  "Free heap ",     // 10
  "WiFi  ",         // 11
  "reconnecting",   // 12
  " bytes",         // 13
  "is below ",      // 14
  "threshold of "   // 15
  };
  const unsigned long zone_chg_interval_t = 25 * 1000L; // 25 seconds
  unsigned long zone_chg_curr_t = 0L;
  unsigned long zone_chg_elapsed_t = 0L;
  unsigned long zone_chg_start_t = millis();
  unsigned int touch_cnt = 0;
  int connect_try = 0;
  bool disp_msg_shown = false;
  bool display_state = display_on;
  bool lStop = false;
  bool forceSync = false;
  bool use_rfid = true;
  char result[30];
  int loopnr = -1;

  while (true) {
    // This value will be sent as part of the data to the Google Sheets spreadsheet
    freeHeap = xPortGetFreeHeapSize();
    // First half ...
    if (!sntpBusy && !handle_requestBusy) {
      loopnr++;
      if (loopnr % 10 == 0) {
        Serial.printf("%s loopnr: %3d, ", txt0, loopnr);
        Serial.print(gtxt2);
        Serial.print(freeHeap);
        Serial.println(txts[13]);
      }
      if (loopnr > 990)
        loopnr = 0;

      M5Dial.update();
      if (use_rfid) {
        if (ck_RFID())
          touch_cnt++;
        else
          touch_cnt = ck_touch();
      }
      if (touch_cnt > 0) {
        touch_cnt = 0; // reset
        display_on = !display_on; // flip the display_on flag
        if (display_on != display_state) {
          display_state = display_on;
          Serial.print(txts[5]);
          Serial.print(txts[6]);
          Serial.println(display_state ? F("On") : F("Off"));
        }
        if (display_on) {
          if (i_am_asleep) {
            M5.Display.wakeup();
            i_am_asleep = false;
            M5.Display.setBrightness(50);  // 0 - 255
            disp_msg(txts[0], true);
            M5Dial.Display.setTextColor(TFT_ORANGE, TFT_BLACK);
            M5Dial.Display.clear(); // M5Dial.Display.clear(TFT_BLACK);
          }
        } else {
          if (!i_am_asleep) {

            if (strlen(txts[4]) + strlen(txts[1]) + 1 > sizeof(result)) { 
              // Handle the error: buffer is not large enough 
              Serial.println(txt3);
            } else { 
              snprintf(result, sizeof(result), "%s%s", txts[4], txts[1]);
              Serial.println(result);
              disp_msg(result, true); 
            }
            M5.Display.sleep();
            i_am_asleep = true;
            M5.Display.setBrightness(0);
            M5Dial.Display.clear(); // M5Dial.Display.fillScreen(TFT_BLACK);  
          }
        }
      }
      M5Dial.update();
      if (!buttonPressed)
        ck_BtnA(); // Sets/resets the global variable buttonPressed
      if ((buttonPressed) || (freeHeap <= MIN_FREE_HEAP)) {
        if (freeHeap <= MIN_FREE_HEAP)
          Serial.printf("%s%s%s%s%lu%s, %s to %s\n", txt0, txts[10], txts[14], txts[15], MIN_FREE_HEAP, txts[13], txts[4], txts[2]);
        disp_msg(txts[2], true); // there is already a wait of 6000 in disp_msg()
        esp_restart();
      }
      while (WiFi.status() != WL_CONNECTED) { // Check if we're still connected to WiFi
        if (!disp_msg_shown) {
          if (strlen(txts[4]) + strlen(txts[1]) + 1 > sizeof(result)) { 
              // Handle the error: buffer is not large enough 
              Serial.println(txt3);
          } else { 
            snprintf(result, sizeof(result), "%s%s", txts[11], txts[12]);
            Serial.println(result);
            disp_msg(result, false);
            disp_msg_shown = true;
          }
        }
        if (connect_WiFi()) {
          connect_try = 0;  // reset count
          break;
        } else {
          connect_try++;
        }
        if (connect_try >= 10) {
          lStop = true;
          break;
        }
        delay(1000);
      }
      if (lStop)
        break;
      disp_msg_shown = false;
    }
    if (forceSync || sync_time) {
      if (forceSync) {
        force_sntp_sync();  // force a sync of time
        forceSync = false;  // reset flag
      }
      if (initTime()) {
        
        if (set_RTC()) {
          Serial.printf("\n%s%s%s\n", txts[7], txts[8], txts[9]);
          handle_request(); // Send data to Google
          sync_time = false; // Clear the flag
        }
        
      }
    }
    // Second half...
    if (!sntpBusy && !handle_requestBusy) {
      zone_chg_curr_t = millis();
      zone_chg_elapsed_t = zone_chg_curr_t - zone_chg_start_t;

      if (lStart || (zone_chg_elapsed_t >= zone_chg_interval_t)) {
        if (lStart) {
          zone_idx = -1; // will be increased in code below
        }
        TimeToChangeZone = true;
        zone_chg_start_t = zone_chg_curr_t;
        if (zone_idx < (nr_of_zones-1))
          zone_idx++;
        else
          zone_idx = 0;
        setTimezone();
        TimeToChangeZone = false;
      }
      if (display_on) {
        disp_data();
      }
      lStart = false;
      M5Dial.update(); // read btn state etc.
    }
  }
  disp_msg(txts[3], false); // don't erase msg 
  do {
    delay(5000);
  } while (true);
}
