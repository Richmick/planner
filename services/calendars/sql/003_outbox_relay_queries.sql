-- Deliver each row as {"id": id, "payload": payload}.
SELECT id, payload
FROM outbox_events
ORDER BY id
LIMIT 100;

-- Remove a row only after the notifier accepts it.
DELETE FROM outbox_events WHERE id = $1;
