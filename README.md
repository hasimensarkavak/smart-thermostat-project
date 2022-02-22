# Smart Thermostat Project
Can use with Sinricpro, Google Home and Apple Homekit. You can also follow Telegram

<img src="https://lh3.googleusercontent.com/LDbL9BDfVZASl8Wh6v8aZn3foy-rDrYaumMK-mb8gLceN6cCxex5OxtAyK0c2hKsDFHplI5sXHl73A=s72" alt="Sinricpro Logo" width="50" height="50" class="lazyloaded" data-ll-status="loaded">
<img src="https://cdn.freelogovectors.net/svg07/google-home-logo.svg" alt="Google Home Logo" width="50" height="50" class="lazyloaded" data-ll-status="loaded">
<img src="https://upload.wikimedia.org/wikipedia/commons/c/cc/Apple_HomeKit_logo.svg" alt="Apple Home Logo" width="50" height="50" class="lazyloaded" data-ll-status="loaded">

## Sinricpro
* You should add a thermostat to your sinricpro account and paste the necessary keys into the places in relay.ino.

## Google Home
* You can use it by connecting with Sinricpro

## Apple Home Kit
* You can connect by entering the password you specified in the "thermostat_homekit.c" file

## Hardware Used
* Esp8266 (NodeMCU etc.) x17
* Relay x9
* Dht22 Tempreature Sensor x8
* Button x17

## Used
* WiFiManager
* ESP8266HTTPClient
* ESP8266WebServer
* EEPROM
* ArduinoJson
* SinricPro
* UniversalTelegramBot
* DHTesp
* homekit

## Project Detail
In this system, where 4 different houses are heated from the same combi boiler, every floor of each house was asked to be checked separately.
* House 1 => 1 Floor
* House 2 => 2 Floors
* House 3 => 2 Floors
* House 4 => 3 Floors
* Total => 8 Floors

So, we need 9 relay with main combi boiler and we need one tempreature sensor for every each floor, as a result we need 9 relay and 8 tempreature sensor

User can install the sensor anywhere on the floor because all devices will communicate via wifi. Relays on floors are server, main combi boiler and sensors are client

* You can change the codes according to your own need
