// <edit>
 
#include "llviewerprecompiledheaders.h"
#include "llfloatertexturelog.h"
#include "lluictrlfactory.h"
#include "llchat.h"
#include "llfloaterchat.h"
#include "llagent.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include <vector>
#include <iostream>
#include <fstream>
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llsdserialize.h"
#include "llselectmgr.h"
#include "llscrolllistctrl.h"
#include "llviewerwindow.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "llpreviewtexture.h"
#include "llviewercontrol.h"

//Superlifetexturereader* list; 
//std::vector<Superlifetexturereader*> Superlifetexturereader::instances;
Superlifetexturereader* Superlifetexturereader::sInstance = NULL;
Superlifetexturereader::Superlifetexturereader():LLFloater()
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_texturelog.xml");
	//Superlifetexturereader::instances.push_back(this);
	sInstance = this;
	childSetCommitCallback("neiltexturelogger", onSelectTexture, this);
	childSetAction("opentext", opentext, this);
}


Superlifetexturereader::~Superlifetexturereader()
{
	sInstance = NULL;
}

BOOL Superlifetexturereader::postBuild(void)
{
	BOOL crapperlulz = true;//seperating definations
//	BOOL nolulz = true;//seperating definations
if(crapperlulz)
	{

	}
	return TRUE;
}
void Superlifetexturereader::show(void*)
{
	if (!sInstance)
	{
		sInstance = new Superlifetexturereader();
	}

	sInstance->open();	 /*Flawfinder: ignore*/
}

void Superlifetexturereader::textlogger(LLUUID key,LLUUID texture)
{
		if(!sInstance) return;


	if(texture.asString() == "c228d1cf-4b5d-4ba8-84f4-899a0796aa97") return;
	if(texture.asString() == "4934f1bf-3b1f-cf4f-dbdf-a72550d05bc6") return;
	

	
	
//	chat.mText = texture.asString();
LLScrollListCtrl* list = sInstance->getChild<LLScrollListCtrl>("neiltexturelogger");
//listBox1->Items->Add(sss);
LLSD element;

element["id"] = texture.asString();
	LLSD& name_columnx = element["columns"][0];
				name_columnx["column"] = "lulz";
				std::string lolok;
				gCacheName->getFullName(key,lolok);
				name_columnx["value"] = lolok;
				
LLSD& name_column = element["columns"][1];
				name_column["column"] = "name";
				
				name_column["value"] = texture.asString();
			
				list->addElement(element, ADD_TOP);
				
	
}
void Superlifetexturereader::onSelectTexture(LLUICtrl* ctrl, void* user_data)
{
	Superlifetexturereader* floater = (Superlifetexturereader*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("neiltexturelogger");
	LLUUID selection = list->getSelectedValue().asUUID();
		LLTextureCtrl* futton = floater->getChild<LLTextureCtrl>("ng");
	if (futton)
	{
		futton->setImageAssetID(selection);
	}


}
void Superlifetexturereader::opentext(void* userdata)
{
	Superlifetexturereader* floater = (Superlifetexturereader*)userdata;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("neiltexturelogger");
	LLUUID selection = list->getSelectedValue().asUUID();
	if(!LLPreview::show(selection))
			{
					// There isn't one, so make a new preview
					S32 left, top;
					gFloaterView->getNewFloaterPosition(&left, &top);
					LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
					rect.translate( left - rect.mLeft, top - rect.mTop );
					LLPreviewTexture* preview = new LLPreviewTexture("preview task texture",
															 rect,
															 std::string(selection.asString()),
															 selection);
					preview->setFocus(TRUE);
			}


}
