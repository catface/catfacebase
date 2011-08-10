// <edit>
#include "llfloater.h"


class Superlifetexturereader
: public LLFloater
{
public:

	Superlifetexturereader();
	BOOL postBuild(void);

	//static std::vector<Superlifetexturereader*> instances; // for callback-type use
 static void textlogger(LLUUID key,LLUUID texture);
  static void show(void*);
 static void onSelectTexture(LLUICtrl* ctrl, void* user_data);
 static void opentext(void* userdata);
private:
	virtual ~Superlifetexturereader();
 static Superlifetexturereader* sInstance;

};
// </edit>
