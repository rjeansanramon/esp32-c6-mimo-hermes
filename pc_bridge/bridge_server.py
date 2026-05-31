#!/usr/bin/env python3
"""
AI Pocket - PC Bridge Server

This script runs on your local PC alongside Hermes Agent.
It receives AI responses from Hermes and forwards them to the ESP32-C6 display.

Two modes of operation:
  1. Webhook mode: Hermes sends responses to this server, which forwards to ESP32
  2. Telegram intercept mode: Monitors Telegram messages and forwards AI responses

Usage:
  python bridge_server.py --esp32-ip 192.168.1.100

Environment Variables:
  ESP32_IP          - IP address of the ESP32-C6 display
  BRIDGE_PORT       - Port for this bridge server (default: 8765)
  HERMES_WEBHOOK    - Enable Hermes webhook endpoint (default: true)
  TELEGRAM_MODE     - Enable Telegram monitoring mode (default: false)
"""

import os
import sys
import json
import time
import argparse
import logging
import threading
from datetime import datetime
from typing import Optional, Dict, Any

try:
    from flask import Flask, request, jsonify
    import requests
except ImportError:
    print("[ERROR] Required packages not installed.")
    print("[INFO] Install with: pip install flask requests")
    sys.exit(1)

# ============== CONFIGURATION ==============
ESP32_IP = os.getenv("ESP32_IP", "192.168.1.100")
ESP32_PORT = int(os.getenv("ESP32_PORT", "80"))
BRIDGE_PORT = int(os.getenv("BRIDGE_PORT", "8765"))
HERMES_WEBHOOK = os.getenv("HERMES_WEBHOOK", "true").lower() == "true"
TELEGRAM_MODE = os.getenv("TELEGRAM_MODE", "false").lower() == "true"

# Telegram config (optional - for intercept mode)
TELEGRAM_BOT_TOKEN = os.getenv("TELEGRAM_BOT_TOKEN", "")
TELEGRAM_CHAT_ID = os.getenv("TELEGRAM_CHAT_ID", "")

# ============== LOGGING ==============
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(levelname)-8s | %(message)s",
    datefmt="%H:%M:%S"
)
logger = logging.getLogger("AI-Pocket-Bridge")

# ============== FLASK APP ==============
app = Flask(__name__)

# State tracking
last_ai_response: Dict[str, Any] = {
    "user_message": "",
    "ai_response": "",
    "timestamp": "",
    "source": "none"
}
message_queue: list = []
queue_lock = threading.Lock()


# ============== ESP32 COMMUNICATION ==============
def send_to_esp32(user_msg: str, ai_msg: str, source: str = "hermes") -> bool:
    """Send a message to the ESP32-C6 display."""
    url = f"http://{ESP32_IP}:{ESP32_PORT}/message"

    payload = {
        "user_message": user_msg[:500],  # Limit to 500 chars
        "ai_response": ai_msg[:500],
        "source": source,
        "timestamp": datetime.now().isoformat()
    }

    try:
        response = requests.post(
            url,
            json=payload,
            timeout=5,
            headers={"Content-Type": "application/json"}
        )

        if response.status_code == 200:
            logger.info(f"[ESP32] Message sent successfully to {ESP32_IP}")
            return True
        else:
            logger.warning(f"[ESP32] Failed: HTTP {response.status_code} - {response.text}")
            return False

    except requests.exceptions.ConnectTimeout:
        logger.error(f"[ESP32] Connection timeout to {ESP32_IP}:{ESP32_PORT}")
        return False
    except requests.exceptions.ConnectionError:
        logger.error(f"[ESP32] Connection refused to {ESP32_IP}:{ESP32_PORT}")
        return False
    except Exception as e:
        logger.error(f"[ESP32] Error: {e}")
        return False


def check_esp32_status() -> Optional[Dict]:
    """Check if the ESP32 is reachable and get its status."""
    url = f"http://{ESP32_IP}:{ESP32_PORT}/status"

    try:
        response = requests.get(url, timeout=3)
        if response.status_code == 200:
            return response.json()
        return None
    except Exception:
        return None


# ============== HERMES WEBHOOK ENDPOINTS ==============
@app.route("/", methods=["GET"])
def index():
    """Bridge status page."""
    esp32_status = check_esp32_status()

    html = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>AI Pocket Bridge</title>
        <style>
            body {{ font-family: 'Segoe UI', Arial, sans-serif; 
                    background: #0f0f1e; color: #e0e0e0; 
                    max-width: 700px; margin: 0 auto; padding: 20px; }}
            h1 {{ color: #4ecca3; border-bottom: 2px solid #4ecca3; padding-bottom: 10px; }}
            .card {{ background: #1a1a3e; border-radius: 12px; padding: 20px; margin: 15px 0; }}
            .label {{ color: #888; font-size: 12px; text-transform: uppercase; letter-spacing: 1px; }}
            .value {{ color: #fff; font-size: 16px; margin: 5px 0 15px 0; }}
            .status-ok {{ color: #4ecca3; }}
            .status-err {{ color: #ff4757; }}
            .endpoint {{ background: #0f3460; padding: 10px; border-radius: 6px; 
                         font-family: monospace; font-size: 13px; margin: 5px 0; }}
            pre {{ background: #0f0f2e; padding: 15px; border-radius: 8px; 
                   overflow-x: auto; font-size: 13px; }}
        </style>
    </head>
    <body>
        <h1>AI Pocket Bridge</h1>
        
        <div class="card">
            <div class="label">Bridge Status</div>
            <div class="value status-ok">Running on port {BRIDGE_PORT}</div>
            
            <div class="label">ESP32-C6 Display</div>
            <div class="value">{'<span class="status-ok">Connected</span>' if esp32_status else '<span class="status-err">Unreachable</span>'}</div>
            {'<div class="label">ESP32 IP</div><div class="value">' + esp32_status.get('ip', 'N/A') + '</div>' if esp32_status else ''}
            {'<div class="label">WiFi RSSI</div><div class="value">' + str(esp32_status.get('wifi_rssi', 'N/A')) + ' dBm</div>' if esp32_status else ''}
            
            <div class="label">Target Address</div>
            <div class="value">http://{ESP32_IP}:{ESP32_PORT}</div>
        </div>
        
        <div class="card">
            <div class="label">Hermes Webhook Endpoint</div>
            <div class="endpoint">POST http://localhost:{BRIDGE_PORT}/hermes/webhook</div>
            
            <div class="label">Direct Display Endpoint</div>
            <div class="endpoint">POST http://localhost:{BRIDGE_PORT}/display/message</div>
            
            <div class="label">ESP32 Status Check</div>
            <div class="endpoint">GET http://localhost:{BRIDGE_PORT}/esp32/status</div>
        </div>
        
        <div class="card">
            <div class="label">Last AI Response</div>
            <pre>{json.dumps(last_ai_response, indent=2)}</pre>
        </div>
    </body>
    </html>
    """
    return html


@app.route("/hermes/webhook", methods=["POST"])
def hermes_webhook():
    """
    Webhook endpoint for Hermes Agent.
    
    Hermes can send AI responses here via its webhook skill.
    Expected JSON payload:
    {
        "user_message": "What the user asked",
        "ai_response": "The AI's response",
        "session_id": "optional-session-id",
        "timestamp": "2026-05-31T12:00:00Z"
    }
    """
    try:
        data = request.get_json() or request.form.to_dict()
        logger.info(f"[HERMES] Webhook received: {json.dumps(data)[:200]}...")

        user_msg = data.get("user_message", data.get("prompt", ""))
        ai_msg = data.get("ai_response", data.get("response", data.get("text", "")))
        session_id = data.get("session_id", "unknown")

        if not ai_msg:
            return jsonify({"status": "error", "message": "No AI response provided"}), 400

        # Update state
        global last_ai_response
        last_ai_response = {
            "user_message": user_msg,
            "ai_response": ai_msg,
            "session_id": session_id,
            "timestamp": datetime.now().isoformat(),
            "source": "hermes_webhook"
        }

        # Forward to ESP32
        success = send_to_esp32(user_msg, ai_msg, "hermes")

        if success:
            return jsonify({
                "status": "ok",
                "message": "Forwarded to ESP32 display",
                "esp32_ip": ESP32_IP
            })
        else:
            # Queue for retry
            with queue_lock:
                message_queue.append({
                    "user_message": user_msg,
                    "ai_response": ai_msg,
                    "timestamp": datetime.now().isoformat()
                })
            return jsonify({
                "status": "queued",
                "message": "ESP32 unreachable, queued for retry",
                "esp32_ip": ESP32_IP
            }), 202

    except Exception as e:
        logger.error(f"[HERMES] Webhook error: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


@app.route("/display/message", methods=["POST"])
def direct_display():
    """
    Direct endpoint to send a message to the ESP32 display.
    
    JSON payload:
    {
        "user_message": "optional user prompt",
        "ai_response": "the message to display"
    }
    """
    try:
        data = request.get_json() or {}

        user_msg = data.get("user_message", "Direct message")
        ai_msg = data.get("ai_response", data.get("message", ""))

        if not ai_msg:
            return jsonify({"status": "error", "message": "No message provided"}), 400

        success = send_to_esp32(user_msg, ai_msg, "direct")

        return jsonify({
            "status": "ok" if success else "failed",
            "esp32_reachable": success,
            "esp32_ip": ESP32_IP
        })

    except Exception as e:
        logger.error(f"[DISPLAY] Error: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500


@app.route("/esp32/status", methods=["GET"])
def esp32_status():
    """Get the current status of the ESP32-C6 display."""
    status = check_esp32_status()

    if status:
        return jsonify({
            "status": "ok",
            "esp32_connected": True,
            "esp32_info": status
        })
    else:
        return jsonify({
            "status": "error",
            "esp32_connected": False,
            "message": f"Cannot reach ESP32 at {ESP32_IP}:{ESP32_PORT}"
        }), 503


@app.route("/esp32/led", methods=["POST"])
def esp32_led():
    """Control the ESP32 RGB LED."""
    try:
        data = request.get_json() or {}
        state = data.get("state", "on")

        url = f"http://{ESP32_IP}:{ESP32_PORT}/led"
        response = requests.post(url, json={"state": state}, timeout=3)

        return jsonify({
            "status": "ok",
            "led_state": state,
            "esp32_response": response.json() if response.status_code == 200 else None
        })

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500


# ============== TELEGRAM MODE (Optional) ==============
def telegram_polling_loop():
    """Optional: Poll Telegram for messages and forward AI responses to ESP32."""
    if not TELEGRAM_BOT_TOKEN or not TELEGRAM_CHAT_ID:
        logger.warning("[TELEGRAM] Mode enabled but token/chat_id not configured")
        return

    logger.info("[TELEGRAM] Polling started")
    offset = 0

    while True:
        try:
            url = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/getUpdates"
            params = {"offset": offset, "limit": 10}

            response = requests.get(url, params=params, timeout=30)
            data = response.json()

            if data.get("ok") and data.get("result"):
                for update in data["result"]:
                    offset = update["update_id"] + 1

                    message = update.get("message", {})
                    chat = message.get("chat", {})
                    chat_id = str(chat.get("id", ""))

                    if chat_id == TELEGRAM_CHAT_ID:
                        text = message.get("text", "")
                        logger.info(f"[TELEGRAM] Message from user: {text[:100]}")

                        # This is a user message - the AI response will come back via webhook
                        # or you can integrate with Hermes here

            time.sleep(1)

        except Exception as e:
            logger.error(f"[TELEGRAM] Polling error: {e}")
            time.sleep(5)


# ============== RETRY QUEUE ==============
def retry_queue_loop():
    """Background thread to retry failed messages."""
    while True:
        time.sleep(10)

        with queue_lock:
            if message_queue:
                msg = message_queue[0]
                logger.info(f"[RETRY] Attempting to send queued message...")

                success = send_to_esp32(
                    msg["user_message"],
                    msg["ai_response"],
                    "hermes_retry"
                )

                if success:
                    message_queue.pop(0)
                    logger.info(f"[RETRY] Queued message sent successfully")
                else:
                    logger.warning(f"[RETRY] Still unreachable, will retry later")


# ============== MAIN ==============
def main():
    parser = argparse.ArgumentParser(description="AI Pocket - PC Bridge Server")
    parser.add_argument("--esp32-ip", default=ESP32_IP, help="ESP32-C6 IP address")
    parser.add_argument("--esp32-port", type=int, default=ESP32_PORT, help="ESP32 web server port")
    parser.add_argument("--port", type=int, default=BRIDGE_PORT, help="Bridge server port")
    parser.add_argument("--no-webhook", action="store_true", help="Disable Hermes webhook endpoint")
    args = parser.parse_args()

    global ESP32_IP, ESP32_PORT, BRIDGE_PORT, HERMES_WEBHOOK

    ESP32_IP = args.esp32_ip
    ESP32_PORT = args.esp32_port
    BRIDGE_PORT = args.port
    HERMES_WEBHOOK = not args.no_webhook

    print("""
    ╔══════════════════════════════════════════════════════════╗
    ║                    AI Pocket Bridge                       ║
    ║              Hermes Agent → ESP32-C6 Display              ║
    ╚══════════════════════════════════════════════════════════╝
    """)
    logger.info(f"ESP32 Target: http://{ESP32_IP}:{ESP32_PORT}")
    logger.info(f"Bridge Server: http://0.0.0.0:{BRIDGE_PORT}")
    logger.info(f"Hermes Webhook: {'Enabled' if HERMES_WEBHOOK else 'Disabled'}")
    logger.info(f"Telegram Mode: {'Enabled' if TELEGRAM_MODE else 'Disabled'}")

    # Check ESP32 connectivity
    logger.info("Checking ESP32 connectivity...")
    status = check_esp32_status()
    if status:
        logger.info(f"ESP32 reachable! IP: {status.get('ip', 'N/A')}")
    else:
        logger.warning(f"ESP32 not reachable at {ESP32_IP}:{ESP32_PORT}")
        logger.info("Make sure the ESP32 is connected to WiFi and the IP is correct")

    # Start background threads
    retry_thread = threading.Thread(target=retry_queue_loop, daemon=True)
    retry_thread.start()

    if TELEGRAM_MODE:
        tg_thread = threading.Thread(target=telegram_polling_loop, daemon=True)
        tg_thread.start()

    # Start Flask server
    logger.info("Starting bridge server...")
    app.run(host="0.0.0.0", port=BRIDGE_PORT, debug=False)


if __name__ == "__main__":
    main()
