#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include "IntermediateRepresentation/IrProgram.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace ShaderRecompiler;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Requirement failed: %s\n", message);
        std::abort();
    }
}

template <typename T, typename U>
void ExpectEq(const T& actual, const U& expected, const char* message) {
    if (actual != expected) {
        std::fprintf(stderr, "ExpectEq failed: %s\n", message);
        std::abort();
    }
}
#define EXPECT_EQ(a, b) ExpectEq((a), (b), #a " == " #b)

void ExpectThrows(const std::array<std::uint32_t, 2>& words, const char* expectedMessage) {
    bool threw = false;
    try {
        (void)DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
    } catch (const std::runtime_error& e) {
        threw = true;
        EXPECT_EQ(std::string_view(e.what()), std::string_view(expectedMessage));
    }
    EXPECT_EQ(threw, true);
}

RdnaInstruction DecodeWords(std::uint32_t word0, std::uint32_t word1) {
    const std::array<std::uint32_t, 2> words = {word0, word1};
    return DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);
}

void ExpectTranslationThrows(const RdnaInstruction& inst, const std::string& expectedMessage) {
    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(inst);
    } catch (const std::runtime_error& e) {
        threw = true;
        EXPECT_EQ(std::string(e.what()), expectedMessage);
    }
    EXPECT_EQ(threw, true);
}

void TestDsSrc2B64() {
    struct Form {
        std::uint32_t opcode;
        RdnaOpcode op;
        const char* name;
    };
    constexpr std::array<Form, 15> forms = {{
        {0xC0u, RdnaOpcode::DsAddSrc2U64, "ds_add_src2_u64"},
        {0xC1u, RdnaOpcode::DsSubSrc2U64, "ds_sub_src2_u64"},
        {0xC2u, RdnaOpcode::DsRsubSrc2U64, "ds_rsub_src2_u64"},
        {0xC3u, RdnaOpcode::DsIncSrc2U64, "ds_inc_src2_u64"},
        {0xC4u, RdnaOpcode::DsDecSrc2U64, "ds_dec_src2_u64"},
        {0xC5u, RdnaOpcode::DsMinSrc2I64, "ds_min_src2_i64"},
        {0xC6u, RdnaOpcode::DsMaxSrc2I64, "ds_max_src2_i64"},
        {0xC7u, RdnaOpcode::DsMinSrc2U64, "ds_min_src2_u64"},
        {0xC8u, RdnaOpcode::DsMaxSrc2U64, "ds_max_src2_u64"},
        {0xC9u, RdnaOpcode::DsAndSrc2B64, "ds_and_src2_b64"},
        {0xCAu, RdnaOpcode::DsOrSrc2B64, "ds_or_src2_b64"},
        {0xCBu, RdnaOpcode::DsXorSrc2B64, "ds_xor_src2_b64"},
        {0xCDu, RdnaOpcode::DsWriteSrc2B64, "ds_write_src2_b64"},
        {0xD2u, RdnaOpcode::DsMinSrc2F64, "ds_min_src2_f64"},
        {0xD3u, RdnaOpcode::DsMaxSrc2F64, "ds_max_src2_f64"},
    }};
    for (const auto& form : forms) {
        for (const std::uint32_t gds : {0u, 1u}) {
            const std::uint32_t word0 = 0xD8000000u | (form.opcode << 18u) | (gds << 17u) | (0x7Fu << 8u) | 0xF8u;
            const std::uint32_t word1 = (9u << 24u) | (8u << 16u) | (7u << 8u) | 6u;
            const RdnaInstruction inst = DecodeWords(word0, word1);
            EXPECT_EQ(inst.family, RdnaInstructionFamily::DS);
            EXPECT_EQ(inst.op, form.op);
            EXPECT_EQ(inst.opcodeId, form.opcode);
            EXPECT_EQ(inst.gds, gds != 0u);
            EXPECT_EQ(inst.dataDwordCount, 2u);
            EXPECT_EQ(inst.sourceCount, 1u);
            EXPECT_EQ(inst.memoryOffset, 0x7FF8u);
            EXPECT_EQ(inst.secondaryOffset, 0u);
            EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
            EXPECT_EQ(inst.source0.reg, 6u);
            EXPECT_EQ(inst.destination.kind, RdnaOperandKind::None);
            ExpectTranslationThrows(inst, std::string(form.name) + " at pc 0: 64-bit DS src2 operations are not supported, because no public source says whether the offset of B is scaled by 4 or by 8");
        }
        ExpectThrows({0xD8000000u | (form.opcode << 18u) | (0x80u << 8u), 6u}, "DS src2 operation with the offset taken from the address is not supported");
    }
}

void TestScalarMemoryStoresAndAtomics() {
    enum class ScalarWrite { Store, Scratch, Atomic };
    struct Form {
        std::uint32_t opcode;
        RdnaOpcode op;
        const char* name;
        std::uint32_t dwords;
        ScalarWrite kind;
    };
    constexpr std::array<Form, 61> forms = {{
        {0x10u, RdnaOpcode::SStoreDword, "s_store_dword", 1u, ScalarWrite::Store},
        {0x11u, RdnaOpcode::SStoreDwordx2, "s_store_dwordx2", 2u, ScalarWrite::Store},
        {0x12u, RdnaOpcode::SStoreDwordx4, "s_store_dwordx4", 4u, ScalarWrite::Store},
        {0x15u, RdnaOpcode::SScratchStoreDword, "s_scratch_store_dword", 1u, ScalarWrite::Scratch},
        {0x16u, RdnaOpcode::SScratchStoreDwordx2, "s_scratch_store_dwordx2", 2u, ScalarWrite::Scratch},
        {0x17u, RdnaOpcode::SScratchStoreDwordx4, "s_scratch_store_dwordx4", 4u, ScalarWrite::Scratch},
        {0x18u, RdnaOpcode::SBufferStoreDword, "s_buffer_store_dword", 1u, ScalarWrite::Store},
        {0x19u, RdnaOpcode::SBufferStoreDwordx2, "s_buffer_store_dwordx2", 2u, ScalarWrite::Store},
        {0x1Au, RdnaOpcode::SBufferStoreDwordx4, "s_buffer_store_dwordx4", 4u, ScalarWrite::Store},
        {0x40u, RdnaOpcode::SBufferAtomicSwap, "s_buffer_atomic_swap", 1u, ScalarWrite::Atomic},
        {0x41u, RdnaOpcode::SBufferAtomicCmpswap, "s_buffer_atomic_cmpswap", 2u, ScalarWrite::Atomic},
        {0x42u, RdnaOpcode::SBufferAtomicAdd, "s_buffer_atomic_add", 1u, ScalarWrite::Atomic},
        {0x43u, RdnaOpcode::SBufferAtomicSub, "s_buffer_atomic_sub", 1u, ScalarWrite::Atomic},
        {0x44u, RdnaOpcode::SBufferAtomicSmin, "s_buffer_atomic_smin", 1u, ScalarWrite::Atomic},
        {0x45u, RdnaOpcode::SBufferAtomicUmin, "s_buffer_atomic_umin", 1u, ScalarWrite::Atomic},
        {0x46u, RdnaOpcode::SBufferAtomicSmax, "s_buffer_atomic_smax", 1u, ScalarWrite::Atomic},
        {0x47u, RdnaOpcode::SBufferAtomicUmax, "s_buffer_atomic_umax", 1u, ScalarWrite::Atomic},
        {0x48u, RdnaOpcode::SBufferAtomicAnd, "s_buffer_atomic_and", 1u, ScalarWrite::Atomic},
        {0x49u, RdnaOpcode::SBufferAtomicOr, "s_buffer_atomic_or", 1u, ScalarWrite::Atomic},
        {0x4Au, RdnaOpcode::SBufferAtomicXor, "s_buffer_atomic_xor", 1u, ScalarWrite::Atomic},
        {0x4Bu, RdnaOpcode::SBufferAtomicInc, "s_buffer_atomic_inc", 1u, ScalarWrite::Atomic},
        {0x4Cu, RdnaOpcode::SBufferAtomicDec, "s_buffer_atomic_dec", 1u, ScalarWrite::Atomic},
        {0x60u, RdnaOpcode::SBufferAtomicSwapX2, "s_buffer_atomic_swap_x2", 2u, ScalarWrite::Atomic},
        {0x61u, RdnaOpcode::SBufferAtomicCmpswapX2, "s_buffer_atomic_cmpswap_x2", 4u, ScalarWrite::Atomic},
        {0x62u, RdnaOpcode::SBufferAtomicAddX2, "s_buffer_atomic_add_x2", 2u, ScalarWrite::Atomic},
        {0x63u, RdnaOpcode::SBufferAtomicSubX2, "s_buffer_atomic_sub_x2", 2u, ScalarWrite::Atomic},
        {0x64u, RdnaOpcode::SBufferAtomicSminX2, "s_buffer_atomic_smin_x2", 2u, ScalarWrite::Atomic},
        {0x65u, RdnaOpcode::SBufferAtomicUminX2, "s_buffer_atomic_umin_x2", 2u, ScalarWrite::Atomic},
        {0x66u, RdnaOpcode::SBufferAtomicSmaxX2, "s_buffer_atomic_smax_x2", 2u, ScalarWrite::Atomic},
        {0x67u, RdnaOpcode::SBufferAtomicUmaxX2, "s_buffer_atomic_umax_x2", 2u, ScalarWrite::Atomic},
        {0x68u, RdnaOpcode::SBufferAtomicAndX2, "s_buffer_atomic_and_x2", 2u, ScalarWrite::Atomic},
        {0x69u, RdnaOpcode::SBufferAtomicOrX2, "s_buffer_atomic_or_x2", 2u, ScalarWrite::Atomic},
        {0x6Au, RdnaOpcode::SBufferAtomicXorX2, "s_buffer_atomic_xor_x2", 2u, ScalarWrite::Atomic},
        {0x6Bu, RdnaOpcode::SBufferAtomicIncX2, "s_buffer_atomic_inc_x2", 2u, ScalarWrite::Atomic},
        {0x6Cu, RdnaOpcode::SBufferAtomicDecX2, "s_buffer_atomic_dec_x2", 2u, ScalarWrite::Atomic},
        {0x80u, RdnaOpcode::SAtomicSwap, "s_atomic_swap", 1u, ScalarWrite::Atomic},
        {0x81u, RdnaOpcode::SAtomicCmpswap, "s_atomic_cmpswap", 2u, ScalarWrite::Atomic},
        {0x82u, RdnaOpcode::SAtomicAdd, "s_atomic_add", 1u, ScalarWrite::Atomic},
        {0x83u, RdnaOpcode::SAtomicSub, "s_atomic_sub", 1u, ScalarWrite::Atomic},
        {0x84u, RdnaOpcode::SAtomicSmin, "s_atomic_smin", 1u, ScalarWrite::Atomic},
        {0x85u, RdnaOpcode::SAtomicUmin, "s_atomic_umin", 1u, ScalarWrite::Atomic},
        {0x86u, RdnaOpcode::SAtomicSmax, "s_atomic_smax", 1u, ScalarWrite::Atomic},
        {0x87u, RdnaOpcode::SAtomicUmax, "s_atomic_umax", 1u, ScalarWrite::Atomic},
        {0x88u, RdnaOpcode::SAtomicAnd, "s_atomic_and", 1u, ScalarWrite::Atomic},
        {0x89u, RdnaOpcode::SAtomicOr, "s_atomic_or", 1u, ScalarWrite::Atomic},
        {0x8Au, RdnaOpcode::SAtomicXor, "s_atomic_xor", 1u, ScalarWrite::Atomic},
        {0x8Bu, RdnaOpcode::SAtomicInc, "s_atomic_inc", 1u, ScalarWrite::Atomic},
        {0x8Cu, RdnaOpcode::SAtomicDec, "s_atomic_dec", 1u, ScalarWrite::Atomic},
        {0xA0u, RdnaOpcode::SAtomicSwapX2, "s_atomic_swap_x2", 2u, ScalarWrite::Atomic},
        {0xA1u, RdnaOpcode::SAtomicCmpswapX2, "s_atomic_cmpswap_x2", 4u, ScalarWrite::Atomic},
        {0xA2u, RdnaOpcode::SAtomicAddX2, "s_atomic_add_x2", 2u, ScalarWrite::Atomic},
        {0xA3u, RdnaOpcode::SAtomicSubX2, "s_atomic_sub_x2", 2u, ScalarWrite::Atomic},
        {0xA4u, RdnaOpcode::SAtomicSminX2, "s_atomic_smin_x2", 2u, ScalarWrite::Atomic},
        {0xA5u, RdnaOpcode::SAtomicUminX2, "s_atomic_umin_x2", 2u, ScalarWrite::Atomic},
        {0xA6u, RdnaOpcode::SAtomicSmaxX2, "s_atomic_smax_x2", 2u, ScalarWrite::Atomic},
        {0xA7u, RdnaOpcode::SAtomicUmaxX2, "s_atomic_umax_x2", 2u, ScalarWrite::Atomic},
        {0xA8u, RdnaOpcode::SAtomicAndX2, "s_atomic_and_x2", 2u, ScalarWrite::Atomic},
        {0xA9u, RdnaOpcode::SAtomicOrX2, "s_atomic_or_x2", 2u, ScalarWrite::Atomic},
        {0xAAu, RdnaOpcode::SAtomicXorX2, "s_atomic_xor_x2", 2u, ScalarWrite::Atomic},
        {0xABu, RdnaOpcode::SAtomicIncX2, "s_atomic_inc_x2", 2u, ScalarWrite::Atomic},
        {0xACu, RdnaOpcode::SAtomicDecX2, "s_atomic_dec_x2", 2u, ScalarWrite::Atomic},
    }};
    for (const auto& form : forms) {
        for (const std::uint32_t glc : {0u, 1u}) {
            const std::uint32_t word0 = 0xF4000000u | (form.opcode << 18u) | (glc << 16u) | (8u << 6u) | 2u;
            const std::uint32_t word1 = (0x7Du << 25u) | 0x1FFFF8u;
            const RdnaInstruction inst = DecodeWords(word0, word1);
            EXPECT_EQ(inst.family, RdnaInstructionFamily::SMEM);
            EXPECT_EQ(inst.op, form.op);
            EXPECT_EQ(inst.opcodeId, form.opcode);
            EXPECT_EQ(inst.dataDwordCount, form.dwords);
            EXPECT_EQ(inst.glc, glc != 0u);
            EXPECT_EQ(inst.memoryOffset, 0xFFFFFFF8u);
            EXPECT_EQ(inst.sourceCount, 3u);
            EXPECT_EQ(inst.source0.kind, RdnaOperandKind::ScalarRegister);
            EXPECT_EQ(inst.source0.reg, 4u);
            EXPECT_EQ(inst.source1.kind, RdnaOperandKind::Null);
            EXPECT_EQ(inst.source2.kind, RdnaOperandKind::ScalarRegister);
            EXPECT_EQ(inst.source2.reg, 8u);
            if (form.kind == ScalarWrite::Atomic && glc != 0u) {
                EXPECT_EQ(inst.destination.kind, RdnaOperandKind::ScalarRegister);
                EXPECT_EQ(inst.destination.reg, 8u);
            } else {
                EXPECT_EQ(inst.destination.kind, RdnaOperandKind::None);
            }
            const std::string location = std::string(form.name) + " at pc 0: ";
            switch (form.kind) {
            case ScalarWrite::Store:
                ExpectTranslationThrows(inst, location + "scalar memory stores are not supported: scalar loads are resolved as reads of unchanging memory, and the scalar data cache write-back is not modeled");
                break;
            case ScalarWrite::Scratch:
                ExpectTranslationThrows(inst, location + "scalar scratch stores are not supported: wave-uniform or swizzled scratch layout required");
                break;
            case ScalarWrite::Atomic:
                ExpectTranslationThrows(inst, location + "scalar memory atomics are not supported: they act once per wave whatever EXEC holds, and the translation has no single-invocation election for them");
                break;
            }
        }
        const std::uint32_t lastFit = 106u - form.dwords;
        const RdnaInstruction high = DecodeWords(0xF4000000u | (form.opcode << 18u) | (lastFit << 6u) | 2u, (1u << 25u) | 0x10u);
        EXPECT_EQ(high.source2.reg, lastFit);
        EXPECT_EQ(high.source1.kind, RdnaOperandKind::ScalarRegister);
        EXPECT_EQ(high.source1.reg, 1u);
        EXPECT_EQ(high.memoryOffset, 0x10u);
        if (form.dwords > 1u) {
            ExpectThrows({0xF4000000u | (form.opcode << 18u) | ((lastFit + 1u) << 6u) | 2u, 0xFA000000u}, "SMEM data register range overflow");
        }
        const RdnaInstruction vccBase = DecodeWords(0xF4000000u | (form.opcode << 18u) | (8u << 6u) | 53u, 0xFA000000u);
        EXPECT_EQ(vccBase.source0.kind, RdnaOperandKind::VccLo);
        if (form.dwords <= 2u) {
            const RdnaInstruction vcc = DecodeWords(0xF4000000u | (form.opcode << 18u) | (1u << 16u) | (106u << 6u) | 2u, 0xFA000000u);
            EXPECT_EQ(vcc.source2.kind, RdnaOperandKind::VccLo);
            EXPECT_EQ(vcc.destination.kind, form.kind == ScalarWrite::Atomic ? RdnaOperandKind::VccLo : RdnaOperandKind::None);
        } else {
            ExpectThrows({0xF4000000u | (form.opcode << 18u) | (106u << 6u) | 2u, 0xFA000000u}, "SMEM data register range overflow");
        }
        if (form.dwords == 1u) {
            EXPECT_EQ(DecodeWords(0xF4000000u | (form.opcode << 18u) | (107u << 6u) | 2u, 0xFA000000u).source2.kind, RdnaOperandKind::VccHi);
        } else {
            ExpectThrows({0xF4000000u | (form.opcode << 18u) | (107u << 6u) | 2u, 0xFA000000u}, "SMEM data register range overflow");
        }
        ExpectThrows({0xF4000000u | (form.opcode << 18u) | (124u << 6u) | 2u, 0xFA000000u}, "SMEM data register range overflow");
    }
    const RdnaInstruction load = DecodeWords(0xF4010202u, 0xFA000010u);
    EXPECT_EQ(load.op, RdnaOpcode::SLoadDword);
    EXPECT_EQ(load.sourceCount, 2u);
    EXPECT_EQ(load.destination.kind, RdnaOperandKind::ScalarRegister);
    EXPECT_EQ(load.destination.reg, 8u);
}

void TestDsWriteAddtidTolerance() {
    constexpr std::array<std::uint32_t, 3> data1Values = {0u, 42u, 255u};
    for (const std::uint32_t data1 : data1Values) {
        const std::uint32_t word0 = 0xDAC00000u;
        const std::uint32_t word1 = (data1 << 16) | (5u << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::DS);
        EXPECT_EQ(inst.op, RdnaOpcode::DsWriteAddtidB32);
        EXPECT_EQ(inst.sourceCount, 1u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source1.reg, 5u);
        EXPECT_EQ(inst.source2.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source2.reg, data1);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundLaneId = false;
        bool foundWriteShared = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::LaneId) {
                foundLaneId = true;
            }
            if (val->Opcode() == IrOpcode::WriteSharedU32) {
                foundWriteShared = true;
            }
        }
        EXPECT_EQ(foundLaneId, true);
        EXPECT_EQ(foundWriteShared, true);
    }
}

void TestDsReadAddtidTolerance() {
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 4> dataPairs = {{
        {0u, 0u},
        {17u, 0u},
        {0u, 88u},
        {255u, 255u}
    }};

    for (const auto& [data0, data1] : dataPairs) {
        const std::uint32_t word0 = 0xDAC40000u;
        const std::uint32_t word1 = (7u << 24) | (data1 << 16) | (data0 << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::DS);
        EXPECT_EQ(inst.op, RdnaOpcode::DsReadAddtidB32);
        EXPECT_EQ(inst.sourceCount, 0u);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.destination.reg, 7u);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundLaneId = false;
        bool foundLoadShared = false;
        bool foundSetDest = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::LaneId) {
                foundLaneId = true;
            }
            if (val->Opcode() == IrOpcode::LoadSharedU32) {
                foundLoadShared = true;
            }
            if (val->Opcode() == IrOpcode::SetVectorRegister && val->Argument(0)->Register().index == 7u) {
                foundSetDest = true;
            }
        }
        EXPECT_EQ(foundLaneId, true);
        EXPECT_EQ(foundLoadShared, true);
        EXPECT_EQ(foundSetDest, true);
    }
}

void Test64BitScalarDescriptorVccLoAndRefusals() {
    {
        const std::uint32_t word0 = 0xDC308000u;
        const std::uint32_t word1 = (1u << 24) | (106u << 16);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::FLAT);
        EXPECT_EQ(inst.op, RdnaOpcode::FlatLoadDword);
        EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source0.reg, 0u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VccLo);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundAddressResource = false;
        bool foundLoadAddress = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::GetAddressResource) {
                foundAddressResource = true;
            }
            if (val->Opcode() == IrOpcode::LoadAddressU32) {
                foundLoadAddress = true;
            }
        }
        EXPECT_EQ(foundAddressResource, true);
        EXPECT_EQ(foundLoadAddress, true);
    }

    {
        const std::uint32_t word0 = 0xDC708000u;
        const std::uint32_t word1 = (106u << 16) | (4u << 8);
        const std::array<std::uint32_t, 2> words = {word0, word1};
        const RdnaInstruction inst = DecodeRdnaMemoryOp(std::span<const std::uint32_t>(words), 0u);

        EXPECT_EQ(inst.family, RdnaInstructionFamily::FLAT);
        EXPECT_EQ(inst.op, RdnaOpcode::FlatStoreDword);
        EXPECT_EQ(inst.destination.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.destination.reg, 4u);
        EXPECT_EQ(inst.source0.kind, RdnaOperandKind::VectorRegister);
        EXPECT_EQ(inst.source0.reg, 0u);
        EXPECT_EQ(inst.source1.kind, RdnaOperandKind::VccLo);

        IrProgram program;
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        TranslationContext context(program, block, 256);
        context.TranslateInstruction(inst);

        bool foundAddressResource = false;
        bool foundStoreAddress = false;
        for (const auto* val : block.Instructions()) {
            if (val->Opcode() == IrOpcode::GetAddressResource) {
                foundAddressResource = true;
            }
            if (val->Opcode() == IrOpcode::StoreAddressU32) {
                foundStoreAddress = true;
            }
        }
        EXPECT_EQ(foundAddressResource, true);
        EXPECT_EQ(foundStoreAddress, true);
    }

    ExpectThrows({0xDC588000u, 0x056B0000u}, "global_load_dword_addtid supports only an SGPR pair as base address");
    ExpectThrows({0xDC588000u, 0x007E0000u}, "global_load_dword_addtid supports only an SGPR pair as base address");
    ExpectThrows({0xDC588000u, 0x00690000u}, "global_load_dword_addtid supports only an SGPR pair as base address");

    ExpectThrows({0xDC708000u, 0x007E0201u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDC308000u, 0x046B0001u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDCC88000u, 0x007C0201u}, "FLAT scalar address register range overflow");
    ExpectThrows({0xDC308000u, 0x00690001u}, "FLAT scalar address register range overflow");
}

}

int main() {
    TestDsWriteAddtidTolerance();
    TestDsReadAddtidTolerance();
    Test64BitScalarDescriptorVccLoAndRefusals();
    TestDsSrc2B64();
    TestScalarMemoryStoresAndAtomics();
    return 0;
}
