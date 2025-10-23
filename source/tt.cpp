#include "tt.h"
#include "misc.h"
#include <cstring>
#include <algorithm>

TranspositionTable TT;

// TTEntryの実装

// ■ Move圧縮関数 move_to16() の解説
//
// 32bitのMove情報を16bitに効率的に圧縮する。
// 5五将棋の盤面特性（25升）を活かして最小のbit数で表現。
//
// 【5五将棋におけるMoveのbit配分】
// bit 15-11: from (0-24) + 1 = 1-25 (5bit)
// bit 10-6 :  to   (0-24) + 1 = 1-25 (5bit)
// bit 5     :  promotion (0-1)           (1bit)
// bit 4     :  is_drop   (0-1)           (1bit)
// bit 3-0   :  piece_type (0-6)          (4bit, 実際は3bit使用）
//
// 【設計思想】
// ・通常の指し手: from(5) + to(5) + promote(1) + 0(1) = 11bit
// ・駒打ち:       0(5) + to(5) + 0(1) + 1(1) + piece(3) = 10bit
// ・合計で最大15bitを使用するので16bitに余裕を持って収まる
//
// 【利点】
// ・メモリ使用量を半減できる（32bit→16bit）
// ・キャッシュ効率が向上する（より多くのエントリがL1キャッシュに収まる）
//
static uint16_t move_to16(Move m) {
    // 特殊な指し手は0として表現
    if (m == MOVE_NONE || m == MOVE_NULL || m == MOVE_RESIGN)
        return 0;

    // 各フィールドを抽出してビットシフトで配置
    uint16_t from = is_drop(m) ? 0 : (move_from(m) + 1);      // 移動元（0は駒打ち用）
    uint16_t to = move_to(m) + 1;                            // 移動先（1-25の範囲）
    uint16_t promote = is_promote(m) ? 1 : 0;                  // 成りフラグ
    uint16_t drop = is_drop(m) ? 1 : 0;                         // 打ち駒フラグ
    uint16_t dropped_piece = drop ? (move_dropped_piece(m) - PAWN + 1) : 0;  // 駒種

    // ビットフィールドを結合して16bitに圧縮
    return (from << 11) | (to << 6) | (promote << 5) | (drop << 4) | dropped_piece;
}

static Move move_from16(uint16_t m16) {
    if (m16 == 0)
        return MOVE_NONE;

    uint16_t from = (m16 >> 11) & 0x1f;
    uint16_t to = (m16 >> 6) & 0x1f;
    uint16_t promote = (m16 >> 5) & 1;
    uint16_t drop = (m16 >> 4) & 1;
    uint16_t dropped_piece = m16 & 0xf;

    if (drop) {
        // 駒打ち
        Piece pt = Piece(dropped_piece - 1 + PAWN);
        return make_move_drop(pt, Square(to - 1));
    } else {
        // 通常の移動
        Square from_sq = Square(from - 1);
        Square to_sq = Square(to - 1);
        if (promote)
            return make_move_promote(from_sq, to_sq);
        else
            return make_move(from_sq, to_sq);
    }
}

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

uint8_t TTEntry::generation() const {
    return genBound8 & 0x7f;
}

void TTEntry::save(uint32_t k32, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t g8) {
    key32 = k32;
    move16 = move_to16(m);
    value16 = int16_t(v);
    eval16 = int16_t(ev);
    depth8 = uint8_t(d & 0x3f);

    // genBound8: bit7=PV flag, bit6-5=bound, bit4-0=generation
    genBound8 = g8 | ((b & 0x03) << 5) | (pv ? 0x80 : 0);
}

bool TTEntry::empty() const {
    return depth8 == 0;
}

bool TTEntry::matches(Key k) const {
    // 上位32bitが一致すれば同じ局面とみなす
    return (key32 == uint32_t(k >> 32));
}

TTData TTEntry::get_data() const {
    return TTData(move(), value(), eval(), depth(), bound(), is_pv());
}

// TTWriterの実装
void TTWriter::write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8) {
    entry->save(uint32_t(k >> 32), v, pv, b, d, m, ev, generation8);
}

// TTWriter::TTWriter(TTEntry* tte) : entry(tte) {}

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

// ■ resize()メソッドの解説
//
// 指定されたサイズ[MB]で置換表を再確保する。
// 5五将棋の特性を考慮したサイズ計算を行う。
//
// 【サイズ計算の詳細】
// 1. MB→バイト変換: mbSize * 1024 * 1024
// 2. クラスタ数計算: total_bytes / sizeof(Cluster)
//    sizeof(Cluster) = 16 * 3 = 48 bytes
//    16MBの場合: 16,777,216 clusters
//
// 【メモリ確保の仕組み】
// ・aligned_mallocで64byte境界に合わせて確保
// ・CPUキャッシュラインとの整合性を確保
// ・確保失敗時のエラー処理を実装
//
void TranspositionTable::resize(size_t mbSize) {
    // 新しいクラスタ数を計算
    size_t newClusterCount = (mbSize * 1024 * 1024) / sizeof(Cluster);

    // 同じサイズなら何もしない（無駄な再確保防止）
    if (newClusterCount == clusterCount)
        return;

    clusterCount = newClusterCount;

    // 既存のテーブルがあれば解放
    if (table) {
        aligned_free(table);
    }

    // 新しいテーブルをアラインドメモリとして確保
    table = (Cluster*)aligned_malloc(sizeof(Cluster) * clusterCount, 64);

    // 確保失敗時のエラー処理
    if (!table) {
        std::cerr << "Failed to allocate transposition table: " << mbSize << " MB" << std::endl;
        clusterCount = 0;
        return;
    }

    // 新しいテーブルをゼロクリア
    clear();
}

void TranspositionTable::clear() {
    if (table) {
        std::memset(table, 0, sizeof(Cluster) * clusterCount);
    }
}

int TranspositionTable::hashfull() const {
    if (!table)
        return 0;

    int count = 0;
    const int sample_size = std::min(1000, (int)clusterCount);

    for (int i = 0; i < sample_size; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (!table[i].entry[j].empty() &&
                table[i].entry[j].generation() == generation8)
                count++;
        }
    }

    return count * 1000 / (sample_size * 3);
}

void TranspositionTable::new_search() {
    generation8 = (generation8 + 1) & 0x7f;
}

uint8_t TranspositionTable::generation() const {
    return generation8;
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
        return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false), TTWriter(nullptr));
    }

    // ハッシュキーの上位32bitで比較対象とする
    TTEntry* tte = first_entry(key);
    uint32_t key32 = uint32_t(key >> 32);

    // クラスタ内の3エントリを線形検索
    for (int i = 0; i < 3; ++i, ++tte) {
        // ハッシュキーが一致し、かつエントリが使用中ならヒット
        if (tte->key32 == key32 && !tte->empty()) {
            // ヒットした場合：データコピーと書き込み用オブジェクトを返す
            return std::make_tuple(true, tte->get_data(), TTWriter(tte));
        }
    }

    // 未ヒットの場合：ダミーデータと最初のエントリの書き込み権を返す
    return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false), TTWriter(first_entry(key)));
}

// ■ first_entry()メソッドの解説
//
// 指定されたハッシュキーに対応するクラスタの先頭エントリを返す。
// この関数が置換表のハッシュ関数としての役割も果たす。
//
// 【ハッシュ戦略】
// ・64bitキーの全体を使わず、下位bitを使用してインデックスを計算
// ・これにより、キーの上位32bitは比較専用として確保できる
// ・剰余演算により、キー値が均等に分散される
//
// 【5五将棋への最適化】
// ・盤面が25升なので、通常将棋よりクラスタ数が少なくて十分
// ・16MBで約350万クラスタが確保可能
//
TTEntry* TranspositionTable::first_entry(const Key key) const {
    // テーブル未確保時はnullptrを返す
    if (!table)
        return nullptr;

    // 剰余演算でクラスタインデックスを計算
    // これにより同じキーは必ず同じクラスタにマッピングされる
    size_t index = size_t(key) % clusterCount;

    // 該当クラスタの先頭エントリへのポインタを返す
    return &table[index].entry[0];
}