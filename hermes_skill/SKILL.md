# AI Pocket Display Skill

Send AI responses to an ESP32-C6 Waveshare 1.47" display from Hermes Agent.

## Description

This skill enables Hermes Agent to push AI responses to a portable ESP32-C6
display device. When you chat with Hermes via Telegram, the AI's responses
are also shown on your pocket display in real-time.

## Requirements

- Hermes Agent v0.10.0+
- ESP32-C6 Waveshare 1.47" display running AI Pocket firmware
- PC Bridge server running on the same machine as Hermes Agent
- Both devices on the same WiFi network

## Configuration

Set these environment variables in your `~/.hermes/.env`:

```bash
# Required - IP of your ESP32-C6 display (check the display when it boots)
AI_POCKET_ESP32_IP=192.168.1.100

# Optional - Bridge server port (default: 8765)
AI_POCKET_BRIDGE_PORT=8765

# Optional - Enable automatic display push (default: true)
AI_POCKET_AUTO_PUSH=true
```

## Usage

Once configured, the skill activates automatically. Every AI response will be
sent to your ESP32-C6 display.

Manual commands available inside Hermes:

```
/pocket status     - Check ESP32 connectivity
/pocket on         - Enable display push
/pocket off        - Disable display push
/pocket test       - Send a test message to the display
/pocket led on     - Turn on RGB LED
/pocket led off    - Turn off RGB LED
```

## How It Works

1. You send a message to Hermes via Telegram
2. Hermes processes it using Xiaomi MiMo (or your configured LLM)
3. This skill intercepts the response and sends it to the bridge server
4. The bridge server forwards it to the ESP32-C6 over your local WiFi
5. The ESP32 displays both your message and the AI response on its 1.47" LCD

## Troubleshooting

**Display not updating?**
- Check that the ESP32 and your PC are on the same WiFi network
- Verify the ESP32 IP address in the status bar on boot
- Check the bridge server logs for errors

**Bridge server not reachable?**
- Make sure the bridge server is running: `python bridge_server.py`
- Check that the port (default 8765) is not blocked by firewall

## Author

Generated for AI Pocket project
License: MIT
