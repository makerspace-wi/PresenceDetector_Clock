/* DESCRIPTION
  ====================
 * blinkenclock - multiprupose LED wall clock
 * version 0.1 alpha
 * Copyright by Bjoern Knorr 2013
 *
 * http://netaddict.de/blinkenlights:blinkenclock
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * credits to
 *   Adafruit (NeoPixel Library)
 *
 * 07 Nov 2013 - initial release
 *
 -Parameter of our system-------------:
  Commands to Raspi
  'SENSOR'  - from xBee (=Ident)
  'POR'     - machine power on reset (Ident;por)

  Commands from Raspi
  'time'    - format time2018,09,12,12,22,00
  'cm'      - set to Clock Mode
  'cc'      - set to Color (rgb) Cycle
  'bm'      - set to Bouncing Mode (shows bouncing colors)
  'am'      - set to Ambient Mode (clock shows defined color)
  'ga'      - set to Green Alert (clock flashes orange) - alert mode
  'oa'      - set to Orange Alert (clock flashes orange) - alert mode
  'co'      - set Clock Option X minute dots

  last change: 20.01.2019 by Michael Muehl
  changed: adapt to xBee control and add precenece detection with tasker, set names for xBee, delete I2C
           changed debounce for signal change or set time for next signal check.
*/
#define Version "1.2" // (Test =1.x ==> 1.3)

#include <Arduino.h>
#include <TaskScheduler.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Time.h>
#include <RTClib.h>

// PIN Assignments
// Button --------------
#define BUTTON_PIN1   2 // Button for mode
#define BUTTON_PIN2   5 // Button for MOVE
#define BUTTON_PIN3   7 // Button for MOVE
#define BUTTON_PIN4   9 // Button for MOVE
// LedStrip Data in ----
#define DATA_PIN      6 //data pin from stripe
// Brightness sensor
#define LDR_READ_PIN A0 //PWR ---|VT93N1|---+---|10k|-- GND
#define LDR_PWR_PIN  A1 //        ______  READ   ___

#define BUSError      8 // Bus error
#define xbeError     13 // Bus error

// DEFINES
#define clockTime     500 // ms Clock Abfragezeit
#define porTime         5 // wait seconds for sending Ident + POR
#define SECONDS      1000 // multiplier for second
#define PIXEL_12OCLOCK 30 // first pixel (12 o'clock)
#define moveTime  20 * SECONDS // signal lenght after recognizing move

// CREATE OBJECTS
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, DATA_PIN, NEO_GRB + NEO_KHZ800);

Scheduler runner;
RTC_DS1307 RTC;  //Create the DS1307 object

// Callback methods prototypes
void checkXbee();        // Task connect to xBee Server
void pixelCallback();    // Task for clock time
void lightCallback();    // Task for switch light on
void debounceCallback(); // Task to let LED blink - added by D. Haude 08.03.2017

// TASKS
Task tP(clockTime/10, TASK_FOREVER, &pixelCallback); // main task default clock mode
Task tL(clockTime, TASK_FOREVER, &checkXbee);        // task for check xBee
Task tD(5000, TASK_FOREVER, &retryPOR);              // task for debounce; added M. Muehl
Task tR(1, TASK_ONCE, &sendREADY);                   // task set bit ready2send after wait; added M. Muehl

// VARIABLES
typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} COLOR;

typedef struct
{
  uint8_t magic; //0xAA
  uint8_t clock_mode;
  COLOR hour;
  COLOR minute;
  COLOR second;
  COLOR off;
  uint8_t brightness_min;
  uint8_t brightness_max;
  uint16_t analog_min;
  uint16_t analog_max;
} SETTINGS;

SETTINGS settings;

// default mode is clock mode
uint8_t mode = 0;

// alert mode is per default green
uint8_t alert = 0;

// clock option show five minute dots
uint8_t coptionfivemin = 1;

// clock option fade seconds
boolean coptionfade = 1;

// multiprupose counter
int counter = 0;

// alert counter
int alertcounter = 0;

// redraw flag
boolean redraw = 1;

// Senor PresenceDetecctor Pin 2
byte sensorInterrupt = 0; // 0 = digital pin 2

// time cache
unsigned long currenttime = 0;
unsigned long lasttime = 0;
unsigned long alerttime = 0;

// last second
uint8_t lastsecond = 0;

// strip color (ambient)
uint32_t color_ambient;

byte getTime = porTime;
byte inPin = 0;
// Serial
String inStr = "";  // a string to hold incoming data
String IDENT = "";  // Machine identifier for remote access control
byte plplpl = 0;    // send +++ control AT sequenz

boolean ready2send = HIGH;  // ready to send commands = HIGH; added MM

// ======>  SET UP AREA <=====
void setup() {
  //init Serial port
  Serial.begin(57600);  // Serial
  inStr.reserve(40);    // reserve for instr serial input
  IDENT.reserve(5);     // reserve for instr serial input

  //init strip
  strip.begin();
  strip.show();

  //init RTC
  RTC.begin();

  // PIN MODES
  pinMode(BUSError, OUTPUT);
  pinMode(LDR_PWR_PIN, OUTPUT); //init LDR
//  pinMode(LDR_READ_PIN, INPUT); //init LDR

  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);
  pinMode(BUTTON_PIN4, INPUT_PULLUP);

  // Set default values
  digitalWrite(BUSError, HIGH);	// turn the LED ON (init start)
  digitalWrite(xbeError, HIGH);	// turn the LED ON (init start)

  digitalWrite(LDR_PWR_PIN, HIGH);

  runner.init();
  runner.addTask(tP);
  runner.addTask(tL);
  runner.addTask(tD);
  runner.addTask(tR);

  lasttime = millis();
  currenttime = millis();
  lastsecond = 0;
  color_ambient = strip.Color(0, 160, 230);

  Serial.print("+++"); //Starting the request of IDENT
  tP.enable();
  tL.enable();
}

// FUNCTIONS (Tasks) ----------------------------
void checkXbee() {
  if (IDENT.startsWith("SENSOR") && plplpl == 2) {
    ++plplpl;
    tD.enable();
    digitalWrite(xbeError, LOW); // turn the LED off (Programm start)
  }
}

void retryPOR() {
  if (getTime < porTime * 5) {
    Serial.println(String(IDENT) + ";POR");
    ++getTime;
    tD.setInterval(getTime * SECONDS);
  }
  else if (getTime == 255)
  {
    tL.setCallback(lightCallback);
    // set tD to debounce
    tD.setCallback(debounceCallback);
    tD.setInterval(clockTime/10);
    tD.disable();
    digitalWrite(BUSError, LOW); // turn the LED off (Programm start)
  }
}

void lightCallback() {
  if (digitalRead(BUTTON_PIN1) == LOW) {
    inPin = 1;
    tD.enable();
  }
  if (digitalRead(BUTTON_PIN2) == LOW) {
    inPin = 2;
    tD.enable();
  }
  if (digitalRead(BUTTON_PIN3) == LOW) {
    inPin = 3;
    tD.enable();
  }
  if (digitalRead(BUTTON_PIN4) == LOW) {
    inPin = 4;
    tD.enable();
  }
}

void debounceCallback() {
  tD.disable();
  if (inPin == 1 && digitalRead(BUTTON_PIN1) == LOW) {
    ++mode;
    if (mode > 3) mode = 0;
  }

  if (ready2send) {
    if (inPin == 2 && digitalRead(BUTTON_PIN2) == LOW) {
      ready2send = LOW;
      tR.restartDelayed(moveTime);
      presence();
    }
    if (inPin == 3 && digitalRead(BUTTON_PIN3) == LOW) {
      ready2send = LOW;
      tR.restartDelayed(moveTime);
      presence();
    }
    if (inPin == 4 && digitalRead(BUTTON_PIN4) == LOW) {
      ready2send = LOW;
      tR.restartDelayed(moveTime);
      presence();
    }
  }
  inPin = 0;
}

void pixelCallback() {   // Pixel 50ms Tick
  if (mode == 0) {      // 45ms  clock mode - show time
    clockMode();
    redraw = 1;
  } else if (mode==1) { // 50ms  demo mode - show rgb cycle
    // reset counter
    if (counter >= 256) {
      counter = 0;
    }
    for(uint16_t i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + counter) & 255));
    }
    redraw = 1;
    counter++;
  } else if (mode==2) { // 5ms music mode
    int sensorvalue = map(analogRead(LDR_READ_PIN), 990, 300, 0, 200);
    if (sensorvalue<0) {
      sensorvalue = 0;
    }
    lightPixels(strip.Color(1*sensorvalue, 1*sensorvalue, 1*sensorvalue));
    redraw = 1;
    lasttime = currenttime;
  } else if (mode==3) { // -----------------------
    lightPixels(color_ambient);
    redraw = 1;
  }

  // alert - overrides everything
  if (alert && (currenttime - alerttime > 20)) {
    if (alertcounter > 59) {
      alertcounter = 0;
    }
    alertcounter++;
    redraw = 1;
    alerttime = millis();
  }
  if (alert==1) {
    drawCycle(alertcounter, strip.Color(25, 20, 0));
  }
  if (alert==2) {
    drawCycle(alertcounter, strip.Color(25, 0, 0));
  }
  if (redraw == 1) {
    strip.show();
    redraw =0;
  }
}
// END OF TASKS ---------------------------------

// FUNCTIONS ------------------------------------
// clock mode
void clockMode() {
  DateTime now = RTC.now();
  uint8_t analoghour = now.hour();
  uint8_t currentsecond = now.second();

  if (analoghour > 12) {
    analoghour=(analoghour-12);
  }
  analoghour = analoghour*5+(now.minute()/12);

  lightPixels(strip.Color(2, 2, 2));

  if (coptionfivemin) {
    for (uint8_t i=0; i<60; i += 5) {
      strip.setPixelColor(i,strip.Color(10, 10, 10));
    }
  }

  strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + analoghour-1),strip.Color(70, 0, 0));
  strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + analoghour),strip.Color(255, 0, 0));
  strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + analoghour+1),strip.Color(70, 0, 0));

  strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + now.minute()),strip.Color(0, 0, 255));

  if (coptionfade) {
    // reset counter
    if(counter>25) {
      counter = 0;
    }
    else if (lastsecond != currentsecond) {
      lastsecond = now.second();
      counter = 0;
    }
    strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + now.second()+1),strip.Color(0, counter*10, 0));
    strip.setPixelColor(pixelCheck(PIXEL_12OCLOCK + now.second()),strip.Color(0, 255-(counter*10), 0));
    counter++;
  }
  else {
    strip.setPixelColor(now.second(),strip.Color(0, 255, 0));
  }
}

// set clock
void setClock(String stringSet) {
  String str;
  int secupg;
  str = stringSet.substring(17, 19);
  secupg = str.toInt();
  int minupg;
  str = stringSet.substring(14, 16);
	minupg = str.toInt();
  int hourupg;
  str = stringSet.substring(11, 13);
	hourupg = str.toInt();
  int dayupg;
  str = stringSet.substring(8, 10);
	dayupg = str.toInt();
  int monthupg;
  str = stringSet.substring(5, 7);
	monthupg = str.toInt();
  int yearupg;
  str = stringSet.substring(0, 4);
	yearupg = str.toInt();
  RTC.adjust(DateTime(yearupg,monthupg,dayupg,hourupg,minupg,secupg));
}

// cycle mode
void drawCycle(int i, uint32_t c) {
  for(uint8_t ii=5; ii>0; ii--) {
    strip.setPixelColor(pixelCheck(i-ii),c);
  }
}

// show a progress bar - assuming that the input-value is based on 100%
void progressBar(int i) {
  map(i, 0, 100, 0, 59);
  lightPixels(strip.Color(0, 0, 0));
  for (uint8_t ii=0; ii<i; ii++) {
    strip.setPixelColor(ii,strip.Color(5, 0, 5));
  }
}

// light all pixels with given values
void lightPixels(uint32_t c) {
  for (uint8_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i,c);
  }
}

// set the correct pixels
int pixelCheck(int i) {
  if (i>59) {
    i = i - 60;
  }
  if (i<0) {
    i = i +60;
  }
  return i;
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  else {
    WheelPos -= 170;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

void sendREADY() {
  ready2send = HIGH;
}

void presence() {
  Serial.println("MOVE");
}
// End Funktions --------------------------------

// Funktions Serial Input (Event) ---------------
void evalSerialData() {
  inStr.toUpperCase();

  if (inStr.startsWith("OK")) {
    if (plplpl == 0) {
      ++plplpl;
      Serial.println("ATNI");
    } else {
      ++plplpl;
    }
  }

  if (inStr.startsWith("SENSOR")) {
    Serial.println("ATCN");
    IDENT = inStr;
  }

  if (inStr.startsWith("TIME")) { // adjust TIME
    setClock(inStr.substring(4));
    tL.setInterval(500);
    getTime = 255;
}

  if (inStr.startsWith("CM")) { // set to Clock Mode
    mode=0;
    tP.setInterval(clockTime/10);
  }

  if (inStr.startsWith("CC")) { // shows color (rgb) cycle
    mode=1;
    tP.setInterval(clockTime/10);
  }

  if (inStr.startsWith("BM")) { //bouncing mode (shows bouncing colors)
    mode=2;
    tP.setInterval(clockTime);
  }

  if (inStr.startsWith("AM")) { //ambient mode (clock shows defined color)
    mode=3;
    tP.setInterval(clockTime/10);
  }
  if (inStr.startsWith("GA")) { //alert mode - Green Alert (clock flashes orange)
    alert = 0;
  }

  if (inStr.startsWith("OA")) { //alert mode - orange alert (clock flashes orange)
    alert = 1;
  }

  if (inStr.startsWith("CO")) { // set Clock Option X minute dots
    if (coptionfivemin) {
      coptionfivemin = 0;
    } else {
      coptionfivemin = 1;
    }
  }
}

/* SerialEvent occurs whenever a new data comes in the
  hardware serial RX.  This routine is run between each
  time loop() runs, so using delay inside loop can delay
  response.  Multiple bytes of data may be available.
*/
void serialEvent() {
  char inChar = (char)Serial.read();
  if (inChar == '\x0d') {
    evalSerialData();
    inStr = "";
  } else {
    inStr += inChar;
  }
}
// End Funktions Serial Input -------------------

// PROGRAM LOOP AREA ----------------------------
void loop() {
  runner.execute();
}
