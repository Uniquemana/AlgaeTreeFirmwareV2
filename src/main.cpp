/*--------------------------------------------------------------------
Author: Kristians Abolins
Date: 03.11.2023
SerialInputs - None
SerialPrompts - Full data representation in .json format. If SD card or RTC is not readable will promt error
Software version - 2.2.1 //Updates: Data transfer between Mega and ESP32, New UI layout, Disabled all heaters. 
Hardware version - 2.1.0 //Updates: ESP32 data transfer, PCB shield(no changes in electronics), Removed heaters, added second aerator.
--------------------------------------------------------------------*/


#define LEFT_HEATER_T A0    //Analog pin
#define LEFT_WATER_T 24     //Digital pin
#define LEFT_HEATER_PWM 3   //PWM pin
#define RIGHT_HEATER_T A1   //Analog pin
#define RIGHT_WATER_T 25    //Digital pin
#define RIGHT_HEATER_PWM 4  //PWM pin
#define TOWER_LED_PWM 6     // PWM pin for both tower leds
#define LED 2               //PWM pin
#define DHTPIN 7            // DHT11 data pin is connected to Arduino 7 pin.
#define TFT_CS 9            // PWM pin
#define TFT_DC 10           // PWM pin
#define TFT_SCLK 13         // PWM pin
#define TFT_MOSI 11         // PWM pin
#define DHTTYPE DHT11
#define YELLOW 35
#define RED 37
#define GREEN 39
#define REALTIMECLOCK 49
#define RXp2 17 //ESP32 communication
#define TXp2 16 //ESP32 communication


#include <Wire.h>
#include <Adafruit_SCD30.h>  //CO2, Humidity, Air Temperature
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>         //screen library
#include <Fonts/FreeSans9pt7b.h>  // sans font
#include <Fonts/FreeMono9pt7b.h>
#include <SPI.h>
#include <stdio.h>
#include <DHT.h>
#include <SD.h>                 //sd card logging library
#include <OneWire.h>            //water temp sensor library
#include <DallasTemperature.h>  //water temp sensor library
#include <ThreeWire.h>          //Real time clock
#include <RtcDS1302.h>          //Real time clock
//#include <TimeInterval.h>
#include "bitmaps.h"
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <TimerOne.h>
#include <Regexp.h>

//BH1750FVI myLux(0x23);
DHT dht(DHTPIN, DHTTYPE);
File myFile;  //create a myFile variable
OneWire oneWire(LEFT_WATER_T);
OneWire oneWire2(RIGHT_WATER_T);
DallasTemperature sensors(&oneWire);
DallasTemperature sensors2(&oneWire2);
ThreeWire myWire(31, 8, 33);       // IO, SCLK, CE for Realtime clock
RtcDS1302<ThreeWire> Rtc(myWire);  //realtime clock
//TimeInterval myDelay;

#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif

Adafruit_ST7735 tft = Adafruit_ST7735(9, 10, 11, 13, 8);
Adafruit_SCD30 scd30;  // scd30 co2,humidity,temperature sensor
//Black theme
#define COLOR2 ST7735_WHITE
#define COLOR1 ST7735_BLACK

String SV = "2.2.1";
String HV = "2.1.0";
int deviceID = 1002;
const int buttonPin = 48;  // the number of the pushbutton pin
float air_humidity, air_temp, co2;
float humidity;
int temperature;
int left_water_temp;
int right_water_temp;
int tower_led_pwm;
String message;
int counter = 0;
String fileName;
String fileFormat = ".txt";
int count = 0;                                                           // count to chich SD dump dumps the data (if count is devidable by 5)
int counting = 0;                                                        // counter to SD dump data log heaters

bool ADMIN_SCREEN = false;                                               //Admin Screen

bool lefttowerstatuss;
bool righttowerstatuss;
bool clearScreenOnce;

int buttonStatePrevious = LOW;                    // previousstate of the switch
unsigned long minButtonLongPressDuration = 3000;  // Time we wait before we see the press as a long press
unsigned long buttonLongPressMillis;              // Time in ms when we the button was pressed
bool buttonStateLongPress = false;                // True if it is a long press
const int intervalButton = 50;                    // Time between two readings of the button state
unsigned long previousButtonMillis;               // Timestamp of the latest reading
unsigned long buttonPressDuration;                // Time the button is pressed in ms

unsigned long previousMillis = 0;
unsigned long currentMillis1 = millis();
unsigned long previousMillis1 = 0;
unsigned long currentMillis2 = millis();
unsigned long previousMillis2 = 0;
unsigned long currentMillis3 = millis();
unsigned long previousMillis3 = 0;
unsigned long currentMillis4 = millis();
unsigned long previousMillis4 = 0;
unsigned long currentMillis5 = millis();
unsigned long previousMillis5 = 0;
const long interval = 10000;
const long interval2 = 10000;
const long counterInterval = 1000;

int brightness = 0;     // Current LED brightness

SoftwareSerial espSerial(19, 18);  // Define the ESP32 communication pins



int calculateBrightness(long seconds) {
  // Define brightness variables
  int brightnessMin = 50;
  int brightnessMax = 255;
  int brightness = 0;

  // Define time variables
  long startTime = 28800;
  long peakTime = 43200;
  long endTime = 75600;

  // Calculate brightness based on time of day
  if (seconds >= startTime && seconds <= peakTime) {
    // Brightness increases from 0 to 255 from 8 AM to 12 PM
    brightness = map(seconds, startTime, peakTime, brightnessMin, brightnessMax);
  } else if (seconds > peakTime && seconds <= endTime) {
    // Brightness decreases from 255 to 0 from 12 PM to 8 PM
    brightness = map(seconds, peakTime, endTime, brightnessMax, brightnessMin);
  }

  // Constrain brightness to range 0-255
  brightness = constrain(brightness, 0, 255);

  return brightness;
}

// Function for reading the button state
int readButtonState(unsigned long currentMillis) {

  // If the difference in time between the previous reading is larger than intervalButton
  if (currentMillis - previousButtonMillis > intervalButton) {

    // Read the digital value of the button (LOW/HIGH)
    int buttonState = digitalRead(buttonPin);

    // If the button has been pushed AND
    // If the button wasn't pressed before AND
    // IF there was not already a measurement running to determine how long the button has been pressed
    if (buttonState == HIGH && buttonStatePrevious == LOW && !buttonStateLongPress) {
      buttonLongPressMillis = currentMillis;
      // made by Saikne - Kristians Abolins
      buttonStatePrevious = HIGH;
      Serial.println("Button pressed");
    }

    // Calculate how long the button has been pressed
    buttonPressDuration = currentMillis - buttonLongPressMillis;

    // If the button is pressed AND
    // If there is no measurement running to determine how long the button is pressed AND
    // If the time the button has been pressed is larger or equal to the time needed for a long press
    if (buttonState == HIGH && ADMIN_SCREEN == false && !buttonStateLongPress && buttonPressDuration >= minButtonLongPressDuration) {
      buttonStateLongPress = true;
      ADMIN_SCREEN = true;
      Serial.println("ADMIN SCREEN: TRUE");
    }
    if (buttonState == HIGH && ADMIN_SCREEN == true && !buttonStateLongPress && buttonPressDuration >= minButtonLongPressDuration) {
      buttonStateLongPress = true;
      ADMIN_SCREEN = false;
      Serial.println("ADMIN SCREEN: FALSE");
    }

    // If the button is released AND
    // If the button was pressed before
    if (buttonState == LOW && buttonStatePrevious == HIGH) {
      buttonStatePrevious = LOW;
      buttonStateLongPress = false;
      Serial.println("Button released");
    }

    // store the current timestamp in previousButtonMillis
    previousButtonMillis = currentMillis;
  }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}
void printDateTimeTFT(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  tft.print(datestring);
}
void printDateTimeSD(const RtcDateTime& dt) {
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  myFile.print(datestring);
}


void dumpSD() {
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  //Serial.print("Initializing SD card...");
  if (!SD.begin(53)) {
    Serial.println("main SD initialization failed!");
    return;
  } 
  else {
    //Serial.println("initialization done.");
    RtcDateTime now = Rtc.GetDateTime();  //rtc get time
    char buffer[256] = "";
    uint16_t ThisYear;
    uint8_t ThisMonth, ThisDay, ThisHour, ThisMinute, ThisSecond;
    ThisYear = now.Year();
    ThisMonth = now.Month();
    ThisDay = now.Day();
    ThisHour = now.Hour();
    ThisMinute = now.Minute();
    ThisSecond = now.Second();
    // sprintf(buffer, "%04u%02u%02u", ThisYear, ThisMonth, ThisDay);
    sprintf(buffer, "log", ThisYear, ThisMonth, ThisDay);
    myFile = SD.open(buffer + fileFormat, FILE_WRITE);
  }

  if (myFile) {  // if the file opened okay, write to it:

    RtcDateTime now = Rtc.GetDateTime();  //rtc get time
    myFile.print(" \n");
    printDateTimeSD(now);
    // Making the data colums inside the opened file
    myFile.print(" \t");
    myFile.print(scd30.temperature);
    myFile.print(" \t");
    myFile.print(scd30.relative_humidity);
    myFile.print("\t");
    myFile.print(scd30.CO2);
    myFile.print("\t");
    myFile.print(left_water_temp);
    myFile.print(" \t");
    myFile.print(right_water_temp);
    myFile.print("\t");
    myFile.print(tower_led_pwm);
    myFile.print(" \t");

    myFile.close();
  }

  else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
    return;
  }
}

// Function to convert RtcDateTime to String
String RtcDateTimeToString(const RtcDateTime& dt) {
  char date[12];
  sprintf(date, "%04d-%02d-%02d", dt.Year(), dt.Month(), dt.Day());
  return String(date);
}

int getUniqueDateCount() {
  File myFile;
  int uniqueDays = 0;
  int uniqueMonths = 0;
  int uniqueYears = 0;

  // Strings to keep track of unique dates, months, and years
  String previousDay = "";
  String previousMonth = "";
  String previousYear = "";

  // Open the file for reading
  if (SD.begin(53)) {
    myFile = SD.open("log.txt", FILE_READ);
    if (myFile) {
      while (myFile.available()) {
        String line = myFile.readStringUntil('\n');
        // Extract the timestamp (assuming it's in the format "DD/MM/YYYY hh:mm:ss" or "DD/MM/YYYY")
        String timestamp;
        if (line.length() >= 10) {
          timestamp = line.substring(0, 10);
        } else {
          continue; // Skip lines with invalid timestamps
        }

        String day = timestamp.substring(0, 2);
        String month = timestamp.substring(3, 5);
        String year = timestamp.substring(6, 10);

        if (day != previousDay) {
          uniqueDays++;
          previousDay = day;
        }
        if (month != previousMonth) {
          uniqueMonths++;
          previousMonth = month;
        }
        if (year != previousYear) {
          uniqueYears++;
          previousYear = year;
        }
      }
      myFile.close();
    } else {
      // File open error
      Serial.println("Error opening log.txt");
    }
  } else {
    // SD card initialization error
    Serial.println("SD card initialization failed");
  }

  // Calculate the sum of unique days, months, and years
  int totalUniqueDates = uniqueDays + uniqueMonths + uniqueYears;

  // Output the total count
  Serial.print("Total Unique Dates: ");
  Serial.println(totalUniqueDates);

  return totalUniqueDates;
}


void setup(void) {
  Serial.begin(9600);
  espSerial.begin(9600);      // Serial communication with ESP32
  //Serial2.begin(115200, SERIAL_8N1, 0, 1);
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();
  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }
  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
  Rtc.SetDateTime(compiled);

  while (!Serial) delay(10);  // will pause until serial console opens
  if (!scd30.begin()) {       //initialize
    Serial.println("Failed to find SCD30 chip");
    while (1) { delay(10); }
  }
  
//  ----SET FOR RTC CALIBRATION----//
  // int year = 2024;
  // int month = 01;
  // int date = 19;
  // int hour = 17;
  // int minute = 19;
  // now = RtcDateTime(year, month, date, hour, minute, now.Second());
  // Rtc.SetDateTime(now);
  // ----END----//
  Serial.print("Starting sensors...");
  sensors.begin();   //DS18B20 left water temp sensor
  sensors2.begin();  //DS18B20 right water temp sensor
  Serial.print("Sensors started successfully!");
  Serial.print("Initializing buttons and LEDs...");
  pinMode(buttonPin, INPUT);
  pinMode(LEFT_WATER_T, INPUT_PULLUP);
  pinMode(YELLOW, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(REALTIMECLOCK, OUTPUT);
  Serial.print("Buttons and LEDs initialized successfully!");
  tft.initR(INITR_BLACKTAB);  // initialize a ST7735S chip, black tab
  tft.setRotation(1);  //screen rotation

  //-----START Welcome and Starting SCREEN-----
  Serial.print("Starting screen...");
  tft.fillScreen(COLOR1);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(20, 60);
  tft.print("ALGAE TREE");
  tft.setTextSize(1);
  tft.setCursor(20, 100);
  tft.setFont(NULL);
  tft.setTextSize(1);
  //tft.print("Developed by");
  // tft.setCursor(20, 105);
  //tft.print("saikne.com");
  delay(1000);
  tft.fillScreen(COLOR1);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(30, 60);
  tft.print("STARTING...");
  delay(1500);
  tft.fillScreen(COLOR1);
  Serial.print("Starting screen finished");

  Timer1.initialize(5000000);  // Set the interrupt interval to 5 seconds (5,000,000 microseconds)
}


void loop() {
  unsigned long currentMillis = millis();
  //currentMillis = millis();  // store the current time
  readButtonState(currentMillis);  // read the button state
  RtcDateTime now = Rtc.GetDateTime();  //rtc get time
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Create a JSON document
    StaticJsonDocument<512> doc;  // Adjust the size based on your data

    // Extract individual components of the date and time
    uint16_t year = now.Year();
    uint8_t month = now.Month();
    uint8_t day = now.Day();
    uint8_t hour = now.Hour();
    uint8_t minute = now.Minute();
    uint8_t second = now.Second();
    // Populate the JSON document
    doc["deviceID"] = deviceID;
    doc["CO2"] = scd30.CO2;
    doc["air_temp"] = scd30.temperature;
    doc["air_humid"] = scd30.relative_humidity;
    doc["left_water_temp"] = left_water_temp;
    doc["right_water_temp"] = right_water_temp;
    doc["tower_led_pwm"] = tower_led_pwm;

    // Create a nested JSON object for the date and time
    JsonObject timeObject = doc.createNestedObject("time");
    timeObject["year"] = year;
    timeObject["month"] = month;
    timeObject["day"] = day;
    timeObject["hour"] = hour;
    timeObject["minute"] = minute;
    timeObject["second"] = second;
  
    // Serialize the JSON document into a string
    String jsonString;
    serializeJson(doc, jsonString);

    // Send the JSON data to the ESP32
    espSerial.println(jsonString);
    Serial.println(jsonString);
    Serial.print("\n");

    // Resume the main program execution
    Timer1.resume();
  }

  // int totalUniqueDates = getUniqueDateCount();

  //Tower LEDs
  long seconds = (long(now.Hour()) * 3600) + (long(now.Minute()) * 60) + long(now.Second());
  int brightness = calculateBrightness(seconds);
  tower_led_pwm = brightness;
  analogWrite(TOWER_LED_PWM, brightness);
  

  sensors.requestTemperatures();                   //request for left DS18B20 water temp sensor
  sensors2.requestTemperatures();                  //request for right DS18B20 water temp sensor
  left_water_temp = sensors.getTempCByIndex(0);    //assign left DS18B20 water temp sensor
  right_water_temp = sensors2.getTempCByIndex(0);  //assign right DS18B20 water temp sensor


  if ((right_water_temp > 40) || (right_water_temp < 1)) {
    righttowerstatuss = false;
  }
  else {
    righttowerstatuss = true;
  }

  if ((left_water_temp > 40) || (left_water_temp < 1)) {
    lefttowerstatuss = false;
  }
  else {
    lefttowerstatuss = true;
  }

  //Counter counting
  currentMillis5 = millis();
  if (currentMillis5 - previousMillis5 >= counterInterval) {
    previousMillis5 = currentMillis5;
    counting++;
    }
  
  //Dump to SD car the data
  if (((counting % 5) == 0) && (counting != 1)) {
    dumpSD();
    //Serial.println("Dumped to SD");
  }

// If we are NOT in the admin screen
  if (ADMIN_SCREEN == false) {
    if (clearScreenOnce == true){
      tft.fillScreen(COLOR1);
      clearScreenOnce = false;
    }
    int uniqueDays = getUniqueDateCount();

    //Starting the first screen
    currentMillis1 = millis();
    if (currentMillis1 - previousMillis1 >= interval) {
      previousMillis1 = currentMillis;
      tft.fillScreen(COLOR1);
      tft.setTextColor(COLOR2);
      tft.setFont(&FreeSansBold9pt7b);
      tft.setTextSize(1);
      tft.setCursor(15,30);
      tft.fillRect(15, 10, 20, 20, COLOR1);
      if (scd30.dataReady()) {
        if (!scd30.read()) {
          Serial.println("Error reading sensor data");
            return;
          }
        tft.print(scd30.temperature, 0);
      }
      tft.print("C");
      // tft.drawBitmap(37,15,airTempIcon,19,19,COLOR2);

      // //file off
      // tft.drawBitmap(55,15,fileOFF,19,19,COLOR2);

      // //file on
      tft.drawBitmap(65,15,fileON,19,19,COLOR2);

      // //wifi off
      // tft.drawBitmap(75,15,wifiOFF,19,19,COLOR2);

      // //wifi on
      // tft.drawBitmap(85,15,wifiON,19,19,COLOR2);

      // //Bluetooth off
      // tft.drawBitmap(100,15,bluetoothOFF,19,19,COLOR2);

      // //Bluetooth on
      // tft.drawBitmap(95,20,bluetoothON,19,19,COLOR2);

      //airHumid
      tft.setCursor(115,30);
      tft.fillRect(115, 10, 20, 20, COLOR1);
      tft.print(scd30.relative_humidity, 0);
      tft.drawBitmap(135,15,waterDrop,19,19,COLOR2);

      //airCO2
      tft.setTextSize(1);
      tft.setFont(&FreeSansBold24pt7b);
      if (scd30.CO2 <= 999){
        tft.setCursor(25, 80);
        tft.drawRoundRect(23, 40, 115, 55, 5, COLOR1);
        tft.fillRect(25, 45, 110, 40, COLOR1);

        tft.setCursor(40,80);
        tft.drawRoundRect(30, 40, 95, 55, 5, COLOR2);
        tft.fillRect(32, 45, 90, 40, COLOR1);
        tft.print(scd30.CO2, 0);
      }
      else{
        tft.setCursor(40,80);
        tft.drawRoundRect(30, 40, 95, 55, 5, COLOR1);
        tft.fillRect(32, 45, 90, 40, COLOR1);

        tft.setCursor(25, 80);
        tft.drawRoundRect(23, 40, 115, 55, 5, COLOR2);
        tft.fillRect(25, 45, 110, 40, COLOR1);
        tft.print(scd30.CO2, 0);
      }
      tft.setFont(NULL);
      tft.setCursor(70, 85);
      tft.print("PPM");

      if (righttowerstatuss == false){
        tft.drawBitmap(135,100, warning,19,19,COLOR2);
        tft.drawBitmap(135,88, rightArrow,19,19,COLOR2);
      }
      else{
        tft.fillRect(130, 90, 40, 30, COLOR1);
      }

      if(lefttowerstatuss == false){
        tft.drawBitmap(10,100, warning,19,19,COLOR2);
        tft.drawBitmap(10,88, leftArrow,19,19,COLOR2);
      }
      else{
        tft.fillRect(0, 90, 30, 35, COLOR1);
      }

    }

    //Starting the other screen
    currentMillis2 = millis();
    if (currentMillis2 - previousMillis2 >= interval2) {
      previousMillis2 = currentMillis2;  
      tft.fillScreen(COLOR1);
      tft.drawBitmap(30,30, breath,96,96,COLOR2);
      tft.setCursor(100,60);
      tft.setFont(&FreeSansBold24pt7b);
      tft.print(uniqueDays);
    }
  }

  if (ADMIN_SCREEN == true) {
    clearScreenOnce = true;
    currentMillis4 = millis();
    if (currentMillis4 - previousMillis4 >= interval) {
      previousMillis4 = currentMillis4;
      tft.fillScreen(COLOR1);
      tft.setTextColor(COLOR2);
      tft.setFont(NULL);
      tft.setTextSize(1);

      //first line
      tft.setCursor(15, 7);
      tft.print("CO2 level:");
      if (scd30.dataReady()) {
        if (!scd30.read()) {
          Serial.println("Error reading sensor data");
          return;
        }
        tft.setCursor(80, 7);
        tft.print(scd30.CO2, 1);
      }
      tft.setCursor(135, 7);
      tft.print("PPM");

      //second line
      tft.setCursor(10, 17);
      tft.print("Air Temp:");
      tft.setCursor(100, 15);
      tft.print(scd30.temperature, 1);
      tft.setCursor(125, 17);
      tft.print("C");

      //third line
      tft.setCursor(5, 27);
      tft.print("Humidity:");
      tft.setCursor(100, 25);
      tft.print(scd30.relative_humidity, 1);
      tft.setCursor(125, 27);
      tft.print("%");

      //fourth line
      tft.setCursor(5, 37);
      tft.print("L water temp:");
      tft.setCursor(100, 35);
      tft.print(left_water_temp, 1);
      tft.setCursor(125, 37);
      tft.print("C");

      //fifth line
      tft.setCursor(5, 47);
      tft.print("R water temp:");
      tft.setCursor(100, 45);
      tft.print(right_water_temp, 1);
      tft.setCursor(125, 47);
      tft.print("C");

      //sixth line
      tft.setCursor(5, 57);
      tft.print("tower led pwm:");
      tft.setCursor(100, 55);
      tft.print(tower_led_pwm, 1);
      tft.setCursor(125, 57);

      //tenth line
      tft.setCursor(5, 97);
      tft.print("TIME:");
      tft.setCursor(40, 95);
      printDateTimeTFT(now);

      //eleventh line
      tft.setCursor(5, 107);
      tft.print("SW_V:");
      tft.setCursor(40, 107);
      tft.print(SV);

      //twelvth line
      tft.setCursor(75, 107);
      tft.print("HW_V:");
      tft.setCursor(100, 107);
      tft.print(HV);
    }
  }

  count++;  //counts from 0 to infinity in background
  // Serial.print("counting: ");
  // Serial.println(counting);

  float CO2 = scd30.CO2;
  if (CO2 <= 1000) {
    digitalWrite(GREEN, HIGH);
    digitalWrite(YELLOW, LOW);
    digitalWrite(RED, LOW);
  }
  if ((CO2 > 1000) && (CO2 < 2000)) {
    digitalWrite(GREEN, LOW);
    digitalWrite(YELLOW, HIGH);
    digitalWrite(RED, LOW);
  }
  if (CO2 >= 2000) {
    digitalWrite(GREEN, LOW);
    digitalWrite(YELLOW, LOW);
    digitalWrite(RED, HIGH);
  }

  /* ---Algae growth and efficiency calculation---*/ 
  //  float light_intensity = myLux.getLux();
  //  Serial.print("light_intensity: ");
  //  Serial.println(light_intensity, 1);
  // if (scd30.dataAvailable()) {
  //   float co2_ppm = scd30.getCO2();
  //   float temperature = scd30.getTemperature();
  //   float humidity = scd30.getHumidity();

  //   // Optimal growth conditions
  //   float light_intensity = 60000; // lux
  //   float nutrient_concentration = 0.1; // g/L
  //   float density = 40.0; // g/L

  //   // Air flow rate
  //   float air_flow_rate = 5.0; // L/min

  //   // Calculate the Spirulina growth parameters
  //   float co2_concentration = co2_ppm / 1000.0; // Convert CO2 concentration from ppm to %
  //   float light_energy = light_intensity * 0.0079; // Convert light intensity from lux to µmol/m²/s
  //   float nutrient_mass = nutrient_concentration * density; // Calculate the mass of nutrients available to the algae

  //   // Calculate the specific growth rate using the Monod equation
  //   float k = 0.1; // Half-saturation constant for CO2 uptake rate (in %)
  //   float mu_max = 0.5; // Maximum specific growth rate (in 1/day)
  //   float mu = mu_max * (co2_concentration / (co2_concentration + k));
  
  //   // Calculate the biomass productivity
  //   float biomass_density_change = mu * density;
  //   float biomass_production = biomass_density_change * nutrient_mass;

  //   // Calculate the CO2 biofixation rate
  //   float co2_biofixation = biomass_production * (44.0/12.0);
  //   float co2_biofixation_rate = co2_biofixation / nutrient_mass;

  //   // Calculate the CO2 conversion efficiency
  //   float co2_collected_per_hour = (co2_ppm / 1000.0) * air_flow_rate * 60.0; // Convert ppm to %, multiply by air flow rate and time (60 min)
  //   float biomass_production_per_hour = biomass_production * density * 60.0; // Convert biomass production to g/L/hour
  //   float co2_conversion_efficiency = biomass_production_per_hour / co2_collected_per_hour;
  // }



}


