#!/bin/bash
# Sphere99 Server Start Script
# Usage: ./start_server.sh [sphere_dir]
#
# Prerequisites:
#   sudo apt-get install -y gcc-multilib g++-multilib make libc6:i386 libstdc++6:i386
#
# sphere_dir must contain: sphere99svr, sphere.ini, scripts/, muls/, save/, accounts/

SPHERE_DIR="${1:-$(pwd)}"
cd "$SPHERE_DIR" || { echo "ERROR: Cannot cd to $SPHERE_DIR"; exit 1; }

# Verify required files
for f in sphere99svr sphere.ini; do
    [ -f "$f" ] || { echo "ERROR: $f not found in $SPHERE_DIR"; exit 1; }
done
[ -d scripts ] || { echo "ERROR: scripts/ not found"; exit 1; }
[ -d muls ] || { echo "ERROR: muls/ not found"; exit 1; }

# Fix sphere.ini backslash paths (Windows → Linux)
sed -i 's|=scripts\\|=scripts/|; s|=muls\\|=muls/|; s|=accounts\\|=accounts/|; s|=save\\|=save/|; s|=logs\\|=logs/|' sphere.ini

# Fix spheretables.scp backslash paths
[ -f scripts/spheretables.scp ] && sed -i 's|\\|/|g' scripts/spheretables.scp

# Ensure log directory exists
mkdir -p logs

# Kill any existing server
pkill -x sphere99svr 2>/dev/null
sleep 1

# Start server with stdin kept open (tail -f /dev/null prevents EOF exit)
# Stdout/stderr to log file
echo "Starting Sphere99 server in $SPHERE_DIR..."
tail -f /dev/null | ./sphere99svr > sphere99svr.log 2>&1 &
SERVER_PID=$!

echo "Server PID: $SERVER_PID"
echo "Log: $SPHERE_DIR/sphere99svr.log"
echo "Waiting for server to load..."

# Wait up to 10 minutes for port 2593
for i in $(seq 1 60); do
    sleep 10
    if ss -tlnp 2>/dev/null | grep -q ":2593 "; then
        echo "Server READY on port 2593 (after $((i*10))s)"
        echo "Run tests: python3 tools/test_suite.py localhost 2593"
        exit 0
    fi
    # Check if server died
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo "Server crashed during loading. Check sphere99svr.log"
        exit 1
    fi
done

echo "Server did not start within 10 minutes"
exit 1
