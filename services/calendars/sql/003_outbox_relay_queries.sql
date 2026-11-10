SELECT id, aggregate_type, aggregate_id, event_type, payload, retry_count
FROM outbox_events
WHERE status = 'pending'
ORDER BY created_at ASC
LIMIT 10
FOR UPDATE SKIP LOCKED;

UPDATE outbox_events 
SET status = 'processed', processed_at = NOW()
WHERE id = $1;

UPDATE outbox_events 
SET retry_count = retry_count + 1, 
    status = CASE WHEN retry_count >= 5 THEN 'failed' ELSE 'pending' END
WHERE id = $1;

INSERT INTO outbox_events (aggregate_type, aggregate_id, event_type, payload)
SELECT 
    aggregate_type, 
    aggregate_id, 
    REPLACE(event_type, 'Created', 'Cancelled'),
    payload || '{"compensated": true}'::jsonb
FROM outbox_events
WHERE id = $1 AND status = 'failed';
