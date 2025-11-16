#include "tt.h"
#include "misc.h"

// グローバル置換表
TranspositionTable TT;

// ■ TTEntryのメソッド実装（ヘッダーファイルに移動したもの以外）

Move TTEntry::move() const {
    return move_from16(move16);
}

Value TTEntry::value() const {
    return Value(value16);
}

Value TTEntry::eval() const {
    return Value(eval16);
}

Depth TTEntry::depth() const {
    return Depth(depth8 & 0x3f);
}

Bound TTEntry::bound() const {
    return Bound((genBound8 & 0xc0) >> 6);
}

bool TTEntry::is_pv() const {
    return (genBound8 & 0x80) != 0;
}

// ■ TranspositionTableコンストラクタの解説
//
// 置換表を初期化する。メモリ確保はresize()に任せる。
// 世代カウンターを0で初期化（最初の探索セッションを意味する）
TranspositionTable::TranspositionTable() : generation8(0) {}

TranspositionTable::~TranspositionTable() {
    if (table) {
        aligned_free(table);
        table = nullptr;
    }
}

// ■ probe()メソッドの解説
//
// 置換表から指定された局面を検索する最も重要な関数。
// ハッシュキーを基に対応するクラスタを特定し、その中のエントリを検索。
//
// 【検索アルゴリズム】
// 1. keyの上位32bitでクラスタを特定
// 2. クラスタ内の3エントリを先頭から順に検索
// 3. ハッシュ一致かつ未使用のエントリがあればヒットとみなす
// 4. 見つからない場合は最初のエントリを書き込み用として返す
//
// 【返り値の意味】
// bool: ヒットしたかどうか
// TTData: ヒットした場合のデータ（未使用ならダミーデータ）
// TTWriter: この局面用の書き込みオブジェクト
//
std::tuple<bool, TTData, TTWriter> TranspositionTable::probe(const Key key) const {
    // テーブルが未確保の場合は未ヒットで返す
    if (!table) {
        return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false, 0), TTWriter(nullptr));
    }

    // ハッシュキーの上位32bitで比較対象とする
    TTEntry* tte = first_entry(key);
    uint32_t key32 = uint32_t(key >> 32);

    // クラスタ内の3エントリを線形検索
    for (int i = 0; i < TT_ENTRY_NB; ++i, ++tte) {
        // ハッシュキーが一致し、かつエントリが使用中ならヒット
        if (tte->key32 == key32 && !tte->empty()) {
            // ヒットした場合：データコピーと書き込み用オブジェクトを返す
            return std::make_tuple(true, tte->get_data(), TTWriter(tte));
        }
    }

    // 未ヒットの場合：最適な書き込み先エントリを選択して返す
    // 本家やねうら王のエントリ選択戦略を実装
    tte = first_entry(key);
    TTEntry* replace = tte;

    for (int i = 0; i < TT_ENTRY_NB; ++i, ++tte) {
        // 1. 空のエントリを最優先
        if (tte->empty()) {
            replace = tte;
            break;
        }

        // 2. 古い世代のエントリを次に優先
        if ((generation8 - tte->generation()) >= 64) {
            replace = tte;
            break;
        }

        // 3. より浅い深さのエントリを選択
        if (tte->depth() < replace->depth()) {
            replace = tte;
        }
    }

    // 未ヒット：ダミーデータと選択したエントリの書き込み権を返す
    return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false, 0), TTWriter(replace));
}