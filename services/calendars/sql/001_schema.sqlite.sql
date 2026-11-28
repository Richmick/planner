PRAGMA foreign_keys = ON;
BEGIN;

CREATE TABLE IF NOT EXISTS calendar_types
(
	id TEXT PRIMARY KEY NOT NULL CHECK(length(id) <= 7),
	description TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS calendars
(
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	type_id TEXT NOT NULL REFERENCES calendar_types(id),
	external_id TEXT NOT NULL,
	metadata TEXT NOT NULL DEFAULT '{}',
	created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
	visibility TEXT NOT NULL DEFAULT 'public' CHECK(visibility IN ('public', 'private', 'protected')),
	owner_id INTEGER CHECK(owner_id IS NULL OR (typeof(owner_id) = 'integer' AND owner_id > 0))
		CHECK(visibility = 'public' OR owner_id IS NOT NULL),
	UNIQUE (type_id, external_id)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_calendars_id_visibility ON calendars (id, visibility);

CREATE TABLE IF NOT EXISTS calendar_access
(
	calendar_id INTEGER NOT NULL,
	user_id INTEGER NOT NULL CHECK(typeof(user_id) = 'integer' AND user_id > 0),
	access TEXT NOT NULL CHECK(access IN ('r', 'w', 's')),
	visibility TEXT GENERATED ALWAYS AS ('protected') STORED,
	PRIMARY KEY (calendar_id, user_id),
	FOREIGN KEY (calendar_id, visibility) REFERENCES calendars (id, visibility) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS events
(
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	calendar_id INTEGER NOT NULL REFERENCES calendars(id) ON DELETE CASCADE,
	external_id TEXT CHECK(length(external_id) <= 64),
	name TEXT NOT NULL,
	global_scheduled TEXT NOT NULL DEFAULT '[]',
	source_data TEXT NOT NULL DEFAULT '{}',
	finished INTEGER NOT NULL DEFAULT 0,
	start_after TEXT,
	finish_before TEXT,
	overlap TEXT NOT NULL DEFAULT 'inclusive',
	visibility TEXT NOT NULL DEFAULT 'all',
	interruptible INTEGER NOT NULL DEFAULT 1,
	base_dependencies TEXT,
	base_restrictions TEXT,
	version INTEGER NOT NULL DEFAULT 1,
	updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
	UNIQUE (calendar_id, external_id)
);

CREATE TABLE IF NOT EXISTS event_overrides
(
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	event_id INTEGER NOT NULL REFERENCES events(id) ON DELETE CASCADE,
	user_id INTEGER NOT NULL,
	local_scheduled TEXT,
	confirmed TEXT,
	finished INTEGER,
	success INTEGER,
	global_transform TEXT,
	dependencies TEXT,
	restrictions TEXT,
	updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
	UNIQUE (event_id, user_id)
);

CREATE TABLE IF NOT EXISTS subscriptions
(
	user_id INTEGER NOT NULL,
	calendar_id INTEGER NOT NULL REFERENCES calendars(id) ON DELETE CASCADE,
	subscribed_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
	settings TEXT NOT NULL DEFAULT '{}',
	PRIMARY KEY (user_id, calendar_id)
);

-- UUIDv7 starts with the 48-bit Unix timestamp in milliseconds.
CREATE TABLE IF NOT EXISTS outbox_events
(
	id TEXT PRIMARY KEY NOT NULL DEFAULT (
		substr(printf('%012x', 1000 * strftime('%s', 'now') + substr(strftime('%f', 'now'), 4)), 1, 8) || '-' ||
		substr(printf('%012x', 1000 * strftime('%s', 'now') + substr(strftime('%f', 'now'), 4)), 9, 4) ||
		'-7' || substr(lower(hex(randomblob(2))), 2) || '-' ||
		substr('89ab', (random() & 3) + 1, 1) || substr(lower(hex(randomblob(2))), 2) || '-' ||
		lower(hex(randomblob(6)))
	),
	payload TEXT NOT NULL CHECK(json_valid(payload))
);

CREATE INDEX IF NOT EXISTS idx_events_calendar ON events (calendar_id, finish_before);
CREATE INDEX IF NOT EXISTS idx_overrides_user ON event_overrides (user_id, updated_at);
CREATE INDEX IF NOT EXISTS idx_subscriptions_calendar ON subscriptions (calendar_id);

CREATE TRIGGER IF NOT EXISTS calendar_access_on_visibility_change
BEFORE UPDATE OF visibility ON calendars
WHEN OLD.visibility = 'protected' AND NEW.visibility <> 'protected'
BEGIN
	DELETE FROM calendar_access WHERE calendar_id = OLD.id;
END;

INSERT INTO calendar_types (id, description)
VALUES ('ruz', 'Расписание учебных групп СПбПУ')
ON CONFLICT (id) DO NOTHING;

CREATE TRIGGER IF NOT EXISTS ruz_event_inserted
AFTER INSERT ON events WHEN NEW.external_id IS NOT NULL
BEGIN
	INSERT INTO outbox_events (payload)
	SELECT json_object(
		'change', 'insert',
		'calendar_type', c.type_id,
		'calendar_id', NEW.calendar_id,
		'event_id', NEW.id,
		'version', NEW.version,
		'external_id', NEW.external_id,
		'name', NEW.name,
		'scheduled', json(NEW.global_scheduled),
		'source_data', json(NEW.source_data)
	)
	FROM calendars c
	WHERE c.id = NEW.calendar_id AND c.type_id = 'ruz';
END;

CREATE TRIGGER IF NOT EXISTS ruz_event_updated
AFTER UPDATE OF name, global_scheduled, source_data, finish_before ON events
WHEN NEW.external_id IS NOT NULL AND (
	OLD.name IS NOT NEW.name OR
	OLD.global_scheduled IS NOT NEW.global_scheduled OR
	OLD.source_data IS NOT NEW.source_data OR
	OLD.finish_before IS NOT NEW.finish_before
)
BEGIN
	INSERT INTO outbox_events (payload)
	SELECT json_object(
		'change', 'update',
		'calendar_type', c.type_id,
		'calendar_id', NEW.calendar_id,
		'event_id', NEW.id,
		'version', NEW.version,
		'external_id', NEW.external_id,
		'name', NEW.name,
		'scheduled', json(NEW.global_scheduled),
		'source_data', json(NEW.source_data)
	)
	FROM calendars c
	WHERE c.id = NEW.calendar_id AND c.type_id = 'ruz';
END;

CREATE TRIGGER IF NOT EXISTS ruz_event_deleted
AFTER DELETE ON events WHEN OLD.external_id IS NOT NULL
BEGIN
	INSERT INTO outbox_events (payload)
	SELECT json_object(
		'change', 'delete',
		'calendar_type', c.type_id,
		'calendar_id', OLD.calendar_id,
		'event_id', OLD.id,
		'version', OLD.version,
		'external_id', OLD.external_id,
		'name', OLD.name,
		'scheduled', json(OLD.global_scheduled),
		'source_data', json(OLD.source_data)
	)
	FROM calendars c
	WHERE c.id = OLD.calendar_id AND c.type_id = 'ruz';
END;

COMMIT;
