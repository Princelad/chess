#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SERVER="$BUILD_DIR/server/chess-server"
CLIENT="$BUILD_DIR/client/chess-client"

# Rebuild if --build flag is passed or binaries don't exist.
if [[ "${1:-}" == "--build" ]] || [[ ! -f "$SERVER" ]] || [[ ! -f "$CLIENT" ]]; then
    echo "Building..."
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR"
    cmake --build "$BUILD_DIR" -j"$(nproc)"
fi

if [[ ! -f "$SERVER" ]]; then
    echo "Error: server binary not found at $SERVER" >&2
    exit 1
fi

PORT="${PORT:-5555}"

echo "Starting server on port $PORT..."
"$SERVER" --port "$PORT" &
SERVER_PID=$!

cleanup() {
    echo "Shutting down server..."
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
}
trap cleanup EXIT

sleep 0.5

echo "Starting client 1..."
"$CLIENT" &
CLIENT1_PID=$!

echo "Starting client 2..."
"$CLIENT" &
CLIENT2_PID=$!

echo "Both clients started. Close a client window or press Ctrl+C to stop."
wait "$CLIENT1_PID" "$CLIENT2_PID" 2>/dev/null || true
