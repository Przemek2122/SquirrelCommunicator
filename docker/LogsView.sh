#!/usr/bin/env bash

CONTAINERS=("communicator-backend-1" "communicator-voice_service-1")

clear_logs() {
  echo "=== Clearing log files... ==="
  for name in "${CONTAINERS[@]}"; do
    id=$(docker inspect --format='{{.Id}}' "$name" 2>/dev/null)
    if [ -n "$id" ]; then
      log_path="/var/lib/docker/containers/${id}/${id}-json.log"
      if [ -f "$log_path" ]; then
        sudo truncate -s 0 "$log_path"
        echo "Truncated: $name"
      fi
    fi
  done
}

if [[ "$1" == "--clear" || "$1" == "-c" ]]; then
  clear_logs
  exit 0
fi

# Streaming logic
echo "=== Streaming logs for: ${CONTAINERS[*]} ==="
docker logs -f --tail 100 -t "${CONTAINERS[0]}" &
PID1=$!

docker logs -f --tail 100 -t "${CONTAINERS[1]}" &
PID2=$!

trap 'kill $PID1 $PID2 2>/dev/null; exit 0' INT TERM EXIT
wait
