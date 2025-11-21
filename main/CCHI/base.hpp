#include <cstdint>

enum class CCHIOpcodeEVT {
    Evict           = 0,
    //              = 1,
    WriteBackFull   = 2
    //              = 3,
};

namespace CCHI {
    class Flit {
    };

    class DecoupledBundle {
    public:
        bool    valid;
        bool    ready;

        bool fire() const {
            return ready && valid;
        }
    };

    class BundleChannelEVT : public DecoupledBundle {
    public:
        uint8_t     txnID;
        uint8_t     opcode;
        uint64_t    addr;
        uint8_t     ns;
        uint8_t     memAttr;
        uint8_t     wayValid;
        uint8_t     way;
        uint8_t     traceTag;
        uint8_t     allowRetry;
    };

    class BundleChannelREQ : public DecoupledBundle {
    public:
        uint8_t     txnID;
        uint8_t     opcode;
        uint8_t     size;
        uint64_t    addr;
        uint8_t     ns;
        uint8_t     order;
        uint8_t     memAttr;
        uint8_t     excl;
        uint8_t     expCompStash;
        uint8_t     wayValid;
        uint8_t     way;
        uint8_t     traceTag;
        uint8_t     allowRetry;
    };

    class BundleChannelSNP : public DecoupledBundle {
    public:
        uint8_t     txnID;
        uint8_t     opcode;
        uint64_t    addr;
        uint8_t     ns;
        uint8_t     traceTag;
    };

    class BundleChannelRSP : public DecoupledBundle {
    public:
        uint8_t     txnID;
        uint8_t     opcode;
        uint8_t     respErr;
        uint8_t     resp;
        uint8_t     cbusy;
        uint8_t     wayValid;
        uint8_t     way;
        uint8_t     traceTag;
    };

    class BundleChannelDAT : public DecoupledBundle {
    public:
        uint8_t     txnID;
        uint8_t     opcode;
        uint8_t     respErr;
        uint8_t     resp;
        uint8_t     dataSource;
        uint8_t     cbusy;
        uint8_t     wayValid;
        uint8_t     way;
        uint8_t     traceTag;
        uint32_t    be;
        uint64_t    data[4];
    };

    class FCBundle {
    public:
        BundleChannelEVT    txevt;
        BundleChannelREQ    txreq;
        BundleChannelSNP    rxsnp;
        BundleChannelRSP    txrsp;
        BundleChannelDAT    txdat;
        BundleChannelRSP    rxrsp;
        BundleChannelDAT    rxdat;
    };

    enum class State {
        INV = 0,
        UC,
        SC,
        UD
    };
}