#!/bin/sh
set -eu
# Fixed E1 map is versioned content; port is an integer deployment setting.
port=${SF_GAME_PORT:?Set the validated UDP port}
case "$port" in ''|*[!0-9]*) echo 'SF_GAME_PORT must be an integer' >&2; exit 64;; esac
if [ "$port" -lt 1024 ] || [ "$port" -gt 65535 ]; then
    echo 'SF_GAME_PORT must be within 1024..65535' >&2
    exit 64
fi
test "$(id -u)" != 0
mkdir -p /srv/saved/Logs
executable=$(cat /opt/server/.sf-executable)
exec "$executable" /Game/Frontier/Maps/Greenlands_E1 "-port=$port" \
    -unattended -NullRHI -nosound -stdout -FullStdOutLogOutput -abslog=/srv/saved/Logs/Server.log
