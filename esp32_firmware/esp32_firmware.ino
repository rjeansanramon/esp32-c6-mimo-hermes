/*
 * AI Pocket v2.0 - ESP32-C6 Waveshare 1.47" Display Firmware
 * 
 * Trendy UI with:
 *  - Real-time clock & date (NTP sync)
 *  - Modern chat bubbles with rounded corners
 *  - Animated mood emoji based on AI response
 *  - Gradient header & status bar
 *  - Neon accent colors
 * 
 * Hardware: Waveshare ESP32-C6-LCD-1.47 (172x320, ST7789, WiFi 6)
 * 
 * Required Libraries:
 *   - LovyanGFX by lovyan03
 *   - ArduinoJSON by Benoit Blanchon
 *   - WiFi (built-in)
 *   - WebServer (built-in)
 * 
 * Display Pinout (ESP32-C6-LCD-1.47):
 *   MOSI  -> GPIO6
 *   SCLK  -> GPIO7
 *   CS    -> GPIO14
 *   DC    -> GPIO15
 *   RST   -> GPIO21
 *   BL    -> GPIO22
 *   RGB   -> GPIO8
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>
#include <time.h>

// ============== LOVYANGFX DISPLAY CONFIG ==============
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host   = SPI2_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.pin_sclk   = 7;
      cfg.pin_mosi   = 6;
      cfg.pin_miso   = -1;
      cfg.pin_dc     = 15;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs          = 14;
      cfg.pin_rst         = 21;
      cfg.pin_busy        = -1;
      cfg.memory_width    = 172;
      cfg.memory_height   = 320;
      cfg.panel_width     = 172;
      cfg.panel_height    = 320;
      cfg.offset_x        = 34;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable        = false;
      cfg.invert          = true;
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

// ============== CONFIGURATION ==============
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  const char* WIFI_SSID = "YOUR_WIFI_SSID";
  const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

// NTP Configuration (WIB = UTC+7)
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 8 * 3600;  // UTC+8
const int DST_OFFSET_SEC = 0;

const int WEB_SERVER_PORT = 80;
#define RGB_LED_PIN 8
#define SCREEN_WIDTH 172
#define SCREEN_HEIGHT 320

// ============== COLOR PALETTE (Trendy Dark Theme) ==============
// Base colors
#define C_BG          0x0841  // Very dark blue-black (#0A0E1A)
#define C_BG_DARK     0x0000  // Pure black
#define C_SURFACE     0x10A3  // Dark surface (#141B2D)
#define C_SURFACE2    0x18E5  // Slightly lighter surface

// Accent colors (neon)
#define C_CYAN        0x07FF  // Neon cyan (#00FFFF)
#define C_CYAN_DIM    0x04B5  // Dim cyan
#define C_GREEN       0x07E0  // Neon green
#define C_GREEN_DIM   0x03E0  // Dim green
#define C_MAGENTA     0xF81F  // Neon magenta/pink
#define C_PINK        0xFE19  // Hot pink
#define C_ORANGE      0xFD20  // Neon orange
#define C_YELLOW      0xFFE0  // Neon yellow
#define C_PURPLE      0x780F  // Purple

// Text colors
#define C_TEXT        0xE71C  // Light gray
#define C_TEXT_DIM    0x8410  // Dim gray
#define C_TEXT_BRIGHT 0xFFFF  // White

// Chat bubble colors
#define C_USER_BUBBLE 0x1124  // Dark blue bubble
#define C_AI_BUBBLE   0x0A69  // Dark green-tinted bubble
#define C_USER_BORDER 0x2A6E  // Blue border
#define C_AI_BORDER   0x0544  // Green border

// Header gradient colors
#define C_GRAD_TOP    0x0169  // Deep navy
#define C_GRAD_BOT    0x0A2B  // Dark teal

// ============== MOOD SYSTEM ==============
enum MoodType {
  MOOD_HAPPY = 0,
  MOOD_THINKING,
  MOOD_EXCITED,
  MOOD_STRONG,
  MOOD_NEUTRAL,
  MOOD_WAVE,
  MOOD_LOVE,
  MOOD_ANGRY,
  MOOD_SAD,
  MOOD_COUNT
};

// Mood emoji bitmaps (12x12 for better visibility, uint16_t for 12-bit width)
#define MOOD_SIZE 12

const uint16_t moodBitmaps[MOOD_COUNT][MOOD_SIZE] PROGMEM = {
  // Happy 😊 - clear smile (circle face + smile)
  {0x000,0x1F8,0x204,0x294,0x204,0x204,0x30C,0x0F0,0x000,0x000,0x000,0x000},
  // Thinking 🤔 - raised eyebrow + chin touch
  {0x000,0x1F8,0x204,0x684,0x208,0x204,0x108,0x0F0,0x020,0x040,0x000,0x000},
  // Excited 😄 - big open mouth grin
  {0x000,0x1F8,0x204,0x294,0x204,0x2F4,0x3FC,0x108,0x0F0,0x000,0x000,0x000},
  // Strong 💪 - flexed bicep
  {0x000,0x0E0,0x110,0x390,0x270,0x040,0x0C0,0x120,0x210,0x000,0x000,0x000},
  // Neutral 😐 - straight line mouth
  {0x000,0x1F8,0x204,0x294,0x204,0x204,0x1F8,0x000,0x000,0x000,0x000,0x000},
  // Wave 👋 - waving hand
  {0x030,0x0D8,0x098,0x090,0x070,0x020,0x060,0x080,0x000,0x000,0x000,0x000},
  // Love ❤️ - heart shape
  {0x000,0x330,0x7F8,0x7F8,0x7F8,0x3F0,0x1E0,0x0C0,0x000,0x000,0x000,0x000},
  // Angry 😠 - V-shaped eyebrows, frown
  {0x000,0x1F8,0x204,0x492,0x294,0x204,0x108,0x0F0,0x000,0x000,0x000,0x000},
  // Sad 😢 - downturned mouth, tear drop
  {0x000,0x1F8,0x204,0x294,0x204,0x204,0x1F8,0x108,0x020,0x010,0x000,0x000},
};

// Mood colors
const uint16_t moodColors[MOOD_COUNT] = {
  0xFFE0,  // Happy - Yellow
  0x07FF,  // Thinking - Cyan
  0xFE19,  // Excited - Hot pink
  0x07E0,  // Strong - Green
  0xBDF7,  // Neutral - Light gray
  0xAFE5,  // Wave - Yellow-green
  0xF800,  // Love - Red
  0xF800,  // Angry - Red
  0x001F,  // Sad - Blue
};

// Mood labels
const char* moodLabels[MOOD_COUNT] = {
  "Happy", "Thinking", "Excited", "Strong", "Neutral", "Wave", "Love", "Angry", "Sad"
};

// ============== EMOJI BITMAP SYSTEM (8x8 inline) ==============
#define EMOJI_SIZE 8

enum EmojiIndex {
  EMOJI_SMILE = 0, EMOJI_LAUGH, EMOJI_HEART, EMOJI_CHECK, EMOJI_CROSS,
  EMOJI_WARN, EMOJI_IDEA, EMOJI_WAVE, EMOJI_PARTY, EMOJI_SPARKLE,
  EMOJI_FIRE, EMOJI_THUMB, EMOJI_THINK, EMOJI_STRONG, EMOJI_ROCKET,
  EMOJI_COUNT
};

const uint8_t emojiBitmaps[EMOJI_COUNT][8] PROGMEM = {
  {0x00,0x24,0x00,0x00,0x42,0x3C,0x00,0x00},  // 😊
  {0x00,0x24,0x00,0x18,0x42,0x3C,0x00,0x00},  // 😄
  {0x00,0x66,0xFF,0xFF,0xFF,0x7E,0x3C,0x18},  // ❤️
  {0xFE,0x82,0x84,0x88,0x90,0xA0,0x82,0xFE},  // ✅
  {0x00,0x42,0x24,0x18,0x18,0x24,0x42,0x00},  // ❌
  {0x10,0x28,0x28,0x44,0x44,0x82,0xFE,0x00},  // ⚠️
  {0x10,0x38,0x38,0x38,0x10,0x38,0x10,0x00},  // 💡
  {0x1E,0x22,0x42,0x44,0x48,0x50,0x20,0x00},  // 👋
  {0x08,0x14,0x22,0x41,0x22,0x14,0x08,0x00},  // 🎉
  {0x10,0x10,0xFE,0x10,0x10,0x28,0x44,0x82},  // ✨
  {0x10,0x38,0x38,0x7C,0x7C,0xFE,0x7C,0x38},  // 🔥
  {0x08,0x1C,0x08,0x08,0x1C,0x3E,0x7F,0x3E},  // 👍
  {0x00,0x24,0x00,0x00,0x42,0x3E,0x00,0x00},  // 🤔
  {0x1C,0x22,0x42,0x44,0x48,0x30,0x20,0x40},  // 💪
  {0x10,0x38,0x7C,0xFE,0x38,0x28,0x44,0x82}   // 🚀
};

const uint16_t emojiColors[EMOJI_COUNT] = {
  0xFFE0,0xFFE0,0xF800,0x07E0,0xF800,
  0xFFE0,0xFFE0,0xFFE0,0xF81F,0xFFE0,
  0xF800,0xFFE0,0xFFE0,0xFFE0,0xF800
};

const uint32_t emojiCodepoints[EMOJI_COUNT] PROGMEM = {
  0x1F60A,0x1F604,0x2764,0x2705,0x274C,
  0x26A0,0x1F4A1,0x1F44B,0x1F389,0x2728,
  0x1F525,0x1F44D,0x1F914,0x1F4AA,0x1F680
};

// ============== GLOBALS ==============
LGFX tft;
WebServer server(WEB_SERVER_PORT);

#define MAX_MSG_LENGTH 512
char lastUserMessage[MAX_MSG_LENGTH] = "Waiting for message...";
char lastAiResponse[MAX_MSG_LENGTH] = "AI Pocket Ready";
char statusText[64] = "Connecting...";
bool newMessageArrived = false;
unsigned long lastActivityTime = 0;
unsigned long lastAnimFrame = 0;
unsigned long lastClockUpdate = 0;
int animFrame = 0;
MoodType currentMood = MOOD_WAVE;
MoodType animMood = MOOD_WAVE;
int moodAnimPhase = 0;

// AI text scroll marquee state
enum AiScrollState { AI_SCROLL_TOP, AI_SCROLL_UP, AI_SCROLL_BOTTOM };
AiScrollState aiScrollState = AI_SCROLL_TOP;
int aiScrollOffset = 0;
unsigned long aiScrollTimer = 0;
int aiTotalLines = 0;
int aiVisibleLines = 6;
int aiLineHeight = 10;
bool aiNeedsScroll = false;
int g_aiBubbleY = 0;
int g_aiBubbleH = 0;
bool g_aiBubbleValid = false;

// RGB LED
bool rgbLedOn = true;

// ============== FUNCTION DECLARATIONS ==============
void setupDisplay();
void setupWiFi();
void setupWebServer();
void setupRGB();
void setupNTP();
void handleRoot();
void handleMessage();
void handleStatus();
void handleLED();
void handleNotFound();
void updateDisplay();
void drawClock();
void drawMoodEmoji();
void drawChatArea();
void drawStatusBar();
void drawRoundedRect(int x, int y, int w, int h, int r, uint16_t color);
void fillRoundedRect(int x, int y, int w, int h, int r, uint16_t color, uint16_t borderColor);
void wrapText(const char* text, int x, int y, int maxWidth, int maxLines, uint16_t color);
void drawInlineEmoji(int emojiIdx, int x, int y, uint16_t color);
int findEmoji(const char* utf8, int* bytesUsed);
MoodType detectMood(const char* text);
void setStatus(const char* msg);
void updateRGB();
void drawGradientHeader();
uint16_t blendColor(uint16_t c1, uint16_t c2, uint8_t ratio);
int countWrappedLines(const char* text, int maxWidth);
void drawAIScrolledText();

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("   AI Pocket v2.0 - Trendy Edition");
  Serial.println("   Hermes Agent + Xiaomi MiMo");
  Serial.println("========================================\n");

  setupRGB();
  setupDisplay();
  setupWiFi();
  setupNTP();
  setupWebServer();

  setStatus("Ready");
  lastActivityTime = millis();

  Serial.println("\n========================================");
  Serial.println("   Setup Complete!");
  Serial.println("========================================");
}

// ============== LOOP ==============
void loop() {
  server.handleClient();

  // Update clock every second
  unsigned long now = millis();
  if (now - lastClockUpdate >= 1000) {
    lastClockUpdate = now;
    drawClock();
  }

  // Mood animation (every 400ms)
  if (now - lastAnimFrame >= 400) {
    lastAnimFrame = now;
    animFrame++;
    drawMoodEmoji();
  }

  // AI text marquee scroll animation
  if (aiNeedsScroll && g_aiBubbleValid) {
    switch (aiScrollState) {
      case AI_SCROLL_TOP:
        // Pause at top for 3 seconds before scrolling
        if (now - aiScrollTimer >= 3000) {
          aiScrollState = AI_SCROLL_UP;
          aiScrollTimer = now;
        }
        break;
      case AI_SCROLL_UP:
        // Scroll up 2px every 150ms
        if (now - aiScrollTimer >= 150) {
          aiScrollTimer = now;
          aiScrollOffset += 2;
          int maxScroll = (aiTotalLines - aiVisibleLines) * aiLineHeight;
          if (aiScrollOffset >= maxScroll) {
            aiScrollOffset = maxScroll;
            aiScrollState = AI_SCROLL_BOTTOM;
            aiScrollTimer = now;
          }
          drawAIScrolledText();
        }
        break;
      case AI_SCROLL_BOTTOM:
        // Pause at bottom for 2 seconds then reset
        if (now - aiScrollTimer >= 2000) {
          aiScrollOffset = 0;
          aiScrollState = AI_SCROLL_TOP;
          aiScrollTimer = now;
          drawAIScrolledText();
        }
        break;
    }
  }

  if (newMessageArrived) {
    newMessageArrived = false;
    updateDisplay();
    lastActivityTime = millis();
  }

  updateRGB();

  if (WiFi.status() != WL_CONNECTED) {
    setStatus("WiFi Lost!");
    delay(5000);
    setupWiFi();
  }

  delay(10);
}

// ============== NTP SETUP ==============
void setupNTP() {
  Serial.println("[NTP] Syncing time...");
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  
  struct tm timeinfo;
  int retries = 0;
  while (!getLocalTime(&timeinfo) && retries < 10) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (retries < 10) {
    Serial.printf("\n[NTP] Time synced: %02d:%02d:%02d\n", 
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    Serial.println("\n[NTP] Sync failed, will retry");
  }
}

// ============== COLOR UTILITIES ==============
uint16_t blendColor(uint16_t c1, uint16_t c2, uint8_t ratio) {
  // ratio: 0 = c1, 255 = c2
  uint8_t r1 = (c1 >> 11) & 0x1F;
  uint8_t g1 = (c1 >> 5) & 0x3F;
  uint8_t b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F;
  uint8_t g2 = (c2 >> 5) & 0x3F;
  uint8_t b2 = c2 & 0x1F;
  uint8_t r = r1 + (((int32_t)(r2 - r1) * ratio) >> 8);
  uint8_t g = g1 + (((int32_t)(g2 - g1) * ratio) >> 8);
  uint8_t b = b1 + (((int32_t)(b2 - b1) * ratio) >> 8);
  return (r << 11) | (g << 5) | b;
}

// ============== DISPLAY SETUP ==============
void setupDisplay() {
  Serial.println("[DISPLAY] Initializing ST7789 172x320...");
  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(false);
  tft.fillScreen(C_BG);

  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH);

  // Boot animation
  tft.fillScreen(C_BG_DARK);
  
  // Animated boot logo
  for (int i = 0; i < 6; i++) {
    uint16_t c = blendColor(C_CYAN, C_GREEN, i * 50);
    tft.fillCircle(86, 140, 20 + i * 3, c);
    delay(80);
    tft.fillCircle(86, 140, 20 + i * 3, C_BG_DARK);
  }
  
  // Title
  tft.setTextColor(C_CYAN, C_BG_DARK);
  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.println("AI POCKET");
  
  tft.setTextColor(C_TEXT_DIM, C_BG_DARK);
  tft.setTextSize(1);
  tft.setCursor(25, 125);
  tft.println("Hermes Agent");
  tft.setCursor(30, 138);
  tft.println("Xiaomi MiMo");
  
  tft.setTextColor(C_GREEN_DIM, C_BG_DARK);
  tft.setCursor(15, 170);
  tft.println("v2.0 | ESP32-C6");
  
  delay(1500);
  tft.fillScreen(C_BG);
  
  Serial.println("[DISPLAY] Initialized OK");
}

// ============== WIFI SETUP ==============
void setupWiFi() {
  Serial.println("\n[WIFI] Connecting...");
  setStatus("WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected!");
    Serial.printf("[WIFI] IP: %s, RSSI: %d dBm\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    setStatus("Connected");
  } else {
    Serial.println("\n[WIFI] Failed!");
    setStatus("No WiFi");
  }
}

// ============== WEB SERVER SETUP ==============
void setupWebServer() {
  Serial.println("\n[SERVER] Starting...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/message", HTTP_POST, handleMessage);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/led", HTTP_POST, handleLED);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("[SERVER] Running on port %d\n", WEB_SERVER_PORT);
}

// ============== RGB LED ==============
void setupRGB() {
  pinMode(RGB_LED_PIN, OUTPUT);
  digitalWrite(RGB_LED_PIN, LOW);
  Serial.println("[RGB] Initialized");
}

void updateRGB() {
  if (!rgbLedOn) {
    neopixelWrite(RGB_LED_PIN, 0, 0, 0);  // GRB order for Waveshare
    return;
  }

  float t = millis() / 1000.0;

  switch (currentMood) {
    case MOOD_HAPPY: {
      // Yellow warm pulse
      int b = (sin(t * 3) + 1.0) * 20;
      neopixelWrite(RGB_LED_PIN, b, b, 0);  // G=R=b, B=0 → Yellow
      break;
    }
    case MOOD_ANGRY: {
      // Red fast flicker/pulse
      int b = (sin(t * 8) + 1.0) * 30;
      neopixelWrite(RGB_LED_PIN, 0, b, 0);  // G=0, R=b → Red (GRB order!)
      break;
    }
    case MOOD_SAD: {
      // Blue slow breathe
      int b = (sin(t * 1.0) + 1.0) * 15;
      neopixelWrite(RGB_LED_PIN, 0, 0, b);  // G=0, R=0, B=b → Blue
      break;
    }
    case MOOD_EXCITED: {
      // Magenta/Pink fast pulse
      int b = (sin(t * 6) + 1.0) * 20;
      neopixelWrite(RGB_LED_PIN, 0, b, b);  // G=0, R=b, B=b → Magenta (GRB!)
      break;
    }
    case MOOD_THINKING: {
      // Cyan slow rotate
      int b = (sin(t * 1.5) + 1.0) * 15;
      neopixelWrite(RGB_LED_PIN, b, 0, b);  // G=b, R=0, B=b → Cyan (GRB!)
      break;
    }
    case MOOD_STRONG: {
      // Green steady
      neopixelWrite(RGB_LED_PIN, 15, 0, 0);  // G=15, R=0 → Green (GRB!)
      break;
    }
    case MOOD_NEUTRAL: {
      // Dim white subtle
      int b = (sin(t * 0.5) + 1.0) * 3;
      neopixelWrite(RGB_LED_PIN, b, b, b);  // White is same in GRB
      break;
    }
    case MOOD_WAVE: {
      // Yellow-green
      int b = (sin(t * 2) + 1.0) * 10;
      int g = (sin(t * 2) + 1.0) * 7;
      neopixelWrite(RGB_LED_PIN, g, b, 0);  // G=g, R=b → Yellow-green (GRB!)
      break;
    }
    case MOOD_LOVE: {
      // Red-pink heartbeat rhythm
      float heartbeat = (sin(t * 4) > 0.5) ? 1.0 : 0.3;
      int b = heartbeat * 30;
      neopixelWrite(RGB_LED_PIN, 0, b, b / 4);  // G=0, R=b, B=b/4 → Red-pink (GRB!)
      break;
    }
    default: {
      // Fallback - slow blue breathe
      int b = (sin(t * 0.8) + 1.0) * 3;
      neopixelWrite(RGB_LED_PIN, 0, 0, b);
      break;
    }
  }
}

// ============== HTTP HANDLERS ==============
void handleRoot() {
  String html = "<!DOCTYPE html>"
    "<html><head><title>AI Pocket v2</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:-apple-system,sans-serif;background:#0A0E1A;color:#E71C;padding:16px;max-width:420px;margin:0 auto;}"
    "h1{color:#00FFFF;font-size:1.4em;margin-bottom:12px;}"
    ".card{background:#141B2D;border:1px solid #1E2A42;border-radius:12px;padding:14px;margin:10px 0;}"
    ".label{color:#8410;font-size:11px;text-transform:uppercase;letter-spacing:1px;}"
    ".value{color:#E71C;font-size:14px;margin-top:4px;word-break:break-word;}"
    ".accent{color:#00FFFF;}"
    "input[type=text]{width:100%;padding:10px;border:1px solid #1E2A42;border-radius:8px;background:#0A0E1A;color:#fff;margin:4px 0;}"
    "button{width:100%;padding:12px;background:linear-gradient(135deg,#00FFFF,#00CC88);color:#0A0E1A;border:none;border-radius:8px;cursor:pointer;font-weight:bold;margin-top:8px;}"
    "</style></head><body>"
    "<h1>AI Pocket <span style='font-size:0.6em;color:#8410'>v2.0</span></h1>"
    "<div class='card'>"
    "<div class='label'>Device</div><div class='value'>ESP32-C6 Waveshare 1.47\"</div>"
    "<div class='label' style='margin-top:8px'>IP</div><div class='value accent'>" + WiFi.localIP().toString() + "</div>"
    "<div class='label' style='margin-top:8px'>Mood</div><div class='value'>" + String(moodLabels[currentMood]) + "</div>"
    "</div>"
    "<div class='card'>"
    "<div class='label'>User</div><div class='value'>" + String(lastUserMessage) + "</div>"
    "<div class='label' style='margin-top:8px'>AI</div><div class='value'>" + String(lastAiResponse) + "</div>"
    "</div>"
    "<form action='/message' method='POST'>"
    "<div class='card'>"
    "<div class='label'>Test Message</div>"
    "<input type='text' name='user_msg' placeholder='User message'>"
    "<input type='text' name='ai_msg' placeholder='AI response'>"
    "<button type='submit'>Send</button>"
    "</div></form></body></html>";
  server.send(200, "text/html", html);
}

void handleMessage() {
  Serial.println("\n[SERVER] POST /message");
  String body = server.arg("plain");

  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (!error) {
    const char* userMsg = doc["user_message"] | "";
    const char* aiMsg = doc["ai_response"] | "";
    const char* src = doc["source"] | "unknown";

    if (strlen(userMsg) > 0) {
      strncpy(lastUserMessage, userMsg, MAX_MSG_LENGTH - 1);
      lastUserMessage[MAX_MSG_LENGTH - 1] = '\0';
    }
    if (strlen(aiMsg) > 0) {
      strncpy(lastAiResponse, aiMsg, MAX_MSG_LENGTH - 1);
      lastAiResponse[MAX_MSG_LENGTH - 1] = '\0';
    }

    // Detect mood from AI response
    currentMood = detectMood(lastAiResponse);
    animMood = currentMood;
    moodAnimPhase = 0;

    Serial.printf("[SERVER] From: %s | Mood: %s\n", src, moodLabels[currentMood]);
    Serial.printf("[SERVER] User: %s\n", lastUserMessage);
    Serial.printf("[SERVER] AI: %s\n", lastAiResponse);

    updateDisplay();
    lastActivityTime = millis();

    StaticJsonDocument<256> resp;
    resp["status"] = "ok";
    resp["mood"] = moodLabels[currentMood];
    String respStr;
    serializeJson(resp, respStr);
    server.send(200, "application/json", respStr);
  } else {
    String userMsg = server.arg("user_msg");
    String aiMsg = server.arg("ai_msg");

    if (userMsg.length() > 0) {
      strncpy(lastUserMessage, userMsg.c_str(), MAX_MSG_LENGTH - 1);
      lastUserMessage[MAX_MSG_LENGTH - 1] = '\0';
    }
    if (aiMsg.length() > 0) {
      strncpy(lastAiResponse, aiMsg.c_str(), MAX_MSG_LENGTH - 1);
      lastAiResponse[MAX_MSG_LENGTH - 1] = '\0';
    }

    currentMood = detectMood(lastAiResponse);
    animMood = currentMood;
    moodAnimPhase = 0;

    Serial.printf("[SERVER] Mood: %s\n", moodLabels[currentMood]);
    Serial.printf("[SERVER] User: %s\n", lastUserMessage);
    Serial.printf("[SERVER] AI: %s\n", lastAiResponse);

    updateDisplay();
    lastActivityTime = millis();
    server.send(200, "text/plain", "ok");
  }
}

void handleStatus() {
  struct tm timeinfo;
  char timeStr[20] = "??:??";
  if (getLocalTime(&timeinfo)) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  StaticJsonDocument<512> doc;
  doc["status"] = "ok";
  doc["device"] = "AI-Pocket-v2.0";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();
  doc["time"] = timeStr;
  doc["mood"] = moodLabels[currentMood];
  doc["last_user"] = lastUserMessage;
  doc["last_ai"] = lastAiResponse;
  String resp;
  serializeJson(doc, resp);
  server.send(200, "application/json", resp);
}

void handleLED() {
  String body = server.arg("plain");
  StaticJsonDocument<128> doc;
  if (!deserializeJson(doc, body)) {
    rgbLedOn = doc["on"] | true;
  }
  server.send(200, "text/plain", "ok");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ============== MOOD DETECTION ==============
MoodType detectMood(const char* text) {
  // Convert to lowercase for matching
  char lower[MAX_MSG_LENGTH];
  int len = strlen(text);
  if (len >= MAX_MSG_LENGTH) len = MAX_MSG_LENGTH - 1;
  for (int i = 0; i < len; i++) {
    lower[i] = tolower(text[i]);
  }
  lower[len] = '\0';

  // Check for excitement markers
  int exclCount = 0;
  int quesCount = 0;
  for (int i = 0; i < len; i++) {
    if (text[i] == '!') exclCount++;
    if (text[i] == '?') quesCount++;
  }

  // Love indicators
  if (strstr(lower, "love") || strstr(lower, "heart") || strstr(lower, "<3") ||
      strstr(lower, "sayang") || strstr(lower, "cinta")) {
    return MOOD_LOVE;
  }

  // Angry indicators
  if (strstr(lower, "angry") || strstr(lower, "kesel") || strstr(lower, "marah") ||
      strstr(lower, "emosi") || strstr(lower, "benci") || strstr(lower, "dongkol") ||
      strstr(lower, "sebel") || strstr(lower, "jengkel") ||
      strstr(lower, "\xF0\x9F\x98\xA4") || strstr(lower, "\xF0\x9F\x98\xA0") || strstr(lower, "\xF0\x9F\x98\xA1")) {
    return MOOD_ANGRY;
  }

  // Sad indicators
  if (strstr(lower, "sad") || strstr(lower, "sedih") || strstr(lower, "galau") ||
      strstr(lower, "capek") || strstr(lower, "lelah") || strstr(lower, "bosan") ||
      strstr(lower, "\xF0\x9F\x98\xA2") || strstr(lower, "\xF0\x9F\x98\xAD") || strstr(lower, "\xF0\x9F\x98\xA5")) {
    return MOOD_SAD;
  }

  // Strong/confident indicators (before Excited to catch "berhasil!" etc)
  if (strstr(lower, "done") || strstr(lower, "completed") || strstr(lower, "success") ||
      strstr(lower, "berhasil") || strstr(lower, "selesai") || strstr(lower, "siap") ||
      strstr(lower, "💪") || strstr(lower, "✅")) {
    return MOOD_STRONG;
  }

  // Happy indicators (before Excited to catch "bagus!" etc)
  if (strstr(lower, "happy") || strstr(lower, "great") || strstr(lower, "good") ||
      strstr(lower, "senang") || strstr(lower, "bagus") || strstr(lower, "mantap") ||
      strstr(lower, "😊") || strstr(lower, "😄") || strstr(lower, "👍")) {
    return MOOD_HAPPY;
  }

  // Excited indicators (require specific keywords, not just "!")
  if (exclCount >= 3 || strstr(lower, "wow") || strstr(lower, "amazing") ||
      strstr(lower, "awesome") || strstr(lower, "incredible") ||
      strstr(lower, "luar biasa") || strstr(lower, "keren") ||
      strstr(lower, "🎉") || strstr(lower, "🚀")) {
    return MOOD_EXCITED;
  }

  // Thinking indicators
  if (quesCount >= 2 || strstr(lower, "let me think") || strstr(lower, "hmm") ||
      strstr(lower, "interesting") || strstr(lower, "perhaps") ||
      strstr(lower, "mungkin") || strstr(lower, "🤔") ||
      strstr(lower, "analys") || strstr(lower, "checking")) {
    return MOOD_THINKING;
  }

  // Greeting / wave
  if (strstr(lower, "hello") || strstr(lower, "hi ") || strstr(lower, "halo") ||
      strstr(lower, "hey") || strstr(lower, "👋")) {
    return MOOD_WAVE;
  }

  return MOOD_NEUTRAL;
}

// ============== DRAWING HELPERS ==============
void fillRoundedRect(int x, int y, int w, int h, int r, uint16_t fillColor, uint16_t borderColor) {
  // Fill with border color first (slightly larger)
  tft.fillRoundRect(x, y, w, h, r, borderColor);
  // Fill inner with fill color
  tft.fillRoundRect(x + 1, y + 1, w - 2, h - 2, r > 1 ? r - 1 : 0, fillColor);
}

void drawGradientHeader() {
  // Header area: 0 to 50px
  for (int y = 0; y < 50; y++) {
    uint16_t lineColor = blendColor(C_GRAD_TOP, C_GRAD_BOT, (y * 255) / 50);
    tft.drawFastHLine(0, y, SCREEN_WIDTH, lineColor);
  }
  
  // Bottom accent line
  tft.drawFastHLine(0, 50, SCREEN_WIDTH, C_CYAN_DIM);
  tft.drawFastHLine(0, 51, SCREEN_WIDTH, blendColor(C_CYAN_DIM, C_BG, 128));
}

// ============== MAIN DISPLAY UPDATE ==============
void updateDisplay() {
  // Reset AI scroll marquee for new content
  aiScrollOffset = 0;
  aiScrollState = AI_SCROLL_TOP;
  aiScrollTimer = millis();
  g_aiBubbleValid = false;

  tft.fillScreen(C_BG);
  
  drawGradientHeader();
  drawClock();
  drawMoodEmoji();
  drawChatArea();
  drawStatusBar();
}

// ============== CLOCK DRAWING ==============
void drawClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  // Time (HH:MM) - large
  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  
  // Clear clock area
  tft.fillRect(5, 4, 100, 22, C_GRAD_TOP);
  
  tft.setTextColor(C_TEXT_BRIGHT, C_GRAD_TOP);
  tft.setTextSize(2);
  tft.setCursor(8, 6);
  tft.print(timeStr);

  // Seconds indicator (blinking colon effect via brightness)
  static bool colonVisible = true;
  colonVisible = !colonVisible;
  if (colonVisible) {
    tft.setTextColor(C_CYAN, C_GRAD_TOP);
  } else {
    tft.setTextColor(C_CYAN_DIM, C_GRAD_TOP);
  }
  tft.setCursor(8 + 24, 6);  // Position of colon (3rd char: HH:MM)
  tft.setTextSize(2);
  // Redraw just the colon area
  tft.fillRect(8 + 24, 6, 12, 16, C_GRAD_TOP);
  tft.print(":");

  // Date (DD Mon) - small
  char dateStr[12];
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  snprintf(dateStr, sizeof(dateStr), "%02d %s", timeinfo.tm_mday, months[timeinfo.tm_mon]);
  
  tft.setTextColor(C_TEXT_DIM, C_GRAD_TOP);
  tft.setTextSize(1);
  tft.setCursor(8, 30);
  tft.print(dateStr);

  // Day name
  const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  tft.setCursor(50, 30);
  tft.print(days[timeinfo.tm_wday]);

  // WiFi signal
  int rssi = WiFi.RSSI();
  tft.setCursor(SCREEN_WIDTH - 40, 6);
  if (rssi > -50) {
    tft.setTextColor(C_GREEN, C_GRAD_TOP);
    tft.print("[|||]");
  } else if (rssi > -65) {
    tft.setTextColor(C_YELLOW, C_GRAD_TOP);
    tft.print("[|| ]");
  } else {
    tft.setTextColor(C_ORANGE, C_GRAD_TOP);
    tft.print("[|  ]");
  }

  // Connection dot
  if (WiFi.status() == WL_CONNECTED) {
    tft.fillCircle(SCREEN_WIDTH - 8, 12, 3, C_GREEN);
  } else {
    tft.fillCircle(SCREEN_WIDTH - 8, 12, 3, C_ORANGE);
  }
}

// ============== ANIMATED MOOD EMOJI =============
void drawMoodEmoji() {
  // Mood area: 52 to 100 (expanded for 24x24 emoji)
  int emojiX = 6;
  int emojiY = 62;
  
  // Clear mood area
  tft.fillRect(0, 52, SCREEN_WIDTH, 48, C_BG);
  
  // Draw mood emoji (12x12 scaled to 24x24 with bounce animation)
  int bounceY = 0;
  if (moodAnimPhase < 4) {
    bounceY = -moodAnimPhase;
  } else {
    bounceY = moodAnimPhase - 8;
  }
  bounceY = abs(bounceY);
  
  const uint16_t* bitmap = moodBitmaps[currentMood];
  uint16_t color = moodColors[currentMood];
  
  for (int row = 0; row < MOOD_SIZE; row++) {
    uint16_t bits = pgm_read_word(&bitmap[row]);
    for (int col = 0; col < MOOD_SIZE; col++) {
      if (bits & (0x800 >> col)) {
        // 2x scale: each bitmap pixel becomes a 2x2 filled rectangle
        tft.fillRect(emojiX + col * 2, emojiY + row * 2 - bounceY, 2, 2, color);
      }
    }
  }
  
  // Mood label (next to the 24x24 emoji)
  tft.setTextColor(color, C_BG);
  tft.setTextSize(1);
  tft.setCursor(emojiX + 30, emojiY + 2);
  tft.print(moodLabels[currentMood]);

  // Status text
  tft.setTextColor(C_TEXT_DIM, C_BG);
  tft.setCursor(emojiX + 30, emojiY + 14);
  tft.print(statusText);

  // Decorative dots (animated)
  int dotX = SCREEN_WIDTH - 30;
  for (int i = 0; i < 3; i++) {
    uint16_t dotColor = blendColor(C_CYAN_DIM, C_CYAN, 
                          ((animFrame + i * 3) % 8) * 32);
    tft.fillCircle(dotX + i * 8, emojiY + 8, 2, dotColor);
  }
  
  // Update animation phase
  moodAnimPhase = (moodAnimPhase + 1) % 10;
}

// ============== CHAT AREA ==============
void drawChatArea() {
  int startY = 104;
  int areaHeight = SCREEN_HEIGHT - startY - 28; // Leave room for status bar
  
  // Clear chat area
  tft.fillRect(0, startY, SCREEN_WIDTH, areaHeight, C_BG);
  
  // Divider line at top
  tft.drawFastHLine(4, startY, SCREEN_WIDTH - 8, C_SURFACE2);
  
  // User message section
  int bubbleY = startY + 6;
  
  // User label
  tft.setTextColor(C_CYAN_DIM, C_BG);
  tft.setTextSize(1);
  tft.setCursor(6, bubbleY);
  tft.print("YOU");
  bubbleY += 12;
  
  // User message bubble
  int userMsgLen = strlen(lastUserMessage);
  int userBubbleH = 28; // Min height
  if (userMsgLen > 25) userBubbleH += 10;
  if (userMsgLen > 50) userBubbleH += 10;
  if (userMsgLen > 75) userBubbleH += 10;
  if (userBubbleH > 60) userBubbleH = 60;
  
  fillRoundedRect(4, bubbleY, SCREEN_WIDTH - 8, userBubbleH, 6, C_USER_BUBBLE, C_USER_BORDER);
  wrapText(lastUserMessage, 10, bubbleY + 4, SCREEN_WIDTH - 20, 4, C_TEXT);
  
  bubbleY += userBubbleH + 6;
  
  // Separator with gradient effect
  for (int i = 0; i < 3; i++) {
    uint16_t sepColor = blendColor(C_CYAN_DIM, C_BG, i * 85);
    tft.drawFastHLine(20 + i * 10, bubbleY + 1, SCREEN_WIDTH - 40 - i * 20, sepColor);
  }
  bubbleY += 8;
  
  // AI label
  tft.setTextColor(C_GREEN_DIM, C_BG);
  tft.setCursor(6, bubbleY);
  tft.print("AI");
  bubbleY += 12;
  
  // AI message bubble with marquee scroll support
  int aiMaxBubbleH = 80;
  int textMaxWidth = SCREEN_WIDTH - 20;

  // Count how many lines the AI text actually needs
  aiTotalLines = countWrappedLines(lastAiResponse, textMaxWidth);
  aiVisibleLines = (aiMaxBubbleH - 8) / aiLineHeight;  // 80-8=72, 72/10=7 visible lines
  if (aiVisibleLines < 1) aiVisibleLines = 1;

  // Determine bubble height (capped at max)
  int aiBubbleH = 8 + aiTotalLines * aiLineHeight;
  if (aiBubbleH < 28) aiBubbleH = 28;
  if (aiBubbleH > aiMaxBubbleH) aiBubbleH = aiMaxBubbleH;

  // Determine if scrolling is needed
  aiNeedsScroll = (aiTotalLines > aiVisibleLines);

  // Store bubble position for scroll redraws
  g_aiBubbleY = bubbleY;
  g_aiBubbleH = aiBubbleH;
  g_aiBubbleValid = true;

  // Draw bubble frame
  fillRoundedRect(4, bubbleY, SCREEN_WIDTH - 8, aiBubbleH, 6, C_AI_BUBBLE, C_AI_BORDER);

  if (aiNeedsScroll) {
    // Draw with clipping for scroll
    tft.setClipRect(5, bubbleY + 1, SCREEN_WIDTH - 10, aiBubbleH - 2);
    wrapText(lastAiResponse, 10, bubbleY + 4 - aiScrollOffset, textMaxWidth, aiTotalLines, C_TEXT);
    tft.clearClipRect();

    // Scroll indicator: small animated dots on right side
    int indX = SCREEN_WIDTH - 8;
    int indY = bubbleY + aiBubbleH - 4;
    tft.fillCircle(indX, indY - 6, 1, C_CYAN_DIM);
    tft.fillCircle(indX, indY, 1, C_CYAN_DIM);
    // Animated indicator dot
    int animDot = (millis() / 300) % 3;
    tft.fillCircle(indX, indY - 6 + animDot * 3, 1, C_CYAN);
  } else {
    // Normal drawing without scroll
    wrapText(lastAiResponse, 10, bubbleY + 4, textMaxWidth, aiVisibleLines, C_TEXT);
  }
}

// ============== STATUS BAR ==============
void drawStatusBar() {
  int barY = SCREEN_HEIGHT - 24;
  
  // Gradient status bar
  for (int y = barY; y < SCREEN_HEIGHT; y++) {
    uint16_t c = blendColor(C_BG, C_GRAD_TOP, ((y - barY) * 200) / 24);
    tft.drawFastHLine(0, y, SCREEN_WIDTH, c);
  }
  
  // Top accent line
  tft.drawFastHLine(0, barY, SCREEN_WIDTH, C_CYAN_DIM);
  
  // Status text
  tft.setTextColor(C_TEXT_DIM, C_GRAD_TOP);
  tft.setTextSize(1);
  tft.setCursor(6, barY + 5);
  tft.print("AI Pocket v2.0");
  
  // Uptime
  unsigned long sec = millis() / 1000;
  unsigned long min = sec / 60;
  unsigned long hr = min / 60;
  char uptimeStr[12];
  if (hr > 0) {
    snprintf(uptimeStr, sizeof(uptimeStr), "%luh %lum", hr, min % 60);
  } else {
    snprintf(uptimeStr, sizeof(uptimeStr), "%lum %lus", min, sec % 60);
  }
  tft.setCursor(SCREEN_WIDTH - 45, barY + 5);
  tft.print(uptimeStr);
  
  // IP address
  tft.setTextColor(C_TEXT_DIM, C_GRAD_TOP);
  tft.setCursor(6, barY + 14);
  tft.print(WiFi.localIP().toString());
}

// ============== TEXT WRAPPING WITH EMOJI SUPPORT =============
// Count wrapped lines (mirrors wrapText logic without drawing)
int countWrappedLines(const char* text, int maxWidth) {
  int cursorX = 0;
  int lineCount = 0;
  int charWidth = 6;

  int len = strlen(text);
  int pos = 0;

  while (pos < len) {
    cursorX = 0;

    while (pos < len && cursorX < maxWidth) {
      if (text[pos] == '\n') {
        pos++;
        break;
      }

      int bytesUsed = 0;
      int emojiIdx = findEmoji(text + pos, &bytesUsed);

      if (emojiIdx >= 0) {
        int emojiW = 10;
        if (cursorX + emojiW > maxWidth) break;
        cursorX += emojiW;
        pos += bytesUsed;
      } else if (bytesUsed > 1) {
        pos += bytesUsed;
      } else {
        if (text[pos] == ' ' || pos == 0 || text[pos-1] == ' ') {
          int wordEnd = pos;
          while (wordEnd < len && text[wordEnd] != ' ' && text[wordEnd] != '\n') wordEnd++;
          int wordPixels = (wordEnd - pos) * charWidth;
          if (cursorX + wordPixels > maxWidth && cursorX > 0) break;
        }
        cursorX += charWidth;
        pos++;
      }
    }

    if (cursorX == 0 && lineCount > 0) break;
    lineCount++;

    while (pos < len && text[pos] == ' ') pos++;
  }

  return lineCount;
}

void wrapText(const char* text, int x, int startY, int maxWidth, int maxLines, uint16_t color) {
  tft.setTextColor(color);  // Transparent background (bubble already filled)
  tft.setTextSize(1);

  int cursorX = x;
  int cursorY = startY;
  int lineCount = 0;
  int charWidth = 6;
  int lineHeight = 10;

  int len = strlen(text);
  int pos = 0;

  while (pos < len && lineCount < maxLines) {
    cursorX = x;
    
    while (pos < len && cursorX - x < maxWidth) {
      if (text[pos] == '\n') {
        pos++;
        break;
      }
      
      int bytesUsed = 0;
      int emojiIdx = findEmoji(text + pos, &bytesUsed);
      
      if (emojiIdx >= 0) {
        int emojiW = 10;
        if (cursorX + emojiW - x > maxWidth) break;
        drawInlineEmoji(emojiIdx, cursorX, cursorY, emojiColors[emojiIdx]);
        cursorX += emojiW;
        pos += bytesUsed;
      } else if (bytesUsed > 1) {
        pos += bytesUsed;
      } else {
        if (text[pos] == ' ' || pos == 0 || text[pos-1] == ' ') {
          int wordEnd = pos;
          while (wordEnd < len && text[wordEnd] != ' ' && text[wordEnd] != '\n') wordEnd++;
          int wordPixels = (wordEnd - pos) * charWidth;
          if (cursorX + wordPixels - x > maxWidth && cursorX > x) break;
        }
        tft.setCursor(cursorX, cursorY);
        tft.print(text[pos]);
        cursorX += charWidth;
        pos++;
      }
    }
    
    if (cursorX == x && lineCount > 0) break;
    cursorY += lineHeight;
    lineCount++;
    
    while (pos < len && text[pos] == ' ') pos++;
  }
}

// ============== INLINE EMOJI DRAWING ==============
void drawInlineEmoji(int emojiIdx, int x, int y, uint16_t color) {
  const uint8_t* bitmap = emojiBitmaps[emojiIdx];
  for (int row = 0; row < 8; row++) {
    uint8_t bits = pgm_read_byte(&bitmap[row]);
    for (int col = 0; col < 8; col++) {
      if (bits & (0x80 >> col)) {
        tft.drawPixel(x + col + 1, y + row + 1, color);
      }
    }
  }
}

int findEmoji(const char* utf8, int* bytesUsed) {
  uint32_t codepoint = 0;
  int len = 0;
  
  uint8_t first = (uint8_t)utf8[0];
  if (first < 0x80) { *bytesUsed = 1; return -1; }
  else if ((first & 0xE0) == 0xC0) { codepoint = first & 0x1F; len = 2; }
  else if ((first & 0xF0) == 0xE0) { codepoint = first & 0x0F; len = 3; }
  else if ((first & 0xF8) == 0xF0) { codepoint = first & 0x07; len = 4; }
  else { *bytesUsed = 1; return -1; }
  
  for (int i = 1; i < len; i++) {
    if (((uint8_t)utf8[i] & 0xC0) != 0x80) { *bytesUsed = 1; return -1; }
    codepoint = (codepoint << 6) | ((uint8_t)utf8[i] & 0x3F);
  }
  
  *bytesUsed = len;
  for (int i = 0; i < EMOJI_COUNT; i++) {
    if (pgm_read_dword(&emojiCodepoints[i]) == codepoint) return i;
  }
  return -1;
}

// ============== STATUS ==============
void setStatus(const char* msg) {
  strncpy(statusText, msg, sizeof(statusText) - 1);
  statusText[sizeof(statusText) - 1] = '\0';
}

// ============== AI SCROLL MARQUEE ==============
// Redraw just the AI bubble with current scroll offset (for animation)
void drawAIScrolledText() {
  if (!g_aiBubbleValid || !aiNeedsScroll) return;

  int bubbleY = g_aiBubbleY;
  int aiBubbleH = g_aiBubbleH;
  int textMaxWidth = SCREEN_WIDTH - 20;

  // Redraw full bubble (border + fill) then draw text on top with clipping
  fillRoundedRect(4, bubbleY, SCREEN_WIDTH - 8, aiBubbleH, 6, C_AI_BUBBLE, C_AI_BORDER);

  // Set clip rect to bubble interior so text doesn't bleed out
  tft.setClipRect(5, bubbleY + 1, SCREEN_WIDTH - 10, aiBubbleH - 2);
  wrapText(lastAiResponse, 10, bubbleY + 4 - aiScrollOffset, textMaxWidth, aiTotalLines, C_TEXT);
  tft.clearClipRect();

  // Scroll indicator dots on right side
  int indX = SCREEN_WIDTH - 8;
  int indY = bubbleY + aiBubbleH - 4;
  tft.fillCircle(indX, indY - 6, 1, C_CYAN_DIM);
  tft.fillCircle(indX, indY, 1, C_CYAN_DIM);
  int animDot = (millis() / 300) % 3;
  tft.fillCircle(indX, indY - 6 + animDot * 3, 1, C_CYAN);
}
