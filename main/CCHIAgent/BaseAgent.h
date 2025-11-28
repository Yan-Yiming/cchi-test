#include <memory>
#include <set>
#include <cstdint>
#include <assert.h>
#include <random>
#include <unordered_map>
#include "../CCHI/xact.h"
#include "../CCHI/base.hpp"
#include "../Utils/ScoreBoard.hpp"

typedef uint64_t paddr_t;

namespace CCHIAgent {

    template<typename T>
    class PendingTrans {
    public:
        int beat_cnt;
        int nr_beat;
        std::shared_ptr<T> info;

        PendingTrans() {
            nr_beat = 0;
            beat_cnt = 0;
        }
        ~PendingTrans() = default;

        bool is_multiBeat() { return (this->nr_beat != 1); };
        bool is_pending() { return (beat_cnt != 0); }
        void init(std::shared_ptr<T> &info, int nr_beat) {
            this->info = info;
            this->nr_beat = nr_beat;
            beat_cnt = nr_beat;
        }
        void update() {
            beat_cnt--;
            tlc_assert(beat_cnt >= 0);
        }
    };

    class IDPool {
    private:
        std::set<int> *idle_ids;
        std::set<int> *used_ids;
        int pending_freeid;
    public:
        IDPool(int start, int end) {
            idle_ids = new std::set<int>();
            used_ids = new std::set<int>();
            for (int i = start; i < end; i++) {
                idle_ids->insert(i);
            }
            used_ids->clear();
            pending_freeid = -1;
        }
        ~IDPool() {
            delete idle_ids;
            delete used_ids;
        }
        int getid() {
            if (idle_ids->size() == 0)
                return -1;
            int ret = *idle_ids->begin();
            used_ids->insert(ret);
            idle_ids->erase(ret);
            return ret;
        }
        void freeid(int id) {
            this->pending_freeid = id;
        }
        bool full() {
            return idle_ids->empty();
        }
    };


    class BaseAgent {
    public:
        IDPool      idpool;
    };

    // type 1: full coherent agent
    class FCAgent : public BaseAgent{

    private:
        uint64_t    *cycles;

        // txnID to xact
        std::unordered_map<uint8_t, std::shared_ptr<Xact::Xaction>>     Transactions;
        // std::unordered_map<uint8_t, Xact::Xaction>     Transactions;
        
        // std::unordered_map<uint64_t, Xact::Xaction> evtTransactions;
        // std::unordered_map<uint64_t, Xact::Xaction> reqTransactions;
        // std::unordered_map<uint64_t, Xact::Xaction> snpTransactions;

        std::unordered_map<paddr_t, localBoardEntry>    localBoard;
        std::unordered_map<uint64_t, uint64_t>          DBID2TxnID;

        PendingTrans<CCHI::BundleChannelEVT>    pendingTXEVT;
        PendingTrans<CCHI::BundleChannelREQ>    pendingTXREQ;
        PendingTrans<CCHI::BundleChannelRSP>    pendingTXRSP;
        PendingTrans<CCHI::BundleChannelDAT>    pendingTXDAT;

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
        
        void        send_txevt(std::shared_ptr<CCHI::BundleChannelEVT> &txevt, uint8_t alias);
        void        send_txreq(std::shared_ptr<CCHI::BundleChannelREQ> &txreq, uint8_t alias);
        void        send_txdat(std::shared_ptr<CCHI::BundleChannelDAT> &txdat, uint8_t alias);


        bool        do_writebackfull(paddr_t addr);
        bool        do_readunique(paddr_t addr);
    };
}