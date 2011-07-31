// <edit>
#ifndef LL_LLFLOATERINTERCEPTOR_H
#define LL_LLFLOATERINTERCEPTOR_H

#include "llfloater.h"
#include "llviewerobject.h"

class LLFloaterInterceptor
: public LLFloater
{
public:
	LLFloaterInterceptor();
	BOOL postBuild(void);
	void close(bool app_quitting);
	void updateNumberAffected();
	
private:
	virtual ~LLFloaterInterceptor();

// static stuff
public:
	static bool gInterceptorActive;
	static LLFloaterInterceptor* sInstance;
	static std::list<LLViewerObject*> affected;
	static void show();
	static void onChangeStuff(LLUICtrl* ctrl, void* userData);
	static void changeRange(F32 range);
	static void affect(LLViewerObject* object);
	static void grab(LLViewerObject* object);
	static void letGo(LLViewerObject* object);
	static bool has(LLViewerObject* vobj);
};

#endif
// </edit>
