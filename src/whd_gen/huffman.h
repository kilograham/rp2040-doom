/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <map>
#include <cstdint>
#include <queue>
#include <memory>
#include <iostream>

template<typename S, typename C = std::less<S>, int N = 15> struct huffman_params {
    using symbol_type = S;
    using comparator_type = C;
    static const int max_code_size = N;
};

template<typename S, typename H = huffman_params<S>> struct huffman_encoding;

template<typename S> struct symbol_stats {
    void add(S symbol, uint32_t count = 1) {
        symbol_counts[symbol] += count;
        total += count;
    }
    uint32_t get_total() { return total; }
    template<typename H> huffman_encoding<S,H> create_huffman_encoding() {
        return create_huffman_encoding(H());
    }
    template<typename H> huffman_encoding<S,H> create_huffman_encoding(const H& params);
    std::map<S, uint32_t> symbol_counts;
    uint32_t total = 0;
};

// bit sequences are little endian
struct bit_sequence : public std::pair<uint32_t, uint> {
    bit_sequence() : pair(0,0) {}
    bit_sequence(uint32_t bits, int length) : pair(bits, length) {
        assert(length <= 32);
        assert(length == 32 || bits < (1u << length));
    }
    static bit_sequence for_byte(uint8_t byte) {
        return bit_sequence(byte, 8);
    }

    bit_sequence append(bool bit) const {
        assert(second < 32);
        return bit_sequence(first | (bit * (1u << second)), second + 1);
    }

    uint length() const { return second; }
    uint32_t bits() const { return first; }
    bool bit(int pos) const { return first & (1u << pos); }
    friend std::ostream& operator <<(std::ostream& out, const bit_sequence &bs) {
        if (bs.length()) {
            for(uint bit = 0; bit < bs.length(); bit++) {
                out << ((bs.first & (1u << bit)) ? '1' : '0');
            }
        }
        return out;
    }
};

template <typename S> struct symbol_adapter {
    using ostream_type = S;
};

template<> struct symbol_adapter<int8_t> {
    using ostream_type = int;
};

template<> struct symbol_adapter<uint8_t> {
    using ostream_type = int;
};

static inline uint32_t __rev(uint32_t v) {
    v = ((v & 0x55555555u) << 1u) | ((v >> 1u) & 0x55555555u);
    v = ((v & 0x33333333u) << 2u) | ((v >> 2u) & 0x33333333u);
    v = ((v & 0x0f0f0f0fu) << 4u) | ((v >> 4u) & 0x0f0f0f0fu);
    return (v << 24u) | ((v & 0xff00u) << 8u) | ((v >> 8u) & 0xff00u) | (v >> 24u);
}

template<typename S, typename H> struct huffman_encoding {
    struct node;
    using node_ptr = std::shared_ptr<node>;
    static const int CODE_MAX = 32; // longest we expect to see

    huffman_encoding() = default;

    struct node {
        node() :symbol_index(-1), count(0) {}
        node(int symbol_index, uint32_t count) : symbol_index(symbol_index), count(count) {}
        node(const node_ptr& n1, const node_ptr& n2) : symbol_index(-1), count(n1->count + n2->count), zero(n1), one(n2) {}
        int symbol_index;
        uint32_t count;
        node_ptr zero, one;
    };

    struct node_comparator {
        bool operator()(const node_ptr& a, const node_ptr &b) {
            return a->count > b->count;
        }
    };

    static huffman_encoding get(const symbol_stats<S> &stats) {
        return huffman_encoding<S>(stats);
    }

    explicit huffman_encoding(const symbol_stats<S> &stats, const H& params) : symbol_encodings() , stats(stats) {
        initted = true;
        std::priority_queue<node_ptr,std::vector<node_ptr>,node_comparator> node_queue;
        for(const auto &e : stats.symbol_counts) {
            if (e.second) {
                node_queue.push(std::make_shared<node>((int)symbols.size(), e.second));
                symbols.push_back(e.first);
            }
        }
        while (node_queue.size() > 1) {
            auto n1 = node_queue.top();
            node_queue.pop();
            auto n2 = node_queue.top();
            node_queue.pop();
            node_queue.push(std::make_shared<node>(n1,n2));
        }
        if (node_queue.empty()) {
            root = nullptr;
        } else {
            // we need to assigned in symbol comparator order rather than tree traversal order, so we assign the codes
            // according to length in comparator order, then rebuild the tree to match (to maintain the ability to
            // decode)
            std::vector<std::pair<int,uint>> symbol_indexes_and_lengths;
            std::vector<int> length_counts;
            std::function<void(const node_ptr& node, int)> classify_by_length = [&](const node_ptr& node, uint length) {
                if (node->symbol_index < 0) {
                    assert(node->one && node->zero);
                    classify_by_length(node->zero, length + 1);
                    classify_by_length(node->one, length + 1);
                } else {
                    symbol_indexes_and_lengths.template emplace_back(node->symbol_index, length);
                    if (length >= length_counts.size()) length_counts.resize(length+1);
                    length_counts[length]++;
                }
            };
            classify_by_length(node_queue.top(), 0);
            auto comparator = typename H::comparator_type();
            auto sorter = [&](const auto &a, const auto &b) {
                if (a.second < b.second) return true;
                if (a.second > b.second) return false;
                return comparator(symbols[a.first], symbols[b.first]);
            };
            std::sort(symbol_indexes_and_lengths.begin(), symbol_indexes_and_lengths.end(), sorter);

            assert (symbol_indexes_and_lengths.size() < (1u << H::max_code_size));
            if (limit_code_size(length_counts, H::max_code_size)) {
                // regenerate the lengths
                uint pos = 0;
                for(uint l=0;l<length_counts.size();l++) {
                    for(int i=0;i<length_counts[l];i++) {
                        assert(pos < symbol_indexes_and_lengths.size());
                        symbol_indexes_and_lengths[pos++].second = l;
                    }
                }
                std::sort(symbol_indexes_and_lengths.begin(), symbol_indexes_and_lengths.end(), sorter);
            }
            root = std::make_shared<node>();
            std::function<void(const node_ptr& node, const bit_sequence &code, uint length, uint symbol_index)> insert_node = [&](const node_ptr& n, const bit_sequence& code, uint length, uint symbol_index) {
                if (length < code.length()) {
                    if (code.bit(length)) {
                        if (!n->one) n->one = std::make_shared<node>();
                        n->one->symbol_index = -1;
                        insert_node(n->one, code, length+1, symbol_index);
                    } else {
                        if (!n->zero) n->zero = std::make_shared<node>();
                        insert_node(n->zero, code, length+1, symbol_index);
                    }
                } else {
                    assert(!n->one);
                    assert(!n->zero);
                    assert(!n->count);
                    n->symbol_index = symbol_index;
                    n->count = 1;
                }
            };
            uint32_t code_bits = 0;
            uint last_length = 0;
            for(auto &e : symbol_indexes_and_lengths) {
                const auto &s = e.first;
                const auto length = e.second;
                assert(length >= last_length);
                assert(length < 16); // todo fix this
                if (length > last_length) {
                    code_bits <<= (length - last_length);
                    last_length = length;
                }
                uint32_t reversed = __rev(code_bits) >> (32 - length);
                auto code = bit_sequence(reversed, length);
                symbol_encodings[symbols[s]] = code;
//                std::cout << ">>: " << code << " " << static_cast<typename symbol_adapter<S>::ostream_type>(symbols[s]) << "\n";
                insert_node(root, code, 0, s);
                code_bits++;
            }
            min_code_length = H::max_code_size;
            max_code_length = 0;
            for(uint l=0;l<length_counts.size();l++) {
                if (length_counts[l]) {
                    min_code_length = std::min(min_code_length, l);
                    max_code_length = std::max(max_code_length, l);
                    length_stats.add(l, length_counts[l]);
                }
            }
        }
    }

    bool empty() const {
        return symbol_encodings.empty();

    }
    S get_first_symbol() const {
        return symbol_encodings.begin()->first;
    }

    S get_last_symbol() const {
        return symbol_encodings.rbegin()->first;
    }

    uint8_t get_min_code_length() {
        return min_code_length;
    }
    uint8_t get_max_code_length() {
        return max_code_length;
    }
    void dump() {
        std::function<void(const node_ptr& node)> dump_node = [&](const node_ptr& node) {
            if (node->symbol_index < 0) {
                assert(node->one && node->zero);
                dump_node(node->zero);
                dump_node(node->one);
            } else {
                std::cout << symbol_encodings[symbols[node->symbol_index]] << ": "
                    << static_cast<typename symbol_adapter<S>::ostream_type>(symbols[node->symbol_index]) << " ("
                    << stats.symbol_counts[symbols[node->symbol_index]] << ")" << std::endl;
            }
        };
        std::cout << "A\n";
        if (root) dump_node(root);
        std::cout << "B\n";
        for(const auto &e : symbol_encodings) {
            std::cout << static_cast<typename symbol_adapter<S>::ostream_type>(e.first) << " " << e.second << " (" << stats.symbol_counts[e.first] << ")\n";
        }
    }

    bit_sequence encode(const S& symbol) {
        auto it = symbol_encodings.find(symbol);
        assert(it != symbol_encodings.end());
        return it->second;
    }

    int get_code_length(const S& symbol) const {
        auto it = symbol_encodings.find(symbol);
        return it == symbol_encodings.end() ? 0 : it->second.length();
    }

    int get_stats_length() const {
        int length = 0;
        for(const auto &e : stats.symbol_counts) {
            length += e.second * get_code_length(e.first);
        }
        return length;
    }

    template<typename BI> S decode(BI& bi) {
        auto n = root;
        while (n->symbol_index < 0) {
            if (bi.bit()) n = n->one;
            else          n = n->zero;
        }
        return symbols[n->symbol_index];
    }

    template <typename Iterator> size_t size(Iterator it, const Iterator end) {
        size_t size = 0;
        while (it != end) {
            size += encode(*it++).length();
        }
        return size;
    }

    size_t size() {
        size_t size = 0;
        for(const auto& e : stats.symbol_counts) {
            if (e.second)
                size += e.second * encode(e.first).length();
        }
        return size;
    }


    template <typename I> void decode(const S& symbol, I& input) {
        input.bit();
    }

    symbol_stats<S> get_stats() const { return stats; }
    symbol_stats<uint8_t> get_length_stats() const { return length_stats; }
    bool initialized() const { return initted; }
    std::vector<S> get_symbols() const { return symbols; }

private:
    static bool limit_code_size(std::vector<int>& length_counts, uint max_code_size) {
        if (max_code_size >= length_counts.size()) return false;
        for (uint i = max_code_size + 1; i < length_counts.size(); i++) {
            length_counts[max_code_size] += length_counts[i];
        }
        uint32_t total = 0;
        for (int i = (int)max_code_size; i > 0; i--) {
            total += (((uint32_t) length_counts[i]) << (max_code_size - i));
        }
        bool rc = false;
        while (total > (1u << max_code_size)) {
            length_counts[max_code_size]--;
            for (int i = (int)max_code_size - 1; i > 0; i--) {
                if (length_counts[i]) {
                    length_counts[i]--;
                    length_counts[i + 1] += 2;
                    break;
                }
            }
            total--;
            rc = true;
        }
        length_counts.resize(max_code_size+1);
        return rc;
    }

    node_ptr root;
    std::map<S, bit_sequence, typename H::comparator_type> symbol_encodings;
    symbol_stats<S> stats;
    symbol_stats<uint8_t> length_stats;
    std::vector<S> symbols;
    uint min_code_length, max_code_length;
    bool initted = false;
};

template<typename S> template<typename H> huffman_encoding<S,H> symbol_stats<S>::create_huffman_encoding(const H& params) {
    return huffman_encoding<S,H>(*this, params);
}

struct byte_vector_bit_input {
    explicit byte_vector_bit_input(const std::vector<uint8_t>& data) : data(data), pos(0) {}
    bool bit() {
        assert(pos < data.size() * 8);
        bool rc = data[pos>>3]&(1u<<(pos&7u));
        pos++;
        return rc;
    }
    uint32_t read(int n) {
        uint32_t accum = 0;
        for(int i=0;i<n;i++) {
            if (bit()) accum |= 1u << i;
        }
        return accum;
    }
//protected:
    std::vector<uint8_t> data;
    uint32_t pos;
};


struct byte_vector_bit_output {
    byte_vector_bit_output() : accumulator(0), bits(0) {}
    void write(const bit_sequence& seq) {
        accumulator |= seq.bits() << bits;
        bits += seq.length();
        while (bits >= 8) {
            output.push_back(accumulator);
            accumulator >>= 8;
            bits -= 8;
        }
    }
    std::vector<uint8_t> get_output() {
        pad_to_byte();
        return output;
    }

    uint bit_size() const {
        return output.size() * 8 + bits;
    }

    uint bit_index() const {
        return bits;
    }

    void truncate(uint size) {
        assert(size <= bit_size());
        output.resize(size/8);
        bits = size & 7u;
    }

    template<typename BO> void write_to(std::shared_ptr<BO> bo) {
        for(const auto& b : output) {
            bo->write(bit_sequence(b, 8));
        }
        if (bits) {
            bo->write(bit_sequence(accumulator, bits));
        }
    }

    template<typename BO> void write_to(BO &bo) {
        for(const auto& b : output) {
            bo.write(bit_sequence(b, 8));
        }
        if (bits) {
            bo.write(bit_sequence(accumulator, bits));
        }
    }

    void pad_to_byte() {
        if (bits) {
            assert(bits < 8);
            write(bit_sequence(0, 8u-bits));
        }
    }
private:
    uint32_t accumulator;
    uint bits;
    std::vector<uint8_t> output;
};

template <typename S, bool DEBUG=false> struct huffman_decoder {
    huffman_decoder() = default;
    explicit huffman_decoder(const std::vector<std::pair<S, int>>& symbol_lengths) {
        if (symbol_lengths.size() > 1) {
            // could have passed this, but fine for now
            int min_code_length = std::numeric_limits<int>::max();
            int max_code_length = std::numeric_limits<int>::min();
            for(const auto & e : symbol_lengths) {
                min_code_length = std::min(min_code_length, e.second);
                max_code_length = std::max(max_code_length, e.second);
            }
            code_ceiling.resize(max_code_length);
            offset.resize(max_code_length);
            std::vector<int> num_codes(max_code_length);
            for(const auto & e : symbol_lengths) {
                num_codes[e.second-1]++;
            }
            for(int i = 1; i < max_code_length; i++) {
                offset[i] = offset[i-1] + num_codes[i-1];
            }
            symbols.resize(symbol_lengths.size());
            for(const auto &e : symbol_lengths) {
                const auto length = e.second;
                symbols[offset[length-1]++] = e.first;
            }
            int code = 0;
            int pos = 0;
            for(int length = 0; length < max_code_length; length++) {
                if (DEBUG) {
                    std::cout << "Length " << length << "\n";
                    for(int i=0; i< num_codes[length]; i++) {
                        uint32_t reversed = __rev(code + i) >> (31 - length);
                        auto code = bit_sequence(reversed, length+1);
                        std::cout << (pos + i)  << " " << code << " " << static_cast<typename symbol_adapter<S>::ostream_type>(symbols[pos + i]) << "\n";
                    }
                }
                offset[length] = code - pos;
                code += num_codes[length];
                code_ceiling[length] = code;
                pos += num_codes[length];
                code <<= 1;
            }
        } else if (symbol_lengths.size() == 1) {
            symbols.push_back(symbol_lengths[0].first);
        }
    }

    template <typename BI> S decode(BI& bi) {
        assert(!symbols.empty());
        if (symbols.size() == 1) {
            return symbols[0];
        }
        uint code = 0;
        uint length = 0;
        do {
            code = (code << 1u) | bi.bit();
            if (code < code_ceiling[length]) {
                return symbols[code - offset[length]];
            }
            length++;
        } while (true);
    }

    std::vector<int> code_ceiling;
    std::vector<int> offset;
    std::vector<S> symbols;
};

template<typename BO, typename uint8_t, typename H>
void output_min_max_best(BO &bit_output, huffman_encoding<uint8_t, H> &huff) {
    bit_output->write(bit_sequence(!huff.empty(), 1));
    if (huff.empty()) return;
    auto bo1 = std::make_shared<byte_vector_bit_output>();
    output_min_max(bo1, huff, true, true);
    auto bo2 = std::make_shared<byte_vector_bit_output>();
    output_min_max(bo2, huff, true, false);
    auto bo3 = std::make_shared<byte_vector_bit_output>();
    output_min_max(bo3, huff, false, false);
//    printf("BO1/2/3 %d %d %d\n", (int) bo1->bit_size(), (int) bo2->bit_size(), (int) bo3->bit_size());
    size_t min = std::min(bo1->bit_size(), std::min(bo2->bit_size(), bo3->bit_size()));
    if (bo1->bit_size() == min) {
        bit_output->write(bit_sequence(1, 1));
        bo1->write_to(bit_output);
    } else if (bo2->bit_size() == min) {
        bit_output->write(bit_sequence(0, 1));
        bo2->write_to(bit_output);
    } else {
//        printf("WA\n"); // now seems to happen, but rare so not sure we care enough to add complexity
        if (bo1->bit_size() < bo2->bit_size()) {
            bit_output->write(bit_sequence(1, 1));
            bo1->write_to(bit_output);
        } else {
            bit_output->write(bit_sequence(0, 1));
            bo2->write_to(bit_output);
        }
        //assert(false); // doesn't seem to be ever, plan to remove
        //        bit_output->write(bit_sequence(0b11, 2));
        //        bo3->write_to(bit_output);
    }
}

template<typename BO, typename uint8_t, typename H>
void output_min_max(BO &bit_output, huffman_encoding<uint8_t, H> &huff, bool zero_flag, bool group8) {
    if (huff.empty()) {
        assert(false); // should be handled at a higher level
        return;
    }
    uint8_t min = huff.get_first_symbol();
    uint8_t max = huff.get_last_symbol();
    bit_output->write(bit_sequence(min, 8));
    bit_output->write(bit_sequence(max, 8));
    int min_cl = huff.get_min_code_length();
    int max_cl = huff.get_max_code_length();
    if (min == max) {
        assert(min_cl == max_cl);
        assert(!min_cl);
        return;
    }
    assert(max_cl < 16);
    assert(min_cl <= max_cl);
    bit_output->write(bit_sequence(min_cl, 4));
    bit_output->write(bit_sequence(max_cl, 4));
    if (min_cl == max_cl && zero_flag) {
        for (int val = min; val <= max; val++) {
            int length = huff.get_code_length(val);
            assert(!length || length == min_cl);
            bit_output->write(bit_sequence(length != 0, 1));
        }
    } else {
        uint bit_count = 32 - __builtin_clz(max_cl - min_cl + (zero_flag ? 0 : 1));
        auto stats = huff.get_stats();
        if (zero_flag) {
            if (group8) {
                for (uint base_val = min; base_val <= max; base_val += 8) {
                    uint8_t mask = 0;
                    uint8_t bits = 0;
                    for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                        if (huff.get_code_length(base_val + i)) mask |= 1u << i;
                        bits |= 1u << i;

                    }
                    if (mask == 0) {
                        bit_output->write(bit_sequence(0b01, 2));
                    } else if (mask == bits) {
                        bit_output->write(bit_sequence(0b11, 2));
                        for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                            int length = huff.get_code_length(base_val + i);
                            assert(length);
                            length -= min_cl;
                            bit_output->write(bit_sequence(length, bit_count));
                        }
                    } else {
                        bit_output->write(bit_sequence(0b0, 1));
                        for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                            int length = huff.get_code_length(base_val + i);
                            bit_output->write(bit_sequence(length != 0, 1));
                            if (length) {
                                length -= min_cl;
                                bit_output->write(bit_sequence(length, bit_count));
                            }
                        }
                    }
                }
            } else {
                for (int val = min; val <= max; val++) {
                    int length = huff.get_code_length(val);
                    bit_output->write(bit_sequence(length != 0, 1));
                    if (length) {
                        length -= min_cl;
                        bit_output->write(bit_sequence(length, bit_count));
                    }
                }
            }
        } else {
            for (int val = min; val <= max; val++) {
                int length = huff.get_code_length(val);
                if (length) {
                    length -= min_cl;
                } else {
                    length = (1u << bit_count) - 1;
                }
                bit_output->write(bit_sequence(length, bit_count));
            }
        }
    }
}

template<typename BI> std::vector<std::pair<uint8_t, int>> decode_min_max8(BI &bi) {
    bool non_empty = bi.bit();
    if (!non_empty) {
        printf("  No values\n");
        return {};
    } else {
        bool group8 = bi.bit();
        uint8_t min = bi.read(8);
        uint8_t max = bi.read(8);
        if (min == max) {
            printf("  %d: 0 bits\n", min);
            return {std::make_pair(min, 0)};
        }
        printf("  Min/Max %d/%d\n", min, max);
        int min_cl = bi.read(4);
        int max_cl = bi.read(4);
        printf("  Code length %d->%d\n", min_cl, max_cl);
        std::vector<std::pair<uint8_t, int>> symbol_lengths;
        if (min_cl == max_cl) {
            for (uint val = min; val <= max; val++) {
                if (bi.bit()) {
                    printf("  %d: %d bits\n", val, min_cl);
                    symbol_lengths.template emplace_back(val, min_cl);
                }
            }
        } else {
            int bit_count = 32 - __builtin_clz(max_cl - min_cl);
            if (group8) {
                for (int base_val = min; base_val <= max; base_val += 8) {
                    bool all_same = bi.bit();
                    if (all_same) {
                        if (bi.bit()) {
                            for (uint i = 0; i <= std::min(7, max - base_val); i++) {
                                int code_length = min_cl + bi.read(bit_count);
                                symbol_lengths.template emplace_back(base_val + i, code_length);
                                printf("  %d: %d bits\n", base_val + i, code_length);
                            }
                        }
                    } else {
                        for (int i = 0; i <= std::min(7, max - base_val); i++) {
                            if (bi.bit()) {
                                int code_length = min_cl + bi.read(bit_count);
                                symbol_lengths.template emplace_back(base_val + i, code_length);
                                printf("  %d: %d bits\n", base_val + i, code_length);
                            }
                        }
                    }
                }
            } else {
                for (int val = min; val <= max; val++) {
                    if (bi.bit()) {
                        int code_length = min_cl + bi.read(bit_count);
                        symbol_lengths.template emplace_back(val, code_length);
                        printf("  %d: %d bits\n", val, code_length);
                    }
                }
            }
        }
        return symbol_lengths;
    }
}
