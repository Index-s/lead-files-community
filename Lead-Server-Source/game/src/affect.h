#ifndef __INC_AFFECT_H
#define __INC_AFFECT_H

#include "common/length.h"

class CAffect
{
	public:
		DWORD	dwType;
		BYTE    bApplyOn;
		long    lApplyValue;
		DWORD   dwFlag;
		TimeT64	lDuration;
		long	lSPCost;

		static CAffect* Acquire();
		static void Release(CAffect* p);
};

enum EAffectTypes
{
	AFFECT_NONE,

	AFFECT_MOV_SPEED		= 200,
	AFFECT_ATT_SPEED,
	AFFECT_ATT_GRADE,
	AFFECT_INVISIBILITY,
	AFFECT_STR,
	AFFECT_DEX,			// 205
	AFFECT_CON,	
	AFFECT_INT,	
	AFFECT_FISH_MIND_PILL,

	AFFECT_POISON,
	AFFECT_STUN,		// 210
	AFFECT_SLOW,
	AFFECT_DUNGEON_READY,
	AFFECT_DUNGEON_UNIQUE,

	AFFECT_BUILDING,
	AFFECT_REVIVE_INVISIBLE,	// 215
	AFFECT_FIRE,
	AFFECT_CAST_SPEED,
	AFFECT_HP_RECOVER_CONTINUE,
	AFFECT_SP_RECOVER_CONTINUE,

	AFFECT_POLYMORPH,		// 220
	AFFECT_MOUNT,

	AFFECT_WAR_FLAG,		// 222

	AFFECT_BLOCK_CHAT,		// 223
	AFFECT_CHINA_FIREWORK,

	AFFECT_BOW_DISTANCE,	// 225
	AFFECT_DEF_GRADE,		// 226

	AFFECT_PREMIUM_START	= 500,
	AFFECT_EXP_BONUS		= 500,	// ring of experience
	AFFECT_ITEM_BONUS		= 501,	// Thief's Gloves
	AFFECT_SAFEBOX		= 502,  // PREMIUM_SAFEBOX,
	AFFECT_AUTOLOOT		= 503,	// PREMIUM_AUTOLOOT,
	AFFECT_FISH_MIND		= 504,	// PREMIUM_FISH_MIND,
	AFFECT_MARRIAGE_FAST	= 505,	// mandarin duck feather
	AFFECT_GOLD_BONUS		= 506,	// Money drop probability 50% increase
	AFFECT_PREMIUM_END		= 509,

	AFFECT_MALL			= 510,	// Mall Item Effect
	AFFECT_NO_DEATH_PENALTY	= 511,	// Dragon God's Protection ( Experience prevents one penalty )
	AFFECT_SKILL_BOOK_BONUS	= 512,	// Lessons from good men ( Book training success probability 50% increase )
	AFFECT_SKILL_NO_BOOK_DELAY	= 513,	// Book of magic spells

	AFFECT_HAIR	= 514,	// hair effect
	AFFECT_COLLECT = 515, // Collection Quest 
	AFFECT_EXP_BONUS_EURO_FREE = 516, // ring of experience ( european version 14 Basic effects below level )
	AFFECT_EXP_BONUS_EURO_FREE_UNDER_15 = 517,
	AFFECT_UNIQUE_ABILITY = 518,

	AFFECT_CUBE_1,
	AFFECT_CUBE_2,
	AFFECT_CUBE_3,
	AFFECT_CUBE_4,
	AFFECT_CUBE_5,
	AFFECT_CUBE_6,
	AFFECT_CUBE_7,
	AFFECT_CUBE_8,
	AFFECT_CUBE_9,
	AFFECT_CUBE_10,
	AFFECT_CUBE_11,
	AFFECT_CUBE_12,

	AFFECT_BLEND,

	AFFECT_HORSE_NAME,
	AFFECT_MOUNT_BONUS,

	AFFECT_AUTO_HP_RECOVERY = 534,
	AFFECT_AUTO_SP_RECOVERY = 535,

	AFFECT_DRAGON_SOUL_QUALIFIED = 540,
	AFFECT_DRAGON_SOUL_DECK_0 = 541,
	AFFECT_DRAGON_SOUL_DECK_1 = 542,


	AFFECT_RAMADAN_ABILITY = 300,
	AFFECT_RAMADAN_RING	   = 301,

	AFFECT_NOG_ABILITY = 302,
	AFFECT_HOLLY_STONE_POWER = 303,

	AFFECT_QUEST_START_IDX = 1000
};

enum EAffectBits
{   
	AFF_NONE,

	AFF_YMIR,
	AFF_INVISIBILITY,
	AFF_SPAWN,

	AFF_POISON,
	AFF_SLOW,
	AFF_STUN,

	AFF_DUNGEON_READY,		// Readiness in the Dungeon
	AFF_DUNGEON_UNIQUE,		// Dungeon Unique ( Not culled on client )

	AFF_BUILDING_CONSTRUCTION_SMALL,
	AFF_BUILDING_CONSTRUCTION_LARGE,
	AFF_BUILDING_UPGRADE,

	AFF_MOV_SPEED_POTION,
	AFF_ATT_SPEED_POTION,

	AFF_FISH_MIND,

	AFF_JEONGWIHON,		// ex-ghost marriage
	AFF_GEOMGYEONG,		// speculum
	AFF_CHEONGEUN,		// Cheongeunchu
	AFF_GYEONGGONG,		// light surgery
	AFF_EUNHYUNG,		// silver criminal law
	AFF_GWIGUM,			// ghost sword
	AFF_TERROR,			// horror
	AFF_JUMAGAP,		// Jumagap
	AFF_HOSIN,			// self defense
	AFF_BOHO,			// protect
	AFF_KWAESOK,		// Rapid
	AFF_MANASHIELD,		// Mana Shield
	AFF_MUYEONG,		// Youngjin Mu affect
	AFF_REVIVE_INVISIBLE,	// Invincible for a while when resurrected
	AFF_FIRE,			// Continuous fire damage
	AFF_GICHEON,		// Grand Duke Gicheon
	AFF_JEUNGRYEOK,		// Augmentation
	AFF_TANHWAN_DASH,		// Running effect for bullet hits
	AFF_PABEOP,			// Pabeopjutsu
	AFF_CHEONGEUN_WITH_FALL,	// Cheongeunchu
	AFF_POLYMORPH,
	AFF_WAR_FLAG1,
	AFF_WAR_FLAG2,
	AFF_WAR_FLAG3,

	AFF_CHINA_FIREWORK,
	AFF_HAIR,	// hair
	AFF_GERMANY, // germany 

	AFF_BITS_MAX
};

extern void SendAffectAddPacket(LPDESC d, CAffect * pkAff);

// AFFECT_DURATION_BUG_FIX
enum AffectVariable
{
	// Affect Used when it must be infinite .
	// Because we keep decreasing time, we emulate infinity with very large values. .
	//// 24 Because there are few beats 25 use bits .
	// ... 25 You say you're using beats 29bit What great annotations are being used? ...
	// collect quest infinite time in 60 Since I am using it as a year , Here too 60 Let's make it a year .

	INFINITE_AFFECT_DURATION = 60 * 365 * 24 * 60 * 60
};
// END_AFFECT_DURATION_BUG_FIX

#endif
