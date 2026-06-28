-- Yang > 2kkk / GoldType (int64) migration
-- Target: MySQL 5.6+

-- player database
ALTER TABLE player.guild MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.player MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.player_deleted MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.safebox MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;

ALTER TABLE player.item_proto MODIFY COLUMN gold BIGINT NULL DEFAULT 0;
ALTER TABLE player.item_proto MODIFY COLUMN shop_buy_price BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.mob_proto MODIFY COLUMN gold_min BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.mob_proto MODIFY COLUMN gold_max BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.myshop_pricelist MODIFY COLUMN price BIGINT NOT NULL DEFAULT 0;

-- optional but recommended for consistency with packet/table changes
ALTER TABLE player.guild_war_bet MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.guild_war_reservation MODIFY COLUMN bet_from BIGINT NOT NULL DEFAULT 0;
ALTER TABLE player.guild_war_reservation MODIFY COLUMN bet_to BIGINT NOT NULL DEFAULT 0;

-- log database
ALTER TABLE log.money_log MODIFY COLUMN gold BIGINT NOT NULL DEFAULT 0;
