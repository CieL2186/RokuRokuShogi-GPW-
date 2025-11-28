#if defined (ENABLE_SOLVE_CMD)

#include "../types.h"
#include "../search.h"
#include "../thread.h"
#include "../usi.h"
#include "../tt.h"
#include "../book/book.h"

#include <string>
#include <sstream>
#include <ios>     // std::right
#include <iomanip> // std::setw, std::setfill
#include <unordered_map>
#include <unordered_set>

using namespace std;

// C#のstring.Split()みたいなの
vector<string> split(const string& s, char delim)
{
	vector<string> elems;
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		if (!item.empty()) {
			elems.push_back(item);
		}
	}
	return elems;
}

void position_cmd(Position& pos, istringstream& is, StateListPtr& states, vector<Move>& moves)
{
	Move m;
	string token;

	states = StateListPtr(new StateList(1));
	pos.set_hirate(&states->back(), Threads.main());

	while (is >> token && (m = USI::to_move(pos, token)) != MOVE_NONE)
	{
		states->emplace_back();
		pos.do_move(m, states->back());
		moves.push_back(m);
	}
}

struct MyBookMove
{
	Move16 move;
	int value;
	int mate_ply;

	MyBookMove() : move(MOVE_NONE), value(0), mate_ply(MAX_PLY) {};
};

struct Solver
{
	void extend(Position& pos, istringstream& is, StateListPtr& states);
	void dfs(Position& pos, istringstream& is, StateListPtr& states);
	void build(Position& pos, istringstream& is, StateListPtr& states);

private:
	bool extend_impl(Position& pos, const vector<Move>& root_moves, const string& root_sfen, const int eval_llimit, const int eval_ulimit, u64 byoyomi);
	int dfs_impl(Position& pos, const string& root_sfen, bool& success);
	void Solver::write_book(Position& pos, string& filepath);

	fstream fs;
	unordered_set<string> sfens, leaf_nodes;
	unordered_map<string, int> and_nodes, exand_nodes;
	unordered_map<string, Move16> or_nodes;
	unordered_map<string, MyBookMove> memory;
};

void Solver::extend(Position& pos, istringstream& is, StateListPtr& states)
{
	// 評価値の上限
	int eval_ulimit = VALUE_MATE_IN_MAX_PLY;

	// 秒読みの時間 (ms)
	u64 byoyomi = 5000;

	// 探索回数の上限
	// u64 maximum_count = 10;

	// MultiPV
	// u64 multi_pv = 1;

	// ファイル名
	string read_file_name = "read.txt";
	string write_file_name = "result.txt";
	string database_file_name = "";

	// ファイルの分割
	bool separate = true;

	string token;
	while (is >> token)
	{
		if (token == "eval_limit")
			is >> eval_ulimit;
		else if (token == "byoyomi")
			is >> byoyomi;
		// else if (token == "maximum_count")
		// 	is >> maximum_count;
		else if (token == "read_file_name")
			is >> read_file_name;
		else if (token == "write_file_name")
			is >> write_file_name;
		else if (token == "database_file_name")
			is >> database_file_name;
		else if (token == "separate")
			separate = true;
	}

	cout << "solve extend : "
		<< "\neval_limit         = " << eval_ulimit
		<< "\nbyoyomi            = " << byoyomi
		<< "\nread_file_name     = " << read_file_name
		<< "\nwrite_file_name    = " << write_file_name
		<< "\ndatabase_file_name = " << database_file_name
		<< "\nseparate           = " << (separate ? "True" : "False")
		<< '\n';

	auto read_database = [&](const string& filename)
	{
		u64 count = 0;
		cout << "read : " << filename << '\n';

		ifstream ifs(filename, ios::in);
		ASSERT(ifs);

		string line;
		while (getline(ifs, line))
		{
			auto sp = split(line, ' ');
			if (sp[0] == "w") continue;

			sfens.insert(line);
		}

		cout << sfens.size() << '\n';
	};

	vector<string> lines;
	auto result = FileOperator::ReadAllLines(read_file_name, lines);
	ASSERT(result.is_ok());

	if (database_file_name != "")
		read_database(database_file_name);

	vector<int> v = { 25319,30949,34504,34593,37104 };
	int j = 0;
	int loop = 0;
	while (j < v.size())
	{
		auto i = v[j];
		string line = lines[i];

		// 進捗を表示する
		cout << Tools::now_string() << " , " << i << "/" << lines.size() - 1 << ' ' << sfens.size() << '\n';

		// 置換表をクリアする
		// TT.clear();

		// 評価値の下限
		int eval_llimit = 2000;
		{
			auto p0 = lines[i].rfind('.');
			if (p0 != string::npos)
			{
				// eval_llimit = stoi(line.substr(p0 + 1));
				line = line.substr(0, p0 - 1);
			}
		}

		vector<Move> root_moves;
		position_cmd(pos, istringstream(line), states, root_moves);
		string root_sfen = pos.sfen(0);

		string output_file_name;
		{
			stringstream ss;
			auto p1 = write_file_name.rfind('.');
			ss << write_file_name.substr(0, p1);

			if (separate)
				ss << setfill('0') << setw((int)to_string(lines.size()).length()) << right << to_string(i);

//			if (loop)
//				ss << '-' << to_string(loop);

			ss << write_file_name.substr(p1);
			output_file_name = ss.str();
		}

		fs.open(output_file_name, ios::out);
		bool is_ok = extend_impl(pos, root_moves, root_sfen, eval_llimit, eval_ulimit, byoyomi);
		fs << "# " << (is_ok ? "True" : "False") << '\n';
		fs.close();

		if (!is_ok)
		{
			++loop;
			TT.clear();
			cout << i << " : False\n";
		}
		else
		{
			++j;
			loop = 0;
		}

		if (loop >= 10)
			break;
	}

	cout << "done.\n";

	{
		ofstream ofs("sfen.txt", ios::out);
		for (auto s : sfens)
			ofs << s << '\n';
	}
}

bool Solver::extend_impl(Position& pos, const vector<Move>& root_moves, const string& root_sfen,
						 const int eval_llimit, const int eval_ulimit, u64 byoyomi)
{
	// root以降の棋譜集合
	deque<vector<Move>> kifs;

	// 局面の重複を除去するための集合
	// (局面の繰り返しとは異なる)
	// (京都将棋では後手番の局面)
	// 証明木を見つけられなかったときは破棄する
	unordered_set<string> sfens_c;

	// 思考条件
	Search::LimitsType limits;
	limits.enteringKingRule = EKR_NONE;
	limits.bench = limits.silent = true;
	limits.generate_all_legal_moves = true;
	limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = TimePoint(byoyomi);

	StateListPtr states;
	u64 loop = 0;

	// 思考局面を設定する
	auto set_position = [&](const vector<Move>& v)
	{
		states = StateListPtr(new StateList(1));
		pos.set_hirate(&states->back(), Threads.main());

		for (auto move : root_moves)
		{
			states->emplace_back();
			pos.do_move(move, states->back());
		}
		for (auto move : v)
		{
			states->emplace_back();
			pos.do_move(move, states->back());
		}
	};

	// 棋譜を出力する
	auto output_kif = [&](const vector<Move>& v)
	{
		stringstream ss;

		for (int i = 0; i < (int)root_moves.size(); ++i)
		{
			if (i) ss << ' ';
			ss << root_moves[i];
		}
		for (auto move : v)
			ss << ' ' << move;

		return ss.str();
	};

	// 繰り返しでないか
	auto is_ok = [](RepetitionState rs)
	{
		return rs == REPETITION_NONE
			|| rs == REPETITION_SUPERIOR
			|| rs == REPETITION_INFERIOR;
	};

	// はじめに1つ挿入
	kifs.push_back(vector<Move>());
	sfens_c.insert(root_sfen);

	while (!kifs.empty())
	{
#if 0
			cout
				<< Tools::now_string()
				<< " , loop = " << loop
				<< " , size = " << kifs.size()
				<< '\n';
#endif

		stringstream ss;

		// 棋譜を1つpopする
		// ※ root以降の指し手(初期局面からでない)
		auto kif = kifs.back();
		kifs.pop_back();

		// 棋譜を再現する
		set_position(kif);

		// 京都将棋の場合、現局面は後手番
		// ASSERT(pos.side_to_move() == WHITE);

		// 現局面に至るまでの手順を書き出す
		ss << "moves " << output_kif(kif) << '\n';

		auto search = [&](Move move, int pliesFromNull, stringstream& os)
		{
#if 0
			// 先手勝ちの局面に誘導できるか
			{
				bool rep_ok;
				bool skip = false;
				const auto npos = const_cast<Position*>(&pos);
				string sfen;

				for (Move m : MoveList<LEGAL_ALL>(*npos))
				{
					StateInfo si;
					npos->do_move(m, si);
					sfen = pos.sfen(0);
					rep_ok = is_ok(npos->is_repetition(pliesFromNull + 1));
					npos->undo_move(m);

					if (rep_ok && (sfens.count(sfen) || sfens_c.count(sfen)))
					{
						ss << move << ' ' << m << ' ' << VALUE_NONE << '\n';
						skip = true;
						break;
					}

				}

				if (skip)
					continue;
			}
#endif

			// --- 先手番の局面に対して、探索を用いて指し手を1つ選択する

			// 思考開始時間の初期化
			Time.reset();

			// 探索開始
			Threads.start_thinking(pos, states, limits);

			// 探索終了
			Threads.main()->wait_for_search_finished();

			const auto rm = Threads.main()->rootMoves;

			// moveに対する先手の指し手と評価値
			const Move ponderMove = rm[0].pv[0];
			const Value value = rm[0].score;

			if (value < eval_llimit)
			{
#if 0
				if (kif.size() >= 2)
				{
					kif.pop_back();
					kif.pop_back();
					kifs.push_back(kif);
					sfens_c.erase(cur_sfen);

					// For Loopを脱出する
					break;
				}
				else
					return false;
#else
				os << move << ' ' << ponderMove << ' ' << value << '\n';
				// fs << ss.str() << flush;
				return false;
#endif
			}
			else
			{
				// 後手の指し手、対する先手の指し手、評価値を書き出す
				os << move << ' ' << ponderMove << ' ' << value;
				if (eval_ulimit <= value && value <= VALUE_MATE)
					os << ' ' << USI::value(value);

				os << '\n';
			}

			const auto npos = const_cast<Position*>(&pos);
			{
				StateInfo st;
				npos->do_move(ponderMove, st);
			}

			// 繰り返し(千日手)でないか
			if (!is_ok(npos->is_repetition(pliesFromNull + 1)))
			{
				// fs << ss.str() << flush;
				return false;
			}

			// 評価値が1手詰みのスコアのとき
			// 局面が詰みであることを確かめる
//			if (value == VALUE_MATE - 1)
//				ASSERT(npos->is_mated());

			if (eval_ulimit <= value && value <= VALUE_MATE)
				return true;

			// 棋譜の更新
			// 局面が重複する場合は集合に追加しない
			auto sfen = npos->sfen(0);
			if (!sfens.count(sfen) && !sfens_c.count(sfen))
			{
				sfens_c.insert(sfen);

				auto nkif = kif;

				// 棋譜に2手追加する
				nkif.push_back(move);
				nkif.push_back(ponderMove);

				// 更新した棋譜を棋譜集合に追加する
				kifs.push_back(nkif);
			}

			// pos_->undo_move(ponderMove);
			return true;
		};


		for (Move move : MoveList<LEGAL_ALL>(pos))
		{
			int counter = 0;
			while (true)
			{
				stringstream os;
				
				// 思考局面
				set_position(kif);
				states->emplace_back();
				pos.do_move(move, states->back());

				// 遡り可能な手数
				const int pliesFromNull = pos.state()->pliesFromNull;

				// 繰り返し(千日手)でないか
				if (counter == 0 && !is_ok(pos.is_repetition(pliesFromNull)))
				{
					ss << move << '\n';
					fs << ss.str() << flush;
					return false;
				}

				bool is_result = search(move, pliesFromNull, os);

				if (is_result)
				{
					ss << os.str();
					break;
				}
				else
				{
					TT.clear();
					if (++counter >= 3)
					{
						ss << os.str();
						return false;
					}
				}
			};
		}

		// ファイルに出力する
		fs << ss.str() << flush;

		++loop;
	}

	// sfens_cの各要素をsfensに追加する
	for (auto sfen : sfens_c)
	{
		if (!sfens.count(sfen))
			sfens.insert(sfen);
	}

	return true;
}

void Solver::dfs(Position& pos, istringstream& is, StateListPtr& states)
{
	string book_filenames = "";
	string target_dir = "";
	string base_dir;
	string root_sfen = "";

	string token;
	while (is >> token)
	{
		if (token == "bookfilename") is >> book_filenames;
		else if (token == "targetdir") is >> target_dir;
		else if (token == "basedir")   is >> base_dir;
		else if (token == "rootsfen") is >> root_sfen;
	}

//	Options["IgnoreBookPly"] = true;

	cout << "\nsolve dfs :";
	cout << "\nbook file name  = " << book_filenames;
	cout << "\ntarget dir      = " << target_dir;
	cout << "\nbase dir        = " << base_dir;
	cout << "\n";
	cout << Tools::now_string() << endl;

	string log_base_dir = Path::Combine(base_dir, target_dir);
	auto filenames = Directory::EnumerateFiles(log_base_dir, ".txt");

	auto read_txt = [&](const string& filename)
	{
		// memory.clear();

		TextFileReader reader;
		// ReadLine()の時に行の末尾のスペース、タブを自動トリム。空行は自動スキップ。
		reader.SetTrim(true);
		reader.SkipEmptyLine(true);

		auto result = reader.Open(filename);
		ASSERT(result.is_ok());

		string or_sfen;
		string line;

		while (reader.ReadLine(line).is_ok())
		{
			if (line[0] == '#')
				continue;
			
			auto sp = split(line, ' ');
			if (sp[0] == "moves")
				position_cmd(pos, istringstream(line.substr(6)), states, vector<Move>());
			else
			{
				StateInfo si;
				Move and_move = USI::to_move(pos, sp[0]);
				ASSERT(is_ok(and_move));
				pos.do_move(and_move, si);
				or_sfen = pos.sfen(0);

				MyBookMove bm;
				bm.move = USI::to_move16(sp[1]);
				bm.mate_ply = -1;
				//bm.value = stoi(sp[2]);

				if (!memory.count(or_sfen))
					memory.insert(pair(or_sfen, bm));
				/*
				if (memory.count(b_sfen))
				{
					auto tmp = memory[b_sfen];
					if (tmp.move != bm.move && tmp.value < bm.value)
						memory[b_sfen] = bm;
				}
				else
					memory.insert(pair(b_sfen, bm));
				*/

				pos.undo_move(and_move);
			}
		}
	};

	auto log_output = [=](size_t i)
	{
		{
			string path = Path::Combine(base_dir, to_string(i) + "/or_nodes.txt");
			ofstream ofs(path, ios::app);
			for (const auto& [sfen, move] : or_nodes)
				ofs << sfen << '#' << move << '\n';
			ofs.close();
		}
		{
			string path = Path::Combine(base_dir, to_string(i) + "/and_nodes.txt");
			ofstream ofs(path, ios::app);
			for (const auto &[sfen, ply] : and_nodes)
				ofs << sfen << '#' << ply << '\n';
			ofs.close();
		}
		{
			string path = Path::Combine(base_dir, to_string(i) + "/leaf_nodes.txt");
			ofstream ofs(path, ios::app);
			for (const string sfen : leaf_nodes)
				ofs << sfen << '\n';
			ofs.close();
		}
	};

	auto clear = [=]()
	{
		cout << "clear" << endl;
		or_nodes.clear();
		and_nodes.clear();
		leaf_nodes.clear();
//		memory.clear();
	};

	clear();

	int i = 0;
	while (i < (int)filenames.size())
	{
		read_txt(filenames[i]);
		cout << '.';

		if (++i % 80 == 0)
			cout << endl << Tools::now_string() << endl;
	}

	cout << "dfs" << endl;
	{
		string log_path = Path::Combine(base_dir, to_string(i) + "/log.txt");
		fs.open(log_path, ios::app);

		string rootSfen = root_sfen; // ANDノードの局面

		states = StateListPtr(new StateList(1));
		pos.set(rootSfen, &states->back(), Threads.main());
		
		bool success = true;
		int mate = dfs_impl(pos, "", success);

		if (success)
		{
			fs << "sfen " << root_sfen << '\n';
			fs << mate << '\n';
			fs << or_nodes.size() << ' ' << and_nodes.size() << ' ' << leaf_nodes.size() << endl;
		}
		else
			fs << "fail " << endl;
		
		fs.close();
		log_output(i);
	}

	cout << "done\n";
}

int Solver::dfs_impl(Position& pos, const string& root_moves, bool& success)
{
	// cout << pos.moves_from_start() << '\n';

	// and node
	string and_sfen = pos.sfen(0);

	if (pos.is_mated())
	{
		leaf_nodes.insert(and_sfen);
		return 0;
	}

	auto rs = pos.is_repetition();
	if (rs == REPETITION_WIN || rs == REPETITION_LOSE)
	{
#if defined (KEEP_LAST_MOVE)
		fs << "startpos moves " << root_moves << ' ' << pos.moves_from_start() << '\n';
#endif
		success = false;
		return 0;
	}

	if (and_nodes.count(and_sfen))
		return and_nodes[and_sfen];

	int max_ply = 0;

	for (Move and_move : MoveList<LEGAL_ALL>(pos))
	{
		StateInfo si0, si1;
		pos.do_move(and_move, si0);

		string or_sfen = pos.sfen(0);

		// from and node
		int this_ply;
		Move or_move = MOVE_RESIGN;

		if (memory.count(or_sfen))
		{
			auto& bm = memory[or_sfen];
			if (bm.mate_ply == -1)
			{
				or_move = pos.to_move(bm.move);
				pos.do_move(or_move, si1);
				{
					this_ply = dfs_impl(pos, root_moves, success) + 2;
					bm.mate_ply = this_ply;
				}
				pos.undo_move(or_move);
			}
			else
				this_ply = bm.mate_ply;
		}
		else
			ASSERT(false);

		or_nodes.insert(pair(or_sfen, or_move));

		if (max_ply < this_ply)
			max_ply = this_ply;

		pos.undo_move(and_move);
	}

	and_nodes.insert(pair(and_sfen, max_ply));

	return max_ply;
}

void Solver::build(Position& pos, istringstream& is, StateListPtr& states)
{
	string read_filename = "";
	string target_dir = "";
	string base_dir;

	string token;
	while (is >> token)
	{
		if (token == "readfilename") is >> read_filename;
		else if (token == "basedir")   is >> base_dir;
	}

	cout << "\nread filename  = " << read_filename;
	cout << "\nbase dir       = " << base_dir;
	cout << "\n";

	auto read_txt = [&](const string& filename)
	{
		TextFileReader reader;
		auto result = reader.Open(filename);
		ASSERT(result.is_ok());

		string line;
		while (reader.ReadLine(line).is_ok())
		{
			auto sp = split(line, '#');
			and_nodes.insert(pair(sp[0], stoi(sp[1])));
		}

		cout << "read done" << endl;
	};

	auto build_moves = [&](Position& pos_)
	{
		StateInfo si0, si1;
		int ply0 = and_nodes[pos_.sfen(0)];

		for (Move m0 : MoveList<LEGAL_ALL>(pos_))
		{
			pos_.do_move(m0, si0);
			for (Move m1 : MoveList<LEGAL_ALL>(pos_))
			{
				pos_.do_move(m1, si1);
				bool quit = pos_.is_mated();
				if (quit || and_nodes.count(pos_.sfen(0)))
				{
					int ply1 = and_nodes[pos_.sfen(0)];
					if (ply0 == ply1 + 2)
					{
						fs << m0 << ' ' << m1 << ' ';
						return quit;
					}
				}
				pos_.undo_move(m1);
			}
			pos_.undo_move(m0);
		}

		return true;
		//ASSERT(false);
	};

	and_nodes.clear();
	read_txt(Path::Combine(base_dir, "../log/4e4d/00/and_nodes.txt"));
	read_txt(Path::Combine(base_dir, "../log/4e4d/01/and_nodes.txt"));
	states = StateListPtr(new StateList(1));
	pos.set_hirate(&states->back(), Threads.main());

	StateInfo si;
	string s = "4e4d+";
	pos.do_move(USI::to_move(pos, s), si);
	
	fs.open(Path::Combine(base_dir, "moves.txt"), ios::app);
	fs << "startpos moves " << s << ' ';
	while (true) 
	{
		if (build_moves(pos))
			break;
	}
	fs << '\n';
	fs.close();

	cout << "done" << endl;
}

void Solver::write_book(Position& pos, string& filepath)
{
	cout << "write book : " << filepath << '\n';

	// OR節点を書き出す
	for (const auto [sfen, move] : or_nodes)
	{
		const auto& bm = memory[sfen];

		fs << "sfen " << sfen << '\n';
		fs << bm.move
			<< " none "
			<< bm.value << '\n';
	}

	// AND節点の局面を書き出す
	for (const auto [sfen, ply] : and_nodes)
	{
		StateInfo si0, si1;
		pos.set(sfen, &si0, Threads.main());
		fs << "sfen " << sfen << '\n';

		for (Move move : MoveList<LEGAL_ALL>(pos))
		{
			pos.do_move(move, si1);
			const auto& bm = memory[pos.sfen(0)];

			fs << move << ' '
			   << bm.move << ' '
			   << -bm.value + 1 << '\n';

			pos.undo_move(move);
		}
	}

	fs.close();

	cout << "write book done" << '\n';
}

void purse(Position& pos, istringstream& is, StateListPtr& states)
{
	int eval_limit = VALUE_MATE_IN_MAX_PLY;
	string target_dir;
	string base_dir;
	string write_file_name = "write.txt";

	string token;
	while (is >> token)
	{
		if (token == "eval_limit")
			is >> eval_limit;
		else if (token == "targetdir")
			is >> target_dir;
		else if (token == "basedir")
			is >> base_dir;
		else if (token == "write_file_name")
			is >> write_file_name;
	}

	cout << "\nsolve purse ..";
	cout << "\neval limit      = " << eval_limit;
	cout << "\nbase dir        = " << base_dir;
	cout << "\ntarget dir      = " << target_dir;
	cout << "\nwrite file name = " << write_file_name;
	cout << "\n";

	string log_base_dir = Path::Combine(base_dir, target_dir);
	auto filenames = Directory::EnumerateFiles(log_base_dir, ".txt");
	unordered_set<string> sets;
	u64 loop = 0;

	ofstream fs;
	fs.open(write_file_name, ios::out);

	for (auto it = filenames.begin(); it != filenames.end(); ++it)
	{
		vector<string> lines;

		// cout << "read : " << *it << '\n';
		FileOperator::ReadAllLines(*it, lines);

		string last_kif_line;
		for (auto line : lines)
		{
			auto sp = split(line, ' ');

			if (sp[0] == "#")
				continue;
			else if (sp[0] == "moves")
			{
				states = StateListPtr(new StateList(1));
				pos.set_hirate(&states->back(), Threads.main());

				for (int i = 1; i < (int)sp.size(); ++i)
				{
					Move m = USI::to_move(pos, sp[i]);
					pos.do_move(m, states->back());
				}

				last_kif_line = line;
			}
			else
			{
				if (stoi(sp[2]) >= eval_limit)
				{
					ASSERT(sp.size() == 5);

					// 1手詰めは除外
					if (sp[3] == "mate" && sp[4] == "1")
						continue;

					Move moves[2];
					StateInfo st[2];
					for (int i = 0; i < 2; ++i)
					{
						auto m = USI::to_move(pos, sp[i]);
						ASSERT(is_ok(m));
						pos.do_move(m, st[i]);
						moves[i] = m;
					}

					++loop;

					// 重複は除外
					auto sfen = pos.sfen(0);
					if (!sets.count(sfen))
					{
						sets.insert(sfen);
						fs << last_kif_line.substr(6) << ' ' << sp[0] << ' ' << sp[1] << " ." << sp[2] << '\n';
					}

					pos.undo_move(moves[1]);
					pos.undo_move(moves[0]);
				}
			}
		}
	}

	cout << sets.size() << " nodes / " << loop << " times\n";
	cout << "done..\n";
}

void solve_cmd(Position& pos, istringstream& is, StateListPtr& states)
{
	Solver solver;
	string token;

	is_ready();
	is >> token;

	if (token == "extend")     solver.extend(pos, is, states);
	else if (token == "dfs")   solver.dfs(pos, is, states);
	else if (token == "build") solver.build(pos, is, states);
	//else if (token == "read")  solver.read_book(pos);
	else if (token == "purse") purse(pos, is, states);
}

#endif
