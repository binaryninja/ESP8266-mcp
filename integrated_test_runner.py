#!/usr/bin/env python3
"""
ESP8266 MCP Integrated Test Runner
Combines ESP8266 serial console logging with client test execution
for real-time correlation of server logs and client actions.
"""

import threading
import time
import sys
import json
import subprocess
import signal
import os
from datetime import datetime
from dataclasses import dataclass
from typing import List, Optional, Dict, Any
import queue
import serial
import argparse

@dataclass
class LogEntry:
    timestamp: datetime
    source: str  # 'ESP8266' or 'CLIENT'
    level: str   # 'INFO', 'ERROR', 'DEBUG', 'TEST'
    message: str
    raw_data: Optional[str] = None

class SerialMonitor:
    """Monitor ESP8266 serial output in a separate thread"""

    def __init__(self, port: str, baud_rate: int = 74880, log_queue: queue.Queue = None):
        self.port = port
        self.baud_rate = baud_rate
        self.log_queue = log_queue
        self.running = False
        self.serial_conn = None

    def start(self):
        """Start serial monitoring thread"""
        self.running = True
        self.thread = threading.Thread(target=self._monitor_serial, daemon=True)
        self.thread.start()

    def stop(self):
        """Stop serial monitoring"""
        self.running = False
        if self.serial_conn:
            self.serial_conn.close()

    def _monitor_serial(self):
        """Serial monitoring loop"""
        try:
            self.serial_conn = serial.Serial(self.port, self.baud_rate, timeout=1)
            print(f"📡 Serial monitor connected to {self.port} at {self.baud_rate} baud")

            while self.running:
                if self.serial_conn.in_waiting:
                    try:
                        data = self.serial_conn.read(self.serial_conn.in_waiting)
                        text = data.decode('utf-8', errors='ignore')

                        if text.strip():
                            # Process each line separately
                            for line in text.replace('\r\n', '\n').replace('\r', '\n').split('\n'):
                                if line.strip():
                                    self._process_serial_line(line.strip())
                    except Exception as e:
                        log_entry = LogEntry(
                            timestamp=datetime.now(),
                            source='ESP8266',
                            level='ERROR',
                            message=f"Serial read error: {e}",
                            raw_data=str(data) if 'data' in locals() else None
                        )
                        if self.log_queue:
                            self.log_queue.put(log_entry)

                time.sleep(0.01)  # 10ms polling

        except Exception as e:
            error_entry = LogEntry(
                timestamp=datetime.now(),
                source='ESP8266',
                level='ERROR',
                message=f"Serial connection failed: {e}"
            )
            if self.log_queue:
                self.log_queue.put(error_entry)

    def _process_serial_line(self, line: str):
        """Process and categorize serial output line"""
        timestamp = datetime.now()

        # Parse ESP-IDF log levels
        level = 'INFO'
        if 'E (' in line:
            level = 'ERROR'
        elif 'W (' in line:
            level = 'WARNING'
        elif 'D (' in line:
            level = 'DEBUG'
        elif 'V (' in line:
            level = 'VERBOSE'

        # Extract component and message from ESP-IDF format
        # Format: "I (timestamp) component: message"
        message = line
        if ') ' in line and ': ' in line:
            try:
                parts = line.split(') ', 1)[1]  # Remove "I (timestamp) "
                if ': ' in parts:
                    component, msg = parts.split(': ', 1)
                    message = f"[{component}] {msg}"
                else:
                    message = parts
            except:
                message = line

        log_entry = LogEntry(
            timestamp=timestamp,
            source='ESP8266',
            level=level,
            message=message,
            raw_data=line
        )

        if self.log_queue:
            self.log_queue.put(log_entry)

class TestExecutor:
    """Execute test scripts and capture output"""

    def __init__(self, esp_ip: str, log_queue: queue.Queue):
        self.esp_ip = esp_ip
        self.log_queue = log_queue

    def run_test_script(self, script_name: str, args: List[str] = None):
        """Run a test script and capture its output"""
        cmd = ['python3', script_name, self.esp_ip]
        if args:
            cmd.extend(args)

        self._log_client_message(f"🚀 Starting test: {script_name}", 'TEST')

        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1
            )

            # Read output line by line in real-time
            for line in iter(process.stdout.readline, ''):
                if line.strip():
                    self._log_client_message(line.strip(), 'INFO')

            process.wait()

            if process.returncode == 0:
                self._log_client_message(f"✅ Test completed successfully: {script_name}", 'TEST')
            else:
                self._log_client_message(f"❌ Test failed: {script_name} (exit code: {process.returncode})", 'ERROR')

            return process.returncode == 0

        except Exception as e:
            self._log_client_message(f"❌ Test execution error: {e}", 'ERROR')
            return False

    def _log_client_message(self, message: str, level: str = 'INFO'):
        """Log a client-side message"""
        log_entry = LogEntry(
            timestamp=datetime.now(),
            source='CLIENT',
            level=level,
            message=message
        )
        self.log_queue.put(log_entry)

class IntegratedTestRunner:
    """Main test runner that coordinates serial monitoring and test execution"""

    def __init__(self, esp_ip: str, serial_port: str = '/dev/ttyUSB0', baud_rate: int = 74880):
        self.esp_ip = esp_ip
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.log_queue = queue.Queue()
        self.serial_monitor = SerialMonitor(serial_port, baud_rate, self.log_queue)
        self.test_executor = TestExecutor(esp_ip, self.log_queue)
        self.running = False
        self.log_file = None

    def start_logging(self, log_file_path: str = None):
        """Start integrated logging system"""
        if log_file_path:
            self.log_file = open(log_file_path, 'w')
            self._write_log_header()

        # Start serial monitoring
        self.serial_monitor.start()

        # Start log processing thread
        self.running = True
        self.log_thread = threading.Thread(target=self._process_log_queue, daemon=True)
        self.log_thread.start()

        print("🔄 Integrated logging started")
        print(f"📡 Monitoring ESP8266 on {self.serial_port}")
        print(f"🎯 Testing ESP8266 at {self.esp_ip}")
        if self.log_file:
            print(f"📝 Logging to {log_file_path}")
        print("-" * 80)

    def stop_logging(self):
        """Stop integrated logging system"""
        self.running = False
        self.serial_monitor.stop()

        if self.log_file:
            self.log_file.close()

        print("\n" + "=" * 80)
        print("🏁 Integrated logging stopped")

    def run_test_suite(self, test_config: Dict[str, Any]):
        """Run a complete test suite with integrated logging"""

        self._log_test_message("🚀 Starting ESP8266 MCP Test Suite", 'TEST')

        # Wait for ESP8266 to stabilize
        self._log_test_message("⏳ Waiting for ESP8266 to stabilize...", 'INFO')
        time.sleep(3)

        test_results = {}

        for test_name, test_config_item in test_config.items():
            self._log_test_message(f"📋 Running test: {test_name}", 'TEST')

            # Add delay between tests to see correlation clearly
            time.sleep(1)

            script = test_config_item.get('script')
            args = test_config_item.get('args', [])

            if script:
                success = self.test_executor.run_test_script(script, args)
                test_results[test_name] = success

                # Add delay to see server response
                time.sleep(2)
            else:
                self._log_test_message(f"⚠️  No script specified for test: {test_name}", 'WARNING')
                test_results[test_name] = False

        # Print summary
        self._print_test_summary(test_results)

        return test_results

    def _process_log_queue(self):
        """Process log entries from the queue and display them"""
        while self.running:
            try:
                log_entry = self.log_queue.get(timeout=0.1)
                self._display_log_entry(log_entry)

                if self.log_file:
                    self._write_log_entry(log_entry)

            except queue.Empty:
                continue
            except Exception as e:
                print(f"❌ Log processing error: {e}")

    def _display_log_entry(self, entry: LogEntry):
        """Display a log entry with color coding and formatting"""
        timestamp_str = entry.timestamp.strftime("%H:%M:%S.%f")[:-3]  # milliseconds

        # Color coding based on source and level
        if entry.source == 'ESP8266':
            if entry.level == 'ERROR':
                color = '\033[91m'  # Red
                icon = '🔥'
            elif entry.level == 'WARNING':
                color = '\033[93m'  # Yellow
                icon = '⚠️ '
            else:
                color = '\033[94m'  # Blue
                icon = '📡'
        else:  # CLIENT
            if entry.level == 'ERROR':
                color = '\033[91m'  # Red
                icon = '❌'
            elif entry.level == 'TEST':
                color = '\033[95m'  # Magenta
                icon = '🧪'
            else:
                color = '\033[92m'  # Green
                icon = '💻'

        reset_color = '\033[0m'

        # Format the output
        source_tag = f"{entry.source:>7}"
        print(f"{color}{timestamp_str} {icon} {source_tag}: {entry.message}{reset_color}")

    def _write_log_header(self):
        """Write log file header"""
        if self.log_file:
            header = f"""
# ESP8266 MCP Integrated Test Log
# Generated: {datetime.now().isoformat()}
# ESP8266 IP: {self.esp_ip}
# Serial Port: {self.serial_port}
# Baud Rate: {self.baud_rate}
{'=' * 80}
"""
            self.log_file.write(header)
            self.log_file.flush()

    def _write_log_entry(self, entry: LogEntry):
        """Write log entry to file"""
        if self.log_file:
            line = f"{entry.timestamp.isoformat()} [{entry.source:>7}] {entry.level:>5}: {entry.message}\n"
            self.log_file.write(line)
            self.log_file.flush()

    def _log_test_message(self, message: str, level: str = 'INFO'):
        """Log a test framework message"""
        log_entry = LogEntry(
            timestamp=datetime.now(),
            source='CLIENT',
            level=level,
            message=message
        )
        self.log_queue.put(log_entry)

    def _print_test_summary(self, results: Dict[str, bool]):
        """Print test summary"""
        total_tests = len(results)
        passed_tests = sum(1 for success in results.values() if success)
        success_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0

        self._log_test_message("", 'INFO')  # Empty line
        self._log_test_message("📊 TEST SUMMARY", 'TEST')
        self._log_test_message("=" * 50, 'TEST')

        for test_name, success in results.items():
            status = "✅ PASS" if success else "❌ FAIL"
            self._log_test_message(f"{status} {test_name}", 'TEST')

        self._log_test_message("=" * 50, 'TEST')
        self._log_test_message(f"📈 Results: {passed_tests}/{total_tests} tests passed", 'TEST')
        self._log_test_message(f"🎯 Success Rate: {success_rate:.1f}%", 'TEST')

        if passed_tests == total_tests:
            self._log_test_message("🎉 All tests passed!", 'TEST')
        else:
            self._log_test_message("⚠️  Some tests failed - check logs above", 'TEST')

def load_test_config(config_file: str = 'test_config.json') -> Dict[str, Any]:
    """Load test configuration from JSON file"""
    try:
        with open(config_file, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        # Default test configuration
        return {
            "basic_mcp_test": {
                "script": "test_mcp_client.py",
                "args": []
            },
            "session_management_test": {
                "script": "test_session_management.py",
                "args": ["--verbose"]
            },
            "session_tools_test": {
                "script": "test_session_tools.py",
                "args": ["--verbose"]
            }
        }

def signal_handler(signum, frame):
    """Handle Ctrl+C gracefully"""
    print("\n🛑 Received interrupt signal...")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='ESP8266 MCP Integrated Test Runner')
    parser.add_argument('esp_ip', help='ESP8266 IP address')
    parser.add_argument('--serial-port', default='/dev/ttyUSB0', help='Serial port (default: /dev/ttyUSB0)')
    parser.add_argument('--baud-rate', type=int, default=74880, help='Serial baud rate (default: 74880)')
    parser.add_argument('--log-file', help='Output log file path')
    parser.add_argument('--config', default='test_config.json', help='Test configuration file')
    parser.add_argument('--monitor-only', action='store_true', help='Only monitor serial output, no tests')
    parser.add_argument('--test-only', action='store_true', help='Only run tests, no serial monitoring')

    args = parser.parse_args()

    # Set up signal handler for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)

    # Create test runner
    runner = IntegratedTestRunner(args.esp_ip, args.serial_port, args.baud_rate)

    try:
        # Start logging system
        log_file_path = args.log_file or f"integrated_test_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        runner.start_logging(log_file_path)

        if args.monitor_only:
            print("🔍 Monitor-only mode - Press Ctrl+C to stop")
            while True:
                time.sleep(1)
        elif args.test_only:
            print("🧪 Test-only mode - Running tests without serial monitoring")
            runner.serial_monitor.stop()  # Stop serial monitoring
            test_config = load_test_config(args.config)
            runner.run_test_suite(test_config)
        else:
            # Full integrated mode
            test_config = load_test_config(args.config)
            runner.run_test_suite(test_config)

            # Keep monitoring for a bit after tests complete
            runner._log_test_message("🔍 Continuing to monitor for 10 seconds...", 'INFO')
            time.sleep(10)

    except KeyboardInterrupt:
        print("\n🛑 Interrupted by user")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
    finally:
        runner.stop_logging()

if __name__ == "__main__":
    main()
