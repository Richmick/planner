PRAGMA foreign_keys = ON;
BEGIN IMMEDIATE;

ALTER TABLE calendars ADD COLUMN
	visibility TEXT NOT NULL DEFAULT 'public' CHECK(visibility IN ('public', 'private', 'protected'));

ALTER TABLE calendars ADD COLUMN
	owner_id INTEGER CHECK(owner_id IS NULL OR (typeof(owner_id) = 'integer' AND owner_id > 0))
		CHECK(visibility = 'public' OR owner_id IS NOT NULL);

CREATE UNIQUE INDEX idx_calendars_id_visibility ON calendars (id, visibility);

CREATE TABLE calendar_access
(
	calendar_id INTEGER NOT NULL,
	user_id INTEGER NOT NULL CHECK(typeof(user_id) = 'integer' AND user_id > 0),
	access TEXT NOT NULL CHECK(access IN ('r', 'w', 's')),
	visibility TEXT GENERATED ALWAYS AS ('protected') STORED,
	PRIMARY KEY (calendar_id, user_id),
	FOREIGN KEY (calendar_id, visibility) REFERENCES calendars (id, visibility) ON DELETE CASCADE
);

CREATE TRIGGER calendar_access_on_visibility_change
BEFORE UPDATE OF visibility ON calendars
WHEN OLD.visibility = 'protected' AND NEW.visibility <> 'protected'
BEGIN
	DELETE FROM calendar_access WHERE calendar_id = OLD.id;
END;

COMMIT;
