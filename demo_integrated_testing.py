#!/usr/bin/env python3
"""
ESP8266 MCP Integrated Testing Demo
Demonstrates how to use the integrated test runner for correlating
ESP8266 server logs with client test execution in real-time.
"""

import sys
import time
from integrated_test_runner import IntegratedTestRunner, load_test_config

def demo_basic_usage():
    """Demonstrate basic integrated testing usage"""

    print("🚀 ESP8266 MCP Integrated Testing Demo")
    print("=" * 60)

    # Check if ESP8266 IP was provided
    if len(sys.argv) < 2:
        print("❌ Usage: python3 demo_integrated_testing.py <ESP8266_IP>")
        print("📝 Example: python3 demo_integrated_testing.py 192.168.86.30")
        sys.exit(1)

    esp_ip = sys.argv[1]
    print(f"🎯 Target ESP8266 IP: {esp_ip}")
    print(f"📡 Serial Port: /dev/ttyUSB0")
    print(f"⚡ Baud Rate: 74880")
    print()

    # Create integrated test runner
    runner = IntegratedTestRunner(
        esp_ip=esp_ip,
        serial_port='/dev/ttyUSB0',
        baud_rate=74880
    )

    try:
        # Start integrated logging
        log_file = f"demo_integrated_test_{int(time.time())}.log"
        print(f"📝 Starting integrated logging to: {log_file}")
        runner.start_logging(log_file)

        print("🔄 Integrated logging active - you should see both:")
        print("   📡 ESP8266 serial output (blue)")
        print("   💻 Client test output (green)")
        print("   🧪 Test framework messages (magenta)")
        print()

        # Wait for ESP8266 to stabilize
        print("⏳ Waiting for ESP8266 to stabilize...")
        time.sleep(3)

        print("🧪 Starting demonstration test sequence...")
        print("-" * 60)

        # Demo 1: Basic connectivity test
        print("\n📋 DEMO 1: Basic MCP Connectivity Test")
        print("This will test basic TCP connection and MCP protocol")
        runner.test_executor.run_test_script('test_mcp_client.py')

        # Wait to see correlation
        time.sleep(2)

        # Demo 2: Single tool test
        print("\n📋 DEMO 2: Single Tool Test")
        print("Testing echo tool functionality")
        runner.test_executor._log_client_message("🔧 Testing echo tool with custom message", 'TEST')

        # Simulate a simple tool test (since we don't have individual tool scripts)
        import socket
        import json

        try:
            # Simple echo test
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((esp_ip, 8080))

            # Send echo request
            request = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "tools/call",
                "params": {
                    "name": "echo",
                    "arguments": {"message": "Hello from integrated test demo!"}
                }
            }

            runner.test_executor._log_client_message(f"📤 Sending echo request: {request['params']['arguments']['message']}", 'INFO')
            sock.send((json.dumps(request) + '\n').encode())

            # Receive response
            response = sock.recv(1024).decode().strip()
            runner.test_executor._log_client_message(f"📥 Received response: {len(response)} bytes", 'INFO')

            sock.close()
            runner.test_executor._log_client_message("✅ Echo test completed successfully", 'TEST')

        except Exception as e:
            runner.test_executor._log_client_message(f"❌ Echo test failed: {e}", 'ERROR')

        # Wait to see server response
        time.sleep(2)

        # Demo 3: Show correlation in action
        print("\n📋 DEMO 3: Real-time Correlation Demo")
        print("Performing rapid requests to show client/server correlation")

        for i in range(3):
            runner.test_executor._log_client_message(f"🔄 Sending ping request #{i+1}", 'TEST')

            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                sock.connect((esp_ip, 8080))

                ping_request = {
                    "jsonrpc": "2.0",
                    "id": i+10,
                    "method": "ping"
                }

                sock.send((json.dumps(ping_request) + '\n').encode())
                response = sock.recv(1024).decode().strip()
                sock.close()

                runner.test_executor._log_client_message(f"✅ Ping #{i+1} successful", 'INFO')

            except Exception as e:
                runner.test_executor._log_client_message(f"❌ Ping #{i+1} failed: {e}", 'ERROR')

            time.sleep(1)  # Delay to see correlation clearly

        # Demo 4: Error handling demonstration
        print("\n📋 DEMO 4: Error Handling Demo")
        print("Testing invalid requests to show error correlation")

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((esp_ip, 8080))

            # Send invalid request
            invalid_request = {
                "jsonrpc": "2.0",
                "id": 999,
                "method": "tools/call",
                "params": {
                    "name": "nonexistent_tool",
                    "arguments": {}
                }
            }

            runner.test_executor._log_client_message("📤 Sending invalid tool request (should fail)", 'TEST')
            sock.send((json.dumps(invalid_request) + '\n').encode())

            response = sock.recv(1024).decode().strip()
            runner.test_executor._log_client_message(f"📥 Error response received (expected): {len(response)} bytes", 'INFO')

            sock.close()

        except Exception as e:
            runner.test_executor._log_client_message(f"❌ Error test failed: {e}", 'ERROR')

        time.sleep(2)

        # Summary
        print("\n" + "=" * 60)
        runner.test_executor._log_client_message("🎉 Integrated Testing Demo Complete!", 'TEST')
        runner.test_executor._log_client_message("", 'INFO')
        runner.test_executor._log_client_message("📊 Demo Summary:", 'TEST')
        runner.test_executor._log_client_message("✅ Basic connectivity test", 'TEST')
        runner.test_executor._log_client_message("✅ Single tool test", 'TEST')
        runner.test_executor._log_client_message("✅ Real-time correlation demo", 'TEST')
        runner.test_executor._log_client_message("✅ Error handling demo", 'TEST')
        runner.test_executor._log_client_message("", 'INFO')
        runner.test_executor._log_client_message(f"📝 Full log saved to: {log_file}", 'INFO')

        # Continue monitoring for a bit
        print("\n🔍 Continuing to monitor for 5 seconds...")
        print("   (You can reset the ESP8266 now to see boot messages)")
        time.sleep(5)

    except KeyboardInterrupt:
        print("\n🛑 Demo interrupted by user")
    except Exception as e:
        print(f"❌ Demo error: {e}")
    finally:
        runner.stop_logging()
        print("\n🏁 Demo completed")

def demo_advanced_usage():
    """Demonstrate advanced integrated testing features"""

    print("🚀 Advanced Integrated Testing Demo")
    print("=" * 60)

    if len(sys.argv) < 2:
        print("❌ Usage: python3 demo_integrated_testing.py <ESP8266_IP>")
        sys.exit(1)

    esp_ip = sys.argv[1]

    # Load test configuration
    try:
        test_config = load_test_config('integrated_test_config.json')
        print(f"📋 Loaded test configuration with {len(test_config)} test suites")
    except:
        print("⚠️  Using default test configuration")
        test_config = {
            "basic_connectivity": {
                "script": "test_mcp_client.py",
                "args": []
            }
        }

    # Create runner
    runner = IntegratedTestRunner(esp_ip)

    try:
        # Start integrated logging
        log_file = f"advanced_demo_{int(time.time())}.log"
        runner.start_logging(log_file)

        # Run full test suite with integrated logging
        print("🧪 Running full test suite with integrated logging...")
        results = runner.run_test_suite(test_config)

        # Analyze results
        total_tests = len(results)
        passed_tests = sum(1 for success in results.values() if success)

        print(f"\n📊 Advanced Demo Results:")
        print(f"   📈 {passed_tests}/{total_tests} tests passed")
        print(f"   🎯 Success rate: {passed_tests/total_tests*100:.1f}%")
        print(f"   📝 Detailed log: {log_file}")

    except Exception as e:
        print(f"❌ Advanced demo error: {e}")
    finally:
        runner.stop_logging()

def show_usage_examples():
    """Show various usage examples"""

    print("🔧 ESP8266 MCP Integrated Test Runner - Usage Examples")
    print("=" * 70)
    print()

    print("📋 Basic Usage:")
    print("   python3 integrated_test_runner.py 192.168.86.30")
    print()

    print("🔍 Monitor-Only Mode (no tests, just ESP8266 logs):")
    print("   python3 integrated_test_runner.py 192.168.86.30 --monitor-only")
    print()

    print("🧪 Test-Only Mode (no serial monitoring):")
    print("   python3 integrated_test_runner.py 192.168.86.30 --test-only")
    print()

    print("⚙️  Custom Configuration:")
    print("   python3 integrated_test_runner.py 192.168.86.30 \\")
    print("       --config my_test_config.json \\")
    print("       --log-file my_test_log.txt \\")
    print("       --serial-port /dev/ttyUSB1 \\")
    print("       --baud-rate 115200")
    print()

    print("📊 Demo Scripts:")
    print("   python3 demo_integrated_testing.py 192.168.86.30")
    print()

    print("🔧 Key Features:")
    print("   ✅ Real-time correlation of ESP8266 logs with client tests")
    print("   ✅ Color-coded output (ESP8266=blue, Client=green, Tests=magenta)")
    print("   ✅ Timestamped logs with millisecond precision")
    print("   ✅ Automatic log file generation")
    print("   ✅ Configurable test suites")
    print("   ✅ Graceful error handling and cleanup")
    print()

    print("📝 Log File Format:")
    print("   2024-12-19T14:30:15.123 [ESP8266]  INFO: [wifi] connected to AP")
    print("   2024-12-19T14:30:15.456 [ CLIENT]  TEST: 🚀 Starting test: basic_mcp")
    print("   2024-12-19T14:30:15.789 [ESP8266]  INFO: [MCP] client connected")
    print("   2024-12-19T14:30:16.012 [ CLIENT]  INFO: ✅ Test completed successfully")
    print()

def main():
    """Main demo function"""

    if len(sys.argv) == 1:
        show_usage_examples()
        return

    if '--help' in sys.argv or '-h' in sys.argv:
        show_usage_examples()
        return

    if '--advanced' in sys.argv:
        demo_advanced_usage()
    else:
        demo_basic_usage()

if __name__ == "__main__":
    main()
