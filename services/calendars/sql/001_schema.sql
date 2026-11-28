BEGIN;

CREATE TABLE IF NOT EXISTS calendar_types
(
	id VARCHAR(7) PRIMARY KEY,
	description TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS calendars
(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	type_id VARCHAR(7) NOT NULL REFERENCES calendar_types(id),
	external_id VARCHAR(255) NOT NULL,
	metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
	created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
	UNIQUE (type_id, external_id)
);

-- Calendars created by earlier versions have no recorded owner.
ALTER TABLE calendars
	ADD COLUMN IF NOT EXISTS visibility VARCHAR(9) NOT NULL DEFAULT 'public'
		CHECK (visibility IN ('public', 'private', 'protected')),
	ADD COLUMN IF NOT EXISTS owner_id BIGINT
		CHECK (owner_id > 0)
		CONSTRAINT calendars_visibility_requires_owner
			CHECK (visibility = 'public' OR owner_id IS NOT NULL);

CREATE UNIQUE INDEX IF NOT EXISTS idx_calendars_id_visibility ON calendars (id, visibility);

CREATE TABLE IF NOT EXISTS calendar_access
(
	calendar_id BIGINT NOT NULL,
	user_id BIGINT NOT NULL CHECK (user_id > 0),
	access TEXT NOT NULL CHECK (access IN ('r', 'w', 's')),
	visibility VARCHAR(9) GENERATED ALWAYS AS ('protected') STORED NOT NULL,
	PRIMARY KEY (calendar_id, user_id),
	FOREIGN KEY (calendar_id, visibility) REFERENCES calendars (id, visibility) ON DELETE CASCADE
);

CREATE OR REPLACE FUNCTION clear_calendar_access() RETURNS TRIGGER
LANGUAGE plpgsql AS $$
BEGIN
	DELETE FROM calendar_access WHERE calendar_id = OLD.id;
	RETURN NEW;
END;
$$;

CREATE OR REPLACE TRIGGER calendar_no_longer_protected
BEFORE UPDATE ON calendars FOR EACH ROW
WHEN (OLD.visibility = 'protected' AND NEW.visibility <> 'protected')
EXECUTE FUNCTION clear_calendar_access();

CREATE TABLE IF NOT EXISTS events
(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	calendar_id BIGINT NOT NULL REFERENCES calendars(id) ON DELETE CASCADE,
	external_id VARCHAR(64),
	name TEXT NOT NULL,
	global_scheduled JSONB NOT NULL DEFAULT '[]'::jsonb,
	source_data JSONB NOT NULL DEFAULT '{}'::jsonb,
	finished BOOLEAN NOT NULL DEFAULT FALSE,
	start_after TIMESTAMPTZ,
	finish_before TIMESTAMPTZ,
	overlap VARCHAR(20) NOT NULL DEFAULT 'inclusive',
	visibility VARCHAR(20) NOT NULL DEFAULT 'all',
	interruptible BOOLEAN NOT NULL DEFAULT TRUE,
	base_dependencies JSONB,
	base_restrictions JSONB,
	version INT NOT NULL DEFAULT 1,
	updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
	UNIQUE (calendar_id, external_id)
);

CREATE TABLE IF NOT EXISTS event_overrides
(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	event_id BIGINT NOT NULL REFERENCES events(id) ON DELETE CASCADE,
	user_id BIGINT NOT NULL,
	local_scheduled JSONB,
	confirmed JSONB,
	finished BOOLEAN,
	success BOOLEAN,
	global_transform JSONB,
	dependencies JSONB,
	restrictions JSONB,
	updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
	UNIQUE (event_id, user_id)
);

CREATE TABLE IF NOT EXISTS subscriptions
(
	user_id BIGINT NOT NULL,
	calendar_id BIGINT NOT NULL REFERENCES calendars(id) ON DELETE CASCADE,
	subscribed_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
	settings JSONB NOT NULL DEFAULT '{}'::jsonb,
	PRIMARY KEY (user_id, calendar_id)
);

CREATE TABLE IF NOT EXISTS outbox_events
(
	id UUID PRIMARY KEY DEFAULT uuidv7(),
	payload JSONB NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_events_calendar ON events (calendar_id, finish_before);
CREATE INDEX IF NOT EXISTS idx_overrides_user ON event_overrides (user_id, updated_at);
CREATE INDEX IF NOT EXISTS idx_subscriptions_calendar ON subscriptions (calendar_id);

INSERT INTO calendar_types (id, description)
VALUES ('ruz', 'Расписание учебных групп СПбПУ')
ON CONFLICT (id) DO NOTHING;

CREATE OR REPLACE FUNCTION enqueue_ruz_event() RETURNS TRIGGER
LANGUAGE plpgsql AS $$
DECLARE
	lesson events%ROWTYPE;
BEGIN
	IF TG_OP = 'DELETE' THEN
		lesson := OLD;
	ELSE
		lesson := NEW;
	END IF;

	IF lesson.external_id IS NOT NULL THEN
		INSERT INTO outbox_events (payload)
		SELECT jsonb_build_object(
			'change', LOWER(TG_OP),
			'calendar_type', c.type_id,
			'calendar_id', lesson.calendar_id,
			'event_id', lesson.id,
			'version', lesson.version,
			'external_id', lesson.external_id,
			'name', lesson.name,
			'scheduled', lesson.global_scheduled,
			'source_data', lesson.source_data
		)
		FROM calendars c
		WHERE c.id = lesson.calendar_id AND c.type_id = 'ruz';
	END IF;
	RETURN NULL;
END;
$$;

CREATE OR REPLACE TRIGGER ruz_event_inserted
AFTER INSERT ON events FOR EACH ROW EXECUTE FUNCTION enqueue_ruz_event();

CREATE OR REPLACE TRIGGER ruz_event_updated
AFTER UPDATE OF name, global_scheduled, source_data, finish_before ON events FOR EACH ROW
WHEN (ROW(OLD.name, OLD.global_scheduled, OLD.source_data, OLD.finish_before)
	IS DISTINCT FROM ROW(NEW.name, NEW.global_scheduled, NEW.source_data, NEW.finish_before))
EXECUTE FUNCTION enqueue_ruz_event();

CREATE OR REPLACE TRIGGER ruz_event_deleted
AFTER DELETE ON events FOR EACH ROW EXECUTE FUNCTION enqueue_ruz_event();

COMMIT;
