#include "../../types.h"

#if defined (YANEURAOU_SOLVE_ENGINE)

#include "../../thread.h"
#include "../../usi.h"
#include "../../book/book.h"

Book::BookMoveSelector book;

// USIに追加オプションを設定したいときは、この関数を定義すること。
// USI::init()のなかからコールバックされる。
void USI::extra_option(USI::OptionsMap& o)
{
	// 定跡設定
	book.init(o);

	Options["BookDepthLimit"] = "0";
	Options["BookFile"] = "sorted.db";
	Options["BookOnTheFly"] = true;
	Options["IgnoreBookPly"] = true;
}

void gameover_handler(const std::string& cmd)
{
}

// 起動時に呼び出される。時間のかからない探索関係の初期化処理はここに書くこと。
void Search::init()
{
}

// isreadyコマンドの応答中に呼び出される。時間のかかる処理はここに書くこと。
void  Search::clear()
{
	// 定跡の読み込み
	book.read_book();

	Threads.clear();
}

// 探索開始時に呼び出される。
// この関数内で初期化を終わらせ、slaveスレッドを起動してThread::search()を呼び出す。
// そのあとslaveスレッドを終了させ、ベストな指し手を返すこと。
void MainThread::search()
{
	Move move;
	if (rootPos.sfen(1) == SFEN_HIRATE)
		move = rootPos.to_move(USI::to_move16("1e1d+"));
	else
		move = book.probe(rootPos);

	if (!is_ok(move))
		move = MOVE_RESIGN;

	sync_cout << "bestmove " << move << sync_endl;
}

// 探索本体。並列化している場合、ここがslaveのエントリーポイント。
void Thread::search()
{
}

#endif
