// vim:ts=8 sw=4
#ifndef __INC_DB_CACHE_H__
#define __INC_DB_CACHE_H__

#include "common/cache.h"

class CItemCache : public cache<TPlayerItem>
{
    public:
	CItemCache();
	virtual ~CItemCache();

	void Delete();
	virtual void OnFlush();
};

class CPlayerTableCache : public cache<TPlayerTable>
{
    public:
	CPlayerTableCache();
	virtual ~CPlayerTableCache();

	virtual void OnFlush();

	DWORD GetLastUpdateTime() { return static_cast<DWORD>(m_lastUpdateTime); }
};

// MYSHOP_PRICE_LIST
/**
 * @class	CItemPriceListTableCache
 * @brief	Cache for the list of item price information in private stores class
 * @version	05/06/10 Bang2ni - First release.
 */
class CItemPriceListTableCache : public cache< TItemPriceListTable >
{
    public:

	/// Constructor
	/**
	 * Set cache expiration time .
	 */
	CItemPriceListTableCache(void);

	/// List update
	/**
	 * @param [in]	pUpdateList List to update
	 *
	 * Update cached price information .
	 * When the price information list is full, previously cached information is deleted from the back. .
	 */
	void	UpdateList(const TItemPriceListTable* pUpdateList);

	/// price information DB record in .
	virtual void	OnFlush(void);

    private:

	static const int	s_nMinFlushSec;		///< Minimum cache expire time
};
// END_OF_MYSHOP_PRICE_LIST
#endif
