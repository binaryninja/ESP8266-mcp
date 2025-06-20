#!/usr/bin/env python3
import serial
import time
import sys

def test_serial_connection(port='/dev/ttyUSB0', baud_rates=[74880, 115200, 9600]):
    """Test serial connection at different baud rates"""

    for baud in baud_rates:
        print(f"\n--- Testing {port} at {baud} baud ---")
        try:
            ser = serial.Serial(port, baud, timeout=2)
            print(f"Connected to {port} at {baud} baud")

            # For 74880 baud (ESP8266 boot messages), read for longer
            read_time = 10 if baud == 74880 else 5
            print(f"Reading for {read_time} seconds... Press Ctrl+C to trigger reset")

            # Read for specified time
            start_time = time.time()
            while time.time() - start_time < read_time:
                if ser.in_waiting:
                    try:
                        data = ser.read(ser.in_waiting)
                        # Try to decode as UTF-8, fall back to raw bytes
                        try:
                            text = data.decode('utf-8', errors='ignore')
                            if text.strip():
                                # Clean up the output for better readability
                                clean_text = text.replace('\r\n', '\n').replace('\r', '\n')
                                for line in clean_text.split('\n'):
                                    if line.strip():
                                        print(f"[{baud}] {line}")
                        except:
                            print(f"[{baud}] Raw: {data.hex()}")
                    except Exception as e:
                        print(f"Read error: {e}")
                        break
                time.sleep(0.1)
        except KeyboardInterrupt:
            print(f"\nInterrupted - you can now reset the ESP8266 to see boot messages")
            if 'ser' in locals():
                # Continue reading after interrupt to catch reset
                try:
                    print("Waiting for reset... (Press Ctrl+C again to stop)")
                    while True:
                        if ser.in_waiting:
                            data = ser.read(ser.in_waiting)
                            try:
                                text = data.decode('utf-8', errors='ignore')
                                if text.strip():
                                    clean_text = text.replace('\r\n', '\n').replace('\r', '\n')
                                    for line in clean_text.split('\n'):
                                        if line.strip():
                                            print(f"[RESET-{baud}] {line}")
                            except:
                                print(f"[RESET-{baud}] Raw: {data.hex()}")
                        time.sleep(0.1)
                except KeyboardInterrupt:
                    print("\nStopping...")
                    break

            ser.close()

        except Exception as e:
            print(f"Failed to connect at {baud}: {e}")

    print("\n--- Test complete ---")

if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0'
    test_serial_connection(port)
