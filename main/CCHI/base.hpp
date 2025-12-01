#pragma once

#include <cstdint>
#include <climits>

namespace CCHI {

    namespace details {

        template<class T>
        class Enumeration {
        public:
            const char*     name;
            const int       value;
            const T* const  prev;

        public:
            inline constexpr Enumeration(const char* name, const int value, const T* const prev = nullptr) noexcept
            : name(name), value(value), prev(prev) { }

            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const T*() const noexcept
            { return static_cast<const T*>(this); };

            inline constexpr bool operator==(const T& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const T& obj) const noexcept
            { return !(*this == obj); }

            inline constexpr bool IsValid() const noexcept
            { return value != INT_MIN; }
        };
    }
}

enum class CCHIOpcodeEVT {
    Evict           = 0,
    //              = 1,
    WriteBackFull   = 2
    //              = 3,
};

enum class CCHIOpcodeREQ {
    StashShared     = 0x0,
    ReadUnique      = 0x10
};

enum class CCHIOpcodeRSP {
    CompStash       = 0,
    Comp            = 1,
    DBIDResp        = 2,
    CompDBIDResp    = 3
};

enum class CCHIOpcodeDAT_UP {
    CompData       = 0
};

enum class CCHIOpcodeDAT_DOWN {
    NonCopyBackWrData   = 0,
    NCBWrDataCompAck    = 1,
    CopyBackWrData      = 2,
    SnpRespData         = 3
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
        uint8_t     dbID;
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
        uint8_t     data[32];
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