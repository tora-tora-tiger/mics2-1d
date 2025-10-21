#include "evaluate.h"

namespace Eval {

// 利き評価テーブルの定義
// [先手玉のマス][後手玉のマス][対象駒][そのマスの先手の利きの数(max2)][そのマスの後手の利きの数(max2)][駒(先後区別あり)]
int16_t KKPEE[SQ_NB][SQ_NB][SQ_NB][3][3][PIECE_NB];
void init() {
  // 利きが1つの升にm個ある時に、our_effect_value(their_effect_value)の価値は何倍されるのか？
  // 利きは最大で10個のことがある。格納されている値は1024を1.0とみなす固定小数。
  // optimizerの答えは、{ 0 , 1024/* == 1.0 */ , 1800, 2300 , 2900,3500,3900,4300,4650,5000,5300 }
  //   6365 - pow(0.8525,m-1)*5341 　みたいな感じ？
  int multi_effect_value[11];

  // 利きがm個ある時に、our_effect_value(their_effect_value)の価値は何倍されるのか？
  // 利きは最大で10個のことがある。
  for (int m = 0; m < 11; ++m)
  multi_effect_value[m] = m == 0 ? 0 : int(6365 - std::pow(0.8525, m - 1) * 5341);
  
  // 利きを評価するテーブル
  //    [自玉の位置][対象となる升][利きの数(0～10)]
  double our_effect_table  [SQ_NB][SQ_NB][3];
  double their_effect_table[SQ_NB][SQ_NB][3];
  
  for(auto king_sq : SQ)
    for (auto sq : SQ)
      for(int m = 0 ; m <= 2 ; ++m) {// 利きの数
        // 筋と段でたくさん離れているほうの数をその距離とする。
        int d = dist(sq, king_sq);
        
        our_effect_table  [king_sq][sq][m] = double(multi_effect_value[m] * our_effect_value  [d]) / (1024 * 1024);
        their_effect_table[king_sq][sq][m] = double(multi_effect_value[m] * their_effect_value[d]) / (1024 * 1024);
      }
  
  // ある升の利きの価値のテーブルの初期化
  for (auto king_black : SQ)
    for (auto king_white : SQ)
      for (auto sq : SQ)
        for(int m1 = 0 ; m1<=2 ; ++m1) // 先手の利きの数
          for (int m2 = 0 ; m2 <=2 ; ++m2) // 後手の利きの数
            for (Piece pc = NO_PIECE ; pc < PIECE_NB ; ++pc) { // 対象駒(先後区別あり)
              int score = 0;
              score += our_effect_table  [    king_black ][    sq ][m1];
              score -= their_effect_table[    king_black ][    sq ][m2];
              score -= our_effect_table  [Inv(king_white)][Inv(sq)][m2];
              score += their_effect_table[Inv(king_white)][Inv(sq)][m1];

              if(pc != NO_PIECE) {
                // 盤上の駒に対しては、その価値を1/10ほど減ずる。
                auto piece_value = PieceValue[pc];
                score -= piece_value * 104 / 1024;
              }

              KKPEE[king_black][king_white][sq][m1][m2][pc] = int16_t(score);
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
    // 25ループ
    for (Square sq : SQ) {
      // 盤上の駒の評価
      score += PieceValue[pos.piece_on(sq)];
    // 利きの評価
    // enum Square: int32_t
    // std::cout << "pos.attackers_to(sq, BLACK): " << pos.attackers_to(sq, BLACK) << std::endl;
    score += KKPEE[pos.king_square(BLACK)]
                  [pos.king_square(WHITE)]
                  [sq]
                  [std::min(2, (int32_t)pos.attackers_to(BLACK, sq).pop_count())]
                  [std::min(2, (int32_t)pos.attackers_to(WHITE, sq).pop_count())]
                  [pos.piece_on(sq)];
  }
    
    // 手駒の評価
    // 2 * 5 = 10ループ
    for (Color c : COLOR) {
      const Hand &hand = pos.hand_of(c);
      if (hand == HAND_ZERO)
        continue;
      for (Piece pc : {PAWN, SILVER, BISHOP, ROOK, GOLD}) {
        // 手駒の枚数を取得
        int cnt = hand_count(hand, pc);
        score += (c == BLACK ? 1 : -1) * Value(cnt * (HavingPieceValue[pc]));
      }
    }
  }
  
  // 手番側から見た評価値を返す
  return pos.side_to_move() == BLACK ? score : -score;
}
  
} // namespace Eval
