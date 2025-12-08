#include "bitboard.h"
#include "extra/long_effect.h"
#include "mate/mate.h"

#include <sstream>

using namespace std;

// ----- Bitboard const

Bitboard ALL_BB = Bitboard(UINT64_C(0xfffffffff));
Bitboard ZERO_BB = Bitboard(UINT64_C(0));

Bitboard FILE1_BB = Bitboard(UINT64_C(0x3f) << (6 * 0));
Bitboard FILE2_BB = Bitboard(UINT64_C(0x3f) << (6 * 1));
Bitboard FILE3_BB = Bitboard(UINT64_C(0x3f) << (6 * 2));
Bitboard FILE4_BB = Bitboard(UINT64_C(0x3f) << (6 * 3));
Bitboard FILE5_BB = Bitboard(UINT64_C(0x3f) << (6 * 4));
Bitboard FILE6_BB = Bitboard(UINT64_C(0x3f) << (6 * 5));

Bitboard RANK1_BB = Bitboard(UINT64_C(0x41041041) << 0);
Bitboard RANK2_BB = Bitboard(UINT64_C(0x41041041) << 1);
Bitboard RANK3_BB = Bitboard(UINT64_C(0x41041041) << 2);
Bitboard RANK4_BB = Bitboard(UINT64_C(0x41041041) << 3);
Bitboard RANK5_BB = Bitboard(UINT64_C(0x41041041) << 4);
Bitboard RANK6_BB = Bitboard(UINT64_C(0x41041041) << 5);

Bitboard FILE_BB[FILE_NB] = { FILE1_BB,FILE2_BB,FILE3_BB,FILE4_BB,FILE5_BB,FILE6_BB };
Bitboard RANK_BB[RANK_NB] = { RANK1_BB,RANK2_BB,RANK3_BB,RANK4_BB,RANK5_BB,RANK6_BB };

Bitboard ForwardRanksBB[COLOR_NB][RANK_NB] = {
	{ ZERO_BB, RANK1_BB, RANK1_BB | RANK2_BB, RANK1_BB | RANK2_BB | RANK3_BB, ~(RANK5_BB | RANK6_BB), ~RANK6_BB },
	{ ~RANK1_BB, ~(RANK1_BB | RANK2_BB), ~(RANK1_BB | RANK2_BB | RANK3_BB), RANK5_BB | RANK6_BB, RANK6_BB, ZERO_BB }
};

// 敵陣を表現するBitboard。
Bitboard EnemyField[COLOR_NB] = { RANK1_BB | RANK2_BB, RANK5_BB | RANK6_BB };


// ----- Bitboard tables

// sqの升が1であるbitboard
Bitboard SquareBB[SQ_NB_PLUS1];

// 玉、金、銀、桂、歩の利き
Bitboard KingEffectBB[SQ_NB_PLUS1];
Bitboard GoldEffectBB[SQ_NB_PLUS1][COLOR_NB];
Bitboard SilverEffectBB[SQ_NB_PLUS1][COLOR_NB];
Bitboard KnightEffectBB[SQ_NB_PLUS1][COLOR_NB];
Bitboard PawnEffectBB[SQ_NB_PLUS1][COLOR_NB];

// 盤上の駒をないものとして扱う、遠方駒の利き。香、角、飛
Bitboard LanceStepEffectBB[SQ_NB_PLUS1][COLOR_NB];
Bitboard BishopStepEffectBB[SQ_NB_PLUS1];
Bitboard RookStepEffectBB[SQ_NB_PLUS1];


// 歩が打てる筋を得るためのmask
u64 PAWN_DROP_MASKS[SQ_NB];

// LineBBは、王手の指し手生成からしか使っていないが、move_pickerからQUIET_CHECKS呼び出しているので…。
// そして、配列シュリンクした。
Bitboard LineBB[SQ_NB][4];

Bitboard CheckCandidateBB[SQ_NB_PLUS1][KING-1][COLOR_NB];
Bitboard CheckCandidateKingBB[SQ_NB_PLUS1];

u8 Slide[SQ_NB_PLUS1] = {
  1 , 1 , 1 , 1 , 1 , 1 ,
  7 , 7 , 7 , 7 , 7 , 7 ,
  13, 13, 13, 13, 13, 13,
  19, 19, 19, 19, 19, 19,
  25, 25, 25, 25, 25, 25,
  31, 31, 31, 31, 31, 31,
  0 , // SQ_NB用
};

Bitboard BetweenBB[181];
u16 BetweenIndex[SQ_NB_PLUS1][SQ_NB_PLUS1];

// SquareからSquareWithWallへの変換テーブル
SquareWithWall sqww_table[SQ_NB_PLUS1];

// ----------------------------------------------------------------------------------------------
// 飛車・角の利きのためのテーブル
// ----------------------------------------------------------------------------------------------

// 飛車の縦の利き
u64      RookFileEffect[RANK_NB + 1][16];

#if defined(USE_OLD_YANEURAOU_EFFECT)

// magic bitboardを用いない時の実装

// 角の利き
Bitboard BishopEffect[2][1856 + 1];
Bitboard BishopEffectMask[2][SQ_NB_PLUS1];
int BishopEffectIndex[2][SQ_NB_PLUS1];

// 飛車の横の利き
Bitboard RookRankEffect[FILE_NB + 1][128];

#else


// ----------------------------------
//  Magic Bitboard Table from Apery
//  https://github.com/HiraokaTakuya/apery/blob/master/src/bitboard.cpp
// ----------------------------------

// 各マスのrookが利きを調べる必要があるマスの数
const int RookBlockBits[SQ_NB_PLUS1] = {
	8, 7, 7, 7, 7, 8,
	7, 6, 6, 6, 6, 7,
	7, 6, 6, 6, 6, 7,
	7, 6, 6, 6, 6, 7,
	7, 6, 6, 6, 6, 7,
	8, 7, 7, 7, 7, 8,
	0
};

// 各マスのbishopが利きを調べる必要があるマスの数
const int BishopBlockBits[SQ_NB_PLUS1] = {
	4, 3, 3, 3, 3, 4,
	3, 3, 3, 3, 3, 3,
	3, 3, 5, 5, 3, 3,
	3, 3, 5, 5, 3, 3,
	3, 3, 3, 3, 3, 3,
	4, 3, 3, 3, 3, 4,
	0
};

// Magic Bitboard で利きを求める際のシフト量
// RookShiftBits[17], RookShiftBits[53] はマジックナンバーが見つからなかったため、
// シフト量を 1 つ減らす。(テーブルサイズを 2 倍にする。)
// この方法は issei_y さんに相談したところ、教えて頂いた方法。
// PEXT Bitboardを使用する際はシフト量を減らす必要が無い。
const int RookShiftBits[SQ_NB_PLUS1] = {
	56, 57, 57, 57, 57, 56,
	57, 58, 58, 58, 58, 57,
	57, 58, 58, 58, 58, 57,
	57, 58, 58, 58, 58, 57,
	57, 58, 58, 58, 58, 57,
	56, 57, 57, 57, 57, 56,
	0
};

// Magic Bitboard で利きを求める際のシフト量
const int BishopShiftBits[SQ_NB_PLUS1] = {
	60, 61, 61, 61, 61, 60,
	61, 61, 61, 61, 61, 61,
	61, 61, 59, 59, 61, 61,
	61, 61, 59, 59, 61, 61,
	61, 61, 61, 61, 61, 61,
	60, 61, 61, 61, 61, 60,
	0
};

#if defined (USE_BMI2)
#else
const u64 RookMagic[SQ_NB_PLUS1] = {
	UINT64_C(0x1208004001000010), UINT64_C(0x410100200002000), UINT64_C(0x340204c401800280), UINT64_C(0x208040801082804), UINT64_C(0x403400802020041), UINT64_C(0x200200c00110000),
	UINT64_C(0x2210408010000c01), UINT64_C(0x2104081100000189), UINT64_C(0x20128803000020), UINT64_C(0x200a0102800140), UINT64_C(0x440502050110200), UINT64_C(0x210010022002020),
	UINT64_C(0x8204002c00402), UINT64_C(0x14010200ab20080), UINT64_C(0xc8040c400000200), UINT64_C(0x5040406400020081), UINT64_C(0x4601002d00404c02), UINT64_C(0x4128202400034001),
	UINT64_C(0x80c008000101040), UINT64_C(0x1084038202002041), UINT64_C(0x821022020012400), UINT64_C(0x200a0104210000), UINT64_C(0x42060800100001), UINT64_C(0x102013480001c),
	UINT64_C(0x310200401400400), UINT64_C(0x210200920a100448), UINT64_C(0x3042008200002880), UINT64_C(0x1c1090400000020), UINT64_C(0x4e20411402100002), UINT64_C(0x81050800001104),
	UINT64_C(0x10201130004800), UINT64_C(0x1c28102410006000), UINT64_C(0x440180a08000000), UINT64_C(0x10081010040940), UINT64_C(0x10020050040810), UINT64_C(0x28110018080000),
};

const u64 BishopMagic[SQ_NB_PLUS1] = {
	UINT64_C(0x1043420840081400), UINT64_C(0x18c1208000201200), UINT64_C(0x1042488012206000), UINT64_C(0x22021000082200), UINT64_C(0x10720880000c0610), UINT64_C(0x50c10110c1040),
	UINT64_C(0x25080a02000040), UINT64_C(0x205040208001804), UINT64_C(0x2002420008480a00), UINT64_C(0x51303105013208), UINT64_C(0x2009022024020000), UINT64_C(0x700241000a80020),
	UINT64_C(0x521147140010100), UINT64_C(0x1440080440000100), UINT64_C(0x21580c0400404206), UINT64_C(0x5210020404020004), UINT64_C(0x21046a0100200008), UINT64_C(0x11b038f000000a00),
	UINT64_C(0x1083412001011010), UINT64_C(0x122000e00400000), UINT64_C(0x2203011800051004), UINT64_C(0x884490802000220), UINT64_C(0xc21006800400040), UINT64_C(0x82020e800000a00),
	UINT64_C(0x6411d0007200440), UINT64_C(0x311020028111089), UINT64_C(0x2440000001050), UINT64_C(0x1001090500005040), UINT64_C(0x2182030000900130), UINT64_C(0x284650040001004),
	UINT64_C(0x90264000240000), UINT64_C(0x300241a000042702), UINT64_C(0x800031100080643), UINT64_C(0x14011012040400a4), UINT64_C(0x2401049010800e04), UINT64_C(0x181610840200800),
};
#endif

// これらは一度値を設定したら二度と変更しない。
// 本当は const 化したい。
Bitboard RookAttack[4096];
int RookAttackIndex[SQ_NB_PLUS1];
Bitboard RookBlockMask[SQ_NB_PLUS1];
Bitboard BishopAttack[416];
int BishopAttackIndex[SQ_NB_PLUS1];
Bitboard BishopBlockMask[SQ_NB_PLUS1];

namespace {

	// square のマスにおける、障害物を調べる必要がある場所を調べて Bitboard で返す。
	Bitboard rookBlockMaskCalc(const Square square) {
		Bitboard result = FILE_BB[file_of(square)] ^ RANK_BB[rank_of(square)];
		if (file_of(square) != FILE_6) result &= ~FILE6_BB;
		if (file_of(square) != FILE_1) result &= ~FILE1_BB;
		if (rank_of(square) != RANK_6) result &= ~RANK6_BB;
		if (rank_of(square) != RANK_1) result &= ~RANK1_BB;
		return result;
	}

	// square のマスにおける、障害物を調べる必要がある場所を調べて Bitboard で返す。
	Bitboard bishopBlockMaskCalc(const Square square) {
		const Rank rank = rank_of(square);
		const File file = file_of(square);
		Bitboard result = ZERO_BB;
		for (auto sq : SQ)
		{
			const Rank r = rank_of(sq);
			const File f = file_of(sq);
			if (abs(rank - r) == abs(file - f))
				result |= sq;
		}
		result &= ~(RANK6_BB | RANK1_BB | FILE6_BB | FILE1_BB);
		result &= ~Bitboard(square);

		return result;
	}

	// Rook or Bishop の利きの範囲を調べて bitboard で返す。
	// occupied  障害物があるマスが 1 の bitboard
	Bitboard attackCalc(const Square square, const Bitboard& occupied, const bool isBishop) {

		// 飛車と角の利きの方角
		const SquareWithWall deltaArray[2][4] = {
			{ SQWW_U, SQWW_D , SQWW_L , SQWW_R },
			{ SQWW_LU , SQWW_LD , SQWW_RD , SQWW_RU}
		};

		Bitboard result = ZERO_BB;
		for (SquareWithWall delta : deltaArray[isBishop]) {

			// 壁に当たるまでsqを利き方向に伸ばしていく
			for (auto sq = to_sqww(square) + delta; is_ok(sq) ; sq += delta)
			{
				auto s = sqww_to_sq(sq);  // まだ障害物に当っていないのでここまでは利きが到達している
				result |= s;
				if (occupied & s) // sqの地点に障害物があればこのrayは終了。
					break;
			}
		}

		return result;
	}

	// index, bits の情報を元にして、occupied の 1 のbit を いくつか 0 にする。
	// index の値を, occupied の 1のbit の位置に変換する。
	// index   [0, 1<<bits) の範囲のindex
	// bits    bit size
	// blockMask   利きのあるマスが 1 のbitboard
	// result  occupied
	Bitboard indexToOccupied(const int index, const int bits, const Bitboard& blockMask) {
		Bitboard tmpBlockMask = blockMask;
		Bitboard result = ZERO_BB;;
		for (int i = 0; i < bits; ++i) {
			const Square sq = tmpBlockMask.pop();
			if (index & (1 << i))
				result |= sq;
		}
		return result;
	}

	void initAttacks(const bool isBishop)
	{
		auto* attacks = (isBishop ? BishopAttack : RookAttack);
		auto* attackIndex = (isBishop ? BishopAttackIndex : RookAttackIndex);
		auto* blockMask = (isBishop ? BishopBlockMask : RookBlockMask);
		auto* shift = (isBishop ? BishopShiftBits : RookShiftBits);
#if defined (USE_BMI2)
#else
		auto* magic = (isBishop ? BishopMagic : RookMagic);
#endif
		int index = 0;
		for (Square sq = SQ_11; sq < SQ_NB; ++sq) {
			blockMask[sq] = (isBishop ? bishopBlockMaskCalc(sq) : rookBlockMaskCalc(sq));
			attackIndex[sq] = index;

			const int num1s = (isBishop ? BishopBlockBits[sq] : RookBlockBits[sq]);
			for (int i = 0; i < (1 << num1s); ++i) {
				const Bitboard occupied = indexToOccupied(i, num1s, blockMask[sq]);
#if defined (USE_BMI2)
				attacks[index + occupiedToIndex(occupied & blockMask[sq], blockMask[sq])] = attackCalc(sq, occupied, isBishop);
#else
				attacks[index + occupiedToIndex(occupied, magic[sq], shift[sq])] = attackCalc(sq, occupied, isBishop);
#endif
			}
			index += 1 << (64 - shift[sq]);
		}
	}

	// Apery型の遠方駒の利きの処理で用いるテーブルの初期化
	void init_apery_attack_tables()
	{
		// 飛車の利きテーブルの初期化
		initAttacks(false);

		// 角の利きテーブルの初期化
		initAttacks(true);
	}

} // of nameless namespace


#endif // defined(USE_OLD_YANEURAOU_EFFECT)

// ----------------------------------------------------------------------------------------------

// Bitboardを表示する(USI形式ではない) デバッグ用
std::ostream& operator<<(std::ostream& os, const Bitboard& board)
{
  for (Rank rank = RANK_1; rank <= RANK_6; ++rank)
  {
	for (File file = FILE_6; file >= FILE_1; --file)
	  os << ((board & (file | rank)) ? " *" : " .");
	os << endl;
  }
  // 連続して表示させるときのことを考慮して改行を最後に入れておく。
  os << endl;
  return os;
}

// 盤上sqに駒pc(先後の区別あり)を置いたときの利き。
Bitboard effects_from(Piece pc, Square sq, const Bitboard& occ)
{
  switch (pc)
  {
  case B_PAWN: return pawnEffect(BLACK, sq);
  case B_LANCE: return lanceEffect(BLACK, sq, occ);
  case B_KNIGHT: return knightEffect(BLACK, sq);
  case B_SILVER: return silverEffect(BLACK, sq);
  case B_GOLD: case B_PRO_PAWN: case B_PRO_LANCE: case B_PRO_KNIGHT: case B_PRO_SILVER: return goldEffect(BLACK, sq);

  case W_PAWN: return pawnEffect(WHITE, sq);
  case W_LANCE: return lanceEffect(WHITE, sq, occ);
  case W_KNIGHT: return knightEffect(WHITE, sq);
  case W_SILVER: return silverEffect(WHITE, sq);
  case W_GOLD: case W_PRO_PAWN: case W_PRO_LANCE: case W_PRO_KNIGHT: case W_PRO_SILVER: return goldEffect(WHITE, sq);

	//　先後同じ移動特性の駒
  case B_BISHOP: case W_BISHOP: return bishopEffect(sq, occ);
  case B_ROOK:   case W_ROOK:   return rookEffect(sq, occ);
  case B_HORSE:  case W_HORSE:  return horseEffect(sq, occ);
  case B_DRAGON: case W_DRAGON: return dragonEffect(sq, occ);
  case B_KING:   case W_KING:   return kingEffect(sq);
  case B_QUEEN:  case W_QUEEN:  return horseEffect(sq, occ) | dragonEffect(sq, occ);
  case NO_PIECE: case PIECE_WHITE: return ZERO_BB; // これも入れておかないと初期化が面倒になる。

  default: UNREACHABLE; return ALL_BB;
  }
}


// Bitboard関連の各種テーブルの初期化。
void Bitboards::init()
{
	// ------------------------------------------------------------
	//        Bitboard関係のテーブルの初期化
	// ------------------------------------------------------------

	// 1) SquareWithWallテーブルの初期化。

	for (auto sq : SQ)
		sqww_table[sq] = SquareWithWall(SQWW_11 + (int32_t)file_of(sq) * SQWW_L + (int32_t)rank_of(sq) * SQWW_D);


	// 2) direct_tableの初期化

	for (auto sq1 : SQ)
		for (auto dir = Effect8::DIRECT_ZERO; dir < Effect8::DIRECT_NB; ++dir)
		{
			// dirの方角に壁にぶつかる(盤外)まで延長していく。このとき、sq1から見てsq2のDirectionsは (1 << dir)である。
			auto delta = Effect8::DirectToDeltaWW(dir);
			for (auto sq2 = to_sqww(sq1) + delta; is_ok(sq2); sq2 += delta)
			Effect8::direc_table[sq1][sqww_to_sq(sq2)] = Effect8::to_directions(dir);
		}


	// 3) Square型のsqの指す升が1であるBitboardがSquareBB。これをまず初期化する。

	for (auto sq : SQ)
	{
		Rank r = rank_of(sq);
		File f = file_of(sq);
		SquareBB[sq].p = (u64)1 << (f * 6 + r);
	}


	// 4) 遠方利きのテーブルの初期化
	//  thanks to Apery (Takuya Hiraoka)

	// 引数のindexをbits桁の2進数としてみなす。すなわちindex(0から2^bits-1)。
	// 与えられたmask(1の数がbitsだけある)に対して、1のbitのいくつかを(indexの値に従って)0にする。
	auto indexToOccupied = [](const int index, const int bits, const Bitboard& mask_)
	{
		auto mask = mask_;
		auto result = ZERO_BB;
		for (int i = 0; i < bits; ++i)
		{
			const Square sq = mask.pop();
			if (index & (1 << i))
			result ^= sq;
		}
		return result;
	};

	// Rook or Bishop の利きの範囲を調べて bitboard で返す。
	// occupied  障害物があるマスが 1 の bitboard
	// n = 0 右上から左下 , n = 1 左上から右下
	auto effectCalc = [](const Square square, const Bitboard& occupied, int n)
	{
		auto result = ZERO_BB;

		// 角の利きのrayと飛車の利きのray
		const SquareWithWall deltaArray[2][2] = { { SQWW_RU, SQWW_LD },{ SQWW_RD, SQWW_LU} };
		for (auto delta : deltaArray[n])
		{
			// 壁に当たるまでsqを利き方向に伸ばしていく
			for (auto sq = to_sqww(square) + delta; is_ok(sq); sq += delta)
			{
				result ^= sqww_to_sq(sq); // まだ障害物に当っていないのでここまでは利きが到達している

				if (occupied & sqww_to_sq(sq)) // sqの地点に障害物があればこのrayは終了。
					break;
			}
		}
		return result;
	};

	// pieceをsqにおいたときに利きを得るのに関係する升を返す
	auto calcBishopEffectMask = [](Square sq, int n)
	{
		Bitboard result = ZERO_BB;

		// 外周は角の利きには関係ないのでそこは除外する。
		for (Rank r = RANK_2; r <= RANK_5; ++r)
			for (File f = FILE_2; f <= FILE_5; ++f)
			{
				auto dr = rank_of(sq) - r;
				auto df = file_of(sq) - f;
				// dr == dfとdr != dfとをnが0,1とで切り替える。
				if (abs(dr) == abs(df)
					&& (!!((int)dr == (int)df) ^ n ))
						result ^= (f | r);
			}

		// sqの地点は関係ないのでクリアしておく。
		result &= ~Bitboard(sq);

		return result;
	};
  
	// 5. 飛車の縦方向の利きテーブルの初期化
	// ここでは飛車の利きを使わずに初期化しないといけない。

	for (Rank rank = RANK_1; rank <= RANK_6; ++rank)
	{
		// sq = SQ_11 , SQ_12 , ... , SQ_19
		Square sq = FILE_1 | rank;

		const int num1s = 4;
		for (int i = 0; i < (1 << num1s); ++i)
		{
			// iはsqに駒をおいたときに、その筋の2段～8段目の升がemptyかどうかを表現する値なので
			// 1ビットシフトして、1～9段目の升を表現するようにする。
			int ii = i << 1;
			Bitboard bb = ZERO_BB;
			for (int r = rank_of(sq) - 1; r >= RANK_1; --r)
			{
				bb |= file_of(sq) | (Rank)r;
				if (ii & (1 << r))
					break;
			}
			for (int r = rank_of(sq) + 1; r <= RANK_6; ++r)
			{
				bb |= file_of(sq) | (Rank)r;
				if (ii & (1 << r))
					break;
			}
			RookFileEffect[rank][i] = bb.p;
			// RookEffectFile[RANK_NB][x] には値を代入していないがC++の規約によりゼロ初期化されている。
		}
	}

#if defined(USE_OLD_YANEURAOU_EFFECT)

	// 角の利きテーブルの初期化
	for (int n : { 0 , 1 })
	{
		int index = 0;
		for (auto sq : SQ)
		{
			// sqの升に対してテーブルのどこを見るかのindex
			BishopEffectIndex[n][sq] = index;

			// sqの地点にpieceがあるときにその利きを得るのに関係する升を取得する
			auto& mask = BishopEffectMask[n][sq];
			mask = calcBishopEffectMask(sq, n);

			// p[0]とp[1]が被覆していると正しく計算できないのでNG。
			// Bitboardのレイアウト的に、正しく計算できるかのテスト。
			// 縦型Bitboardであるならp[0]のbit63を余らせるようにしておく必要がある。
			ASSERT_LV3(!(mask.cross_over()));

			// sqの升用に何bit情報を拾ってくるのか
			const int bits = mask.pop_count();

			// 参照するoccupied bitboardのbit数と、そのbitの取りうる状態分だけ..
			const int num = 1 << bits;

			for (int i = 0; i < num; ++i)
			{
				Bitboard occupied = indexToOccupied(i, bits, mask);
				// 初期化するテーブル
				BishopEffect[n][index + occupiedToIndex(occupied & mask, mask)] = effectCalc(sq, occupied, n);
			}
			index += num;
		}

		// 盤外(SQ_NB)に駒を配置したときに利きがZERO_BBとなるときのための処理
		BishopEffectIndex[n][SQ_NB] = index;

		// 何番まで使ったか出力してみる。(確保する配列をこのサイズに収めたいので)
		// cout << index << endl;
	}

	// 飛車の横の利き
	for (File file = FILE_1 ; file <= FILE_9 ; ++file )
	{
		// sq = SQ_11 , SQ_21 , ... , SQ_NBまで
		Square sq = file | RANK_1;
		
		const int num1s = 7;
		for (int i = 0; i < (1 << num1s); ++i)
		{
			int ii = i << 1;
			Bitboard bb = ZERO_BB;
			for (int f = file_of(sq) - 1; f >= FILE_1; --f)
			{
				bb |= (File)f | rank_of(sq);
				if (ii & (1 << f))
					break;
			}
			for (int f = file_of(sq) + 1; f <= FILE_9; ++f)
			{
				bb |= (File)f | rank_of(sq);
				if (ii & (1 << f))
					break;
			}
			RookRankEffect[file][i] = bb;
			// RookRankEffect[FILE_NB][x] には値を代入していないがC++の規約によりゼロ初期化されている。
		}
	}

#else

	// Apery型の遠方駒の利きの処理で用いるテーブルの初期化
	init_apery_attack_tables();

#endif

	// 6. 近接駒(+盤上の利きを考慮しない駒)のテーブルの初期化。
	// 上で初期化した、香・馬・飛の利きを用いる。

	for (auto sq : SQ)
	{
		// 玉は長さ1の角と飛車の利きを合成する
		KingEffectBB[sq] = bishopEffect(sq, ALL_BB) | rookEffect(sq, ALL_BB);
	}

	for (auto c : COLOR)
		for(auto sq : SQ)
			// 障害物がないときの香の利き
			// これを最初に初期化しないとlanceEffect()が使えない。
			LanceStepEffectBB[sq][c] = rookEffect(sq, ZERO_BB) & ForwardRanksBB[c][rank_of(sq)];

	for (auto c : COLOR)
		for (auto sq : SQ)
		{
			// 歩は長さ1の香の利きとして定義できる
			PawnEffectBB[sq][c] = lanceEffect(c, sq, ALL_BB);

			// 桂の利きは、歩の利きの地点に長さ1の角の利きを作って、前方のみ残す。
			Bitboard tmp = ZERO_BB;
			Bitboard pawn = lanceEffect(c, sq, ALL_BB);
			if (pawn)
			{
				Square sq2 = pawn.pop();
				Bitboard pawn2 = lanceEffect(c, sq2, ALL_BB); // さらに1つ前
				if (pawn2)
					tmp = bishopEffect(sq2, ALL_BB) & RANK_BB[rank_of(pawn2.pop())];
			}
			KnightEffectBB[sq][c] = tmp;

			// 銀は長さ1の角の利きと長さ1の香の利きの合成として定義できる。
			SilverEffectBB[sq][c] = lanceEffect(c, sq, ALL_BB) | bishopEffect(sq, ALL_BB);

			// 金は長さ1の角と飛車の利き。ただし、角のほうは相手側の歩の行き先の段でmaskしてしまう。
			Bitboard e_pawn = lanceEffect(~c, sq, ALL_BB);
			Bitboard mask = ZERO_BB;
			if (e_pawn)
				mask = RANK_BB[rank_of(e_pawn.pop())];
			GoldEffectBB[sq][c]= (bishopEffect(sq, ALL_BB) & ~mask) | rookEffect(sq, ALL_BB);

			// 障害物がないときの角と飛車の利き
			BishopStepEffectBB[sq] = bishopEffect(sq, ZERO_BB);
			RookStepEffectBB[sq]   = rookEffect(sq, ZERO_BB);

			// --- 以下のbitboard、あまり頻繁に呼び出さないので他のbitboardを合成して代用する。

			// 盤上の駒がないときのqueenの利き
			// StepEffectsBB[sq][c][PIECE_TYPE_BITBOARD_QUEEN] = bishopEffect(sq, ZERO_BB) | rookEffect(sq, ZERO_BB);

			// 長さ1の十字
			// StepEffectsBB[sq][c][PIECE_TYPE_BITBOARD_CROSS00] = rookEffect(sq, ALL_BB);

			// 長さ1の斜め
			// StepEffectsBB[sq][c][PIECE_TYPE_BITBOARD_CROSS45] = bishopEffect(sq, ALL_BB);
		}

	// 7) 二歩用のテーブル初期化

	for (auto sq : SQ)
		PAWN_DROP_MASKS[sq] = ~FILE_BB[SquareToFile[sq]].p;

	// 8) BetweenBB , LineBBの初期化
	{
		u16 between_index = 1;
		// BetweenBB[0] == ZERO_BBであることを保証する。

		for (auto s1 : SQ)
			for (auto s2 : SQ)
			{
				// 十字方向か、斜め方向かだけを判定して、例えば十字方向なら
				// rookEffect(sq1,Bitboard(s2)) & rookEffect(sq2,Bitboard(s1))
				// のように初期化したほうが明快なコードだが、この初期化をそこに依存したくないので愚直にやる。
					
				// これについてはあとで設定する。
				if (s1 >= s2)
					continue;

				// 方角を用いるテーブルの初期化
				if (Effect8::directions_of(s1, s2))
				{
					Bitboard bb = ZERO_BB;
					// 間に挟まれた升を1に
					Square delta = (s2 - s1) / dist(s1, s2);
					for (Square s = s1 + delta; s != s2; s += delta)
						bb |= s;

					// ZERO_BBなら、このindexとしては0を指しておけば良いので書き換える必要ない。
					if (!bb)
						continue;

					BetweenIndex[s1][s2] = between_index;
					BetweenBB[between_index++] = bb;
				}
			}

		ASSERT_LV1(between_index == 181);

		// 対称性を考慮して、さらにシュリンクする。
		for (auto s1 : SQ)
			for (auto s2 : SQ)
				if (s1 > s2)
					BetweenIndex[s1][s2] = BetweenIndex[s2][s1];

	}
	for (auto s1 : SQ)
		for (int d = 0; d < 4; ++d)
		{
			// BishopEffect0 , RookRankEffect , BishopEffect1 , RookFileEffectを用いて初期化したほうが
			// 明快なコードだが、この初期化をそこに依存したくないので愚直にやる。

			const Square deltas[4] = { SQ_RU , SQ_R , SQ_RD , SQ_U };
			const Square delta = deltas[d];
			Bitboard bb = Bitboard(s1);

			// 壁に当たるまでs1から-delta方向に延長
			for (Square s = s1; dist(s, s - delta) <= 1; s -= delta) bb |= (s - delta);

			// 壁に当たるまでs1から+delta方向に延長
			for (Square s = s1; dist(s, s + delta) <= 1; s += delta) bb |= (s + delta);

			LineBB[s1][d] = bb;
		}


	// 9) 王手となる候補の駒のテーブル初期化(王手の指し手生成に必要。やねうら王nanoでは削除予定)

#define FOREACH_KING(BB, EFFECT ) { for(auto sq : BB){ target|= EFFECT(sq); } }
#define FOREACH(BB, EFFECT ) { for(auto sq : BB){ target|= EFFECT(them,sq); } }
#define FOREACH_BR(BB, EFFECT ) { for(auto sq : BB) { target|= EFFECT(sq,ZERO_BB); } }

	for (auto Us : COLOR)
		for (auto ksq : SQ)
		{
			Color them = ~Us;
			auto enemyGold = goldEffect(them, ksq) & enemy_field(Us);
			Bitboard target = ZERO_BB;

			// 歩で王手になる可能性のあるものは、敵玉から２つ離れた歩(不成での移動) + ksqに敵の金をおいた範囲(enemyGold)に成りで移動できる
			FOREACH(pawnEffect(them, ksq), pawnEffect);
			FOREACH(enemyGold, pawnEffect);
			CheckCandidateBB[ksq][PAWN - 1][Us] = target & ~Bitboard(ksq);

			// 香で王手になる可能性のあるものは、ksqに敵の香をおいたときの利き。(盤上には何もないものとする)
			// と、王が1から3段目だと成れるので王の両端に香を置いた利きも。
			target = lanceStepEffect(them, ksq);
			if (enemy_field(Us) & ksq)
			{
				if (file_of(ksq) != FILE_1)
					target |= lanceStepEffect(them, ksq + SQ_R);
				if (file_of(ksq) != FILE_6)
					target |= lanceStepEffect(them, ksq + SQ_L);
			}
			CheckCandidateBB[ksq][LANCE - 1][Us] = target;

			// 桂で王手になる可能性のあるものは、ksqに敵の桂をおいたところに移動できる桂(不成) + ksqに金をおいた範囲(enemyGold)に成りで移動できる桂
			target = ZERO_BB;
			FOREACH(knightEffect(them, ksq) | enemyGold, knightEffect);
			CheckCandidateBB[ksq][KNIGHT - 1][Us] = target & ~Bitboard(ksq);

			// 銀も同様だが、2,3段目からの引き成りで王手になるパターンがある。(4段玉と5段玉に対して)
			target = ZERO_BB;
			FOREACH(silverEffect(them, ksq), silverEffect);
			FOREACH(enemyGold, silverEffect); // 移動先が敵陣 == 成れる == 金になるので、敵玉の升に敵の金をおいた利きに成りで移動すると王手になる。
			FOREACH(goldEffect(them, ksq), enemy_field(Us) & silverEffect); // 移動元が敵陣 == 成れる == 金になるので、敵玉の升に敵の金をおいた利きに成りで移動すると王手になる。
			CheckCandidateBB[ksq][SILVER - 1][Us] = target & ~Bitboard(ksq);

			// 金
			target = ZERO_BB;
			FOREACH(goldEffect(them, ksq), goldEffect);
			CheckCandidateBB[ksq][GOLD - 1][Us] = target & ~Bitboard(ksq);

			// 角
			target = ZERO_BB;
			FOREACH_BR(bishopEffect(ksq, ZERO_BB), bishopEffect);
			FOREACH_BR(kingEffect(ksq) & enemy_field(Us), bishopEffect); // 移動先が敵陣 == 成れる == 王の動き
			FOREACH_BR(kingEffect(ksq), enemy_field(Us) & bishopEffect); // 移動元が敵陣 == 成れる == 王の動き
			CheckCandidateBB[ksq][BISHOP - 1][Us] = target & ~Bitboard(ksq);

			// 飛・龍は無条件全域。
			// ROOKのところには馬のときのことを格納

			// 馬
			target = ZERO_BB;
			FOREACH_BR(horseEffect(ksq, ZERO_BB), horseEffect);
			CheckCandidateBB[ksq][ROOK - 1][Us] = target & ~Bitboard(ksq);

			// 王(24近傍が格納される)
			target = ZERO_BB;
			FOREACH_KING(kingEffect(ksq), kingEffect);
			CheckCandidateKingBB[ksq] = target & ~Bitboard(ksq);
		}

	// 以下はBitboardとは関係はないが、Bitboardが初期化されていないと初期化できないので
	// ここから初期化しておいてやる。

	// 10. LONG_EFFECT_LIBRARYの初期化

#if defined (LONG_EFFECT_LIBRARY)
	LongEffect::init();
#endif

	// 11. 1手詰めテーブルの初期化
#if defined (USE_MATE_1PLY)
	Mate::init();
#endif

}
