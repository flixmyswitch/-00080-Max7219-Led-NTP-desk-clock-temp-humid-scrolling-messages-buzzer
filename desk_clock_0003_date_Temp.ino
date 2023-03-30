/*  A very basic matrix clock using a Wemos d1 Mini and 4 x 1 Max7219 matrix
    Based upon https://www.youtube.com/watch?v=g62Atuf1cm4&t=324s
    Based upon the standard example MD_Parola example Parola_Zone_TimeMsg.ino
    
    Just add you wifi SSID and PASSWORD in to the myconfig.h file
   
    Edited by Andy @Flixmyswitch    www.flixmyswitch.com for updates on this code and more explanations

    code:- desk clock 0003 date, temp, humid & message display  dated 29 March 2023

    New in this code
    
    adding temp and humid reading.
    messages.
    buzzer

    using case statements case number must be secquantial as we incease by 1
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>  // Built-in for ESP8266
#else
#include <WiFi.h>  // Built-in for ESP32
#endif
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
//#include <SPI.h>   // use these for Arduino
//#include <time.h>  // use these for Arduino  /* time_t, struct tm, time, localtime, strftime */
#include "myconfig.h"
#include "Font_Data.h"
// needed for Parola
#include <DHT.h>
#define SPEED_TIME 75
#define PAUSE_TIME 0
#define MAX_MESG 101  // has to be 1 longer than your longest message

// These are for the temperature
#define DHT_PIN 2       // D4
#define DHT_TYPE DHT11  // we are using the blue one.  DHT22 white one would be better
#define TIME_DHT 28000  // time between readings its a slow sensor. not so much in this code
// but to fast a reading and it will fail make this time very slightly less than the scrolling interval
// to show the temp & humid this prevents the sensor heating itself by reading it too fast. this made
// a massive differance in the offset required

// DHT11 temp +/- temp 5%, humid +/- 5%   DHT22 +/- temp 2%, humid 2%. both repeatability +/- 1%.
// Dont use these is you want to send a rocket into space
// more reading here https://www.kandrsmith.org/RJS/Misc/Hygrometers/calib_dht22_dht11_sht71.html

int buzzer = 5;                       //D1 ***** Dont use D3/gpio0 as it grounds during booting  and the buzzer will sound
float humidity, celsius, fahrenheit;  // to store decimal numbers like 66.7

// char need to store info for the matrix
char hm_Char[10];
char yearChar[4], dayChar[30], fullChar[50], weekdayChar[10];
char hour_Char[3], min_Char[3], sec_Char[3];
char szTime[9];  // mm:ss
char szMesg[MAX_MESG + 1] = "";

byte hour_Int, min_Int, sec_Int;
// bytes can store 0 to 255, an int can store -32,768 to 32,767 so bytes take up less memory

String TimeFormat_str, Date_str;  // to select M or I for 24hrs or 12hr clock

// parola effects
textEffect_t scrollEffect = PA_SCROLL_LEFT;
textPosition_t scrollAlign = PA_LEFT;

uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 };  // Deg C font as this is not in our font files
uint8_t degF[] = { 6, 3, 3, 124, 20, 20, 4 };  // Deg F font  as this is not in our font files

uint32_t timerDHT = TIME_DHT;

//********* USER SETTINGS - CHANGE YOUR SETTING BELOW **********

String Time_display = "M";  // M for metric 24hrs clock,  I for imperial 12hrs clock

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  // See video number 45 you have a choice of 4 settings
#define MAX_DEVICES 4                      // As we have a 4x1 matrix
#define MAX_NUMBER_OF_ZONES 1

//Connections to a Wemos D1 Mini
const uint8_t CLK_PIN = 14;   // or SCK D5
const uint8_t DATA_PIN = 13;  // or MOSI  D7
const uint8_t CS_PIN = 15;    // or SS D8

//MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);  // I have called my instance "P"
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

byte Time_dim = 20;    // what time to auto dim the display 24 hrs clock
byte Time_bright = 7;  // what time to auto dim the display 24 hr clock
byte Max_bright = 3;   // max brightness of the maxtrix
byte Min_bright = 0;   // minimum brightness of the matrix 0-15

// *************** temp humid sensor crude offsets ***************************//

float Ctemp_Offset = 0.7;

// as the DHT11 is not very accurate and inside a tube its effected by the heat from the matrix and esp8266
// this is very crude but works once the clock has been on for a while and up to normal temp.  the temp
// change if you have been uploading code to the chip. also going to drill some vent holes in the top and
// bottom of the case  ********* adjust to your offset or comment out ******
// 0.8 at 2 when inside case

// to correct the temp and humid readings. very crude but works.
float Ftemp_Offset = 32.72;  // adjust to your offset or comment out  (1°C × 9/5) + 32 = 33.8°F
float Humid_Offset = 2;      // 10 offset when inside case at 2 seconds

// ********** Change to your WiFi credentials in the myconfig.h file, select your time zone and adjust your NTS **********

const char *ssid = WIFI_SSID;                         // SSID of local network
const char *password = WIFI_PW;                       // Password on network
const char *Timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Rome/italy see link below for more short codes -1
const char *NTP_Server_1 = "europe.pool.ntp.org";
const char *NTP_Server_2 = "time.nist.gov";

/*  Useful links

    Zones               https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

    NTP world servers   https://www.ntppool.org/en/

    World times         https://www.timeanddate.com/time/zones/
*/

// //Messages to display
char *Message(uint8_t code, char *psz, uint8_t len) {
  static const __FlashStringHelper *str[] = {
    F("note 1"),
    F("note 2"),                                                                                                // just short so we can show effects with all text on the matrix
    F("Message 3 this is a long 100 character message, just adjust the char above ********************  100"),  // 100
  };

  // https://www.lettercount.com/

  strncpy_P(psz, (const char PROGMEM *)str[code - 1], len);  //Copy characters from string to program memory
  psz[len] = '\0';

  return (psz);
}

//********* END OF USER SETTINGS**********/

DHT dht(DHT_PIN, DHT_TYPE);  // set up the dht

//Get Temperature & humidity

void getTemperature() {  // only read if our TIME_DHT has past.
  // Wait for a time between measurements
  if ((millis() - timerDHT) > TIME_DHT) {
    // Update the timer
    Serial.println("reading temp");
    Serial.println((millis() - timerDHT));
    timerDHT = millis();

    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    humidity = dht.readHumidity();
    humidity = humidity - Humid_Offset;

    if (humidity >= 100) {
      humidity = 99;  // so that the matrix does not overflow due to the font size of 4x7
    }
    // Read temperature as Celsius (the default)
    celsius = dht.readTemperature();
    celsius = celsius - Ctemp_Offset;
    // celsius=celsius+90; no need to check temp is greater than 100C as it will fit on the matrix at font size 4x7

    // Read temperature as Fahrenheit (is Fahrenheit = true)
    fahrenheit = dht.readTemperature(false);
    fahrenheit = fahrenheit - Ftemp_Offset;

    // Check if any reads failed and exit early (to try again)
    if (isnan(humidity) || isnan(celsius) || isnan(fahrenheit)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
  }
}


void getTime(char *psz, bool f = true)  // get the time and pull out the parts we need hours, minutes, seconds
{
  min_Int = atoi(min_Char);  // convert char to int
  hour_Int = atoi(hour_Char);
  sec_Int = atoi(sec_Char);

  // Serial.println(hour_Int);  // to check reading etc if you wish
  // Serial.println(hour_Char);
  // Serial.println(min_Int);
  // Serial.println(min_Char);
  // Serial.println(sec_Int);
  // Serial.println(sec_Char);
  // Serial.println("");

  //combine text and variables into a string for output to the Serial Monitor/matrix
  sprintf(psz, "%02d%c%02d", hour_Int, (f ? ':' : ' '), min_Int);  // add the seperating two dots between the Hours and minutes
  //  d or i for signed decimal integer, u – unsigned decimal integer, c for Character   	s a string of characters
}

// set up messages
void getMessage1(char *psz) {
  char szBuf1[33];  // must be at least 1 greater than the message length
  sprintf(psz, "%s", Message(1, szBuf1, sizeof(szBuf1) - 1));
}

void getMessage2(char *psz) {
  char szBuf2[33];  // must be at least 1 greater than the message length
  sprintf(psz, "%s", Message(2, szBuf2, sizeof(szBuf2) - 1));
}

void getMessage3(char *psz) {
  char szBuf3[101];  // Length of message plus 1
  sprintf(psz, "%s", Message(3, szBuf3, sizeof(szBuf3) - 1));
}

void shortDate(char *psz) {  // Formats date as: Fri Mar 17 11:28:33 2023
  sprintf(psz, "%s", dayChar);
  //sprintf(psz, "%02d%c%02d", hour_Int,':', minInt);
}


// https://www.tutorialspoint.com/c_standard_library/c_function_sprintf.htm

void setup() {
  Serial.begin(115200);
  StartWiFi();
  digitalWrite(buzzer, HIGH);  // so the buzzer is off during boot
  configTime(0, 0, NTP_Server_1, NTP_Server_2);
  setenv("TZ", Timezone, 1);

  TimeFormat_str = Time_display;  // using the string above to select the type of clock 24 hrs or 12hr

  UpdateLocalTime(TimeFormat_str);
  pinMode(buzzer, OUTPUT);  // set buzzer pin as output

  P.begin(0);  // start Parola
  P.displayClear();
  P.setInvert(false);

  P.setZone(0, 0, 3);  //  for HH:MM  zone 0 0,1,3 when not using PA_FLIP_UD ect below
  //P.setZone(1, 0, 3);
  P.setFont(0, nullptr);
  P.setIntensity(Max_bright);  // brightness of matrix

  P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  //P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);

  // we dont need this now but here how to turn the matrix 180 degrees
  // you also would have to change the text direction in the effects below
  //   P.setZone(0, 0, 3);
  //    P.setZoneEffect(0, true, PA_FLIP_UD);  // will make it upside down but reversed text
  //    P.setZoneEffect(0, true, PA_FLIP_LR);  // make the digits the not mirrored
  //    P.setZoneEffect(1, true, PA_FLIP_UD);
  //    P.setZoneEffect(1, true, PA_FLIP_LR);

  P.addChar('$', degC);  // where ever we put $ the ⁰C will appear with the superscript like o
  P.addChar('&', degF);  // where ever we put $ the ⁰F will appear with the superscript like o

  dht.begin();  // start the temp/humid sensor
}


void loop() {

  getTemperature();                 // get temp/humid reading
  UpdateLocalTime(TimeFormat_str);  // update time

  // Serial.println(hour_Int);
  // Serial.println(hour_Char);
  // Serial.println(minInt);
  // Serial.println(min_Char);
  // Serial.println(sec_Int);
  // Serial.println(sec_Char);
  // Serial.println("");

  if (hour_Int >= Time_dim || hour_Int <= Time_bright) P.setIntensity(Min_bright);  // based using 24hr clock to save code dim at 20:00 bright at 07:00
  else {
    P.setIntensity(Max_bright);  // matrix brightness
  }

  static uint32_t lastTime = 0;  // Memory (ms)
  static uint8_t display = 0;    // Current display mode
  static bool flasher = false;   // Seconds passing flasher

  P.displayAnimate();

  if (P.getZoneStatus(0)) {
    switch (display) {

      case 0:  // HH:MM format like 13:55 in a new font

        P.setFont(0, F4x7straight);
        P.setTextEffect(0, PA_PRINT, PA_NO_EFFECT);  // so use print no effects

        if (millis() - lastTime >= 1000) {
          lastTime = millis();
          getTime(szMesg, flasher);
          flasher = !flasher;
        }
        // time delay to show the time for a while
        if (sec_Int == 15 && sec_Int <= 45) {  // wait for both seconds to have equal 15 and 45 so this "time"
                                               // delay can vary a bit the first time around. dont use 00 and 30
                                               // otherwise you dont see the hour all change at say 19:59 to 20:00
          display++;

          P.setTextEffect(0, PA_PRINT, PA_SCROLL_LEFT);
        }
        break;


      case 1:  // scroll short date, Wed Mar 15 11:11:23 2023

        P.setFont(0, nullptr);  // use the built-in font
        P.displayZoneText(0, dayChar, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        display++;
        break;

      case 2:  // scroll full date Wednesday, 15, March, 2023, 11:11 the one we created

        P.setFont(0, nullptr);  // use the built-in font
        P.displayZoneText(0, fullChar, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        display++;
        break;

      case 3:  // message 1

        P.setFont(0, nullptr);  // use the built-in font
        P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_DOWN);
        getMessage1(szMesg);
        display++;
        break;

      case 4:  // scrolls message 3 the long one all 101 of them

        P.setFont(0, nullptr);  // use the built-in font
        P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        getMessage3(szMesg);
        display++;
        break;

      case 5:  // message 2

        P.setFont(0, F3x5std);
        P.displayZoneText(0, szMesg, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_SCROLL_LEFT, PA_SCROLL_DOWN);
        getMessage2(szMesg);
        display++;
        break;

      case 6:  // Formats date as: Wed Mar 15 10:20:34 2023

        P.setFont(0, F3x5std);  // small font just because we can
        P.setPause(0, 0);
        P.setTextEffect(0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        shortDate(szMesg);
        display++;
        break;

      case 7:  // Temperature deg Celsius

        P.setPause(0, 1500);
        P.setFont(0, F4x7straight);
        P.setTextEffect(0, PA_SCROLL_DOWN, PA_SCROLL_UP_RIGHT);
        dtostrf(celsius, 3, 1, szMesg); // turns a float into a string, float, min width, num digits after dec, where to store it
        strcat(szMesg, "$"); // appends the string to the char 
        display++;
        break;


      case 8:  // Relative Humidity

        P.setPause(0, 1500);
        P.setFont(0, F4x7straight);
        P.setTextEffect(0, PA_SCROLL_DOWN_LEFT, PA_SCROLL_UP);
        dtostrf(humidity, 3, 1, szMesg);
        strcat(szMesg, "%RH");
        if (humidity >= 66) {  // just an example how to use the buzzer
          digitalWrite(buzzer, LOW);
          delay(100);  // should not effect time or anomations if it does use millis
        }
        digitalWrite(buzzer, HIGH);  // buzzer off
        display = 0;                 // as its our last case statement
        break;
    }
    P.displayReset(0);  // zone 0 update the display
  }
}
//#########################################################################################
void UpdateLocalTime(String Format) {
  time_t now;  // = time(nullptr);
  time(&now);

  //Serial.println(time(&now));
  //Unix time or Epoch or Posix time, time since 00:00:00 UTC 1st jan 1970 minus leap seconds
  // an example as of 28 feb 2023 at 16:19 1677597570

  //See http://www.cplusplus.com/reference/ctime/strftime/

  if (TimeFormat_str == "M") {

    strftime(hm_Char, 10, "%H:%M", localtime(&now));  // hours and minutes NO flashing dots 14:06
  } else {
    strftime(hm_Char, 10, "%I:%M", localtime(&now));  // Formats hour as: 02:06 ,  12hrs clock
  }

  strftime(hour_Char, 3, "%H", localtime(&now));                     // hours xx
  strftime(min_Char, 3, "%M", localtime(&now));                      // minutes xx
  strftime(sec_Char, 3, "%S", localtime(&now));                      // seconds xx
  strftime(yearChar, 5, "%G", localtime(&now));                      // year xxxx
  strftime(weekdayChar, 12, "%A", localtime(&now));                  // day of the week like Thursday
  strftime(dayChar, 30, "%c", localtime(&now));                      // Formats date as: Thu Aug 23 14:55:02 2001
  strftime(fullChar, 50, "%A, %d, %B, %G, %H:%M", localtime(&now));  // as the above includes the seconds lets make our own

  // convert char to int      NOTE  based on 24hr time so we can select when to dim the matrix

  min_Int = atoi(min_Char);
  hour_Int = atoi(hour_Char);
  sec_Int = atoi(sec_Char);
  String dayChar(dayChar);
  // Serial.println(hour_Int);
  // Serial.println(hour_Char);
  // Serial.println(minInt);
  // Serial.println(min_Char);
  // Serial.println(sec_Int);
  // Serial.println(sec_Char);
  // Serial.println("");
  //wdayInt = atoi(weekdayChar);
  //Date_str = dayChar;
}

//#########################################################################################
void StartWiFi() {
  /* Set the ESP to be a WiFi-client, otherwise by default, it acts as ss both a client and an access-point
      and can cause network-issues with other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  Serial.print(F("\r\nConnecting to SSID: "));
  Serial.println(String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(" ");
  Serial.print("WiFi connected to address: ");
  Serial.print(WiFi.localIP());
  Serial.println(" ");
}

//==================================================================================
// void dispDate() {
//   P.displayClear();
//   P.displayText("Date", PA_LEFT, 30, 30, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
//   while (!P.displayAnimate())
//     ;
//   delay(500);
//   for (int i = 1; i <= 7; i++) {
//     //hum  = bme.readHumidity();
//     //humString  = "   " + String(hum) + " %";
//     P.print(weekdayChar);
//     delay(1000);
//   }
// }
