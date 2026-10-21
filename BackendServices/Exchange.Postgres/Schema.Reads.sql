BEGIN;
DO $$ BEGIN
  IF (SELECT version FROM exchange.schema_version WHERE singleton=true) IS DISTINCT FROM 3 THEN
    RAISE EXCEPTION 'Exchange schema version 3 required';
  END IF;
END $$;
ALTER TABLE exchange.listings ADD COLUMN IF NOT EXISTS market_id text NOT NULL DEFAULT 'market/global', ADD COLUMN IF NOT EXISTS created_at timestamptz NOT NULL DEFAULT clock_timestamp();
ALTER TABLE exchange.settlements ADD COLUMN IF NOT EXISTS party_a text, ADD COLUMN IF NOT EXISTS party_b text;
CREATE INDEX IF NOT EXISTS exchange_active_market_page ON exchange.listings(market_id,item_id,id) WHERE state='active';
CREATE INDEX IF NOT EXISTS exchange_active_market_all_page ON exchange.listings(market_id,id) WHERE state='active';
CREATE INDEX IF NOT EXISTS exchange_pending_trade_a_page ON exchange.settlements(party_a,operation_id) WHERE kind='trade' AND state='pending';
CREATE INDEX IF NOT EXISTS exchange_pending_trade_b_page ON exchange.settlements(party_b,operation_id) WHERE kind='trade' AND state='pending';
COMMIT;
