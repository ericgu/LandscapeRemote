#include <Arduino.h>
#include "Wifi.h"

#include <WebServer.h>
#include <WiFiManager.h>
#include <HTTPClient.h>

/*
Deep Sleep with Touch Wake Up
=====================================
This code displays how to use deep sleep with
a touch as a wake up source and how to store data in
RTC memory to use it over reboots

This code is under Public Domain License.

Author:
Pranav Cherukupalli <cherukupallip@gmail.com>
*/

#define Threshold 40 /* Greater the value, more the sensitivity */
const int LED = 2;
const int PowerLed = 17;
const int PowerControl = 16;
const int SleepTimeoutInitial = 20; // 2 seconds

int awakeCount = 0;
int sleepTimeout;

RTC_DATA_ATTR int bootCount = 0;
touch_pad_t touchPin;

// NOTE: Out of order T8 and T9 to fix mismatch between the pin reported by esp_sleep_get_touchpad_wakeup_status and
// the value from touchRead. It looks like those pins are swapped...

int touchButtons[8] = {T0, T3, T4, T5, T6, T7, T9, T8};
int buttonState[8];

const char *pAddress = "192.168.1.200";

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

/*
Method to print the touchpad by which ESP32
has been awaken from sleep
*/
int getWakeupTouchpad()
{
  touchPin = esp_sleep_get_touchpad_wakeup_status();

  switch (touchPin)
  {
  case 0:
    Serial.println("Touch detected on GPIO 4");
    break;
  case 1:
    Serial.println("Touch detected on GPIO 0");
    break;
  case 2:
    Serial.println("Touch detected on GPIO 2");
    break;
  case 3:
    Serial.println("Touch detected on GPIO 15");
    break;
  case 4:
    Serial.println("Touch detected on GPIO 13");
    break;
  case 5:
    Serial.println("Touch detected on GPIO 12");
    break;
  case 6:
    Serial.println("Touch detected on GPIO 14");
    break;
  case 7:
    Serial.println("Touch detected on GPIO 27");
    break;
  case 8:
    Serial.println("Touch detected on GPIO 33");
    break;
  case 9:
    Serial.println("Touch detected on GPIO 32");
    break;
  default:
    Serial.println("Wakeup not by touchpad");
    return -1;
  }

  int number = touchPin;

  if (number > 0)
  {
    number -= 2;
    ; // we skip pins 1 and 2...
  }

  Serial.println(number);

  return number;
}

int convertBrightnessToNumber(int brightness)
{
  for (int i = 0; i < 5; i++)
  {
    if (brightness < i * 200 + 100)
    {
      return i;
    }
  }

  return 5;
}

int getUmbrellaBrightness()
{
  char command[128];

  sprintf(command, "http://%s/", pAddress);

  HTTPClient http;

  http.begin(command);       //Specify the URL
  int httpCode = http.GET(); //Make the request

  if (httpCode > 0)
  { //Check for the returning code

    String payload = http.getString();
    //Serial.println(httpCode);
    //Serial.println(payload);

    const char *pResult = payload.c_str();

    const char *pUmbrella = strstr(pResult, "umbrella");
    //Serial.println(pUmbrella);

    const char *pValue = strstr(pUmbrella, "=> ") + 3;
    //Serial.println(pValue);

    int umbrellaBrightness = atoi(pValue);
    //Serial.println(umbrellaBrightness);

    return convertBrightnessToNumber(umbrellaBrightness);
  }

  else
  {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources

  return 0;
}

void executeCommand(char *pCommand)
{
  HTTPClient http;

  http.begin(pCommand);      //Specify the URL
  int httpCode = http.GET(); //Make the request

  if (httpCode > 0)
  { //Check for the returning code

    String payload = http.getString();
    Serial.println(httpCode);
    Serial.println(payload);
  }

  else
  {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources
}

void dimUmbrella(int delta)
{
  int umbrellaBrightness = getUmbrellaBrightness();

  umbrellaBrightness += delta;

  if (umbrellaBrightness < 1)
  {
    umbrellaBrightness = 1;
  }
  else if (umbrellaBrightness > 5)
  {
    umbrellaBrightness = 5;
  }

  int percent = umbrellaBrightness * 20;

  char command[128];
  sprintf(command, "http://%s/command?r=umbrella&a=dim%d", pAddress, percent);

  Serial.println(command);
  executeCommand(command);
}

void executeCommandNumber(int commandNumber)
{
  Serial.print("Command: ");
  Serial.println(commandNumber);

  char command[128];

  char device[10];
  char action[10];

  switch (commandNumber)
  {
  case 2:
    strcpy(device, "pump");
    strcpy(action, "toggle");
    break;

  case 3:
    strcpy(device, "bed");
    strcpy(action, "toggle");
    break;

  case 6:
    strcpy(device, "house");
    strcpy(action, "toggle");
    break;

  case 4:
    strcpy(device, "umbrella");
    strcpy(action, "toggle");
    break;

  case 7:
    // umbrella up

    dimUmbrella(1);
    return;

  case 1:
    // umbrella down

    dimUmbrella(-1);
    return;

  case 5:
    strcpy(device, "lights");
    strcpy(action, "toggle");
    break;
  }

  sprintf(command, "http://%s/command?r=%s&a=%s", pAddress, device, action);

  Serial.println(command);
  executeCommand(command);
}

/*
http://192.168.1.200/command?r=pump&a=toggle
http://192.168.1.200/command?r=bed&a=toggle
http://192.168.1.200/command?r=house&a=toggle
http://192.168.1.200/command?r=umbrella&a=toggle

http://192.168.1.200/command?r=umbrella&a=dim60

http://192.168.1.200/command?r=lights&a=on
http://192.168.1.200/command?r=lights&a=off

*/

void callback()
{
}

void setup()
{
  //pinMode(PowerControl, OUTPUT);
  //digitalWrite(PowerControl, HIGH);
  pinMode(PowerLed, OUTPUT);
  digitalWrite(PowerLed, HIGH); // off
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 1);

  Serial.begin(115200);

  WiFiManager wifiManager;

  wifiManager.autoConnect("SequenceController", "12345678");
  digitalWrite(PowerLed, LOW);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  for (int i = 0; i < 8; i++)
  {
    buttonState[i] = 0;
  }

  //Print the wakeup reason for ESP32 and touchpad too
  //print_wakeup_reason();
  int wakeupButton = getWakeupTouchpad();
  if (wakeupButton != -1)
  {
    Serial.print("Execute startup command: ");
    Serial.println(wakeupButton);
    executeCommandNumber(wakeupButton);
    buttonState[wakeupButton] = 4; // keep it from re-executing on hold...
  }

  awakeCount = 0;
  sleepTimeout = SleepTimeoutInitial;
}

void doSleep()
{
  return;

  digitalWrite(PowerLed, LOW);
  //digitalWrite(PowerControl, LOW);

  Serial.println("Going to sleep now");

  for (int i = 0; i < 8; i++)
  {
    touchAttachInterrupt(touchButtons[i], callback, Threshold);
  }

  esp_sleep_enable_touchpad_wakeup();

  esp_deep_sleep_start();
}

void loop()
{
  //digitalWrite(LED, (awakeCount / 5) % 2);

  //Serial.println(sleepTimeout);
  //delay(100);

  sleepTimeout--;
  if (sleepTimeout == 0)
  {
    //doSleep();
  }

  awakeCount++;

  //Serial.println("");
  for (int i = 0; i < 8; i++)
  {
    int value = touchRead(touchButtons[i]);

    if (value < Threshold)
    {
      buttonState[i]++;
    }
    else
    {
      if (buttonState[i] != 0)
      {
        buttonState[i] = -1;
      }
    }
    //Serial.print(value);
    //Serial.print("  ");
  }

  for (int i = 0; i < 8; i++)
  {
    if (buttonState[i] == 3)
    {
      Serial.print("On:  ");
      Serial.println(i);
      executeCommandNumber(i);
      sleepTimeout = SleepTimeoutInitial;
    }

    if (buttonState[i] == -1)
    {
      //Serial.print("Off: " );
      //Serial.println(i);

      buttonState[i] = 0;
    }
  }

  //Serial.println(touchRead(2));  // get value of Touch 0 pin = GPIO 4
}
