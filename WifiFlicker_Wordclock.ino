// WifiFlicker_Wordclock
//
// This creates a "Word Clock" using a WifiFlicker board (bascially an ESP8266
// module with a 5V level shifter), an array of 144 NeoPixels, and a RTC module
//
// It implements a web server to change the time and behavior of the clock. It
// uses the WiFiManager library to connect to a Wifi network.
//
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>  // Include Wire if you're using I2C
#include <RTClib.h>

////////////////////////////////////////////////////////////
//
#define AP_NAME "WordClock_Steve"
#define AP_PASSWORD "whattimeisit?"
const String Clock_Name = "Steve's";

////////////////////////////////////////////////////////////
// NeoPixel array, functions and variables
//
#define PIN 13
#define XWIDTH 12
#define YWIDTH 12

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(NEO_LENGTH, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(XWIDTH, YWIDTH, PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

#define COLOR_RED 0
#define COLOR_GREEN 1
#define COLOR_BLUE 2
#define COLOR_WHITE 3
#define COLOR_YELLOW 4
#define COLOR_CYAN 5
#define COLOR_PURPLE 6
const uint16_t colors[] = {
  matrix.Color(255, 0, 0),
  matrix.Color(0, 255, 0),
  matrix.Color(0, 0, 255),
  matrix.Color(200, 200, 200),
  matrix.Color(200, 200, 0),
  matrix.Color(0, 200, 200),
  matrix.Color(200, 0, 200)};

typedef struct Text {
  uint16_t x;
  uint16_t y;
  int len;
};

// Turn on a horizontal (left to right) string of Neopixels specified
// by the txt structure
//
void display_text_horiz (struct Text txt, uint16_t color) {
  for (int i=0; i<txt.len; i++)
    matrix.drawPixel(txt.x+i, txt.y, color);
}

// Turn on a vertical (top to bottom) string of Neopixels specified
// by the txt structure
//
void display_text_vert (struct Text txt, uint16_t color) {
  for (int i=0; i<txt.len; i++)
    matrix.drawPixel(txt.x, txt.y+i, color);
}

// Turn on a diagonal (top-left to bottom-right) string of
// Neopixels specified by the txt structure
//
void display_text_diag (struct Text txt, uint16_t color) {
  for (int i=0; i<txt.len; i++)
    matrix.drawPixel(txt.x+i, txt.y+i, color);
}

// Display a binary number horizontally (right to left)
// at a given position
// (pos_x, pos_y) => lsb position
//
void display_binary_horiz (int b, int w, int pos_x, int pos_y) {
  int x;
  for (x=0; x<w; x++) {
    if (b%2 == 0)
      matrix.drawPixel(pos_x-x, pos_y, 0);
    else
      matrix.drawPixel(pos_x-x, pos_y, colors[0]);
    b = b >> 1;
  }
  matrix.show();
}

// Display a binary number vertically (bottom to top)
// at a given position
// (pos_x, pos_y) => lsb position
void display_binary_vert (int b, int w, int pos_x, int pos_y) {
  int y;
  for (y=0; y<w; y++) {
    if (b%2 == 0)
      matrix.drawPixel(pos_x, pos_y-y, 0);
    else
      matrix.drawPixel(pos_x, pos_y-y, colors[0]);
    b = b >> 1;
  }
  matrix.show();
}

// Test every pixel in the matrix
//
void test_matrix () {
  int i,j,c;
  for (c=0; c<7; c++) {
    for (i=0; i<XWIDTH; i++) {
      for (j=0; j<YWIDTH; j++) {
        matrix.drawPixel(i, j, colors[c]);
        matrix.show();
        delay(25);
        matrix.fillScreen(0);
      }
    }
  }
  matrix.show();
}

////////////////////////////////////////////////////////////
// RTC, functions and variables
//
#define WIRE_PIN0 5
#define WIRE_PIN1 3
RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
DateTime lastTime = 0;
int lastHour = 0;


// NTP Client
//
#define NTP_SERVER "pool.ntp.org"
#define NTP_DEFAULT_TZ -3600*6
#define NTP_UPDATE_INTERVAL 3600*1000

int TimeZone = NTP_DEFAULT_TZ;
bool NTPEnable = false;
bool NTPValid = false;

WiFiUDP ntpUDP;
NTPClient timeClient (ntpUDP, NTP_SERVER, NTP_DEFAULT_TZ, NTP_UPDATE_INTERVAL);

void setRTCfromNTP () {
  DateTime now = rtc.now();
  if (NTPEnable && NTPValid)
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), timeClient.getHours(), timeClient.getMinutes(), timeClient.getSeconds()));
}


////////////////////////////////////////////////////////////
// Web Server, functions and variables
//
ESP8266WebServer server(80);

// Wait 5 minutes before giving up on Wifi
#define MANAGER_TIMEOUT 300

void handleRoot() {
  DateTime now = rtc.now();
  String content = "<html><body>"
    "<h1>hello, you successfully connected to " + Clock_Name + " Word Clock!</h1><br>"
    "The time is: " + now.hour() + ":" + now.minute() + ":" + now.second() + "<br>"
    "NTP Time Server updates are " + (NTPEnable?"enabled":"disabled") + "<br>"
    "<br>"
    "<form action=\"/SET_TIME\" method=\"POST\">"
    "Time: (hh:mm:ss)<br>"
    "<input type=\"text\" name=\"time_str\"><br>"
    "<input type=\"submit\" value=\"Set Time\">"
    "</form>"
    "<br>"
    "<form action=\"SET_NTP\" method=\"POST\">"
    "<input type=\"radio\" name=\"ntp\" value=\"1\"" + (NTPEnable?"checked":"") + ">Enable Time Server Query<br>"
    "<input type=\"radio\" name=\"ntp\" value=\"0\"" + (NTPEnable?"":"checked") + ">Disable Time Server Query<br>"
    "<input type=\"submit\" value=\"Set Time Server Query\">"
    "</form>"
    "<br>"
    "<form action=\"SET_TZ\" method=\"POST\">"
    "<input type=\"radio\" name=\"tz\" value=\"0\"" + ((TimeZone==0)?"checked":"") + ">GMT<br>"
    "<input type=\"radio\" name=\"tz\" value=\"-14400\"" + ((TimeZone==-14400)?"checked":"") + ">EDT/AST<br>"
    "<input type=\"radio\" name=\"tz\" value=\"-18000\"" + ((TimeZone==-18000)?"checked":"") + ">CDT/EST<br>"
    "<input type=\"radio\" name=\"tz\" value=\"-21600\"" + ((TimeZone==-21600)?"checked":"") + ">MDT/CST<br>"
    "<input type=\"radio\" name=\"tz\" value=\"-25200\"" + ((TimeZone==-25200)?"checked":"") + ">PDT/MST<br>"
    "<input type=\"radio\" name=\"tz\" value=\"-28800\"" + ((TimeZone==-28800)?"checked":"") + ">PST<br>"
    "<input type=\"submit\" value=\"Set Time Zone\">"
    "</form>"
    "<br>"
    "<form action=\"/UPDATE_TIME\" method=\"POST\">"
    "<input type=\"submit\" value=\"Update Time from Time Server\">"
    "</form>"
    "<br>"
    "<form action=\"/TEST\" method=\"POST\">"
    "<input type=\"submit\" value=\"Test Matrix\">"
    "</form>";
  server.send(200, "text/html", content);
  
}

// set time manually
//
void handleSetTime() {
  if (!server.hasArg("time_str") || (server.arg("time_str") == NULL)) {
    server.send(400, "text/plain", "400: Invalid Request");
  }
  String time_str = server.arg("time_str");
  int index1 = time_str.indexOf(":");
  int index2 = time_str.indexOf(":", index1+1);
  if ((index1 == -1) || (index2 == -1)) {
    server.send(400, "text/plain", "400: Invalid Request");
  } else {
    int hours = time_str.substring(0, index1).toInt();
    int minutes = time_str.substring(index1+1, index2).toInt();
    int seconds = time_str.substring(index2+1).toInt();

    DateTime now = rtc.now();
    rtc.adjust(DateTime(now.year(), now.month(), now.day(), hours, minutes, seconds));
    //server.send(200, "text/html", "<h1>Time set to " + time_str + "</h1>");
    server.sendHeader("Location","/");
    server.send(303);
  }
}

void handleSetNTP () {
  if (!server.hasArg("ntp") || (server.arg("ntp") == NULL)) {
    server.send(400, "text/plain", "400: Invalid Request");
  }
  if (server.arg("ntp") == "1") {
    NTPEnable = true;
    if (NTPEnable && (NTPValid = timeClient.forceUpdate()))
      setRTCfromNTP ();
  } else {
    NTPEnable = false;
    NTPValid = false;
  }
  server.sendHeader("Location","/");
  server.send(303);
}

// change time zone
void handleSetTZ () {
  if (!server.hasArg("tz") || (server.arg("tz") == NULL)) {
    server.send(400, "text/plain", "400: Invalid Request");
  }
  String offset_str = server.arg("tz");
  TimeZone = offset_str.toInt();
  timeClient.setTimeOffset(TimeZone);
  if (NTPEnable && (NTPValid = timeClient.forceUpdate()))
    setRTCfromNTP ();
  server.sendHeader("Location","/");
  server.send(303);
}

// update time from time server
void handleUpdateTime () {
  if (NTPEnable && (NTPValid = timeClient.forceUpdate()))
    setRTCfromNTP ();
  server.sendHeader("Location","/");
  server.send(303);
}

void handleTest() {
  test_matrix();
  //server.send(200, "text/html", "<h1>Running matrix text</h1>");
  server.sendHeader("Location","/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

////////////////////////////////////////////////////////////
// Positions of text on front bezel

struct Text text_a = {2, 0, 1};
struct Text text_project = {4, 4, 7};
struct Text text_by = {0, 8, 2};
struct Text text_fcch = {8, 9, 4};
struct Text text_it = {0, 0, 2};
struct Text text_is = {3, 0, 2};
struct Text text_five_min = {6, 2, 4};
struct Text text_ten_min = {9, 1, 3};
struct Text text_fifteen = {1, 1, 7};
struct Text text_twenty = {0, 2, 6};
struct Text text_minutes = {0, 3, 7}; 
struct Text text_half = {8, 3, 4};
struct Text text_past = {4, 4, 4};
struct Text text_to = {7, 4, 2};
struct Text text_one = {6, 6, 3};
struct Text text_two = {9, 8, 3};
struct Text text_three = {1, 7, 5};
struct Text text_four = {7, 5, 4};
struct Text text_five_hr = {8, 7, 4};
struct Text text_six = {0, 10, 3};
struct Text text_seven = {0, 5, 5};
struct Text text_eight = {3, 9, 5};
struct Text text_nine = {5, 8, 4};
struct Text text_ten_hr = {9, 6, 3};
struct Text text_eleven = {0, 6, 6};
struct Text text_oclock = {0, 11, 7};
struct Text text_midnight = {3, 10, 8};
struct Text text_noon = {1, 8, 4};
struct Text text_am = {8, 11, 2};
struct Text text_pm = {10, 11, 2};

bool wifi_enabled = false;

void setup() {
  // Init NeoPixel array
  //
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  matrix.setTextColor(colors[0]);
  matrix.fillScreen(0);
  display_text_horiz (text_a, colors[0]);
  matrix.show();

  // Init RTC over I2C
  //
  delay(100);
  Wire.begin(5,3);

  delay(3000); // wait for console opening

  if (! rtc.begin()) {
  //  Serial.println("Couldn't find RTC");
  //  while (1);
    // draw red dash
    for (int x=0; x<5; x++)
      matrix.drawPixel(x, 1, colors[COLOR_RED]);
  } else {
    display_text_diag (text_project, colors[COLOR_GREEN]);
    matrix.show();
    delay (1000);
    display_text_vert (text_by, colors[COLOR_YELLOW]);
  }
  matrix.show();

  if (rtc.lostPower()) {
  //  Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  delay (1000);

  // Init WifiManager
  //
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(MANAGER_TIMEOUT); // wait 10 minutes before giving up
  if (!wifiManager.autoConnect(AP_NAME, AP_PASSWORD)) {
    // draw red fcch
    display_text_horiz (text_fcch, colors[COLOR_RED]);
    matrix.show();
    delay(2000);
  } else {
    wifi_enabled = true;
    display_text_horiz (text_fcch, colors[COLOR_BLUE]);
    matrix.show();
    delay(2000);
  }

  // Init web server
  //
  if (wifi_enabled) {
    server.on("/", handleRoot);
    server.on("/SET_TIME", HTTP_POST, handleSetTime);
    server.on("/SET_NTP", HTTP_POST, handleSetNTP);
    server.on("/SET_TZ", HTTP_POST, handleSetTZ);
    server.on("/UPDATE_TIME", HTTP_POST, handleUpdateTime);
    server.on("/TEST", HTTP_POST, handleTest);
    server.onNotFound(handleNotFound);
    const char * headerkeys[] = {"User-Agent", "Cookie"} ;
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
    //ask server to track these headers
    server.collectHeaders(headerkeys, headerkeyssize);
    server.begin();
  }

  // Init time server
  //
  if (wifi_enabled) {
    timeClient.begin();
    NTPEnable = true;
    NTPValid = timeClient.forceUpdate();
  }

  matrix.fillScreen(0);
  matrix.show();
}

void loop() {

  DateTime now = rtc.now();
  if (wifi_enabled && NTPEnable)
    NTPValid = timeClient.update();

  if (now != lastTime) {

    int hour = now.hour();
    int minute = now.minute();

    if (wifi_enabled && NTPEnable && NTPValid && (hour != lastHour)) {
      setRTCfromNTP(); // won't take effect for one second
    }

    lastTime = now;
    lastHour = hour;
  
    matrix.fillScreen(0);
    
    uint16_t itis_color = colors[COLOR_WHITE];
    if (minute % 5 == 1)
      itis_color = colors[COLOR_RED];
    if (minute % 5 == 2)
      itis_color = colors[COLOR_YELLOW];
    if (minute % 5 == 3)
      itis_color = colors[COLOR_GREEN];
    if (minute % 5 == 4)
      itis_color = colors[COLOR_BLUE];    
    display_text_horiz (text_it, itis_color);
    display_text_horiz (text_is, itis_color);

    if ((minute >= 5) && (minute < 10)) {
      display_text_horiz (text_five_min, colors[COLOR_GREEN]);
    } else if ((minute >= 10) && (minute < 15)) {
      display_text_horiz (text_ten_min, colors[COLOR_GREEN]);
    } else if ((minute >= 15) && (minute < 20)) {
      display_text_horiz (text_fifteen, colors[COLOR_GREEN]);
    } else if ((minute >= 20) && (minute < 25)) {
      display_text_horiz (text_twenty, colors[COLOR_GREEN]);
    } else if ((minute >= 25) && (minute < 30)) {
      display_text_horiz (text_twenty, colors[COLOR_GREEN]);
      display_text_horiz (text_five_min, colors[COLOR_GREEN]);
    } else if ((minute >= 30) && (minute < 35)) {
      display_text_horiz (text_half, colors[COLOR_GREEN]);
    } else if ((minute >= 35) && (minute < 40)) {
      display_text_horiz (text_twenty, colors[COLOR_GREEN]);
      display_text_horiz (text_five_min, colors[COLOR_GREEN]);
    } else if ((minute >= 40) && (minute < 45)) {
      display_text_horiz (text_twenty, colors[COLOR_GREEN]);
    } else if ((minute >= 45) && (minute < 50)) {
      display_text_horiz (text_fifteen, colors[COLOR_GREEN]);
    } else if ((minute >= 50) && (minute < 55)) {
      display_text_horiz (text_ten_min, colors[COLOR_GREEN]);
    } else if (minute >= 55) {
      display_text_horiz (text_five_min, colors[COLOR_GREEN]);
    }

    if (((minute >= 5) && (minute < 30)) || (minute >= 35)) {
      display_text_horiz (text_minutes, colors[COLOR_YELLOW]);
    }
    if ((minute >= 5) && (minute < 35)) {
      display_text_horiz (text_past, colors[COLOR_YELLOW]);
    } else if (minute >= 35) {
      display_text_horiz (text_to, colors[COLOR_YELLOW]);
    }

    int disp_hour = hour;
    if (minute >= 35)
      disp_hour++;
    if (disp_hour == 24)
      disp_hour = 0;
      
    
    if (disp_hour == 0)
      display_text_horiz (text_midnight, colors[COLOR_BLUE]);
    if ((disp_hour == 1) || (disp_hour == 13))
      display_text_horiz (text_one, colors[COLOR_BLUE]);
    if ((disp_hour == 2) || (disp_hour == 14))
      display_text_horiz (text_two, colors[COLOR_BLUE]);
    if ((disp_hour == 3) || (disp_hour == 15))
      display_text_horiz (text_three, colors[COLOR_BLUE]);
    if ((disp_hour == 4) || (disp_hour == 16))
      display_text_horiz (text_four, colors[COLOR_BLUE]);
    if ((disp_hour == 5) || (disp_hour == 17))
      display_text_horiz (text_five_hr, colors[COLOR_BLUE]);
    if ((disp_hour == 6) || (disp_hour == 18))
      display_text_horiz (text_six, colors[COLOR_BLUE]);
    if ((disp_hour == 7) || (disp_hour == 19))
      display_text_horiz (text_seven, colors[COLOR_BLUE]);
    if ((disp_hour == 8) || (disp_hour == 20))
      display_text_horiz (text_eight, colors[COLOR_BLUE]);
    if ((disp_hour == 9) || (disp_hour == 21))
      display_text_horiz (text_nine, colors[COLOR_BLUE]);
    if ((disp_hour == 10) || (disp_hour == 22))
      display_text_horiz (text_ten_hr, colors[COLOR_BLUE]);
    if ((disp_hour == 11) || (disp_hour == 23))
      display_text_horiz (text_eleven, colors[COLOR_BLUE]);
    if (disp_hour == 12)
      display_text_horiz (text_noon, colors[COLOR_BLUE]);

    if ((disp_hour != 0) && (disp_hour != 12)) {
      display_text_horiz (text_oclock, colors[COLOR_CYAN]);
      if (disp_hour < 12)
        display_text_horiz (text_am, colors[COLOR_PURPLE]);
      else
        display_text_horiz (text_pm, colors[COLOR_PURPLE]);
    }
    matrix.show();
    
    //display_binary_vert (now.hour()/10,   4, 1,  11);
    //display_binary_vert (now.hour()%10,   4, 3,  11);
    //display_binary_vert (now.minute()/10, 4, 5,  11);
    //display_binary_vert (now.minute()%10, 4, 7,  11);
    //display_binary_vert (now.second()/10, 4, 9,  11);
    //display_binary_vert (now.second()%10, 4, 11, 11);  
    
    
    //Serial.print("Temperature: ");
    //Serial.print(rtc.getTemperature());
    //Serial.println(" C");
    
    //Serial.println();
    //delay(3000);
  }
  

  if (wifi_enabled)
    server.handleClient();
  delay (50);
}
