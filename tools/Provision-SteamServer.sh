#!/usr/bin/env bash
# User-authorized Ubuntu host preparation. Never formats an existing logical volume.
set -euo pipefail
test "$(id -u)" = 0
test "$(hostname -s)" = steam-server
vg=ubuntu-vg
lv=sf-game
device=/dev/$vg/$lv
destination=/srv/shattered-frontier
test ! -L "$destination"
if mountpoint -q "$destination"; then
    test -b "$device"
    test -n "$(blkid -s UUID -o value "$device")"
    test "$(findmnt -n -o UUID --target "$destination")" = "$(blkid -s UUID -o value "$device")"
elif test -d "$destination" && test -n "$(find "$destination" -mindepth 1 -maxdepth 1 -print -quit)"; then
    echo 'Refusing to mount over existing game data' >&2
    exit 1
fi
if ! lvs "$vg/$lv" >/dev/null 2>&1; then
    # Separate 200GiB allocation; leave the OS LV and existing second disk intact.
    lvcreate --yes --size 200G --name "$lv" "$vg"
    test -z "$(blkid -o value -s TYPE "$device" || true)"
    mkfs.ext4 -L SF_GAME "$device"
fi
test "$(blkid -s TYPE -o value "$device")" = ext4
test "$(blkid -s LABEL -o value "$device")" = SF_GAME
uuid=$(blkid -s UUID -o value "$device")
test -n "$uuid"
mkdir -p "$destination"
awk -v source="UUID=$uuid" -v path="$destination" '
    NF && $1 !~ /^#/ {
        if ($2 == path) { count++; if ($1 != source || $3 != "ext4") bad=1 }
        if ($1 == source && $2 != path) bad=1
    }
    END {exit (bad || count > 1)}' /etc/fstab || {
    echo 'Conflicting fstab source/destination; inspect before proceeding' >&2
    exit 1
}
if ! awk -v path="$destination" '$1 !~ /^#/ && $2 == path {found=1} END {exit !found}' /etc/fstab; then
    cp -a /etc/fstab "/etc/fstab.sf-$(date -u +%Y%m%dT%H%M%SZ)"
    printf 'UUID=%s %s ext4 defaults,nodev,nosuid,nofail 0 2\n' "$uuid" "$destination" >> /etc/fstab
fi
findmnt --verify --tab-file /etc/fstab >/dev/null
if ! mountpoint -q "$destination"; then mount "$destination"; fi
test "$(findmnt -n -o UUID --mountpoint "$destination")" = "$uuid"
test "$(findmnt -n -o FSTYPE --mountpoint "$destination")" = ext4
test "$(findmnt -n -o FSROOT --mountpoint "$destination")" = /
if ! getent passwd sfserver >/dev/null; then
    useradd --system --user-group --home-dir "$destination/home" --shell /usr/sbin/nologin sfserver
fi
test "$(id -u sfserver)" != 0
install -d -o root -g root -m 0755 "$destination/releases"
install -d -o sfserver -g sfserver -m 0750 "$destination/shared" "$destination/home"
findmnt "$destination"
df -h "$destination"
id sfserver
echo 'Storage and non-root runtime account prepared. No game service/container was started.'
