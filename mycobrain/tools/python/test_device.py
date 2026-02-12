#!/usr/bin/env python3
"""
MycoBrain Device Test Script
Tests communication with ESP32-S3 on COM5
"""
import sys
import time

try:
    import serial
except ImportError:
    print("Installing pyserial...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pyserial"])
    import serial

def test_device(port="COM5", baud=115200):
    print(f"\n{'='*60}")
    print(f"  MycoBrain Device Test - {port}")
    print(f"{'='*60}\n")
    
    try:
        # Open serial connection
        print(f"[*] Opening {port} at {baud} baud...")
        ser = serial.Serial(port, baud, timeout=2)
        time.sleep(0.5)  # Wait for device to reset
        
        # Flush any existing data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        print(f"[+] Connected to {port}")
        print(f"[*] Waiting for device output...\n")
        
        # Send newline to trigger any response
        ser.write(b'\n')
        time.sleep(0.5)
        
        # Read any available data
        data = b""
        start = time.time()
        while time.time() - start < 3:
            if ser.in_waiting:
                chunk = ser.read(ser.in_waiting)
                data += chunk
                print(f"[RX] {chunk}")
            time.sleep(0.1)
        
        if data:
            print(f"\n[+] Received {len(data)} bytes:")
            try:
                decoded = data.decode('utf-8', errors='replace')
                print(decoded)
            except:
                print(f"    Raw: {data.hex()}")
        else:
            print("[!] No data received - device may need firmware")
            print("    The ESP32-S3 appears to be in bootloader or blank state")
        
        # Try sending a simple JSON command
        print("\n[*] Sending test command...")
        test_cmd = b'{"cmd":0}\n'  # CMD_NOP = 0
        ser.write(test_cmd)
        time.sleep(0.5)
        
        response = ser.read(500)
        if response:
            print(f"[+] Response: {response.decode('utf-8', errors='replace')}")
        else:
            print("[!] No response to command")
        
        ser.close()
        print(f"\n[*] Port {port} closed")
        
        return data or response
        
    except serial.SerialException as e:
        print(f"[!] Serial error: {e}")
        return None
    except Exception as e:
        print(f"[!] Error: {e}")
        return None

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    test_device(port)
    
    print("\n" + "="*60)
    print("  NEXT STEPS:")
    print("="*60)
    print("""
If no response was received, the device needs firmware.
To flash the MycoBrain Gateway firmware:

1. Navigate to firmware directory:
   cd C:\\Users\\admin2\\Desktop\\MYCOSOFT\\CODE\\MYCOBRAIN\\mycobrain\\firmware\\gateway

2. Build and upload:
   pio run -t upload --upload-port COM5

3. Open serial monitor to verify:
   pio device monitor -b 115200 -p COM5
""")
