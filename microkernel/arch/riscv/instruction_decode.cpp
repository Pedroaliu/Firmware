#include "microkernel/arch/riscv/instruction_decode.h"

namespace jixia::arch::riscv {
namespace {

[[nodiscard]]
uint16_t load_little_endian_u16(Xlen address)
{
    const auto* bytes =
        reinterpret_cast<const volatile uint8_t*>(address);

    return static_cast<uint16_t>(bytes[0]) |
        static_cast<uint16_t>(
            static_cast<uint16_t>(bytes[1]) << 8U);
}

} // namespace

BreakpointDecode decode_breakpoint_at(Xlen pc)
{
    const uint16_t first_parcel = load_little_endian_u16(pc);
    const InstructionLength length =
        decode_instruction_length(first_parcel);

    if (length == InstructionLength::bytes_2)
    {
        return {
            first_parcel == compressed_ebreak_encoding
                ? BreakpointInstruction::compressed
                : BreakpointInstruction::none,
            length,
        };
    }

    if (length == InstructionLength::bytes_4)
    {
        const uint16_t second_parcel =
            load_little_endian_u16(pc + 2U);

        const uint32_t instruction =
            static_cast<uint32_t>(first_parcel) |
            (static_cast<uint32_t>(second_parcel) << 16U);

        return {
            instruction == standard_ebreak_encoding
                ? BreakpointInstruction::standard
                : BreakpointInstruction::none,
            length,
        };
    }

    return {
        BreakpointInstruction::none,
        InstructionLength::unsupported,
    };
}

} // namespace jixia::arch::riscv
