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
valid_json='{"callsign":"TEST123","locator":"AA00aa","powerDbm":10,"txPercent":25}'
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

# Test 10: REST API - POST settings with browser-like data (form fields)
print_status "INFO" "Testing: Browser-like form submission"
browser_json='{"wifiSsid":"TestNetwork","wifiPassword":"password123","hostname":"test-beacon","callsign":"WB7NAB","locator":"CN87xx","powerDbm":30,"txPercent":50}'
response=$(curl -s -w "\n%{http_code}" --max-time $TEST_TIMEOUT \
               -X POST -H "Content-Type: application/json" \
               -d "$browser_json" "$BASE_URL/api/settings" 2>/dev/null || echo -e "\n000")
http_code=$(echo "$response" | tail -n1)

if [ "$http_code" = "204" ]; then
    # Verify the settings were actually saved
    saved_settings=$(curl -s --max-time $TEST_TIMEOUT "$BASE_URL/api/settings" 2>/dev/null)
    if echo "$saved_settings" | grep -q "WB7NAB" && echo "$saved_settings" | grep -q "CN87xx" && echo "$saved_settings" | grep -q "TestNetwork"; then
        print_status "PASS" "Browser-like form submission (HTTP $http_code)"
        ((TESTS_PASSED++))
    else
        print_status "FAIL" "Browser-like form submission - Settings not saved correctly"
        echo "  Saved settings: $saved_settings"
        ((TESTS_FAILED++))
    fi
else
    print_status "FAIL" "Browser-like form submission - HTTP $http_code"
    ((TESTS_FAILED++))
fi

# Test 11: Verify settings persistence after save/load cycle
print_status "INFO" "Testing: Settings persistence after save/load"
test_settings='{"callsign":"TEST456","locator":"DM13","powerDbm":20,"txPercent":75}'
# Save test settings
save_response=$(curl -s -w "%{http_code}" --max-time $TEST_TIMEOUT \
                     -X POST -H "Content-Type: application/json" \
                     -d "$test_settings" "$BASE_URL/api/settings" 2>/dev/null || echo "000")
save_code=$(echo "$save_response" | tail -c 4)

if [ "$save_code" = "204" ]; then
    # Retrieve and verify all fields are present
    loaded_settings=$(curl -s --max-time $TEST_TIMEOUT "$BASE_URL/api/settings" 2>/dev/null)
    if echo "$loaded_settings" | grep -q '"callsign":"TEST456"' && \
       echo "$loaded_settings" | grep -q '"locator":"DM13"' && \
       echo "$loaded_settings" | grep -q '"powerDbm":20' && \
       echo "$loaded_settings" | grep -q '"txPercent":75'; then
        print_status "PASS" "Settings persistence after save/load"
        ((TESTS_PASSED++))
    else
        print_status "FAIL" "Settings persistence - Not all fields preserved correctly"
        echo "  Expected: TEST456, DM13, powerDbm:20, txPercent:75"
        echo "  Got: $loaded_settings"
        ((TESTS_FAILED++))
    fi
else
    print_status "FAIL" "Settings persistence - Save failed with HTTP $save_code"
    ((TESTS_FAILED++))
fi

# Test 12: Verify band configuration API
print_status "INFO" "Testing: Band configuration in settings"
response=$(curl -s --max-time $TEST_TIMEOUT "$BASE_URL/api/settings" 2>/dev/null)
if echo "$response" | grep -q '"bands"' && \
   echo "$response" | grep -q '"160m"' && \
   echo "$response" | grep -q '"frequency"' && \
   echo "$response" | grep -q '"schedule"'; then
    print_status "PASS" "Band configuration present in settings"
    ((TESTS_PASSED++))
else
    print_status "FAIL" "Band configuration missing or incomplete"
    echo "  Response: $response"
    ((TESTS_FAILED++))
fi

# Test 13: Verify statistics in status API
print_status "INFO" "Testing: Statistics in status API"
response=$(curl -s --max-time $TEST_TIMEOUT "$BASE_URL/api/status.json" 2>/dev/null)
if echo "$response" | grep -q '"statistics"' && \
   echo "$response" | grep -q '"totalTransmissions"' && \
   echo "$response" | grep -q '"byBand"' && \
   echo "$response" | grep -q '"currentBand"'; then
    print_status "PASS" "Statistics present in status API"
    ((TESTS_PASSED++))
else
    print_status "FAIL" "Statistics missing or incomplete"
    echo "  Response: $response"
    ((TESTS_FAILED++))
fi

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