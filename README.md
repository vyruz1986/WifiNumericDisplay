# WiFi numeric display

## Introduction

This project is a generic 4-digit 7-segment numeric display which can be controlled over WiFi

It was created for use in Flyball, but can easily be applied elsewhere

Pictures & video: https://goo.gl/photos/G3tg8hrfvAJZ6PEY8

## Hardware

-Under construction-

Basically this is a 'beefed up' version of [Sparkfun's large 7 segment displays](https://www.sparkfun.com/products/8530)
together with [Sparkfun's accompanying large digit driver boards](https://www.sparkfun.com/products/13279).

The code is 1:1 compatible, the difference with my project is that I use the following components
to allow for better visibility (moar light!):

|Part type			| Sparkfun part						| My Part																	| Description																|
| ----------------- | --------------------------------- | ------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| Digit segment		| [12 SMD LEDs per segment (200mcd)](https://www.sparkfun.com/datasheets/Components/YSD-1600AR6F-89.pdf) driven with 12V	| 10cm of [24V 240 LED/meter strip](http://s.click.aliexpress.com/e/3zjiyZV)	| These are larger, more powerful LEDs which are also denser positioned.	|
| Segment driver	| [TPIC6C596](https://cdn.sparkfun.com/datasheets/Widgets/TPIC6C596.pdf) | [TPIC6B595](https://www.reichelt.de/?ACTION=3;ARTICLE=147328;SEARCH=tpic) | Compatible chip which can handle the extra current that out LED strip segments will draw |

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