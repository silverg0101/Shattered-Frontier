#!/bin/sh
set -eu
test "$(id -u)" = 998
test -n "${SF_INVENTORY_LEDGER_TOKEN:?Missing private ledger credential}"
test -n "${SF_GAMEPLAY_TOKEN:?Missing private gameplay credential}"
mkdir -p /srv/saved/Logs
: > /srv/saved/Logs/Server.log
executable=$(cat /opt/server/.sf-executable)
exec "$executable" ShatteredFrontier /Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook -port=17777 \
    -SFDevIdentity \
    '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]:bEnabled=True' \
    '-ini:Game:[/Script/ShatteredFrontier.SFInventoryLedgerSettings]:Endpoint=127.0.0.1:50071' \
    '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]:bEnabled=True' \
    '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]:Endpoint=127.0.0.1:50072' \
    '-ini:Game:[/Script/ShatteredFrontier.SFGameplayServicesSettings]:bRestoreOnConnect=True' \
    -unattended -NullRHI -nosound -stdout -FullStdOutLogOutput -abslog=/srv/saved/Logs/Server.log
