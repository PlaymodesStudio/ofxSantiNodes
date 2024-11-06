#!/bin/bash
export PYTHONPATH="/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/lib/python3.9/site-packages"
export DYLD_LIBRARY_PATH="/Applications/VLC.app/Contents/MacOS/lib"
export VLC_PLUGIN_PATH="/Applications/VLC.app/Contents/MacOS/plugins"
export PATH="/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/bin:$PATH"

echo "PYTHONPATH=$PYTHONPATH"
echo "DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH"
echo "VLC_PLUGIN_PATH=$VLC_PLUGIN_PATH"

source "/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/bin/activate"
"/Users/santiagovilanova/Documents/_OF/openFrameworks_GIT_PM/apps/Liceu/OceanodeScroller/bin/data/radio/venv/bin/python3" "$@"
