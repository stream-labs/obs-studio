#!/bin/bash

# Check if a file path is passed as an argument
if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_binary>"
    exit 1
fi

BINARY_PATH=$1

# Use otool to get the list of linked libraries
LIB_PATHS=$(otool -L "$BINARY_PATH" | grep "obs-deps" | awk '{print $1}')

# Check if any obs-deps libraries were found
if [ -z "$LIB_PATHS" ]; then
    echo "No obs-deps libraries found in $BINARY_PATH."
    exit 0
fi

# Loop through each library path and change it to @rpath using install_name_tool
for OLD_PATH in $LIB_PATHS; do
    LIB_NAME=$(basename "$OLD_PATH")
    NEW_PATH="@rpath/$LIB_NAME"
    echo "Changing $OLD_PATH to $NEW_PATH"
    install_name_tool -change "$OLD_PATH" "$NEW_PATH" "$BINARY_PATH"
done

echo "All obs-deps libraries have been updated to use @rpath in $BINARY_PATH."
