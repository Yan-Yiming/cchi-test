#include <cstdint>
#include "CCHI/base.hpp"
#include "CCHI/xact.h"

#define ALIAS_NUM 4

class localBoardEntry {
public:
    Xact::Xaction*  xact;
    
    paddr_t         addr;
    CCHI::State     state[ALIAS_NUM];
    uint8_t         dirty[ALIAS_NUM];
};

class globalBoardEntry {
public:
    paddr_t         addr;
};
    