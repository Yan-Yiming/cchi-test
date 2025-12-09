#include <cstdint>
#include <unordered_map>
#include "CCHIAgent/BaseAgent.h"

class CHISequencer {
public:
    enum class State {
        INIT = 0,
        ALIVE,
        FAILED,
        FINISHED
    };

public:
    using FCAgent   = CCHIAgent::FCAgent;

public:
    State       state;
    uint64_t    cycles;

    FCAgent     *fcagent;
    std::unordered_map<paddr_t, globalBoardEntry> globalBoard;

    CHISequencer();
    ~CHISequencer();

    State   GetState();
    bool    IsAlive();
    bool    IsFailed();
    bool    IsFinished();

    void    Tick(uint64_t cycles);
    void    Tock();

};