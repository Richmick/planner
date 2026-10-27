WITH new_event AS (
    INSERT INTO events (
        calendar_id, name, global_scheduled, finish_before, 
        overlap, visibility, interruptible
    ) VALUES (
        (SELECT calendar_id FROM calendars 
         WHERE type_id = 'users' AND external_id = 'id172'),
        'встреча по ОС',
        '[{"start":"2026-10-01T14:00:00+03","end":"2026-10-01T15:30:00+03"}]',
        '2026-10-04T00:00:00+03',
        'exclusive', 'all', false
    ) RETURNING event_id
)
INSERT INTO event_overrides (
    event_id, user_id, local_scheduled, confirmed, 
    dependencies, restrictions
) SELECT 
    ne.event_id, 22,
    '[{"start":"2026-10-01T14:00:00+03","end":"2026-10-01T15:30:00+03"}]',
    '[{"start":"2026-10-01T14:00:00+03","end":"2026-10-01T15:30:00+03"}]',
    '[{"id":3312,"type":"after"}]',
    '[{"type":"daytime","intervals":["08:00-12:00","14:00-18:00"]}]'
FROM new_event ne;

SELECT 
    e.event_id,
    e.name,
    COALESCE(o.local_scheduled, e.global_scheduled) AS effective_scheduled,
    o.confirmed,
    COALESCE(o.finished, e.finished) AS is_finished,
    COALESCE(o.success, false) AS is_success,
    COALESCE(o.dependencies, e.base_dependencies) AS effective_deps,
    COALESCE(o.restrictions, e.base_restrictions) AS effective_restrictions,
    e.overlap,
    e.interruptible,
    e.version
FROM events e
JOIN subscriptions s ON s.calendar_id = e.calendar_id
LEFT JOIN event_overrides o ON o.event_id = e.event_id AND o.user_id = s.user_id
WHERE s.user_id = 22
  AND e.finish_before > NOW()
ORDER BY (COALESCE(o.local_scheduled, e.global_scheduled)->0->>'start')::timestamptz;

INSERT INTO event_overrides (event_id, user_id, local_scheduled, updated_at)
VALUES (
    3375, 22, 
    '[{"start":"2026-10-01T15:00:00+03","end":"2026-10-01T16:30:00+03"}]', 
    NOW()
)
ON CONFLICT (event_id, user_id) 
DO UPDATE SET 
    local_scheduled = EXCLUDED.local_scheduled,
    updated_at = EXCLUDED.updated_at
WHERE event_overrides.updated_at < EXCLUDED.updated_at;

INSERT INTO subscriptions (user_id, calendar_id) VALUES (22, 1);
INSERT INTO outbox_events (aggregate_type, aggregate_id, event_type, payload)
VALUES (
    'subscription', 
    '22:1', 
    'SubscriptionCreated',
    '{"user_id": 22, "calendar_id": 1, "calendar_key": "users::id172"}'::jsonb
);

DELETE FROM events WHERE event_id = 3375;
