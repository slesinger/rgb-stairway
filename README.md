# 🌈 RGB Stairway Lighting

> Transform your staircase into a dynamic light show with motion detection and Home Assistant integration

[![Video Demo](https://raw.githubusercontent.com/slesinger/rgb-stairway/master/docs/video-preview.png)](https://www.youtube.com/watch?v=-Y23b8RhR6I)

## ✨ What Is This?

An ESP32-powered smart stairway lighting system that makes every trip up or down the stairs a delightful experience. Using WS2812B LED strips and IR motion detection, your stairs automatically illuminate with customizable color animations when someone approaches—then gracefully fade away. Effects can be tailored via MQTT commands from Home Assistant, allowing for endless creativity.

**Why you'll love it:**
- 🎯 Automatic detection via IR sensors (no false triggers!)
- 🎨 Per-step color control with smooth blending
- 🏠 Full Home Assistant integration via MQTT
- 🌙 Day/Night modes for smart automation
- 🎭 Custom animation programs (or use built-in random colors)
- ⚡ Real-time response using ESP32 dual-core processing
- 🔧 Easy WiFi setup with captive portal

**Creative ideas to try:**
- 🎆 National colors on holidays
- 📬 Light up N steps to show unread emails
- 🔔 Flash all stairs when someone rings the doorbell
- 🏓 Ping-pong game (race the light wave!)
- 🏃 Kids competing to beat the animation
- 🎨 Daily color themes synced with your smart home

## 🛠️ Hardware Requirements

### Components
- **ESP32** development board (e.g., Lolin32) - ESP8266 won't cut it, we need dual cores!
- **WS2812B LED strips** - 3 LEDs per step (48 LEDs total for 16 steps)
- **2× IR LED transmitters** (38kHz compatible)
- **2× IR receivers/phototransistors**
- **Quality wiring** - Use UTP cable pairs (16 wires total) for reliability
- **Power supply** - 5V, sized for your LED count

### Tested Configuration
- 16 steps total (8 bottom + 8 top)
- 3 LEDs per step
- Split into two separate strips to avoid voltage drop

> ⚠️ **Important:** Don't power more than 8 steps from one end. Split your LED strips into upper and lower sections!

## 🔌 Wiring Guide

### ESP32 Pin Assignments

| Pin | Function | Description |
|-----|----------|-------------|
| **GPIO 13** | Bottom LED Data | WS2812B data line (steps 0-7) |
| **GPIO 12** | Top LED Data | WS2812B data line (steps 8-15) |
| **GPIO 4** | Bottom IR Receiver | IR sensor input (first step) |
| **GPIO 17** | Top IR Receiver | IR sensor input (last step) |
| **GPIO 16** | Top IR Transmitter | IR LED output (36kHz carrier) |

### Connection Notes
- Power both LED strips from adequate 5V supply
- ESP32 and LEDs should share common ground
- IR sensors work best with ~1-2 meter range across the step
- Consider resistors for IR LEDs (typically 100-330Ω depending on your LEDs)

## 🏠 Home Assistant Integration

### MQTT Topics

#### Published by Device
| Topic | Values | Description |
|-------|--------|-------------|
| `schody/sviti` | `on` / `off` | Whether any animation is currently running |
| `schody/status` | `noc` / `den` / `RECONNECTED` | Night/day mode status or reconnection event |
| `schody/ir/dole` | `on` / `off` | Bottom step IR beam broken |
| `schody/ir/nahore` | `on` / `off` | Top step IR beam broken |

#### Subscribed by Device
| Topic | Payload | Description |
|-------|---------|-------------|
| `schody/cmd` | Animation program | Custom color animation sequence |
| `schody/mode` | `N` / `D` / `R` | Night mode / Day mode / Reset |
| `schody/status` | Any | Acknowledged (resets watchdog) |

### Day/Night Modes

**Night Mode (`N`):**
- IR sensors trigger built-in animations (`PROGRAM_UPSTAIRS`/`PROGRAM_DOWNSTAIRS`)
- Instant response (~0ms delay)
- Perfect for automatic operation

**Day Mode (`D`):**
- IR sensors only publish MQTT events
- No automatic animations
- Great for manual control or disabling lights during daytime

### Sending Programs from Home Assistant

#### Simple Example (Single Step Test)
```yaml
# In your automation or script
service: mqtt.publish
data:
  topic: "schody/cmd"
  payload: "0:100(120r0g0b)1000(0r0g120b)100(0r0g0b)"
```

#### Wave Effect (All Steps)
```yaml
service: mqtt.publish
data:
  topic: "schody/cmd"
  payload: >
    0:10(0r0g0b)100(0r120g0b)8000(254r254g254b)2000(0r0g0b)
    1:50(0r0g0b)100(0r120g0b)8000(254r254g254b)2000(0r0g0b)
    2:100(0r0g0b)100(0r120g0b)8000(254r254g254b)2000(0r0g0b)
    3:150(0r0g0b)100(0r120g0b)8000(254r254g254b)2000(0r0g0b)
    ...
```

#### Random Color Animation
```yaml
service: mqtt.publish
data:
  topic: "schody/cmd"
  payload: >
    0:10(0r0g0b)100(255r255g255b)8000(254r254g254b)2000(0r0g0b)
    1:50(0r0g0b)100(255r255g255b)8000(254r254g254b)2000(0r0g0b)
    ...
```

### Program Syntax

**Format:** `<step>:<period>(<r>r<g>g<b>b)<period>(<r>r<g>g<b>b)...`

- **step:** Step number (0-15)
- **period:** Duration in **3ms ticks** (100 = 300ms)
- **r/g/b:** Color intensity (0-127 for normal values)

**Special Color Values:**
- `254` - Copy that channel from previous animation step's target (per-channel persistence)
- `255` - Random hue (generates one random color at 50% brightness)

**Examples:**
```
# Fade to red, hold, fade to black
0:10(0r0g0b)100(60r0g0b)1000(254r254g254b)2000(0r0g0b)

# Fade to red, shift to purple (R maintained, add blue)
0:10(0r0g0b)100(60r0g0b)1000(254r254g40b)2000(0r0g0b)

# Random color, hold, fade out
0:10(0r0g0b)100(255r255g255b)8000(254r254g254b)2000(0r0g0b)
```

### Home Assistant Configuration Example

```yaml
# configuration.yaml
mqtt:
  broker: YOUR_BROKER_IP
  port: 8883
  username: !secret mqtt_user
  password: !secret mqtt_password

# automations.yaml
- alias: "Stairs: Night Mode at Sunset"
  trigger:
    - platform: sun
      event: sunset
  action:
    - service: mqtt.publish
      data:
        topic: "schody/mode"
        payload: "N"

- alias: "Stairs: Day Mode at Sunrise"
  trigger:
    - platform: sun
      event: sunrise
  action:
    - service: mqtt.publish
      data:
        topic: "schody/mode"
        payload: "D"

- alias: "Stairs: Demo Animation"
  trigger:
    - platform: state
      entity_id: input_button.stairs_demo
      to: "on"
  action:
    - service: mqtt.publish
      data:
        topic: "schody/cmd"
        payload: >
          0:50(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b)
          1:200(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b)
          2:300(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b)
          3:450(0r0g0b)60(0r120g0b)85(0b120g0r)60(0r0g0b)
```

## 📦 Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- Python 3.x
- Git

### Quick Start

1. **Clone the repository**
   ```bash
   git clone https://github.com/slesinger/rgb-stairway.git
   cd rgb-stairway
   ```

2. **Create your config file**
   ```bash
   cp src/config.h-template src/config.h
   nano src/config.h  # Edit with your settings
   ```

3. **Configure settings in `src/config.h`**
   ```cpp
   #define MQTT_HOST  "192.168.1.2"      // Your MQTT broker IP
   #define MQTT_PORT  8883                // MQTT port
   #define MQTT_USER  "hass"              // MQTT username
   #define MQTT_PASS  "your_password"     // MQTT password
   
   #define WIFI_SETUP_AP_NAME "stupnice"  // Initial WiFi AP name
   #define WIFI_SETUP_AP_PASS "password123" // Initial WiFi AP password
   ```

4. **Build and upload**
   ```bash
   pio run --target upload
   ```

5. **Monitor serial output (optional)**
   ```bash
   pio device monitor
   ```

### First-Time WiFi Setup

On first boot (or when WiFi credentials are lost):

1. ESP32 creates WiFi access point named `stupnice` (or your configured name)
2. Connect to it with password `password123` (or your configured password)
3. Captive portal opens automatically (or navigate to `192.168.4.1`)
4. Select your WiFi network and enter password
5. Device reboots and connects to your network

The device will remember credentials and auto-reconnect on subsequent boots.

### Testing Your Installation

Send a test MQTT message:
```bash
mosquitto_pub -h YOUR_BROKER -p 8883 -u hass -P password \
  -t schody/cmd \
  -m "0:100(120r0g0b)1000(0r120g0b)100(0r0g0b)"
```

## 🧰 Maintenance & Updates

**Update PlatformIO packages:**
```bash
pio pkg update
```

**View serial logs:**
```bash
pio device monitor
```

**Clean build:**
```bash
pio run --target clean
pio run --target upload
```

## 📚 Libraries Used

This project leverages these excellent libraries:

- **[NeoPixelBus](https://github.com/Makuna/NeoPixelBus)** (v2.7.9+) - WS2812B LED control with DMA
- **[WiFiManager](https://github.com/tzapu/WiFiManager)** - Easy WiFi configuration
- **[PubSubClient](https://github.com/knolleary/pubsubclient)** (v2.8.0+) - MQTT client
- **[ArduinoOTA](https://github.com/esp8266/Arduino)** - Over-the-air updates (commented out by default)
- **ESP32 Arduino Core** - FreeRTOS and hardware abstraction

## 🔧 Advanced Configuration

### Customizing Default Animations

Edit `src/config.h` to modify built-in animations:

```cpp
// Random colors ascending
#define PROGRAM_UPSTAIRS "0:10(0r0g0b)100(255r255g255b)8000(254r254g254b)2000(0r0g0b) ..."

// Random colors descending
#define PROGRAM_DOWNSTAIRS "15:10(0r0g0b)100(255r255g255b)8000(254r254g254b)2000(0r0g0b) ..."

// Boot animation
#define PROGRAM_INIT "0:100(120r0g0b)100(0r120g0b)100(0r0g120b)1000(0r0g120b)100(0r0g0b)..."
```

### Adjusting Hardware Pins

Modify in `src/config.h`:
```cpp
#define NUM_SCHODU 16           // Total number of steps
#define STRIP_DOLE_PIN 13       // Bottom strip data pin
#define STRIP_NAHORE_PIN 12     // Top strip data pin
#define IR_RECV_DOLE_PIN 4      // Bottom IR receiver
#define IR_RECV_NAHORE_PIN 17   // Top IR receiver
#define IR_LED_NAHORE_PIN GPIO_NUM_16  // IR transmitter
```

## 🐛 Troubleshooting

**LEDs not lighting:**
- Check 5V power supply is adequate
- Verify data pin connections
- Ensure common ground between ESP32 and LED strips

**IR sensors not triggering:**
- Verify IR LED is powered and blinking (use phone camera to see IR)
- Check IR receiver orientation and range (1-2m optimal)
- Monitor serial output for IR debug messages

**WiFi connection issues:**
- Reset WiFi: Delete credentials and restart (access point will appear)
- Check WiFi signal strength
- Verify router supports 2.4GHz (ESP32 doesn't support 5GHz)

**MQTT not connecting:**
- Verify broker IP, port, username, password in `config.h`
- Check network connectivity: `ping YOUR_BROKER_IP`
- Monitor serial output for connection status
- Device auto-restarts if no status acknowledgment for 5 minutes

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest new features
- Submit pull requests
- Share your stairway setup photos/videos

## ⭐ Show Your Support

If you like this project, give it a ⭐ on GitHub and share your installation on social media!

---

Made with ❤️ for makers who want their stairs to be awesome
