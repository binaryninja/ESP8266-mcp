#!/usr/bin/env python3
"""
ESP8266 MCP JSON Corruption Diagnostic Tool

This script helps investigate the JSON serialization corruption bug
by testing various scenarios and analyzing server responses.
"""

import socket
import json
import time
import argparse
import sys
from typing import Dict, Any, Optional, List
import re

class JsonCorruptionDiagnostic:
    def __init__(self, esp_ip: str, port: int = 8080):
        self.esp_ip = esp_ip
        self.port = port
        self.test_results = []
        self.corruption_patterns = []

    def connect(self) -> Optional[socket.socket]:
        """Create a connection to the ESP8266 MCP server"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((self.esp_ip, self.port))
            return sock
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return None

    def send_request(self, sock: socket.socket, request: Dict[str, Any]) -> Optional[str]:
        """Send a JSON-RPC request and return the response"""
        try:
            request_str = json.dumps(request) + '\n'
            sock.send(request_str.encode())

            # Read response
            response = sock.recv(4096).decode().strip()
            return response
        except Exception as e:
            print(f"❌ Request failed: {e}")
            return None

    def analyze_response(self, response: str, expected_strings: List[str]) -> Dict[str, Any]:
        """Analyze response for corruption patterns"""
        analysis = {
            'raw_response': response,
            'is_valid_json': False,
            'parsed_json': None,
            'corruption_found': False,
            'corruption_details': [],
            'expected_strings_found': [],
            'unexpected_values': []
        }

        try:
            # Try to parse JSON
            parsed = json.loads(response)
            analysis['is_valid_json'] = True
            analysis['parsed_json'] = parsed

            # Check for corruption patterns
            corruption_found = self._check_corruption_recursive(parsed, "", analysis)
            analysis['corruption_found'] = corruption_found

            # Check for expected strings
            response_lower = response.lower()
            for expected in expected_strings:
                if expected.lower() in response_lower:
                    analysis['expected_strings_found'].append(expected)
                else:
                    # Check if it appears as 'true' instead
                    if 'true' in response_lower:
                        analysis['corruption_details'].append(f"Expected '{expected}' possibly replaced with 'true'")

        except json.JSONDecodeError as e:
            analysis['json_error'] = str(e)

        return analysis

    def _check_corruption_recursive(self, obj: Any, path: str, analysis: Dict[str, Any]) -> bool:
        """Recursively check for corruption patterns in JSON object"""
        corruption_found = False

        if isinstance(obj, dict):
            for key, value in obj.items():
                current_path = f"{path}.{key}" if path else key

                if isinstance(value, bool) and value is True:
                    # Check if this should be a string
                    if key in ['jsonrpc', 'name', 'version', 'protocolVersion', 'description', 'text']:
                        analysis['corruption_details'].append(f"Boolean 'true' found at {current_path} (likely corrupted string)")
                        corruption_found = True

                corruption_found |= self._check_corruption_recursive(value, current_path, analysis)

        elif isinstance(obj, list):
            for i, item in enumerate(obj):
                current_path = f"{path}[{i}]"
                corruption_found |= self._check_corruption_recursive(item, current_path, analysis)

        return corruption_found

    def test_initialization(self) -> Dict[str, Any]:
        """Test initialization request and analyze response"""
        print("🔍 Testing initialization request...")

        sock = self.connect()
        if not sock:
            return {'error': 'Connection failed'}

        try:
            request = {
                "jsonrpc": "2.0",
                "id": "diagnostic_init",
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "clientInfo": {
                        "name": "JSON-Corruption-Diagnostic",
                        "version": "1.0.0"
                    },
                    "capabilities": {
                        "roots": {"listChanged": False},
                        "sampling": {}
                    }
                }
            }

            response = self.send_request(sock, request)
            if response:
                expected_strings = ['ESP8266-MCP', '2024-11-05', '1.0.0', 'jsonrpc', '2.0']
                analysis = self.analyze_response(response, expected_strings)
                analysis['test_name'] = 'initialization'
                analysis['request'] = request
                return analysis

        finally:
            sock.close()

        return {'error': 'No response received'}

    def test_tools_list(self) -> Dict[str, Any]:
        """Test tools/list request"""
        print("🔍 Testing tools/list request...")

        sock = self.connect()
        if not sock:
            return {'error': 'Connection failed'}

        try:
            # First initialize
            init_request = {
                "jsonrpc": "2.0",
                "id": "1",
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "clientInfo": {"name": "Diagnostic", "version": "1.0.0"},
                    "capabilities": {}
                }
            }
            self.send_request(sock, init_request)
            time.sleep(0.1)  # Brief delay

            # Now test tools/list
            request = {
                "jsonrpc": "2.0",
                "id": "diagnostic_tools",
                "method": "tools/list"
            }

            response = self.send_request(sock, request)
            if response:
                expected_strings = ['echo', 'gpio_control', 'description', 'inputSchema']
                analysis = self.analyze_response(response, expected_strings)
                analysis['test_name'] = 'tools_list'
                analysis['request'] = request
                return analysis

        finally:
            sock.close()

        return {'error': 'No response received'}

    def test_echo_tool(self, test_string: str) -> Dict[str, Any]:
        """Test echo tool with specific string"""
        print(f"🔍 Testing echo tool with: '{test_string}'")

        sock = self.connect()
        if not sock:
            return {'error': 'Connection failed'}

        try:
            # Initialize first
            init_request = {
                "jsonrpc": "2.0",
                "id": "1",
                "method": "initialize",
                "params": {
                    "protocolVersion": "2024-11-05",
                    "clientInfo": {"name": "Diagnostic", "version": "1.0.0"},
                    "capabilities": {}
                }
            }
            self.send_request(sock, init_request)
            time.sleep(0.1)

            # Test echo
            request = {
                "jsonrpc": "2.0",
                "id": "diagnostic_echo",
                "method": "tools/call",
                "params": {
                    "name": "echo",
                    "arguments": {
                        "text": test_string
                    }
                }
            }

            response = self.send_request(sock, request)
            if response:
                expected_strings = [test_string, 'Echo:', 'content']
                analysis = self.analyze_response(response, expected_strings)
                analysis['test_name'] = f'echo_tool_{test_string.replace(" ", "_")}'
                analysis['request'] = request
                analysis['test_string'] = test_string
                return analysis

        finally:
            sock.close()

        return {'error': 'No response received'}

    def test_multiple_strings(self) -> List[Dict[str, Any]]:
        """Test multiple different string values to identify patterns"""
        test_strings = [
            "hello",
            "world",
            "ESP8266-MCP",
            "2024-11-05",
            "Simple string",
            "String with spaces",
            "String_with_underscores",
            "String-with-dashes",
            "String123WithNumbers",
            "SpecialChars!@#$%",
            "Unicode: 🚀",
            "Long string that is quite a bit longer than the others to test if length matters",
            "",  # Empty string
            "true",  # String that matches the corruption value
            "false",
            "null"
        ]

        results = []
        for test_string in test_strings:
            try:
                result = self.test_echo_tool(test_string)
                results.append(result)
                time.sleep(0.5)  # Delay between tests
            except Exception as e:
                print(f"❌ Failed testing string '{test_string}': {e}")

        return results

    def test_concurrent_connections(self) -> Dict[str, Any]:
        """Test multiple concurrent connections to check for race conditions"""
        print("🔍 Testing concurrent connections...")

        import threading
        results = []

        def connection_test():
            try:
                result = self.test_initialization()
                results.append(result)
            except Exception as e:
                results.append({'error': str(e)})

        # Create multiple threads
        threads = []
        for i in range(3):
            thread = threading.Thread(target=connection_test)
            threads.append(thread)
            thread.start()

        # Wait for all threads
        for thread in threads:
            thread.join()

        return {
            'test_name': 'concurrent_connections',
            'connection_count': len(threads),
            'results': results
        }

    def run_comprehensive_diagnostic(self) -> Dict[str, Any]:
        """Run all diagnostic tests"""
        print("🚀 Starting comprehensive JSON corruption diagnostic")
        print(f"🎯 Target: {self.esp_ip}:{self.port}")
        print("=" * 60)

        diagnostic_results = {
            'target': f"{self.esp_ip}:{self.port}",
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
            'tests': {}
        }

        # Test 1: Basic initialization
        try:
            diagnostic_results['tests']['initialization'] = self.test_initialization()
        except Exception as e:
            diagnostic_results['tests']['initialization'] = {'error': str(e)}

        # Test 2: Tools list
        try:
            diagnostic_results['tests']['tools_list'] = self.test_tools_list()
        except Exception as e:
            diagnostic_results['tests']['tools_list'] = {'error': str(e)}

        # Test 3: Multiple string values
        try:
            string_tests = self.test_multiple_strings()
            diagnostic_results['tests']['string_corruption'] = string_tests
        except Exception as e:
            diagnostic_results['tests']['string_corruption'] = {'error': str(e)}

        # Test 4: Concurrent connections
        try:
            diagnostic_results['tests']['concurrent'] = self.test_concurrent_connections()
        except Exception as e:
            diagnostic_results['tests']['concurrent'] = {'error': str(e)}

        return diagnostic_results

    def print_summary(self, results: Dict[str, Any]):
        """Print a summary of diagnostic results"""
        print("\n" + "=" * 60)
        print("📊 DIAGNOSTIC SUMMARY")
        print("=" * 60)

        total_tests = 0
        corrupted_tests = 0

        for test_name, test_result in results['tests'].items():
            if isinstance(test_result, list):
                # Handle multiple test results
                for i, result in enumerate(test_result):
                    total_tests += 1
                    if result.get('corruption_found', False):
                        corrupted_tests += 1
                        print(f"❌ {test_name}[{i}]: CORRUPTION DETECTED")
                        for detail in result.get('corruption_details', []):
                            print(f"   📋 {detail}")
                    else:
                        print(f"✅ {test_name}[{i}]: No corruption detected")
            else:
                total_tests += 1
                if test_result.get('corruption_found', False):
                    corrupted_tests += 1
                    print(f"❌ {test_name}: CORRUPTION DETECTED")
                    for detail in test_result.get('corruption_details', []):
                        print(f"   📋 {detail}")
                else:
                    print(f"✅ {test_name}: No corruption detected")

        print(f"\n📈 CORRUPTION RATE: {corrupted_tests}/{total_tests} ({corrupted_tests/total_tests*100:.1f}%)")

        # Identify patterns
        corruption_patterns = []
        for test_name, test_result in results['tests'].items():
            if isinstance(test_result, list):
                for result in test_result:
                    if result.get('corruption_found', False):
                        corruption_patterns.extend(result.get('corruption_details', []))
            else:
                if test_result.get('corruption_found', False):
                    corruption_patterns.extend(test_result.get('corruption_details', []))

        if corruption_patterns:
            print(f"\n🔍 CORRUPTION PATTERNS IDENTIFIED:")
            for pattern in set(corruption_patterns):
                print(f"   📋 {pattern}")

    def save_results(self, results: Dict[str, Any], filename: str = None):
        """Save diagnostic results to file"""
        if filename is None:
            timestamp = time.strftime('%Y%m%d_%H%M%S')
            filename = f"json_corruption_diagnostic_{timestamp}.json"

        try:
            with open(filename, 'w') as f:
                json.dump(results, f, indent=2)
            print(f"📝 Results saved to: {filename}")
        except Exception as e:
            print(f"❌ Failed to save results: {e}")

def main():
    parser = argparse.ArgumentParser(description='ESP8266 MCP JSON Corruption Diagnostic Tool')
    parser.add_argument('esp_ip', help='ESP8266 IP address')
    parser.add_argument('--port', type=int, default=8080, help='Port number (default: 8080)')
    parser.add_argument('--output', help='Output file for results')
    parser.add_argument('--test', choices=['init', 'tools', 'echo', 'all'], default='all',
                       help='Specific test to run')

    args = parser.parse_args()

    # Test connectivity first
    try:
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(5)
        test_sock.connect((args.esp_ip, args.port))
        test_sock.close()
        print(f"✅ ESP8266 reachable at {args.esp_ip}:{args.port}")
    except Exception as e:
        print(f"❌ Cannot connect to ESP8266 at {args.esp_ip}:{args.port}: {e}")
        sys.exit(1)

    # Run diagnostic
    diagnostic = JsonCorruptionDiagnostic(args.esp_ip, args.port)

    if args.test == 'all':
        results = diagnostic.run_comprehensive_diagnostic()
    elif args.test == 'init':
        results = {'tests': {'initialization': diagnostic.test_initialization()}}
    elif args.test == 'tools':
        results = {'tests': {'tools_list': diagnostic.test_tools_list()}}
    elif args.test == 'echo':
        results = {'tests': {'echo_test': diagnostic.test_echo_tool("Test string")}}

    # Print summary
    diagnostic.print_summary(results)

    # Save results
    if args.output:
        diagnostic.save_results(results, args.output)
    else:
        diagnostic.save_results(results)

if __name__ == "__main__":
    main()
