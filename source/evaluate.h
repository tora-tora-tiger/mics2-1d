#ifndef _EVALUATE_H_
#define _EVALUATE_H_

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

Value evaluate(const Position &pos);
} // namespace Eval

#endif
