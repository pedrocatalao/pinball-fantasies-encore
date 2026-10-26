-- What program sent each game, as it names itself (the User-Agent header): builds made by others
-- can send games too, and this shows which ones do. Kept for the server's owner only.
ALTER TABLE runs ADD COLUMN user_agent TEXT;
