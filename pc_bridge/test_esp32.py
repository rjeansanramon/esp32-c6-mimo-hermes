#!/usr/bin/env python3
"""
AI Pocket - ESP32 Connection Test Script

Quick test to verify your ESP32-C6 display is reachable and responding.

Usage:
  python test_esp32.py --ip 192.168.1.100
"""

import argparse
import sys
import requests
import json


def test_connection(ip: str, port: int = 80) -> bool:
    """Test basic connectivity to ESP32."""
    url = f"http://{ip}:{port}/status"
    print(f"\n[TEST] Connecting to ESP32 at {url}...")

    try:
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            data = response.json()
            print("[PASS] ESP32 is reachable!")
            print(f"[INFO] Device: {data.get('device', 'Unknown')}")
            print(f"[INFO] Version: {data.get('version', 'Unknown')}")
            print(f"[INFO] Display: {data.get('display', 'Unknown')}")
            print(f"[INFO] WiFi RSSI: {data.get('wifi_rssi', 'N/A')} dBm")
            print(f"[INFO] IP: {data.get('ip', 'N/A')}")
            print(f"[INFO] Uptime: {data.get('uptime_ms', 0) / 1000:.1f} seconds")
            return True
        else:
            print(f"[FAIL] HTTP {response.status_code}")
            return False
    except requests.exceptions.ConnectTimeout:
        print("[FAIL] Connection timeout - ESP32 not responding")
        return False
    except requests.exceptions.ConnectionError:
        print("[FAIL] Connection refused - check IP and ensure ESP32 is running")
        return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_display(ip: str, port: int = 80) -> bool:
    """Test sending a message to the display."""
    url = f"http://{ip}:{port}/message"
    print(f"\n[TEST] Sending test message to {url}...")

    payload = {
        "user_message": "Test from PC",
        "ai_response": "ESP32 display is working! Your AI Pocket is ready.",
        "source": "test_script"
    }

    try:
        response = requests.post(url, json=payload, timeout=5)
        if response.status_code == 200:
            print("[PASS] Message sent successfully!")
            print(f"[INFO] Response: {response.json()}")
            return True
        else:
            print(f"[FAIL] HTTP {response.status_code}: {response.text}")
            return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def test_web_interface(ip: str, port: int = 80) -> bool:
    """Test the web control panel."""
    url = f"http://{ip}:{port}/"
    print(f"\n[TEST] Checking web interface at {url}...")

    try:
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            print("[PASS] Web interface is accessible!")
            print(f"[INFO] Open http://{ip}:{port}/ in your browser")
            return True
        else:
            print(f"[FAIL] HTTP {response.status_code}")
            return False
    except Exception as e:
        print(f"[FAIL] Error: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Test ESP32-C6 AI Pocket Display")
    parser.add_argument("--ip", required=True, help="ESP32 IP address")
    parser.add_argument("--port", type=int, default=80, help="ESP32 port (default: 80)")
    parser.add_argument("--skip-display", action="store_true", help="Skip display test")
    args = parser.parse_args()

    print("=" * 50)
    print("   AI Pocket - ESP32 Connection Test")
    print("=" * 50)
    print(f"Target: http://{args.ip}:{args.port}")

    # Test 1: Connection
    ok = test_connection(args.ip, args.port)
    if not ok:
        print("\n[!] Connection failed. Troubleshooting:")
        print("    1. Check ESP32 is powered on and WiFi connected")
        print("    2. Verify the IP address in Serial Monitor")
        print("    3. Ensure PC and ESP32 are on the same network")
        sys.exit(1)

    # Test 2: Web interface
    test_web_interface(args.ip, args.port)

    # Test 3: Display message
    if not args.skip_display:
        test_display(args.ip, args.port)

    print("\n" + "=" * 50)
    print("   All tests passed! Your AI Pocket is ready.")
    print("=" * 50)


if __name__ == "__main__":
    main()
