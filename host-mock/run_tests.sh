#!/bin/bash

# Web server test script for host-testbench

EXECUTABLE="$1"
BASE_URL="http://localhost:8080"
TEST_TIMEOUT=10
STARTUP_WAIT=5

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    local status=$1
    local message=$2
    case $status in
        "PASS")
            echo -e "${GREEN}[PASS]${NC} $message"
            ;;
        "FAIL")
            echo -e "${RED}[FAIL]${NC} $message"
            ;;
        "INFO")
            echo -e "${YELLOW}[INFO]${NC} $message"
            ;;
    esac
}

# Function to cleanup server process
cleanup() {
    if [ ! -z "$SERVER_PID" ]; then
        print_status "INFO" "Stopping server (PID: $SERVER_PID)"
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Check if executable exists
if [ ! -f "$EXECUTABLE" ]; then
    print_status "FAIL" "Executable not found: $EXECUTABLE"
    exit 1
fi

print_status "INFO" "Starting web server tests..."
print_status "INFO" "Executable: $EXECUTABLE"
print_status "INFO" "Base URL: $BASE_URL"

# Start the server in background
print_status "INFO" "Starting server..."
$EXECUTABLE > server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
print_status "INFO" "Waiting for server to be ready..."
sleep 2

# Wait for server to be ready to accept connections
for i in {1..10}; do
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        print_status "FAIL" "Server process died during startup"
        echo "Server log:"
        cat server.log
        exit 1
    fi
    
    if curl -s --max-time 2 "$BASE_URL/api/settings" > /dev/null 2>&1; then
        print_status "INFO" "Server is ready (attempt $i)"
        break
    fi
    
    if [ $i -eq 10 ]; then
        print_status "FAIL" "Server not responding after 10 attempts"
        echo "Server log:"
        cat server.log
        exit 1
    fi
    
    sleep 1
done

print_status "INFO" "Server started with PID: $SERVER_PID"

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to run a test
run_test() {
    local test_name="$1"
    local url="$2"
    local expected_content="$3"
    local method="${4:-GET}"
    local post_data="$5"
    
    print_status "INFO" "Testing: $test_name"
    
    if [ "$method" = "POST" ]; then
        response=$(curl -s -w "\n%{http_code}" --max-time $TEST_TIMEOUT \
                       -X POST -H "Content-Type: application/json" \
                       -d "$post_data" "$url" 2>/dev/null || echo -e "\n000")
    else
        response=$(curl -s -w "\n%{http_code}" --max-time $TEST_TIMEOUT "$url" 2>/dev/null || echo -e "\n000")
    fi
    
    # Split response and status code
    http_code=$(echo "$response" | tail -n1)
    content=$(echo "$response" | head -n -1)
    
    if [ "$http_code" = "200" ] || [ "$http_code" = "204" ]; then
        if [ -z "$expected_content" ] || echo "$content" | grep -q "$expected_content"; then
            print_status "PASS" "$test_name (HTTP $http_code)"
            ((TESTS_PASSED++))
            return 0
        else
            print_status "FAIL" "$test_name - Expected content not found"
            echo "  Expected: $expected_content"
            echo "  Got: $content"
            ((TESTS_FAILED++))
            return 1
        fi
    else
        print_status "FAIL" "$test_name - HTTP $http_code"
        echo "  Response: $content"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Test 1: Static content - index.html
run_test "Static content - index.html" "$BASE_URL/" "<html"

# Test 2: Static content - CSS file
run_test "Static content - CSS file" "$BASE_URL/style.css" "body"

# Test 3: Static content - JS file  
run_test "Static content - JavaScript file" "$BASE_URL/common.js" "function"

# Test 4: Static content - Settings page
run_test "Static content - settings.html" "$BASE_URL/settings.html" "<html"

# Test 5: REST API - GET settings
run_test "REST API - GET /api/settings" "$BASE_URL/api/settings" "callsign"

# Test 6: REST API - GET status  
run_test "REST API - GET /api/status.json" "$BASE_URL/api/status.json" "callsign"

# Test 7: REST API - POST settings (test with valid JSON)
valid_json='{"callsign":"TEST123","locator":"AA00aa","powerDbm":10,"txIntervalMinutes":4}'
run_test "REST API - POST /api/settings (valid)" "$BASE_URL/api/settings" "" "POST" "$valid_json"

# Test 8: REST API - POST settings (test with invalid JSON)
print_status "INFO" "Testing: REST API - POST /api/settings (invalid)"
invalid_json='{"invalid_json"'
response=$(curl -s -w "\n%{http_code}" --max-time $TEST_TIMEOUT \
               -X POST -H "Content-Type: application/json" \
               -d "$invalid_json" "$BASE_URL/api/settings" 2>/dev/null || echo -e "\n000")
http_code=$(echo "$response" | tail -n1)
content=$(echo "$response" | head -n -1)

if [ "$http_code" = "400" ] && echo "$content" | grep -q "error"; then
    print_status "PASS" "REST API - POST /api/settings (invalid) (HTTP $http_code)"
    ((TESTS_PASSED++))
else
    print_status "FAIL" "REST API - POST /api/settings (invalid) - Expected HTTP 400 with error message"
    echo "  Got HTTP $http_code: $content"
    ((TESTS_FAILED++))
fi

# Test 9: 404 for non-existent path
print_status "INFO" "Testing: 404 for non-existent path"
response=$(curl -s -w "\n%{http_code}" --max-time $TEST_TIMEOUT "$BASE_URL/nonexistent" 2>/dev/null || echo -e "\n000")
http_code=$(echo "$response" | tail -n1)
if [ "$http_code" = "404" ]; then
    print_status "PASS" "404 for non-existent path (HTTP $http_code)"
    ((TESTS_PASSED++))
else
    print_status "FAIL" "404 for non-existent path - Expected 404, got HTTP $http_code"
    ((TESTS_FAILED++))
fi

# Summary
echo
print_status "INFO" "Test Summary:"
print_status "INFO" "  Passed: $TESTS_PASSED"
print_status "INFO" "  Failed: $TESTS_FAILED"
print_status "INFO" "  Total:  $((TESTS_PASSED + TESTS_FAILED))"

if [ $TESTS_FAILED -eq 0 ]; then
    print_status "PASS" "All tests passed!"
    exit 0
else
    print_status "FAIL" "$TESTS_FAILED test(s) failed"
    echo
    echo "Server log:"
    cat server.log
    exit 1
fi