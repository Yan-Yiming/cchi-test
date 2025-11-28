#include <cstdint>
#include "../CCHI/base.hpp"
#include "../CCHI/xact.h"

#define ALIAS_NUM 4

class localBoardEntry {
public:
    Xact_type       inflight_xact;
    bool            inflight;
    
    CCHI::State     state[ALIAS_NUM];
    uint8_t         dirty[ALIAS_NUM];

    inline localBoardEntry(Xact_type type, CCHI::State state, uint8_t dirty, uint8_t alias) {
        this->state[alias]  = state;
        this->dirty[alias]  = dirty;
        this->inflight_xact = type;
        this->inflight      = true;
    }
};

class globalBoardEntry {
public:
    paddr_t         addr;

    // data
};
    