#include "evaluate.h"

namespace Eval {

// 利き評価テーブルの定義
// [先手玉のマス][後手玉のマス][対象駒][そのマスの先手の利きの数][そのマスの後手の利きの数]
int16_t effect_table[SQ_NB][SQ_NB][SQ_NB][11][11];
void init() {
  //int16_t effect_table[先手玉のマス][後手玉のマス][対象駒][そのマスの先手の利きの数][そのマスの後手の利きの数]
  int multi_effect_value[11];

  // 利きがm個ある時に、our_effect_value(their_effect_value)の価値は何倍されるのか？
  // 利きは最大で10個のことがある。
  for (int m = 0; m < 11; ++m)
  multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);
  
  // 利きを評価するテーブル
  //    [自玉の位置][対象となる升][利きの数(0～10)]
  int our_effect_table  [SQ_NB][SQ_NB][11];
  int their_effect_table[SQ_NB][SQ_NB][11];
  
  for(auto king_sq : SQ)
    for (auto sq : SQ)
      for(int m = 0 ; m < 11 ; ++m) {// 利きの数
        // 筋と段でたくさん離れているほうの数をその距離とする。
        int d = dist(sq, king_sq);
        
        our_effect_table  [king_sq][sq][m] = multi_effect_value[m] * our_effect_value  [d] / (1024 * 1024);
        their_effect_table[king_sq][sq][m] = multi_effect_value[m] * their_effect_value[d] / (1024 * 1024);
      }
  
  // ある升の利きの価値のテーブルの初期化
  for (auto king_black : SQ)
    for (auto king_white : SQ)
      for (auto sq : SQ)
        for(int m1 = 0; m1<11;++m1) // 先手の利きの数
          for (int m2 = 0; m2 < 11; ++m2) // 後手の利きの数
          {
            int score = 0;
            score += our_effect_table  [    king_black ][    sq ][m1];
            score -= their_effect_table[    king_black ][    sq ][m2];
            score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
            score += their_effect_table[Inv(king_white)][Inv(sq)][m1];
            effect_table[king_black][king_white][sq][m1][m2] = int16_t(score);
          }
  
}
int PieceValue[PIECE_NB] = {
  0,
  PawnValue,
  0,
  0,
  SilverValue,
  BishopValue,
  RookValue,
  GoldValue,
  KingValue,
  ProPawnValue,
  0,
  0,
  ProSilverValue,
  HorseValue,
  DragonValue,
  0,
  
  0,
  -PawnValue,
  0,
  0,
  -SilverValue,
  -BishopValue,
  -RookValue,
  -GoldValue,
  -KingValue,
  -ProPawnValue,
  0,
  0,
  -ProSilverValue,
  -HorseValue,
  -DragonValue,
  0,
};

int HavingPieceValue[PIECE_NB] = {
  0,
  HavingPawnValue,
  0,
  0,
  HavingSilverValue,
  HavingBishopValue,
  HavingRookValue,
  HavingGoldValue,
  HavingKingValue,
  HavingProPawnValue,
  0,
  0,
  HavingProSilverValue,
  HavingHorseValue,
  HavingDragonValue,
  0,
  
  0,
  -HavingPawnValue,
  0,
  0,
  -HavingSilverValue,
  -HavingBishopValue,
  -HavingRookValue,
  -HavingGoldValue,
  -HavingKingValue,
  -HavingProPawnValue,
  0,
  0,
  -HavingProSilverValue,
  -HavingHorseValue,
  -HavingDragonValue,
  0,
};

Value evaluate(const Position &pos) {
  Value score = VALUE_ZERO;
  
  // material value
  {
    // 盤上の駒の評価
    for (Square sq : SQ)
    score += PieceValue[pos.piece_on(sq)];
    
    // 手駒の評価
    for (Color c : COLOR)
    for (Piece pc : {PAWN, SILVER, BISHOP, ROOK, GOLD}) {
      // 手駒の枚数を取得
      int cnt = hand_count(pos.hand_of(c), pc);
      score += (c == BLACK ? 1 : -1) * Value(cnt * (HavingPieceValue[pc]));
    }
  }
  
  // 利きの評価
  for(auto sq: SQ) {
    score += effect_table[pos.king_square(BLACK)][pos.king_square(WHITE)][sq][pos.attackers_to(sq, BLACK)][pos.attackers_to(sq, WHITE)];
  }
  
  // 手番側から見た評価値を返す
  return pos.side_to_move() == BLACK ? score : -score;
}
  
} // namespace Eval
