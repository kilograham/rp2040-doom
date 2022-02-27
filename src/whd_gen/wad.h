/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include <cctype>
#include <cstring>
#include <string>
#include <set>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>
#include <cassert>
inline std::string to_lower(std::string x) {
    std::for_each(x.begin(), x.end(), [](char & c){
        c = ::tolower(c);
    });
    return x;
}

inline std::string to_upper(std::string x) {
    std::for_each(x.begin(), x.end(), [](char & c){
        c = ::toupper(c);
    });
    return x;
}

struct lump {
    lump() = default;
    explicit lump(std::string name, std::vector<uint8_t> v, int num) : name(std::move(name)), data(std::move(v)), num(num) {}
    std::string name;
    std::vector<uint8_t> data;
    int num = -1;
};

struct wad {
    wad() {
        set_name("");
    }
    static wad read(const std::string& filename);
    void write(const std::string& filename);
    void write_whd(const std::string& filename, std::set<std::string> name_required, uint32_t hash, bool super_tiny);

    std::map<int, lump>& get_lumps() {
        return lumps;
    }

    void keep_only(const std::set<int>& nums) {
        for(auto it = lumps.begin(); it != lumps.end();) {
            if (it->second.data.empty() || nums.find(it->first) != nums.end()) {
//                printf("Keeping %s\n", it->second.name.c_str());
                it++;
            } else {
                it = lumps.erase(it);
                lump_names.erase(it->second.name);
            }
        }
    }

    static std::string wad_string(const char data[8]) {
        std::string rc = data[7] ? std::string(data, 8) : std::string(data);
        rc = rc.substr(0, strlen(rc.c_str()));
        return rc;
    }

    int get_lump_index(const std::string &name) {
        auto it = lump_names.find(to_lower(name));
        if (it != lump_names.end()) {
            return it->second;
        }
        return -1;
    }

    int get_lump(const std::string &name, lump& lump_out) {
        auto it = lump_names.find(to_lower(name));
        if (it != lump_names.end()) {
            lump_out = lumps[it->second];
            return it->second + 1;
        }
        return 0;
    }

    bool get_lump(int num, lump& lump_out) {
        if (lumps[num].data.size()) {
            lump_out = lumps[num];
            return true;
        }
        return false;
    }

    void update_lump(const lump& lump) {
        assert(lump.num>=0);
        if (to_lower(lumps[lump.num].name) != to_lower(lump.name)) {
            auto it = lump_names.find(to_lower(name));
            if (it != lump_names.end()) {
                lump_names.erase(it);
            }
            lump_names[to_lower(lump.name)] = lump.num;
            lumps[lump.num].name = lump.name;
        }
        lumps[lump.num].num = lump.num; // incase this is a new lump
        lumps[lump.num].data = lump.data;
    }

    void remove_lump(const std::string &name) {
        auto it = lump_names.find(to_lower(name));
        if (it != lump_names.end()) {
            lumps.erase(lumps.find(it->second));
            lump_names.erase(it);
        }
    }

    void set_name(std::string name) {
        if (!name.empty() && name.length() <= 13)
            this->name = to_upper(name);
        else
            this->name = "DOOM.WAD";
    }
protected:
    std::string name;
    std::map<int, lump> lumps; // we support spare wads for now
    std::map<std::string, int> lump_names;
};