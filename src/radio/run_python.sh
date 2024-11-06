#!/bin/bash
export PYTHONPATH="/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/lib/python3.9/site-packages:$PYTHONPATH"
export DYLD_LIBRARY_PATH="/Applications/VLC.app/Contents/MacOS/lib:$DYLD_LIBRARY_PATH"
export VLC_PLUGIN_PATH="/Applications/VLC.app/Contents/MacOS/plugins"
source "/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/bin/activate"
"/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/bin/python3" "$@"