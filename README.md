# AI Pocket - Portable AI Agent Display

A portable **AI pocket companion** built with the **ESP32-C6 Waveshare 1.47"** display, connected to **Hermes Agent** by Nous Research using **Xiaomi MiMo** as the LLM backend. Chat with your AI via Telegram and see responses on a pocket-sized display.

![Architecture](docs/architecture.png)

## What This Is

**AI Pocket** turns an ESP32-C6 microcontroller with a tiny 1.47" LCD into a portable display for your AI agent. Here's how it works:

1. You send a message to **Hermes Agent** via Telegram on your phone
2. Hermes processes your message using **Xiaomi MiMo** API (or any configured LLM)
3. The AI response is **pushed in real-time** to the ESP32-C6 display over your local WiFi
4. You see both your message and the AI's response on the pocket display

## Hardware Requirements

| Component | Specification | Notes |
|-----------|--------------|-------|
| **MCU** | Waveshare ESP32-C6-LCD-1.47 | 160MHz RISC-V, WiFi 6, BLE 5 |
| **Display** | 1.47" TFT LCD | 172x320px, ST7789 driver, 262K colors |
| **Storage** | 16MB Flash + TF card slot | For firmware and assets |
| **LED** | RGB color LED | GPIO8, visual status indicator |
| **Power** | USB-C 5V | Or 3.7V LiPo via GPIO |

## Project Structure

```
ai-pocket-esp32/
├── esp32_firmware/
│   ├── esp32_firmware.ino    # Main Arduino sketch
│   └── User_Setup.h          # TFT_eSPI display configuration
├── pc_bridge/
│   ├── bridge_server.py      # PC bridge (Hermes → ESP32)
│   └── requirements.txt      # Python dependencies
├── hermes_skill/
│   └── SKILL.md              # Hermes integration skill
└── README.md                 # This guide
```

## System Architecture

```
┌─────────────────┐     Telegram      ┌──────────────────────────────┐
│  Your Phone     │◄─────────────────►│  Local PC                    │
│  (Telegram App) │                   │  ┌────────────────────────┐  │
└─────────────────┘                   │  │ Hermes Agent           │  │
                                      │  │ ├─ Telegram Gateway    │  │
                                      │  │ ├─ Xiaomi MiMo API     │  │
                                      │  │ └─ AI Pocket Skill     │  │
                                      │  └────────────────────────┘  │
                                      │  ┌────────────────────────┐  │
                                      │  │ Bridge Server (Python) │  │
                                      │  │ Port: 8765             │  │
                                      │  └────────────────────────┘  │
                                      └──────────────┬───────────────┘
                                                     │ WiFi/LAN
                                                     │ HTTP POST
                                      ┌──────────────▼───────────────┐
                                      │  ESP32-C6 Waveshare 1.47"    │
                                      │  ├─ ST7789 Display 172x320   │
                                      │  ├─ WiFi 6 Client            │
                                      │  ├─ HTTP Server (Port 80)    │
                                      │  └─ RGB LED Status           │
                                      └──────────────────────────────┘
```

## Quick Start

### Step 1: Configure the ESP32 Display

#### 1.1 Install Arduino IDE & Board Support

1. Open **Arduino IDE** (v2.0+ recommended)
2. Go to **File → Preferences → Additional Boards Manager URLs**
3. Add this URL:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search for **"ESP32"** and install **"ESP32 by Espressif Systems"** (v3.0+)
6. Select board: **Tools → Board → ESP32Arduino → ESP32C6 Dev Module**

#### 1.2 Install Required Libraries

In Arduino IDE, go to **Sketch → Include Library → Manage Libraries** and install:

| Library | Version | Author | Purpose |
|---------|---------|--------|---------|
| **TFT_eSPI** | 2.5.43+ | Bodmer | ST7789 display driver |
| **ArduinoJSON** | 7.0+ | Benoit Blanchon | JSON parsing |

> **Note for TFT_eSPI**: ESP32-C6 requires a [patch](https://github.com/Bodmer/TFT_eSPI/pull/3438). Replace these files in your `libraries/TFT_eSPI/` folder with the patched versions.

#### 1.3 Configure Display Pins

Copy `esp32_firmware/User_Setup.h` to your TFT_eSPI library:

```bash
# macOS/Linux
cp esp32_firmware/User_Setup.h ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h

# Windows
copy esp32_firmware\User_Setup.h %USERPROFILE%\Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
```

#### 1.4 Flash the Firmware

1. Open `esp32_firmware/esp32_firmware.ino` in Arduino IDE
2. **Edit WiFi credentials** at the top of the sketch:
   ```cpp
   const char* WIFI_SSID = "YourWiFiName";
   const char* WIFI_PASSWORD = "YourWiFiPassword";
   ```
3. Select port: **Tools → Port → [your ESP32-C6 COM port]**
4. Click **Upload** (→ arrow)
5. Open **Tools → Serial Monitor** (115200 baud) to see boot messages

After flashing, the display will show:
- Boot screen with "AI Pocket" title
- WiFi connection status
- IP address (write this down!)

### Step 2: Set Up the PC Bridge

The bridge runs on the same PC as Hermes Agent.

#### 2.1 Install Dependencies

```bash
cd pc_bridge
pip install -r requirements.txt
```

#### 2.2 Run the Bridge Server

```bash
python bridge_server.py --esp32-ip <ESP32_IP>
```

Replace `<ESP32_IP>` with the IP address shown on your ESP32 display at boot.

Example:
```bash
python bridge_server.py --esp32-ip 192.168.1.100
```

The bridge will start on **port 8765** and show:
- Connection status to ESP32
- Web interface at `http://localhost:8765`
- Hermes webhook endpoint at `http://localhost:8765/hermes/webhook`

### Step 3: Configure Hermes Agent

#### 3.1 Set Xiaomi MiMo as LLM Provider

```bash
hermes auth add xiaomi_mimo
# Enter your MiMo API key when prompted

hermes model
# Select "xiaomi_mimo" as provider
# Select "mimo-v2.5-pro" as model
```

Your MiMo API key is available at [platform.xiaomimimo.com](https://platform.xiaomimimo.com).

#### 3.2 Configure Telegram Gateway

```bash
hermes gateway setup telegram
# Follow prompts to enter your bot token from @BotFather
```

Set allowed users for security:
```bash
export TELEGRAM_ALLOWED_USERS="your_telegram_user_id"
```

#### 3.3 Add the AI Pocket Skill

Copy the skill to Hermes skills directory:

```bash
cp -r hermes_skill ~/.hermes/skills/ai-pocket-display
```

Configure the skill in `~/.hermes/.env`:
```bash
# Required
AI_POCKET_ESP32_IP=192.168.1.100  # Your ESP32's IP

# Optional
AI_POCKET_BRIDGE_PORT=8765
AI_POCKET_AUTO_PUSH=true
```

#### 3.4 Configure Hermes Webhook (Recommended)

Add to your Hermes config (`~/.hermes/config.yaml`):

```yaml
platforms:
  webhook:
    enabled: true
    extra:
      port: 8644
      secret: "your-webhook-secret"
      routes:
        ai-pocket:
          events: ["agent_response"]
          secret: "pocket-secret"
          prompt: "{{response}}"
          deliver: "webhook"
          deliver_extra:
            url: "http://localhost:8765/hermes/webhook"
```

Or use the CLI:
```bash
hermes webhook subscribe ai-pocket \
  --events "agent_response" \
  --deliver webhook \
  --deliver-url "http://localhost:8765/hermes/webhook" \
  --secret "pocket-secret"
```

### Step 4: Test Everything

1. **Start the bridge** (if not running):
   ```bash
   python pc_bridge/bridge_server.py --esp32-ip 192.168.1.100
   ```

2. **Start Hermes gateway**:
   ```bash
   hermes gateway start telegram
   ```

3. **Send a message** to your Telegram bot

4. **Watch the magic** — your ESP32 display should show both your message and the AI response!

## API Reference

### ESP32-C6 Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web control panel |
| `/message` | POST | Receive message to display (JSON or form) |
| `/status` | GET | Get device status (JSON) |
| `/led` | POST | Control RGB LED (`{"state":"on"}` or `{"state":"off"}`) |

### Bridge Server Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Bridge status page |
| `/hermes/webhook` | POST | Hermes Agent webhook receiver |
| `/display/message` | POST | Direct message to ESP32 |
| `/esp32/status` | GET | Check ESP32 connectivity |
| `/esp32/led` | POST | Control ESP32 LED remotely |

### Direct API Test (cURL)

Send a test message directly:
```bash
curl -X POST http://192.168.1.100/message \
  -H "Content-Type: application/json" \
  -d '{"user_message":"Hello","ai_response":"Hi there!"}'
```

Via the bridge:
```bash
curl -X POST http://localhost:8765/display/message \
  -H "Content-Type: application/json" \
  -d '{"ai_response":"Test from bridge"}'
```

## Display Layout

The 172x320 pixel display is organized as:

```
┌─────────────────────┐  ← 0,0
│  AI Pocket    [|||] │  ← Header (30px): Title + WiFi signal
├─────────────────────┤
│  YOU:               │  ← User message area
│  Hello AI, how are  │
│  you doing today?   │
├─────────────────────┤
│  AI:                │  ← AI response area
│  I'm doing great!   │
│  Ready to help you  │
│  with anything.     │
│                     │
│                     │
│                     │
│                     │
├─────────────────────┤
│  IP: 192.168.1.100  │  ← Status bar (22px): Connection info
└─────────────────────┘  ← 171,319
       172px wide
```

## Troubleshooting

### ESP32 won't connect to WiFi
- Double-check SSID and password in the firmware
- Ensure 2.4GHz WiFi (ESP32-C6 doesn't support 5GHz-only networks)
- Check WiFi security: WPA2 supported, WPA3 may have issues

### Display shows garbled text
- Verify TFT_eSPI `User_Setup.h` has correct pin mappings
- Check SPI frequency (try reducing to 20000000)
- Ensure color order is correct (`TFT_RGB` vs `TFT_BGR`)

### Bridge can't reach ESP32
- Verify both devices are on the same network
- Check the ESP32 IP in Serial Monitor or on display boot
- Temporarily disable firewall: `sudo ufw disable` (Linux)

### Hermes not sending to display
- Check bridge is running: `curl http://localhost:8765/`
- Verify webhook is configured correctly
- Check Hermes logs: `hermes logs`

### Xiaomi MiMo API errors
- Verify API key at [platform.xiaomimimo.com](https://platform.xiaomimimo.com)
- Check model name: use `mimo-v2.5-pro` or `mimo-v2.5`
- Note: MiMo API has some OpenAI compatibility issues with tool calling

## Hardware Pinout Reference

### ESP32-C6 Waveshare 1.47" LCD Pin Mapping

| Function | GPIO | Description |
|----------|------|-------------|
| **MOSI** | GPIO6 | SPI data to display |
| **SCLK** | GPIO7 | SPI clock |
| **CS** | GPIO14 | Chip select |
| **DC** | GPIO15 | Data/command select |
| **RST** | GPIO21 | Display reset |
| **BL** | GPIO22 | Backlight control |
| **RGB** | GPIO8 | WS2812 RGB LED |

### TF Card Pin Mapping (Optional)

| Function | GPIO |
|----------|------|
| MISO | GPIO5 |
| MOSI | GPIO6 (shared) |
| SCLK | GPIO7 (shared) |
| CS | GPIO4 |

## Power Options

| Source | Voltage | Notes |
|--------|---------|-------|
| USB-C | 5V | Primary power, stable |
| LiPo Battery | 3.7V | Via 2-pin header, portable use |
| GPIO 3V3 | 3.3V | For external sensors |

**Power consumption**: ~80mA with backlight on, ~30mA in sleep mode.

A 1000mAh LiPo battery provides approximately **8-10 hours** of continuous use.

## Extending the Project

### Add a Battery
Connect a 3.7V LiPo battery to the 2-pin battery header for portable operation.

### Add a Speaker (Future)
The ESP32-C6 has I2S support. Add a MAX98357A I2S amplifier + speaker for:
- Audio notifications
- Text-to-speech responses
- Wake word detection

### Touch Support
If using the **ESP32-C6-Touch-LCD-1.47** variant (with touch):
- Touch controller: AXS5106L
- Add touch calibration in firmware
- Implement on-screen buttons for quick actions

### Deep Sleep Mode
Add deep sleep for battery saving:
```cpp
// Wake on button press or message arrival
esp_sleep_enable_ext0_wakeup(GPIO_NUM_9, 0);  // BOOT button
esp_light_sleep_start();
```

## Resources

- **ESP32-C6-LCD-1.47 Docs**: [docs.waveshare.com/ESP32-C6-LCD-1.47](https://docs.waveshare.com/ESP32-C6-LCD-1.47)
- **Hermes Agent**: [hermes-agent.nousresearch.com](https://hermes-agent.nousresearch.com/)
- **Hermes GitHub**: [github.com/NousResearch/hermes-agent](https://github.com/NousResearch/hermes-agent)
- **Xiaomi MiMo API**: [platform.xiaomimimo.com/docs](https://platform.xiaomimimo.com/docs/en-US/api/chat/openai-api)
- **TFT_eSPI Library**: [github.com/Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)

## License

MIT License - Feel free to modify and extend!

---

Built with passion for portable AI. The future fits in your pocket.
