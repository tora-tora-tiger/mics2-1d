#ifndef _EVALUATE_H_
#define _EVALUATE_H_

#include <cmath>
#include "position.h"
#include "types.h"

namespace Eval {
// Apery(WCSC26)の駒割り
// 評価値参考： http://minerva.cs.uec.ac.jp/~ito/entcog/contents/lecture/date/5-kakinoki.pdf
enum {
  // 駒の評価値
  PawnValue = 107,
  SilverValue = 810,
  GoldValue = 907,
  BishopValue = 1291,
  RookValue = 1670,
  ProPawnValue = 895,
  ProSilverValue = 933,
  HorseValue = 1985,
  DragonValue = 2408,
  KingValue = 15000,
};

enum {
  // 持ち駒の評価値
  HavingPawnValue = 152,
  HavingSilverValue = 1110,
  HavingGoldValue = 1260,
  HavingBishopValue = 1464,
  HavingRookValue = 1998,
  HavingProPawnValue = 0,
  HavingProSilverValue = 0,
  HavingHorseValue = 0,
  HavingDragonValue = 0,
  HavingKingValue = 0,
};

// 駒の価値のテーブル(後手の駒は負の値)
extern int PieceValue[PIECE_NB];
extern int HavingPieceValue[PIECE_NB];

inline constexpr int our_effect_value[] = {
  68 * 1024 / 1,
  68 * 1024 / 2,
  68 * 1024 / 3,
  68 * 1024 / 4,
  68 * 1024 / 5,
};
inline constexpr int their_effect_value[] = {
  96 * 1024 / 1,
  96 * 1024 / 2,
  96 * 1024 / 3,
  96 * 1024 / 4,
  96 * 1024 / 5,
};

/**
 * 機器の勝ちを合算した値を求めるテーブル
 * [先手玉のマス][後手玉のマス][対象駒][そのマスの先手の利きの数(max2)][そのマスの後手の利きの数(max2)][駒(先後区別あり)]
 * 81*81*81*3*3*size_of(int16_t)*32 = 306MB
 * 1つの升にある利きは、2つ以上の利きは同一視。
 */
// extern int16_t effect_table[SQ_NB][SQ_NB][SQ_NB][11][11];
extern int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];

// ビットが0か1か2以上かを高速に判定する関数
inline int fast_effect_count(const Bitboard &b) {
  return (b ? (((u32)b & (b - 1)) ? 2 : 1) : 0);
}

void init();
Value evaluate(const Position &pos);
} // namespace Eval

#endif
