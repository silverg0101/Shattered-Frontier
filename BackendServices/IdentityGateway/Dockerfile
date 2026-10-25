FROM mcr.microsoft.com/dotnet/sdk:10.0.402 AS build
WORKDIR /src
COPY BackendServices/ BackendServices/
RUN dotnet restore BackendServices/IdentityGateway/IdentityGateway.csproj --locked-mode
RUN dotnet publish BackendServices/IdentityGateway/IdentityGateway.csproj -c Release --no-restore -o /app

FROM mcr.microsoft.com/dotnet/aspnet:10.0
WORKDIR /app
COPY --from=build /app/ ./
COPY content/appearance/human.json content/appearance/human.json
ENTRYPOINT ["dotnet", "IdentityGateway.dll"]
