#!/usr/bin/env bash

TARGET="${1:-communicatorsrv}"
INTERVAL="${2:-1}"

# Resolve PID
if [[ "$TARGET" =~ ^[0-9]+$ ]]; then
    PID="$TARGET"
else
    PID=$(pgrep -f "$TARGET" | grep -v "$$" | head -n 1)
fi

if [ -z "$PID" ] || [ ! -d "/proc/$PID" ]; then
    echo "[-] Process not found. Run: ps aux | grep communicatorsrv"
    exit 1
fi

echo "[+] Attached to PID: $PID"

# Get clock ticks per second (usually 100 on Linux)
CLK_TCK=$(getconf CLK_TCK)

# Initial tick read
read -r _ _ _ _ _ _ _ _ _ _ _ _ _ utime1 stime1 _ < "/proc/$PID/stat"

while true; do
    sleep "$INTERVAL"

    if [ ! -d "/proc/$PID" ]; then
        echo "[-] Process $PID terminated."
        exit 0
    fi

    # Read current process CPU ticks and memory
    read -r _ _ _ _ _ _ _ _ _ _ _ _ _ utime2 stime2 _ < "/proc/$PID/stat" 2>/dev/null
    rss_pages=$(awk '/VmRSS/{print $2}' "/proc/$PID/status" 2>/dev/null) # in KB

    # Calculate CPU ticks delta
    delta_ticks=$(( (utime2 + stime2) - (utime1 + stime1) ))
    utime1=$utime2
    stime1=$stime2

    # Calculate CPU %: (delta_ticks / CLK_TCK) / INTERVAL * 100
    cpu_pct=$(awk -v d="$delta_ticks" -v hz="$CLK_TCK" -v i="$INTERVAL" \
        'BEGIN {printf "%.2f", (d / hz / i) * 100}')

    # Calculate approximate CPU cycles consumed in that interval (based on core clock)
    # 1 tick = (CPU_FREQ / CLK_TCK) cycles
    cpu_freq_khz=$(awk -F': ' '/cpu MHz/{print int($2 * 1000); exit}' /proc/cpuinfo)
    est_cycles=$(awk -v d="$delta_ticks" -v hz="$CLK_TCK" -v freq="$cpu_freq_khz" \
        'BEGIN {printf "%.2fM", (d * (freq * 1000 / hz)) / 1000000}')

    # Calculate RAM in MB (2 decimal places)
    rss_mb=$(awk -v r="${rss_pages:-0}" 'BEGIN {printf "%.2f", r/1024}')

    # Render output
    printf "\r[PID: %s] | CPU: %6s%% | Active Cycles: %10s/s | RAM: %7s MB " \
        "$PID" "$cpu_pct" "$est_cycles" "$rss_mb"
done