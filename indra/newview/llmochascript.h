// <edit>
#ifndef LL_LLMOCHASCRIPT_H
#define LL_LLMOCHASCRIPT_H
#include "llfloater.h"
// LLMochaScriptLauncher
class LLMochaScriptRunner;
class LLMochaScriptStep;
class LLMochaScriptLauncher
{
public:
	LLMochaScriptLauncher(std::string script_text, void (*runner_status_callback)(std::vector<LLMochaScriptRunner*>), void (*add_to_list_callback)(LLUUID));
	~LLMochaScriptLauncher();
	void start();
	void stop();
	BOOL isEnabled();
	BOOL isBusy();
	void addToList(std::list<LLUUID> ignore_list);
	void addToList(LLUUID av_id);
	void tryLaunch(LLUUID av_id);
	void runnerFinished(LLUUID av_id);
	static void receiveIM(LLMessageSystem* msg, void **user_data);
	static void onCoarseLocationUpdate(LLMessageSystem* msg, void **user_data);
	void cancelAll();
	static LLMochaScriptLauncher* sInstance;
	std::vector<LLMochaScriptStep*> mStepCache;
	void onExecuteStep(LLMochaScriptRunner* runner, S32 step_index);
private:
	BOOL mEnabled;
	std::string mScriptText;
	std::list<LLUUID> mIgnoreList;
	std::vector<LLMochaScriptRunner*> mRunners;
	void (*mRunnerStatusCallback)(std::vector<LLMochaScriptRunner*>);
	void (*mAddToListCallback)(LLUUID);
};
// LLMochaScriptRunner
class LLMochaScriptStep;
class LLMochaScriptRunner
{
public:
	LLMochaScriptRunner(LLMochaScriptLauncher* launcher, std::string script_text, LLUUID subject);
	~LLMochaScriptRunner();
	void start();
	void executeStep(LLMochaScriptStep* step);
	void cancelStep(LLMochaScriptStep* step);
	void finishStep(BOOL status);
	void cancel();
	void receiveIM(U8 dialog, std::string message);
	LLUUID mSubject;
	std::string mReceivedIM;
	LLMochaScriptLauncher* mLauncher;
	std::string mName;
	S32 mStepIndex;
private:
	void finished(BOOL status);
	std::vector<LLMochaScriptStep*> mSteps;
	std::vector<LLMochaScriptStep*>::iterator mStepIter;
	BOOL mFinished;
	BOOL mStatus;
	BOOL mScriptBad;
};
// LLMochaScriptStep
class LLMochaScriptStep
{
public:
	LLMochaScriptStep();
	virtual ~LLMochaScriptStep();
	void setRunner(LLMochaScriptRunner* runner);
	virtual void execute();
	virtual void cancel();
	virtual void receiveIM(U8 dialog, std::string message);
	virtual std::string getText();
	void finished(BOOL status);
	LLMochaScriptRunner* mRunner;
};
// LLMochaScriptStepWait
class LLMochaScriptStepFinisher;
class LLMochaScriptStepWait : public LLMochaScriptStep
{
public:
	LLMochaScriptStepWait(F32 minimum_seconds, F32 random_seconds);
	std::string getText();
	void execute();
	void cancel();
private:
	F32 mMinimumSeconds;
	F32 mRandomSeconds;
	LLMochaScriptStepFinisher* mStepFinisher;
};
// LLMochaScriptStepWaitTimeOrResponse
class LLMochaScriptStepWaitTimeOrResponse : public LLMochaScriptStep
{
public:
	LLMochaScriptStepWaitTimeOrResponse(F32 minimum_seconds, F32 random_seconds);
	std::string getText();
	void execute();
	void cancel();
	void receiveIM(U8 dialog, std::string message);
private:
	F32 mMinimumSeconds;
	F32 mRandomSeconds;
	LLMochaScriptStepFinisher* mStepFinisher;
};
// LLMochaScriptStepAddToList
class LLMochaScriptStepAddToList : public LLMochaScriptStep
{
public:
	LLMochaScriptStepAddToList();
	std::string getText();
	void execute();
};
// LLMochaScriptStepIM
class LLMochaScriptStepIM : public LLMochaScriptStep
{
public:
	LLMochaScriptStepIM(std::string message);
	std::string getText();
	void execute();
private:
	std::string mMessage;
};
// LLMochaScriptStepStartTyping
class LLMochaScriptStepStartTyping : public LLMochaScriptStep
{
public:
	LLMochaScriptStepStartTyping();
	std::string getText();
	void execute();
};
// LLMochaScriptStepStopTyping
class LLMochaScriptStepStopTyping : public LLMochaScriptStep
{
public:
	LLMochaScriptStepStopTyping();
	std::string getText();
	void execute();
};
// LLMochaScriptStepAbortIfGone
class LLMochaScriptStepAbortIfGone : public LLMochaScriptStep
{
public:
	LLMochaScriptStepAbortIfGone();
	std::string getText();
	void execute();
};
// LLMochaScriptStepAbortIfNoneFound
class LLMochaScriptStepAbortIfNoneFound : public LLMochaScriptStep
{
public:
	LLMochaScriptStepAbortIfNoneFound();
	std::string getText();
	void execute();
	void setTokens(std::vector<std::string> tokens);
private:
	std::vector<std::string> mTokens;
};
// LLMochaScriptStepAbortIfAnyFound
class LLMochaScriptStepAbortIfAnyFound : public LLMochaScriptStep
{
public:
	LLMochaScriptStepAbortIfAnyFound();
	std::string getText();
	void execute();
	void setTokens(std::vector<std::string> tokens);
private:
	std::vector<std::string> mTokens;
};
// LLMochaScriptStepGive
class LLMochaScriptStepGive : public LLMochaScriptStep
{
public:
	LLMochaScriptStepGive(LLUUID item_id);
	std::string getText();
	void execute();
private:
	LLUUID mItemID;
};
// LLMochaScriptStepFinisher
class LLMochaScriptStepFinisher : public LLEventTimer
{
public:
	LLMochaScriptStepFinisher(LLMochaScriptStep* step, BOOL status, F32 seconds);
	BOOL tick();
	void finishEarly(BOOL status);
private:
	LLMochaScriptStep* mStep;
	BOOL mStatus;
	BOOL mFinished;
};

// LLFloaterMochaScript
class LLFloaterMochaScript : public LLFloater
{
public:
	LLFloaterMochaScript();
	~LLFloaterMochaScript();
	static void show();
	BOOL postBuild();
	static void onClickApply(void* user_data);
	static void onClickOnOff(void* user_data);
	static void onClickStop(void* user_data);
	static void onRunnerStatus(std::vector<LLMochaScriptRunner*> runners);
	static void onAddToList(LLUUID id);
	static void onTeleportFinish(LLMessageSystem* msg, void **user_data);
private:
	static LLFloaterMochaScript* sInstance;
	std::string mScriptText;
	LLMochaScriptLauncher* mLauncher;
	std::list<LLUUID> mIgnoreList;
};
#endif
// </edit>
