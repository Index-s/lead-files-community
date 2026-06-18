#ifndef __PRIV_MANAGER_H
#define __PRIV_MANAGER_H
#include "common/length.h"

/**
 * @version 05/06/08	Bang2ni - Guild privilege Add related function duration
 * 			          RequestGiveGuildPriv, GiveGuildPriv Edit function prototype
 * 			          m_aPrivGuild type correction
 * 			          struct SPrivGuildData, member function GetPrivByGuildEx addition
 */
class CPrivManager : public singleton<CPrivManager>
{
	public:
		CPrivManager();

		void RequestGiveGuildPriv(DWORD guild_id, BYTE type, int value, TimeT64 dur_time_sec);
		void RequestGiveEmpirePriv(BYTE empire, BYTE type, int value, TimeT64 dur_time_sec);
		void RequestGiveCharacterPriv(DWORD pid, BYTE type, int value);

		void GiveGuildPriv(DWORD guild_id, BYTE type, int value, BYTE bLog, TimeT64 end_time_sec);
		void GiveEmpirePriv(BYTE empire, BYTE type, int value, BYTE bLog, TimeT64 end_time_sec);
		void GiveCharacterPriv(DWORD pid, BYTE type, int value, BYTE bLog);

		void RemoveGuildPriv(DWORD guild_id, BYTE type);
		void RemoveEmpirePriv(BYTE empire, BYTE type);
		void RemoveCharacterPriv(DWORD pid, BYTE type);

		int GetPriv(LPCHARACTER ch, BYTE type);
		int GetPrivByEmpire(BYTE bEmpire, BYTE type);
		int GetPrivByGuild(DWORD guild_id, BYTE type);
		int GetPrivByCharacter(DWORD pid, BYTE type);

	public:
		struct SPrivEmpireData
		{
			int m_value;
			TimeT64 m_end_time_sec;
		};

		SPrivEmpireData* GetPrivByEmpireEx(BYTE bEmpire, BYTE type);

		/// Guild Bonus Data
		struct SPrivGuildData
		{
			int		value;		///< Bonus figures
			TimeT64	end_time_sec;	///< duration
		};

		/// Obtain guild bonus data .
		/**
		 * @param [in]	dwGuildID guild to get ID
		 * @param [in]	byType bonus type
		 * @return	Pointer to the guild bonus data of the target guild , The corresponding bonus type and guild ID If there is no bonus data for NULL
		 */
		const SPrivGuildData*	GetPrivByGuildEx( DWORD dwGuildID, BYTE byType ) const;

	private:
		SPrivEmpireData m_aakPrivEmpireData[MAX_PRIV_NUM][EMPIRE_MAX_NUM];
		std::map<DWORD, SPrivGuildData> m_aPrivGuild[MAX_PRIV_NUM];
		std::map<DWORD, int> m_aPrivChar[MAX_PRIV_NUM];
};
#endif
