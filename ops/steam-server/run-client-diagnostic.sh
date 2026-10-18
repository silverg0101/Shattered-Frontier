#!/usr/bin/env bash
# Finite, isolated native packaged-client evidence collection; never declares gameplay PASS.
set -euo pipefail
readonly VOLUME=/srv/shattered-frontier
readonly REVISION=2a5b634429b75d46519ad2a28560b8953ed99831
readonly ENGINE=396c9f059903aed5fec78ecd3d437a40c6415368
readonly SERVER_IMAGE=sha256:42b382b6e3d581388be3bc8341a10ba7ff08876e9b0788457f71cb8e53d6f0ce
[[ $# -ge 1 && $# -le 2 && $1 =~ ^sha256:[0-9a-f]{64}$ ]] || { echo 'Supply an audited diagnostic Client image SHA256 ID and optional --capture-elapsed-trace' >&2; exit 1; }
readonly CLIENT_IMAGE=$1
capture_trace=false
trace_arguments=()
if [[ $# == 2 ]]; then
    [[ $2 == --capture-elapsed-trace ]] || { echo 'Only --capture-elapsed-trace is supported' >&2; exit 1; }
    capture_trace=true
    trace_arguments=(-trace=cpu,frame,counters,log -tracefile=/srv/saved/Server.utrace -statnamedevents)
fi
[[ $(id -u) == 0 && $(hostname -s) == steam-server ]] || exit 1
[[ $(findmnt -n -o UUID --mountpoint "$VOLUME") == cade5511-9e1c-4edf-96f9-b16e8ede17fd ]] || exit 1
[[ $(df -Pk "$VOLUME" | awk 'NR==2 {print $4}') -ge $((10 * 1024 * 1024)) ]] || exit 1
for image in "$SERVER_IMAGE" "$CLIENT_IMAGE"; do
    [[ $(docker image inspect --format '{{index .Config.Labels "org.opencontainers.image.revision"}}' "$image") == "$REVISION" ]] || exit 1
    [[ $(docker image inspect --format '{{index .Config.Labels "com.shatteredfrontier.engine.revision"}}' "$image") == "$ENGINE" ]] || exit 1
done
[[ $(docker image inspect --format '{{index .Config.Labels "com.shatteredfrontier.purpose"}}' "$CLIENT_IMAGE") == native-linux-client-diagnostic ]] || exit 1
readonly BASE="$VOLUME/diagnostics"
[[ ! -L "$BASE" ]] || exit 1
install -d -m 0700 -o root -g root "$BASE"
[[ $(realpath -e "$BASE") == "$BASE" ]] || exit 1
exec 9>"$BASE/client-diagnostic.lock"
flock -n 9 || { echo 'Another client diagnostic is active' >&2; exit 1; }
readonly NAME="sf-client-diag-$(date -u +%Y%m%d-%H%M%S)-$$"
readonly ROOT="$BASE/$NAME"
[[ ! -e "$ROOT" && ! -L "$ROOT" ]] || exit 1
install -d -m 0700 -o root -g root "$ROOT"
for role in Server Driver Observer; do
    install -d -m 0700 -o 998 -g 998 "$ROOT/$role"
done
owned=()
cleanup_done=false
cleanup() {
    if $cleanup_done; then return 0; fi
    cleanup_done=true
    local role id failed=0
    # Reverse order stops clients before their private server namespace.
    # Docker writes cidfiles on creation, covering interrupted/failed run calls too.
    for role in Observer Driver Server; do
        [[ -f "$ROOT/$role.cid" ]] || continue
        id=$(cat "$ROOT/$role.cid")
        if [[ ! "$id" =~ ^[0-9a-f]{64}$ ]] || \
           [[ $(docker inspect --format '{{index .Config.Labels "com.shatteredfrontier.diagnostic"}}' "$id" 2>/dev/null) != "$NAME" ]]; then
            echo "Refusing cleanup of unverified $role container" >&2; failed=1; continue
        fi
        docker stop --time 10 "$id" >/dev/null 2>&1 || failed=1
        case $(docker inspect --format '{{.State.Running}} {{.State.OOMKilled}} {{.RestartCount}} {{.State.ExitCode}}' "$id" 2>/dev/null) in
            'false false 0 0'|'false false 0 143') ;;
            *) echo "Unexpected final state for $role" >&2; failed=1 ;;
        esac
        docker inspect "$id" >"$ROOT/container-$role.json" 2>/dev/null || failed=1
        docker logs "$id" >"$ROOT/container-$role.log" 2>&1 || failed=1
    done
    return "$failed"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
common=(--user 998:998 --read-only --init --restart=no --ulimit core=0
    --label "com.shatteredfrontier.diagnostic=$NAME"
    --cap-drop ALL --security-opt no-new-privileges
    --memory 2g --memory-swap 2g --pids-limit 256
    --tmpfs /tmp:rw,nosuid,nodev,size=256m,mode=1777
    --tmpfs /srv/home:rw,nosuid,nodev,size=64m,mode=0700,uid=998,gid=998
    --log-driver json-file --log-opt max-size=10m --log-opt max-file=2)
server=$(docker run -d --name "$NAME-server" --cidfile "$ROOT/Server.cid" "${common[@]}" --cpus 1 --network none \
    --mount "type=bind,src=$ROOT/Server,dst=/srv/saved" \
    --entrypoint /usr/bin/timeout "$SERVER_IMAGE" \
    --signal=TERM --kill-after=10s 90s /bin/sh -ec '
        mkdir -p /srv/saved/Logs
        exec "$(cat /opt/server/.sf-executable)" ShatteredFrontier /Game/Frontier/Maps/Greenlands_E1 \
            -port=17779 -MULTIHOME=127.0.0.1 -E1ProbeRole=Server \
            -NullRHI -nosound -unattended -stdout -FullStdOutLogOutput \
            -UserDir=/srv/home -abslog=/srv/saved/Logs/Server.log \
            -PktLag=0 -PktLagVariance=0 -PktLoss=0 -PktOrder=0 -PktDup=0 \
            -PktIncomingLagMin=0 -PktIncomingLagMax=0 "$@"' sf-native-server "${trace_arguments[@]}")
owned+=("$server")
running() { [[ $(docker inspect --format '{{.State.Running}} {{.State.OOMKilled}} {{.RestartCount}}' "$1") == 'true false 0' ]]; }
deadline=$((SECONDS + 15))
until [[ -f "$ROOT/Server/Logs/Server.log" ]] && grep -q 'E1_WORLD_READY.*NetMode=1' "$ROOT/Server/Logs/Server.log"; do
    running "$server" || { echo 'Diagnostic server exited before ready' >&2; exit 1; }
    (( SECONDS < deadline )) || { echo 'Diagnostic server ready timeout' >&2; exit 1; }
    sleep 0.25
done
for role in Driver Observer; do
    client=$(docker run -d --name "$NAME-${role,,}" --cidfile "$ROOT/$role.cid" "${common[@]}" --cpus 0.5 \
        --network "container:$server" --mount "type=bind,src=$ROOT/$role,dst=/srv/saved" \
        --entrypoint /usr/bin/timeout "$CLIENT_IMAGE" --signal=TERM --kill-after=10s 90s \
        /usr/local/bin/sf-client-test "$role" 127.0.0.1:17779)
    owned+=("$client")
done
deadline=$((SECONDS + 45))
while true; do
    for id in "${owned[@]}"; do
        running "$id" || { echo 'Diagnostic process exited early' >&2; exit 1; }
    done
    complete=true
    for role in Server Driver Observer; do
        log="$ROOT/$role/Logs/$role.log"
        if [[ ! -f "$log" ]]; then complete=false; continue; fi
        if grep -Eq 'NetworkFailure|ConnectionTimeout|OutdatedClient|OutdatedServer|Fatal error:|Assertion failed:' "$log"; then
            echo "Diagnostic $role reported failure" >&2; exit 1
        fi
        grep -q "E1_SCENARIO_START Role=$role DurationSeconds=30" "$log" || complete=false
        grep -q "E1_SCENARIO_DONE Role=$role" "$log" || complete=false
    done
    if $complete; then break; fi
    (( SECONDS < deadline )) || { echo 'Diagnostic scenario timeout' >&2; exit 1; }
    sleep 0.25
done
for id in "${owned[@]}"; do running "$id" || exit 1; done
cleanup
for role in Server Driver Observer; do
    if grep -Eq 'NetworkFailure|ConnectionTimeout|OutdatedClient|OutdatedServer|Fatal error:|Assertion failed:' \
        "$ROOT/$role/Logs/$role.log" "$ROOT/container-$role.log"; then
        echo "Diagnostic $role reported failure in finalized logs" >&2; exit 1
    fi
done
if $capture_trace; then
    [[ -s "$ROOT/Server/Server.utrace" ]] || { echo 'Missing server trace after orderly shutdown' >&2; exit 1; }
    printf 'SF_ELAPSED_TRACE_COLLECTED Path=%s\n' "$ROOT/Server/Server.utrace"
    echo 'Trace scopes measure elapsed time, including waits/descheduling. Successful complete-file analysis is still required.'
fi
printf 'SF_DIAGNOSTIC_COLLECTED Root=%s Game=%s ServerImage=%s ClientImage=%s\n' "$ROOT" "$REVISION" "$SERVER_IMAGE" "$CLIENT_IMAGE"
echo 'Collection only: independently validate welcomes, identity, possession, movement, jump and probe logs before gameplay acceptance.'
