#!/usr/bin/env python3
"""Telegram -> ESP32 bridge: fast API for chat, Hermes CLI for computer access."""
import os, sys, json, time, urllib.request, subprocess

LOG = "/tmp/telegram_bridge.log"

def log(msg):
    line = f"{time.strftime('%H:%M:%S')} | {msg}\n"
    with open(LOG, "a") as f:
        f.write(line)
    sys.stdout.write(line)
    sys.stdout.flush()

# Load hermes .env for API keys
hermes_env = os.path.expanduser("~/.hermes/.env")
if os.path.exists(hermes_env):
    with open(hermes_env) as f:
        for line in f:
            line = line.strip()
            if "=" in line and not line.startswith("#"):
                k, v = line.split("=", 1)
                os.environ[k] = v

# Load local .env (takes precedence)
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

# Xiaomi MiMo API config
MIMO_API_KEY = os.environ.get("XIAOMI_API_KEY", "")
MIMO_BASE_URL = "https://token-plan-sgp.xiaomimimo.com/v1"
MIMO_MODEL = "mimo-v2.5-pro"

SYSTEM_PROMPT = (
    "Kamu adalah AI asisten pocket yang ramah dan helpful. "
    "Balas dalam 1-2 kalimat pendek, bahasa Indonesia santai. "
    "Jangan gunakan markdown formatting. "
    "Tulis respons langsung tanpa awalan atau penjelasan."
)

# Keywords that need computer access
COMPUTER_KEYWORDS = [
    "cek ", "check ", "folder", "file", "hardisk", "harddisk", "disk",
    "ram", "cpu", "memory", "proses", "process", "running", "jalan",
    "terminal", "command", "jalankan", "install", "hapus", "delete",
    "copy", "pindah", "rename", "buat folder", "make directory",
    "lihat", "list", "ls", "cat ", "baca", "read", "tulis", "write",
    "docker", "git", "npm", "pip", "apt", "systemctl", "service",
    "ip ", "wifi", "network", "internet", "ping", "curl", "wget",
    "python", "node", "bash", "script", "compile", "build",
    "restart", "shutdown", "reboot", "update", "upgrade",
    "size", "besar", "kapasitas", "space", "free", "sisa"
]

log(f"Bot: {TOKEN[:10]}...")
log(f"Chat: {CHAT_ID}")
log(f"ESP32: {ESP32}")
log(f"API: {MIMO_BASE_URL}")
log(f"Model: {MIMO_MODEL}")
log(f"Hermes: {HERMES_CLI}")
log("Starting hybrid AI bridge (API + Hermes)...")


def needs_computer_access(text):
    """Check if the message needs computer access."""
    lower = text.lower()
    for kw in COMPUTER_KEYWORDS:
        if kw in lower:
            return True
    return False


def send_chat_action(chat_id, action="typing"):
    """Send typing indicator to Telegram."""
    url = f"https://api.telegram.org/bot{TOKEN}/sendChatAction"
    body = json.dumps({"chat_id": chat_id, "action": action}).encode()
    try:
        req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=5)
    except Exception:
        pass


def send_esp32_thinking(user_msg):
    """Show thinking state on ESP32 display."""
    url = f"http://{ESP32}/message"
    body = json.dumps({
        "user_message": user_msg[:200],
        "ai_response": "🤔 Thinking...",
        "source": "telegram"
    }).encode()
    try:
        req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json"})
        urllib.request.urlopen(req, timeout=3)
    except Exception:
        pass


def get_ai_fast(user_text):
    """Call Xiaomi MiMo API directly for fast chat response."""
    try:
        log(f"  [API] Calling MiMo...")
        start = time.time()

        url = f"{MIMO_BASE_URL}/chat/completions"
        body = json.dumps({
            "model": MIMO_MODEL,
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": user_text}
            ],
            "max_tokens": 200,
            "temperature": 0.7
        }).encode()

        req = urllib.request.Request(url, data=body, headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {MIMO_API_KEY}"
        })

        resp = urllib.request.urlopen(req, timeout=15)
        data = json.loads(resp.read())
        elapsed = time.time() - start

        ai_text = data["choices"][0]["message"]["content"].strip()
        log(f"  [API] Response ({elapsed:.1f}s): {ai_text[:80]}...")
        return ai_text

    except urllib.error.HTTPError as e:
        log(f"  [API] Error {e.code}: {e.read().decode()[:100]}")
        return "Maaf, AI error 😅"
    except Exception as e:
        log(f"  [API] Error: {e}")
        return "Maaf, AI sedang sibuk 😅"


def get_ai_hermes(user_text):
    """Call Hermes CLI for computer access tasks."""
    prompt = (
        f"Kamu adalah AI asisten pocket yang punya akses ke komputer user. "
        f"User minta: {user_text}\n\n"
        f"Jawab dengan singkat dan langsung. "
        f"Jangan gunakan markdown formatting."
    )
    try:
        log(f"  [HERMES] Calling Hermes CLI...")
        start = time.time()

        result = subprocess.run(
            [HERMES_CLI, "-z", prompt],
            capture_output=True, text=True, timeout=60,
            env={**os.environ, "HERMES_SILENT": "1"}
        )
        elapsed = time.time() - start

        ai_text = result.stdout.strip()
        if ai_text:
            log(f"  [HERMES] Response ({elapsed:.1f}s): {ai_text[:80]}...")
            return ai_text
        else:
            log(f"  [HERMES] Empty response, stderr: {result.stderr[:100]}")
            return "Maaf, AI sedang sibuk 😅"

    except subprocess.TimeoutExpired:
        log(f"  [HERMES] Timeout (60s)")
        return "Hmm, terlalu lama mikir... coba lagi ya ⏰"
    except Exception as e:
        log(f"  [HERMES] Error: {e}")
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
                    if txt.startswith("/start"):
                        reply(cid, "🤖 AI Pocket Bot aktif!\n\nKirim pesan biasa → AI chat cepat\nKirim perintah komputer → akses ke PC kamu")
                        continue

                    # Show typing + thinking
                    send_chat_action(cid, "typing")
                    send_esp32_thinking(txt)

                    # Route: fast API for chat, Hermes for computer access
                    if needs_computer_access(txt):
                        log(f"  → Route: HERMES (computer access)")
                        ai_response = get_ai_hermes(txt)
                    else:
                        log(f"  → Route: API (fast chat)")
                        ai_response = get_ai_fast(txt)

                    # Send to ESP32
                    esp_ok = send_esp32(txt, ai_response)

                    # Reply to Telegram
                    if esp_ok:
                        reply(cid, f"🤖 {ai_response}")
                    else:
                        reply(cid, f"🤖 {ai_response}\n\n⚠️ ESP32 display tidak tersedia")
                else:
                    log(f"  Ignored (wrong chat)")
    except Exception as e:
        log(f"ERROR: {e}")
        time.sleep(5)
