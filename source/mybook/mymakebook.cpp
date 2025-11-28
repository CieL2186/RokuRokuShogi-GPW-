#include "../config.h"
#include "mybook.h"
#include "../position.h"
#include <sstream>

using namespace std;

namespace MyBook
{

#if defined(USE_MYBOOK) && defined (ENABLE_MAKEBOOK_CMD)

	// learner.cpp
	int makebook(Position& pos, istringstream& is);

	void makebook_cmd(Position& pos, istringstream& is)
	{
		// EVAL_LEARNが有効でないときは使えないようにしておく。
#if !(defined(EVAL_LEARN) && defined(YANEURAOU_ENGINE))
		cout << "Error!:define EVAL_LEARN and YANEURAOU_ENGINE" << endl;
		return;
#else
		// 評価関数を読み込まないとPositionのset()が出来ないのでis_ready()の呼び出しが必要。
		// ただし、このときに定跡ファイルを読み込まれると読み込みに時間がかかって嫌なので一時的にno_bookに変更しておく。
		auto original_book_file = Options["BookFile"];
		Options["BookFile"] = string("no_book");

		// IgnoreBookPlyオプションがtrue(デフォルトでtrue)のときは、定跡書き出し時にply(手数)のところを無視(0)にしてしまうので、
		// これで書き出されるとちょっと嫌なので一時的にfalseにしておく。
		auto original_ignore_book_ply = (bool)Options["IgnoreBookPly"];
		Options["IgnoreBookPly"] = false;

		SCOPE_EXIT(Options["BookFile"] = original_book_file; Options["IgnoreBookPly"] = original_ignore_book_ply; );

		// ↑ SCOPE_EXIT()により、この関数を抜けるときには復旧する。

		is_ready();

		// makebookコマンド
		makebook(pos, is);
#endif
	}

#endif

}
