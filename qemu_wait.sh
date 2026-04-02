#!/bin/bash
echo "[qemu_wait] building and Starting QEMU..."
setsid ./qemu.sh > logs/qemu.log 2>&1 &
QEMU_PID=$!

echo "[qemu_wait] QEMU PID: $QEMU_PID"
sleep 0.5

if ! kill -0 $QEMU_PID 2>/dev/null; then
    echo "[qemu_wait] QEMU died immediately!"
    exit 1
fi

echo "[qemu_wait] Polling port 1234..."
TRIES=0
while ! bash -c 'echo >/dev/tcp/localhost/1234' 2>/dev/null; do
    sleep 0.2
    TRIES=$((TRIES+1))
    if [ $TRIES -gt 100 ]; then
        echo "[qemu_wait] timeout!"
        exit 1
    fi
done

echo "[qemu_wait] port 1234 open, GDB can connect"