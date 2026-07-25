#!/bin/bash

# Show a "recognizing..." notification (while songrec is working)
notify-send "Recognizing..." "Please wait while the song is being recognized."

# Run songrec recognize and capture both stdout and stderr
song_info=$(songrec recognize 2>&1)

# Check if the output is empty or there was an error
if [[ -z "$song_info" || "$song_info" == *"error"* ]]; then
    notify-send "Recognition Failed" "No song recognized or an error occurred."
    exit 1
fi

# Extract the song name (it should be the second line of the output)
song_name=$(echo "$song_info" | sed -n '2p')

# If no song name was extracted, show an error
if [[ -z "$song_name" ]]; then
    notify-send "Recognition Failed" "Song name could not be extracted."
    exit 1
fi

# Copy the song name to the clipboard using xclip
echo "$song_name" | xclip -selection clipboard

# Show a final notification with the recognized song name
notify-send "Song Recognized" "Now playing: $song_name"
