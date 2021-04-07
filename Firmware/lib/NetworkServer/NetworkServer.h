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

#ifndef _NetworkServer_h
#define _NetworkServer_h

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

