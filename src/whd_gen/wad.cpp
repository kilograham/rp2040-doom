/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "wad.h"
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include <cassert>
#include "../whddata.h"

typedef struct __attribute__((packed)) {
    // Should be "IWAD" or "PWAD".
    char		identification[4];
    int32_t			numlumps;
    int32_t			infotableofs;
} wadinfo_t;

// todo we may be able to limit all lumps to 64K?
typedef struct __attribute__((packed)) {
    uint32_t offset;
    int size;
    char name[9];
    char _pad;
    short next;
} whd_lump_t;

typedef struct __attribute__((packed)){
int32_t			filepos;
int32_t			size;
char		name[8];
} filelump_t;

template<typename T> struct typed_element_ptr {
    std::vector<uint8_t> storage;
    int count;
    typed_element_ptr(std::vector<uint8_t> v, int count) : storage(std::move(v)), count(count) {}

    int size() {
        return count;
    }
    T* get(int index = 0) {
        assert(index < count);
        return ((T*)storage.data()) + index;
    }
};

template<typename T> typed_element_ptr<T> read_raw(FILE *in, int count = 1) {
    size_t size = count * sizeof(T);
    std::vector<uint8_t> v(size);
    if (1 != fread(v.data(), size, 1, in)) {
        throw std::runtime_error(std::string("Failed to read ") + std::to_string(size) + " bytes from file");
    }
    return typed_element_ptr<T>(v, count);
}

template<typename T> void write_raw(FILE *out, T* data, int count = 1) {
    size_t size = count * sizeof(T);
    if (1 != fwrite(data, size, 1, out)) {
        throw std::runtime_error(std::string("Failed to write ") + std::to_string(size) + " bytes to file");
    }
}

void write_raw(FILE *out, const std::vector<uint8_t>& data) {
    size_t size = data.size();
    if (1 != fwrite(data.data(), size, 1, out)) {
        throw std::runtime_error(std::string("Failed to write ") + std::to_string(size) + " bytes to file");
    }
}

wad wad::read(const std::string &filename) {
    wad rc;
    FILE *in = fopen(filename.c_str(), "rb");
    if (!in) throw std::invalid_argument(filename + " not found");

    auto header_raw = read_raw<wadinfo_t>(in);
    auto header = header_raw.get();
    if (strncmp(header->identification, "IWAD", 4)) {
        throw std::runtime_error("file is not an IWAD");
    }
    fseek(in, header->infotableofs, SEEK_SET);
    auto lumps_raw = read_raw<filelump_t>(in, header->numlumps);
    for(int i=0;i<(int)lumps_raw.size();i++) {
        auto lraw = lumps_raw.get(i);
        std::string name = wad::wad_string(lraw->name);
        if (!lraw->size) {
            rc.lumps[i] = lump(name, std::vector<uint8_t>(), i);
        } else {
            fseek(in, lraw->filepos, SEEK_SET);
            rc.lumps[i] = lump(name, read_raw<uint8_t>(in, lraw->size).storage, i);
        }
        rc.lump_names[to_lower(name)] = i;
    }
    printf("LUMP METADATA %d (%dK)\n", (int)(lumps_raw.size() * sizeof(filelump_t)), ((int)(lumps_raw.size() * sizeof(filelump_t))+512)/1024);
    fclose(in);
    return rc;
}

// Hash function used for lump names.
unsigned int W_LumpNameHash(const char *s)
{
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i=0; i < 8 && s[i] != '\0'; ++i)
    {
        result = ((result << 5) ^ result ) ^ toupper(s[i]);
    }

    return result;
}

void wad::write_whd(const std::string &filename, std::set<std::string> name_required, uint32_t hash, bool super_tiny) {
    FILE *out = fopen(filename.c_str(), "wb");
    if (!out) throw std::invalid_argument(filename + " can't be opened for write");

    std::set<std::string> name_required_lower; // use lower case to match c stricmp sorting
    for (const auto&s : name_required) {
        if (get_lump_index(s) >= 0) {
            name_required_lower.insert(to_lower(s));
        }
    }
    int num_lumps = 1 + lumps.rbegin()->first;
    wadinfo_t header {
        .identification = { 'I', 'W', 'H', super_tiny ? 'X' : 'D' },
        .numlumps = num_lumps,
        .infotableofs = sizeof(wadinfo_t) + sizeof(whdheader_t),
    };
    write_raw(out, &header);
    uint32_t name_count = name_required_lower.size();
    whdheader_t whdheader = {
            .hash = hash,
            .num_named_lumps = (uint16_t)name_count,
    };
    strcpy(whdheader.name, name.c_str());
    write_raw(out, &whdheader); // we will write it again later with size
    assert(ftell(out) == header.infotableofs);
#if 0
    int num = 0;
    whd_lump_t empty_lump = {
            .offset = 0,
            .size = 0,
            .name = { '?', 0, 0, 0, 0, 0, 0, 0, 0},
            ._pad = 0,
            .next = -1
    };
    int num_hash_entries = num_lumps; // no reason for this
    int base_data_offset = header.infotableofs + num_lumps * sizeof(whd_lump_t) + num_hash_entries * 2;
    int data_offset = base_data_offset;
    std::vector<int> next(num_lumps, -1);

    for(const auto &e : lumps) {
        while (num < e.first) {
            write_raw(out, &empty_lump);
            num++;
        }
        const auto& l = e.second;
        int size = l.data.size();
        whd_lump_t fl;
        fl.offset = data_offset;
        fl.size = size;
        memset(&fl.name, 0, 9);
        fl._pad = 0;
        int hash = W_LumpNameHash(l.name.c_str()) % num_lumps;
        fl.next = next[hash];
        next[hash] = num;
        if (l.name.size() < 8)
            strcpy(fl.name, l.name.c_str());
        else
            memcpy(fl.name, l.name.c_str(), 8);
        write_raw(out, &fl);
//        printf("%d %08x %08x %s\n", num, data_offset, size, l.name.c_str());
        data_offset += size;
        data_offset = (data_offset + 3) &~3;
        num++;
    }
    for(int n : next) {
        short s = (short)n;
        write_raw(out, &s);
    }
#else
    int base_data_offset = header.infotableofs + (num_lumps + 1) * sizeof(uint32_t) + name_count * 12;
    int data_offset = base_data_offset;
    int num = 0;
    for(const auto &e : lumps) {
        while (num < e.first) {
            write_raw(out, &data_offset);
            num++;
        }
        const auto& l = e.second;
        int size = l.data.size();
        uint32_t combined = data_offset | ((4 -size) << 30); // store amount to substract off word aligned size to get real size in two high bits
        write_raw(out, &combined);
        data_offset += size;
        data_offset = (data_offset + 3) &~3;
        num++;
    }
#endif
    write_raw(out, &data_offset);
    for(const auto &s : name_required_lower) {
        std::vector<uint8_t> n(10);
        strncpy((char *)n.data(), s.c_str(), 8);
        write_raw(out, n);
        int16_t lnum = get_lump_index(s);
//        printf("%s %d\n", s.c_str(), lnum);
        assert(num >= 0);
        write_raw(out, &lnum);
    }
    printf("WHD LUMP METADATA %d (%dK)\n", (int)ftell(out), (((int)ftell(out))+512)/1024);

    assert(ftell(out) == base_data_offset);
    for(const auto &e : lumps) {
        if (e.second.data.size()) {
            write_raw(out, e.second.data);
            for(int i=e.second.data.size() & 3; i && i < 4; i++) {
                fputc(0, out);
            }
        }
    }
    whdheader.size = ftell(out);
    fseek(out, sizeof(wadinfo_t), SEEK_SET);
    write_raw(out, &whdheader);
    fclose(out);
}

void wad::write(const std::string &filename) {
    FILE *out = fopen(filename.c_str(), "wb");
    if (!out) throw std::invalid_argument(filename + " can't be opened for write");

    int num_lumps = 1 + lumps.rbegin()->first;
    wadinfo_t header {
            .identification = { 'I', 'W', 'A', 'D' },
            .numlumps = num_lumps,
            .infotableofs = sizeof(wadinfo_t),
    };
    write_raw(out, &header);
    assert(ftell(out) == header.infotableofs);
    int num = 0;
    filelump_t empty_lump = {
            .filepos = 0,
            .size = 0,
            .name = { '?' },
    };
    int base_data_offset = header.infotableofs + num_lumps * sizeof(filelump_t);
    int data_offset = base_data_offset;
    std::vector<int> next(num_lumps, -1);

    for(const auto &e : lumps) {
        while (num < e.first) {
            write_raw(out, &empty_lump);
            num++;
        }
        const auto& l = e.second;
        int size = l.data.size();
        filelump_t fl;
        fl.filepos = data_offset;
        fl.size = size;
        memset(&fl.name, 0, 8);
        int hash = W_LumpNameHash(l.name.c_str()) % num_lumps;
        next[hash] = num;

        if (l.name.size() < 8)
            strcpy(fl.name, l.name.c_str());
        else
            memcpy(fl.name, l.name.c_str(), 8);
        write_raw(out, &fl);
        data_offset += size;
        data_offset = (data_offset + 3) &~3;
        num++;
    }

    assert(ftell(out) == base_data_offset);
    for(const auto &e : lumps) {
        if (e.second.data.size()) {
            write_raw(out, e.second.data);
            for(int i=e.second.data.size() & 3; i && i < 4; i++) {
                fputc(0, out);
            }
        }
    }
    fclose(out);
}