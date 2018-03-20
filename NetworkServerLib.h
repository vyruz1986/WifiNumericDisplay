/*
Name:		NetworkServerLib.h
Created:	12/24/2016 10:24:58 PM
Author:	Alex
Editor:	http://www.visualmicro.com
*/

#ifndef _NetworkServerLib_h
#define _NetworkServerLib_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include <ESP8266WiFi.h>
#define CLIENT_TIMEOUT 5000
#define ACK_MSG 0x06
#define NAK_MSG 0x15
#define ENQ_MSG 0x05

class NetworkServer
{
protected:

public:
   void init(WiFiServer* Server);
   void Loop();
   String GetOldestData();
   bool Available();

private:
   //struct to manage wifi connected clients
   struct _NetworkClient
   {
      bool bClientConnected = false;
      WiFiClient ClientObj;
      uint iLastActivityTime = 0;
      String strReceivedData;
      bool bDataComplete = false;
   };
   //Array to manage 5 different clients
   _NetworkClient _NetworkClients[4];

   WiFiServer* _Server;

   uint _iLastNetworkCheck = 0;
   uint _iNetworkCheckTimer = 10;

   unsigned long _ulLastStatusLog = 0;

   void _NetworkAccept();
   void _NetworkListen();
   _NetworkClient &_GetFreeNetworkClient();
   void _ResetNetworkClient(_NetworkClient & Client);
   void _DisconnectNetworkClient(_NetworkClient & Client);
   _NetworkClient &_GetOldestClientWithDataComplete();

   void _LogClientStates();

};

#endif

