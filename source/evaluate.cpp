#include "evaluate.h"

namespace Eval {
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
    Bitboard effects[] = {pos.attackers_to(sq, BLACK), pos.attackers_to(sq, WHITE)};

    for(auto color : COLOR) {
      auto kingSq = pos.king_square(color);
      if(kingSq == sq) continue;

      int d = dist(kingSq, sq);

      int s1 = effects[color] * our_effect_value[d] / 1024;
      int s2 = effects[~color] * their_effect_value[d] / 1024;

      score += (color == BLACK ? 1 : -1) * Value(s1 - s2);
    }
  }

  // 手番側から見た評価値を返す
  return pos.side_to_move() == BLACK ? score : -score;
}

} // namespace Eval
