#!/bin/sh
set -x # show cmds
set -e # fail globally

# Check if sda exists before running dd
if [ -b /dev/sda ]; then
    dd if=DualFuse.iso of=/dev/sda bs=1M status=progress && sync
else
    echo "Error: /dev/sda not found"
fi