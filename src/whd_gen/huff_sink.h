/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "huffman.h"

template<typename H, typename BO=std::shared_ptr<byte_vector_bit_output>, bool DEBUG=false>
        struct symbol_sink {
    using S = typename H::symbol_type;

    symbol_sink() : name("<none>") {}

    explicit symbol_sink(std::string name) : name(std::move(name)) {}

    void output(S symbol) {
        if (bit_output) {
            bit_output->write(huff.encode(symbol));
        } else {
            stats.add(symbol);
        }
    }

    void begin_output(BO &bo) {

        if (!huff.initialized()) huff = stats.template create_huffman_encoding<H>();
        bit_output = bo;
    }

    template<typename BI>
    S decode(BI &bi) {
        if (!huff.initialized()) {
            huff = stats.template create_huffman_encoding<H>();
        }
        return huff.template decode(bi);
    }

    void dump(bool force_debug = false) {
        if (DEBUG || force_debug) {
            if (!huff.initialized()) {
                huff = stats.template create_huffman_encoding<H>();
            }
            huff.dump();
            auto level1_stats = huff.get_length_stats();
            auto level1_huff = level1_stats.template create_huffman_encoding<huffman_params<uint8_t>>();
            //        level1_huff.dump();
//            printf("SIZE %d %d\n", stats.get_total(), (int) huff.template size() / 8);
//            printf("CODE SIZE %d %d\n", level1_stats.get_total(), (int) level1_huff.template size() / 8);
            printf("\n");
        }
    }

    std::string get_name() const { return name; }

    symbol_stats<S> stats;
    huffman_encoding<S, H> huff;
    BO bit_output;
    std::string name;
};

template<typename BO=std::shared_ptr<byte_vector_bit_output>>
        struct bit_sink {
            void output(bit_sequence bs) {
                if (bit_output) {
                    bit_output->write(bs);
                }
            }

            void begin_output(BO bo) {
                bit_output = bo;
            }

            void dump() {
            }

            std::string get_name() const { return "raw bits"; }

        private:
            BO bit_output;
        };

template<typename BO=std::shared_ptr<byte_vector_bit_output>>
        struct sink_wrapper {
            template<class T>
                    sink_wrapper(T &t) {
                        auto ptr = &t;
                        dump = [ptr]() { return ptr->dump(); };
                        get_name = [ptr]() { return ptr->get_name(); };
                        begin_output = [ptr](BO bo) {
                            ptr->begin_output(bo);
//                            std::cout << ptr->get_name() << "\n";
//                            ptr->dump();
                        };
                    }

                    std::function<void(void)> dump;
            std::function<std::string(void)> get_name;
            std::function<void(BO)> begin_output;
        };

template<typename BO=std::shared_ptr<byte_vector_bit_output>>
        struct sink_wrappers {
            sink_wrappers(std::initializer_list<sink_wrapper<BO>> &&init) : wrappers(
                    std::forward<std::vector<sink_wrapper<BO>>>(init)) {}

                    void dump() {
                std::for_each(wrappers.begin(), wrappers.end(), [](auto e) {
                    std::cout << e.get_name() << std::endl;
                    e.dump();
                });
            }

            void begin_output(BO bo) {
                std::for_each(wrappers.begin(), wrappers.end(), [bo](auto &e) {
                    e.begin_output(bo);
                });
            }

            std::vector<sink_wrapper<BO>> wrappers;
        };

static inline th_bit_input *create_bip(byte_vector_bit_input &bi) {
    auto bip = new th_bit_input();
    bip->cur = bi.data.data() + (bi.pos >> 3);
#ifndef NDEBUG
    bip->end = bi.data.data() + bi.data.size();
#endif
    bip->bit = bi.pos & 7u;
    return bip;
}

static inline void uncreate_bip(byte_vector_bit_input &bi, th_bit_input *bip) {
    bi.pos = (bip->cur - bi.data.data()) * 8 + bip->bit;
    delete bip;
}

