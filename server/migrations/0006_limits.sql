-- Small things the server keeps between requests: when it last started the checking job, so
-- that a burst of games starts it once a minute at most (the job's schedule catches the rest).
CREATE TABLE meta (
  key TEXT PRIMARY KEY,
  value INTEGER NOT NULL
);
INSERT INTO meta (key, value) VALUES ('verifier_started', 0);

-- How many players were made in the last hour, which is capped whatever tokens are sent.
CREATE INDEX players_created ON players (created_at);
