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

#include "NetworkServerLib.h"
#include <ESP8266WiFi.h>

void NetworkServer::init(WiFiServer* Server)
{
   _Server = Server;
   return;
}

void NetworkServer::Loop()
{
   _LogClientStates();
   _NetworkAccept();
   _NetworkListen();
}

String NetworkServer::GetOldestData()
{
   String strReturnString;
   
   if (!Available())
   {
      return strReturnString;
   }

   auto &OldestClient = _GetOldestClientWithDataComplete();

   if (OldestClient.bDataComplete)
   {
      strReturnString = OldestClient.strReceivedData;
      Serial.printf("Found data in client %i: '%s'!\r\n", 0, strReturnString.c_str());
      _ResetNetworkClient(OldestClient);
   }

   return strReturnString;
}

bool NetworkServer::Available()
{
   auto &OldestClient = _GetOldestClientWithDataComplete();
   if (OldestClient.bDataComplete)
   {
      return true;
   }
   return false;
}


void NetworkServer::_NetworkAccept()
{
   //Listen on server
   auto ClientObj = _Server->available();
   if (ClientObj.connected())
   {
      Serial.printf("Connection available\r\n");
      auto &Client = _GetFreeNetworkClient();
      Client.ClientObj = ClientObj;
      //client.flush();
      Serial.printf("Client connected!\r\n");
      Client.iLastActivityTime = millis();
      Client.bClientConnected = true;
   }
}
void NetworkServer::_NetworkListen()
{
   int i = 0;
   for (auto &Client : _NetworkClients)
   {
      //Handle client disconnects
      if (Client.bClientConnected
         && !Client.ClientObj.connected())
      {
         //Client has closed connection, close it from our side as well
         _ResetNetworkClient(Client);
         _DisconnectNetworkClient(Client);
      }

      //Serial.printf("Checking client %i: Connected %i(%i)\r\n", i, Client.bClientConnected, true);
      if (Client.bClientConnected
         && millis() - Client.iLastActivityTime > CLIENT_TIMEOUT
         && Client.strReceivedData.length() > 0)
      {
         //Timeout, reset buffer
         _ResetNetworkClient(Client);
         Serial.printf("No data received after timeout (%i\s), resetting buffer...\r\n", CLIENT_TIMEOUT / 1000);
      }
      else if (Client.ClientObj.available() > 0)
      {
         Serial.printf("Receiving: '");
         while (Client.ClientObj.available() > 0 && !Client.bDataComplete)
         {
            char cInChar = Client.ClientObj.read(); // Read a character
            //Check if this was an ENQ message
            if (cInChar == ENQ_MSG)
            {
               //ENQ received, confirm with ACK
               Client.ClientObj.write(ACK_MSG);
               return;
            }
            Serial.printf("%x", cInChar);
            if (cInChar == '\n') //Check if buffer contains complete message, terminated by newline (\n)
            {
               Serial.printf("'\r\n");
               //Message in buffer is complete, set complete marker
               Client.bDataComplete = true;
               Serial.printf("Received: '%s'\r\n", Client.strReceivedData.c_str());
               //And confirm with ACK to client
               Client.ClientObj.write(ACK_MSG);
               break;
            }
            Client.strReceivedData += cInChar; // Store it
         }
         Client.iLastActivityTime = millis();

      }
      i++;
   }
}

NetworkServer::_NetworkClient &NetworkServer::_GetFreeNetworkClient()
{
   _NetworkClient &OldestClient = _NetworkClients[0];
   bool bFreeClientFound = false;
   uint8_t i = 0;
   uint8_t iOldestClient = 0;
   for (auto &Client : _NetworkClients)
   {
      Serial.printf("Client %i last active: %lu\r\n", i, Client.iLastActivityTime);
      //Keep track of oldest client in case we have no more free ones
      if (Client.iLastActivityTime < OldestClient.iLastActivityTime)
      {
         OldestClient = Client;
         iOldestClient = i;
      }
      if (!Client.bClientConnected
         && Client.strReceivedData.length() == 0)
      {
         //Free client found, assign it
         Serial.printf("Found free client: %i\r\n", i);
         bFreeClientFound = true;
         return Client;
      }
      i++;
   }
   
   if (!bFreeClientFound)
   {
      //No free client is found.
      //Reset oldest (kick it out) and return that one
      Serial.printf("Using oldest client (%i) with alive time of %lu\r\n", iOldestClient, OldestClient.iLastActivityTime);
      _ResetNetworkClient(OldestClient);
      _DisconnectNetworkClient(OldestClient);
      return OldestClient;
   }
}

void NetworkServer::_ResetNetworkClient(_NetworkClient &Client)
{
   Serial.printf("Client buffer cleared\r\n");
   Client.bDataComplete = false;
   Client.strReceivedData = "";
}

void NetworkServer::_DisconnectNetworkClient(_NetworkClient &Client)
{
   if (Client.ClientObj.connected())
   {
      Client.ClientObj.stop();
   }
   Serial.printf("Client disconnected\r\n");
   Client.bClientConnected = false;
   Client.iLastActivityTime = 0;
   _ResetNetworkClient(Client);

}

NetworkServer::_NetworkClient& NetworkServer::_GetOldestClientWithDataComplete()
{
   _NetworkClient &OldestClient = _NetworkClients[0];
   int i = 0;
   for (auto &Client : _NetworkClients)
   {
      //Check each client if it is older and has valid data
      if (Client.iLastActivityTime < OldestClient.iLastActivityTime
         && Client.strReceivedData.length() > 0
         && Client.bDataComplete)
      {
         _NetworkClient& OldestClient = Client;
      }
      i++;
   }

   return OldestClient;
}

void NetworkServer::_LogClientStates()
{
   if (millis() - _ulLastStatusLog < 500)
   {
      return;
   }
   _ulLastStatusLog = millis();

   uint8_t i = 0;
   for (auto &Client : _NetworkClients)
   {
      Serial.printf("Client %i: C: %i | CO: %i | LaT: %lu\r\n", i, Client.bClientConnected, Client.ClientObj.connected(), Client.iLastActivityTime);
      i++;
   }
}