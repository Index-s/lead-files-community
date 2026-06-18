#ifndef __INC_ITEM_MANAGER__
#define __INC_ITEM_MANAGER__

// special_item_group.txt Attribute group defined by
// type attr can be declared as .
// This attribute group can be used special_item_group.txt at Special type belonging to a group defined as UNIQUE ITEM am .
class CSpecialAttrGroup
{
public:
	CSpecialAttrGroup(DWORD vnum)
		: m_dwVnum(vnum)
	{}
	struct CSpecialAttrInfo
	{
		CSpecialAttrInfo (DWORD _apply_type, DWORD _apply_value)
			: apply_type(_apply_type), apply_value(_apply_value)
		{}
		DWORD apply_type;
		DWORD apply_value;

	};
	DWORD m_dwVnum;
	std::string	m_stEffectFileName;
	std::vector<CSpecialAttrInfo> m_vecAttrs;
};

class CSpecialItemGroup
{
public:
	enum EGiveType
	{
		NONE,
		GOLD,
		EXP,
		MOB,
		SLOW,
		DRAIN_HP,
		POISON,
		MOB_GROUP,
	};

	// QUEST The type is in the quest script. vnum.sig_use This is a group that can use .
	//		step , To join this group ITEM own TYPE this QUEST must be .
	// SPECIAL The type is idx, item_vnum, attr_vnum Enter . attr_vnum on silver CSpecialAttrGroup of Vnum am .
	//		Items in this group cannot be worn together. .
	enum ESIGType { NORMAL, PCT, QUEST, SPECIAL };

	struct CSpecialItemInfo
	{
		DWORD vnum;
		DWORD vnum_end;
		int count;
		int rare;

		CSpecialItemInfo(DWORD _vnum, DWORD _vnum_end, int _count, int _rare)
			: vnum(_vnum), vnum_end(_vnum_end), count(_count), rare(_rare)
		{
		}

		DWORD GetRandomVnum() const
		{
			if (vnum >= vnum_end || vnum_end == 0) return vnum;
			return number(vnum, vnum_end);
		}
	};

	CSpecialItemGroup(DWORD vnum, BYTE type = 0)
		: m_dwVnum(vnum), m_bType(type)
	{
	}

	void AddItem(DWORD vnum, DWORD vnum_end, int count, int prob, int rare)
	{
		if (!prob) return;
		if (m_bType != PCT && !m_vecProbs.empty())
			prob += m_vecProbs.back();

		m_vecProbs.push_back(prob);
		m_vecItems.emplace_back(vnum, vnum_end, count, rare);
	}

	bool IsEmpty() const
	{
		return m_vecProbs.empty();
	}

	int GetMultiIndex(std::vector<int>& idx_vec) const
	{
		idx_vec.clear();
		if (m_bType == PCT)
		{
			for (size_t i = 0; i < m_vecProbs.size(); ++i)
			{
				if (number(1, 100) <= m_vecProbs[i])
					idx_vec.push_back(static_cast<int>(i));
			}
			return static_cast<int>(idx_vec.size());
		}
		else
		{
			idx_vec.push_back(GetOneIndex());
			return 1;
		}
	}

	int GetOneIndex() const
	{
		if (m_vecProbs.empty()) return -1;
		int n = number(1, m_vecProbs.back());
		auto it = std::lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
		return static_cast<int>(std::distance(m_vecProbs.begin(), it));
	}

	int GetVnum(int idx) const { return m_vecItems[idx].GetRandomVnum(); }
	int GetCount(int idx) const { return m_vecItems[idx].count; }
	int GetRarePct(int idx) const { return m_vecItems[idx].rare; }

	bool Contains(DWORD dwVnum) const
	{
		for (const auto& item : m_vecItems)
		{
			if (dwVnum >= item.vnum && dwVnum <= item.vnum_end)
				return true;
		}
		return false;
	}

	DWORD GetAttrVnum(DWORD dwVnum) const
	{
		if (m_bType != SPECIAL)
			return 0;

		for (const auto& item : m_vecItems)
		{
			if (item.vnum == dwVnum)
			{
				return item.count;
			}
		}
		return 0;
	}

	int GetGroupSize() const
	{
		return static_cast<int>(m_vecProbs.size());
	}

	DWORD m_dwVnum;
	BYTE	m_bType;
	std::vector<int> m_vecProbs;
	std::vector<CSpecialItemInfo> m_vecItems; // vnum, count
};

class CMobItemGroup
{
public:
	struct SMobItemGroupInfo
	{
		DWORD dwItemVnumStart;
		DWORD dwItemVnumEnd;
		DWORD dwItemVnum = number(dwItemVnumStart, dwItemVnumEnd);
		int iCount;
		int iRarePct;

		SMobItemGroupInfo(DWORD dwItemVnumStart, DWORD dwItemVnumEnd, int iCount, int iRarePct)
			: dwItemVnumStart(dwItemVnumStart), dwItemVnumEnd(dwItemVnumEnd),
			iCount(iCount),
			iRarePct(iRarePct)
		{
		}
	};

	CMobItemGroup(DWORD dwMobVnum, int iKillDrop, const std::string& r_stName)
		:
		m_dwMobVnum(dwMobVnum),
		m_iKillDrop(iKillDrop),
		m_stName(r_stName)
	{
	}

	int GetKillPerDrop() const
	{
		return m_iKillDrop;
	}

	void AddItem(DWORD dwItemVnumStart, DWORD dwItemVnumEnd, int iCount, int iPartPct, int iRarePct)
	{
		if (!m_vecProbs.empty())
			iPartPct += m_vecProbs.back();
		m_vecProbs.push_back(iPartPct);
		m_vecItems.push_back(SMobItemGroupInfo(dwItemVnumStart, dwItemVnumEnd, iCount, iRarePct));
	}

	// MOB_DROP_ITEM_BUG_FIX
	bool IsEmpty() const
	{
		return m_vecProbs.empty();
	}

	int GetOneIndex() const
	{
		int n = number(1, m_vecProbs.back());
		itertype(m_vecProbs) it = lower_bound(m_vecProbs.begin(), m_vecProbs.end(), n);
		return static_cast<int>(std::distance(m_vecProbs.begin(), it));
	}
	// END_OF_MOB_DROP_ITEM_BUG_FIX

	const SMobItemGroupInfo& GetOne() const
	{
		return m_vecItems[GetOneIndex()];
	}

	std::vector<std::pair<int, int>> GetVector()
	{
		std::vector<std::pair<int, int>> item_list;
		for (auto& x : m_vecItems)
			item_list.emplace_back(std::make_pair(x.dwItemVnum, x.iCount));

		return item_list;
	}

	const std::vector<int>& GetProbVector() const
	{
		return m_vecProbs;
	}

	const std::vector<SMobItemGroupInfo>& GetItemVector() const
	{
		return m_vecItems;
	}

private:
	DWORD m_dwMobVnum;
	int m_iKillDrop;
	std::string m_stName;
	std::vector<int> m_vecProbs;
	std::vector<SMobItemGroupInfo> m_vecItems;
};

class CDropItemGroup
{
public:
	struct SDropItemGroupInfo
	{
		DWORD	dwVnumStart;
		DWORD	dwVnumEnd;
		DWORD	dwPct;
		int	iCount;

		SDropItemGroupInfo(DWORD dwVnumStart, DWORD dwVnumEnd, DWORD dwPct, int iCount)
			: dwVnumStart(dwVnumStart), dwVnumEnd(dwVnumEnd), dwPct(dwPct), iCount(iCount)
		{
		}
	};

public:
	CDropItemGroup(DWORD dwVnum, DWORD dwMobVnum, const std::string& r_stName)
		:
		m_dwVnum(dwVnum),
		m_dwMobVnum(dwMobVnum),
		m_stName(r_stName)
	{
	}

	const std::vector<SDropItemGroupInfo>& GetVector()
	{
		return m_vec_items;
	}

	void AddItem(DWORD dwItemVnumStart, DWORD dwItemVnumEnd, DWORD dwPct, int iCount)
	{
		m_vec_items.push_back(SDropItemGroupInfo(dwItemVnumStart, dwItemVnumEnd, dwPct, iCount));
	}

private:
	DWORD m_dwVnum;
	DWORD m_dwMobVnum;
	std::string m_stName;
	std::vector<SDropItemGroupInfo> m_vec_items;
};

class CLevelItemGroup
{
public:
	struct SLevelItemGroupInfo
	{
		DWORD dwVNumStart, dwVNumEnd;
		DWORD dwPct;
		int iCount;

		SLevelItemGroupInfo(DWORD dwVnumStart, DWORD dwVnumEnd, DWORD dwPct, int iCount)
			: dwVNumStart(dwVnumStart), dwVNumEnd(dwVnumEnd), dwPct(dwPct), iCount(iCount)
		{
		}
	};

private:
	DWORD m_dwLevelLimitStart;
	DWORD m_dwLevelLimitEnd;
	std::string m_stName;
	std::vector<SLevelItemGroupInfo> m_vec_items;

public:
	CLevelItemGroup(DWORD dwLevelLimitStart, DWORD dwLevelLimitEnd) : m_dwLevelLimitStart(dwLevelLimitStart), m_dwLevelLimitEnd(dwLevelLimitEnd) {}

	DWORD GetLevelLimitStart() { return m_dwLevelLimitStart; }
	DWORD GetLevelLimitEnd() { return m_dwLevelLimitEnd; }

	void AddItem(DWORD dwItemVnumStart, DWORD dwItemVnumEnd, DWORD dwPct, int iCount)
	{
		m_vec_items.push_back(SLevelItemGroupInfo(dwItemVnumStart, dwItemVnumEnd, dwPct, iCount));
	}

	const std::vector<SLevelItemGroupInfo>& GetVector()
	{
		return m_vec_items;
	}
};

class ITEM;

class ITEM_MANAGER : public singleton<ITEM_MANAGER>
{
	public:
		ITEM_MANAGER();
		virtual ~ITEM_MANAGER();

		bool                    Initialize(TItemTable * table, int size);
		void			Destroy();
		void			Update();	// Called every loop .
		void			GracefulShutdown();

		DWORD			GetNewID();
		bool			SetMaxItemID(TItemIDRangeTable range); // Specifies the maximum unique ID
		bool			SetMaxSpareItemID(TItemIDRangeTable range);

		// DelayedSave: If you do a lot of things that need to be saved within a routine, save it.
		// Because there are too many queries " Save it " Just mark it and wait a moment.
		// ( yes : 1 frame) Save it later .
		void			DelayedSave(LPITEM item);
		void			FlushDelayedSave(LPITEM item); // Delayed If it is on the list, delete it and save it. . Used when handling disconnection .
		void			SaveSingleItem(LPITEM item);

		LPITEM                  CreateItem(DWORD vnum, DWORD count = 1, DWORD dwID = 0, bool bTryMagic = false, int iRarePct = -1, bool bSkipSave = false);
		void DestroyItem(LPITEM item);
		void			RemoveItem(LPITEM item, const char * c_pszReason=NULL); // Remove item from user

		LPITEM			Find(DWORD id);
		LPITEM                  FindByVID(DWORD vid);
		TItemTable *            GetTable(DWORD vnum);
		bool			GetVnum(const char * c_pszName, DWORD & r_dwVnum);
		bool			GetVnumByOriginalName(const char * c_pszName, DWORD & r_dwVnum);

		bool			GetDropPct(LPCHARACTER pkChr, LPCHARACTER pkKiller, OUT int& iDeltaPercent, OUT int& iRandRange);
		bool			CreateDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item);
		bool			CreateDropItemVector(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<std::pair<int, int> >& vec_item);
		bool			IsItemMetin(const DWORD& vnum);

		std::pair<bool, std::string> ReadCommonDropItemFile(const char* c_pszFileName);
		std::pair<bool, std::string> ReadEtcDropItemFile(const char* c_pszFileName);
		std::pair<bool, std::string> ReadDropItemGroup(const char* c_pszFileName);
		std::pair<bool, std::string> ReadMonsterDropItemGroup(const char* c_pszFileName);
		std::pair<bool, std::string> ReadSpecialDropItemFile(const char* c_pszFileName);

		DWORD			GetRefineFromVnum(DWORD dwVnum);

		static void		CopyAllAttrTo(LPITEM pkOldItem, LPITEM pkNewItem);		// pkNewItem A function that returns all properties and socket values ​​to .


		const CSpecialItemGroup* GetSpecialItemGroup(DWORD dwVnum);
		const CSpecialAttrGroup* GetSpecialAttrGroup(DWORD dwVnum);

		const std::vector<TItemTable> & GetTable() { return m_vec_prototype; }

		// CHECK_UNIQUE_GROUP
		int			GetSpecialGroupFromItem(DWORD dwVnum) const { itertype(m_ItemToSpecialGroup) it = m_ItemToSpecialGroup.find(dwVnum); return (it == m_ItemToSpecialGroup.end()) ? 0 : it->second; }
		// END_OF_CHECK_UNIQUE_GROUP

	protected:
		int                     RealNumber(DWORD vnum);
		void			CreateQuestDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item, int iDeltaPercent, int iRandRange);

	public:
		const std::map<DWORD, std::vector<CMobItemGroup*> >& GetMobItemGroupMap() const { return m_map_pkMobItemGroup; }
		const std::map<DWORD, CDropItemGroup*>& GetDropItemGroupMap() const { return m_map_pkDropItemGroup; }
		const std::map<DWORD, CLevelItemGroup*>& GetLevelItemGroupMap() const { return m_map_pkLevelItemGroup; }

	protected:
		typedef std::map<DWORD, LPITEM> ITEM_VID_MAP;		

		std::vector<TItemTable>		m_vec_prototype;
		std::vector<TItemTable*> m_vec_item_vnum_range_info;
		std::map<DWORD, DWORD>		m_map_ItemRefineFrom;
		int				m_iTopOfTable;

		ITEM_VID_MAP			m_VIDMap;			///< m_dwVIDCount Store items in value units. .
		DWORD				m_dwVIDCount;			///< This guy VID It's not, it's just a process-level unique number. .
		DWORD				m_dwCurrentID;
		TItemIDRangeTable	m_ItemIDRange;
		TItemIDRangeTable	m_ItemIDSpareRange;

		TR1_NS::unordered_set<LPITEM> m_set_pkItemForDelayedSave;
		std::map<DWORD, LPITEM>		m_map_pkItemByID;
		std::map<DWORD, DWORD>		m_map_dwEtcItemDropProb;
		std::map<DWORD, CDropItemGroup*> m_map_pkDropItemGroup;
		std::map<DWORD, CSpecialItemGroup*> m_map_pkSpecialItemGroup;
		std::map<DWORD, CSpecialItemGroup*> m_map_pkQuestItemGroup;
		std::map<DWORD, CSpecialAttrGroup*> m_map_pkSpecialAttrGroup;
		std::map<DWORD, std::vector<CMobItemGroup*> > m_map_pkMobItemGroup;
		std::map<DWORD, CLevelItemGroup*> m_map_pkLevelItemGroup;

		// CHECK_UNIQUE_GROUP
		std::map<DWORD, int>		m_ItemToSpecialGroup;
		// END_OF_CHECK_UNIQUE_GROUP
	public:	
		std::map<DWORD, TItemTable>  m_map_vid;
		std::map<DWORD, TItemTable>&  GetVIDMap() { return m_map_vid; }
		std::vector<TItemTable>& GetVecProto() { return m_vec_prototype; }	

		bool GetVnumRangeByString(const std::string& stVnumRange, DWORD& r_dwVnumStart, DWORD& r_dwVnumEnd);
	private:
};

#define M2_DESTROY_ITEM(ptr) ITEM_MANAGER::instance().DestroyItem(ptr)

#endif
