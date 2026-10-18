FROM ubuntu:22.04@sha256:0a9887931b0e38af2e3487ed39e29436b0535c4f3f52e57745baaf79d1898956

# Development builder only. No published port, Docker socket or existing-game mount.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential ca-certificates curl git unzip xz-utils libicu70 libssl3 \
    zlib1g libkrb5-3 libunwind8 && rm -rf /var/lib/apt/lists/*
ARG SF_BUILD_UID
ARG SF_BUILD_GID
RUN test "$SF_BUILD_UID" -ge 100 && test "$SF_BUILD_UID" -lt 60000 \
    && test "$SF_BUILD_GID" -ge 100 && test "$SF_BUILD_GID" -lt 60000 \
    && groupadd --gid "$SF_BUILD_GID" sfbuild \
    && useradd --uid "$SF_BUILD_UID" --gid "$SF_BUILD_GID" --no-create-home \
       --home-dir /work/home --shell /usr/sbin/nologin sfbuild
ENV HOME=/work/home DOTNET_CLI_HOME=/work/home DOTNET_CLI_TELEMETRY_OPTOUT=1 \
    DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 NUGET_PACKAGES=/work/cache/nuget
USER sfbuild
WORKDIR /work/UE_5.8.3
