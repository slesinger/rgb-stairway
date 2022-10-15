# RGB LED Stairway - Schody

*WS2812B LED strip animations using modified FastLED for ESP32 and FreeRTOS*

Make you stairway very effective yet practical by using using colorful RGB LED lighting. Each step is controlled separately having its own color and intensity.

Operation is automated using IR gates on first and last step. It can also communicate with home automation system via MQTT.

Basic operation is that first and last step is shining in a dim color. When somebody steps on the step it will progressively light all steps and then nicely dim back after a minute.

With commands sent over MQTT it is possible to automate switching day/night mode and much more. For example, stairs can be switched off completely even during night, first/last step will illuminate only when nearby motion sensor is activated. MQTT message can carry color animation to play. Hence you are not limited to preprogrammed colors, just unleash your creativity :-)

Few ideas: 
 - I am using MQTT to run a demo sequence to test all steps and colors.
 - Stairs use national colors during some public holiday.
 - Number of illuminated steps can indicate number of a new mail recevied that day.
 - Stairs can blink to catch visual attention when somebody is ringing the bell.
 - Play ping-pong game as one step at a time progressively illuminates white (ping-pong ball color) and to bat by activating nearby motion sensor left or right.
 - Last but not least, kids love to compete to run the stairs faster then the light progresses.

[![video preview](https://raw.githubusercontent.com/slesinger/rgb-stairway/master/docs/video-preview.png)](https://www.youtube.com/watch?v=-Y23b8RhR6I)


## Hardware
 - RGB LED strib based on WS2812B
 - ESP32 (ESB 8266 is not sufficient because both CPU cores of ESP32 need to be used for precise timing)
 - 2x IR LED
 - 2x IR transistor
 - high quality cables - each end of stairway should get pair of UTP cables (16 wires)
 - stairs - my setup 8 + 8 steps, devided in two blocks

 Do not power more then 8 steps from one end of the strip. Better split strips to two parts.

## Wiring

See ```docs``` folder for wiring diagram.


### ESP32 pinout
**I**  - upper block - IR LED

**D** - upper block - WS2812 data

**R** - upper block - IR receiver

**^I**  - bottom block - IR LED

**^D** - bottom block - WS2812 data

**^R** - bottom block - IR receiver


## MQTT Topics

**Published topics**

```schody/sviti``` on or off based on wether animation is in progress

```schody/status```  OK or RECONNECTED to wifi

```schody/ir/dole``` on if IR gate is crossed on first (bottom) step, off otherwise

```schody/ir/nahore``` on if IR gate is crossed on last (top) step, off otherwise

**Listening on topics**

```schody/cmd``` receive color animation commands, see MQTT CMD Message layout

```schody/mode``` switch to a mode
 
### MQTT Mode Message Payload
 - R reset device
 - N night mode. Internal Down/Upstairs procedures will be enabled.
 - D day mode. Internal Down/Upstairs procedures will be suppressed.

### MQTT CMD Message layout
This is example demo sequence:
```
0:50(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 1:200(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 2:300(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 3:450(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 4:600(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 5:750(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 6:900(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b) 7:1050(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b)
```

## Internal triggers

User expects an immediate respone when stepping on first orlast step. Relying on MQTT and Hass automation brings additional delay ~500ms. This is workarounded by triggering PROGRAM_UPSTAIRS or PROGRAM_DOWNSTAIRS (see config.h) internally. This behavior can be enabled/disabled by setting nigh/day mode. Event sent via MQTT on user stepping first/last step is not impacted.

**Syntax**

Up to 30 repetitions of commands

```<command sequence id>```**:**```<duration in 5ms units>```**(**```<red-start>```**r**```<green-start>```**g**```<blue-start>```**b** **)**```<duration in 5ms units>```**(**```<red-end>```**r**```<green-end>```**g**```<blue-end>```**b** **)**```<space>```

Color values are intensities from 0-120. Test your specific LED strip what lowest value gives light. My strip is of in 0-7 and starts to light at 8 and more.

Suffix -start and -end distinguish color that the animation will beging with and end with. It is possible to insert step that does nothing just by providing -start and -end having the same color.

# Installation
1. Clone this repo
2. Create python venev ```python3 -m venev venv```
3. Activate venv ```source venv/binactivate```
4. Install [Platform.io Core](https://docs.platformio.org/en/latest/core/installation.html) ```pio install platformio```
5. Adjust values in src/config.h. Mostly MQTT connection details are needed.

## Build & Upload
```
platformio run --target upload
pio device monitor ?
pio update
pio lib update
```
## Connect to WiFi
Wifi password is not uploaded with binary. ESP will enter AP mode after first startup (or anytime it cannot connect to a know network). Default SSID of the setup AP is ```schody``` and password is ```password123```. Feel free to change this default in src/config.h

## Libraries used:
- `FastLED` https://github.com/FastLED/FastLED
- `FreeRTOS`
- `pubsubclient` https://github.com/knolleary/pubsubclient
- `WiFiManager` https://github.com/tzapu/WiFiManager

Project is built with [Arduino for ESP32](https://github.com/espressif/arduino-esp32) project.


## License

MIT license.
