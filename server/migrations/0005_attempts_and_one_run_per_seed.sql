-- How many times the verifier tried a game and could not play it at all (its recording could
-- not be fetched, or encore-play stopped): such a game no longer holds up the others, and after
-- a few tries it is rejected as one that cannot be checked.
ALTER TABLE runs ADD COLUMN attempts INTEGER NOT NULL DEFAULT 0;

-- One game per seed, kept by the database itself: two copies sent at the same moment can no
-- longer both get in between the server's look for the seed and its insert.
DROP INDEX runs_seed;
CREATE UNIQUE INDEX runs_seed ON runs (seed);
