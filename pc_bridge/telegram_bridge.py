#!/usr/bin/env python3
"""Telegram -> ESP32 bridge with REAL AI responses via Hermes CLI."""
import os, sys, json, time, urllib.request, subprocess

LOG = "/tmp/telegram_bridge.log"

def log(msg):
    line = f"{time.strftime('%H:%M:%S')} | {msg}\n"
    with open(LOG, "a") as f:
        f.write(line)
    sys.stdout.write(line)
    sys.stdout.flush()

# Load .env
env_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".env")
with open(env_path) as f:
    for line in f:
        line = line.strip()
        if "=" in line and not line.startswith("#"):
            k, v = line.split("=", 1)
            os.environ[k] = v

# Clear log on start
open(LOG, "w").close()

TOKEN = os.environ.get("TELEGRAM_BOT_TOKEN", "")
CHAT_ID = os.environ.get("TELEGRAM_CHAT_ID", "")
ESP32 = os.environ.get("ESP32_IP", "192.168.90.37")
HERMES_CLI = os.environ.get("HERMES_CLI", "/home/alpha01/.local/bin/hermes")

log(f"Bot: {TOKEN[:10]}...")
log(f"Chat: {CHAT_ID}")
log(f"ESP32: {ESP32}")
log(f"Hermes: {HERMES_CLI}")
log("Starting AI-powered bridge...")


def get_ai_response(user_text):
    """Call Hermes CLI to get a real AI response."""
    prompt = (
        f"Kamu adalah AI asisten pocket yang ramah dan helpful. "
        f"Balas dalam 1-2 kalimat pendek, bahasa Indonesia santai. "
        f"Jangan gunakan markdown formatting. "
        f"Pesan user: {user_text}"
    )
    try:
        log(f"  Calling Hermes AI...")
        result = subprocess.run(
            [HERMES_CLI, "-z", prompt],
            capture_output=True, text=True, timeout=30,
            env={**os.environ, "HERMES_SILENT": "1"}
        )
        ai_text = result.stdout.strip()
        if ai_text:
            log(f"  AI response: {ai_text[:80]}...")
            return ai_text
        else:
            log(f"  AI empty response, stderr: {result.stderr[:100]}")
            return "Maaf, AI sedang sibuk 😅"
    except subprocess.TimeoutExpired:
        log(f"  AI timeout (30s)")
        return "Hmm, terlalu lama mikir... coba lagi ya ⏰"
    except Exception as e:
        log(f"  AI error: {e}")
        return f"Error: {str(e)[:50]}"


def send_esp32(user_msg, ai_msg):
    """Send message to ESP32 display."""
    url = f"http://{ESP32}/message"
    body = json.dumps({
        "user_message": user_msg[:200],
        "ai_response": ai_msg[:200],
        "source": "telegram"
    }).encode()
    try:
        req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
        resp = urllib.request.urlopen(req, timeout=5)
        r = json.loads(resp.read())
        log(f"  ESP32 OK: {r}")
        return True
    except Exception as e:
        log(f"  ESP32 ERR: {e}")
        return False


def reply(chat_id, text):
    """Send reply to Telegram."""
    url = f"https://api.telegram.org/bot{TOKEN}/sendMessage"
    # Split long messages (Telegram max ~4096 chars)
    if len(text) > 4000:
        text = text[:4000] + "..."
    body = json.dumps({"chat_id": chat_id, "text": text}).encode()
    try:
        req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=10)
        log(f"  Reply sent ({len(text)} chars)")
    except Exception as e:
        log(f"  Reply ERR: {e}")


# Main polling loop
offset = 0
log("Polling Telegram...")
while True:
    try:
        url = f"https://api.telegram.org/bot{TOKEN}/getUpdates?offset={offset}&timeout=30"
        req = urllib.request.urlopen(url, timeout=35)
        data = json.loads(req.read())

        if data.get("ok") and data.get("result"):
            for u in data["result"]:
                offset = u["update_id"] + 1
                msg = u.get("message", {})
                cid = str(msg.get("chat", {}).get("id", ""))
                txt = msg.get("text", "")

                log(f"MSG chat={cid} text={txt[:50]}")

                if cid == CHAT_ID:
                    # Handle /start command
                    if txt.startswith("/start"):
                        reply(cid, "🤖 AI Pocket Bot aktif!\n\nKirim pesan dan AI akan merespons + tampil di ESP32 display.")
                        continue

                    # Get real AI response
                    ai_response = get_ai_response(txt)

                    # Send to ESP32 display
                    esp_ok = send_esp32(txt, ai_response)

                    # Reply to Telegram with AI response
                    if esp_ok:
                        reply(cid, f"🤖 {ai_response}")
                    else:
                        reply(cid, f"🤖 {ai_response}\n\n⚠️ ESP32 display tidak tersedia")
                else:
                    log(f"  Ignored (wrong chat)")
    except Exception as e:
        log(f"ERROR: {e}")
        time.sleep(5)
