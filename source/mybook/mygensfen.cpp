#include "../config.h"

#if defined(EVAL_LEARN) && defined(YANEURAOU_ENGINE)

#include "mybook.h"
#include <sstream>
#include <unordered_set>
#include "../learn/multi_think.h"

using Learner::SfenReader;
using Learner::PackedSfenValue;

// これは探索部で定義されているものとする。
extern MyBook::BookMoveSelector mybook;

namespace MyBook
{
// 局面を与えて、その局面で思考させるために、やねうら王探索部が必要。
struct MakeBook
{
	MakeBook(SfenReader& sr_, MemoryBook& book_)
		: sr(sr_), book(book_), appended(false) {}

	void worker();

	// 局面ファイルをバックグラウンドで読み込むスレッドを起動する。
	void start_file_read_worker() { sr.start_file_read_worker(); }

	// sfenの読み出し器
	SfenReader& sr;

	bool stop_flag;

	// メモリ上の定跡ファイル(ここに追加していく)
	MemoryBook& book;

	// 前回から新たな指し手が追加されたかどうかのフラグ。
	bool appended;
};

void MakeBook::worker()
{
	Position pos;

	while (true)
	{
		PackedSfenValue ps;
	RetryRead:;
		if (!sr.read_to_thread_buffer(0, ps))
		{
			stop_flag = true;
			break;
		}

		StateInfo si;
		if (pos.set_from_packed_sfen(ps.sfen, &si, Threads.main()).is_not_ok())
		{
			// 変なsfenを掴かまされた。デバッグすべき！
			// 不正なsfenなのでpos.sfen()で表示できるとは限らないが、しないよりマシ。
			cout << "Error! : illigal packed sfen = " << pos.sfen() << endl;
			goto RetryRead;
		}

		auto move = Move16(ps.move);
		auto score = (int)ps.score;
		auto depth = (int)ps.depth;
		double win = ps.game_result == 1 ? 1.0 : 0.0;
		auto sfen = pos.sfen();

		BookMove bp(move, MOVE_NONE, score, depth, 1, win, 0.0);
		book.insert(sfen, bp);
	}
}

int makebook(Position&, istringstream& is)
{
	SfenReader sr(1);
	MemoryBook book;
	vector<string> filenames;

	// 棋譜ファイル格納フォルダ(ここから相対pathで棋譜ファイルを取得)
	string base_dir;

	string read_book_name = "";
	string write_book_name = "";

	while (true)
	{
		string option;
		is >> option;

		if (option == "")
			break;

		// 棋譜ファイル格納フォルダ(ここから相対pathで棋譜ファイルを取得)
		if (option == "basedir")   is >> base_dir;

		else if (option == "read_book_name")  is >> read_book_name;
		else if (option == "write_book_name") is >> write_book_name;

		// さもなくば、それはファイル名である。
		else
			filenames.push_back(option);
	}

	if (read_book_name != "" || write_book_name != "")
	{
		return 0;
	}

	// 棋譜ファイルの表示
	cout << "make from ";
	for (auto s : filenames)
		cout << s << " , ";
	cout << endl;

	cout << "base dir        : " << base_dir << endl;
	cout << "read_book_name　: " << read_book_name << endl;
	cout << "write_book_name : " << write_book_name << endl;

	// sfen reader、逆順で読むからここでreverseしておく。すまんな。
	for (auto it = filenames.rbegin(); it != filenames.rend(); ++it)
		sr.filenames.push_back(Path::Combine(base_dir, *it));

	// -----------------------------------
	//            各種初期化
	// -----------------------------------

	// 定跡の読み込み

	cout << "read book.." << endl;
	if (book.read_book(read_book_name).is_not_ok())
		cout << "..failed but , create new file." << endl;
	else
		cout << "..done" << endl;

	MakeBook make_book(sr, book);
	make_book.sr.no_shuffle = true;

	// 局面ファイルをバックグラウンドで読み込むスレッドを起動
	make_book.start_file_read_worker();

	// 生成。
	make_book.worker();

	// 保存。
	book.write_book(write_book_name);

	return 1;
}

} // namespace MyBook

namespace Learner
{

struct MultiThinkGenSfenForBook : public MultiThink
{
	MultiThinkGenSfenForBook(SfenWriter& sw_, int search_depth_, u64 nodes_limit_, const string& book_file_name_)
		: sw(sw_), search_depth(search_depth_), nodes_limit(nodes_limit_), book_file_name(book_file_name_) {}


	void do_move(Position& pos, Move move, StateInfo* states)
	{
		ASSERT_LV3(is_ok(move) && pos.pseudo_legal(move) && pos.legal(move));

		pos.do_move(move, states[pos.game_ply()]);

		ASSERT_LV3(pos.pos_is_ok());

		Eval::evaluate(pos);
	}

	virtual void thread_worker(size_t thread_id);
	void start_file_write_worker() { sw.start_file_write_worker(); }

	int eval_limit;

	int write_minply;
	int write_maxply;

	u64 nodes_limit;
	int search_depth;

	Color use_book_player;
	string book_file_name;

	SfenWriter& sw;
};

void MultiThinkGenSfenForBook::thread_worker(size_t thread_id)
{
	const int MAX_PLY2 = write_maxply;

	// StateInfoを最大手数分 + SearchのPVでleafにまで進めるbuffer
	vector<StateInfo> states_((size_t)MAX_PLY2 + MAX_PLY);
	StateInfo* const states = &states_[0];

	// Positionに対して従属スレッドの設定が必要。
	// 並列化するときは、Threads (これが実体が vector<Thread*>なので、
	// Threads[0]...Threads[thread_num-1]までに対して同じようにすれば良い。
	auto& th = *Threads[thread_id];

	auto& pos = th.rootPos;

	// 終了フラグ
	bool quit = false;

	Move move;

	PSVector a_psv;
	a_psv.reserve(MAX_PLY2 + MAX_PLY);

	while (!quit)
	{
		// -- 1局分スタート

		// 自分スレッド用の置換表があるはずなので自分の置換表だけをクリアする。
		th.tt.clear();

		// 探索部で定義されているBookMoveSelectorのメンバを参照する。
		auto& book = ::book;

		// 局面の初期化
		pos.set_hirate(states, &th);

		// 局面バッファのクリア
		a_psv.clear();

		Value lastValue = VALUE_NONE;

		u64 nodes = nodes_limit;

		while (pos.game_ply() < MAX_PLY2
			   && !pos.is_mated()
			   && pos.is_repetition() != REPETITION_DRAW /* 千日手 */)
		{
			// 定跡
			if (pos.side_to_move() == use_book_player)
			{
				if ((move = mybook.probe(pos)) != MOVE_NONE)
				{

				}
			}

			auto pv_value = search(pos, search_depth);

			lastValue = pv_value.first;
			auto& pv = pv_value.second;

			if (abs(lastValue) > eval_limit)
				break;

			if (write_minply <= pos.game_ply())
			{
				a_psv.emplace_back(PackedSfenValue());
				auto& psv = a_psv.back();

				// packを要求されているならpackされたsfenとそのときの評価値を書き出す。
				// 最終的な書き出しは、勝敗がついてから。
				pos.sfen_pack(psv.sfen);

				// PV leafのevaluate()の値とどちらが良いかはよくわからない。
				// PV leafの値だと詰みかけの局面で駒を捨ててて自分不利に見えるのが少し嫌。
				psv.score = (s16)lastValue;
				psv.gamePly = (u16)pos.game_ply();

				psv.depth = (u8)search_depth;

				// この局面の手番を仮で入れる。この値はファイルに書き出すまでに書き換える。
				psv.game_result = (s8)pos.side_to_move();

				// PVの初手を取り出す。これはdepth 0でない限りは存在するはず。
				psv.move = (u16)pv[0];
			}

			// search_depth手読みの指し手で局面を進める。
			// is_mated()ではないので、pv[0]として合法手が存在するはずなのだが..
			move = pv[0];
			do_move(pos, move, states);

		} // 対局シミュレーション終わり

		ASSERT_LV3(lastValue != VALUE_NONE);

		// 勝利した側
		Color win;

		if (pos.is_mated()) {
			// 負け
			// 詰まされた
			win = ~pos.side_to_move();
		}
		else if (lastValue > eval_limit) {
			// 勝ち
			win = ~pos.side_to_move();
		}
		else if (lastValue < -eval_limit) {
			// 負け
			win = ~pos.side_to_move();
		}
		else {
			// それ以外は引き分け等なので書き出さない
			// 千日手も同様。
			continue;
		}

		// 各局面に，対局の勝敗の情報を付与しておく。
		// a_psvに保存されている局面は(手番的に)連続しているものとする。

		for (auto& psv : a_psv)
		{
			// この局面の手番側が仮でgame_resultに入っている
			// 最後の局面の手番側の勝利であれば1 , 負けであれば -1 を入れる。
			auto stm = (Color)psv.game_result;
			psv.game_result = (stm == win) ? 1 : -1;

			// 局面を一つ書き出す。
			sw.write(thread_id, psv);
		}

		auto loop_count = get_next_loop_count();
		if (loop_count == UINT64_MAX)
			quit = true;

	} // while(!quit)

	sw.finalize(thread_id);
}

void my_gen_sfen(Position pos, istringstream& is)
{
	u32 thread_num = (u32)Options["Threads"];

	// 生成棋譜の個数 default = 1000局(局面の数でない)
	u64 loop_max = 1000;

	// 評価値がこの値を超えたら生成を打ち切る。
	// default = VALUE_MATE_IN_MAX_PLY - 1
	int eval_limit = VALUE_MATE_IN_MAX_PLY - 1;

	// 探索深さ
	int search_depth = 24;
	u64 nodes_limit = 0;

	int write_minply = 1;
	int write_maxply = 300;

	// 使用する定跡ファイル。
	string book_file_name = "book/flood2018.sfen";

	// 教師局面を書き出すファイル名。
	string output_file_name = "generated_kifu.bin";

	string token;

	// eval hashにhitすると初期局面付近の評価値として、hash衝突して大きな値を書き込まれてしまうと
	// eval_limitが小さく設定されているときに初期局面で毎回eval_limitを超えてしまい局面の生成が進まなくなる。
	// そのため、eval hashは無効化する必要がある。
	// あとeval hashのhash衝突したときに、変な値の評価値が使われ、それを教師に使うのが気分が悪いというのもある。
	bool use_eval_hash = false;

	// この単位でファイルに保存する。
	// ファイル名は file_1.bin , file_2.binのように連番がつく。
	u64 save_every = UINT64_MAX;

	while (true)
	{
		token = "";
		is >> token;
		if (token == "")
			break;

		if (token == "loop")
			is >> loop_max;
		else if (token == "output_file_name")
			is >> output_file_name;
		else if (token == "eval_limit")
			is >> eval_limit;
		else if (token == "search_depth")
			is >> write_minply;
		else if (token == "nodes_limit")
			is >> nodes_limit;
		else if (token == "use_eval_hash")
			is >> use_eval_hash;
		else if (token == "save_every")
			is >> save_every;
		else if (token == "book_file_name")
			is >> book_file_name;
		else
			cout << "Error! : Illegal token " << token << endl;
	}

#if defined(USE_GLOBAL_OPTIONS)
	// あとで復元するために保存しておく。
	auto oldGlobalOptions = GlobalOptions;
	GlobalOptions.use_eval_hash = use_eval_hash;
#endif

	std::cout << "gensfenforbook : " << endl
		<< "  search_depth = " << search_depth << endl
		<< "  nodes_limit = " << nodes_limit << endl
		<< "  loop_max = " << loop_max << endl
		<< "  eval_limit = " << eval_limit << endl
		<< "  thread_num (set by USI setoption) = " << thread_num << endl
		<< "  write_minply            = " << write_minply << endl
		<< "  write_maxply            = " << write_maxply << endl
		<< "  output_file_name        = " << output_file_name << endl
		<< "  use_eval_hash           = " << use_eval_hash << endl
		<< "  save_every              = " << save_every << endl
		<< "  book_file_name          = " << book_file_name << endl
		;

	// Options["Threads"]の数だけスレッドを作って実行。
	{
		SfenWriter sw(output_file_name, thread_num);
		sw.save_every = save_every;

		MultiThinkGenSfenForBook multi_think(sw, search_depth, nodes_limit, book_file_name);
		multi_think.set_loop_max(loop_max);
		multi_think.eval_limit = eval_limit;
		multi_think.write_minply = write_minply;
		multi_think.write_maxply = write_maxply;
		multi_think.use_book_player = BLACK;
		multi_think.start_file_write_worker();
		multi_think.go_think();

		// SfenWriterのデストラクタでjoinするので、joinが終わってから終了したというメッセージを
		// 表示させるべきなのでここをブロックで囲む。
	}

	std::cout << "gensfenforbook finished." << endl;

#if defined(USE_GLOBAL_OPTIONS)
	// GlobalOptionsの復元。
	GlobalOptions = oldGlobalOptions;
#endif
}

} // namespace Learner

#endif
