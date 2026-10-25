# Pinned official ASP.NET 10.0.13 manifest, resolved on the authorized host.
FROM mcr.microsoft.com/dotnet/aspnet:10.0.13@sha256:8af87eba5e7cdafb1514d7388be287365da97c132509e84dfe252ffd552398ca
ARG SF_REVISION
LABEL org.opencontainers.image.revision=$SF_REVISION
WORKDIR /opt/services
COPY Gameplay.Host ./Gameplay.Host
COPY InventoryLedger.Host ./InventoryLedger.Host
COPY SchemaMigration.Tool ./SchemaMigration.Tool
COPY content ./content
COPY base-schema ./base-schema
COPY Runtime ./Runtime
COPY release.json ./release.json
COPY Dockerfile ./Dockerfile
COPY SHA256SUMS ./SHA256SUMS
RUN test "${#SF_REVISION}" = 40 \
    && case "$SF_REVISION" in *[!0-9a-f]*) exit 1;; esac \
    && grep -F "\"revision\": \"$SF_REVISION\"" release.json >/dev/null \
    && sha256sum -c SHA256SUMS --quiet
ENV DOTNET_EnableDiagnostics=0 HOME=/tmp
USER 998:998
ENTRYPOINT ["dotnet"]
CMD ["Gameplay.Host/Gameplay.Host.dll"]
