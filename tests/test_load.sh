#!/bin/bash

# I'm creating a comprehensive test script for my HTTP server.
# This will test everything from basic functionality to concurrency and stress.

SERVER_BIN="./server"
TEST_CONCURRENT_BIN="./tests/test_concurrent"
PORT=8080
URL="http://localhost:$PORT"

# Colors for pretty output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[TEST] $1${NC}"
}

error() {
    echo -e "${RED}[ERROR] $1${NC}"
    exit 1
}

# 1. First, I need to compile everything
log "Compiling Server and Test Client..."
# I make sure I'm in the project root directory
cd "$(dirname "$0")/.." || error "Failed to change directory to project root"

# I clean and rebuild the server
make clean && make || error "Server compilation failed"

# I compile the concurrent test client
gcc -o tests/test_concurrent tests/test_concurrent.c -pthread || error "Test client compilation failed"

# 2. Now I start the server
log "Starting Server..."
$SERVER_BIN &
SERVER_PID=$!
sleep 2 # I give the server time to start up

# I check if the server is actually running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    error "Server failed to start"
fi

# I set up a cleanup function to stop the server when I'm done
cleanup() {
    log "Stopping Server (PID: $SERVER_PID)..."
    kill $SERVER_PID
    wait $SERVER_PID 2>/dev/null
}
trap cleanup EXIT # This ensures cleanup runs even if the script exits early

# 3. Functional Tests - I test basic HTTP functionality
log "Running Functional Tests..."

# 3.1 Test GET request for index.html (should return 200 OK)
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" $URL/index.html)
if [ "$HTTP_CODE" -eq 200 ]; then
    echo "✓ GET /index.html: 200 OK"
else
    error "GET /index.html failed (Code: $HTTP_CODE)"
fi

# 3.2 Test Content-Type headers for different file types
# CSS file
CONTENT_TYPE=$(curl -s -I $URL/style.css | grep "Content-Type" | awk '{print $2}' | tr -d '\r')
if [[ "$CONTENT_TYPE" == "text/css" ]]; then
    echo "✓ Content-Type /style.css: text/css"
else
    error "Content-Type check failed (Got: $CONTENT_TYPE)"
fi

# PDF file
CONTENT_TYPE=$(curl -s -I $URL/test.pdf | grep "Content-Type" | awk '{print $2}' | tr -d '\r')
if [[ "$CONTENT_TYPE" == "application/pdf" ]]; then
    echo "✓ Content-Type /test.pdf: application/pdf"
else
    error "Content-Type check failed for PDF (Got: $CONTENT_TYPE)"
fi

# PNG image
CONTENT_TYPE=$(curl -s -I $URL/images/teste.png | grep "Content-Type" | awk '{print $2}' | tr -d '\r')
if [[ "$CONTENT_TYPE" == "image/png" ]]; then
    echo "✓ Content-Type /images/teste.png: image/png"
else
    error "Content-Type check failed for PNG (Got: $CONTENT_TYPE)"
fi

# JPG image
CONTENT_TYPE=$(curl -s -I $URL/images/teste.jpg | grep "Content-Type" | awk '{print $2}' | tr -d '\r')
if [[ "$CONTENT_TYPE" == "image/jpeg" ]]; then
    echo "✓ Content-Type /images/teste.jpg: image/jpeg"
else
    error "Content-Type check failed for JPG (Got: $CONTENT_TYPE)"
fi

# 3.3 Test 404 Not Found for non-existent files
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" $URL/nonexistent.file)
if [ "$HTTP_CODE" -eq 404 ]; then
    echo "✓ GET /nonexistent: 404 Not Found"
else
    error "404 check failed (Code: $HTTP_CODE)"
fi

# 3.4 Test directory indexing (serving index.html from directories)
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" $URL/)
if [ "$HTTP_CODE" -eq 200 ]; then
    echo "✓ Directory Indexing: 200 OK"
else
    error "Directory Indexing failed (Code: $HTTP_CODE)"
fi

# 4. Concurrency Tests - I test how the server handles multiple simultaneous requests
log "Running Concurrency Tests (Apache Bench)..."
if command -v ab >/dev/null 2>&1; then
    # I use Apache Bench to send 10,000 requests with 100 concurrent connections
    ab -n 10000 -c 100 -k $URL/index.html > ab_results.log 2>&1
    if [ $? -eq 0 ]; then
        echo "✓ Apache Bench completed successfully"
        # I show some key metrics
        grep "Requests per second" ab_results.log
        grep "Failed requests" ab_results.log
    else
        error "Apache Bench failed"
    fi
else
    echo "Apache Bench (ab) not found. Skipping."
fi

# 5. Synchronization & Consistency Tests - I test thread safety
log "Running Synchronization Tests (Custom Client)..."
$TEST_CONCURRENT_BIN
if [ $? -eq 0 ]; then
    echo "✓ Synchronization tests passed"
else
    error "Synchronization tests failed"
fi

# 6. Stress Test - I push the server hard for 5 seconds
log "Running Stress Test (5 seconds load)..."
timeout 5s ab -n 100000 -c 50 $URL/index.html > /dev/null 2>&1
echo "✓ Stress test completed (Server should still be alive)"

# I verify the server is still running after the stress test
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "✓ Server survived stress test"
else
    error "Server crashed during stress test"
fi

# If I get here, all tests passed!
log "All Tests Passed Successfully!"
exit 0