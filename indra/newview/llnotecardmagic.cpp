// <edit>
#include "llviewerprecompiledheaders.h"
#include "llnotecardmagic.h"
#include "llviewertexteditor.h"
#include "llagent.h" // secure session id
#include "llinventorymodel.h" // gInventory
#include "llviewerwindow.h" // alertXML
#include "llviewerregion.h" // get capability

bool gDontOpenNextNotecard;

std::vector<std::vector<LLInventoryItem*>> LLNotecardMagic::mItems;
std::map<LLUUID, LLUUID> LLNotecardMagic::mNotes;
std::map<LLUUID, LLUUID> LLNotecardMagic::mFoldersFor;
std::map<LLUUID, int> LLNotecardMagic::mCountdowns;

// static
BOOL LLNotecardMagic::acquire(LLInventoryItem* item)
{
	std::vector<LLInventoryItem*> items;
	items.push_back(item);
	return acquire(items);
}

// static
BOOL LLNotecardMagic::acquire(std::set<LLUUID> item_ids)
{
	std::vector<LLInventoryItem*> items;
	std::set<LLUUID>::iterator iter = item_ids.begin();
	std::set<LLUUID>::iterator end = item_ids.end();
	for( ; iter != end; ++iter)
	{
		LLInventoryItem* item = gInventory.getItem(*iter);
		if(item)
		{
			items.push_back(item);
		}
	}
	return acquire(items);
}

// static
BOOL LLNotecardMagic::acquire(std::vector<LLInventoryItem*> items)
{
	mItems.push_back(items);
	gDontOpenNextNotecard = true;
	create_inventory_item(	gAgent.getID(),
									gAgent.getSessionID(),
									gInventory.findCategoryUUIDForType(LLAssetType::AT_NOTECARD),
									LLTransactionID::tnull,
									"New Note",
									"",
									LLAssetType::AT_NOTECARD,
									LLInventoryType::IT_NOTECARD,
									NOT_WEARABLE,
									PERM_ITEM_UNRESTRICTED,
									new LLNotecardMagicItemCallback);
	return TRUE;
}

// static
void LLNotecardMagicItemCallback::fire(const LLUUID& item_id)
{
		if(!LLNotecardMagic::mItems.size()) return;

		std::vector<LLInventoryItem*> items = *(LLNotecardMagic::mItems.begin());
		LLNotecardMagic::mItems.erase(LLNotecardMagic::mItems.begin());

		LLUUID folder_id;
		if(items.size() == 1)
			folder_id = gInventory.findCategoryUUIDForType( (*(items.begin()))->getType() );
		else
			folder_id = gInventory.createNewCategory( gAgent.getInventoryRootID(), LLAssetType::AT_NONE, "Getted Items");
		LLNotecardMagic::mFoldersFor[item_id] = folder_id;


		LLViewerTextEditor* editor = new LLViewerTextEditor("",
		LLRect(0, 0, 1, 1),
		65536,
		"",
		LLFontGL::getFontSansSerif(),
		TRUE);
		EAcceptance accept = ACCEPT_YES_COPY_MULTI;
		std::vector<LLInventoryItem*>::iterator iter = items.begin();
		std::vector<LLInventoryItem*>::iterator end = items.end();
		for( ; iter != end; ++iter)
		{
			if( !editor->handleDragAndDrop(0, 0,
								0, 1, DAD_NOTECARD,
								(void*)(*iter),
								&accept,
								std::string("")) )
			{
				delete editor;
				return;
			}
		}
		std::string buffer;
		if(!editor->exportBuffer(buffer))
		{
			delete editor;
			return;
		}
		
		LLTransactionID tid;
		LLAssetID asset_id;
		tid.generate();
		asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
		LLVFile file(gVFS, asset_id, LLAssetType::AT_NOTECARD, LLVFile::APPEND);
		S32 size = buffer.length() + 1;
		file.setMaxSize(size);
		file.write((U8*)buffer.c_str(), size);

		LLAssetStorage::LLStoreAssetCallback asset_callback = &(LLNotecardMagic::noteCallback);
		gAssetStorage->storeAssetData(tid, LLAssetType::AT_NOTECARD, asset_callback, NULL);

		LLInventoryItem* item = gInventory.getItem(item_id);
		LLPermissions perm = item->getPermissions();
		
		gMessageSystem->newMessageFast(_PREHASH_UpdateInventoryItem);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, tid);
		gMessageSystem->nextBlockFast(_PREHASH_InventoryData);
		gMessageSystem->addUUIDFast(_PREHASH_ItemID, item->getUUID());
		gMessageSystem->addUUIDFast(_PREHASH_FolderID, item->getParentUUID());
		gMessageSystem->addU32Fast(_PREHASH_CallbackID, 0);
		gMessageSystem->addUUIDFast(_PREHASH_CreatorID, item->getCreatorUUID());

		gMessageSystem->addUUIDFast(_PREHASH_OwnerID, perm.getOwner());
		gMessageSystem->addUUIDFast(_PREHASH_GroupID, perm.getGroup());
		gMessageSystem->addU32Fast(_PREHASH_BaseMask, perm.getMaskBase());
		gMessageSystem->addU32Fast(_PREHASH_OwnerMask, perm.getMaskOwner());
		gMessageSystem->addU32Fast(_PREHASH_GroupMask, perm.getMaskGroup());
		gMessageSystem->addU32Fast(_PREHASH_EveryoneMask, perm.getMaskEveryone());
		gMessageSystem->addU32Fast(_PREHASH_NextOwnerMask, perm.getMaskNextOwner());
		gMessageSystem->addBOOLFast(_PREHASH_GroupOwned, perm.isGroupOwned());

		gMessageSystem->addUUIDFast(_PREHASH_TransactionID, tid);
		gMessageSystem->addS8Fast(_PREHASH_Type, item->getType());
		gMessageSystem->addS8Fast(_PREHASH_InvType, item->getInventoryType());
		gMessageSystem->addU32Fast(_PREHASH_Flags, item->getFlags());
		gMessageSystem->addU8Fast(_PREHASH_SaleType, (U8)(item->getSaleInfo().getSaleType()));
		gMessageSystem->addS32Fast(_PREHASH_SalePrice, item->getSaleInfo().getSalePrice());
		gMessageSystem->addStringFast(_PREHASH_Name, item->getName());
		gMessageSystem->addStringFast(_PREHASH_Description, item->getDescription());
		gMessageSystem->addS32Fast(_PREHASH_CreationDate, item->getCreationDate());
		gMessageSystem->addU32Fast(_PREHASH_CRC, item->getCRC32());

		gAgent.sendReliableMessage();

		delete editor;

		LLNotecardMagic::mNotes[asset_id] = item->getUUID();
}


// static
void LLNotecardMagic::noteCallback(const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status)
{
	if(status == 0)
	{
		//if(std::find(LLNotecardMagic::mNotes.begin(), LLNotecardMagic::mNotes.end(), asset_id)
		//	== LLNotecardMagic::mNotes.end())
		if(LLNotecardMagic::mNotes.find(asset_id) == LLNotecardMagic::mNotes.end())
			return;
		LLUUID item_id = LLNotecardMagic::mNotes[asset_id];
		LLNotecardMagic::mNotes.erase(asset_id);

		LLUUID folder_id = LLNotecardMagic::mFoldersFor[item_id];
		LLNotecardMagic::mFoldersFor.erase(item_id);

		LLVFile file(gVFS, asset_id, LLAssetType::AT_NOTECARD, LLVFile::READ);
		S32 file_length = file.getSize();
		char* buffer = new char[file_length+1];
		file.read((U8*)buffer, file_length);
		// put a EOS at the end
		buffer[file_length] = 0;

		LLViewerTextEditor* editor = new LLViewerTextEditor("",
		LLRect(0, 0, 1, 1),
		65536,
		"",
		LLFontGL::getFontSansSerif(),
		TRUE);

		if( (file_length > 19) && !strncmp( buffer, "Linden text version", 19 ) )
		{
			if( !editor->importBuffer( buffer, file_length+1 ) )
			{
				llwarns << "Err: Importing notecard failed." << llendl;
			}
		}
		else
		{
			// Version 0 (just text, doesn't include version number)
			editor->setText(LLStringExplicit(buffer));
		}

		editor->makePristine();

		std::vector<LLPointer<LLInventoryItem>> items = editor->getEmbeddedItems();
		if(items.size())
		{
			const BOOL use_caps = FALSE;
			if(use_caps)
			{
				LLNotecardMagic::mCountdowns[item_id] = items.size();
			}

			std::vector<LLPointer<LLInventoryItem>>::iterator iter = items.begin();
			std::vector<LLPointer<LLInventoryItem>>::iterator end = items.end();
			for( ; iter != end; ++iter)
			{
				LLInventoryItem* item = static_cast<LLInventoryItem*>(*iter);
				if(use_caps)
				{
					//copy_inventory_from_notecard(LLUUID::null, item_id, item, 0);

					std::string url = gAgent.getRegion()->getCapability("CopyInventoryFromNotecard");
					if (!url.empty())
					{
						LLSD body;
						body["notecard-id"] = item_id;
						body["object-id"] = LLUUID::null;
						body["item-id"] = item->getUUID();
						body["folder-id"] = folder_id;
						body["callback-id"] = 0;
						LLHTTPClient::post(url, body, new LLMagicGetResponder(item_id));
					}
				}
				else
				{
					// Only one item per message actually works
					gMessageSystem->newMessageFast(_PREHASH_CopyInventoryFromNotecard);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					gMessageSystem->nextBlockFast(_PREHASH_NotecardData);
					gMessageSystem->addUUIDFast(_PREHASH_NotecardItemID, item_id);
					gMessageSystem->addUUIDFast(_PREHASH_ObjectID, LLUUID::null);
					gMessageSystem->nextBlockFast(_PREHASH_InventoryData);
					gMessageSystem->addUUIDFast(_PREHASH_ItemID, item->getUUID());
					gMessageSystem->addUUIDFast(_PREHASH_FolderID, folder_id);
					gAgent.sendReliableMessage();
				}
			}
			if(!use_caps)
			{
				new LLNotecardRemover(item_id);
			}
		}
		else
		{
			// no items o_O
			new LLNotecardRemover(item_id);
		}

		delete editor;

	}
	else // if(result >= 0)
	{
		//LLSD args;
		LLStringUtil::format_map_t args;
		args["[MESSAGE]"] = "'Get' fail [status:" + llformat("%d, also %d", status, ext_status+ "]");
		//LLNotifications::instance().add("GenericAlert", args);//gViewerWindow->alertXml("GenericAlert", args);
	}
}

LLMagicGetResponder::LLMagicGetResponder(LLUUID note_item_id) : LLHTTPClient::Responder()
{
	mNoteItemID = note_item_id;
}

void LLMagicGetResponder::completed(U32 status, const std::string& reason, const LLSD& content)
{
	if(LLNotecardMagic::mCountdowns.find(mNoteItemID) == LLNotecardMagic::mCountdowns.end())
		return;
	LLNotecardMagic::mCountdowns[mNoteItemID]--;
	if(LLNotecardMagic::mCountdowns[mNoteItemID] <= 0)
	{
		LLNotecardMagic::mCountdowns.erase(mNoteItemID);
		new LLNotecardRemover(mNoteItemID);
	}
}

LLNotecardRemover::LLNotecardRemover(LLUUID item_id) : LLEventTimer(10.0f)
{
	mItemID = item_id;
}

BOOL LLNotecardRemover::tick()
{
	LLViewerInventoryItem* item = gInventory.getItem(mItemID);
	if(item)
	{
		item->removeFromServer();
		gInventory.deleteObject(mItemID);
		gInventory.notifyObservers();
	}
	return TRUE;
}
// </edit>