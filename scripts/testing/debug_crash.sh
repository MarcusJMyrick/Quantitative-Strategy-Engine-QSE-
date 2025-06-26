#!/bin/bash

echo "Running strategy engine with crash debugging..."
echo "Core dump location: /cores/"

# Run the program and capture output
./build/strategy_engine 2>&1 | tee crash_output.log

# Check if program crashed and core dump was created
if [ $? -ne 0 ]; then
    echo "Program crashed with exit code $?"
    
    # Look for core dumps
    echo "Checking for core dumps..."
    ls -la /cores/
    
    # If we have a core dump, analyze it
    if [ -f /cores/core.* ]; then
        echo "Found core dump, analyzing with LLDB..."
        COREFILE=$(ls /cores/core.* | head -1)
        lldb ./build/strategy_engine -c "$COREFILE" -o "bt" -o "quit"
    fi
else
    echo "Program completed successfully"
fi 