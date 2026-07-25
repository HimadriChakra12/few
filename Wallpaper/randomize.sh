#!/bin/bash

WALLPAPER_DIR="$HOME/.config/i3/Wallpaper"
INTERVAL=60   # time in seconds (300 = 5 minutes)

while true; do
    feh --randomize --bg-fill "$WALLPAPER_DIR"
    sleep $INTERVAL
done

