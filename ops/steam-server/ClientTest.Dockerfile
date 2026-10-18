FROM ubuntu:22.04@sha256:0a9887931b0e38af2e3487ed39e29436b0535c4f3f52e57745baaf79d1898956

# Native Linux diagnostic client only. Validate the actual ELF dependency closure before use.
# The launcher must supply read-only root, private writable Saved/home, dropped capabilities,
# resource/time limits and the temporary diagnostic server's isolated network namespace.
ARG SF_REVISION
LABEL org.opencontainers.image.source="https://github.com/silverg0101/Shattered-Frontier" \
      org.opencontainers.image.revision="$SF_REVISION" \
      com.shatteredfrontier.engine.revision="396c9f059903aed5fec78ecd3d437a40c6415368" \
      com.shatteredfrontier.purpose="native-linux-client-diagnostic"
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates libgcc-s1 libstdc++6 libcurl4 libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 998 sfclient \
    && useradd --uid 998 --gid 998 --home-dir /srv/home --no-create-home --shell /usr/sbin/nologin sfclient
COPY package/ /opt/client/
COPY client-test-entrypoint.sh /usr/local/bin/sf-client-test
RUN set -eu; \
    test "${#SF_REVISION}" = 40; \
    case "$SF_REVISION" in *[!0-9a-f]*) exit 1;; esac; \
    executable="$(find /opt/client -type f -path '*/Binaries/Linux/ShatteredFrontierClient' -print)"; \
    test -n "$executable"; test "$(printf '%s\n' "$executable" | wc -l)" = 1; \
    test -x "$executable"; \
    project="${executable%/Binaries/Linux/ShatteredFrontierClient}"; \
    test -f "$project/ShatteredFrontier.uproject"; \
    printf '%s\n' "$executable" > /opt/client/.sf-client-executable; \
    chmod 0755 /usr/local/bin/sf-client-test
WORKDIR /opt/client
ENV HOME=/srv/home
USER 998:998
ENTRYPOINT ["/usr/local/bin/sf-client-test"]
