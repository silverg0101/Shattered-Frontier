FROM ubuntu:22.04@sha256:0a9887931b0e38af2e3487ed39e29436b0535c4f3f52e57745baaf79d1898956
ARG SF_UID=998
ARG SF_GID=998
ARG SF_REVISION
LABEL org.opencontainers.image.source="https://github.com/silverg0101/Shattered-Frontier" \
      org.opencontainers.image.revision="$SF_REVISION" \
      com.shatteredfrontier.engine.revision="396c9f059903aed5fec78ecd3d437a40c6415368"
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libgcc-s1 libstdc++6 libcurl4 libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid "$SF_GID" sfserver \
    && useradd --uid "$SF_UID" --gid "$SF_GID" --home-dir /srv/home --no-create-home --shell /usr/sbin/nologin sfserver
COPY package/ /opt/server/
COPY entrypoint.sh /usr/local/bin/sf-entrypoint
RUN set -eu; \
    executable="$(find /opt/server -type f -path '*/Binaries/Linux/ShatteredFrontierServer' -print)"; \
    test -n "$executable"; test "$(printf '%s\n' "$executable" | wc -l)" = 1; \
    chmod 0755 "$executable" /usr/local/bin/sf-entrypoint; \
    printf '%s\n' "$executable" > /opt/server/.sf-executable; \
    project="${executable%/Binaries/Linux/ShatteredFrontierServer}"; \
    test ! -e "$project/Saved"; \
    ln -s /srv/saved "$project/Saved"
WORKDIR /opt/server
ENV HOME=/srv/home
USER sfserver
ENTRYPOINT ["/usr/local/bin/sf-entrypoint"]
