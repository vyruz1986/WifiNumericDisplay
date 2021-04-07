/*
WifiNumericDisplay - A numeric 4-digit display which can be controlled over WiFi
Copyright (C) 2018  Alex Goris

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <NetworkServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <math.h>
#include <ArduinoJson.h>

/**************************** Wifi Configuration ****************************/
String strHostname;
wl_status_t PrevWifiState;
WiFiManager wifiMan;

//static IP params
//default custom static IP
char static_ip[16] = "";
char static_gw[16] = "";
char static_sn[16] = "";

//flag for saving data
bool shouldSaveConfig = false;

/**************************** Secure Parameters - Do not publish!!!***********/
//OTA firmware upgrade password - Must match the one in board.txt file
#define OTA_PASSWD "EnterUniquePasswordHere!"

//GPIO declarations FOR 7 segment driver board
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
byte segmentClock = D2;
byte segmentLatch = D3;
byte segmentData = D1;
#define NUM_DIGITS 4

//Configure server which listens for incoming messages
WiFiServer ServerPort23(23);
NetworkServer MessageServer;

//Serial comms stuff
bool bInputStringComplete = false;
String strInputData;

//Countdown timer variables
int iCountDownTimer = 0;
int iCountDownCurrentValue = 0;
unsigned long ulCountDownStartTime = 0;

//Alive ping timer
#define ALIVE_PING_INTERVAL 5000
unsigned long ulLastAlivePing = 0;
unsigned long ulLastLEDOnTime = 0;
byte bLEDState = HIGH;
byte bPrevledState = HIGH;

//Reset NW button pin
#define RESET_NW_PIN D8
#define RESET_NW_PRESS_TIME 3000 //Time to press reset button before NW settings will be cleared
unsigned long ulButtonDepressTime = 0;

//Activity LED
#define ACTIVITY_LED_PIN LED_BUILTIN
unsigned long ulLEDLinkInterval = 1000; //every second
unsigned long ulLEDOnTime = 0;

void ResetNetwork();
void HandleNWResetButton();
void HandleActivityLED();
void HandleWifiConfig();
void StopCountDownTimer();
void StartCountDownTimer(unsigned int iSeconds);
void HandleCountDownTimer();
void serialEvent();
void postNumber(byte number, boolean decimal);
void ShowNumber(long lValue, uint8_t iNumDecimals);
void ClearDisplay();
void saveConfigCallback();

void setup()
{
   Serial.begin(74880);
   Serial.println();
   String realSize = String(ESP.getFlashChipRealSize());
   String ideSize = String(ESP.getFlashChipSize());
   bool flashCorrectlyConfigured = realSize.equals(ideSize);
   if (flashCorrectlyConfigured)
      SPIFFS.begin();
   else
      Serial.println("flash incorrectly configured, SPIFFS cannot start, IDE size: " + ideSize + ", real size: " + realSize);

   Serial.println("Starting setup...");
   //configure IO pins
   pinMode(RESET_NW_PIN, INPUT);
   pinMode(ACTIVITY_LED_PIN, OUTPUT);

   pinMode(segmentClock, OUTPUT);
   pinMode(segmentData, OUTPUT);
   pinMode(segmentLatch, OUTPUT);

   digitalWrite(segmentClock, LOW);
   digitalWrite(segmentData, LOW);
   digitalWrite(segmentLatch, LOW);

   //Clear display
   ClearDisplay();

   //Handle config
   Serial.printf("Starting wifi config\r\n");
   HandleWifiConfig();

   strHostname = WiFi.hostname();
   String strLocalIp = WiFi.localIP().toString();
   uint iLastIpPart = strLocalIp.substring(strLocalIp.lastIndexOf('.') + 1).toInt();
   ShowNumber(iLastIpPart, 0);
   Serial.printf("Connected to AP %s, IP: %s, name: %s\r\n", WiFi.SSID().c_str(), strLocalIp.c_str(), strHostname.c_str());

   ServerPort23.begin();
   MessageServer.init(&ServerPort23);

   //OTA Firmware update stuff
   // Port defaults to 8266
   ArduinoOTA.setPort(8266);

   // Hostname defaults to esp8266-[ChipID]
   //ArduinoOTA.setHostname("myesp8266");

   // No authentication by default
   ArduinoOTA.setPassword((const char *)OTA_PASSWD);

   ArduinoOTA.onStart([]() {
      Serial.printf("Receiving new OTA firmware...\r\n");
   });
   ArduinoOTA.onEnd([]() {
      Serial.printf("New firwmare received, rebooting!\r\n");
   });
   ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
   });
   ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error: %u", error);
   });
   ArduinoOTA.begin();

   Serial.printf("Starting!");
}

void loop()
{
   while (millis() < 3000)
   {
      //Don't do anything first 3 seconds so system can settle down a bit
      return;
   }

   //Call main loops
   serialEvent();
   MessageServer.Loop();
   HandleCountDownTimer();
   HandleActivityLED();
   HandleNWResetButton();
   ArduinoOTA.handle();
   if (MessageServer.Available())
   {
      strInputData = MessageServer.GetOldestData();
      bInputStringComplete = true;
   }

   //Check wifi status
   wl_status_t WifiState = WiFi.status();
   if (WifiState != PrevWifiState)
   {
      Serial.printf("Wifi state changed to: %s!\r\n", (WifiState != WL_CONNECTED ? "Connection lost" : "Connected"));
      if (WifiState == WL_CONNECTED)
      {
         //Show (potentially new) IP on display
         String strLocalIp = WiFi.localIP().toString();
         uint iLastIpPart = strLocalIp.substring(strLocalIp.lastIndexOf('.') + 1).toInt();
         ShowNumber(iLastIpPart, 0);
      }
      PrevWifiState = WifiState;
   }

   if (bInputStringComplete)
   {
      Serial.printf("Received data: %s\r\n", strInputData.c_str());

      if (strInputData.indexOf("CD") > -1)
      {
         //We should start a coundown timer
         unsigned int iRequestedCountDownTime = strInputData.substring(2).toInt();
         StartCountDownTimer(iRequestedCountDownTime);
         Serial.printf("Starting countdown for %i seconds...\r\n", iRequestedCountDownTime);
      }
      else if (strInputData == "CLR")
      {
         ClearDisplay();
         Serial.printf("Clearing display...\r\n");
      }
      else if (strInputData == "RSTNW")
      {
         //Reset network
         ResetNetwork();
      }
      else if (strInputData.toInt() >= -999 && strInputData.toInt() <= 9999)
      {
         StopCountDownTimer();
         ShowNumber(strInputData.toInt(), 2);
         Serial.printf("Showing number: %li ...\r\n", strInputData.toInt());
      }
      else
      {
         //Invalid data received, make logging
         Serial.printf("Invalid data received, don't know what to do with this: %s\r\n", strInputData.c_str());
      }

      //Reset input data for next loop
      bInputStringComplete = false;
      strInputData = "";
   }

   if (millis() - ulLastAlivePing > ALIVE_PING_INTERVAL)
   {
      Serial.printf("Alive for %li seconds!\r\n", millis() / 1000);
      ulLastAlivePing = millis();
   }

   yield(); //Allow background stuff to happen
}

//Clears display so all segments of all digits are OFF
void ClearDisplay()
{
   for (byte x = 0; x < NUM_DIGITS; x++)
   {
      postNumber(' ', false);
   }
   //Latch the current segment data
   digitalWrite(segmentLatch, LOW);
   digitalWrite(segmentLatch, HIGH);
   return;
}

//Displays a number.
void ShowNumber(long lValue, uint8_t iNumDecimals)
{
   byte x = 0;
   bool bNegative = false;
   int8_t iNumDigitsModifier = 0;

   Serial.printf("Got number %lu\r\n", lValue);

   if (lValue < 0)
   {
      //negative value, remember for later (first digit should be sent last)
      bNegative = true;
      iNumDigitsModifier = -1;
      lValue = abs(lValue);
   }
   while (x < (NUM_DIGITS + iNumDigitsModifier))
   {
      int remainder = lValue % 10;
      if (remainder == 0 && lValue == 0 && (x > iNumDecimals))
      {
         //Don't display prefix zeroes
         remainder = ' ';
      }
      Serial.printf("Calling PostNumber with data: %i (lVal: %li, x: %i)\r\n", remainder, lValue, x);
      postNumber(remainder, (x == iNumDecimals && iNumDecimals > 0));

      lValue /= 10;
      x++;
   }
   if (bNegative)
   {
      //Send negative symbol last (=first digit)
      postNumber('-', false);
   }

   //Latch the current segment data
   digitalWrite(segmentLatch, LOW);
   digitalWrite(segmentLatch, HIGH); //Register moves storage register on the rising edge of RCK
}

//Given a number, or '-', shifts it out to the display
void postNumber(byte number, boolean decimal)
{
   //    -  A
   //   / / F/B
   //    -  G
   //   / / E/C
   //    -. D/DP

#define a 1 << 0
#define b 1 << 6
#define c 1 << 5
#define d 1 << 4
#define e 1 << 3
#define f 1 << 1
#define g 1 << 2
#define dp 1 << 7

   byte segments = 0;

   switch (number)
   {
   case 1:
      segments = b | c;
      break;
   case 2:
      segments = a | b | d | e | g;
      break;
   case 3:
      segments = a | b | c | d | g;
      break;
   case 4:
      segments = f | g | b | c;
      break;
   case 5:
      segments = a | f | g | c | d;
      break;
   case 6:
      segments = a | f | g | e | c | d;
      break;
   case 7:
      segments = a | b | c;
      break;
   case 8:
      segments = a | b | c | d | e | f | g;
      break;
   case 9:
      segments = a | b | c | d | f | g;
      break;
   case 0:
      segments = a | b | c | d | e | f;
      break;
   case ' ':
      segments = 0;
      break;
   case 'c':
      segments = g | e | d;
      break;
   case '-':
      segments = g;
      break;
   }

   if (decimal)
      segments |= dp;

   Serial.printf("Writing data to SR: %i\r\n", segments);

   digitalWrite(segmentClock, LOW);
   shiftOut(segmentData, segmentClock, MSBFIRST, segments);
}

void serialEvent()
{
   //Listen on serial port
   Serial.flush();
   while (Serial.available() > 0)
   {
      char cInChar = Serial.read(); // Read a character
      if (cInChar == '\n')          //Check if buffer contains complete serial message, terminated by newline (\n)
      {
         //Serial message in buffer is complete, null terminate it and store it for further handling
         bInputStringComplete = true;
         //strInputData += '\0'; // Null terminate the string
         break;
      }
      strInputData += cInChar; // Store it
   }
}

void HandleCountDownTimer()
{
   if (iCountDownTimer == 0)
   {
      //No timer is set, do nothing
      return;
   }

   //How far are we?
   unsigned int iTimeExpired = (millis() - ulCountDownStartTime) / 1000;
   if ((int)(iCountDownTimer - iTimeExpired) != iCountDownCurrentValue)
   {
      ShowNumber(iCountDownTimer - iTimeExpired, 0);
      iCountDownCurrentValue = iCountDownTimer - iTimeExpired;
   }
   if (iCountDownCurrentValue == 0)
   {
      StopCountDownTimer();
   }
   return;
}

void StartCountDownTimer(unsigned int iSeconds)
{
   ulCountDownStartTime = millis();
   iCountDownTimer = iSeconds;
   iCountDownCurrentValue = iSeconds;
   //Make sure first number of countdown is shown
   ShowNumber(iCountDownTimer, 0);
   return;
}

void StopCountDownTimer()
{
   ulCountDownStartTime = 0;
   iCountDownTimer = 0;
   iCountDownCurrentValue = 0;
   return;
}

void HandleWifiConfig()
{
   //read configuration from FS json
   Serial.println("mounting FS...");

   if (SPIFFS.begin())
   {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/config.json"))
      {
         //file exists, reading and loading
         Serial.println("reading config file");
         File configFile = SPIFFS.open("/config.json", "r");
         if (configFile)
         {
            Serial.println("opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            std::unique_ptr<char[]> buf(new char[size]);

            configFile.readBytes(buf.get(), size);
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(buf.get());
            json.printTo(Serial);
            if (json.success())
            {
               Serial.println("\nparsed json");

               if (json["ip"])
               {
                  Serial.println("setting custom ip from config");
                  //static_ip = json["ip"];
                  strcpy(static_ip, json["ip"]);
                  strcpy(static_gw, json["gateway"]);
                  strcpy(static_sn, json["subnet"]);
                  //strcat(static_ip, json["ip"]);
                  //static_gw = json["gateway"];
                  //static_sn = json["subnet"];
                  Serial.println(static_ip);
                  /*            Serial.println("converting ip");
                  IPAddress ip = ipFromCharArray(static_ip);
                  Serial.println(ip);*/
               }
               else
               {
                  Serial.println("no custom ip in config");
               }
            }
            else
            {
               Serial.println("failed to load json config");
            }
         }
      }
   }
   else
   {
      Serial.println("failed to mount FS, formatting...");
      SPIFFS.format();
      ESP.reset();
   }

   //set config save notify callback
   wifiMan.setSaveConfigCallback(saveConfigCallback);

   //set static ip
   IPAddress _ip, _gw, _sn;
   _ip.fromString(static_ip);
   _gw.fromString(static_gw);
   _sn.fromString(static_sn);

   wifiMan.setSTAStaticIPConfig(_ip, _gw, _sn);

   //reset settings - for testing
   //wifiMan.resetSettings();

   //fetches ssid and pass and tries to connect
   //if it does not connect it starts an access point with the specified name
   //here  "AutoConnectAP"
   //and goes into a blocking loop awaiting configuration
   char ssid[20];
   snprintf(ssid, 20, "EJS-DSP-%08X", ESP.getChipId());
   Serial.printf("Starting access point with ssid %s\r\n", ssid);
   if (!wifiMan.autoConnect(ssid))
   {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
   }

   //if you get here you have connected to the WiFi
   Serial.println("connected...yeey :)");

   //save the custom parameters to FS
   if (shouldSaveConfig)
   {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject &json = jsonBuffer.createObject();

      json["ip"] = WiFi.localIP().toString();
      json["gateway"] = WiFi.gatewayIP().toString();
      json["subnet"] = WiFi.subnetMask().toString();

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile)
      {
         Serial.println("failed to open config file for writing");
      }

      json.prettyPrintTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
   }

   Serial.println("local ip");
   Serial.println(WiFi.localIP());
   Serial.println(WiFi.gatewayIP());
   Serial.println(WiFi.subnetMask());
}

void HandleActivityLED()
{
   if (millis() - ulLastLEDOnTime > (ulLEDLinkInterval + 100))
   {
      //LED should blink
      ulLastLEDOnTime = millis();
      ulLEDOnTime = millis();
   }

   if (millis() > ulLEDOnTime && millis() < (ulLEDOnTime + 100))
   {
      bPrevledState = bLEDState;
      bLEDState = LOW;
   }
   else
   {
      bPrevledState = bLEDState;
      bLEDState = HIGH;
   }

   if (bLEDState != bPrevledState)
   {
      digitalWrite(ACTIVITY_LED_PIN, bLEDState);
   }
}

void HandleNWResetButton()
{
   byte bButtonState = digitalRead(RESET_NW_PIN);
   if (bButtonState == HIGH)
   {
      //Button is being pressed, was this already the case?
      if (ulButtonDepressTime > 0 && millis() - ulButtonDepressTime > RESET_NW_PRESS_TIME)
      {
         //Button was pressed longer than required, trigger NW reset
         ResetNetwork();
      }
      else if (ulButtonDepressTime == 0)
      {
         //Button was pressed for the first time, set depress timestamp
         ulButtonDepressTime = millis();

         //Increase LED blink rate to acknowledge button press
         ulLEDLinkInterval = 200;
      }
   }
   else if (ulButtonDepressTime > 0)
   {
      //Button was pressed but not anymore, reset depress timestamp
      ulButtonDepressTime = 0;

      //Set LED blink rate back to normal operation
      ulLEDLinkInterval = 1000;
   }
}

void ResetNetwork()
{
   //We should clear wifi parameters
   Serial.printf("Received a request to clear network, I will restart with autoconfig AP after this!\r\n");
   digitalWrite(ACTIVITY_LED_PIN, LOW);
   wifiMan.resetSettings();
   ESP.eraseConfig();
   delay(3000);
   ESP.restart();
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
   Serial.println("Should save config");
   shouldSaveConfig = true;
}