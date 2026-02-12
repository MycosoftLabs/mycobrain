#!/usr/bin/env python3
"""
MycoBrain Serial Monitor
Connects to MycoBrain Gateway and monitors output
"""
import sys
import time
import json

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Installing pyserial...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pyserial"])
    import serial
    import serial.tools.list_ports


def find_mycobrain():
    """Find MycoBrain (ESP32-S3) port"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # ESP32-S3 VID:PID = 303A:1001
        if port.vid == 0x303A and port.pid == 0x1001:
            print(f"[+] Found MycoBrain on {port.device}")
            print(f"    VID:PID = {hex(port.vid)}:{hex(port.pid)}")
            print(f"    Serial: {port.serial_number}")
            return port.device
    return None


def monitor(port=None, baud=115200):
    """Monitor MycoBrain serial output"""
    
    if not port:
        port = find_mycobrain()
        if not port:
            print("[!] MycoBrain not found. Available ports:")
            for p in serial.tools.list_ports.comports():
                print(f"    {p.device}: {p.description}")
            return
    
    print(f"\n{'='*60}")
    print(f"  MycoBrain Gateway Monitor - {port}")
    print(f"{'='*60}")
    print("  Press Ctrl+C to exit")
    print(f"{'='*60}\n")
    
    try:
        ser = serial.Serial(port, baud, timeout=1)
        time.sleep(1)  # Wait for boot
        
        # Read initial output
        ser.reset_input_buffer()
        
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    # Try to parse JSON for pretty printing
                    try:
                        data = json.loads(line)
                        timestamp = time.strftime("%H:%M:%S")
                        
                        if "lora_init" in data:
                            status = "✓" if data["lora_init"] == "ok" else "✗"
                            print(f"[{timestamp}] LoRa Init: {status}")
                        elif "side" in data:
                            print(f"[{timestamp}] 🍄 MycoBrain Gateway READY")
                            print(f"           Side: {data.get('side')}")
                            print(f"           MDP Version: {data.get('mdp')}")
                            print(f"           Status: {data.get('status')}")
                        elif "src" in data:
                            # Message from remote side
                            src = {0xA0: "Gateway", 0xA1: "Side-A", 0xB1: "Side-B"}.get(data.get('src', 0), hex(data.get('src', 0)))
                            dst = {0xA0: "Gateway", 0xA1: "Side-A", 0xB1: "Side-B"}.get(data.get('dst', 0), hex(data.get('dst', 0)))
                            print(f"[{timestamp}] 📡 MSG: {src} → {dst} | seq={data.get('seq')} type={data.get('type')}")
                        elif "sent" in data:
                            print(f"[{timestamp}] 📤 Sent command (seq={data.get('seq')})")
                        elif "error" in data:
                            print(f"[{timestamp}] ⚠️  Error: {data.get('error')}")
                        else:
                            print(f"[{timestamp}] {json.dumps(data)}")
                    except json.JSONDecodeError:
                        # Not JSON, print raw
                        print(f"[RAW] {line}")
            
            time.sleep(0.01)
            
    except serial.SerialException as e:
        print(f"[!] Serial error: {e}")
    except KeyboardInterrupt:
        print("\n[*] Monitoring stopped")
    finally:
        if 'ser' in locals():
            ser.close()


def send_command(port, cmd_id, dst=0xA1, data=None):
    """Send a command to MycoBrain"""
    cmd = {"cmd": cmd_id, "dst": dst}
    if data:
        cmd["data"] = data
    
    ser = serial.Serial(port, 115200, timeout=1)
    time.sleep(0.5)
    
    cmd_json = json.dumps(cmd) + "\n"
    print(f"[TX] {cmd_json.strip()}")
    ser.write(cmd_json.encode())
    
    time.sleep(0.5)
    response = ser.readline().decode('utf-8', errors='replace').strip()
    if response:
        print(f"[RX] {response}")
    
    ser.close()


if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "find":
            find_mycobrain()
        elif sys.argv[1] == "send":
            port = find_mycobrain()
            if port and len(sys.argv) > 2:
                cmd_id = int(sys.argv[2])
                send_command(port, cmd_id)
        else:
            monitor(sys.argv[1])
    else:
        monitor()
