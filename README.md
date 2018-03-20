# WiFi numeric display

## Introduction

This project is a generic 4-digit 7-segment numeric display which can be controlled over WiFi

It was created for use in Flyball, but can easily be applied elsewhere

## Hardware

## Flashing the firmware

The firmware should flash just fine using the Arduino IDE with ESP8266 support. Open the `SevenSegmentDisplay.ino` file, select Wemos D1 mini as the board and hit flash.

You might want to change the OTA flash password in the `SevenSegmentDisplay.ino` file, search for the following line:
`#define OTA_PASSWD "EnterUniquePasswordHere!"`

## Connection & protocol

### Connecting to the display

The display listens on TCP port 23, no authentication is currently supported. Up to 5 clients can be connected simultaneously.

### Supported messages

Currently 2 messages are supported:

* `CDnnnn`: Where `nnnn` is a number between 0 and 9999. This message will start a countdown of the given number in seconds.
* `nnnn`: Where `nnnn` is a number between 0 and 9999. The number is expected to be an amount of time in hundredths of seconds. The time will be displayed in the format of SS.ss. e.g. sending `1234`, the display will show 12.34

Each message should be terminated by a newline (`\n`).