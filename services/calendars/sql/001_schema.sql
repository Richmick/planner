BEGIN;

CREATE TABLE IF NOT EXISTS calendar_types (
    type_id VARCHAR(64) PRIMARY KEY,
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS calendars (
    calendar_id BIGSERIAL PRIMARY KEY,
    type_id VARCHAR(64) NOT NULL REFERENCES calendar_types(type_id),
    external_id VARCHAR(255) NOT NULL,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (type_id, external_id)
);

CREATE TABLE IF NOT EXISTS events (
    event_id BIGSERIAL PRIMARY KEY,
    calendar_id BIGINT NOT NULL REFERENCES calendars(calendar_id) ON DELETE CASCADE,
    external_id VARCHAR(255),
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
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

ALTER TABLE events ADD COLUMN IF NOT EXISTS external_id VARCHAR(255);
ALTER TABLE events ADD COLUMN IF NOT EXISTS source_data JSONB NOT NULL DEFAULT '{}'::jsonb;

CREATE TABLE IF NOT EXISTS event_overrides (
    override_id BIGSERIAL PRIMARY KEY,
    event_id BIGINT NOT NULL REFERENCES events(event_id) ON DELETE CASCADE,
    user_id BIGINT NOT NULL,
    local_scheduled JSONB,
    confirmed JSONB,
    finished BOOLEAN,
    success BOOLEAN,
    global_transform JSONB,
    dependencies JSONB,
    restrictions JSONB,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (event_id, user_id)
);

CREATE TABLE IF NOT EXISTS subscriptions (
    user_id BIGINT NOT NULL,
    calendar_id BIGINT NOT NULL REFERENCES calendars(calendar_id) ON DELETE CASCADE,
    subscribed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    settings JSONB NOT NULL DEFAULT '{}'::jsonb,
    PRIMARY KEY (user_id, calendar_id)
);

CREATE TABLE IF NOT EXISTS outbox_events (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    aggregate_type VARCHAR(50) NOT NULL,
    aggregate_id VARCHAR(100) NOT NULL,
    event_type VARCHAR(50) NOT NULL,
    payload JSONB NOT NULL,
    status VARCHAR(20) NOT NULL DEFAULT 'pending',
    retry_count INT NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    processed_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_calendars_lookup ON calendars (type_id, external_id);
CREATE INDEX IF NOT EXISTS idx_events_calendar ON events (calendar_id, finish_before);
CREATE UNIQUE INDEX IF NOT EXISTS idx_events_external_id ON events (calendar_id, external_id);
CREATE INDEX IF NOT EXISTS idx_overrides_user ON event_overrides (user_id, updated_at);
CREATE INDEX IF NOT EXISTS idx_subscriptions_user ON subscriptions (user_id);
CREATE INDEX IF NOT EXISTS idx_outbox_pending ON outbox_events (status, created_at) WHERE status = 'pending';

INSERT INTO calendar_types (type_id, description)
VALUES ('ruz', 'Расписание учебных групп СПбПУ из ruz.spbstu.ru')
ON CONFLICT (type_id) DO UPDATE SET description = EXCLUDED.description;

COMMIT;
