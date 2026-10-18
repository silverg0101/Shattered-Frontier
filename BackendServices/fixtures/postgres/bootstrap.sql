-- Executed only by Test-H2MigrationFixture.ps1 in its newly initialized private cluster.
-- psql variables are supplied from a temporary private input file, never arguments.
CREATE ROLE own_building NOLOGIN;
CREATE ROLE own_world NOLOGIN;
CREATE ROLE mig_building LOGIN NOINHERIT PASSWORD :'migration_password';
CREATE ROLE svc_building LOGIN PASSWORD :'runtime_password';
CREATE ROLE ro_building LOGIN PASSWORD :'readonly_password';
GRANT own_building TO mig_building;
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
SELECT format('REVOKE CREATE, TEMP ON DATABASE %I FROM PUBLIC', current_database()) \gexec
CREATE SCHEMA fixture_control;
REVOKE ALL ON SCHEMA fixture_control FROM PUBLIC;
CREATE TABLE fixture_control.identity(singleton boolean PRIMARY KEY CHECK(singleton), id uuid NOT NULL, system_identifier text NOT NULL);
INSERT INTO fixture_control.identity SELECT true, :'fixture_id'::uuid, system_identifier::text FROM pg_control_system();
GRANT USAGE ON SCHEMA fixture_control TO mig_building;
GRANT SELECT ON fixture_control.identity TO mig_building;
CREATE FUNCTION fixture_control.server_identity() RETURNS text
LANGUAGE sql STABLE SECURITY DEFINER SET search_path = pg_catalog
AS 'SELECT system_identifier::text FROM pg_catalog.pg_control_system()';
REVOKE ALL ON FUNCTION fixture_control.server_identity() FROM PUBLIC;
GRANT EXECUTE ON FUNCTION fixture_control.server_identity() TO mig_building;
CREATE SCHEMA building AUTHORIZATION own_building;
GRANT USAGE ON SCHEMA building TO svc_building, ro_building;
ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO svc_building;
ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT SELECT ON TABLES TO ro_building;
ALTER DEFAULT PRIVILEGES FOR ROLE own_building IN SCHEMA building GRANT USAGE ON SEQUENCES TO svc_building;
CREATE SCHEMA world AUTHORIZATION own_world;
SET ROLE own_world;
CREATE TABLE world.sentinel(id integer PRIMARY KEY, value text NOT NULL);
INSERT INTO world.sentinel VALUES(1, 'unchanged');
RESET ROLE;
ALTER ROLE mig_building SET search_path = pg_catalog;
ALTER ROLE svc_building SET search_path = pg_catalog;
ALTER ROLE ro_building SET search_path = pg_catalog;
