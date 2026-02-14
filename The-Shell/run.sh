#!/bin/bash

# Exit immediately if a command fails
set -e

# Check if a file was provided
if [ $# -lt 1 ]; then
  echo "Usage: ./run.sh <file.cpp>"
  exit 1
fi

SRC="$1"
OUT="${SRC%.cpp}"

# Compile
echo "Compiling $SRC..."
g++ -std=c++17 -w "$SRC" -o "$OUT"

# Run
echo "Running $OUT..."
./"$OUT"
