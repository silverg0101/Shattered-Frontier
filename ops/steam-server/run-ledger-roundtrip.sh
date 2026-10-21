#!/usr/bin/env bash
# INT-03 step 0b proof: the packaged Linux ShatteredFrontierServer reaches InventoryLedger.Host over
# loopback gRPC, and a call to a silent peer ends at its deadline. Three throwaway containers share one
# network namespace that has only loopback (--network none on the first, container:<first> on the rest):
# PostgreSQL (tmpfs), the host (published framework-dependent build), and the release server image run
# with -SFLedgerGrpcProbe. Nothing is published or persisted; all three are removed afterwards.
#   sudo run-ledger-roundtrip.sh <release-image> <published-host-dir> <Schema.sql>
# Image digests: keep in step with GameClient/Source/ThirdParty/gRPC/gRPC.lock.json roundTripImages.
set -euo pipefail
readonly PG_IMAGE='postgres:18.6-bookworm@sha256:bbf83594798e144bff365a9c191c5d779f181fb3135902064ab8ff11918020fc'
readonly HOST_IMAGE='mcr.microsoft.com/dotnet/aspnet:10.0.13-noble@sha256:568a21ba4e54dc68251d80e57f95d484028d15682218c3378a96b3db812bb2b7'
readonly RELEASE_IMAGE=$1 HOST_DIR=$(realpath -e "$2") SCHEMA=$(realpath -e "$3")
[[ $(id -u) == 0 && $(hostname -s) == steam-server ]] || { echo 'Run as root on steam-server' >&2; exit 1; }
[[ -f "$HOST_DIR/InventoryLedger.Host.dll" ]] || { echo 'Published host dir lacks InventoryLedger.Host.dll' >&2; exit 1; }
docker image inspect "$RELEASE_IMAGE" >/dev/null
run=$(date -u +%Y%m%d-%H%M%S)-$$
pg=sf-ledger-rt-pg-$run host=sf-ledger-rt-host-$run server=sf-ledger-rt-server-$run
token=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
password=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
cleanup() { docker rm -f "$server" "$host" "$pg" >/dev/null 2>&1 || true; }
trap cleanup EXIT
hardening=(--cap-drop=ALL --security-opt=no-new-privileges --pids-limit=256 --memory=2g --cpus=1)

# ponytail: superuser on a tmpfs database in a loopback-only namespace; the Windows fixture keeps the
# separate owner/service roles. Add them here if this ever outlives a single proof run.
docker run -d --name "$pg" --network none --tmpfs /var/lib/postgresql:rw,size=256m \
    -e POSTGRES_PASSWORD="$password" -e POSTGRES_DB=ledger --memory=1g --cpus=1 "$PG_IMAGE" >/dev/null
for _ in $(seq 60); do docker exec "$pg" pg_isready -q -h 127.0.0.1 -U postgres -d ledger && break; sleep 1; done
docker exec "$pg" pg_isready -q -h 127.0.0.1 -U postgres -d ledger || { echo 'Postgres did not become ready' >&2; docker logs "$pg" 2>&1 | tail -20; exit 1; }
docker exec -i "$pg" psql -q -v ON_ERROR_STOP=1 -h 127.0.0.1 -U postgres -d ledger < "$SCHEMA"

docker run -d --name "$host" --network "container:$pg" --read-only --user 65534:65534 "${hardening[@]}" \
    --mount "type=bind,src=$HOST_DIR,dst=/host,readonly" -e INVENTORY_LEDGER_PORT=50071 -e SF_INVENTORY_LEDGER_TOKEN="$token" \
    -e INVENTORY_LEDGER_CONNECTION="Host=127.0.0.1;Port=5432;Database=ledger;Username=postgres;Password=$password;Pooling=false;Timeout=2" \
    -e DOTNET_EnableDiagnostics=0 --entrypoint dotnet "$HOST_IMAGE" /host/InventoryLedger.Host.dll >/dev/null
for _ in $(seq 60); do docker logs "$host" 2>&1 | grep -q 'INVENTORY_LEDGER_HOST_READY port=50071' && break; sleep 1; done
docker logs "$host" 2>&1 | grep -q 'INVENTORY_LEDGER_HOST_READY' || { echo 'Host did not become ready' >&2; docker logs "$host" 2>&1 | tail -20; exit 1; }

# The release entrypoint only starts the E1 map; this one-off run calls the same executable with the probe.
executable=$(docker run --rm --entrypoint cat "$RELEASE_IMAGE" /opt/server/.sf-executable)
set +e
docker run --name "$server" --network "container:$pg" --read-only --tmpfs /tmp:rw,size=256m --tmpfs /srv/saved:rw,size=256m \
    --tmpfs /srv/home:rw,size=64m "${hardening[@]}" --memory=4g --cpus=2 -e SF_INVENTORY_LEDGER_TOKEN="$token" --entrypoint "$executable" "$RELEASE_IMAGE" \
    ShatteredFrontier /Game/Frontier/Maps/Greenlands_E1 -port=17790 -unattended -NullRHI -nosound -stdout -FullStdOutLogOutput \
    -SFLedgerGrpcProbe=127.0.0.1:50071 > "/tmp/$server.log" 2>&1
code=$?
set -e
grep -E 'GRPC_(ROUNDTRIP|DEADLINE)_' "/tmp/$server.log" || true
docker logs "$host" 2>&1 | sed "s/$token/[REDACTED]/g" | tail -5
if [[ $code == 0 ]] && grep -q 'GRPC_ROUNDTRIP_OK' "/tmp/$server.log" && grep -q 'GRPC_DEADLINE_EXCEEDED_OK' "/tmp/$server.log"; then
    echo "SF_LEDGER_ROUNDTRIP_PASSED Image=$RELEASE_IMAGE"
else
    echo "SF_LEDGER_ROUNDTRIP_FAILED Exit=$code Log=/tmp/$server.log" >&2; exit 1
fi
