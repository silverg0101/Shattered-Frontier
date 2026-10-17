# Legends of the Shattered Frontier - Client Delivery, Launcher, Patching & Update Architecture
## Version 1.0

---

# Purpose

This document serves as the authoritative specification for:

- Game launcher architecture
- Client update workflows
- Version management
- Patch delivery
- Manifest definitions
- Validation systems
- Repair operations
- Release channel management
- CDN migration strategy
- Expansion package deployment
- Security and integrity validation

This architecture is intended to scale from early prototype testing through full MMO launch without requiring redesign.

---

# Core Design Goals

- Automatic updates at startup
- Support full-client and patch-based updates
- Minimal bandwidth consumption
- Strong file integrity guarantees
- Rollback support
- Secure delivery pipeline
- Platform scalability
- Zero manual intervention for players

---

# Startup Update Flow

```text
Player Launches Game
        |
        v
Launcher Starts
        |
        v
Download Master Manifest
        |
        v
Validate Signature
        |
        v
Check Local Version
        |
        v
Validate Local Files
        |
        +----------------+
        |                |
        v                v
 Patch Exists      Full Download
        |                |
        v                v
 Apply Patch    Download Client
        |                |
        +-------+--------+
                |
                v
       Verify Files
                |
                v
         Launch Game
```

---

# Launcher Platform Architecture

```text
+---------------------------------------------------------+
|                    Launcher UI                          |
+---------------------------------------------------------+
                |
                v
+---------------------------------------------------------+
|                 Update Controller                       |
+---------------------------------------------------------+
     |            |            |            |
     v            v            v            v
Authentication  Patching   Validation   Launch
```

---

# Full Launcher Service Architecture

```text
+---------------------------------------------------------+
|                Legends Launcher                         |
+---------------------------------------------------------+
                |
                v
+-----------------------+
| Authentication Layer |
+-----------------------+
                |
                v
+-----------------------+
| Version Service      |
+-----------------------+
                |
                v
+-----------------------+
| Manifest Downloader  |
+-----------------------+
                |
                v
+-----------------------+
| Signature Validation |
+-----------------------+
                |
                v
+-----------------------+
| File Validation      |
+-----------------------+
         |          |
         v          v
 Patch Path   Full Client Path
         |          |
         +-----+----+
               |
               v
+-----------------------+
| Verification Engine   |
+-----------------------+
               |
               v
+-----------------------+
| Launch Game           |
+-----------------------+
```

---

# GitHub Releases Strategy

## Phase 1

Use GitHub Releases as the primary distribution platform.

Release assets:

```text
client_1.0.0.zip
manifest_master.json
manifest_files.json
patch_1.0.0_to_1.0.1.patch
release_notes.md
```

Benefits:

- No infrastructure cost
- Global CDN
- API integration
- Automated release history

Launcher may query:

```text
GitHub Releases API
```

to locate the latest production build.

---

# Future CDN Migration

```text
GitHub Releases
       |
       v
Cloudflare R2
       |
       v
Global CDN
```

Launcher remains unchanged because content locations are defined in manifests.

---

# Manifest System Overview

The launcher never trusts local assets.

All validation begins with server-provided manifests.

Hierarchy:

```text
Master Manifest
       |
       v
File Manifest
       |
       v
Package Manifest
       |
       v
Local Validation
```

---

# Master Manifest Definition

```json
{
  "game":"Legends of the Shattered Frontier",
  "channel":"production",
  "version":"1.5.2",
  "build":10542,
  "minimumLauncherVersion":"1.2.0",
  "releaseDate":"2027-06-03",
  "filesManifest":"manifest_files_10542.json",
  "signature":"SIGNED_HASH"
}
```

Responsibilities:

- Current game version
- Build number
- Patch paths
- Manifest locations
- Security signature

---

# File Manifest Definition

```json
{
  "version":"1.5.2",
  "files":[
    {
      "path":"Core.pak",
      "size":524288000,
      "sha256":"ABCD1234"
    }
  ]
}
```

Responsibilities:

- Every game file
- Expected size
- Expected hash
- Package ownership

---

# Package Manifest Definition

Used for modular updates.

Examples:

```text
Core.pak
Characters.pak
Environment.pak
Audio.pak
UI.pak
Expansion_01.pak
Expansion_02.pak
```

Allows partial updates instead of rebuilding the entire client.

---

# Differential Patch System

Preferred update approach.

```text
Old File
   |
Binary Diff
   |
New File
```

Example patches:

```text
patch_1.5.0_to_1.5.1.patch
patch_1.5.1_to_1.5.2.patch
```

Benefits:

- Reduced downloads
- Faster patching
- Lower CDN costs

---

# Patch Selection Logic

Example:

```text
Installed: 1.5.0
Latest:    1.5.2
```

Apply:

```text
1.5.0 -> 1.5.2 Patch
```

If no valid path exists:

```text
Download Full Client
```

---

# Validation Architecture

Every startup performs:

```text
Validate Launcher
Validate Manifest
Validate Files
Repair If Needed
Launch
```

## Validation Level 1

Existence Check

```text
File Exists?
```

## Validation Level 2

Size Check

```text
Actual Size
Expected Size
```

## Validation Level 3

SHA-256 Verification

```text
Actual Hash
Expected Hash
```

## Validation Level 4

Manifest Signature Validation

```text
Developer Private Key
        |
        v
   Sign Manifest

Launcher Public Key
        |
        v
 Verify Manifest
```

Protects against:

- Modified manifests
- Malicious mirrors
- Tampered downloads

---

# Repair System

User Option:

```text
Repair Installation
```

Process:

```text
Scan Files
Compare Hashes
Identify Corruption
Download Missing Assets
Verify Again
```

Only damaged content is downloaded.

---

# Content Packaging Strategy

```text
Core.pak
Environment.pak
Characters.pak
Audio.pak
UI.pak
Expansion_01.pak
Expansion_02.pak
```

Advantages:

- Faster validation
- Smaller updates
- Expansion isolation
- Better streaming support

---

# Release Channels

## Production
Live servers.

## Public Test Realm
Upcoming features.

## Internal Development
Developer builds.

Each channel maintains its own manifest and release pipeline.

---

# Security Requirements

Launcher validates:

- Manifest signatures
- File hashes
- TLS certificates
- Package versions

Required for all releases.

---

# Telemetry

Track:

- Patch success rate
- Average download size
- Failed validation events
- Repair requests
- Launcher crashes

---

# Recommended Technology Stack

Launcher:

- C# (.NET)
- Rust

Patching:

- xdelta
- bspatch

Storage:

Phase 1
- GitHub Releases

Phase 2
- Cloudflare R2
- AWS S3
- Azure Blob Storage

---

# Final Design Principle

```text
Manifest Driven
Patch First
Full Client Fallback
SHA256 Validation
Signed Releases
Repairable Installs
CDN Agnostic Architecture
```

This architecture provides a complete foundation for client delivery, launcher operations, patching, expansion deployment, version management, and future MMO-scale content distribution.
