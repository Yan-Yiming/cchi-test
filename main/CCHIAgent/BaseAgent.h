#include <memory>
#include <cstdint>
#include <assert.h>
#include <unordered_map>
#include "../CCHI/xact.h"
#include "../CCHI/base.hpp"

typedef uint64_t paddr_t;

namespace CCHIAgent {


    // type 1: full coherent agent
    class FCAgent {

    private:
        uint64_t    *cycles;

        // txnID to xact?
        std::unordered_map<uint64_t, Xact::Xaction> evtTransactions;
        std::unordered_map<uint64_t, Xact::Xaction> reqTransactions;
        std::unordered_map<uint64_t, Xact::Xaction> snpTransactions;

        std::unordered_map<paddr_t, localBoardEntry> localBoard;

        CCHI::FCBundle*     port;
    
    public:
        void        handle_channel();
        void        random_test();
        void        update_signal();

        void        fire_txevt();
        void        fire_txreq();
        void        fire_rxsnp();
        void        fire_txrsp();
        void        fire_txdat();
        void        fire_rxrsp();
        void        fire_rxdat();

        void        do_writebackfull(paddr_t addr);
    };


}