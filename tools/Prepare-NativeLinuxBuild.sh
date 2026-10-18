#!/usr/bin/env bash
# Prepare only the already-authorized project's new LV. No formatting/deletion.
set -euo pipefail
readonly BASE=/srv/shattered-frontier
readonly WORK="$BASE/native-build-5.8.3"
readonly ENGINE_COMMIT=396c9f059903aed5fec78ecd3d437a40c6415368
[[ $(id -u) == 0 && $(hostname -s) == steam-server ]] || { echo 'Wrong host/user' >&2; exit 1; }
[[ ! -L "$BASE" && $(findmnt -n -o TARGET --mountpoint "$BASE") == "$BASE" ]] || { echo 'Expected dedicated mount missing' >&2; exit 1; }
[[ $(findmnt -n -o UUID --mountpoint "$BASE") == cade5511-9e1c-4edf-96f9-b16e8ede17fd ]] || { echo 'Unexpected mount UUID' >&2; exit 1; }
[[ $(df -Pk "$BASE" | awk 'NR==2 {print $4}') -ge $((100 * 1024 * 1024)) ]] || { echo 'Need at least100GiB free for preparation' >&2; exit 1; }
if ! id sfbuild >/dev/null 2>&1; then
    useradd --system --user-group --home-dir "$WORK/home" --shell /usr/sbin/nologin sfbuild
fi
readonly BUILD_UID=$(id -u sfbuild)
readonly BUILD_GID=$(id -g sfbuild)
[[ "$BUILD_UID" -ge 100 && "$BUILD_UID" -lt 60000 && "$BUILD_GID" -ge 100 && "$BUILD_GID" -lt 60000 ]] || { echo 'Unexpected build identity' >&2; exit 1; }
[[ $(getent passwd sfbuild | cut -d: -f6) == "$WORK/home" && $(getent passwd sfbuild | cut -d: -f7) == /usr/sbin/nologin ]] || { echo 'Unrelated sfbuild account exists' >&2; exit 1; }
[[ "$BUILD_UID" != "$(id -u sfserver)" && "$BUILD_GID" != "$(id -g sfserver)" \
   && $(getent group "$BUILD_GID" | cut -d: -f1) == sfbuild ]] || { echo 'Build/runtime identities must be separate' >&2; exit 1; }
getent passwd | awk -F: -v uid="$BUILD_UID" '$3 == uid { ++count } END { exit(count == 1 ? 0 : 1) }' \
    || { echo 'Build UID is not unique' >&2; exit 1; }
if pgrep -u "$BUILD_UID" >/dev/null; then
    echo 'Stop existing build jobs before preparing directories' >&2; exit 1
fi
if [[ -e "$WORK" || -L "$WORK" ]]; then
    [[ -d "$WORK" && ! -L "$WORK" && -f "$WORK/.sf-native-build" && ! -L "$WORK/.sf-native-build" \
       && $(cat "$WORK/.sf-native-build") == "$ENGINE_COMMIT" && $(stat -c '%u' "$WORK") == "$BUILD_UID" \
       && $(stat -c '%u' "$WORK/.sf-native-build") == 0 ]] \
       || { echo 'Refusing unrelated existing build path' >&2; exit 1; }
else
    install -d -m 0750 -o sfbuild -g sfbuild "$WORK"
    printf '%s\n' "$ENGINE_COMMIT" > "$WORK/.sf-native-build"
    chmod 0444 "$WORK/.sf-native-build"
fi
for directory in home cache inputs UE_5.8.3 project cook stage archive; do
    path="$WORK/$directory"
    if [[ -e "$path" || -L "$path" ]]; then
        [[ -d "$path" && ! -L "$path" && $(stat -c '%u' "$path") == "$BUILD_UID" ]] || { echo "Unexpected build subdirectory: $path" >&2; exit 1; }
    else
        # Never perform root writes through children of the builder-owned directory.
        runuser -u sfbuild -- install -d -m 0750 "$path"
    fi
done
printf 'SF_NATIVE_BUILD_PREPARED UID=%s GID=%s ROOT=%s SOURCE=%s\n' "$BUILD_UID" "$BUILD_GID" "$WORK" "$ENGINE_COMMIT"
df -h "$BASE"
