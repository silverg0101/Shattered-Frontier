-- Existing characters retain the Fighter allocation until their gameplay state is restored.
ALTER TABLE identity.characters
    ADD COLUMN IF NOT EXISTS archetype_id text NOT NULL DEFAULT 'fighter';
DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_constraint WHERE conrelid = 'identity.characters'::regclass
                   AND conname = 'characters_archetype_id_check') THEN
        ALTER TABLE identity.characters ADD CONSTRAINT characters_archetype_id_check
            CHECK (archetype_id IN ('fighter','guardian','archer','mage','healer','shadow'));
    END IF;
END $$;
