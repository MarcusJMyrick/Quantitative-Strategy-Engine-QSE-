#!/bin/bash

# Quick organize script - simple wrapper for basic organization
# Usage: ./scripts/quick_organize.sh [run_name]

RUN_NAME="$1"

if [[ -n "$RUN_NAME" ]]; then
    ./scripts/organize_and_analyze.sh -n "$RUN_NAME"
else
    ./scripts/organize_and_analyze.sh
fi 