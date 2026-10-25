#!/bin/sh
set -eu

for name in account-data-kek-v1 identity-connection identity-maintenance-connection chat-connection \
    ticket-signing-key world-verifier-key chat-verifier-key data-key-chat-key privacy-workflow-key \
    chat-token chat-privacy-token gameplay-token guild-token; do
    systemd-creds decrypt --name="$name" "/etc/credstore.encrypted/$name" "/run/sf-secrets/$name"
    chown "$SF_UID:$SF_GID" "/run/sf-secrets/$name"
    chmod 0400 "/run/sf-secrets/$name"
done
chown "$SF_UID:$SF_GID" /run/sf-secrets
chmod 0700 /run/sf-secrets
