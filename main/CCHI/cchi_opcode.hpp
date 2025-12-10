// #pragma once

// #include <array>
// #include <cstddef>
// #include <type_traits>

// #include "cchi_base.hpp"
// #include "../common/nonstdint.hpp"


// namespace CCHI {

//     // EVT Opcodes
//     using EVTOpcode = uint2_t;

//     class EVTOpcodeEnumBack : public details::Enumeration<EVTOpcodeEnumBack> {
//     public:
//         inline constexpr EVTOpcodeEnumBack(const char* name, const int value = INT_MIN, const EVTOpcodeEnumBack* prev = nullptr) noexcept
//         : Enumeration(name, value, prev) {}
//     };

//     using EVTOpcodeEnum = const EVTOpcodeEnumBack*;

//     namespace EVTOpcodes {

//         inline constexpr EVTOpcode Evict                    = 0x00;
//         //                                                  = 0x01
//         inline constexpr EVTOpcode WriteBackFull            = 0x02;
//         //                                                  = 0x03

//         namespace Enum {
//             inline constexpr EVTOpcodeEnumBack  INVALID         ("<Invalid>");

//             inline constexpr EVTOpcodeEnumBack  Evict           ("Evict"            , EVTOpcodes::Evict);
//             inline constexpr EVTOpcodeEnumBack  WriteBackFull   ("WriteBackFull"    , EVTOpcodes::WriteBackFull , Evict);
//         }

//         inline constexpr EVTOpcodeEnum ToEnum(EVTOpcode opcode) noexcept;
//         inline constexpr bool IsValid(EVTOpcode opcode) noexcept;
//     }

//     // TODO: Implement enumeration structures in the future, or use copilot to make it easier.
//     //       It's okay to do only the mininal definition of values for now as following.

//     // REQ Opcodes
//     // TODO


//     // SNP Opcodes
//     using SNPOpcode = uint2_t;

//     namespace SNPOpcodes {

//         inline constexpr SNPOpcode SnpMakeInvalid           = 0x00;
//         inline constexpr SNPOpcode SnpToInvalid             = 0x01;
//         inline constexpr SNPOpcode SnpToShared              = 0x02;
//         inline constexpr SNPOpcode SnpToClean               = 0x03;
//     }

//     // TODO


//     // DAT Opcodes of Upstream TXs
//     using TXDATOpcode = uint2_t;

//     namespace TXDATOpcodes {

//         inline constexpr TXDATOpcode NonCopyBackWrData      = 0x00;
//         //                                                  = 0x01
//         inline constexpr TXDATOpcode CopyBackWrData         = 0x02;
//         inline constexpr TXDATOpcode SnpRespData            = 0x03;
//     }


//     // DAT Opcodes of Upstream RXs
//     using RXDATOpcode = uint1_t;

//     namespace RXDATOpcodes {

//         inline constexpr RXDATOpcode CompData               = 0x00;
//         //                                                  = 0x01
//     }
// }


// Implementation of enumeration table constevals
// namespace CCHI::details {

//     template<class T, size_t Bits>
//     requires std::is_convertible_v<const T*, const Enumeration<T>*>
//     inline consteval std::array<const T*, 1 << Bits> NextElement(const T* E, std::array<const T*, 1 << Bits> A) noexcept
//     {
//         A[E->value] = E;

//         if (!E->prev)
//             return A;
//         else
//             return NextElement<T, Bits>(*E->prev, A);
//     }

//     template<class T, size_t Bits, const T* First, const T* Invalid>
//     requires std::is_convertible_v<const T*, const Enumeration<T>*>
//     inline consteval std::array<const T*, 1 << Bits> GetTable() noexcept
//     {
//         std::array<const T*, 1 << Bits> A;
//         for (auto& E : A)
//             E = Invalid;

//         return NextElement<T, Bits>(First, A);
//     }
// }


// Implementation of EVTOpcodes enumeration functions
// namespace CCHI::EVTOpcodes {

//     namespace Enum::details {
//         inline constexpr auto TABLE = 
//             CCHI::details::GetTable<EVTOpcodeEnumBack, EVTOpcode::BITS, Enum::WriteBackFull, Enum::INVALID>();
//     }
// }
