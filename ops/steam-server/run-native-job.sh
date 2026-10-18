#!/usr/bin/env bash
# Fixed, auditable development jobs; never exposes a server or runs as root in-container.
set -euo pipefail
readonly WORK=/srv/shattered-frontier/native-build-5.8.3
readonly RUNTIME=/srv/shattered-frontier/runtime/native
readonly IMAGE=shattered-frontier-native-builder:ue583
[[ $(id -u) == 0 && $(hostname -s) == steam-server && ! -L "$WORK" ]] || exit 1
[[ $(findmnt -n -o UUID --mountpoint /srv/shattered-frontier) == cade5511-9e1c-4edf-96f9-b16e8ede17fd ]] || exit 1
[[ $(cat "$WORK/.sf-native-build") == 396c9f059903aed5fec78ecd3d437a40c6415368 ]] || exit 1
readonly buildUid=$(id -u sfbuild)
readonly buildGid=$(id -g sfbuild)
[[ "$buildUid" -ge 100 && "$buildUid" -lt 60000 && "$buildGid" -ge 100 && "$buildGid" -lt 60000 \
   && "$buildUid" != "$(id -u sfserver)" && "$buildGid" != "$(id -g sfserver)" ]] || exit 1
network=bridge
case "${1:-}" in
    setup)
        command=(bash ./Setup.sh --force --threads=2 --no-cache --exclude=Mac --exclude=Android --exclude=TVOS --exclude=VisionOS --exclude=LinuxArm64)
        additionalGiB=95
        ;;
    uat-help)
        command=(bash Engine/Build/BatchFiles/RunUAT.sh BuildCookRun -help)
        additionalGiB=10
        ;;
    stage)
        [[ -f "$WORK/project/GameClient/Binaries/Linux/ShatteredFrontierServer.target" \
           && -f "$WORK/UE_5.8.3/Engine/Binaries/Linux/UnrealPak.target" \
           && -f "$WORK/cook/LinuxServer/ShatteredFrontier/Content/Frontier/Maps/Greenlands_E1.umap" ]] || exit 1
        command=(bash Engine/Build/BatchFiles/RunUAT.sh BuildCookRun
            -project=/work/project/GameClient/ShatteredFrontier.uproject
            -skipbuild -skipcook -stage -pak -skipiostore -archive
            -server -noclient -serverplatform=Linux -serverarchitecture=x64 -serverconfig=Development
            -CookOutputDir=/work/cook/LinuxServer -stagingdirectory=/work/stage -archivedirectory=/work/archive
            -unattended -nop4 -utf8output)
        additionalGiB=20
        network=none
        ;;
    *) echo 'Usage: run-native-job.sh setup|uat-help|stage' >&2; exit 1 ;;
esac
# Fixed ceilings preserve the existing Windrose workload. They are build policy,
# not public/GM runtime controls; changing them requires a reviewed script release.
# Conservative remaining-work budget plus25GiB headroom, not a filesystem quota.
[[ $(df -Pk "$WORK" | awk 'NR==2 {print $4}') -ge $(((25 + additionalGiB) * 1024 * 1024)) ]] || { echo 'Insufficient work budget plus25GiB headroom' >&2; exit 1; }
exec 9>"$RUNTIME/job.lock"
flock -n 9 || { echo 'Another launcher is active' >&2; exit 1; }
if docker ps --filter label=sf.native-build=ue583 --format '{{.ID}}' | grep -q .; then
    echo 'A native build job is already running' >&2; exit 1
fi
job="sf-native-${1}-$(date -u +%Y%m%d-%H%M%S)-$$"
docker run --detach --name "$job" --label sf.native-build=ue583 \
    --cpus=2 --memory=8g --memory-swap=8g --pids-limit=512 --cap-drop=ALL \
    --security-opt=no-new-privileges --read-only --tmpfs /tmp:rw,nosuid,nodev,size=1g \
    --user "$buildUid:$buildGid" --mount "type=bind,src=$WORK,dst=/work" \
    --network="$network" --log-driver=json-file --log-opt max-size=10m --log-opt max-file=3 \
    "$IMAGE" "${command[@]}"
printf 'SF_NATIVE_JOB_STARTED Name=%s Mode=%s UID=%s CPU=2 MemoryGiB=8\n' "$job" "$1" "$buildUid"
