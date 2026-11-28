-- Apply once to a database created with the previous PostgreSQL schema.
BEGIN;

ALTER TABLE calendar_types RENAME COLUMN type_id TO id;
ALTER TABLE calendar_types ALTER COLUMN id TYPE VARCHAR(7);
ALTER TABLE calendar_types ALTER COLUMN description DROP DEFAULT;

ALTER TABLE calendars RENAME COLUMN calendar_id TO id;
ALTER TABLE calendars ALTER COLUMN type_id TYPE VARCHAR(7);
ALTER TABLE calendars ALTER COLUMN id DROP DEFAULT;
ALTER SEQUENCE calendars_calendar_id_seq OWNED BY NONE;
ALTER TABLE calendars ALTER COLUMN id ADD GENERATED ALWAYS AS IDENTITY;
SELECT setval(pg_get_serial_sequence('calendars', 'id'),
	GREATEST((SELECT MAX(id) FROM calendars), (SELECT last_value FROM calendars_calendar_id_seq)));
DROP SEQUENCE calendars_calendar_id_seq;

ALTER TABLE events RENAME COLUMN event_id TO id;
ALTER TABLE events ADD COLUMN IF NOT EXISTS external_id VARCHAR(64);
ALTER TABLE events ADD COLUMN IF NOT EXISTS source_data JSONB NOT NULL DEFAULT '{}'::jsonb;
ALTER TABLE events ALTER COLUMN external_id TYPE VARCHAR(64);
ALTER TABLE events ALTER COLUMN id DROP DEFAULT;
ALTER SEQUENCE events_event_id_seq OWNED BY NONE;
ALTER TABLE events ALTER COLUMN id ADD GENERATED ALWAYS AS IDENTITY;
SELECT setval(pg_get_serial_sequence('events', 'id'),
	GREATEST((SELECT MAX(id) FROM events), (SELECT last_value FROM events_event_id_seq)));
DROP SEQUENCE events_event_id_seq;
ALTER TABLE events ADD UNIQUE USING INDEX idx_events_external_id;

-- The old RUZ importer wrote both interval boundaries with a +03:00 offset.
UPDATE events e
SET global_scheduled = (
	SELECT jsonb_agg(
		CASE
			WHEN slot.value->>'start' ~ '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}[+]03:00$'
				AND slot.value->>'end' ~ '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}[+]03:00$'
				AND pg_input_is_valid(slot.value->>'start', 'timestamptz')
				AND pg_input_is_valid(slot.value->>'end', 'timestamptz')
			THEN slot.value || jsonb_build_object(
				'start', to_char((slot.value->>'start')::timestamptz AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"'),
				'end', to_char((slot.value->>'end')::timestamptz AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
			)
			ELSE slot.value
		END ORDER BY slot.ordinal
	)
	FROM jsonb_array_elements(e.global_scheduled) WITH ORDINALITY slot(value, ordinal)
)
FROM calendars c
WHERE c.id = e.calendar_id AND c.type_id = 'ruz' AND e.source_data ? 'lesson'
	AND CASE WHEN jsonb_typeof(e.global_scheduled) = 'array'
		THEN jsonb_array_length(e.global_scheduled) > 0 ELSE FALSE END;

ALTER TABLE event_overrides RENAME COLUMN override_id TO id;
ALTER TABLE event_overrides ALTER COLUMN id DROP DEFAULT;
ALTER SEQUENCE event_overrides_override_id_seq OWNED BY NONE;
ALTER TABLE event_overrides ALTER COLUMN id ADD GENERATED ALWAYS AS IDENTITY;
SELECT setval(pg_get_serial_sequence('event_overrides', 'id'),
	GREATEST((SELECT MAX(id) FROM event_overrides), (SELECT last_value FROM event_overrides_override_id_seq)));
DROP SEQUENCE event_overrides_override_id_seq;

DROP INDEX IF EXISTS idx_calendars_lookup;
DROP INDEX IF EXISTS idx_subscriptions_user;

DELETE FROM outbox_events WHERE status = 'processed';
ALTER TABLE outbox_events
	ALTER COLUMN id SET DEFAULT uuidv7(),
	DROP COLUMN aggregate_type,
	DROP COLUMN aggregate_id,
	DROP COLUMN event_type,
	DROP COLUMN status,
	DROP COLUMN retry_count,
	DROP COLUMN created_at,
	DROP COLUMN processed_at;

COMMIT;
