#!/bin/sh
set -eu
# Fixed versioned native diagnostic fixture; no arbitrary endpoint or engine arguments.
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo 'Usage: client-test-entrypoint.sh Driver|Observer [127.0.0.1:17779]' >&2
    exit 64
fi
role=$1
case "$role" in Driver|Observer) ;; *) echo 'Role must be Driver or Observer' >&2; exit 64;; esac
server=${2:-127.0.0.1:17779}
if [ "$server" != 127.0.0.1:17779 ]; then
    echo 'Only the isolated diagnostic endpoint 127.0.0.1:17779 is allowed' >&2
    exit 64
fi
if [ "$(id -u)" != 998 ]; then
    echo 'Native diagnostic client must run as UID998' >&2
    exit 64
fi
mkdir -p /srv/saved/Logs
test -d /srv/home && test -w /srv/home
executable=$(cat /opt/client/.sf-client-executable)
test -x "$executable"
exec "$executable" ShatteredFrontier "$server" \
    -NullRHI -nosound -unattended -nop4 -nosplash -stdout -FullStdOutLogOutput \
    "-E1ProbeRole=$role" -UserDir=/srv/home "-abslog=/srv/saved/Logs/$role.log" \
    -PktLag=0 -PktLagVariance=0 -PktLoss=0 -PktOrder=0 -PktDup=0 -PktIncomingLagMin=0 -PktIncomingLagMax=0
