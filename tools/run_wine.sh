#!/bin/bash
# Runs the Windows Calculator.exe under Wine on macOS.
# Expects "Wine Staging.app" in ~/Applications (see README for download).
export WINEPREFIX="${WINEPREFIX:-$HOME/.winecalc}"
exec "$HOME/Applications/Wine Staging.app/Contents/Resources/wine/bin/wine" \
     "$(cd "$(dirname "$0")/.." && pwd)/Calculator.exe"
