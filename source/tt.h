#ifndef _TT_H_
#define _TT_H_

#include "types.h"
#include <cstdint>

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
    Move   move;    // この局面での最善手
    Value  value;   // この局面での探索結果の評価値
    Value  eval;    // この局面での静的評価値（評価関数の直接値）
    Depth  depth;   // この値を得た時の探索深さ
    Bound  bound;   // 値の性質：上界/下界/正確値
    bool   is_pv;   // このエントリがPV nodeから得たか

    // デフォルトコンストラクタを禁止（明示的な初期化を強制）
    TTData() = delete;

    // コンストラクタ：各値を明示的に設定
    TTData(Move m, Value v, Value ev, Depth d, Bound b, bool pv) :
        move(m),      // 最善手
        value(v),    // 探索値
        eval(ev),    // 静的評価値
        depth(d),    // 探索深さ
        bound(b),    // 値の性質
        is_pv(pv)   // PV nodeフラグ
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
    void write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);

    // デフォルトコンストラクタ：未使用状態を示す
    TTWriter() : entry(nullptr) {}

    // コピー代入演算子：他のWriterから状態を引き継ぐ
    TTWriter& operator=(const TTWriter& other) {
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
    uint8_t generation() const;

    // --- 操作メソッド群 ---

    // 指定されたデータをこのエントリに保存する
    // 引数：ハッシュ上位32bit, 探索値, PVフラグ, Bound, 深さ, 指し手, 評価値, 世代
    void save(uint32_t k32, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t g8);

    // このエントリが未使用かどうかを判定
    // depth8が0なら空とみなす
    bool empty() const;

    // 指定された64bitキーがこのエントリに一致するか
    // 実際には上位32bitのみを比較する
    bool matches(Key k) const;

    // このエントリの全データをTTData構造体として返す
    // 読み取り専用として安全なデータアクセスを提供する
    TTData get_data() const;
};

// ■ Cluster構造体の解説
//
// クラスターはハッシュ衝突に対応するための仕組み。
// 同じハッシュ値を持つ複数の局面情報を同じクラスタ内に保存する。
//
// 【クラスタサイズの設計思想】
// ・5五将棋は25升なので通常将棋より局面数が少ない
// ・ハッシュ衝突の確率も低いため、3エントリで十分
// ・メモリ効率と衝突率のバランスを重視
//
// 【エントリの選択戦略】
// ・probe時は0番目から順に検索
// ・保存時は最も古い/浅いエントリを上書き
//
// クラスター（ハッシュ衝突対応のための複数エントリ容器）
struct Cluster {
    TTEntry entry[3];  // 3エントリ：5五将棋に最適化されたクラスタサイズ
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
    void resize(size_t mbSize);

    // 置換表をクリア
    void clear();

    // 置換表の使用率を1000分率で返す
    int hashfull() const;

    // 新しい探索ごとに呼び出す（世代カウンターを更新）
    void new_search();

    // 現在の世代
    uint8_t generation() const;

    // 指定されたkeyで置換表を検索
    // 返り値: (見つかったか, データ, ライター)
    std::tuple<bool, TTData, TTWriter> probe(const Key key) const;

    // 指定されたkeyに対応するクラスターの先頭エントリを返す
    TTEntry* first_entry(const Key key) const;

private:
    // クラスター数
    size_t clusterCount = 0;

    // 確保されているクラスターの先頭
    Cluster* table = nullptr;

    // 世代カウンター（8で割った余り）
    uint8_t generation8;
};

// グローバル置換表
extern TranspositionTable TT;

#endif // _TT_H_