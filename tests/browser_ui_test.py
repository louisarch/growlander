#!/usr/bin/env python3
"""Compatibility entry point for the full-viewport browser regression suite."""
import runpy
import sys
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("browser_overlay_test.py")), run_name="__main__")
