# Calendars

## SQLite

Run `planner-calendars` from `build/service-executables`. The default config listens on localhost:5702 and uses `./db/calendars.sqlite`. The build copies the database from `services/calendars/calendars.sqlite` when the destination is missing. Rebuilding preserves the runtime database. A native build requires Drogon with SQLite ORM support and a standard library that supports C++20 time zones and `std::chrono::from_stream`.

The seed uses [001_schema.sqlite.sql](sql/001_schema.sqlite.sql). For an older runtime database with the `sources` table and no calendar subscriptions, keep that file as a backup and copy the current seed to `build/service-executables/db/calendars.sqlite` before starting the service.

To upgrade a database that already has the `calendars` table and predates the access columns, apply [004_migrate_calendar_access.sqlite.sql](sql/004_migrate_calendar_access.sqlite.sql) once. Run the migration with the service stopped. It keeps existing calendar IDs, subscriptions and events. Enable `PRAGMA foreign_keys = ON` on connections to this database. The default service config sets it when opening the connection.

## PostgreSQL

Use PostgreSQL 18 or later for `uuidv7()` and a Drogon build with PostgreSQL ORM support.

For a local server, create the role and database from an administrative `psql` session.

```sql
CREATE ROLE calendars LOGIN;
\password calendars
CREATE DATABASE calendars OWNER calendars;
```

Apply the schema from the repository root.

```sh
psql -h 127.0.0.1 -U calendars -d calendars -v ON_ERROR_STOP=1 -f services/calendars/sql/001_schema.sql
```

For an existing PostgreSQL database whose `calendars` primary key is `id`, rerun the same command to add the access columns and table.

For Docker, start the optional database and wait for its health check.

```sh
docker compose --profile postgres up -d --build --wait postgres planner
```

PostgreSQL applies the schema automatically on first initialization of the `calendars-postgres` volume. To move an existing database from PostgreSQL 16, back it up with `pg_dump` and restore it to an empty database on PostgreSQL 18. Apply `sql/000_migrate_postgresql.sql` and then `sql/001_schema.sql` to the restored database before starting calendars.

In the runtime `config/calendars.json`, comment out the SQLite `rdbms` and `filename`, uncomment the PostgreSQL settings, and set the credentials. Use `127.0.0.1` for a native server or `postgres` from the Planner container. An existing database with the previous Calendars schema also needs the migration before applying the new schema.

## Calendar access

- `public` calendars allow reading with no grant. `private` calendars are visible only to their owner. `protected` calendars are visible to the owner and users with a grant.
- `calendars.owner_id` identifies the owner. Private and protected calendars require one. Existing calendars remain public with no owner assigned.
- `calendar_access` stores one grant per protected calendar and user. `r` is read, `w` includes editing, and `s` includes managing grants. Ownership comes from `owner_id`. Changing a protected calendar to a different visibility clears its grants.

Subscriptions still store which calendars a user follows. The current API reads subscriptions without evaluating visibility or grants, so enabling restricted calendars for users depends on the backend access checks.

## RUZ

The RUZ plugin reads `custom_config.sources.ruz`. By default it imports seven days on startup and at 05:00 and 18:00 in `Europe/Moscow`. It refreshes the relevant timetable half an hour before each lesson. The settings `sync.static` and `sync.pre_lesson` take hours. Times are stored in UTC. The host needs the IANA time zone database.

The plugin uses a persistent HTTP client and spaces requests by `min_request_interval_seconds`. It retries transient failures after `retry_interval_seconds`.

Use `POST /api/calendars/calendars` with `{"name": "GROUP_NAME"}` to subscribe through the gateway. Use `GET /api/calendars/calendars` to list subscriptions, `GET /api/calendars/events?from=YYYY-MM-DD&to=YYYY-MM-DD` to list a date range, and `GET /api/calendars/events/active` to list unfinished events. The gateway supplies the account in `user-id`.

## Outbox

Changes to imported lessons add entries to `outbox_events`. Set `url` in the `api::calendars::outbox_relay` plugin settings to enable delivery. The relay sends a JSON object with `id` and `payload` to that endpoint with POST. It checks the queue on startup and every `retry_minutes` minutes, and removes an entry after a successful response. A consumer can use `id` to deduplicate retries. With no URL, entries stay in the database.
