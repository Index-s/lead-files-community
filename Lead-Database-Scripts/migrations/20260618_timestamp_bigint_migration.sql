-- 32-bit -> 64-bit TIMESTAMP (Year-2038) migration
-- Target: MySQL 5.6+
-- Pairs with the Lead-Shared-Source TimeT64 (int64) widening; apply in the SAME
-- maintenance window as the new game/db-server binaries and client. Widening
-- INT/INT UNSIGNED -> BIGINT is value-preserving (existing 0..2^31-1 epoch values
-- map cleanly; no CAST needed). Take a full backup first; use online-DDL tooling
-- (pt-online-schema-change / gh-ost) for the large item/quest tables if needed.

-- player database
ALTER TABLE player.affect                MODIFY COLUMN lDuration          BIGINT NOT NULL DEFAULT 0;

ALTER TABLE player.item                  MODIFY COLUMN socket0            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item                  MODIFY COLUMN socket1            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item                  MODIFY COLUMN socket2            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item                  MODIFY COLUMN socket3            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item                  MODIFY COLUMN socket4            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item                  MODIFY COLUMN socket5            BIGINT NOT NULL DEFAULT 0;

ALTER TABLE player.item_award            MODIFY COLUMN socket0            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item_award            MODIFY COLUMN socket1            BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.item_award            MODIFY COLUMN socket2            BIGINT NOT NULL DEFAULT 0;

ALTER TABLE player.marriage              MODIFY COLUMN `time`             BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.object_proto          MODIFY COLUMN upgrade_limit_time BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.object_proto_original MODIFY COLUMN upgrade_limit_time BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.player                MODIFY COLUMN horse_hp_droptime  BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted        MODIFY COLUMN horse_hp_droptime  BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.quest                 MODIFY COLUMN lValue             BIGINT NOT NULL DEFAULT 0;

-- NOTE: audit the privilege/premium expiry columns (the widened
-- TPlayerTable.aiPremiumTimes and TPacket*Priv.*_time_sec write 64-bit values).
-- Widen the matching account/player columns to BIGINT here once confirmed, e.g.:
--   ALTER TABLE player.<priv_table> MODIFY COLUMN end_time BIGINT NOT NULL DEFAULT 0;
--
-- guild_war_reservation.time is DATETIME (already 2038-safe) -- intentionally NOT changed.
