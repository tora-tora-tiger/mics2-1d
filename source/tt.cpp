#include "tt.h"
#include "misc.h"
#include <cstring>
#include <algorithm>

TranspositionTable TT;

// TTEntryの実装

// Moveを16bitに圧縮/展開する関数
static uint16_t move_to16(Move m) {
    if (m == MOVE_NONE || m == MOVE_NULL || m == MOVE_RESIGN)
        return 0;

    // 5五将棋の場合、Squareは0-24なので5bitで表現できる
    // from: 5bit, to: 5bit, promotion: 1bit, drop: 1bit, dropped_piece: 3bit = 15bit
    uint16_t from = is_drop(m) ? 0 : (move_from(m) + 1);
    uint16_t to = move_to(m) + 1;
    uint16_t promote = is_promote(m) ? 1 : 0;
    uint16_t drop = is_drop(m) ? 1 : 0;
    uint16_t dropped_piece = drop ? (move_dropped_piece(m) - PAWN + 1) : 0;

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

// TranspositionTableの実装
TranspositionTable::TranspositionTable() : generation8(0) {}

TranspositionTable::~TranspositionTable() {
    if (table) {
        aligned_free(table);
        table = nullptr;
    }
}

void TranspositionTable::resize(size_t mbSize) {
    size_t newClusterCount = (mbSize * 1024 * 1024) / sizeof(Cluster);

    if (newClusterCount == clusterCount)
        return;

    clusterCount = newClusterCount;

    if (table) {
        aligned_free(table);
    }

    table = (Cluster*)aligned_malloc(sizeof(Cluster) * clusterCount, 64);

    if (!table) {
        std::cerr << "Failed to allocate transposition table: " << mbSize << " MB" << std::endl;
        clusterCount = 0;
        return;
    }

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

std::tuple<bool, TTData, TTWriter> TranspositionTable::probe(const Key key) const {
    if (!table) {
        return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false), TTWriter(nullptr));
    }

    TTEntry* tte = first_entry(key);
    uint32_t key32 = uint32_t(key >> 32);

    // 3つのエントリを検索
    for (int i = 0; i < 3; ++i, ++tte) {
        if (tte->key32 == key32 && !tte->empty()) {
            // 見つかった
            return std::make_tuple(true, tte->get_data(), TTWriter(tte));
        }
    }

    // 見つからなかった
    return std::make_tuple(false, TTData(MOVE_NONE, VALUE_ZERO, VALUE_ZERO, DEPTH_ENTRY_OFFSET, BOUND_NONE, false), TTWriter(first_entry(key)));
}

TTEntry* TranspositionTable::first_entry(const Key key) const {
    if (!table)
        return nullptr;

    // keyをclusterCountで割ってインデックスを計算
    size_t index = size_t(key) % clusterCount;
    return &table[index].entry[0];
}