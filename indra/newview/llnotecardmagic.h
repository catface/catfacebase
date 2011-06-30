// <edit>
#ifndef LL_LLNOTECARDMAGIC_H
#define LL_LLNOTECARDMAGIC_H

#include "llinventory.h" // LLInventoryItem
#include "llviewerinventory.h" // LLInventoryCallback

extern bool gDontOpenNextNotecard;

class LLNotecardMagic
{
public:
	static BOOL acquire(std::vector<LLInventoryItem*> items); // main
	static BOOL acquire(LLInventoryItem* item);
	static BOOL acquire(std::set<LLUUID> item_ids);
	static void noteCallback(const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status);

	static std::vector<std::vector<LLInventoryItem*>> mItems;
	static std::map<LLUUID, LLUUID> mNotes;
	static std::map<LLUUID, LLUUID> mFoldersFor;
	static std::map<LLUUID, int> mCountdowns;
};

class LLNotecardMagicItemCallback : public LLInventoryCallback
{
	void fire(const LLUUID& item_id);
};

class LLMagicGetResponder : public LLHTTPClient::Responder
{
public:
	LLMagicGetResponder(LLUUID note_item_id);
	virtual void completed(U32 status, const std::string& reason, const LLSD& content);
private:
	LLUUID mNoteItemID;
};

class LLNotecardRemover : public LLEventTimer
{
public:
	LLNotecardRemover(LLUUID item_id);
	BOOL tick();
	LLUUID mItemID;
};

#endif
// </edit>