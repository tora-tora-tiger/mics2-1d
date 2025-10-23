#ifndef _TT_H_
#define _TT_H_

#include "types.h"
#include <cstdint>

// 置換表エントリに格納するデータ
struct TTData {
    Move   move;    // 最善手
    Value  value;   // 評価値
    Value  eval;    // 静的評価値
    Depth  depth;   // 探索深さ
    Bound  bound;   // 上界/下界/正確値
    bool   is_pv;   // PV nodeかどうか

    TTData() = delete;

    TTData(Move m, Value v, Value ev, Depth d, Bound b, bool pv) :
        move(m),
        value(v),
        eval(ev),
        depth(d),
        bound(b),
        is_pv(pv) {
    }
};

// 置換表への書き込み用クラス
class TTWriter {
public:
    void write(Key k, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t generation8);

private:
    friend class TranspositionTable;
    struct TTEntry* entry;
    TTWriter(struct TTEntry* tte);
};

// 置換表エントリ
struct TTEntry {
    // 64bit hash keyの上位32bitを保存
    uint32_t key32;

    // 16bitに圧縮されたMove
    uint16_t move16;

    // int16_tで評価値を保存
    int16_t value16;
    int16_t eval16;

    // uint8_tで深さと世代とBoundを保存
    // bit 0-5: depth (0-63)
    // bit 6-7: bound (0-3)
    uint8_t depth8;

    // bit 0-6: generation (0-127)
    // bit 7:   pv node flag
    uint8_t genBound8;

    // 保存されているMoveを取得
    Move move() const;

    // 保存されているValueを取得
    Value value() const;
    Value eval() const;

    // 保存されているDepthを取得
    Depth depth() const;

    // 保存されているBoundを取得
    Bound bound() const;

    // PV nodeかどうか
    bool is_pv() const;

    // 世代を取得
    uint8_t generation() const;

    // データを保存
    void save(uint32_t k32, Value v, bool pv, Bound b, Depth d, Move m, Value ev, uint8_t g8);

    // エントリが空かどうか（深さが0なら空とみなす）
    bool empty() const;

    // このエントリが指定されたkeyに一致するか
    bool matches(Key k) const;

    // TTData構造体としてデータを取得
    TTData get_data() const;
};

// クラスター（複数のTTEntryをまとめたもの）
struct Cluster {
    TTEntry entry[3];  // 5五将棋では3エントリで十分
};

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