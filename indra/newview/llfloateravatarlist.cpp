/** 
 * @file llfloateravatarlist.cpp
 * @brief Avatar list/radar floater
 *
 * @author Dale Glass <dale@daleglass.net>, (C) 2007
 */

/**
 * Rewritten by jcool410
 * Removed usage of globals
 * Removed TrustNET
 * Added utilization of "minimap" data
 * Heavily modified by Henri Beauchamp (the laggy spying tool becomes a true,
 * low lag radar)
 */

#include "llviewerprecompiledheaders.h"

#include "llavatarconstants.h"
#include "llavatarnamecache.h"
#include "llfloateravatarlist.h"

#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llscrolllistctrl.h"
#include "llradiogroup.h"
#include "llviewercontrol.h"

#include "llvoavatar.h"
#include "llimview.h"
#include "llfloateravatarinfo.h"
#include "llregionflags.h"
#include "llfloaterreporter.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "lltracker.h"
#include "llviewerstats.h"
#include "llerror.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llviewermessage.h"
#include "llweb.h"
#include "llviewerobjectlist.h"
#include "llmutelist.h"
#include "llcallbacklist.h"

#include <time.h>
#include <string.h>

#include <map>


#include "llworld.h"

#include "llsdutil.h"

//<edit>
#include "llviewermenu.h"
#include "llfloaterexport.h"
#include "llfloaterexploreanimations.h"
#include "llfloateravatartextures.h"
#include "llfloaterattachments.h"
#include "llfloaterteleporthistory.h"
#include "llfloaterinterceptor.h"
#include "llfloatertexturelog.h"
#include "llvolumemessage.h"
LLVOAvatar* find_avatar_from_object( LLViewerObject* object );
LLVOAvatar* find_avatar_from_object( const LLUUID& object_id );
LLUUID ANIMATION_WEAPON1 = LLUUID("0eadb1f7-1eb9-e373-d337-63c6ebaebf55");
LLUUID ANIMATION_WEAPON2 = LLUUID("5dbd4baa-8b13-9066-4e9f-7257618e702a");
LLUUID ANIMATION_WEAPON3 = LLUUID("8115b5a3-919b-4532-fbe2-88f2b9d3ecc7");
LLUUID SOUNDCRASH1 = LLUUID("0bb19787-34d9-8838-1937-a84b7496dd0b");
LLUUID SOUNDCRASH2 = LLUUID("8115b5a3-919b-4532-fbe2-88f2b9d3ecc7");
LLUUID SOUNDCRASH3 = LLUUID("0bb19787-34d9-8838-1937-a84b7496dd0b");
LLUUID SOUNDCRASH4 = LLUUID("e2b1182d-753e-a47a-b8a8-eac9e151b7bc");
LLUUID SOUNDCRASH5 = LLUUID("c0d495b7-ef21-bcda-a367-3fd06ed1f286");

std::string longfloat(F64 in)
{
 std::string out = llformat("%f", in);
 int i = out.size();
 while(out[--i] == '0') out.erase(i, 1);
 return out;
}
//</edit>

/**
 * @brief How long to keep people who are gone in the list and in memory.
 */
const F32 DEAD_KEEP_TIME = 10.0f;

extern U32 gFrameCount;

typedef enum e_radar_alert_type
{
	ALERT_TYPE_SIM = 0,
	ALERT_TYPE_DRAW = 1,
	ALERT_TYPE_SHOUTRANGE = 2,
	ALERT_TYPE_CHATRANGE = 3
} ERadarAlertType;

void chat_avatar_status(std::string name, LLUUID key, ERadarAlertType type, bool entering)
{
	if (gSavedSettings.getBOOL("RadarChatAlerts"))
	{
		LLChat chat;
		switch(type)
		{
		case ALERT_TYPE_SIM:
			if (gSavedSettings.getBOOL("RadarAlertSim"))
			{
			  chat.mText = name+" has "+(entering ? "entered" : "left")+" the sim.";
			}
		      if (gSavedSettings.getBOOL("RadarTextures"))
			{
			  LLViewerObject *avatarp = gObjectList.findObject(key);
			  if (!avatarp) return;

            for (S32 i = 0; i < avatarp->getNumTEs(); i++)
            {
            const LLTextureEntry* te = avatarp->getTE(i);
            if (!te) continue;
            Superlifetexturereader::textlogger(key,te->getID());
           }
         }
   break;
		case ALERT_TYPE_DRAW:
			if (gSavedSettings.getBOOL("RadarAlertDraw"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" draw distance.";
			}
			break;
		case ALERT_TYPE_SHOUTRANGE:
			if (gSavedSettings.getBOOL("RadarAlertShoutRange"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" shout range.";
			}
			break;
		case ALERT_TYPE_CHATRANGE:
			if (gSavedSettings.getBOOL("RadarAlertChatRange"))
			{
				chat.mText = name+" has "+(entering ? "entered" : "left")+" chat range.";
			}
			break;
		}
		if (chat.mText != "")
		{
			chat.mFromName = name;
			chat.mURL = llformat("secondlife:///app/agent/%s/about",key.asString().c_str());
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChat(chat);
		}
	}
}

LLAvatarListEntry::LLAvatarListEntry(const LLUUID& id, const std::string &name, const LLVector3d &position) :
		mID(id), mName(name), mPosition(position), mDrawPosition(), mMarked(FALSE), mFocused(FALSE),
		mUpdateTimer(), mFrame(gFrameCount), mInSimFrame(U32_MAX), mInDrawFrame(U32_MAX),
		mInChatFrame(U32_MAX), mInShoutFrame(U32_MAX)
{
}

void LLAvatarListEntry::setPosition(LLVector3d position, bool this_sim, bool drawn, bool chatrange, bool shoutrange)
{
	if (drawn)
	{
		mDrawPosition = position;
	}
	else if (mInDrawFrame == U32_MAX)
	{
		mDrawPosition.setZero();
	}

	mPosition = position;

	mFrame = gFrameCount;
	if (this_sim)
	{
		if (mInSimFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_SIM, true);
		}
		mInSimFrame = mFrame;
	}
	if (drawn)
	{
		if (mInDrawFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_DRAW, true);
		}
		mInDrawFrame = mFrame;
	}
	if (shoutrange)
	{
		if (mInShoutFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_SHOUTRANGE, true);
		}
		mInShoutFrame = mFrame;
	}
	if (chatrange)
	{
		if (mInChatFrame == U32_MAX)
		{
			chat_avatar_status(mName, mID, ALERT_TYPE_CHATRANGE, true);
		}
		mInChatFrame = mFrame;
	}

	mUpdateTimer.start();
}

bool LLAvatarListEntry::getAlive()
{
	U32 current = gFrameCount;
	if (mInSimFrame != U32_MAX && (current - mInSimFrame) >= 2)
	{
		mInSimFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_SIM, false);
	}
	if (mInDrawFrame != U32_MAX && (current - mInDrawFrame) >= 2)
	{
		mInDrawFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_DRAW, false);
	}
	if (mInShoutFrame != U32_MAX && (current - mInShoutFrame) >= 2)
	{
		mInShoutFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_SHOUTRANGE, false);
	}
	if (mInChatFrame != U32_MAX && (current - mInChatFrame) >= 2)
	{
		mInChatFrame = U32_MAX;
		chat_avatar_status(mName, mID, ALERT_TYPE_CHATRANGE, false);
	}
	return ((current - mFrame) <= 2);
}

F32 LLAvatarListEntry::getEntryAgeSeconds()
{
	return mUpdateTimer.getElapsedTimeF32();
}

BOOL LLAvatarListEntry::isDead()
{
	return getEntryAgeSeconds() > DEAD_KEEP_TIME;
}

LLFloaterAvatarList* LLFloaterAvatarList::sInstance = NULL;

LLFloaterAvatarList::LLFloaterAvatarList() :  LLFloater(std::string("radar"))
{
	llassert_always(sInstance == NULL);
	sInstance = this;
	mUpdateRate = gSavedSettings.getU32("RadarUpdateRate") * 3 + 3;
}

LLFloaterAvatarList::~LLFloaterAvatarList()
{
	gIdleCallbacks.deleteFunction(LLFloaterAvatarList::callbackIdle);
	sInstance = NULL;
}
//static
void LLFloaterAvatarList::createInstance(bool visible)
{
	sInstance = new LLFloaterAvatarList();
	LLUICtrlFactory::getInstance()->buildFloater(sInstance, "floater_radar.xml");
	if(!visible)
		sInstance->setVisible(FALSE);
}
//static
void LLFloaterAvatarList::toggle(void*)
{
#ifdef LL_RRINTERFACE_H //MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		if (sInstance && sInstance->getVisible())
		{	
			sInstance->close(false);
		}
	}
#endif //mk
	if (sInstance)
	{
		if (sInstance->getVisible())
		{
			sInstance->close(false);
		}
		else
		{
			sInstance->open();
		}
	}
	else
	{
		showInstance();
	}
}

//static
void LLFloaterAvatarList::showInstance()
{
#ifdef LL_RRINTERFACE_H //MK
	if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
	{
		return;
	}
#endif //mk
	if (sInstance)
	{
		if (!sInstance->getVisible())
		{
			sInstance->open();
		}
	}
	else
	{
		createInstance(true);
	}
}

void LLFloaterAvatarList::draw()
{
	LLFloater::draw();
}

void LLFloaterAvatarList::onOpen()
{
	gSavedSettings.setBOOL("ShowRadar", TRUE);
	sInstance->setVisible(TRUE);
}

void LLFloaterAvatarList::onClose(bool app_quitting)
{
	sInstance->setVisible(FALSE);
	if (!app_quitting)
	{
		gSavedSettings.setBOOL("ShowRadar", FALSE);
	}
	if (!gSavedSettings.getBOOL("RadarKeepOpen") || app_quitting)
	{
		destroy();
	}
}

BOOL LLFloaterAvatarList::postBuild()
{
	// Default values
	mTracking = FALSE;
	mUpdate = TRUE;

	// Hide them until some other way is found
	// Users may not expect to find a Ban feature on the Eject button
	childSetVisible("estate_ban_btn", false);

	// Set callbacks
	childSetAction("profile_btn", onClickProfile, this);
	childSetAction("im_btn", onClickIM, this);
	childSetAction("offer_btn", onClickTeleportOffer, this);
	childSetAction("track_btn", onClickTrack, this);
	childSetAction("mark_btn", onClickMark, this);
	childSetAction("focus_btn", onClickFocus, this);
	childSetAction("prev_in_list_btn", onClickPrevInList, this);
	childSetAction("next_in_list_btn", onClickNextInList, this);
	childSetAction("prev_marked_btn", onClickPrevMarked, this);
	childSetAction("next_marked_btn", onClickNextMarked, this);
	//<edit>
	childSetAction("anims_btn", onClickAnim, this);
	childSetAction("export_dude_btn", onClickExport, this);
	childSetAction("debug_btn", onClickDebug, this);
	childSetAction("crash_btn", onClickCrash, this);
	childSetAction("huds_btn", onClickHuds, this);
	childSetAction("follow_btn", onClickFollow, this);
	childSetAction("crash4_btn", onClickCrash4, this);
	childSetAction("crashno_btn", onClickCrashNo, this);
	childSetAction("ground_btn", onClickGround, this);
	childSetAction("tphist_btn", onClickTpHistory, this);
	childSetAction("inter_btn", onClickInterceptor, this);
	childSetAction("phant_btn", onClickPhantom, this);
	childSetAction("packet_btn", onClickPacket, this);
	childSetAction("spammy_btn", onClickSpammy, this);
	childSetAction("texturelog_btn", onClickTextureLog, this);
	childSetAction("rezplat_btn", onClickRezPlat, this);
	childSetAction("allshit_btn", onClickAllShit, this);
	childSetAction("position_btn", onClickPos, this);
	//<edit>
	childSetAction("get_key_btn", onClickGetKey, this);

	childSetAction("freeze_btn", onClickFreeze, this);
	childSetAction("eject_btn", onClickEject, this);
	childSetAction("mute_btn", onClickMute, this);
	childSetAction("ar_btn", onClickAR, this);
	childSetAction("teleport_btn", onClickTeleport, this);
	childSetAction("estate_eject_btn", onClickEjectFromEstate, this);

	childSetAction("send_keys_btn", onClickSendKeys, this);


	getChild<LLRadioGroup>("update_rate")->setSelectedIndex(gSavedSettings.getU32("RadarUpdateRate"));
	childSetCommitCallback("update_rate", onCommitUpdateRate, this);

	// Get a pointer to the scroll list from the interface
	mAvatarList = getChild<LLScrollListCtrl>("avatar_list");
	mAvatarList->sortByColumn("distance", TRUE);
	mAvatarList->setCommitOnSelectionChange(TRUE);
	mAvatarList->setCallbackUserData(this);
	mAvatarList->setCommitCallback(onSelectName);
	mAvatarList->setDoubleClickCallback(onClickFocus);
	refreshAvatarList();

	gIdleCallbacks.addFunction(LLFloaterAvatarList::callbackIdle);

	return TRUE;
}

void LLFloaterAvatarList::updateAvatarList()
{
	if (sInstance != this) return;

	//llinfos << "radar refresh: updating map" << llendl;

	// Check whether updates are enabled
	LLCheckboxCtrl* check = getChild<LLCheckboxCtrl>("update_enabled_cb");
	if (check && !check->getValue())
	{
		mUpdate = FALSE;
		refreshTracker();
		return;
	}
	else
	{
		mUpdate = TRUE;
	}

	LLVector3d mypos = gAgent.getPositionGlobal();

	{
		std::vector<LLUUID> avatar_ids;
		std::vector<LLUUID> sorted_avatar_ids;
		std::vector<LLVector3d> positions;

		LLWorld::instance().getAvatars(&avatar_ids, &positions, mypos, F32_MAX);

		sorted_avatar_ids = avatar_ids;
		std::sort(sorted_avatar_ids.begin(), sorted_avatar_ids.end());

		for (std::vector<LLCharacter*>::const_iterator iter = LLCharacter::sInstances.begin(); iter != LLCharacter::sInstances.end(); ++iter)
		{
			LLUUID avid = (*iter)->getID();

			if (!std::binary_search(sorted_avatar_ids.begin(), sorted_avatar_ids.end(), avid))
			{
				avatar_ids.push_back(avid);
			}
		}

		size_t i;
		size_t count = avatar_ids.size();
		
		bool announce = TRUE;//gSavedSettings.getBOOL("RadarChatKeys");
		std::queue<LLUUID> announce_keys;

		for (i = 0; i < count; ++i)
		{
			std::string name;
			std::string first;
			std::string last;
			const LLUUID &avid = avatar_ids[i];

			LLVector3d position;
			LLVOAvatar* avatarp = gObjectList.findAvatar(avid);

			if (avatarp)
			{
				// Skip if avatar is dead(what's that?)
				// or if the avatar is ourselves.
				// or if the avatar is a dummy
				if (avatarp->isDead() || avatarp->isSelf() || avatarp->mIsDummy)
				{
					continue;
				}

				// Get avatar data
				position = gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition());
				name = avatarp->getFullname();
			
				// [Ansariel: Display name support]
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(avatarp->getID(), &avatar_name))
				{
				    static const LLCachedControl<S32> phoenix_name_system("PhoenixNameSystem", 0);
					switch (phoenix_name_system)
					{
						case 0 : name = avatar_name.getLegacyName(); break;
						case 1 : name = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
						case 2 : name = avatar_name.mDisplayName; break;
						default : name = avatar_name.getLegacyName(); break;
					}

					first = avatar_name.mLegacyFirstName;
					last = avatar_name.mLegacyLastName;
				}
				else continue;
				// [/Ansariel: Display name support]

				//duped for lower section
				if (name.empty() || (name.compare(" ") == 0))// || (name.compare(gCacheName->getDefaultName()) == 0))
				{
					if (gCacheName->getName(avid, first, last))
					{
						name = first + " " + last;
					}
					else
					{
						continue;
					}
				}
#ifdef LL_RRINTERFACE_H //MK
				if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
				{
					name = gAgent.mRRInterface.getDummyName(name);
				}
#endif //mk

				if (avid.isNull())
				{
					//llinfos << "Key empty for avatar " << name << llendl;
					continue;
				}

				if (mAvatars.count(avid) > 0)
				{
					// Avatar already in list, update position
					F32 dist = (F32)(position - mypos).magVec();
					mAvatars[avid].setPosition(position, (avatarp->getRegion() == gAgent.getRegion()), true, dist < 20.0, dist < 100.0);
				}
				else
				{
					// Avatar not there yet, add it
					LLAvatarListEntry entry(avid, name, position);
					if(announce && avatarp->getRegion() == gAgent.getRegion())
						announce_keys.push(avid);
					mAvatars[avid] = entry;
				}
			}
			else
			{
				if (i < positions.size())
				{
					position = positions[i];
				}
				else
				{
					continue;
				}

				if (gCacheName->getName(avid, first, last))
				{
					name = first + " " + last;
				}
				else
				{
					//name = gCacheName->getDefaultName();
					continue; //prevent (Loading...)
				}
#ifdef LL_RRINTERFACE_H //MK
				if (gRRenabled && gAgent.mRRInterface.mContainsShownames)
				{
					name = gAgent.mRRInterface.getDummyName(name);
				}
#endif //mk

				if (mAvatars.count(avid) > 0)
				{
					// Avatar already in list, update position
					F32 dist = (F32)(position - mypos).magVec();
					mAvatars[avid].setPosition(position, gAgent.getRegion()->pointInRegionGlobal(position), false, dist < 20.0, dist < 100.0);
				}
				else
				{
					LLAvatarListEntry entry(avid, name, position);
					if(announce && gAgent.getRegion()->pointInRegionGlobal(position))
						announce_keys.push(avid);
					mAvatars[avid] = entry;
				}
			}
		}
		//let us send the keys in a more timely fashion
		if(announce && !announce_keys.empty())
                {
			std::ostringstream ids;
			int transact_num = (int)gFrameCount;
			int num_ids = 0;
			while(!announce_keys.empty())
			{
				LLUUID id = announce_keys.front();
				announce_keys.pop();

				ids << "," << id.asString();
				++num_ids;

				if(ids.tellp() > 200)
				{
				        gMessageSystem->newMessage("ScriptDialogReply");
				        gMessageSystem->nextBlock("AgentData");
				        gMessageSystem->addUUID("AgentID", gAgent.getID());
				        gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
				        gMessageSystem->nextBlock("Data");
				        gMessageSystem->addUUID("ObjectID", gAgent.getID());
				        gMessageSystem->addS32("ChatChannel", -777777777);
				        gMessageSystem->addS32("ButtonIndex", 1);
				        gMessageSystem->addString("ButtonLabel",llformat("%d,%d", transact_num, num_ids) + ids.str());
				        gAgent.sendReliableMessage();

				        num_ids = 0;
				        ids.seekp(0);
				        ids.str("");
				}
			}
			if(num_ids > 0)
			{
				gMessageSystem->newMessage("ScriptDialogReply");
				gMessageSystem->nextBlock("AgentData");
				gMessageSystem->addUUID("AgentID", gAgent.getID());
				gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
				gMessageSystem->nextBlock("Data");
				gMessageSystem->addUUID("ObjectID", gAgent.getID());
				gMessageSystem->addS32("ChatChannel", -777777777);
				gMessageSystem->addS32("ButtonIndex", 1);
				gMessageSystem->addString("ButtonLabel",llformat("%d,%d", transact_num, num_ids) + ids.str());
				gAgent.sendReliableMessage();
			}
                }
	}
	
//	llinfos << "radar refresh: done" << llendl;

	expireAvatarList();
	refreshAvatarList();
	refreshTracker();
}

void LLFloaterAvatarList::expireAvatarList()
{
//	llinfos << "radar: expiring" << llendl;
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	std::queue<LLUUID> delete_queue;
	bool alive;

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLAvatarListEntry *entry = &iter->second;
		
		alive = entry->getAlive();

		if (entry->isDead())
		{
			//llinfos << "radar: expiring avatar " << entry->getName() << llendl;
			LLUUID av_id = entry->getID();
			delete_queue.push(av_id);
		}
	}

	while (!delete_queue.empty())
	{
		mAvatars.erase(delete_queue.front());
		delete_queue.pop();
	}
}

/**
 * Redraws the avatar list
 * Only does anything if the avatar list is visible.
 * @author Dale Glass
 */
void LLFloaterAvatarList::refreshAvatarList() 
{
	// Don't update list when interface is hidden
	if (!sInstance->getVisible()) return;

	// We rebuild the list fully each time it's refreshed
	// The assumption is that it's faster to refill it and sort than
	// to rebuild the whole list.
	LLDynamicArray<LLUUID> selected = mAvatarList->getSelectedIDs();
	S32 scrollpos = mAvatarList->getScrollPos();

	mAvatarList->deleteAllItems();

	LLVector3d mypos = gAgent.getPositionGlobal();
	LLVector3d posagent;
	posagent.setVec(gAgent.getPositionAgent());
	LLVector3d simpos = mypos - posagent;

	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLSD element;
		LLUUID av_id;
		std::string av_name;

		LLAvatarListEntry *entry = &iter->second;

		// Skip if avatar hasn't been around
		if (entry->isDead())
		{
			continue;
		}

		av_id = entry->getID();
		av_name = entry->getName().c_str();

		LLVector3d position = entry->getPosition();
		BOOL UnknownAltitude = false;

		LLVector3d delta = position - mypos;
		F32 distance = (F32)delta.magVec();
		if (position.mdV[VZ] == 0.0)
		{
			UnknownAltitude = true;
			distance = 9000.0;
		}
		delta.mdV[2] = 0.0f;
		F32 side_distance = (F32)delta.magVec();

		// HACK: Workaround for an apparent bug:
		// sometimes avatar entries get stuck, and are registered
		// by the client as perpetually moving in the same direction.
		// this makes sure they get removed from the visible list eventually

		//jcool410 -- this fucks up seeing dueds thru minimap data > 1024m away, so, lets just say > 2048m to the side is bad
		//aka 8 sims
		if (side_distance > 2048.0f)
		{
			continue;
		}

		if (av_id.isNull())
		{
			//llwarns << "Avatar with null key somehow got into the list!" << llendl;
			continue;
		}

		element["id"] = av_id;

		element["columns"][LIST_MARK]["column"] = "marked";
		element["columns"][LIST_MARK]["type"] = "text";
		if (entry->isMarked())
		{
			element["columns"][LIST_MARK]["value"] = "X";
			element["columns"][LIST_MARK]["color"] = LLColor4::blue.getValue();
			element["columns"][LIST_MARK]["font-style"] = "BOLD";
		}
		else
		{
			element["columns"][LIST_MARK]["value"] = "";
		}

		element["columns"][LIST_AVATAR_NAME]["column"] = "avatar_name";
		element["columns"][LIST_AVATAR_NAME]["type"] = "text";
		element["columns"][LIST_AVATAR_NAME]["value"] = av_name;
		if (entry->isFocused())
		{
			element["columns"][LIST_AVATAR_NAME]["font-style"] = "BOLD";
		}

		//<edit> custom colors for certain types of avatars!
		//Changed a bit so people can modify them in settings. And since they're colors, again it's possibly account-based. Starting to think I need a function just to determine that. - HgB
		//element["columns"][LIST_AVATAR_NAME]["color"] = gColors.getColor( "MapAvatar" ).getValue();
		LLViewerRegion* parent_estate = LLWorld::getInstance()->getRegionFromPosGlobal(entry->getPosition());
		LLUUID estate_owner = LLUUID::null;
		if(parent_estate && parent_estate->isAlive())
		{
			estate_owner = parent_estate->getOwner();
		}

		//Lindens are always more Linden than your friend, make that take precedence
		if(LLMuteList::getInstance()->isLinden(av_name))
		{
			static const LLCachedControl<LLColor4> ascent_linden_color("AscentLindenColor",LLColor4(0.f,0.f,1.f,1.f));
			element["columns"][LIST_AVATAR_NAME]["color"] = ascent_linden_color.get().getValue();
		}
		//check if they are an estate owner at their current position
		else if(estate_owner.notNull() && av_id == estate_owner)
		{
			static const LLCachedControl<LLColor4> ascent_estate_owner_color("AscentEstateOwnerColor",LLColor4(1.f,0.6f,1.f,1.f));
			element["columns"][LIST_AVATAR_NAME]["color"] = ascent_estate_owner_color.get().getValue();
		}
		//without these dots, SL would suck.
		else if(is_agent_friend(av_id))
		{
			static const LLCachedControl<LLColor4> ascent_friend_color("AscentFriendColor",LLColor4(1.f,1.f,0.f,1.f));
			element["columns"][LIST_AVATAR_NAME]["color"] = ascent_friend_color.get().getValue();
		}
		//big fat jerkface who is probably a jerk, display them as such.
		else if(LLMuteList::getInstance()->isMuted(av_id))
		{
			static const LLCachedControl<LLColor4> ascent_muted_color("AscentMutedColor",LLColor4(0.7f,0.7f,0.7f,1.f));
			element["columns"][LIST_AVATAR_NAME]["color"] = ascent_muted_color.get().getValue();
		}
		

		char temp[32];
		LLColor4 color = LLColor4::black;
		element["columns"][LIST_DISTANCE]["column"] = "distance";
		element["columns"][LIST_DISTANCE]["type"] = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
			if (entry->isDrawn())
			{
				color = LLColor4::green2;
			}
		}
		else
		{
			if (distance < 100.0)
			{
				snprintf(temp, sizeof(temp), "%.1f", distance);
				if (distance > 20.0f)
				{
					color = LLColor4::yellow1;
				}
				else
				{
					color = LLColor4::red;
				}
			}
			else
			{
				if (entry->isDrawn())
				{
					color = LLColor4::green2;
				}
				snprintf(temp, sizeof(temp), "%d", (S32)distance);
			}
		}
		element["columns"][LIST_DISTANCE]["value"] = temp;
		element["columns"][LIST_DISTANCE]["color"] = color.getValue();

		position = position - simpos;

		S32 x = (S32)position.mdV[VX];
		S32 y = (S32)position.mdV[VY];
		if (x >= 0 && x <= 256 && y >= 0 && y <= 256)
		{
			snprintf(temp, sizeof(temp), "%d, %d", x, y);
		}
		else
		{
			temp[0] = '\0';
			if (y < 0)
			{
				strcat(temp, "S");
			}
			else if (y > 256)
			{
				strcat(temp, "N");
			}
			if (x < 0)
			{
				strcat(temp, "W");
			}
			else if (x > 256)
			{
				strcat(temp, "E");
			}
		}
		element["columns"][LIST_POSITION]["column"] = "position";
		element["columns"][LIST_POSITION]["type"] = "text";
		element["columns"][LIST_POSITION]["value"] = temp;

		element["columns"][LIST_ALTITUDE]["column"] = "altitude";
		element["columns"][LIST_ALTITUDE]["type"] = "text";
		if (UnknownAltitude)
		{
			strcpy(temp, "?");
		}
		else
		{
			snprintf(temp, sizeof(temp), "%d", (S32)position.mdV[VZ]);
		}
		element["columns"][LIST_ALTITUDE]["value"] = temp;
		
		element["columns"][LIST_CLIENT]["column"] = "client";
		element["columns"][LIST_CLIENT]["type"] = "text";

		element["columns"][LIST_METADATA]["column"] = "metadata";
		element["columns"][LIST_METADATA]["type"] = "text";

		static const LLCachedControl<LLColor4> avatar_name_color("AvatarNameColor",LLColor4(LLColor4U(251, 175, 93, 255)), gColors );
		static const LLCachedControl<LLColor4> unselected_color("ScrollUnselectedColor",LLColor4(LLColor4U(0, 0, 0, 204)), gColors );
		LLColor4 name_color(avatar_name_color);
		std::string client;
		LLVOAvatar *avatarp = gObjectList.findAvatar(av_id);
		if(avatarp)
		{
			avatarp->getClientInfo(client, name_color, TRUE);
			if(client == "")
			{
				name_color = unselected_color;
				client = "?";
			}
			element["columns"][LIST_CLIENT]["value"] = client.c_str();

			// <dogmode>
			// Don't expose Emerald's metadata.

			if(avatarp->extraMetadata.length())
			{
				element["columns"][LIST_METADATA]["value"] = avatarp->extraMetadata.c_str();
			}
		}
		else
		{
			element["columns"][LIST_CLIENT]["value"] = "Out Of Range";
		}
		//Blend to make the color show up better
		name_color = name_color *.5f + unselected_color * .5f;

		element["columns"][LIST_CLIENT]["color"] = avatar_name_color.get().getValue();

		// Add to list
		mAvatarList->addElement(element, ADD_BOTTOM);
	}

	// finish
	mAvatarList->sortItems();
	mAvatarList->selectMultiple(selected);
	mAvatarList->setScrollPos(scrollpos);

//	llinfos << "radar refresh: done" << llendl;

}

// static
void LLFloaterAvatarList::onClickIM(void* userdata)
{
	//llinfos << "LLFloaterFriends::onClickIM()" << llendl;
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		if (ids.size() == 1)
		{
			// Single avatar
			LLUUID agent_id = ids[0];

			char buffer[MAX_STRING];
			// [Ansariel: Display name support]
			// snprintf(buffer, MAX_STRING, "%s", avlist->mAvatars[agent_id].getName().c_str());
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(agent_id, &avatar_name))
			{
				snprintf(buffer, MAX_STRING, "%s", avatar_name.getLegacyName().c_str());
				gIMMgr->setFloaterOpen(TRUE);
				gIMMgr->addSession(buffer,IM_NOTHING_SPECIAL,agent_id);
			}
			// [Ansariel: Display name support]
		}
		else
		{
			// Group IM
			LLUUID session_id;
			session_id.generate();
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession("Avatars Conference", IM_SESSION_CONFERENCE_START, ids[0], ids);
		}
	}
}

void LLFloaterAvatarList::onClickTeleportOffer(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		handle_lure(ids);
	}
}

void LLFloaterAvatarList::onClickTrack(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	
 	LLScrollListItem *item =   self->mAvatarList->getFirstSelected();
	if (!item) return;

	LLUUID agent_id = item->getUUID();

	if (self->mTracking && self->mTrackedAvatar == agent_id) {
		LLTracker::stopTracking(NULL);
		self->mTracking = FALSE;
	}
	else
	{
		self->mTracking = TRUE;
		self->mTrackedAvatar = agent_id;
//		trackAvatar only works for friends allowing you to see them on map...
//		LLTracker::trackAvatar(agent_id, self->mAvatars[agent_id].getName());
		std::string name = self->mAvatars[agent_id].getName();
		if (!self->mUpdate)
		{
			name += "\n(last known position)";
		}
		LLTracker::trackLocation(self->mAvatars[agent_id].getPosition(), name, name);
	}
}

void LLFloaterAvatarList::refreshTracker()
{
	if (!mTracking) return;

	if (LLTracker::isTracking(NULL))
	{
		LLVector3d pos = mAvatars[mTrackedAvatar].getPosition();
		if (pos != LLTracker::getTrackedPositionGlobal())
		{
			std::string name = mAvatars[mTrackedAvatar].getName();
			if (!mUpdate)
			{
				name += "\n(last known position)";
			}
			LLTracker::trackLocation(pos, name, name);
		}
	}
	else
	{	// Tracker stopped.
		LLTracker::stopTracking(NULL);
		mTracking = FALSE;
//		llinfos << "Tracking stopped." << llendl;
	}
}

LLAvatarListEntry * LLFloaterAvatarList::getAvatarEntry(LLUUID avatar)
{
	if (avatar.isNull())
	{
		return NULL;
	}

	std::map<LLUUID, LLAvatarListEntry>::iterator iter;

	iter = mAvatars.find(avatar);
	if (iter == mAvatars.end())
	{
		return NULL;
	}

	return &iter->second;	
}

//static
void LLFloaterAvatarList::onClickMark(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();

	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = self->getAvatarEntry(avid);
		if (entry != NULL)
		{
			entry->toggleMark();
		}
	}
}

void LLFloaterAvatarList::onClickFocus(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		self->mFocusedAvatar = item->getUUID();
		self->focusOnCurrent();
	}
}

void LLFloaterAvatarList::removeFocusFromAll()
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		LLAvatarListEntry *entry = &iter->second;
		entry->setFocus(FALSE);
	}
}

void LLFloaterAvatarList::focusOnCurrent()
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (entry->getID() == mFocusedAvatar)
		{
			removeFocusFromAll();
			entry->setFocus(TRUE);
			gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
			return;
		}
	}
}

void LLFloaterAvatarList::focusOnPrev(BOOL marked_only)
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	LLAvatarListEntry *prev = NULL;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (prev != NULL && entry->getID() == mFocusedAvatar)
		{
			break;
		}

		if ((!marked_only && entry->isDrawn()) || entry->isMarked())
		{
			prev = entry;
		}
	}

	if (prev != NULL)
	{
		removeFocusFromAll();
		prev->setFocus(TRUE);
		mFocusedAvatar = prev->getID();
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

void LLFloaterAvatarList::focusOnNext(BOOL marked_only)
{
	std::map<LLUUID, LLAvatarListEntry>::iterator iter;
	BOOL found = FALSE;
	LLAvatarListEntry *next = NULL;
	LLAvatarListEntry *entry;

	if (mAvatars.size() == 0)
	{
		return;
	}

	for (iter = mAvatars.begin(); iter != mAvatars.end(); iter++)
	{
		entry = &iter->second;

		if (entry->isDead())
			continue;

		if (next == NULL && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
		}

		if (found && ((!marked_only && entry->isDrawn()) || entry->isMarked()))
		{
			next = entry;
			break;
		}

		if (entry->getID() == mFocusedAvatar)
		{
			found = TRUE;
		} 
	}

	if (next != NULL)
	{
		removeFocusFromAll();
		next->setFocus(TRUE);
		mFocusedAvatar = next->getID();
		gAgent.lookAtObject(mFocusedAvatar, CAMERA_POSITION_OBJECT);
	}
}

/*static*/
void LLFloaterAvatarList::lookAtAvatar(LLUUID &uuid)
{ // twisted laws
	LLViewerObject* voavatar = gObjectList.findObject(uuid);
	if(voavatar && voavatar->isAvatar())
	{
		gAgent.setFocusOnAvatar(FALSE, FALSE);
		gAgent.changeCameraToThirdPerson();
		gAgent.setFocusGlobal(voavatar->getPositionGlobal(),uuid);
		gAgent.setCameraPosAndFocusGlobal(voavatar->getPositionGlobal()
				+ LLVector3d(3.5,1.35,0.75) * voavatar->getRotation(),
												voavatar->getPositionGlobal(),
												uuid );
	}
}

//static
void LLFloaterAvatarList::onClickPrevInList(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnPrev(FALSE);
}

//static
void LLFloaterAvatarList::onClickNextInList(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnNext(FALSE);
}

//static
void LLFloaterAvatarList::onClickPrevMarked(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnPrev(TRUE);
}

//static
void LLFloaterAvatarList::onClickNextMarked(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->focusOnNext(TRUE);
}

//static
void LLFloaterAvatarList::onClickGetKey(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();

	if (NULL == item) return;

	LLUUID agent_id = item->getUUID();
	LLChat chat;
	chat.mText = "Key: "+agent_id.asString();
	char buffer[UUID_STR_LENGTH];		/*Flawfinder: ignore*/
	agent_id.toString(buffer);
	gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(buffer));
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
    LLFloaterChat::addChat(chat);
}

//<edit> START SIMMS VOIDS
void LLFloaterAvatarList::onClickAnim(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *simms = self->mAvatarList->getFirstSelected();

	 if (NULL == simms) return;

 LLUUID agent_id = simms->getUUID();
  if(simms)
  {
   new LLFloaterExploreAnimations(agent_id); //temporary
  }
  return;
 }

void LLFloaterAvatarList::onClickExport(void *userdata)
{
 LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
  LLScrollListItem *item = self->mAvatarList->getFirstSelected();
       if(!item) return;
       LLViewerObject *obj=gObjectList.findObject(item->getUUID());
       if(obj)
        {
   LLSelectMgr::getInstance()->selectObjectOnly(obj);
   LLFloaterExport* floater = new LLFloaterExport();
   floater->center();
   LLSelectMgr::getInstance()->deselectAll();
        }
       return;
}

void LLFloaterAvatarList::onClickDebug(void *userdata)
{
 LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *simms = self->mAvatarList->getFirstSelected();

	 if (NULL == simms) return;

 LLUUID agent_id = simms->getUUID();
  if(simms)
  {
   LLFloaterAvatarTextures::show( simms->getUUID() );
  }
 return ;
}


void LLFloaterAvatarList::onClickCrash(void *userdata)
{
	LLChat chat;
	chat.mText = "Executing Impostor Sound Crasher Please Wait...";
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	{
			send_sound_trigger(SOUNDCRASH1,1.0);
			send_sound_trigger(SOUNDCRASH2,1.0);
			send_sound_trigger(SOUNDCRASH3,1.0);
			send_sound_trigger(SOUNDCRASH4,1.0);
			send_sound_trigger(SOUNDCRASH5,1.0);
			send_sound_trigger(SOUNDCRASH1,1.0);
			send_sound_trigger(SOUNDCRASH2,1.0);
			send_sound_trigger(SOUNDCRASH3,1.0);
			send_sound_trigger(SOUNDCRASH4,1.0);
			send_sound_trigger(SOUNDCRASH5,1.0);
			send_sound_trigger(SOUNDCRASH1,1.0);
			send_sound_trigger(SOUNDCRASH2,1.0);
			send_sound_trigger(SOUNDCRASH3,1.0);
			send_sound_trigger(SOUNDCRASH4,1.0);
			send_sound_trigger(SOUNDCRASH5,1.0);
			send_sound_trigger(SOUNDCRASH1,1.0);
			send_sound_trigger(SOUNDCRASH2,1.0);
			send_sound_trigger(SOUNDCRASH3,1.0);
			send_sound_trigger(SOUNDCRASH4,1.0);
			send_sound_trigger(SOUNDCRASH5,1.0);
			//DEAD_KEEP_TIME;
		}
		LLFloaterChat::addChat(chat);
 return ;
}


void LLFloaterAvatarList::onClickHuds(void *userdata)
{
 LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
  LLScrollListItem *item = self->mAvatarList->getFirstSelected();
       if(!item) return;

       LLViewerObject *obj=gObjectList.findObject(item->getUUID());
       if(obj)
        {
   LLSelectMgr::getInstance()->selectObjectOnly(obj);
   LLFloaterAttachments* floater = new LLFloaterAttachments();
  floater->center();
  LLSelectMgr::getInstance()->deselectAll();
        }
       return;
}
       

void LLFloaterAvatarList::onClickFollow(void *userdata)
	{
	LLChat chat;
	chat.mText = "Executing Impostor Animation Crasher Please Wait...";
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	gAgent.sendAnimationRequest(ANIMATION_WEAPON1, ANIM_REQUEST_START);
	gAgent.sendAnimationRequest(ANIMATION_WEAPON2, ANIM_REQUEST_START);
	gAgent.sendAnimationRequest(ANIMATION_WEAPON3, ANIM_REQUEST_START);
	LLFloaterChat::addChat(chat);
	//DEAD_KEEP_TIME;
 return;
}


void LLFloaterAvatarList::onClickCrash4(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
				if(agent_id.notNull())
				{
		F32 fps = LLViewerStats::getInstance()->mFPSStat.getMeanPerSec();
					F32 x = 0.0f;
					F32 tosend = 9999.0f / fps;
					LLMessageSystem* msg = gMessageSystem;
					LLUUID transaction_id;
					while(x < tosend)
					{
						msg->newMessage("OfferCallingCard");
						//msg->newMessage("GodlikeMessage");
						//msg->newMessage("FreezeUser");
						//msg->newMessage("EjectUser");
						//handle_lure(transaction_id);"fuck you bitch";
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_AgentBlock);
						msg->addUUIDFast(_PREHASH_DestID, agent_id);
						transaction_id.generate();
						msg->addUUIDFast(_PREHASH_TransactionID, transaction_id);
						gAgent.sendMessage();
						x = x + 1.0f;
					}
				}
			}
	return ;
}


void LLFloaterAvatarList::onClickCrashNo(void *userdata)
{
	LLChat chat;
	chat.mText = "Killing The Impostor Animation Crashloop";
	gAgent.sendAnimationRequest(ANIMATION_WEAPON1, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIMATION_WEAPON2, ANIM_REQUEST_STOP);
	gAgent.sendAnimationRequest(ANIMATION_WEAPON3, ANIM_REQUEST_STOP);
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
    LLFloaterChat::addChat(chat);
 return ;
}


void LLFloaterAvatarList::onClickGround(void *userdata)
	{
	float height = 20.0;
			LLVector3 pos = gAgent.getPositionAgent();
			pos.mV[VZ] = height;
			gAgent.teleportRequest(gAgent.getRegion()->getHandle(), pos);
	   			LLChat chat;
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			chat.mText = llformat("Impostor Ground Level");
			LLFloaterChat::addChat(chat);	
}

void LLFloaterAvatarList::onClickTpHistory(void *userdata)
	{
			gFloaterTeleportHistory->setVisible(!gFloaterTeleportHistory->getVisible());
		}

void LLFloaterAvatarList::onClickInterceptor(void *userdata)
	{
			LLFloaterInterceptor::show();
		}

void LLFloaterAvatarList::onClickPhantom(void *userdata)
	{
LLAgent::togglePhantom();
	LLAgent::getPhantom();
	LLChat chat;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	chat.mText = llformat("%s%s","Phantom ",(LLAgent::getPhantom()? "On" : "Off"));
	LLFloaterChat::addChat(chat);
}

void LLFloaterAvatarList::onClickSpammy(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();

	if (NULL == item) return;

	LLUUID agent_id = item->getUUID();
	F32 fps = LLViewerStats::getInstance()->mFPSStat.getMeanPerSec();
			F32 x = 0.0f;
			F32 tosend = 9999.0f / fps;
			LLUUID transaction_id;
			while(x < tosend)
			{
			std::string my_name;
			gAgent.buildFullname(my_name);
			LLUUID rand;
			rand.generate();
			send_improved_im(agent_id,
				my_name.c_str(),
				"|",
				IM_OFFLINE,
				IM_BUSY_AUTO_RESPONSE,
				rand,
				NO_TIMESTAMP,
				(U8*)EMPTY_BINARY_BUCKET,
				EMPTY_BINARY_BUCKET_SIZE);
				x = x + 1.0f;
			}
	 }
		
void LLFloaterAvatarList::onClickPacket(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
	LLUUID agent_id = item->getUUID();
				if(agent_id.notNull())
				{
				F32 fps = LLViewerStats::getInstance()->mFPSStat.getMeanPerSec();
					F32 x = 0.0f;
					F32 tosend = 9999.0f / fps;
					//LLMessageSystem* msg = gMessageSystem;
					LLUUID transaction_id;
					while(x < tosend)
						{
//						char buffer[DB_IM_MSG_BUF_SIZE * 2];  /* Flawfinder: ignore */
                        LLDynamicArray<LLUUID> ids;
                        ids.push_back(agent_id);
                        std::string tpMsg="Join me!";
                        LLMessageSystem* msg = gMessageSystem;
                        msg->newMessageFast(_PREHASH_StartLure);
                        msg->nextBlockFast(_PREHASH_AgentData);
                        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
                        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
                        msg->nextBlockFast(_PREHASH_Info);
                        msg->addU8Fast(_PREHASH_LureType, (U8)0); 

                        msg->addStringFast(_PREHASH_Message, tpMsg);
                        for(LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
                        {
                            msg->nextBlockFast(_PREHASH_TargetData);
                            msg->addUUIDFast(_PREHASH_TargetID, *itr);
                        }
                        gAgent.sendReliableMessage();
						x = x + 1.0f;
			}
				}
			}
	return ;
}

void LLFloaterAvatarList::onClickTextureLog(void *userdata)
    {
	new Superlifetexturereader();
	LLFloaterAvatarList *refresh();
	}

void LLFloaterAvatarList::onClickRezPlat(void *userdata)
	{
    LLVector3 agentPos = gAgent.getPositionAgent()+(gAgent.getVelocity()*(F32)0.333);
    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_ObjectAdd);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
    msg->nextBlockFast(_PREHASH_ObjectData);
    msg->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);
    msg->addU8Fast(_PREHASH_Material,    LL_MCODE_METAL);

    if(agentPos.mV[2] > 4096.0)msg->addU32Fast(_PREHASH_AddFlags, FLAGS_CREATE_SELECTED);
    else msg->addU32Fast(_PREHASH_AddFlags, 0);

    LLVolumeParams    volume_params;

    volume_params.setType( LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_CIRCLE_33 );
    volume_params.setRatio    ( 2, 2 );
    volume_params.setShear    ( 0, 0 );
    volume_params.setTaper(2.0f,2.0f);
    volume_params.setTaperX(0.f);
    volume_params.setTaperY(0.f);

    LLVolumeMessage::packVolumeParams(&volume_params, msg);
    LLVector3 rezpos = agentPos - LLVector3(0.0f,0.0f,2.5f);
    LLQuaternion rotation;
    rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);
	//gSavedSettings.getF32("AscentPlatformSize");
	F32 realsize = gSavedSettings.getF32("AscentPlatformSize");
	if (realsize < 0.01f) realsize = 0.01f;
	else if (realsize > 10.0f) realsize = 10.0f;

    msg->addVector3Fast(_PREHASH_Scale,            LLVector3(0.01f,realsize,realsize) );
    msg->addQuatFast(_PREHASH_Rotation,            rotation );
    msg->addVector3Fast(_PREHASH_RayStart,        rezpos );
    msg->addVector3Fast(_PREHASH_RayEnd,            rezpos );
    msg->addU8Fast(_PREHASH_BypassRaycast,        (U8)1 );
    msg->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)FALSE );
    msg->addU8Fast(_PREHASH_State, 0);
    msg->addUUIDFast(_PREHASH_RayTargetID,            LLUUID::null );
    msg->sendReliable(gAgent.getRegionHost());
}

void LLFloaterAvatarList::onClickAllShit(void *userdata)
	{
	LLChat chat;
	chat.mText = "executing all Impostor crashers/spammers";
	onClickFollow(userdata);
	onClickCrash4(userdata);
	onClickPacket(userdata);
	onClickSpammy(userdata);
	onClickCrash(userdata);
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
    LLFloaterChat::addChat(chat);
	}

void LLFloaterAvatarList::onClickPos(void *userdata)
{

 LLFloaterAvatarList* avlist = getInstance();
 LLScrollListItem *item = avlist->mAvatarList->getFirstSelected();

 if ( item )
 {
  LLUUID agent_id = item->getUUID();
  LLAvatarListEntry *ent = avlist->getAvatarEntry(agent_id);
  if ( ent )
  {
   llinfos << "Trying to Copy " << ent->getName() << " position to clipboard : " << ent->getPosition() << llendl;
   LLVector3d agentPosGlobal=ent->getPosition();

   std::string stringVec = "<";
   stringVec.append(longfloat(agentPosGlobal.mdV[VX]));
   stringVec.append(", ");
   stringVec.append(longfloat(agentPosGlobal.mdV[VY]));
   stringVec.append(", ");
   stringVec.append(longfloat(agentPosGlobal.mdV[VZ]));
   stringVec.append(">");

   gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(stringVec));

  }
 }
}
//<edit> END OF SIMMS VOIDS

void LLFloaterAvatarList::sendKeys()
{
	 LLViewerRegion* regionp = gAgent.getRegion();
        if(!regionp)return;//ALWAYS VALIDATE DATA
        std::ostringstream ids;
        static int last_transact_num = 0;
        int transact_num = (int)gFrameCount;
        int num_ids = 0;

        if(!gSavedSettings.getBOOL("RadarChatKeys"))
	{
                return;
	}

        if(transact_num > last_transact_num)
	{
                last_transact_num = transact_num;
	}
        else
	{
                //on purpose, avatar IDs on map don't change until the next frame.
                //no need to send more than once a frame.
                return;
	}

	if (!regionp) return; // caused crash if logged out/connection lost
        for (int i = 0; i < regionp->mMapAvatarIDs.count(); i++)
	{
                const LLUUID &id = regionp->mMapAvatarIDs.get(i);

                ids << "," << id.asString();
                ++num_ids;


                if(ids.tellp() > 200)
		{
                        gMessageSystem->newMessage("ScriptDialogReply");
                        gMessageSystem->nextBlock("AgentData");
                        gMessageSystem->addUUID("AgentID", gAgent.getID());
                        gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
                        gMessageSystem->nextBlock("Data");
                        gMessageSystem->addUUID("ObjectID", gAgent.getID());
                        gMessageSystem->addS32("ChatChannel", -777777777);
                        gMessageSystem->addS32("ButtonIndex", 1);
                        gMessageSystem->addString("ButtonLabel",llformat("%d,%d", transact_num, num_ids) + ids.str());
                        gAgent.sendReliableMessage();

                        num_ids = 0;
                        ids.seekp(0);
                        ids.str("");
		}
	}
	if(num_ids > 0)
	{
                gMessageSystem->newMessage("ScriptDialogReply");
                gMessageSystem->nextBlock("AgentData");
                gMessageSystem->addUUID("AgentID", gAgent.getID());
                gMessageSystem->addUUID("SessionID", gAgent.getSessionID());
                gMessageSystem->nextBlock("Data");
                gMessageSystem->addUUID("ObjectID", gAgent.getID());
                gMessageSystem->addS32("ChatChannel", -777777777);
                gMessageSystem->addS32("ButtonIndex", 1);
                gMessageSystem->addString("ButtonLabel",llformat("%d,%d", transact_num, num_ids) + ids.str());
                gAgent.sendReliableMessage();
        }
}
//static
void LLFloaterAvatarList::sound_trigger_hook(LLMessageSystem* msg,void **)
{
	LLUUID  sound_id,owner_id;
        msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
        msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
        if(owner_id == gAgent.getID() && sound_id == LLUUID("76c78607-93f9-f55a-5238-e19b1a181389"))
        {
                //lets ask if they want to turn it on.
                if(gSavedSettings.getBOOL("RadarChatKeys"))
                {
                        LLFloaterAvatarList::getInstance()->sendKeys();
                }else
                {
                        LLSD args;
			args["MESSAGE"] = "An object owned by you has request the keys from your radar.\nWould you like to enable announcing keys to objects in the sim?";
			LLNotifications::instance().add("GenericAlertYesCancel", args, LLSD(), onConfirmRadarChatKeys);
                }
        }
}
// static
bool LLFloaterAvatarList::onConfirmRadarChatKeys(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if(option == 0) // yes
	{
		gSavedSettings.setBOOL("RadarChatKeys",TRUE);
                LLFloaterAvatarList::getInstance()->sendKeys();
	}
	return false;
}
//static
void LLFloaterAvatarList::onClickSendKeys(void *userdata)
{
	LLFloaterAvatarList::getInstance()->sendKeys();
}

static void send_freeze(const LLUUID& avatar_id, bool freeze)
{
	U32 flags = 0x0;
	if (!freeze)
	{
		// unfreeze
		flags |= 0x1;
	}

	LLMessageSystem* msg = gMessageSystem;
	LLVOAvatar* avatarp = gObjectList.findAvatar(avatar_id);

	if (avatarp && avatarp->getRegion())
	{
		msg->newMessage("FreezeUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable( avatarp->getRegion()->getHost());
	}
}

static void send_eject(const LLUUID& avatar_id, bool ban)
{	
	LLMessageSystem* msg = gMessageSystem;
	LLVOAvatar* avatarp = gObjectList.findAvatar(avatar_id);

	if (avatarp && avatarp->getRegion())
	{
		U32 flags = 0x0;
		if (ban)
		{
			// eject and add to ban list
			flags |= 0x1;
		}

		msg->newMessage("EjectUser");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("TargetID", avatar_id);
		msg->addU32("Flags", flags);
		msg->sendReliable( avatarp->getRegion()->getHost());
	}
}

static void send_estate_message(
	const char* request,
	const LLUUID &target)
{

	LLMessageSystem* msg = gMessageSystem;
	LLUUID invoice;

	// This seems to provide an ID so that the sim can say which request it's
	// replying to. I think this can be ignored for now.
	invoice.generate();

	llinfos << "Sending estate request '" << request << "'" << llendl;
	msg->newMessage("EstateOwnerMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", request);
	msg->addUUID("Invoice", invoice);

	// Agent id
	msg->nextBlock("ParamList");
	msg->addString("Parameter", gAgent.getID().asString().c_str());

	// Target
	msg->nextBlock("ParamList");
	msg->addString("Parameter", target.asString().c_str());

	msg->sendReliable(gAgent.getRegion()->getHost());
}

static void cmd_freeze(const LLUUID& avatar, const std::string &name)      { send_freeze(avatar, true); }
static void cmd_unfreeze(const LLUUID& avatar, const std::string &name)    { send_freeze(avatar, false); }
static void cmd_eject(const LLUUID& avatar, const std::string &name)       { send_eject(avatar, false); }
static void cmd_ban(const LLUUID& avatar, const std::string &name)         { send_eject(avatar, true); }
static void cmd_profile(const LLUUID& avatar, const std::string &name)     { LLFloaterAvatarInfo::showFromDirectory(avatar); }
static void cmd_estate_eject(const LLUUID &avatar, const std::string &name){ send_estate_message("teleporthomeuser", avatar); }

void LLFloaterAvatarList::doCommand(void (*func)(const LLUUID &avatar, const std::string &name))
{
	LLDynamicArray<LLUUID> ids = mAvatarList->getSelectedIDs();

	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = getAvatarEntry(avid);
		if (entry != NULL)
		{
			llinfos << "Executing command on " << entry->getName() << llendl;
			func(avid, entry->getName());
		}
	}
}

std::string LLFloaterAvatarList::getSelectedNames(const std::string& separator)
{
	std::string ret = "";
	
	LLDynamicArray<LLUUID> ids = mAvatarList->getSelectedIDs();
	for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
	{
		LLUUID avid = *itr;
		LLAvatarListEntry *entry = getAvatarEntry(avid);
		if (entry != NULL)
		{
			if (!ret.empty()) ret += separator;
			ret += entry->getName();
		}
	}

	return ret;
}

std::string LLFloaterAvatarList::getSelectedName()
{
	LLUUID id = getSelectedID();
	LLAvatarListEntry *entry = getAvatarEntry(id);
	if (entry)
	{
		return entry->getName();
	}
	return "";
}

LLUUID LLFloaterAvatarList::getSelectedID()
{
	LLScrollListItem *item = mAvatarList->getFirstSelected();
	if (item) return item->getUUID();
	return LLUUID::null;
}

//static 
void LLFloaterAvatarList::callbackFreeze(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;

	if (option == 0)
	{
		self->doCommand(cmd_freeze);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_unfreeze);
	}
}

//static 
void LLFloaterAvatarList::callbackEject(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;
 
	if (option == 0)
	{
		self->doCommand(cmd_eject);
	}
	else if (option == 1)
	{
		self->doCommand(cmd_ban);
	}
}

//static 
void LLFloaterAvatarList::callbackEjectFromEstate(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	LLFloaterAvatarList *self = LLFloaterAvatarList::sInstance;

	if (option == 0)
	{
		self->doCommand(cmd_estate_eject);
	}
}

//static
void LLFloaterAvatarList::callbackIdle(void *userdata) {
	if (LLFloaterAvatarList::sInstance != NULL)
	{
		// Do not update at every frame: this would be insane !
		if (gFrameCount % LLFloaterAvatarList::sInstance->mUpdateRate == 0)
		{
			LLFloaterAvatarList::sInstance->updateAvatarList();
		}
	}
}

void LLFloaterAvatarList::onClickFreeze(void *userdata)
{
	LLSD args;
	LLSD payload;
	args["AVATAR_NAME"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	LLNotifications::instance().add("FreezeAvatarFullname", args, payload, callbackFreeze);
}

//static
void LLFloaterAvatarList::onClickEject(void *userdata)
{
	LLSD args;
	LLSD payload;
	args["AVATAR_NAME"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	LLNotifications::instance().add("EjectAvatarFullname", args, payload, callbackEject);
}

//static
void LLFloaterAvatarList::onClickMute(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;

	LLDynamicArray<LLUUID> ids = self->mAvatarList->getSelectedIDs();
	if (ids.size() > 0)
	{
		for (LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
		{
			LLUUID agent_id = *itr;
		
			std::string agent_name;
			if (gCacheName->getFullName(agent_id, agent_name))
			{
				if (LLMuteList::getInstance()->isMuted(agent_id))
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->remove(mute);	
				}
				else
				{
					LLMute mute(agent_id, agent_name, LLMute::AGENT);
					LLMuteList::getInstance()->add(mute);
				}
			}
		}
	}
}

//static
void LLFloaterAvatarList::onClickEjectFromEstate(void *userdata)
{
	LLSD args;
	LLSD payload;
	args["EVIL_USER"] = ((LLFloaterAvatarList*)userdata)->getSelectedNames();
	LLNotifications::instance().add("EstateKickUser", args, payload, callbackEjectFromEstate);
}

//static
void LLFloaterAvatarList::onClickAR(void *userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
			LLFloaterReporter::showFromObject(entry->getID());
		}
	}
}

// static
void LLFloaterAvatarList::onClickProfile(void* userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
	self->doCommand(cmd_profile);
}

//static
void LLFloaterAvatarList::onClickTeleport(void* userdata)
{
	LLFloaterAvatarList *self = (LLFloaterAvatarList*)userdata;
 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
//			llinfos << "Trying to teleport to " << entry->getName() << " at " << entry->getPosition() << llendl;
			gAgent.teleportViaLocation(entry->getPosition());
		}
	}
}

void LLFloaterAvatarList::onSelectName(LLUICtrl*, void* userdata)
{
	LLFloaterAvatarList* self = (LLFloaterAvatarList*)userdata;

 	LLScrollListItem *item = self->mAvatarList->getFirstSelected();
	if (item)
	{
		LLUUID agent_id = item->getUUID();
		LLAvatarListEntry *entry = self->getAvatarEntry(agent_id);
		if (entry)
		{
			BOOL enabled = entry->isDrawn();
			self->childSetEnabled("focus_btn", enabled);
			self->childSetEnabled("prev_in_list_btn", enabled);
			self->childSetEnabled("next_in_list_btn", enabled);
		}
	}
}

void LLFloaterAvatarList::onCommitUpdateRate(LLUICtrl*, void* userdata)
{
	LLFloaterAvatarList* self = (LLFloaterAvatarList*)userdata;

	self->mUpdateRate = gSavedSettings.getU32("RadarUpdateRate") * 3 + 3;
}

