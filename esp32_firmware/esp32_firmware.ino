/*
 * AI Pocket - ESP32-C6 Waveshare 1.47" Display Firmware
 * 
 * A portable AI agent display that connects to Hermes Agent via local network.
 * Displays AI responses from Xiaomi MiMo (via Hermes) on a compact 172x320 LCD.
 * 
 * Hardware: Waveshare ESP32-C6-LCD-1.47 (172x320, ST7789, WiFi 6)
 * 
 * Required Libraries:
 *   - TFT_eSPI by Bodmer (with ESP32-C6 patch)
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
#include <TFT_eSPI.h>

// ============== CONFIGURATION ==============
// WiFi Credentials - defined in secrets.h
// Copy secrets.h.example to secrets.h and edit your credentials there
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  const char* WIFI_SSID = "YOUR_WIFI_SSID";
  const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

// Optional: static IP configuration (uncomment if needed)
// IPAddress local_IP(192, 168, 1, 100);
// IPAddress gateway(192, 168, 1, 1);
// IPAddress subnet(255, 255, 255, 0);

// Web server port
const int WEB_SERVER_PORT = 80;

// RGB LED pin
#define RGB_LED_PIN 8

// Display dimensions
#define SCREEN_WIDTH 172
#define SCREEN_HEIGHT 320

// Colors
#define COLOR_BG TFT_BLACK
#define COLOR_TEXT TFT_WHITE
#define COLOR_USER_MSG 0x7BEF    // Gray-ish for user messages
#define COLOR_AI_MSG 0x07E0      // Green for AI messages
#define COLOR_STATUS 0xF800      // Red for status
#define COLOR_HEADER_BG 0x18C3   // Dark blue for header
#define COLOR_HEADER_TEXT TFT_WHITE
#define COLOR_BORDER 0x4208      // Dark gray border

// ============== GLOBALS ==============
TFT_eSPI tft = TFT_eSPI();
WebServer server(WEB_SERVER_PORT);

// Message buffer
#define MAX_MSG_LENGTH 512
char lastUserMessage[MAX_MSG_LENGTH] = "Waiting for message...";
char lastAiResponse[MAX_MSG_LENGTH] = "AI Pocket Ready";
char statusText[64] = "Disconnected";
bool newMessageArrived = false;
unsigned long lastActivityTime = 0;

// RGB LED state
bool rgbLedOn = true;
uint8_t rgbHue = 0;

// ============== FUNCTION DECLARATIONS ==============
void setupDisplay();
void setupWiFi();
void setupWebServer();
void setupRGB();
void handleRoot();
void handleMessage();
void handleStatus();
void handleLED();
void handleNotFound();
void updateDisplay();
void drawHeader();
void drawMessageArea();
void drawStatusBar();
void wrapText(const char* text, int x, int y, int maxWidth, int maxLines, uint16_t color);
void setStatus(const char* msg, uint16_t color);
void updateRGB();

// ============== SETUP ==============
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("   AI Pocket - ESP32-C6 AI Display");
  Serial.println("   Hermes Agent + Xiaomi MiMo");
  Serial.println("========================================\n");

  setupRGB();
  setupDisplay();
  setupWiFi();
  setupWebServer();

  setStatus("Ready - Waiting for AI", COLOR_AI_MSG);
  lastActivityTime = millis();

  Serial.println("\n========================================");
  Serial.println("   Setup Complete!");
  Serial.println("========================================");
}

// ============== LOOP ==============
void loop() {
  server.handleClient();

  if (newMessageArrived) {
    newMessageArrived = false;
    updateDisplay();
    lastActivityTime = millis();
  }

  updateRGB();

  // Connection status check
  if (WiFi.status() != WL_CONNECTED) {
    setStatus("WiFi Lost! Reconnecting...", COLOR_STATUS);
    delay(5000);
    setupWiFi();
  }

  delay(10);
}

// ============== DISPLAY SETUP ==============
void setupDisplay() {
  Serial.println("[DISPLAY] Initializing ST7789 172x320...");

  tft.init();
  tft.setRotation(0);           // Portrait mode
  tft.invertDisplay(0);         // Normal colors
  tft.fillScreen(COLOR_BG);

  // Turn on backlight
  pinMode(22, OUTPUT);
  digitalWrite(22, HIGH);

  // Welcome screen
  tft.fillScreen(COLOR_BG);

  // Draw border frame
  tft.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BORDER);
  tft.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, COLOR_BORDER);

  // Title
  tft.setTextColor(COLOR_AI_MSG, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(10, 30);
  tft.println("AI Pocket");

  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 55);
  tft.println("Hermes Agent");
  tft.setCursor(10, 68);
  tft.println("Xiaomi MiMo");

  // Version info
  tft.setTextColor(COLOR_USER_MSG, COLOR_BG);
  tft.setCursor(10, 100);
  tft.println("v1.0.0 | ESP32-C6");
  tft.setCursor(10, 113);
  tft.println("WiFi 6 | 172x320");

  // Status
  tft.setTextColor(COLOR_STATUS, COLOR_BG);
  tft.setCursor(10, 150);
  tft.println("Booting...");

  Serial.println("[DISPLAY] ST7789 initialized OK");
}

// ============== WIFI SETUP ==============
void setupWiFi() {
  Serial.println("\n[WIFI] Connecting to WiFi...");
  setStatus("Connecting WiFi...", COLOR_STATUS);

  // Optional static IP
  // WiFi.config(local_IP, gateway, subnet);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    char ipStr[32];
    snprintf(ipStr, sizeof(ipStr), "IP: %s", WiFi.localIP().toString().c_str());
    setStatus(ipStr, COLOR_AI_MSG);
  } else {
    Serial.println("\n[WIFI] Connection failed!");
    setStatus("WiFi Failed!", COLOR_STATUS);
  }
}

// ============== WEB SERVER SETUP ==============
void setupWebServer() {
  Serial.println("\n[SERVER] Starting web server...");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/message", HTTP_POST, handleMessage);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/led", HTTP_POST, handleLED);
  server.onNotFound(handleNotFound);

  server.begin();

  char serverMsg[64];
  snprintf(serverMsg, sizeof(serverMsg), "Server on port %d", WEB_SERVER_PORT);
  Serial.printf("[SERVER] %s\n", serverMsg);
}

// ============== RGB LED SETUP ==============
void setupRGB() {
  pinMode(RGB_LED_PIN, OUTPUT);
  digitalWrite(RGB_LED_PIN, LOW);  // Off initially
  Serial.println("[RGB] LED initialized");
}

void updateRGB() {
  if (!rgbLedOn) {
    neopixelWrite(RGB_LED_PIN, 0, 0, 0);
    return;
  }

  // Subtle breathing effect when idle
  unsigned long elapsed = millis() - lastActivityTime;

  if (elapsed < 30000) {
    // Recent activity - green pulse
    int brightness = (sin(millis() / 500.0) + 1.0) * 5;
    neopixelWrite(RGB_LED_PIN, 0, brightness, 0);
  } else {
    // Idle - slow blue pulse
    int brightness = (sin(millis() / 2000.0) + 1.0) * 2;
    neopixelWrite(RGB_LED_PIN, 0, 0, brightness);
  }
}

// ============== HTTP HANDLERS ==============
void handleRoot() {
  String html = "<!DOCTYPE html>"
    "<html><head><title>AI Pocket</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px;max-width:400px;margin:0 auto;}"
    "h1{color:#4ecca3;}"
    ".msg{background:#16213e;padding:15px;border-radius:8px;margin:10px 0;}"
    ".label{color:#888;font-size:12px;text-transform:uppercase;}"
    "form{margin:15px 0;}"
    "input[type=text]{width:100%;padding:10px;border:none;border-radius:4px;background:#0f3460;color:#fff;box-sizing:border-box;}"
    "button{padding:10px 20px;background:#4ecca3;color:#1a1a2e;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin-top:10px;}"
    "</style></head><body>"
    "<h1>AI Pocket Control</h1>"
    "<div class='msg'>"
    "<div class='label'>Device</div>"
    "<div>ESP32-C6 Waveshare 1.47\"</div>"
    "<div class='label' style='margin-top:8px;'>IP Address</div>"
    "<div>" + WiFi.localIP().toString() + "</div>"
    "</div>"
    "<div class='msg'>"
    "<div class='label">User Message</div>"
    "<div>" + String(lastUserMessage) + "</div>"
    "</div>"
    "<div class='msg'>"
    "<div class='label'>AI Response</div>"
    "<div>" + String(lastAiResponse) + "</div>"
    "</div>"
    "<form action='/message' method='POST'>"
    "<div class='label'>Send Test Message</div>"
    "<input type='text' name='user_msg' placeholder='User message' value='Hello AI'><br>"
    "<input type='text' name='ai_msg' placeholder='AI response' value='Hello! I am your AI pocket assistant.'>"
    "<button type='submit'>Send to Display</button>"
    "</form>"
    "</body></html>";

  server.send(200, "text/html", html);
}

void handleMessage() {
  Serial.println("\n[SERVER] POST /message received");

  String body = server.arg("plain");

  // Try JSON parsing first
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (!error) {
    // JSON format
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

    Serial.printf("[SERVER] From: %s\n", src);
    Serial.printf("[SERVER] User: %s\n", lastUserMessage);
    Serial.printf("[SERVER] AI: %s\n", lastAiResponse);

    newMessageArrived = true;

    StaticJsonDocument<256> response;
    response["status"] = "ok";
    response["message"] = "Display updated";
    String responseStr;
    serializeJson(response, responseStr);
    server.send(200, "application/json", responseStr);
  } else {
    // Try form data (x-www-form-urlencoded)
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

    Serial.printf("[SERVER] User: %s\n", lastUserMessage);
    Serial.printf("[SERVER] AI: %s\n", lastAiResponse);

    newMessageArrived = true;
    server.send(200, "text/plain", "Display updated");
  }
}

void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["status"] = "ok";
  doc["device"] = "AI-Pocket-ESP32-C6";
  doc["version"] = "1.0.0";
  doc["display"] = "ST7789 172x320";
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime_ms"] = millis();
  doc["last_user_message"] = lastUserMessage;
  doc["last_ai_response"] = lastAiResponse;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleLED() {
  String body = server.arg("plain");

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (!error) {
    const char* state = doc["state"] | "on";
    rgbLedOn = (strcmp(state, "on") == 0);

    if (!rgbLedOn) {
      neopixelWrite(RGB_LED_PIN, 0, 0, 0);
    }

    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
  }
}

void handleNotFound() {
  server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Not found\"}");
}

// ============== DISPLAY RENDERING ==============
void updateDisplay() {
  tft.fillScreen(COLOR_BG);

  drawHeader();
  drawMessageArea();
  drawStatusBar();
}

void drawHeader() {
  // Header background
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, COLOR_HEADER_BG);

  // Title
  tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.print("AI Pocket");

  // WiFi indicator
  int rssi = WiFi.RSSI();
  const char* wifiIcon = rssi > -60 ? "[|||]" : rssi > -75 ? "[|| ]" : "[|  ]";
  tft.setCursor(SCREEN_WIDTH - 35, 5);
  tft.print(wifiIcon);

  // Separator line
  tft.drawLine(0, 30, SCREEN_WIDTH, 30, COLOR_BORDER);
}

void drawMessageArea() {
  int contentY = 35;
  int contentHeight = SCREEN_HEIGHT - 35 - 25;  // Minus header and status bar

  // User message section
  tft.setTextColor(COLOR_USER_MSG, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(3, contentY);
  tft.print("YOU:");

  wrapText(lastUserMessage, 3, contentY + 12, SCREEN_WIDTH - 6, 4, COLOR_TEXT);

  // Separator
  int aiY = contentY + 65;
  tft.drawLine(5, aiY - 5, SCREEN_WIDTH - 5, aiY - 5, COLOR_BORDER);

  // AI response section
  tft.setTextColor(COLOR_AI_MSG, COLOR_BG);
  tft.setCursor(3, aiY);
  tft.print("AI:");

  wrapText(lastAiResponse, 3, aiY + 12, SCREEN_WIDTH - 6, 10, COLOR_TEXT);
}

void drawStatusBar() {
  int barY = SCREEN_HEIGHT - 22;

  // Status bar background
  tft.fillRect(0, barY, SCREEN_WIDTH, 22, COLOR_HEADER_BG);

  // Status text
  tft.setTextColor(COLOR_HEADER_TEXT, COLOR_HEADER_BG);
  tft.setTextSize(1);
  tft.setCursor(3, barY + 4);
  tft.print(statusText);
}

void wrapText(const char* text, int x, int startY, int maxWidth, int maxLines, uint16_t color) {
  tft.setTextColor(color, COLOR_BG);
  tft.setTextSize(1);

  int cursorX = x;
  int cursorY = startY;
  int lineCount = 0;
  int charWidth = 6;  // Font 1 character width
  int lineHeight = 10;

  int len = strlen(text);
  int lineStart = 0;

  while (lineStart < len && lineCount < maxLines) {
    int lineEnd = lineStart;
    int linePixelWidth = 0;

    // Find how many characters fit in this line
    while (lineEnd < len && linePixelWidth < maxWidth) {
      if (text[lineEnd] == '\n') {
        lineEnd++;
        break;
      }
      linePixelWidth += charWidth;
      lineEnd++;
    }

    // Back up to last space if we overflowed
    if (lineEnd < len && text[lineEnd - 1] != '\n' && text[lineEnd] != ' ') {
      int lastSpace = lineEnd - 1;
      while (lastSpace > lineStart && text[lastSpace] != ' ') {
        lastSpace--;
      }
      if (lastSpace > lineStart) {
        lineEnd = lastSpace + 1;
      }
    }

    // Print the line
    int lineLen = lineEnd - lineStart;
    if (lineLen > 0) {
      char lineBuf[128];
      int copyLen = lineLen > 127 ? 127 : lineLen;
      strncpy(lineBuf, text + lineStart, copyLen);
      lineBuf[copyLen] = '\0';

      tft.setCursor(cursorX, cursorY);
      tft.print(lineBuf);
    }

    cursorY += lineHeight;
    lineCount++;

    // Skip spaces at start of next line
    lineStart = lineEnd;
    while (lineStart < len && text[lineStart] == ' ') {
      lineStart++;
    }

    // Handle explicit newlines
    if (lineStart < len && text[lineStart] == '\n') {
      lineStart++;
    }
  }

  // If text was truncated, show ellipsis
  if (lineStart < len && lineCount >= maxLines) {
    tft.setCursor(cursorX + maxWidth - 18, cursorY - lineHeight);
    tft.print("...");
  }
}

void setStatus(const char* msg, uint16_t color) {
  strncpy(statusText, msg, sizeof(statusText) - 1);
  statusText[sizeof(statusText) - 1] = '\0';

  // Update status bar immediately
  int barY = SCREEN_HEIGHT - 22;
  tft.fillRect(0, barY, SCREEN_WIDTH, 22, COLOR_HEADER_BG);
  tft.setTextColor(color, COLOR_HEADER_BG);
  tft.setTextSize(1);
  tft.setCursor(3, barY + 4);
  tft.print(statusText);
}
