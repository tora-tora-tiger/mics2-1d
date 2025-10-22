#include "search.h"
#include "usi.h"
#include "evaluate.h"


int main(int argc, char *argv[]) {
  // --- 全体的な初期化
  Bitboards::init();
  Position::init();
  Search::init();
  Eval::init();

  // USIコマンドの応答部
  USI::loop(argc, argv);
}
