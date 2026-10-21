#!/usr/bin/env bash
# Builds the pinned gRPC C++ release for UE 5.8 Linux x86_64 Server targets, with prefixed BoringSSL
# (INT-03 step 0b). Pins: GameClient/Source/ThirdParty/gRPC/gRPC.lock.json. Output:
# <repo>/Artifacts/ThirdParty/gRPC/install/Linux with manifest.json. Runs on a Linux host or inside the
# steam-server native builder (run-native-job.sh grpc-linux); needs git, curl, tar, unzip and network.
#   build-grpc-linux.sh <ue-engine-root> <repo-root> <work-dir>
set -euo pipefail
readonly ENGINE=$(realpath -e "$1") REPO=$(realpath -e "$2") WORK=$3
readonly LOCK="$REPO/GameClient/Source/ThirdParty/gRPC/gRPC.lock.json"
readonly INSTALL="$REPO/Artifacts/ThirdParty/gRPC/install/Linux"
mkdir -p "$WORK/tools" "$WORK/dl"
PY="$ENGINE/Engine/Binaries/ThirdParty/Python3/Linux/bin/python3"; [[ -x "$PY" ]] || PY=python3
lock() {  # lock <key>... prints a string value, or "key value" lines for an object
    "$PY" - "$LOCK" "$@" <<'PY'
import json, sys
value = json.load(open(sys.argv[1]))
for key in sys.argv[2:]:
    value = value[key]
print(value if isinstance(value, str) else "\n".join(f"{k} {v}" for k, v in value.items()))
PY
}

fetch() {  # fetch <name> -> verified archive path
    local url sha file; url=$(lock linuxTools "$1" url); sha=$(lock linuxTools "$1" sha256); file="$WORK/dl/$(basename "$url")"
    [[ -f "$file" ]] || curl -fsSL --retry 3 -o "$file" "$url"
    echo "$sha  $file" | sha256sum -c --quiet - || { echo "Hash mismatch: $file" >&2; exit 1; }
    echo "$file"
}
[[ -x "$WORK/tools/cmake/bin/cmake" ]] || { mkdir -p "$WORK/tools/cmake"; tar -xzf "$(fetch cmake)" -C "$WORK/tools/cmake" --strip-components=1; }
[[ -x "$WORK/tools/ninja" ]] || unzip -oq "$(fetch ninja)" -d "$WORK/tools"
[[ -x "$WORK/tools/go/bin/go" ]] || tar -xzf "$(fetch go)" -C "$WORK/tools"
export PATH="$WORK/tools/cmake/bin:$WORK/tools:$WORK/tools/go/bin:$PATH" GOFLAGS=-mod=mod GOTOOLCHAIN=local
export GOCACHE="$WORK/gocache" GOPATH="$WORK/gopath"

readonly SRC="$WORK/src"
[[ -d "$SRC/.git" ]] || git clone -q --depth 1 --branch "$(lock grpc tag)" -c advice.detachedHead=false "$(lock grpc repository)" "$SRC"
[[ $(git -C "$SRC" rev-parse HEAD) == "$(lock grpc commit)" ]] || { echo 'gRPC checkout does not match the lock' >&2; exit 1; }
while read -r path commit; do
    git -C "$SRC" submodule update --init --depth 1 -q "$path"
    [[ $(git -C "$SRC/$path" rev-parse HEAD) == "$commit" ]] || { echo "$path does not match the lock" >&2; exit 1; }
done < <(lock submodules; lock linuxSubmodules)

export SF_LINUX_SDK_ROOT=$(find "$ENGINE/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/$(lock linuxToolchain sdk)" -maxdepth 1 -type d -name "$(lock linuxToolchain triple)" | head -1)
[[ -d "$SF_LINUX_SDK_ROOT" ]] || { echo 'UE Linux SDK not found; run the engine Setup first' >&2; exit 1; }
readonly ZLIB="$ENGINE/Engine/Source/ThirdParty/zlib/$(lock zlib version)"
readonly PREFIX=$(lock linuxTls prefix)
common=(-S "$SRC" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$REPO/tools/grpc-linux-toolchain.cmake" -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_STANDARD=17 -DABSL_PROPAGATE_CXX_STD=ON -DBUILD_SHARED_LIBS=OFF
    -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DgRPC_DOWNLOAD_ARCHIVES=OFF -DgRPC_BUILD_CODEGEN=OFF
    -DBUILD_TESTING=OFF -DRE2_BUILD_TESTING=OFF -DCARES_BUILD_TESTS=OFF -Dutf8_range_ENABLE_TESTS=OFF
    -Dprotobuf_BUILD_PROTOC_BINARIES=OFF -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_WITH_ZLIB=OFF
    -DgRPC_ABSL_PROVIDER=module -DgRPC_PROTOBUF_PROVIDER=module -DgRPC_RE2_PROVIDER=module -DgRPC_CARES_PROVIDER=module
    -DgRPC_SSL_PROVIDER=module -DgRPC_ZLIB_PROVIDER=package
    -DZLIB_INCLUDE_DIR="$ZLIB/include" -DZLIB_LIBRARY="$ZLIB/lib/Unix/$(lock linuxToolchain triple)/Release/libz.a")

# Pass 1: unprefixed BoringSSL, only to list its exported symbols.
cmake "${common[@]}" -B "$WORK/build-pass1" >/dev/null
cmake --build "$WORK/build-pass1" --target ssl crypto
readonly SYMBOLS="$WORK/boringssl-symbols.txt" PREFIX_INCLUDE="$WORK/prefix-include"
(cd "$SRC/third_party/boringssl-with-bazel/util" && go run read_symbols.go -obj-file-format elf -out "$SYMBOLS" \
    "$WORK/build-pass1/third_party/boringssl-with-bazel/libcrypto.a" "$WORK/build-pass1/third_party/boringssl-with-bazel/libssl.a")
# gRPC's own TLS code includes <openssl/*.h> too, so every target sees the prefix headers, not only BoringSSL's.
mkdir -p "$PREFIX_INCLUDE"
(cd "$SRC/third_party/boringssl-with-bazel/util" && go run make_prefix_headers.go -out "$PREFIX_INCLUDE" "$SYMBOLS")

# Pass 2: everything, with BoringSSL symbols renamed to sfbssl_* so they cannot clash with engine OpenSSL.
rm -rf "$WORK/build" "$INSTALL"
# The toolchain file reads SF_LINUX_EXTRA_FLAGS; CMAKE_<LANG>_FLAGS here would drop its libc++ flags.
SF_LINUX_EXTRA_FLAGS="-DBORINGSSL_PREFIX=$PREFIX -I$PREFIX_INCLUDE" cmake "${common[@]}" -B "$WORK/build" -DCMAKE_INSTALL_PREFIX="$INSTALL" \
    -DBORINGSSL_PREFIX="$PREFIX" -DBORINGSSL_PREFIX_SYMBOLS="$SYMBOLS" -DGO_EXECUTABLE="$(command -v go)" >/dev/null
cmake --build "$WORK/build"
cmake --install "$WORK/build" >/dev/null

# Fail closed if any OpenSSL-named symbol escaped the prefix.
nm="$SF_LINUX_SDK_ROOT/bin/x86_64-unknown-linux-gnu-nm"
if "$nm" -g --defined-only "$INSTALL/lib/libssl.a" "$INSTALL/lib/libcrypto.a" 2>/dev/null | awk '$2 ~ /[TDBR]/ {print $3}' | grep -E '^(SSL|EVP|X509|BIO|ERR|CRYPTO)_' | head -1 | grep -q .; then
    echo 'Unprefixed BoringSSL symbols found in the install' >&2; exit 1
fi
{
    printf '{\n  "grpcCommit": "%s",\n  "tls": "PrefixedBoringSSL:%s",\n  "libraries": {\n' "$(lock grpc commit)" "$PREFIX"
    (cd "$INSTALL/lib" && sha256sum *.a | sort -k2 | awk '{printf "%s    \"%s\": \"%s\"", (NR>1?",\n":""), $2, $1} END {print ""}')
    printf '  }\n}\n'
} > "$INSTALL/manifest.json"
echo "SF_GRPC_LINUX_DONE Install=$INSTALL Libraries=$(ls "$INSTALL/lib"/*.a | wc -l)"
