//#include <iostream>
//#include "bitboard.h"
//#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "usi.h"
#include "misc.h"

// ----------------------------------------
//  main()
// ----------------------------------------

int main(int argc, char* argv[])
{
	// --- 全体的な初期化
	CommandLine::init(argc, argv);
	USI::init(Options);
	Bitboards::init();   // ← 66対応Bitboardがここで初期化される
	Position::init();    // ← 66対応Position
	Search::init();

	// Threads オプションが無い場合にも耐える
	size_t thread_num = Options.count("Threads")
		? std::max((size_t)Options["Threads"], size_t(1))
		: size_t(1);
	Threads.set(thread_num);

	Eval::init();

	// USIループ
	USI::loop(argc, argv);

	// スレッド終了
	Threads.set(0);
	return 0;
}
