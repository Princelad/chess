#!/usr/bin/env python3
"""Mock UCI engine that never speaks and never exits: teardown must be
bounded by the SIGTERM/SIGKILL fallback, not by waiting for stdout EOF
(a mute child keeps the pipe open forever)."""
import time

while True:
    time.sleep(1)
