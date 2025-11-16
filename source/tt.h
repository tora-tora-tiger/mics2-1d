#ifndef _TT_H_
#define _TT_H_

#include "types.h"
#include "misc.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <algorithm>

// genBound8には大部分の詳細が含まれています。
// 次の定数を使用して、5ビットの先頭世代ビットと3ビットの末尾のその他のビットを操作します。
// これらのビットは他の用途のために予約されています。
// ⇨ generation8の下位↓bitは、generation用ではなく、別の情報を格納するのに用いる。
//   (PV nodeかどうかのフラグとBoundに用いている。)

static constexpr unsigned GENERATION_BITS = 3;

// increment for generation field
// 世代フィールドをインクリメント
// ⇨ 次のgenerationにするために加算する定数。2の↑乗。

static constexpr int GENERATION_DELTA = (1 << GENERATION_BITS);

// cycle length
// サイクル長
// ⇨ generationを加算していき、1周して戻ってくるまでの長さ。

static constexpr int GENERATION_CYCLE = 255 + GENERATION_DELTA;

// mask to pull out generation number
// TTEntryから世代番号を抽出するためのマスク

static constexpr int GENERATION_MASK = (0xFF << GENERATION_BITS) & 0xFF;

// Move圧縮関数の宣言
static inline uint16_t move_to16(Move m);
static inline Move move_from16(uint16_t m16);

// 置換表エントリ数
#define TT_ENTRY_NB 5

// ■ 置換表（Transposition Table）の解説
//
// 置換表とは、一度探索した局面の結果を保存しておき、
// 同じ局面に再度遭遇した時に再利用するためのデータ構造である。
//
// 【利点】
// 1. 同じ局面の再探索を防ぎ、探索ノード数を大幅に削減できる
// 2. Alpha-betaカットがより効率的になり、より深い探索が可能になる
// 3. 最善手の情報を保持できるため、探索順序の最適化に繋がる
//
// 【構成要素】
// ・TTEntry: 1つの局面情報を格納（16bytesに圧縮）
// ・Cluster: 複数のTTEntryをまとめたもの（ハッシュ衝突対応）
// ・TranspositionTable: 全体を管理するクラス
//
// 【やねうら王独自拡張】
// ・PV nodeフラグの保存（Principal Variation）
// ・手駒の優等/劣等区別への対応準備
// ・動的な世代管理によるエントリの有効期限制御

// 置換表エントリに格納するデータ構造体
// 読み取り専用で、TTEntryから取得したデータを保持する
struct TTData {
    Move   move;       // この局面での最善手
    Value  value;      // この局面での探索結果の評価値
    Value  eval;       // この局面での静的評価値（評価関数の直接値）
    Depth  depth;      // この値を得た時の探索深さ
    Bound  bound;      // 値の性質：上界/下界/正確値
    bool   is_pv;      // このエントリがPV nodeから得たか
    uint8_t generation; // このエントリが保存された世代

    // デフォルトコンストラクタを禁止（明示的な初期化を強制）
    TTData() = delete;

    // コンストラクタ：各値を明示的に設定
    TTData(Move m, Value v, Value ev, Depth d, Bound b, bool pv, uint8_t g) :
        move(m),      // 最善手
        value(v),    // 探索値
        eval(ev),    // 静的評価値
        depth(d),    // 探索深さ
        bound(b),    // 値の性質
        is_pv(pv),   // PV nodeフラグ
        generation(g) // 世代
    {
    }
};

// ■ TTWriterクラスの解説
//
// TTWriterは置換表への安全な書き込みを保証するためのクラス。
// probe()関数が返すライターオブジェクトで、これを通してのみ
// 置換表のエントリを更新できるようになっている。
//
// 【設計思想】
// ・直接TTEntryを操作させず、Writerパターンを強制する
// ・マルチスレッド環境での競合を防ぐ
// ・書き込みの整合性を保証する
//
// 【使い方】
// auto tt_result = TT.probe(key);
// TTWriter writer = std::get<2>(tt_result);
// writer.write(key, value, pv, bound, depth, move, eval, gen);
//
// 置換表への書き込み用クラス
class TTWriter {
public:
    // 指定されたパラメータでTTEntryを更新する
    inline void write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);

    // デフォルトコンストラクタ：未使用状態を示す
    TTWriter() : entry(nullptr) {}

    // コピー代入演算子：他のWriterから状態を引き継ぐ
    inline TTWriter& operator=(const TTWriter& other) {
        entry = other.entry;
        return *this;
    }

private:
    // TranspositionTableのみがTTWriterを生成できる（friend指定）
    friend class TranspositionTable;

    // 更新対象のTTEntryを指すポインタ
    struct TTEntry* entry;

    // コンストラクタ：TranspositionTableのみから呼び出される
    TTWriter(struct TTEntry* tte) : entry(tte) {}
};

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
static inline uint16_t move_to16(Move m) {
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

static inline Move move_from16(uint16_t m16) {
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

// ■ TTEntry構造体の解説
//
// 置換表の個々のエントリを表現する構造体。
// 16バイトに圧縮してメモリ効率を最大限に高めている。
//
// 【メモリレイアウト（16bytes合計）
// key32        : 4bytes - 局面ハッシュの上位32bit
// move16       : 2bytes - 圧縮された最善手
// value16      : 2bytes - 探索結果の評価値
// eval16       : 2bytes - 静的評価値
// depth8       : 1bytes - 探索深さ（0-63）
// genBound8    : 1bytes - 世代(7bit) + PVフラグ(1bit)
//
// 【圧縮技術】
// ・Moveの16bit圧縮：from(5bit) + to(5bit) + 成り(1bit) + 打ち(1bit) + 駒種(3bit)
// ・Depthの6bit圧縮：5五将棋では深さ63で十分
// ・世代管理：7bitで128世代まで管理可能
//
struct TTEntry {
    // 【ハッシュキー：4bytes】
    // 64bitハッシュキーの上位32bitのみを保存。
    // 下位32bitはクラスタインデックスとして使用するため省略可能。
    uint32_t key32;

    // 【最善手：2bytes】
    // 16bitに圧縮された指し手情報。
    // 詳細はmove_to16()/move_from16()関数を参照。
    uint16_t move16;

    // 【探索値：2bytes】
    // Alpha-beta探索で得た評価値。
    int16_t value16;

    // 【静的評価値：2bytes】
    // 評価関数の直接の値。
    // 探索値との比較で評価の変動を検出可能。
    int16_t eval16;

    // 【探索深さ：1byte】
    // bit 0-5: depth (0-63) - この値を得た時の深さ
    uint8_t depth8;

    // 【世代とBound：1byte】
    // bit 0-6: generation (0-127) - 新しさ世代マーク
    // bit 7:   pv flag (1) - PV nodeからのものか
    uint8_t genBound8;

    // --- アクセスメソッド群 ---

    // 16bit圧縮されたMoveを復元して返す
    Move move() const;

    // 保存されている探索値をValue型に変換して返す
    Value value() const;

    // 保存されている静的評価値を返す
    Value eval() const;

    // 保存されている深さをDepth型に変換して返す
    Depth depth() const;

    // 保存されているBoundを返す
    // BOUND_NONE/BOUND_UPPER/BOUND_LOWER/BOUND_EXACT
    Bound bound() const;

    // このエントリがPV node（最適解の候補）から得たか
    bool is_pv() const;

    // このエントリの世代番号を返す（0-127）
    inline uint8_t generation() const;

    // 相対的なエイジを計算（やねうら王の実装からコピー）
    inline uint8_t relative_age(const uint8_t g8) const;

    // --- 操作メソッド群 ---

    // 指定されたデータをこのエントリに保存する
    // 引数：ハッシュ上位32bit, 探索値, PVフラグ, Bound, 深さ, 指し手, 評価値, 世代
    inline void save(uint32_t k32, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t g8);

    // このエントリが未使用かどうかを判定
    // depth8が0なら空とみなす
    inline bool empty() const;

    // 指定された64bitキーがこのエントリに一致するか
    // 実際には上位32bitのみを比較する
    inline bool matches(Key k) const;

    // このエントリの全データをTTData構造体として返す
    // 読み取り専用として安全なデータアクセスを提供する
    inline TTData get_data() const;
};

// ■ Cluster構造体の解説
//
// クラスターはハッシュ衝突に対応するための仕組み。
// 同じハッシュ値を持つ複数の局面情報を同じクラスタ内に保存する。
//
// 【クラスタサイズの設計思想】
// ・5五将棋は25升なので通常将棋より局面数が少ない
// ・ハッシュ衝突の確率も低いため、3エントリで十分(？)
// ・メモリ効率と衝突率のバランスを重視
//
// 【エントリの選択戦略】
// ・probe時は0番目から順に検索
// ・保存時は最も古い/浅いエントリを上書き
//
// クラスター（ハッシュ衝突対応のための複数エントリ容器）
struct Cluster {
    TTEntry entry[TT_ENTRY_NB];  // 3エントリ：5五将棋に最適化されたクラスタサイズ
};

// ■ TranspositionTableクラスの解説
//
// 置換表の本体を管理するクラス。以下の機能を持つ。
//
// 【主要機能】
// 1. probe(): 指定局面の検索と書き込み用エントリ取得
// 2. resize(): メモリサイズの動的変更[MB単位]
// 3. clear(): 全エントリの初期化
// 4. new_search(): 世代カウンターの更新
// 5. hashfull(): 置換表使用率の算出
//
// 【メモリ管理】
// ・aligned_mallocでキャッシュライン境界に合わせて確保
// ・Cluster配列として連続的なメモリレイアウトを実現
// ・クラスタ数の計算: (MB * 1024 * 1024) / sizeof(Cluster)
//
// 【世代管理】
// ・new_search()ごとに世代を1進める
// ・古い世代のエントリは優先的に上書きされる
// ・世代0-127のループで管理（7bit分）
//
// 置換表本体
class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    // 置換表のサイズを変更する[MB単位]
    inline void resize(size_t mbSize);

    // 置換表をクリア
    inline void clear();

    // 置換表の使用率を1000分率で返す
    inline int hashfull() const;

    // 新しい探索ごとに呼び出す（世代カウンターを更新）
    inline void new_search();

    // 現在の世代
    inline uint8_t generation() const;

    // 指定されたkeyで置換表を検索
    // 返り値: (見つかったか, データ, ライター)
    std::tuple<bool, TTData, TTWriter> probe(const Key key) const;

    // 指定されたkeyに対応するクラスターの先頭エントリを返す
    inline TTEntry* first_entry(const Key key) const;

private:
    // クラスター数
    size_t clusterCount = 0;

    // 確保されているクラスターの先頭
    Cluster* table = nullptr;

    // 世代カウンター（8で割った余り）
    uint8_t generation8;

    u32 count = 0;
};

// TranspositionTableのinlineメソッド実装
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
    // 先頭1000エントリーのみを使ったフェルミ推計
    const int sample_size = std::min(1000, (int)clusterCount);

    for (int i = 0; i < sample_size; ++i) {
        for (int j = 0; j < TT_ENTRY_NB; ++j) {
            // 空でないエントリをすべてカウント（世代に関係なく）
            if (!table[i].entry[j].empty())
                count++;
        }
    }

    return count * 1000 / (sample_size * TT_ENTRY_NB);
}

void TranspositionTable::new_search() {
    generation8 = (generation8 + 1) & 0x7f;
}

uint8_t TranspositionTable::generation() const {
    return generation8;
}

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

// TTEntryのinlineメソッド実装
uint8_t TTEntry::generation() const {
    return genBound8 & 0x7f;
}

uint8_t TTEntry::relative_age(const uint8_t generation8) const {
    // 世代のパックされた保存形式とその循環的な性質により、
	// 世代エイジを正しく計算するために、GENERATION_CYCLEを加えます
	// （256がモジュロとなり、関係のない下位nビットが
	// 結果に影響を与えないようにするために必要な値も加えます）。
	// これにより、generation8が次のサイクルにオーバーフローした後でも、
	// エントリのエイジを正しく計算できます。

	// ■ 補足情報
	//
	// generationは256になるとオーバーフローして0になるのでそれをうまく処理できなければならない。
	// a,bが8bitであるとき ( 256 + a - b ) & 0xff　のようにすれば、オーバーフローを考慮した引き算が出来る。
	// このテクニックを用いる。
	// いま、
	//   a := generationは下位3bitは用いていないので0。
	//   b := genBound8は下位3bitにはBoundが入っているのでこれはゴミと考える。
	// ( 256 + a - b + c) & 0xfc として c = 7としても結果に影響は及ぼさない、かつ、このゴミを無視した計算が出来る。

	return (GENERATION_CYCLE + generation8 - genBound8) & GENERATION_MASK;
}

bool TTEntry::empty() const {
    return depth8 == 0;
}

bool TTEntry::matches(Key k) const {
    // 上位32bitが一致すれば同じ局面とみなす
    return (key32 == uint32_t(k >> 32));
}

TTData TTEntry::get_data() const {
    return TTData(move(), value(), eval(), depth(), bound(), is_pv(), generation());
}

void TTEntry::save(uint32_t k32, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t g8) {
    // このエントリが空か、古い世代の場合は無条件で上書き
    // BOUND_EXACTか、同じキーで深さが十分に深い場合に上書き
    if (empty() || relative_age(g8) || b == BOUND_EXACT || k32 != key32 ||
        d - DEPTH_ENTRY_OFFSET + 2 * pv > depth8 - 4) {
        key32 = k32;
        move16 = move_to16(m);
        value16 = int16_t(v);
        eval16 = int16_t(ev);
        depth8 = uint8_t(d & 0x3f);
        genBound8 = g8 | ((b & 0x03) << 5) | (pv ? 0x80 : 0);
    }
    // 深さが高くてBOUND_EXACTでないときは、depthを1減らして差別化
    else if (depth8 + DEPTH_ENTRY_OFFSET >= 5 && b != BOUND_EXACT) {
        depth8--;
    }
}

// TTWriterのinlineメソッド実装
void TTWriter::write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8) {
    // debug
    // std::cout << "TT書き込み key=" << std::hex << k << std::dec
    //           << " value=" << v
    //           << " eval=" << ev
    //           << " depth=" << d
    //           << " bound=" << int(b)
    //           << " pv=" << pv
    //           << " move=" << m
    //           << " generation=" << int(generation8)
    //           << std::endl;
    // 単純な書き込み処理：複雑な選択ロジックはprobe()側で実装
    entry->save(uint32_t(k >> 32), v, pv, b, d, m, ev, generation8);
}

// グローバル置換表
extern TranspositionTable TT;

#endif // _TT_H_