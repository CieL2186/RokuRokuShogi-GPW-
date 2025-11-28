#if 0

#include "types.h"

#include <string>
#include <sstream>
#include <ios>     // left, right
#include <iomanip> // setw(int), setfill(char)
#include <unordered_set>
#include "search.h"
#include "thread.h"
#include "usi.h"

using namespace std;

// C#のstring.Split()みたいなの
vector<string> split(const string& s, char delim) {
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

#if 0

struct BookMove
{
	u16 ponder_move;
	bool is_mate;
	bool is_visited;
	int score;
};

using BookMoves = unordered_map<u16, BookMove>;
unordered_map<string, BookMoves> memory;
unordered_set<string> visited;
u64 no_found;

void write_books(Position &pos)
{
	StateListPtr states = StateListPtr(new StateList(1));
	StateInfo si;
	ofstream ofs("mate.db");

	ofs << "#YANEURAOU - DB2016 1.00" << '\n';

	for (auto p : memory)
	{
		// p.first : sfen
		// p.second: BookMoves

		auto sfen = p.first;

		auto it = p.second.begin();
		if (!it->second.is_visited)
			continue;

#if 1
		// 後手の局面を書き出す
		ofs << "sfen " << sfen << '\n';

		for (auto element : p.second)
		{
			auto bm = element.second;
			ofs << Move16(element.first) << ' '
				<< Move16(bm.ponder_move) << ' '
				<< VALUE_MATE - (bm.score + 1)
				<< " # mate " << (bm.score + 1)
				<< '\n';
		}

#else
		// 先手の局面を書き出す
		pos.set(sfen, &states->back(), Threads.main());

		for (auto element : p.second)
		{
			// element.first  : 後手の指し手
			// element.second : BookMove
			// --> mate xxx

			Move m = pos.to_move(element.first);
			auto bm = element.second;

			states->emplace_back();
			pos.do_move(m, states->back());

			ofs << "sfen " << pos.sfen(0) << '\n';
			ofs << Move16(bm.ponder_move) << " "
				<< "none "
				<< VALUE_MATE - bm.score
				<< " # mate " << bm.score
				<< '\n';

			pos.undo_move(m);
			ASSERT_LV1(pos.sfen(0) == sfen);
		}
#endif
	}

	ofs.close();
}

int dfs(Position& pos, const string& sfen)
{
	int max_mate = 0;

	for (auto& element : memory[sfen])
	{
		// element.first : 後手の指し手
		// element.second: BookMoves
		auto &bm = element.second;
		bm.is_visited = true;

		if (!bm.is_mate)
		{
			StateInfo si[3];

			// 後手の局面
			pos.set(sfen, &si[0], Threads.main());

			// 後手の指し手
			Move mw = pos.to_move(Move16(element.first));
			ASSERT_LV1(is_ok(mw));
			pos.do_move(mw, si[1]);

			// 先手の指し手
			Move mb = pos.to_move(Move16(bm.ponder_move));
			ASSERT_LV1(is_ok(mb));
			pos.do_move(mb, si[2]);

			auto fix_mate = dfs(pos, pos.sfen(0));

			if (fix_mate == -1)
				return -1;
			// cout << "fixed: " << sfen << ' ' << fix_mate << '\n';

			bm.is_mate = true;
			bm.score = fix_mate;
		}

		if (max_mate < bm.score)
		{
			max_mate = bm.score;
		}
	}

	ASSERT_LV1(max_mate != 0);

	return max_mate + 2;
}

void experiment_cmd(Position& pos, istringstream& is, StateListPtr& states)
{
#if 0
	FileOperator::ReadAllLines("mate.db", lines);
	parse_file(lines);
	write_legal_moves(pos, sfens);
	cout << "done.." << '\n';
#else

	auto func = [&]()
	{
		ofstream of("output.txt");
		string sfen;
		vector<string> lines;

		FileOperator::ReadAllLines("result_ver2.txt", lines);
		for (auto line : lines)
		{
			auto sp = split(line, ' ');
			if (sp[1] == "w")
			{
				sfen = line;
				pos.set(sfen, &states->back(), Threads.main());
			}
			else if (sp[2] == "mate")
			{
				if (stoi(sp[3]) == 1)
					continue;

				StateInfo si[2];
				auto m0 = USI::to_move(pos, sp[0]);
				ASSERT_LV1(is_ok(m0));
				pos.do_move(m0, si[0]);

				auto m1 = USI::to_move(pos, sp[1]);
				ASSERT_LV1(is_ok(m1));
				pos.do_move(m1, si[1]);

				of << pos.sfen(0) << '\n';

				pos.undo_move(m1);
				pos.undo_move(m0);
			}
		}
	};

	auto construct = [&]()
	{
		u16 move;
		string sfen;
		bool is_count = false;

		cout << "read file" << '\n';
		for (int i = 9; i <= 9; ++i)
		{
			vector<string> lines;

			ostringstream oss;
#if 1
			oss << "../log/extend/extend"
				<< setfill('0') << right << setw(3) << i
				<< "_fix.txt";
#else
			oss << "result_ver2_fix.txt";
#endif
			auto filename = oss.str();
			cout << filename << '\n';

			FileOperator::ReadAllLines(filename, lines);

			for (auto line : lines)
			{
				auto sp = split(line, ' ');
				if (sp[0] == "#")
					continue;
				else if (sp[1] == "w")
				{
					sfen = line;
					is_count = memory.count(sfen);
				}
				else
				{
					BookMove bm;
					move = USI::to_move16(sp[0]).to_u16();
					bm.ponder_move = USI::to_move16(sp[1]).to_u16();
					ASSERT_LV1(move != 0 && bm.ponder_move != 0);

					bm.is_visited = false;
					if (sp[2] == "mate")
					{
						bm.score = stoi(sp[3]);
						bm.is_mate = bm.score == 1 /*true*/;
					}
					else
					{
						bm.score = MAX_PLY;
						bm.is_mate = false;
					}

					if (!is_count)
					{
						if (!memory.count(sfen))
						{
							BookMoves moves;
							moves.insert(std::pair(move, bm));
							memory.insert(std::pair(sfen, moves));
						}
						else
						{
							auto& moves = memory[sfen];
							moves.insert(std::pair(move, bm));
						}
					}
					else
					{
						auto& moves = memory[sfen];
						if (bm.score < moves[move].score)
						{
							// 上書き
							moves[move] = bm;
						}
					}
				}
			}
		}
	};

	// func(); // output.txtの作成
	memory.clear();
	construct();
	cout << "memory size = " << memory.size() << '\n'; // 34,276,232

#if 1
	ofstream of("result_extend.txt");
	vector<string> lines, sfens;
#if 0
	FileOperator::ReadAllLines("result_ver2_fix.txt", lines);
	for (auto line : lines)
	{
		auto sp = split(line, ' ');
		if (sp[1] == "w")
			sfens.push_back(line);
	}

	u64 counter = 0;
	for (auto sfen : sfens)
	{
		if (!memory.count(sfen))
			counter++;
	}

	cout << counter << '/' << sfens.size() << '\n';
	cout << memory.size() << '\n';
	cout << memory.size() + counter << '\n';

#else
	FileOperator::ReadAllLines("../log/output.txt", lines);
	no_found = 0;

	for (size_t i : {4442})
	{
		auto sfen = lines.at(i);
		auto mate = dfs(pos, sfen);

		if (mate == -1)
		{
			of << i << " infinity loop" << '\n';
			cout << '[' << setfill('0') << right << setw(5) << i << '/'
				<< lines.size() << "] " << sfen << " infinity loop" << '\n';
		}
		else
		{
			of << i << ' ' << mate << ' ' << '\n';
			cout << '[' << setfill('0') << right << setw(5) << i << '/'
				<< lines.size() << "] " << sfen << ' ' << mate << '\n';
		}
	}

	// 最後に2手目
	// auto mate = dfs(pos, "p+nks+l/5/5/4+P/+LSK+N1 w - 0");
	// of << "ply 2 : " << mate << '\n';
	cout << memory.size() << endl;
	cout << visited.size() << endl;
	cout << no_found << endl;
	of.close();

	cout << "done.." << '\n';
	// cout << "write book" << '\n';
	// write_books(pos);
#endif
#else
	auto sfen = "p+nks+l/5/5/4+P/+LSK+N1 w - 0";
	auto mate = dfs(pos, sfen);
	cout << mate << '\n';
	write_books(pos);
#endif

	cout << "done.." << '\n';

#if 0
	auto p = memory[init_sfen];
	cout << p.size() << '\n';

	{
		StateInfo si;
		pos.set(init_sfen, &si, Threads.main());

		auto bm = p.at(0);
		auto m1 = USI::to_move(pos, bm.move);
		pos.do_move(m1, si);

		auto m2 = USI::to_move(pos, bm.ponder_move);
		pos.do_move(m2, si);

		auto sfen = pos.sfen(0);
		cout << sfen << '\n';
		cout << memory[sfen].size() << '\n';
	}
#endif
#endif
}

#else

// #define ZIP

struct Experiment
{
	void worker(Position& pos, istringstream& is, StateListPtr& states);
	void worker_impl(Position& pos, StateListPtr& states, string init_sfen);

	bool stop_flag = false;
	ofstream of;

#ifdef ZIP
	deque<PackedSfen> SFENS;
#else
	deque<string> sfens_;
	deque<string> sfens_impl_;
	unordered_set<string> sets_;
#endif
};

void Experiment::worker(Position& pos, istringstream& is, StateListPtr& states)
{
	auto purse = [&]()
	{
		vector<string> lines;
		FileOperator::ReadAllLines("../log/result_ver2.txt", lines);
		for (auto line : lines)
		{
#if 0
			sfens_.push_back(line);
#else
			string sfen;
			StateInfo st[2];
			auto sp = split(line, ' ');
			if (sp[1] == "w")
			{
				sfen = line;
				pos.set(sfen, &states->back(), Threads.main());
			}
			else if (sp[2] == "mate")
			{
				if (stoi(sp[3]) == 1)
					continue;

				auto m0 = USI::to_move(pos, sp[0]);
				ASSERT_LV1(is_ok(m0));
				pos.do_move(m0, st[0]);

				auto m1 = USI::to_move(pos, sp[1]);
				ASSERT_LV1(is_ok(m1));
				pos.do_move(m1, st[1]);

				sfens_.push_back(pos.sfen(0));
				pos.undo_move(m1);
				pos.undo_move(m0);
			}
#endif
	}
};

#if 0
	// テスト
	sync_cout << sfens_.at(2352) << sync_endl;
	// worker_impl(pos, states, sfens_[0]);
#else
	//purse();
	of.open("20210621_3.txt");
	worker_impl(pos, states, "p+nks+l/5/5/4+P/+LSK+N1 w - 0");
	of.close();

	cout << "done.." << '\n';
#endif
}

void Experiment::worker_impl(Position& pos, StateListPtr& states, string init_sfen)
{
#ifdef ZIP
	PackedSfen ps;
	pos.sfen_pack(ps);
	SFENS.push_back(ps);
#else
	sets_.clear();
	sets_.insert(init_sfen);
	sfens_impl_.clear();
	sfens_impl_.push_back(init_sfen);
#endif

	u64 loop = 0;
	Search::LimitsType limits;
	limits.byoyomi[BLACK] = limits.byoyomi[WHITE] = TimePoint(5000);
	limits.enteringKingRule = EKR_NONE;
	limits.bench = true;
	limits.silent = true;
	limits.generate_all_legal_moves = true;

	while (!sfens_impl_.empty() && !stop_flag)
	{
		StateInfo state[MAX_MOVES];

		// 後手番の局面を1つ得る。
		auto sfen = sfens_impl_.back();
		sfens_impl_.pop_back();

#ifdef ZIP
		pos.set_from_packed_sfen(sfen, &state[0], Threads.main());
#else
		pos.set(sfen, &state[0], Threads.main());
#endif

		auto ml = MoveList<LEGAL_ALL>(pos);
		// sync_cout << sfen << ' ' << ml.size() << sync_'\n';

		vector<string> tmp;
		{
			for (size_t i = 0; i < ml.size(); ++i)
			{
				pos.do_move(ml.at(i), state[i + 1]);
				tmp.push_back(pos.sfen(0));
				pos.undo_move(ml.at(i));
			}
		}

		of << pos.sfen(0) << '\n' << flush;
		for (size_t i = 0; i < tmp.size(); ++i)
		{
			auto states = StateListPtr(new StateList(1));
			pos.set(tmp[i], &states->back(), Threads.main());

			//sync_cout << sfens[i] << sync_'\n';

			// 思考開始時刻の初期化。
			Time.reset();
			Threads.start_thinking(pos, states, limits);
			Threads.main()->wait_for_search_finished();

			auto rm = Threads.main()->rootMoves;
			auto pv = rm[0].pv;
			Value value = rm[0].score;
			of << ml.at(i).move << ' ' << pv[0] << ' ' << USI::value(value) << '\n';
			//sync_cout << ml.at(i).move << '\t' << pv[0] << '\t' << USI::value(value) << sync_'\n';

			if (value < VALUE_MATE_IN_MAX_PLY)
			{
				if ((value * 100) / int(Eval::PawnValue) < 1000)
				{
					// 探索のミス
					of << "failed.." << '\n';

					while (!sfens_impl_.empty())
					{
						auto sfen = sfens_impl_.front();
						sfens_impl_.pop_front();
						of << sfen << '\n';
					}
					return;
				}
			}

			if (value < VALUE_MATE_IN_MAX_PLY)
			{
				StateInfo new_st;
				pos.do_move(pv[0], new_st);

#ifdef ZIP
				PackedSfen new_ps;
				pos.sfen_pack(new_ps);
				SFENS.push_back(new_ps);
#else
				auto new_sfen = pos.sfen(0);
				if (!sets_.count(new_sfen))
				{
					sets_.insert(new_sfen);
					sfens_impl_.push_back(new_sfen);
				}
#endif
				pos.undo_move(pv[0]);
			}
		}
		of << flush;

		++loop;
		{
			sync_cout
				<< Tools::now_string()
				<< " , loop = " << loop
				<< " , size = " << sfens_impl_.size()
				<< sync_endl;
		}
	}
}


void experiment_cmd(Position& pos, istringstream& is, StateListPtr& states)
{
	Experiment experiment;
	string token;

	experiment.stop_flag = false;
	experiment.sfens_.clear();
	experiment.sfens_impl_.clear();
	experiment.sets_.clear();

	thread th1([&]() { experiment.worker(pos, is, states); });
	thread th2([&]() {
		while (true)
		{
			cin >> token;

			if (token == "quit")
			{
				experiment.stop_flag = true;
				break;
			}
		}
	});

	th1.join();
	th2.join();
}
#endif

#endif
