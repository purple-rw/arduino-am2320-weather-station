/* 
 * Weather Station using:
 *   . Wemos S2 mini controller
 *   . AM2320 sensor
 *   . SSD1306 128x32 display
 *   . 2 push buttons
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoSort.h>
#include "AM2320_fast.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// constants -- pin numbers
const int buttonPin[2] = {37, 39};  // pin number for upper/lower push button
const int ledPin = LED_BUILTIN;     // the number of the LED pin

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library.
// On an arduino UNO:       A4(SDA), A5(SCL)
// On ESP32-S2 mini:        33(SDA), 35(SCL)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Create an AM2320 instance
AM2320_fast am2320;

// variables
int button[2] = {0, 0};                     // number of pressed that needs to be processed
char prevButtonState[2] = {0, 0};           // previous state of push buttons
unsigned long lastButtonPress[2] = {0, 0};
unsigned long lastButtonRepeat[2] = {0, 0};
unsigned long lastUpdateData = 0;           // last data was updated
unsigned long lastSaveData = 0;
unsigned long lastUpdateDisplay = 0;
unsigned long lastAutoChangeMode = 0;
int displayMode = 0;                        // 0 = Show text
int prevDisplayMode = -1;
int cursor;                                 // cursor x position in Mode 3 or 4
char blink = 0;                             // cursor blink
int dataCount = 0;                          // saved data count

// saved data
struct Range {
  int max;
  int avg;
  int min;
};
struct PastData {
  struct Range temperature;
  struct Range humidity;
};

const int pastDataMAX      = SCREEN_WIDTH;
const int recentDataMAX    = 24;
const int measureInterval  = 30000;         // in ms; minimum 2s
const int saveDataInterval = 30000;         // in ms

const int buttonRepeatThreshold = 500;      // in ms
const int buttonRepeatInterval  = 75;       // in ms

struct PastData pastData[pastDataMAX];
int temperature[recentDataMAX];
int humidity[recentDataMAX];
int ptrPast = 0;
int ptrRecent = 0;

void update_data() {
  digitalWrite(ledPin, HIGH);
  if (am2320.update()) {            // if error
    am2320.temperature = -1000;     // data out of range = no data available
    am2320.humidity = -1000;        // data out of range = no data available
  }
  lastUpdateData = millis();
  digitalWrite(ledPin, LOW);
}

struct Range data_range(int *array, int size) {
  struct Range rtn;

  int min = -1;   // index to min
  long sum = 0;    // for calc
  int count = 0;
  for (int i=0; i<size; i++) {
    // find max
    if (array[i] > rtn.max || !i)
      rtn.max = array[i];

    // next value if invalid
    if (array[i] <= -1000)
      continue;

    // find min
    if (min < 0 || array[i] < array[min])
      min = i;

    // calc mean
    sum += array[i];
    count++;
  }
  rtn.min = (min < 0) ? -1000 : array[min];   // data out of range = no data available
  if (count & 1) {
    rtn.avg = ((sum<<1) + count) / (count<<1);
  } else {
    rtn.avg = (sum + (count>>1)) / count;
  }

  // find min
  return rtn;
}

void save_data() {
  temperature[ptrRecent] = am2320.temperature;
  humidity[ptrRecent] = am2320.humidity;

  // if recentData buffer is full
  if (++ptrRecent >= recentDataMAX) {
    // reset Recent pointer
    ptrRecent = 0;

    // calculate max/min/avg from recentData buffer
    pastData[ptrPast].temperature = data_range(temperature, recentDataMAX);
    pastData[ptrPast].humidity = data_range(humidity, recentDataMAX);

    // wrap around Past pointer
    if (++ptrPast >= pastDataMAX)
      ptrPast = 0;

    // update count
    dataCount = min(pastDataMAX, dataCount+1);
  }

  lastSaveData = millis();
}

// mode 0
void display_temp_and_humi(unsigned long time, char force=0) {
  if (!force && time - lastUpdateDisplay < 500)
    return;

  display.clearDisplay();
  display.setTextSize(2);                 // large 2:1 pixel scale
  display.setTextColor(SSD1306_WHITE);    // draw white text

  display.setCursor(0, 0);
  display.print(F("Temp: "));
  int t = am2320.temperature;
  if (t < -40) {
    display.print(F("err"));
  } else {
    display.print((t * 9 + 25) / 50 + 32);    // conv to F
    display.print(F("F"));
  }

  display.setCursor(0, 16);
  display.print(F("RH:   "));
  int rh = am2320.humidity;
  if (rh < 0) {
    display.print(F("err"));
  } else {
    display.print((rh + 5) / 10);             // round up
    display.print(F("%"));
  }

  display.display();
  lastUpdateDisplay = time;
}

// mode 1
void display_temp_graph(unsigned long time, char force=0) {
  if (!force && time - lastUpdateDisplay < 500)
    return;

  display.clearDisplay();
  for (int x=0; x<pastDataMAX; x++) {
    int i = (ptrPast + x) % pastDataMAX;
    int Fmax = (pastData[i].temperature.max * 9 + 25) / 50 + 32;    // conv to F
    int Fmin = (pastData[i].temperature.min * 9 + 25) / 50 + 32;    // conv to F
    int Favg = (pastData[i].temperature.avg * 9 + 25) / 50 + 32;    // conv to F
    int ymax = ((SCREEN_HEIGHT - 1) >> 1) - Fmax + 68;              // 68F is the average/center
    int ymin = ((SCREEN_HEIGHT - 1) >> 1) - Fmin + 68;              // 68F is the average/center
    int yavg = ((SCREEN_HEIGHT - 1) >> 1) - Favg + 68;              // 68F is the average/center

    if (ymax >= 0 && ymax < SCREEN_HEIGHT)
      display.drawPixel(x, ymax, SSD1306_WHITE);
    if (ymin >= 0 && ymin < SCREEN_HEIGHT && ymin != ymax)
      display.drawPixel(x, ymin, SSD1306_WHITE);
    if (yavg >= 0 && yavg < SCREEN_HEIGHT && yavg != ymax && yavg != ymin)
      display.drawPixel(x, yavg, SSD1306_WHITE);
  }

  // show current temperature
  display.setTextSize(1);                 // small 1:1 pixel scale
  display.setTextColor(SSD1306_INVERSE);  // draw inverse text

  display.setCursor(0, 0);
  int t = am2320.temperature;
  if (t < -400) {
    display.print(F("err"));
  } else {
    display.print((t * 9 + 25) / 50 + 32);   // conv to F
    display.print(F("F"));
  }

  display.display();
  lastUpdateDisplay = time;
}

// mode 2
void display_humi_graph(unsigned long time, char force=0) {
  if (!force && time - lastUpdateDisplay < 500)
    return;

  display.clearDisplay();
  for (int x=0; x<pastDataMAX; x++) {
    int i = (ptrPast + x) % pastDataMAX;
    int ymax = SCREEN_HEIGHT - (pastData[i].humidity.max * SCREEN_HEIGHT) / 1001 - 1;
    int ymin = SCREEN_HEIGHT - (pastData[i].humidity.min * SCREEN_HEIGHT) / 1001 - 1;
    int yavg = SCREEN_HEIGHT - (pastData[i].humidity.avg * SCREEN_HEIGHT) / 1001 - 1;

    if (ymax >= 0 && ymax < SCREEN_HEIGHT)
      display.drawPixel(x, ymax, SSD1306_WHITE);
    if (ymin >= 0 && ymin < SCREEN_HEIGHT && ymin != ymax)
      display.drawPixel(x, ymin, SSD1306_WHITE);
    if (yavg >= 0 && yavg < SCREEN_HEIGHT && yavg != ymax && yavg != ymin)
      display.drawPixel(x, yavg, SSD1306_WHITE);
  }

  // show current humidity
  display.setTextSize(1);                 // small 1:1 pixel scale
  display.setTextColor(SSD1306_INVERSE);  // draw inverse text

  display.setCursor(0, 0);
  int rh = am2320.humidity;
  if (rh < 0) {
    display.print(F("err"));
  } else {
    display.print((rh + 5) / 10);         // round up
    display.print(F("%"));
  }

  display.display();
  lastUpdateDisplay = time;
}

// mode 3
void display_temp_data(unsigned long time, char force=0) {
  int prevCursor = cursor;
  char prevBlink = blink;
  if (button[0] > 0 || button[1] > 0) {
    cursor = constrain(cursor - button[0] + button[1], SCREEN_WIDTH - dataCount, SCREEN_WIDTH - 1);
    button[0] = 0;
    button[1] = 0;
  }

  // blink twice every second, half duty cycle
  if (time - lastUpdateDisplay > 250)
    blink ^= 1;

  // return if no change
  if (!force && prevCursor == cursor && prevBlink == blink)
    return;
    
  display.clearDisplay();

  for (int x=0; x<pastDataMAX; x++) {
    int i = (ptrPast + x) % pastDataMAX;
    int Fmax = (pastData[i].temperature.max * 9 + 25) / 50 + 32;    // conv to F
    int Fmin = (pastData[i].temperature.min * 9 + 25) / 50 + 32;    // conv to F
    int Favg = (pastData[i].temperature.avg * 9 + 25) / 50 + 32;    // conv to F
    int ymax = ((SCREEN_HEIGHT - 1) >> 1) - Fmax + 68;              // 68F is the average/center
    int ymin = ((SCREEN_HEIGHT - 1) >> 1) - Fmin + 68;              // 68F is the average/center
    int yavg = ((SCREEN_HEIGHT - 1) >> 1) - Favg + 68;              // 68F is the average/center

    if (blink || x != cursor) {
      if (ymax >= 0 && ymax < SCREEN_HEIGHT)
         display.drawPixel(x, ymax, SSD1306_WHITE);
      if (ymin >= 0 && ymin < SCREEN_HEIGHT && ymin != ymax)
        display.drawPixel(x, ymin, SSD1306_WHITE);
      if (yavg >= 0 && yavg < SCREEN_HEIGHT && yavg != ymax && yavg != ymin)
        display.drawPixel(x, yavg, SSD1306_WHITE);
    }
  }

  // show temperature @ cursor
  display.setTextSize(1);                 // small 1:1 pixel scale
  display.setTextColor(SSD1306_INVERSE);  // draw inverse text

  display.setCursor(0, 0);
  int i = (ptrPast + cursor) % pastDataMAX;
  int t = pastData[i].temperature.avg;
  if (t < -400) {
    display.print(F("err"));
  } else {
    display.print((t * 9 + 25) / 50 + 32);   // conv to F
    display.print(F("F"));
  }

  // show cursor time
  display.setCursor(0, 24);
  unsigned long elapsed = ((unsigned long)(pastDataMAX - cursor) * saveDataInterval * recentDataMAX + 500) / 1000;
  int sec = elapsed % 60;
  int min = elapsed / 60;
  int hr  = min / 60;
  min %= 60;

  display.print(F("T-"));
  if (hr > 0) {
    display.printf("%dh", hr);;
    if (min > 0 || sec > 0)
      display.printf("%02dm", min);
    if (sec > 0)
      display.printf("%02ds", sec);
  } else if (min > 0) {
    display.printf("%dm", min);
    if (sec > 0)
      display.printf("%02ds", sec);
  } else {
    display.printf("%ds", sec);
  }

  display.display();
  lastUpdateDisplay = time;
}

// mode 4
void display_humi_data(unsigned long time, char force=0) {
  int prevCursor = cursor;
  char prevBlink = blink;
  if (button[0] > 0 || button[1] > 0) {
    cursor = constrain(cursor - button[0] + button[1], SCREEN_WIDTH - dataCount, SCREEN_WIDTH - 1);
    button[0] = 0;
    button[1] = 0;
  }

  // blink twice every second, half duty cycle
  if (time - lastUpdateDisplay > 250)
    blink ^= 1;

  // return if no change
  if (!force && prevCursor == cursor && prevBlink == blink)
    return;
    
  display.clearDisplay();

  for (int x=0; x<pastDataMAX; x++) {
    int i = (ptrPast + x) % pastDataMAX;
    int ymax = SCREEN_HEIGHT - 1 - pastData[i].humidity.max * SCREEN_HEIGHT / 1001;
    int ymin = SCREEN_HEIGHT - 1 - pastData[i].humidity.min * SCREEN_HEIGHT / 1001;
    int yavg = SCREEN_HEIGHT - 1 - pastData[i].humidity.avg * SCREEN_HEIGHT / 1001;

    if (blink || x != cursor) {
      if (ymax >= 0 && ymax < SCREEN_HEIGHT)
         display.drawPixel(x, ymax, SSD1306_WHITE);
      if (ymin >= 0 && ymin < SCREEN_HEIGHT && ymin != ymax)
        display.drawPixel(x, ymin, SSD1306_WHITE);
      if (yavg >= 0 && yavg < SCREEN_HEIGHT && yavg != ymax && yavg != ymin)
        display.drawPixel(x, yavg, SSD1306_WHITE);
    }
  }

  // show humidity @ cursor
  display.setTextSize(1);                 // small 1:1 pixel scale
  display.setTextColor(SSD1306_INVERSE);  // draw inverse text

  display.setCursor(0, 0);
  int i = (ptrPast + cursor) % pastDataMAX;
  int rh = pastData[i].humidity.avg;
  if (rh < 0) {
    display.print(F("err"));
  } else {
    display.print((rh + 5) / 10);         // round up
    display.print(F("%"));
  }

  // show cursor time
  display.setCursor(0, 24);
  unsigned long elapsed = ((unsigned long)(pastDataMAX - cursor) * saveDataInterval * recentDataMAX + 500) / 1000;
  int sec = elapsed % 60;
  int min = elapsed / 60;
  int hr  = min / 60;
  min %= 60;

  display.print(F("T-"));
  if (hr > 0) {
    display.printf("%dh", hr);;
    if (min > 0 || sec > 0)
      display.printf("%02dm", min);
    if (sec > 0)
      display.printf("%02ds", sec);
  } else if (min > 0) {
    display.printf("%dm", min);
    if (sec > 0)
      display.printf("%02ds", sec);
  } else {
    display.printf("%ds", sec);
  }

  display.display();
  lastUpdateDisplay = time;
}

// mode 99
void display_temp_graph_debug(unsigned long time) {
  display.clearDisplay();

  for (int x=0; x<recentDataMAX; x++) {
    int F = (temperature[x] * 9 + 25) / 50 + 32;    // conv to F
    int y = ((SCREEN_HEIGHT - 1) >> 1) - F + 68;    // 68F is the average/center

    if (y >= 0 && y < SCREEN_HEIGHT)
      display.drawPixel(x, y, SSD1306_WHITE);
  }

  display.display();
  lastUpdateDisplay = time;
}


void setup() {
  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  // initialize the pushbutton pins as an input:
  pinMode(buttonPin[0], INPUT);
  pinMode(buttonPin[1], INPUT);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();
  display.display();

  // Clear past data
  for (int i=0; i<pastDataMAX; i++) {
    pastData[i].temperature.max = -1000;
    pastData[i].temperature.min = -1000;
    pastData[i].temperature.avg = -1000;
    pastData[i].humidity.max = -1000;
    pastData[i].humidity.min = -1000;
    pastData[i].humidity.avg = -1000;
  }

  // Clear recent data
  for (int i=0; i<recentDataMAX; i++) {
    temperature[i] = -1000;
    humidity[i] = -1000;
  }

  //Wire.begin();
  delay(400);       // make sure first update_data is good.
  update_data();
}

void loop() { 
  // current time
  unsigned long time = millis();

  // read the state of the push buttons:
  char buttonState[2];
  for (int i=0; i<2; i++) {
    buttonState[i] = digitalRead(buttonPin[i]);
    if (prevButtonState[i] != buttonState[i]) {   // if button pressed or released
      prevButtonState[i] = buttonState[i];
      lastButtonPress[i] = time;
      lastButtonRepeat[i] = 0;                    // 0 means not repeating
      if (!buttonState[i]) {                      // 0 (LOW) = button pressed
        switch (displayMode) {
          case 0: // fall thru
          case 1: if (dataCount > 0) {
                    displayMode = 3;
                    cursor = SCREEN_WIDTH - 1;
                  }
                  break;
          case 2: if (dataCount > 0) {
                    displayMode = 4;
                    cursor = SCREEN_WIDTH - 1;
                  }
                  break;
          case 3: // fall thru
          case 4: button[i]++;                     // process a button press
                  break;
        }
      }
    } else if (!buttonState[i] && buttonState[1-i] && (time - lastButtonPress[i] > buttonRepeatThreshold)) {
      if (!lastButtonRepeat[i])
        lastButtonRepeat[i] = time;
      else if (time - lastButtonRepeat[i] > buttonRepeatInterval) {
        lastButtonRepeat[i] = time;
        button[i]++;
      }
    }
  }

  // update temp/humi every 5 sec, or 2 sec if data contains error
  if (time - lastUpdateData > measureInterval)
    update_data();

  if (time - lastSaveData > saveDataInterval)
    save_data();

  // update display
  char force = (prevDisplayMode != displayMode);
  switch (displayMode) {
    case 0:
      display_temp_and_humi(time, force);
      prevDisplayMode = 0;
      if (time - lastAutoChangeMode > 3000) {
        displayMode = 1;
        lastAutoChangeMode = time;
      }
      break;
    case 1:
      display_temp_graph(time, force);
      prevDisplayMode = 1;
      if (time - lastAutoChangeMode > 3000) {
        displayMode = 2;
        lastAutoChangeMode = time;
      }
      break;
    case 2:
      display_humi_graph(time, force);
      prevDisplayMode = 2;
      if (time - lastAutoChangeMode > 3000) {
        displayMode = 0;
        lastAutoChangeMode = time;
      }
      break;
    case 3:
      display_temp_data(time, force);
      prevDisplayMode = 3;
      if (time - lastButtonPress[0] > 8000 && time - lastButtonPress[1] > 8000) {
        displayMode = 0;
        lastAutoChangeMode = time;
      }
      break;
    case 4:
      display_humi_data(time, force);
      prevDisplayMode = 4;
      if (time - lastButtonPress[0] > 8000 && time - lastButtonPress[1] > 8000) {
        displayMode = 0;
        lastAutoChangeMode = time;
      }
      break;
  }
}