# Legends of the Shattered Frontier - Database Architecture
## Version 1.0

# Design Goals
- Data integrity
- Horizontal scalability
- Fast recovery
- Persistence of world changes

# Database Stack
## PostgreSQL
Stores:
- Accounts
- Characters
- Guilds
- Quests
- Economy
- Marketplace

## Redis
Stores:
- Sessions
- Cache
- Presence
- Queues

## Object Storage
Stores:
- Backups
- Logs
- World snapshots
- Analytics

# Core Domains
## Account Database
Authentication and account security.

## Character Database
Skills, attributes, inventory, progression.

## World Database
Structures, territory ownership, terrain deltas.

## Economy Database
Auctions, trades, vendor statistics.

## Analytics Warehouse
Telemetry and balancing data.

# Backup Strategy
- Hourly transaction backups
- Daily snapshots
- Weekly archival backups

# Replication
Primary -> Regional Replica -> Disaster Recovery Region

# Recovery Targets
RPO < 15 minutes
RTO < 60 minutes

# Security
- Encryption at rest
- Encryption in transit
- Audit trails
- Role-based access control
