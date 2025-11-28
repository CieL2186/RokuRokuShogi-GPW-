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

unordered_set<string> sfens;

void parse_file(Position& pos, StateListPtr& states, const string& file_name)
{
	string sfen;
	vector<string> lines;

	cout << "read file: " << file_name << '\n';

	auto result = FileOperator::ReadAllLines(file_name, lines);
	ASSERT_LV1(result.is_ok());
	cout << "done. " << lines.size() << " lines." << '\n';

	for (auto line : lines)
	{
		if (line[0] == '#')
			continue;

		auto sp = split(line, ' ');
		if (sp[2] == "w")
		{
			sfen = line.substr(5);
			sfens.insert(sfen);
			pos.set(sfen, &states->back(), Threads.main());
		}
		else
		{
			StateInfo si[2];

			auto m0 = USI::to_move(pos, sp[0]);
			ASSERT_LV1(is_ok(m0));
			pos.do_move(m0, si[0]);

			auto m1 = USI::to_move(pos, sp[1]);
			ASSERT_LV1(is_ok(m1));
			pos.do_move(m1, si[1]);

			sfens.insert(pos.sfen(0));
			// 1手詰み
			// if (stoi(sp[2]) == VALUE_MATE - 2)

			pos.undo_move(m1);
			pos.undo_move(m0);
		}
	}
}

void experiment_cmd(Position& pos, istringstream& is, StateListPtr& states)
{
	sfens.clear();

	for (int i = 0; i < 7; ++i)
	{
		ostringstream oss;
		oss << "../log/db/mate"
			<< setfill('0') << right << setw(2) << i
			<< ".db";

		parse_file(pos, states, oss.str());
	}

	cout << sfens.size() << endl;
}

#endif
