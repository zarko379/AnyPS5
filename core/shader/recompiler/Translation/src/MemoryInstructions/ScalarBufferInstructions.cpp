#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

MemoryInfo scalarMemoryInfoFromInstruction(const RdnaInstruction& inst, bool raw) {
    if (inst.family != RdnaInstructionFamily::SMEM) {
        throw std::runtime_error("scalarMemoryInfoFromInstruction requires an SMEM instruction");
    }
    // Raw address loads may take their 64-bit base from VCC; descriptor loads need four SGPRs.
    if (inst.source0.kind != RdnaOperandKind::ScalarRegister && !(raw && inst.source0.kind == RdnaOperandKind::VccLo)) {
        throw std::runtime_error("scalar memory base must be a scalar register");
    }
    MemoryInfo memory;
    memory.kind = raw ? ResourceKind::ScalarAddress : ResourceKind::ScalarBuffer;
    memory.offset = inst.memoryOffset;
    memory.dataDwords = inst.dataDwordCount;
    memory.componentCount = inst.dataDwordCount;
    if (!raw) {
        memory.resource = inst.source0.reg / 4u;
    }
    return memory;
}

const char* scalarMemoryWriteName(RdnaOpcode opcode) {
    switch (opcode) {
    case RdnaOpcode::SStoreDword: return "s_store_dword";
    case RdnaOpcode::SStoreDwordx2: return "s_store_dwordx2";
    case RdnaOpcode::SStoreDwordx4: return "s_store_dwordx4";
    case RdnaOpcode::SScratchStoreDword: return "s_scratch_store_dword";
    case RdnaOpcode::SScratchStoreDwordx2: return "s_scratch_store_dwordx2";
    case RdnaOpcode::SScratchStoreDwordx4: return "s_scratch_store_dwordx4";
    case RdnaOpcode::SBufferStoreDword: return "s_buffer_store_dword";
    case RdnaOpcode::SBufferStoreDwordx2: return "s_buffer_store_dwordx2";
    case RdnaOpcode::SBufferStoreDwordx4: return "s_buffer_store_dwordx4";
    case RdnaOpcode::SBufferAtomicSwap: return "s_buffer_atomic_swap";
    case RdnaOpcode::SBufferAtomicCmpswap: return "s_buffer_atomic_cmpswap";
    case RdnaOpcode::SBufferAtomicAdd: return "s_buffer_atomic_add";
    case RdnaOpcode::SBufferAtomicSub: return "s_buffer_atomic_sub";
    case RdnaOpcode::SBufferAtomicSmin: return "s_buffer_atomic_smin";
    case RdnaOpcode::SBufferAtomicUmin: return "s_buffer_atomic_umin";
    case RdnaOpcode::SBufferAtomicSmax: return "s_buffer_atomic_smax";
    case RdnaOpcode::SBufferAtomicUmax: return "s_buffer_atomic_umax";
    case RdnaOpcode::SBufferAtomicAnd: return "s_buffer_atomic_and";
    case RdnaOpcode::SBufferAtomicOr: return "s_buffer_atomic_or";
    case RdnaOpcode::SBufferAtomicXor: return "s_buffer_atomic_xor";
    case RdnaOpcode::SBufferAtomicInc: return "s_buffer_atomic_inc";
    case RdnaOpcode::SBufferAtomicDec: return "s_buffer_atomic_dec";
    case RdnaOpcode::SBufferAtomicSwapX2: return "s_buffer_atomic_swap_x2";
    case RdnaOpcode::SBufferAtomicCmpswapX2: return "s_buffer_atomic_cmpswap_x2";
    case RdnaOpcode::SBufferAtomicAddX2: return "s_buffer_atomic_add_x2";
    case RdnaOpcode::SBufferAtomicSubX2: return "s_buffer_atomic_sub_x2";
    case RdnaOpcode::SBufferAtomicSminX2: return "s_buffer_atomic_smin_x2";
    case RdnaOpcode::SBufferAtomicUminX2: return "s_buffer_atomic_umin_x2";
    case RdnaOpcode::SBufferAtomicSmaxX2: return "s_buffer_atomic_smax_x2";
    case RdnaOpcode::SBufferAtomicUmaxX2: return "s_buffer_atomic_umax_x2";
    case RdnaOpcode::SBufferAtomicAndX2: return "s_buffer_atomic_and_x2";
    case RdnaOpcode::SBufferAtomicOrX2: return "s_buffer_atomic_or_x2";
    case RdnaOpcode::SBufferAtomicXorX2: return "s_buffer_atomic_xor_x2";
    case RdnaOpcode::SBufferAtomicIncX2: return "s_buffer_atomic_inc_x2";
    case RdnaOpcode::SBufferAtomicDecX2: return "s_buffer_atomic_dec_x2";
    case RdnaOpcode::SAtomicSwap: return "s_atomic_swap";
    case RdnaOpcode::SAtomicCmpswap: return "s_atomic_cmpswap";
    case RdnaOpcode::SAtomicAdd: return "s_atomic_add";
    case RdnaOpcode::SAtomicSub: return "s_atomic_sub";
    case RdnaOpcode::SAtomicSmin: return "s_atomic_smin";
    case RdnaOpcode::SAtomicUmin: return "s_atomic_umin";
    case RdnaOpcode::SAtomicSmax: return "s_atomic_smax";
    case RdnaOpcode::SAtomicUmax: return "s_atomic_umax";
    case RdnaOpcode::SAtomicAnd: return "s_atomic_and";
    case RdnaOpcode::SAtomicOr: return "s_atomic_or";
    case RdnaOpcode::SAtomicXor: return "s_atomic_xor";
    case RdnaOpcode::SAtomicInc: return "s_atomic_inc";
    case RdnaOpcode::SAtomicDec: return "s_atomic_dec";
    case RdnaOpcode::SAtomicSwapX2: return "s_atomic_swap_x2";
    case RdnaOpcode::SAtomicCmpswapX2: return "s_atomic_cmpswap_x2";
    case RdnaOpcode::SAtomicAddX2: return "s_atomic_add_x2";
    case RdnaOpcode::SAtomicSubX2: return "s_atomic_sub_x2";
    case RdnaOpcode::SAtomicSminX2: return "s_atomic_smin_x2";
    case RdnaOpcode::SAtomicUminX2: return "s_atomic_umin_x2";
    case RdnaOpcode::SAtomicSmaxX2: return "s_atomic_smax_x2";
    case RdnaOpcode::SAtomicUmaxX2: return "s_atomic_umax_x2";
    case RdnaOpcode::SAtomicAndX2: return "s_atomic_and_x2";
    case RdnaOpcode::SAtomicOrX2: return "s_atomic_or_x2";
    case RdnaOpcode::SAtomicXorX2: return "s_atomic_xor_x2";
    case RdnaOpcode::SAtomicIncX2: return "s_atomic_inc_x2";
    case RdnaOpcode::SAtomicDecX2: return "s_atomic_dec_x2";
    default: throw std::logic_error("opcode is not a scalar memory store or atomic");
    }
}

}

bool TranslationContext::sLoad(const RdnaInstruction& inst, bool raw) {
    const MemoryInfo memory = scalarMemoryInfoFromInstruction(inst, raw);
    const std::uint32_t baseCode = inst.source0.kind == RdnaOperandKind::VccLo ? 106u : inst.source0.reg;
    IrValue* resource = raw ? getScalarAddressResource(baseCode) : getBufferResource(memory);
    const IrU32 offset = readU32(inst.source1);
    std::array<IrValue*, 16u> loaded{};
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        MemoryInfo scalar = memory;
        scalar.offset += component * 4u;
        scalar.dataDwords = 1u;
        scalar.componentIndex = component;
        const MemoryFlags flags = addMemoryInfo(scalar, inst.programCounter);
        if (raw) {
            loaded[component] = &ir.Emit(IrOpcode::LoadAddressU32, IrOpcodeType(IrOpcode::LoadAddressU32), {resource, &offset.Value(), &ir.Constant(0u), &ir.ConstantBool(true)}, flags);
        } else {
            loaded[component] = &ir.Emit(IrOpcode::ReadConstBuffer, IrOpcodeType(IrOpcode::ReadConstBuffer), {resource, &offset.Value()}, flags);
        }
    }
    for (std::uint32_t component = 0u; component < memory.dataDwords; ++component) {
        writeOperand(scalarDestinationOperand(inst.destination, component), loaded[component]);
    }
    return true;
}

bool TranslationContext::sScratchLoad(const RdnaInstruction& /*inst*/) {
    throw std::runtime_error("s_scratch_load not yet implemented: wave-uniform or swizzled scratch layout required");
}

bool TranslationContext::sMemoryWrite(const RdnaInstruction& inst) {
    const std::string location = std::string(scalarMemoryWriteName(inst.op)) + " at pc " + std::to_string(inst.programCounter);
    if (inst.opcodeId >= 0x40u) {
        throw std::runtime_error(location + ": scalar memory atomics are not modeled: they act once per wave whatever EXEC holds, and the translation has no single-invocation election for them");
    }
    if (inst.opcodeId >= 0x15u && inst.opcodeId <= 0x17u) {
        throw std::runtime_error(location + ": scalar scratch stores are not modeled: wave-uniform or swizzled scratch layout required");
    }
    throw std::runtime_error(location + ": scalar memory stores are not modeled: scalar loads are resolved as reads of unchanging memory, and the scalar data cache write-back is not modeled");
}

void TranslationContext::TranslateCodeTableLoad(const RdnaInstruction& instruction, const ControlFlowGraph::CodeTableLoad& table) {
    if (table.values.empty()) throw std::runtime_error("empty shader code table");
    const IrU32 index = readRawU32(instruction.source1);
    IrU32 low(ir.Constant(static_cast<std::uint32_t>(table.values.back())));
    IrU32 high(ir.Constant(static_cast<std::uint32_t>(table.values.back() >> 32u)));
    for (std::size_t entry = table.values.size() - 1u; entry-- > 0u;) {
        auto& matches = ir.IEqual(index.Value(), ir.Constant(static_cast<std::uint32_t>(entry * instruction.dataDwordCount * 4u)));
        low = IrU32(ir.Select(matches, ir.Constant(static_cast<std::uint32_t>(table.values[entry])), low.Value()));
        if (instruction.dataDwordCount == 2u) high = IrU32(ir.Select(matches, ir.Constant(static_cast<std::uint32_t>(table.values[entry] >> 32u)), high.Value()));
    }
    if (instruction.dataDwordCount == 1u) {
        writeRawU32(instruction.destination, low);
        return;
    }
    writeU32Pair(instruction.destination, {low, high});
}

}