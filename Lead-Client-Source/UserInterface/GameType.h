#pragma once
#include "../GameLib/ItemData.h"
#include "common/tables.h"

struct SAffects
{
	enum
	{
		AFFECT_MAX_NUM = 32,
		PARTY_AFFECT_SLOT_MAX_NUM = 7,
	};

	SAffects() : dwAffects(0) {}
	SAffects(const DWORD & c_rAffects)
	{
		__SetAffects(c_rAffects);
	}
	int operator = (const DWORD & c_rAffects)
	{
		__SetAffects(c_rAffects);
	}

	BOOL IsAffect(BYTE byIndex)
	{
		return dwAffects & (1 << byIndex);
	}

	void __SetAffects(const DWORD & c_rAffects)
	{
		dwAffects = c_rAffects;
	}

	DWORD dwAffects;
};

extern std::string g_strGuildSymbolPathName;

const DWORD c_Name_Max_Length = 64;
const DWORD c_FileName_Max_Length = 128;
const DWORD c_Short_Name_Max_Length = 32;

const DWORD c_Equipment_Count = 12;
const DWORD c_Equipment_Start = INVENTORY_MAX_NUM;

const DWORD c_Equipment_Body	= c_Equipment_Start + 0;
const DWORD c_Equipment_Head	= c_Equipment_Start + 1;
const DWORD c_Equipment_Shoes	= c_Equipment_Start + 2;
const DWORD c_Equipment_Wrist	= c_Equipment_Start + 3;
const DWORD c_Equipment_Weapon	= c_Equipment_Start + 4;
const DWORD c_Equipment_Neck	= c_Equipment_Start + 5;
const DWORD c_Equipment_Ear		= c_Equipment_Start + 6;
const DWORD c_Equipment_Unique1	= c_Equipment_Start + 7;
const DWORD c_Equipment_Unique2	= c_Equipment_Start + 8;
const DWORD c_Equipment_Arrow	= c_Equipment_Start + 9;
const DWORD c_Equipment_Shield	= c_Equipment_Start + 10;

// Newly added rings & belts
// The space that can be assigned to the equipable item is cut off due to existing equipment, Chaegi Rap quest rewards, costume system, etc.
// This is all uproar because they implemented the Chaegi Rap compensation buff as an equipped item...
// 
// To summarize, the existing equipment windows have item positions between 90 and 102 in the server DB.
// New slots added in early 2013 start from 111 ~. The maximum value that can be used in the equipped equipment is 121, and from 122 it can be used in the Dragon Soul Stone.
#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
	const DWORD c_New_Equipment_Start = c_Equipment_Start + 21;
	const DWORD c_New_Equipment_Count = 3;
	const DWORD c_Equipment_Ring1 = c_New_Equipment_Start + 0;
	const DWORD c_Equipment_Ring2 = c_New_Equipment_Start + 1;
	const DWORD c_Equipment_Belt  = c_New_Equipment_Start + 2;;
#endif

#ifdef ENABLE_COSTUME_SYSTEM
	const DWORD c_Costume_Slot_Start	= c_Equipment_Start + 19;	// [Caution] Be careful about hardcoding the number (19). In the current server, costume slots start at 19. See the EWearPositions enumeration in the server common/length.h file.
	const DWORD	c_Costume_Slot_Body		= c_Costume_Slot_Start + 0;
	const DWORD	c_Costume_Slot_Hair		= c_Costume_Slot_Start + 1;
	const DWORD c_Costume_Slot_Count	= 2;
	const DWORD c_Costume_Slot_End		= c_Costume_Slot_Start + c_Costume_Slot_Count;
#endif

const DWORD c_Wear_Max = 32;
const DWORD c_DragonSoul_Equip_Start = INVENTORY_MAX_NUM + c_Wear_Max;
const DWORD c_DragonSoul_Equip_Slot_Max = 6;
const DWORD c_DragonSoul_Equip_End = c_DragonSoul_Equip_Start + c_DragonSoul_Equip_Slot_Max * DRAGON_SOUL_DECK_MAX_NUM;

// NOTE: As of February 5, 2013... There are two Dragon Soul Stone decks, but due to the possibility of expansion in the future, space for three decks has been allocated. The space behind it is used as belt inventory.
const DWORD c_DragonSoul_Equip_Reserved_Count = c_DragonSoul_Equip_Slot_Max * 3;		

#ifdef ENABLE_NEW_EQUIPMENT_SYSTEM
	// Inventory provided by belt items
	const DWORD c_Belt_Inventory_Slot_Start = c_DragonSoul_Equip_End + c_DragonSoul_Equip_Reserved_Count;
	const DWORD c_Belt_Inventory_Width = 4;
	const DWORD c_Belt_Inventory_Height= 4;
	const DWORD c_Belt_Inventory_Slot_Count = c_Belt_Inventory_Width * c_Belt_Inventory_Height;
	const DWORD c_Belt_Inventory_Slot_End = c_Belt_Inventory_Slot_Start + c_Belt_Inventory_Slot_Count;

	const DWORD c_Inventory_Count	= c_Belt_Inventory_Slot_End;
#else
	const DWORD c_Inventory_Count	= c_DragonSoul_Equip_End;
#endif

// Dragon Soul Stone exclusive inventory
const DWORD c_DragonSoul_Inventory_Start = 0;
const DWORD c_DragonSoul_Inventory_Box_Size = 32;
const DWORD c_DragonSoul_Inventory_Count = CItemData::DS_SLOT_NUM_TYPES * DRAGON_SOUL_GRADE_MAX * c_DragonSoul_Inventory_Box_Size;
const DWORD c_DragonSoul_Inventory_End = c_DragonSoul_Inventory_Start + c_DragonSoul_Inventory_Count;

enum ESlotType
{
	SLOT_TYPE_NONE,
	SLOT_TYPE_INVENTORY,
	SLOT_TYPE_SKILL,
	SLOT_TYPE_EMOTION,
	SLOT_TYPE_SHOP,
	SLOT_TYPE_EXCHANGE_OWNER,
	SLOT_TYPE_EXCHANGE_TARGET,
	SLOT_TYPE_QUICK_SLOT,
	SLOT_TYPE_SAFEBOX,
	SLOT_TYPE_PRIVATE_SHOP,
	SLOT_TYPE_MALL,
	SLOT_TYPE_DRAGON_SOUL_INVENTORY,
	SLOT_TYPE_SWITCHBOT,
	SLOT_TYPE_MAX,
};

const DWORD c_QuickBar_Line_Count = 3;
const DWORD c_QuickBar_Slot_Count = 12;

const float c_Idle_WaitTime = 5.0f;

const int c_Monster_Race_Start_Number = 6;
const int c_Monster_Model_Start_Number = 20001;

const float c_fAttack_Delay_Time = 0.2f;
const float c_fHit_Delay_Time = 0.1f;
const float c_fCrash_Wave_Time = 0.2f;
const float c_fCrash_Wave_Distance = 3.0f;

const float c_fHeight_Step_Distance = 50.0f;

enum
{
	DISTANCE_TYPE_FOUR_WAY,
	DISTANCE_TYPE_EIGHT_WAY,
	DISTANCE_TYPE_ONE_WAY,
	DISTANCE_TYPE_MAX_NUM,
};

const float c_fMagic_Script_Version = 1.0f;
const float c_fSkill_Script_Version = 1.0f;
const float c_fMagicSoundInformation_Version = 1.0f;
const float c_fBattleCommand_Script_Version = 1.0f;
const float c_fEmotionCommand_Script_Version = 1.0f;
const float c_fActive_Script_Version = 1.0f;
const float c_fPassive_Script_Version = 1.0f;

// Used by PushMove
const float c_fWalkDistance = 175.0f;
const float c_fRunDistance = 310.0f;

#define FILE_MAX_LEN 128

enum
{
	ITEM_ATTRIBUTE_SLOT_MAX_NUM = 7,
};

#pragma pack(push)
#pragma pack(1)

typedef struct packet_item
{
    DWORD       vnum;
    ItemStackType        count;
	DWORD		flags;
	DWORD		anti_flags;
	TimeT64		alSockets[ITEM_SOCKET_SLOT_MAX_NUM];
    TPlayerItemAttribute aAttr[ITEM_ATTRIBUTE_SLOT_MAX_NUM];
} TItemData;

#pragma pack(pop)

inline float GetSqrtDistance(int ix1, int iy1, int ix2, int iy2) // By sqrt
{
	float dx, dy;

	dx = float(ix1 - ix2);
	dy = float(iy1 - iy2);

	return sqrtf(dx*dx + dy*dy);
}

// DEFAULT_FONT
void DefaultFont_Startup();
void DefaultFont_Cleanup();
void DefaultFont_SetName(const char * c_szFontName);
CResource* DefaultFont_GetResource();
CResource* DefaultItalicFont_GetResource();
// END_OF_DEFAULT_FONT

void SetGuildSymbolPath(const char * c_szPathName);
const char * GetGuildSymbolFileName(DWORD dwGuildID);
BYTE SlotTypeToInvenType(BYTE bSlotType);
