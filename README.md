# ESPNow to MQTT Code

[![Build Status](https://travis-ci.com/debsahu/ESPNowMQTT.svg)](https://travis-ci.com/debsahu/ESPNowMQTT) [![License: MIT](https://img.shields.io/github/license/debsahu/ESPNowMQTT.svg)](https://opensource.org/licenses/MIT) [![version](https://img.shields.io/github/release/debsahu/ESPNowMQTT.svg)](https://github.com/debsahu/ESPNowMQTT/releases/tag/1.0.0) [![LastCommit](https://img.shields.io/github/last-commit/debsahu/ESPNowMQTT.svg?style=social)](https://github.com/debsahu/ESPNowMQTT/commits/master)

[![ESPNowMQTT](https://img.youtube.com/vi/XXXXXXXXXXX/0.jpg)](https://www.youtube.com/watch?v=XXXXXXXXXXX)

Please enter appropiate values in [config.h](https://github.com/debsahu/ESPNowMQTT/blob/master/Arduino/config.h), default [platformio.ini](https://github.com/debsahu/ESPNowMQTT/blob/master/platformio.ini) is ESP8266 code for ESPNowMQTT

## Server Code (ESPNow to MQTT)

### ESPNowMQTT

This is the code that reads ESPNow messages and transforms it to MQTT via WiFi.

Edit appropiate line in [platformio.ini](https://github.com/debsahu/ESPNowMQTT/blob/master/platformio.ini) 
```
src_dir = ./Arduino/ESPNowMQTT
```
Note: This stays continously connected to WiFi and receives ESPNow messages continously. 

Very IMPORTANT: ESPNow channel must be same as WiFi channel of your home router!

## Sensor Code (ESPNow data sent)

### ESPNowSender

This code sends ESPNow data to ESPNow server. So this can be a sensor reading.

Edit appropiate line in [platformio.ini](https://github.com/debsahu/ESPNowMQTT/blob/master/platformio.ini)
```
src_dir = ./Arduino/ESPNowSender
```

## Libraries Needed

[platformio.ini](https://github.com/debsahu/ESPNowMQTT/blob/master/platformio.ini) is included, use [PlatformIO](https://platformio.org/platformio-ide) and it will take care of installing the following libraries.

| Library                   | Link                                                       | Platform    |
|---------------------------|------------------------------------------------------------|-------------|
|MQTT                       |https://github.com/256dpi/arduino-mqtt                      |ESP8266/32   |
|ArduinoJson                |https://github.com/bblanchon/ArduinoJson                    |ESP8266/32   |