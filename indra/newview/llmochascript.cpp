// <edit>
#include "llviewerprecompiledheaders.h"
#include "llmochascript.h"
#include "llagent.h"
#include "llfloateravatarlist.h"
#include "llinventorymodel.h"
#include "lluictrlfactory.h"
#include "llfloaterchat.h" // temp
#include "llchat.h" // temp
// Utils
void send_im(LLUUID recipient, U8 dialog, std::string message, const char* binary_bucket = NULL, S32 binary_bucket_size = 0)
{
	std::string from_agent_name;
	gAgent.getName(from_agent_name);
	gMessageSystem->newMessageFast(_PREHASH_ImprovedInstantMessage);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_MessageBlock);
	gMessageSystem->addBOOLFast(_PREHASH_FromGroup, FALSE);
	gMessageSystem->addUUIDFast(_PREHASH_ToAgentID, recipient);
	gMessageSystem->addU32Fast(_PREHASH_ParentEstateID, 0);
	gMessageSystem->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	gMessageSystem->addVector3Fast(_PREHASH_Position, LLVector3::zero);
	gMessageSystem->addU8Fast(_PREHASH_Offline, 0);
	gMessageSystem->addU8Fast(_PREHASH_Dialog, dialog);
	gMessageSystem->addUUIDFast(_PREHASH_ID, recipient == gAgent.getID() ? gAgent.getID() : (gAgent.getID() ^ recipient));
	gMessageSystem->addU32Fast(_PREHASH_Timestamp, 0);
	gMessageSystem->addStringFast(_PREHASH_FromAgentName, from_agent_name);
	gMessageSystem->addStringFast(_PREHASH_Message, message);
	gMessageSystem->addBinaryDataFast(_PREHASH_BinaryBucket, binary_bucket, binary_bucket_size);
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}
std::vector<std::string> splits(std::string input, std::string separator)
{
	S32 size = input.length();
	char* buffer = new char[size + 1];
	strncpy(buffer, input.c_str(), size);
	buffer[size] = '\0';
	std::vector<std::string> lines;
	char* result = strtok(buffer, separator.c_str());
	while(result)
	{
		lines.push_back(result);
		result = strtok(NULL, separator.c_str());
	}
	delete[] buffer;
	return lines;
}
BOOL f32cast(std::string s, F32& result)
{
	std::stringstream stream(s);
	if((stream >> result).fail())
		return FALSE;
	return TRUE;
}
// Deserialize
BOOL LLMochaScript(std::string script_text, std::vector<LLMochaScriptStep*>& steps, std::string& error_message)
{
	steps.clear();
	S32 line_num = 0;
	std::vector<std::string> lines = splits(script_text, "\n");
	std::vector<std::string>::iterator lines_end = lines.end();
	for(std::vector<std::string>::iterator lines_iter = lines.begin(); lines_iter != lines_end; ++lines_iter)
	{
		++line_num;
		std::string line = (*lines_iter);
		LLStringUtil::trim(line);
		if(line.length() == 0)
			continue;
		std::vector<std::string> tokens = splits(line, " ");
		if(!tokens.size())
			continue;
		std::string command = *(tokens.begin());
		LLStringUtil::toLower(command);
		std::string arguments = "";
		if(line.length() > tokens[0].length())
			arguments = line.substr(tokens[0].length() + 1);
		LLStringUtil::trim(arguments);
		tokens = splits(arguments, ",");
		std::vector<std::string>::iterator arg_end = tokens.end();
		for(std::vector<std::string>::iterator arg_iter = tokens.begin(); arg_iter != arg_end; ++arg_iter)
			LLStringUtil::trim(*arg_iter);
		if(tokens.size() == 1)
			if(tokens[0].length() == 0)
				tokens.clear();
		if(command == "wait")
		{
			if(tokens.size() != 2)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			F32 minimum_seconds;
			if(!f32cast(tokens[0], minimum_seconds))
			{
				error_message = llformat("Couldn't interpret %s as %s on line %d", tokens[1].c_str(), "F32", line_num);
				return FALSE;
			}
			F32 random_seconds;
			if(!f32cast(tokens[1], random_seconds))
			{
				error_message = llformat("Couldn't interpret %s as %s on line %d", tokens[1].c_str(), "F32", line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepWait(minimum_seconds, random_seconds));
		}
		else if(command == "wait_time_or_response")
		{
			if(tokens.size() != 2)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			F32 minimum_seconds;
			if(!f32cast(tokens[0], minimum_seconds))
			{
				error_message = llformat("Couldn't interpret %s as %s on line %d", tokens[1].c_str(), "F32", line_num);
				return FALSE;
			}
			F32 random_seconds;
			if(!f32cast(tokens[1], random_seconds))
			{
				error_message = llformat("Couldn't interpret %s as %s on line %d", tokens[1].c_str(), "F32", line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepWaitTimeOrResponse(minimum_seconds, random_seconds));
		}
		else if(command == "add_to_list")
		{
			if(tokens.size() != 0)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepAddToList());
		}
		else if(command == "start_typing")
		{
			if(tokens.size() != 0)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepStartTyping());
		}
		else if(command == "stop_typing")
		{
			if(tokens.size() != 0)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepStopTyping());
		}
		else if(command == "im")
		{
			if(arguments == "")
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepIM(arguments));
		}
		else if(command == "abort_if_gone")
		{
			if(tokens.size() != 0)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepAbortIfGone());
		}
		else if(command == "abort_if_none_found")
		{
			if(tokens.size() < 1)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			LLMochaScriptStepAbortIfNoneFound* step = new LLMochaScriptStepAbortIfNoneFound();
			step->setTokens(tokens);
			steps.push_back(step);
		}
		else if(command == "abort_if_any_found")
		{
			if(tokens.size() < 1)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			LLMochaScriptStepAbortIfAnyFound* step = new LLMochaScriptStepAbortIfAnyFound();
			step->setTokens(tokens);
			steps.push_back(step);
		}
		else if(command == "give")
		{
			if(tokens.size() != 1)
			{
				error_message = llformat("Incorrect number of arguments for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			LLUUID item_id = LLUUID(tokens[0]);
			if(item_id.isNull())
			{
				error_message = llformat("Null UUID for %s on line %d", command.c_str(), line_num);
				return FALSE;
			}
			steps.push_back(new LLMochaScriptStepGive(item_id));
		}
		else
		{
			error_message = llformat("Unrecognized command on line %d", line_num);
			return FALSE;
		}
	}
	return TRUE;
}
// LLMochaScriptLauncher
LLMochaScriptLauncher* LLMochaScriptLauncher::sInstance = NULL;
LLMochaScriptLauncher::LLMochaScriptLauncher(std::string script_text, void (*runner_status_callback)(std::vector<LLMochaScriptRunner*>), void (*add_to_list_callback)(LLUUID))
:	mScriptText(script_text),
	mEnabled(FALSE),
	mRunnerStatusCallback(runner_status_callback),
	mAddToListCallback(add_to_list_callback)
{
	if(sInstance)
		delete sInstance;
	sInstance = this;
	std::string error_message;
	LLMochaScript(mScriptText, mStepCache, error_message);
	gMessageSystem->addHandlerFuncFast(_PREHASH_ImprovedInstantMessage, LLMochaScriptLauncher::receiveIM, NULL);
	gMessageSystem->addHandlerFuncFast(_PREHASH_CoarseLocationUpdate, LLMochaScriptLauncher::onCoarseLocationUpdate, NULL);
}
LLMochaScriptLauncher::~LLMochaScriptLauncher()
{
	gMessageSystem->delHandlerFuncFast(_PREHASH_ImprovedInstantMessage, LLMochaScriptLauncher::receiveIM);
	gMessageSystem->delHandlerFuncFast(_PREHASH_ImprovedInstantMessage, LLMochaScriptLauncher::onCoarseLocationUpdate);
	std::vector<LLMochaScriptRunner*>::iterator end = mRunners.end();
	for(std::vector<LLMochaScriptRunner*>::iterator iter = mRunners.begin(); iter != end; ++iter)
	{
		(*iter)->cancel();
		delete (*iter);
	}
	sInstance = NULL;
}
void LLMochaScriptLauncher::start()
{
	mEnabled = TRUE;
}
void LLMochaScriptLauncher::stop()
{
	mEnabled = FALSE;
}
BOOL LLMochaScriptLauncher::isEnabled()
{
	return mEnabled;
}
BOOL LLMochaScriptLauncher::isBusy()
{
	return (mRunners.size() > 0);
}
void LLMochaScriptLauncher::addToList(std::list<LLUUID> ignore_list)
{
	std::list<LLUUID>::iterator end = ignore_list.end();
	for(std::list<LLUUID>::iterator iter = ignore_list.begin(); iter != end; ++iter)
		mIgnoreList.push_back(*iter);
}
void LLMochaScriptLauncher::addToList(LLUUID av_id)
{
	mIgnoreList.push_back(av_id);
	if(mAddToListCallback)
		mAddToListCallback(av_id);
}
void LLMochaScriptLauncher::tryLaunch(LLUUID av_id)
{
	if(!mEnabled)
		return;
	if(std::find(mIgnoreList.begin(), mIgnoreList.end(), av_id) == mIgnoreList.end())
	{
		std::vector<LLMochaScriptRunner*>::iterator end = mRunners.end();
		for(std::vector<LLMochaScriptRunner*>::iterator iter = mRunners.begin(); iter != end; ++iter)
		{
			if((*iter)->mSubject == av_id)
				return;
		}
		LLMochaScriptRunner* runner = new LLMochaScriptRunner(this, mScriptText, av_id);
		mRunners.push_back(runner);
		runner->start();
	}
}
void LLMochaScriptLauncher::runnerFinished(LLUUID av_id)
{
	for(std::vector<LLMochaScriptRunner*>::iterator iter = mRunners.begin(); iter != mRunners.end(); )
	{
		if((*iter)->mSubject == av_id)
		{
			delete (*iter);
			iter = mRunners.erase(iter);
		}
		else
			++iter;
	}
	if(mRunnerStatusCallback)
		mRunnerStatusCallback(mRunners);
}
void LLMochaScriptLauncher::receiveIM(LLMessageSystem* msg, void **user_data)
{
	if(!sInstance) // this shouldn't happen
		return;
	LLUUID sender;
	U8 dialog;
	std::string message;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, sender);
	msg->getU8Fast(_PREHASH_MessageBlock, _PREHASH_Dialog, dialog);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_Message, message);
	std::vector<LLMochaScriptRunner*>::iterator end = sInstance->mRunners.end();
	for(std::vector<LLMochaScriptRunner*>::iterator iter = sInstance->mRunners.begin(); iter != end; ++iter)
		if((*iter)->mSubject == sender)
		{
			(*iter)->receiveIM(dialog, message);
			return;
		}
}
void LLMochaScriptLauncher::onCoarseLocationUpdate(LLMessageSystem* msg, void **user_data)
{
	if(!sInstance)
		return;
	if(!sInstance->mEnabled)
		return;
	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_AgentData);
	for(S32 i = 0; i < num_blocks; i++)
	{
		LLUUID id;
		msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, id, i);
		sInstance->tryLaunch(id);
	}
}
void LLMochaScriptLauncher::cancelAll()
{
	while(mRunners.size())
		mRunners.front()->cancel();
}
void LLMochaScriptLauncher::onExecuteStep(LLMochaScriptRunner* runner, S32 step_index)
{
	if(mRunnerStatusCallback)
		mRunnerStatusCallback(mRunners);
}
// LLMochaScriptRunner
LLMochaScriptRunner::LLMochaScriptRunner(LLMochaScriptLauncher* launcher, std::string script_text, LLUUID subject)
:	mSubject(subject),
	mLauncher(launcher),
	mFinished(FALSE),
	mStatus(TRUE),
	mReceivedIM(""),
	mName("???"),
	mScriptBad(FALSE)
{
	std::string fullname;
	if(gCacheName->getFullName(subject, fullname))
		mName = fullname;
	std::string error_message;
	if(!LLMochaScript(script_text, mSteps, error_message))
		mScriptBad = TRUE;
}
LLMochaScriptRunner::~LLMochaScriptRunner()
{
	std::vector<LLMochaScriptStep*>::iterator end = mSteps.end();
	for(std::vector<LLMochaScriptStep*>::iterator iter = mSteps.begin(); iter != end; ++iter)
		delete (*iter);
}
void LLMochaScriptRunner::start()
{
	if(mScriptBad)
		finished(FALSE);
	if(!mFinished)
	{
		mStepIndex = 0;
		mStepIter = mSteps.begin();
		if(mStepIter != mSteps.end())
			executeStep(*mStepIter);
		else
			finished(TRUE);
	}
}
void LLMochaScriptRunner::executeStep(LLMochaScriptStep* step)
{
	mLauncher->onExecuteStep(this, mStepIndex);
	step->setRunner(this);
	step->execute();
}
void LLMochaScriptRunner::cancelStep(LLMochaScriptStep* step)
{
	step->setRunner(this);
	step->cancel();
}
void LLMochaScriptRunner::finishStep(BOOL status)
{
	if(mFinished)
		return;
	if(status)
	{
		mStepIndex++;
		mStepIter++;
		if(mStepIter != mSteps.end())
			executeStep(*mStepIter);
		else
			finished(TRUE);
	}
	else
		finished(FALSE);
}
void LLMochaScriptRunner::finished(BOOL status)
{
	if(!mFinished)
	{
		mStatus = status;
		mFinished = TRUE;
		mLauncher->runnerFinished(mSubject);
	}
}
void LLMochaScriptRunner::cancel()
{
	if(!mFinished)
	{
		if(mStepIter != mSteps.end())
			cancelStep(*mStepIter);
		finished(FALSE); // testzone moved from above
	}
}
void LLMochaScriptRunner::receiveIM(U8 dialog, std::string message)
{
	if(!mFinished)
	{
		if(mStepIter != mSteps.end())
			(*mStepIter)->receiveIM(dialog, message);
	}
}
// LLMochaScriptStep
LLMochaScriptStep::LLMochaScriptStep()
:	mRunner(NULL)
{
}
LLMochaScriptStep::~LLMochaScriptStep()
{
}
std::string LLMochaScriptStep::getText()
{
	return "Missingno.";
}
void LLMochaScriptStep::setRunner(LLMochaScriptRunner* runner)
{
	mRunner = runner;
}
void LLMochaScriptStep::execute()
{
	llwarns << "Override this!" << llendl;
	finished(TRUE);
}
void LLMochaScriptStep::cancel()
{
	finished(FALSE);
}
void LLMochaScriptStep::receiveIM(U8 dialog, std::string message)
{
	if(dialog == 0)
		mRunner->mReceivedIM = message;
}
void LLMochaScriptStep::finished(BOOL status)
{
	if(mRunner)
		mRunner->finishStep(status);
}
// LLMochaScriptStepWait
LLMochaScriptStepWait::LLMochaScriptStepWait(F32 minimum_seconds, F32 random_seconds)
:	LLMochaScriptStep(),
	mMinimumSeconds(minimum_seconds),
	mRandomSeconds(random_seconds),
	mStepFinisher(NULL)
{
}
std::string LLMochaScriptStepWait::getText()
{
	return llformat("wait %f, %f", mMinimumSeconds, mRandomSeconds);
}
void LLMochaScriptStepWait::execute()
{
	mStepFinisher = new LLMochaScriptStepFinisher(this, TRUE, mMinimumSeconds + ll_frand(mRandomSeconds));
}
void LLMochaScriptStepWait::cancel()
{
	if(mStepFinisher)
		mStepFinisher->finishEarly(FALSE);
	LLMochaScriptStep::cancel();
}
// LLMochaScriptStepWaitTimeOrResponse
LLMochaScriptStepWaitTimeOrResponse::LLMochaScriptStepWaitTimeOrResponse(F32 minimum_seconds, F32 random_seconds)
:	LLMochaScriptStep(),
	mMinimumSeconds(minimum_seconds),
	mRandomSeconds(random_seconds),
	mStepFinisher(NULL)
{
}
std::string LLMochaScriptStepWaitTimeOrResponse::getText()
{
	return llformat("wait_time_or_response %f, %f", mMinimumSeconds, mRandomSeconds);
}
void LLMochaScriptStepWaitTimeOrResponse::execute()
{
	mStepFinisher = new LLMochaScriptStepFinisher(this, TRUE, mMinimumSeconds + ll_frand(mRandomSeconds));
}
void LLMochaScriptStepWaitTimeOrResponse::cancel()
{
	if(mStepFinisher)
		mStepFinisher->finishEarly(FALSE);
	LLMochaScriptStep::cancel();
}
void LLMochaScriptStepWaitTimeOrResponse::receiveIM(U8 dialog, std::string message)
{
	if(dialog == 0)
	{
		mRunner->mReceivedIM = message;
		if(mStepFinisher)
			mStepFinisher->finishEarly(TRUE);
		else
			finished(TRUE);
	}
}
// LLMochaScriptStepAddToList
LLMochaScriptStepAddToList::LLMochaScriptStepAddToList()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepAddToList::getText()
{
	return "add_to_list";
}
void LLMochaScriptStepAddToList::execute()
{
	mRunner->mLauncher->addToList(mRunner->mSubject);
	finished(TRUE);
}
// LLMochaScriptStepIM
LLMochaScriptStepIM::LLMochaScriptStepIM(std::string message)
:	LLMochaScriptStep(),
	mMessage(message)
{
}
std::string LLMochaScriptStepIM::getText()
{
	return "im " + mMessage;
}
void LLMochaScriptStepIM::execute()
{
	send_im(mRunner->mSubject, IM_NOTHING_SPECIAL, mMessage);
	finished(TRUE);
}
// LLMochaScriptStepStartTyping
LLMochaScriptStepStartTyping::LLMochaScriptStepStartTyping()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepStartTyping::getText()
{
	return "start_typing";
}
void LLMochaScriptStepStartTyping::execute()
{
	send_im(mRunner->mSubject, IM_TYPING_START, "");
	finished(TRUE);
}
// LLMochaScriptStepStopTyping
LLMochaScriptStepStopTyping::LLMochaScriptStepStopTyping()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepStopTyping::getText()
{
	return "stop_typing";
}
void LLMochaScriptStepStopTyping::execute()
{
	send_im(mRunner->mSubject, IM_TYPING_STOP, "");
	finished(TRUE);
}
// LLMochaScriptStepAbortIfGone
LLMochaScriptStepAbortIfGone::LLMochaScriptStepAbortIfGone()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepAbortIfGone::getText()
{
	return "abort_if_gone";
}
void LLMochaScriptStepAbortIfGone::execute()
{
	//if(LLFloaterAvatars::find(mRunner->mSubject).region == NULL)
		finished(FALSE);
	finished(TRUE);
}
// LLMochaScriptStepAbortIfNoneFound
LLMochaScriptStepAbortIfNoneFound::LLMochaScriptStepAbortIfNoneFound()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepAbortIfNoneFound::getText()
{
	std::string text = "abort_if_none_found  ";
	std::vector<std::string>::iterator end = mTokens.end();
	for(std::vector<std::string>::iterator iter = mTokens.begin(); iter != end; ++iter)
	{
		text.append(*iter);
		text.append(", ");
	}
	return text.substr(0, text.size() - 2);
}
void LLMochaScriptStepAbortIfNoneFound::execute()
{
	std::vector<std::string>::iterator end = mTokens.end();
	for(std::vector<std::string>::iterator iter = mTokens.begin(); iter != end; ++iter)
		if(mRunner->mReceivedIM.find(*iter) != std::string::npos)
		{
			finished(TRUE);
			return;
		}
	finished(FALSE);
}
void LLMochaScriptStepAbortIfNoneFound::setTokens(std::vector<std::string> tokens)
{
	mTokens = tokens;
}
// LLMochaScriptStepAbortIfAnyFound
LLMochaScriptStepAbortIfAnyFound::LLMochaScriptStepAbortIfAnyFound()
:	LLMochaScriptStep()
{
}
std::string LLMochaScriptStepAbortIfAnyFound::getText()
{
	std::string text = "abort_if_any_found  ";
	std::vector<std::string>::iterator end = mTokens.end();
	for(std::vector<std::string>::iterator iter = mTokens.begin(); iter != end; ++iter)
	{
		text.append(*iter);
		text.append(", ");
	}
	return text.substr(0, text.size() - 2);
}
void LLMochaScriptStepAbortIfAnyFound::execute()
{
	std::vector<std::string>::iterator end = mTokens.end();
	for(std::vector<std::string>::iterator iter = mTokens.begin(); iter != end; ++iter)
		if(mRunner->mReceivedIM.find(*iter) != std::string::npos)
		{
			finished(FALSE);
			return;
		}
	finished(TRUE);
}
void LLMochaScriptStepAbortIfAnyFound::setTokens(std::vector<std::string> tokens)
{
	mTokens = tokens;
}
// LLMochaScriptStepGive
LLMochaScriptStepGive::LLMochaScriptStepGive(LLUUID item_id)
:	LLMochaScriptStep(),
	mItemID(item_id)
{
}
std::string LLMochaScriptStepGive::getText()
{
	return llformat("give %s", mItemID.asString().c_str());
}
void LLMochaScriptStepGive::execute()
{
	LLInventoryItem* itemp = gInventory.getItem(mItemID);
	if(!itemp)
		finished(FALSE);
	std::string name;
	gAgent.buildFullname(name);
	LLUUID transaction_id;
	transaction_id.generate();
	const S32 BUCKET_SIZE = sizeof(U8) + UUID_BYTES;
	U8 bucket[BUCKET_SIZE];
	bucket[0] = (U8)itemp->getType();
	memcpy(&bucket[1], &(itemp->getUUID().mData), UUID_BYTES);
	send_im(mRunner->mSubject, IM_INVENTORY_OFFERED, itemp->getName(), (char*)bucket, BUCKET_SIZE);
	gAgent.sendReliableMessage();
	finished(TRUE);
}
// LLMochaScriptStepFinisher
LLMochaScriptStepFinisher::LLMochaScriptStepFinisher(LLMochaScriptStep* step, BOOL status, F32 seconds)
:	LLEventTimer(seconds),
	mStep(step),
	mStatus(status),
	mFinished(FALSE)
{
}
BOOL LLMochaScriptStepFinisher::tick()
{
	if(!mFinished)
	{
		if(mStep)
			mStep->finished(mStatus);
		mFinished = TRUE;
	}
	return TRUE;
}
void LLMochaScriptStepFinisher::finishEarly(BOOL status)
{
	if(!mFinished)
	{
		mStatus = status;
		tick();
	}
}

// LLFloaterMochaScript
LLFloaterMochaScript* LLFloaterMochaScript::sInstance = NULL;
LLFloaterMochaScript::LLFloaterMochaScript()
:	LLFloater(),
	mScriptText(""),
	mLauncher(NULL)
{
	sInstance = this;
	gMessageSystem->addHandlerFuncFast(_PREHASH_TeleportFinish, onTeleportFinish, NULL);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mochascript.xml");
}
LLFloaterMochaScript::~LLFloaterMochaScript()
{
	if(mLauncher)
	{
		mLauncher->cancelAll();
		delete mLauncher;
	}
	gMessageSystem->delHandlerFuncFast(_PREHASH_TeleportFinish, onTeleportFinish);
	sInstance = NULL;
}
void LLFloaterMochaScript::show()
{
	if(!sInstance)
		sInstance = new LLFloaterMochaScript();
	sInstance->open();
}
BOOL LLFloaterMochaScript::postBuild()
{
	childSetLabelArg("on_off", "[ON]", std::string("off"));
	childSetAction("apply", onClickApply, this);
	childSetAction("on_off", onClickOnOff, this);
	childSetAction("stop", onClickStop, this);
	std::string file_path = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "mochascript_list.txt");
	std::ifstream stream(file_path.c_str());
	if(stream.good())
	{
		std::string str("");
		while(std::getline(stream, str))
		{
			LLUUID id(str);
			if(id.notNull())
				mIgnoreList.push_back(id);
		}
		stream.close();
	}
	childSetText("text", llformat("%d names in list", mIgnoreList.size()));
	return TRUE;
}
void LLFloaterMochaScript::onClickApply(void* user_data)
{
	if(sInstance->mLauncher)
	{
		if(sInstance->mLauncher->isBusy())
		{
			sInstance->childSetEnabled("apply", false);
			return;
		}
		else
		{
			delete sInstance->mLauncher;
			sInstance->mLauncher = NULL;
		}
	}
	sInstance->childSetLabelArg("on_off", "[ON]", std::string("off"));
	std::string text = sInstance->childGetValue("edit");
	sInstance->mScriptText = text;
	std::vector<LLMochaScriptStep*> steps;
	std::string error_message;
	if(!LLMochaScript(text, steps, error_message))
	{
		text = error_message;
		sInstance->childSetEnabled("on_off", false);
	}
	else
	{
		text = "";
		S32 step_num = 1;
		std::vector<LLMochaScriptStep*>::iterator end = steps.end();
		bool add_to_list_found = false;
		for(std::vector<LLMochaScriptStep*>::iterator iter = steps.begin(); iter != end; ++iter)
		{
			std::string step_text = (*iter)->getText();
			if(step_text == "add_to_list")
				add_to_list_found = true;
			text.append(llformat("%d. ", step_num++) + step_text + "\n\n\n");
		}
		if(!add_to_list_found)
		{
			text = "You must use add_to_list";
			sInstance->childSetEnabled("on_off", false);
		}
		else
		{
			sInstance->mLauncher = new LLMochaScriptLauncher(sInstance->mScriptText, onRunnerStatus, onAddToList);
			sInstance->mLauncher->addToList(sInstance->mIgnoreList);
			sInstance->childSetEnabled("on_off", true);
		}
	}
	sInstance->childSetText("display", text);
}
void LLFloaterMochaScript::onClickOnOff(void* user_data)
{
	if(sInstance->mLauncher)
	{
		if(sInstance->mLauncher->isEnabled())
		{
			sInstance->mLauncher->stop();
			sInstance->childSetLabelArg("on_off", "[ON]", std::string("off"));
		}
		else
		{
			sInstance->mLauncher->start();
			sInstance->childSetLabelArg("on_off", "[ON]", std::string("on"));
		}
	}
}
void LLFloaterMochaScript::onClickStop(void* user_data)
{
	if(sInstance->mLauncher)
		sInstance->mLauncher->cancelAll();
}
void LLFloaterMochaScript::onRunnerStatus(std::vector<LLMochaScriptRunner*> runners)
{
	if(!sInstance)
		return;
	sInstance->childSetEnabled("apply", runners.size() == 0);
	LLMochaScriptLauncher* launcherp = sInstance->mLauncher;
	if(!launcherp)
		return;
	std::string text = "";
	S32 step_count = launcherp->mStepCache.size();
	std::vector<LLMochaScriptRunner*>::iterator runners_end = runners.end();
	std::vector<LLMochaScriptRunner*>::iterator runners_iter;
	for(S32 i = 0; i < step_count; i++)
	{
		text.append(llformat("%d. ", i + 1) + launcherp->mStepCache[i]->getText() + "\n");
		for(runners_iter = runners.begin(); runners_iter != runners_end; ++runners_iter)
			if((*runners_iter)->mStepIndex == i)
				text.append(llformat("(%s) ", (*runners_iter)->mName.c_str()));
		text.append("\n\n");
	}
	sInstance->childSetText("display", text);
}
void LLFloaterMochaScript::onAddToList(LLUUID id)
{
	if(!sInstance)
		return;
	sInstance->mIgnoreList.push_back(id);
	std::string file_path = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "mochascript_list.txt");
	std::ofstream stream(file_path.c_str(), std::ios_base::app);
	if(stream.good())
	{
		stream << id.asString() << "\n";
		stream.close();
	}
	sInstance->childSetText("text", llformat("%d names in list", sInstance->mIgnoreList.size()));
}
void LLFloaterMochaScript::onTeleportFinish(LLMessageSystem* msg, void **user_data)
{
	if(sInstance)
		if(sInstance->childGetValue("off_on_tp"))
			if(sInstance->mLauncher)
				if(sInstance->mLauncher->isEnabled())
				{
					sInstance->mLauncher->stop();
					sInstance->childSetLabelArg("on_off", "[ON]", std::string("off"));
				}
}
// </edit>
