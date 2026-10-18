-- Stage S synthetic catalogue only. Applied independently by each owning migration role.
-- Provisioner precreates fixture schema with this database's NOLOGIN object owner.
CREATE TABLE fixture.identity(fixture_id uuid PRIMARY KEY);
CREATE TABLE fixture.scope(id integer PRIMARY KEY CHECK(id=1), state text NOT NULL,
 incarnation text NOT NULL, manifest text NOT NULL, pending text NOT NULL DEFAULT '',
 revision bigint NOT NULL DEFAULT 1);
CREATE TABLE fixture.actions(id text PRIMARY KEY, realm text NOT NULL, origin text NOT NULL,
 hash text NOT NULL, profile text NOT NULL, incarnation text NOT NULL,
 generation bigint NOT NULL CHECK(generation>0), worker text NOT NULL,
 revision bigint NOT NULL DEFAULT 1, recovering boolean NOT NULL DEFAULT false,
 decision text NOT NULL DEFAULT '', decision_digest text NOT NULL DEFAULT '',
 completion text NOT NULL DEFAULT '', prepared_set text NOT NULL DEFAULT '[]',
 applied_set text NOT NULL DEFAULT '[]', deadline timestamptz NOT NULL,
 fixed_set text NOT NULL CHECK(fixed_set='geometry,material,terrain'));
CREATE TABLE fixture.participants(id text PRIMARY KEY, realm text NOT NULL, origin text NOT NULL,
 hash text NOT NULL, profile text NOT NULL, fixed_set text NOT NULL,
 incarnation text NOT NULL, generation bigint NOT NULL CHECK(generation>0), worker text NOT NULL,
 manifest text NOT NULL, state text NOT NULL DEFAULT 'EMPTY', revision bigint NOT NULL DEFAULT 1,
 prepared text NOT NULL DEFAULT '', applied text NOT NULL DEFAULT '',
 decision_digest text NOT NULL DEFAULT '', completed text NOT NULL DEFAULT '');
CREATE TABLE fixture.resources(id integer PRIMARY KEY, quantity bigint NOT NULL CHECK(quantity>=0));
CREATE TABLE fixture.holds(action_id text NOT NULL REFERENCES fixture.participants(id),
 resource_id integer NOT NULL REFERENCES fixture.resources(id), quantity bigint NOT NULL CHECK(quantity>0),
 PRIMARY KEY(action_id,resource_id));
CREATE TABLE fixture.effects(action_id text PRIMARY KEY REFERENCES fixture.participants(id),
 debit bigint NOT NULL CHECK(debit>=0), output_held boolean NOT NULL DEFAULT true,
 output_available boolean NOT NULL DEFAULT false CHECK(NOT(output_held AND output_available)));
CREATE TABLE fixture.dedup(step_key text PRIMARY KEY, request_hash text NOT NULL, response text NOT NULL);
CREATE TABLE fixture.outbox(step_key text PRIMARY KEY, action_id text NOT NULL,
 kind text NOT NULL CHECK(kind IN('COORDINATOR','PARTICIPANT')),
 phase text NOT NULL, digest text NOT NULL, acknowledged boolean NOT NULL DEFAULT false);
CREATE TABLE fixture.acks(action_id text NOT NULL, incarnation text NOT NULL, generation bigint NOT NULL,
 worker text NOT NULL, participant text NOT NULL, receipt text NOT NULL,
 PRIMARY KEY(action_id,incarnation,generation,worker,participant));
CREATE TABLE fixture.mappings(action_id text PRIMARY KEY, origin text NOT NULL,
 old_incarnation text NOT NULL, old_manifest text NOT NULL, new_incarnation text NOT NULL,
 old_generation bigint NOT NULL CHECK(old_generation>0), old_worker text NOT NULL,
 manifest text NOT NULL, worker text NOT NULL);
CREATE TABLE fixture.audit(sequence bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 action_id text NOT NULL, operation text NOT NULL, phase text NOT NULL,
 backend_pid integer NOT NULL DEFAULT pg_backend_pid(), at timestamptz NOT NULL DEFAULT clock_timestamp());
