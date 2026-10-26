#!/usr/bin/env bash
# Host side of tools/Deploy-Lan.ps1. Runs as steam; privileged steps use sudo -n.
#   deploy-lan.sh [--dry-run] preflight CONTEXT_BYTES
#   deploy-lan.sh [--dry-run] deploy REVISION GAME_IMAGE_ID SERVICES_IMAGE_ID TOOL
#   deploy-lan.sh [--dry-run] rollback
# Releases live in /srv/shattered-frontier/releases/playable-<rev>/ (profile.final.json,
# Runtime/playable-runtime.py); deploy-state names the current and previous revision.
set -uo pipefail
RELEASES=/srv/shattered-frontier/releases
STATE=$RELEASES/deploy-state
BACKUPS=/srv/shattered-frontier/backups
GAME=shattered-frontier-playable-game-1
DATABASE=shattered-frontier-playable-database-1
DRY=0
[ "${1:-}" = --dry-run ] && { DRY=1; shift; }

step() { echo "== $*"; }
die() { echo "FAIL: $*" >&2; exit 1; }
# Mutating commands: printed under --dry-run, run under sudo -n otherwise.
act() { if [ $DRY = 1 ]; then echo "   would run: $*"; else sudo -n "$@" </dev/null; fi; }

require_sudo() {
    sudo -n true 2>/dev/null || die "sudo -n is unavailable; an operator must open a sudo session (sudo -v) on steam-server first"
    step "sudo -n available"
}

# current/previous from the state file; before the first deploy, current is the running game's revision.
current=; previous=
if [ -f "$STATE" ]; then . "$STATE"; else
    current=$(docker inspect "$GAME" --format '{{index .Config.Labels "org.opencontainers.image.revision"}}' 2>/dev/null)
fi
dir_of() { echo "$RELEASES/playable-$1"; }

healthy() {
    [ "$(docker inspect "$GAME" --format '{{.State.Health.Status}}' 2>/dev/null)" = healthy ] || { echo "   game container not healthy"; return 1; }
    ss -Hlun | grep -q '192.168.50.133:17777 ' || { echo "   UDP 17777 not bound"; return 1; }
    echo "   game healthy, 192.168.50.133:17777/udp bound"
}

runtime() { # REV ACTION
    local d; d=$(dir_of "$1")
    step "playable-runtime $2 ($1)"
    act python3 "$d/Runtime/playable-runtime.py" "$d/profile.final.json" "$2"
}

switch_to() { # REV — start, verify, health; on failure start FALLBACK again
    local target=$1 fallback=$2
    if runtime "$target" start && runtime "$target" verify && { step "health"; [ $DRY = 1 ] || healthy; }; then
        return 0
    fi
    echo "!! $target failed; starting $fallback again" >&2
    runtime "$fallback" start && runtime "$fallback" verify && healthy
    die "switch to $target failed; $fallback restored (check its output above)"
}

backup() { # LABEL
    local file; file="$BACKUPS/sf_playable-pre-$1-$(date -u +%Y%m%dT%H%M%SZ).dump"
    step "backup $file (keep newest 5)"
    act sh -c "docker exec $DATABASE pg_dump -U sf_admin -Fc sf_playable > '$file' && chmod 600 '$file'" || die "pg_dump failed"
    act sh -c "ls -1t $BACKUPS/sf_playable-*.dump | tail -n +6 | xargs -r rm --"
}

write_state() { # CURRENT PREVIOUS
    step "state current=$1 previous=$2"
    if [ $DRY = 1 ]; then echo "   would write $STATE"; else
        printf 'current=%s\nprevious=%s\n' "$1" "$2" | sudo -n tee "$STATE" >/dev/null
    fi
}

cmd=${1:-}; shift || true
require_sudo
[ -n "$current" ] && [ -f "$(dir_of "$current")/profile.final.json" ] || die "no current profile (current='$current')"
step "current $current, previous ${previous:-none}"

case $cmd in
preflight)
    [ $# = 1 ] || die "usage: preflight CONTEXT_BYTES"
    free=$(df -B1 --output=avail "$(docker info -f '{{.DockerRootDir}}')" | tail -1)
    need=$(( 2 * $1 + 4 * 1024 ** 3 ))
    step "docker free $free bytes, need $need"
    [ "$free" -ge "$need" ] || die "not enough Docker free space"
    step "health"; healthy || die "current release is not healthy; fix it before deploying"
    ;;
deploy)
    [ $# = 4 ] || die "usage: deploy REVISION GAME_IMAGE_ID SERVICES_IMAGE_ID TOOL"
    rev=$1; game=$2; services=$3; tool=$4
    [[ $rev =~ ^[a-f0-9]{40}$ ]] || die "full revision required"
    [ "$rev" != "$current" ] || die "$rev is already current"
    for id in "$game" "$services"; do
        [ "$(docker image inspect "$id" --format '{{index .Config.Labels "org.opencontainers.image.revision"}}')" = "$rev" ] \
            || die "image $id is not labelled $rev"
    done
    new=$(dir_of "$rev"); tmp=$(mktemp)
    python3 - "$(dir_of "$current")/profile.final.json" "$rev" "$game" "$services" > "$tmp" <<'PY' || die "profile write failed"
import json, sys
p = json.load(open(sys.argv[1]))
p.update(revision=sys.argv[2], game_image=sys.argv[3], services_image=sys.argv[4])
print(json.dumps(p, indent=2))
PY
    backup "${rev:0:8}"
    step "profile $new/profile.final.json"
    act install -d -m 0755 "$new" "$new/Runtime"
    act install -m 0644 "$tmp" "$new/profile.final.json"
    act install -m 0755 "$tool" "$new/Runtime/playable-runtime.py"
    [ $DRY = 1 ] && { echo "   new profile:"; sed 's/^/   /' "$tmp"; }
    rm -f "$tmp"
    if [ $DRY = 1 ]; then echo "   (check needs the installed profile; skipped)"; else runtime "$rev" check || die "new profile check failed; nothing switched"; fi
    switch_to "$rev" "$current"
    write_state "$rev" "$current"
    ;;
rollback)
    [ -n "$previous" ] && [ -f "$(dir_of "$previous")/profile.final.json" ] || die "no previous profile to roll back to"
    backup "rollback-${previous:0:8}"
    switch_to "$previous" "$current"
    write_state "$previous" "$current"
    ;;
*) die "usage: deploy-lan.sh [--dry-run] preflight|deploy|rollback ..." ;;
esac
step "OK"
