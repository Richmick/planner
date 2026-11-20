WITH new_event AS
(
	INSERT INTO events (calendar_id, name, global_scheduled, finish_before, overlap, interruptible)
	SELECT id, 'встреча по ОС',
		'[{"start":"2026-10-01T11:00:00Z","end":"2026-10-01T12:30:00Z"}]',
		'2026-10-03T21:00:00Z', 'exclusive', FALSE
	FROM calendars
	WHERE type_id = 'users' AND external_id = 'id172'
	RETURNING id
)
INSERT INTO event_overrides (event_id, user_id, local_scheduled, confirmed, dependencies, restrictions)
SELECT id, 22,
	'[{"start":"2026-10-01T11:00:00Z","end":"2026-10-01T12:30:00Z"}]',
	'[{"start":"2026-10-01T11:00:00Z","end":"2026-10-01T12:30:00Z"}]',
	'[{"id":3312,"type":"after"}]',
	'[{"type":"daytime","intervals":[["08:00","12:00"],["14:00","18:00"]]}]'
FROM new_event;

SELECT e.id, e.name,
	COALESCE(o.local_scheduled, e.global_scheduled) AS effective_scheduled,
	o.confirmed,
	COALESCE(o.finished, e.finished) AS is_finished,
	COALESCE(o.success, FALSE) AS is_success,
	COALESCE(o.dependencies, e.base_dependencies) AS effective_dependencies,
	COALESCE(o.restrictions, e.base_restrictions) AS effective_restrictions,
	e.overlap, e.interruptible, e.version
FROM events e
JOIN subscriptions s ON s.calendar_id = e.calendar_id
LEFT JOIN event_overrides o ON o.event_id = e.id AND o.user_id = s.user_id
WHERE s.user_id = 22 AND e.finish_before > CURRENT_TIMESTAMP
ORDER BY (COALESCE(o.local_scheduled, e.global_scheduled)->0->>'start')::timestamptz;

INSERT INTO event_overrides (event_id, user_id, local_scheduled)
VALUES (3375, 22, '[{"start":"2026-10-01T12:00:00Z","end":"2026-10-01T13:30:00Z"}]')
ON CONFLICT (event_id, user_id) DO UPDATE SET
	local_scheduled = EXCLUDED.local_scheduled,
	updated_at = EXCLUDED.updated_at
WHERE event_overrides.updated_at < EXCLUDED.updated_at;

INSERT INTO subscriptions (user_id, calendar_id) VALUES (22, 1);

DELETE FROM events WHERE id = 3375;
