#include "SpirvBackend/SpirvBda.hpp"
#include "SpirvBackend/SpirvEmitterInstructions.hpp"
#include "SpirvBackend/SpirvMemory/SpirvTypes.hpp"
#include "SpirvBackend/SpirvMemory/SpirvConstants.hpp"
#include "SpirvBackend/RtProto.hpp"
#include <array>
#include <cstdlib>
#include <vector>

namespace ShaderRecompiler {

namespace {

struct Helpers {
    SpirvEmitterState& state;
    std::uint32_t u32;
    std::uint32_t u64;
    std::uint32_t f32;
    std::uint32_t boolean;

    std::uint32_t uint(std::uint32_t value) const { return ConstantU32(state, value); }
    std::uint32_t wide(std::uint64_t value) const { return BdaConstant(state, value); }
    std::uint32_t op(spv::Op code, std::uint32_t type, std::uint32_t lhs, std::uint32_t rhs) const { return Binary(state, code, type, lhs, rhs); }
    std::uint32_t widen(std::uint32_t value) const { return Unary(state, spv::OpUConvert, u64, value); }

    std::uint32_t load(std::uint32_t address) const {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, address);
        const auto value = state.module.AllocateId();
        state.module.AddFunction(spv::OpLoad, u32, value, pointer, spv::MemoryAccessAlignedMask, 4u);
        return value;
    }

    std::uint32_t load64(std::uint32_t address) const {
        const auto low = widen(load(address));
        const auto high = widen(load(op(spv::OpIAdd, u64, address, wide(4u))));
        return op(spv::OpBitwiseOr, u64, low, op(spv::OpShiftLeftLogical, u64, high, wide(32u)));
    }

    void count(std::uint64_t table, std::uint32_t slot) const {
        static const bool counting = std::getenv("APS5_RT_PROTO_COUNT") != nullptr;
        if (!counting) return;
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, wide(table + 16u + 512u * 32u + slot * 4u));
        const auto old = state.module.AllocateId();
        state.module.AddFunction(spv::OpAtomicIAdd, u32, old, pointer, uint(1u), uint(0u), uint(1u));
    }

    std::uint32_t label() const {
        const auto id = state.module.AllocateId();
        EmitLabel(state, id);
        return id;
    }
};

std::uint32_t defineQuery(SpirvEmitterState& state, std::uint64_t table) {
    Helpers h{state, TypeU32(state), TypeScalarU64(state), TypeF32(state), TypeBool(state)};
    const auto result4 = TypeU32Composite(state, 4u);
    const auto vec3 = TypeF32Vector(state, 3u);
    const auto vec2 = TypeF32Vector(state, 2u);
    const auto queryType = state.module.Type(spv::OpTypeRayQueryKHR);
    const auto structureType = state.module.Type(spv::OpTypeAccelerationStructureKHR);
    const auto queryPointer = state.module.Type(spv::OpTypePointer, spv::StorageClassFunction, queryType);
    const auto function = state.module.AllocateId();
    state.module.AddName(function, "rt_proto_query");
    state.module.AddFunction(spv::OpFunction, result4, function, spv::FunctionControlInlineMask, state.module.Type(spv::OpTypeFunction, result4, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32, h.u32));
    std::array<std::uint32_t, 11> parameter{};
    for (auto& id : parameter) {
        id = state.module.AllocateId();
        state.module.AddFunction(spv::OpFunctionParameter, h.u32, id);
    }
    const auto entry = h.label();
    const auto query = state.module.AllocateId();
    state.module.AddFunction(spv::OpVariable, queryPointer, query, spv::StorageClassFunction);
    const auto base = h.wide(table);
    h.count(table, 0u);
    const auto count = h.load(base);
    const auto blas = h.op(spv::OpShiftLeftLogical, h.u64, h.op(spv::OpBitwiseOr, h.u64, h.widen(parameter[0]), h.op(spv::OpShiftLeftLogical, h.u64, h.widen(h.op(spv::OpBitwiseAnd, h.u32, parameter[1], h.uint(0x3fffffu))), h.wide(32u))), h.wide(3u));
    for (std::uint32_t slot = 0; slot < (std::getenv("APS5_RT_PROTO_COUNT") != nullptr ? 4u : 0u); ++slot) {
        const auto pointer = state.module.AllocateId();
        state.module.AddFunction(spv::OpConvertUToPtr, TypePhysicalU32Pointer(state), pointer, h.wide(table + 16u + 512u * 32u + (4u + slot) * 4u));
        const auto value = slot == 0u ? parameter[9] : slot == 1u ? parameter[10] : slot == 2u ? Unary(state, spv::OpUConvert, h.u32, blas) : Unary(state, spv::OpUConvert, h.u32, h.op(spv::OpShiftRightLogical, h.u64, blas, h.wide(32u)));
        state.module.AddFunction(spv::OpStore, pointer, value, spv::MemoryAccessAlignedMask, 4u);
    }
    const auto miss = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, result4, miss, parameter[8], h.uint(0u), h.uint(0u), h.uint(0xffffffffu));
    const auto header = state.module.AllocateId();
    const auto body = state.module.AllocateId();
    const auto continuation = state.module.AllocateId();
    const auto searched = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, header);
    const auto index = state.module.AllocateId();
    const auto match = state.module.AllocateId();
    const auto nextIndex = state.module.AllocateId();
    const auto nextMatch = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, h.u32, index, h.uint(0u), entry, nextIndex, continuation);
    state.module.AddFunction(spv::OpPhi, h.u32, match, h.uint(0xffffffffu), entry, nextMatch, continuation);
    const auto searching = h.op(spv::OpULessThan, h.boolean, index, count);
    state.module.AddFunction(spv::OpLoopMerge, searched, continuation, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, searching, body, searched);
    EmitLabel(state, body);
    const auto entryAddress = [&](std::uint32_t at) { return h.op(spv::OpIAdd, h.u64, base, h.op(spv::OpIAdd, h.u64, h.wide(16u), h.op(spv::OpIMul, h.u64, h.widen(at), h.wide(32u)))); };
    const auto guest = h.load64(entryAddress(index));
    state.module.AddFunction(spv::OpSelect, h.u32, nextMatch, h.op(spv::OpIEqual, h.boolean, guest, blas), index, match);
    state.module.AddFunction(spv::OpIAdd, h.u32, nextIndex, index, h.uint(1u));
    state.module.AddFunction(spv::OpBranch, continuation);
    EmitLabel(state, continuation);
    state.module.AddFunction(spv::OpBranch, header);
    EmitLabel(state, searched);
    const auto found = h.op(spv::OpINotEqual, h.boolean, match, h.uint(0xffffffffu));
    const auto traced = state.module.AllocateId();
    const auto joined = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, joined, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, found, traced, joined);
    EmitLabel(state, traced);
    h.count(table, 1u);
    const auto record = entryAddress(match);
    const auto top = h.load64(h.op(spv::OpIAdd, h.u64, record, h.wide(8u)));
    const auto side = h.load64(h.op(spv::OpIAdd, h.u64, record, h.wide(16u)));
    const auto opaque = h.load(h.op(spv::OpIAdd, h.u64, record, h.wide(24u)));
    const auto structure = state.module.AllocateId();
    state.module.AddFunction(spv::OpConvertUToAccelerationStructureKHR, structureType, structure, top);
    const auto asFloat = [&](std::uint32_t value) { return Unary(state, spv::OpBitcast, h.f32, value); };
    const auto asUint = [&](std::uint32_t value) { return Unary(state, spv::OpBitcast, h.u32, value); };
    const auto origin = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, vec3, origin, asFloat(parameter[2]), asFloat(parameter[3]), asFloat(parameter[4]));
    const auto direction = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, vec3, direction, asFloat(parameter[5]), asFloat(parameter[6]), asFloat(parameter[7]));
    state.module.AddFunction(spv::OpRayQueryInitializeKHR, query, structure, h.uint(1u), h.uint(0xffu), origin, ConstantF32Value(state, 0.0f), direction, asFloat(parameter[8]));
    const auto proceedHeader = state.module.AllocateId();
    const auto proceedBody = state.module.AllocateId();
    const auto proceedContinue = state.module.AllocateId();
    const auto proceeded = state.module.AllocateId();
    state.module.AddFunction(spv::OpBranch, proceedHeader);
    EmitLabel(state, proceedHeader);
    state.module.AddFunction(spv::OpLoopMerge, proceeded, proceedContinue, spv::LoopControlMaskNone);
    state.module.AddFunction(spv::OpBranch, proceedBody);
    EmitLabel(state, proceedBody);
    const auto more = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryProceedKHR, h.boolean, more, query);
    state.module.AddFunction(spv::OpBranchConditional, more, proceedContinue, proceeded);
    EmitLabel(state, proceedContinue);
    state.module.AddFunction(spv::OpBranch, proceedHeader);
    EmitLabel(state, proceeded);
    const auto committed = h.uint(1u);
    const auto kind = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryGetIntersectionTypeKHR, h.u32, kind, query, committed);
    const auto isTriangle = h.op(spv::OpIEqual, h.boolean, kind, h.uint(1u));
    {
        const auto counted = state.module.AllocateId();
        const auto after = state.module.AllocateId();
        state.module.AddFunction(spv::OpSelectionMerge, after, spv::SelectionControlMaskNone);
        state.module.AddFunction(spv::OpBranchConditional, isTriangle, counted, after);
        EmitLabel(state, counted);
        h.count(table, 2u);
        state.module.AddFunction(spv::OpBranch, after);
        EmitLabel(state, after);
    }
    const auto t = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryGetIntersectionTKHR, h.f32, t, query, committed);
    const auto primitive = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryGetIntersectionPrimitiveIndexKHR, h.u32, primitive, query, committed);
    const auto geometry = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryGetIntersectionGeometryIndexKHR, h.u32, geometry, query, committed);
    const auto barycentrics = state.module.AllocateId();
    state.module.AddFunction(spv::OpRayQueryGetIntersectionBarycentricsKHR, vec2, barycentrics, query, committed);
    const auto local = Select(state, h.u32, isTriangle, h.op(spv::OpIAdd, h.u32, primitive, Select(state, h.u32, h.op(spv::OpIEqual, h.boolean, geometry, h.uint(1u)), opaque, h.uint(0u))), h.uint(0u));
    const auto sideEntry = h.op(spv::OpIAdd, h.u64, side, h.op(spv::OpIMul, h.u64, h.widen(local), h.wide(16u)));
    const auto node = h.load(sideEntry);
    const auto triangleId = h.load(h.op(spv::OpIAdd, h.u64, sideEntry, h.wide(8u)));
    const auto shift = h.op(spv::OpIMul, h.u32, h.op(spv::OpBitwiseAnd, h.u32, node, h.uint(7u)), h.uint(8u));
    const auto selector = [&](std::uint32_t extra) { return h.op(spv::OpBitwiseAnd, h.u32, h.op(spv::OpShiftRightLogical, h.u32, triangleId, h.op(spv::OpIAdd, h.u32, shift, h.uint(extra))), h.uint(3u)); };
    const auto component = [&](std::uint32_t index) {
        const auto id = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeExtract, h.f32, id, barycentrics, index);
        return id;
    };
    const auto u = component(0u);
    const auto v = component(1u);
    const auto w = h.op(spv::OpFSub, h.f32, h.op(spv::OpFSub, h.f32, ConstantF32Value(state, 1.0f), u), v);
    const auto pick = [&](std::uint32_t chosen) {
        return Select(state, h.f32, h.op(spv::OpIEqual, h.boolean, chosen, h.uint(1u)), u, Select(state, h.f32, h.op(spv::OpIEqual, h.boolean, chosen, h.uint(2u)), v, w));
    };
    const auto sideBase = h.load64(h.op(spv::OpIAdd, h.u64, base, h.wide(8u)));
    const auto global = Unary(state, spv::OpUConvert, h.u32, h.op(spv::OpShiftRightLogical, h.u64, h.op(spv::OpISub, h.u64, sideEntry, sideBase), h.wide(4u)));
    const auto hit = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, result4, hit, Select(state, h.u32, isTriangle, asUint(t), parameter[8]), asUint(pick(selector(0u))), asUint(pick(selector(2u))), Select(state, h.u32, isTriangle, global, h.uint(0xffffffffu)));
    auto hitValue = hit;
    if (std::getenv("APS5_RT_PROTO_QUERY_ONLY") != nullptr) {
        hitValue = state.module.AllocateId();
        state.module.AddFunction(spv::OpCompositeConstruct, result4, hitValue, parameter[8], h.uint(0u), h.uint(0u), Select(state, h.u32, h.op(spv::OpIEqual, h.boolean, kind, h.uint(0xdeadbeefu)), h.uint(0u), h.uint(0xffffffffu)));
    }
    const auto tracedEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, joined);
    EmitLabel(state, joined);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, result4, value, hitValue, tracedEnd, miss, searched);
    state.module.AddFunction(spv::OpReturnValue, value);
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
}

std::uint32_t defineSide(SpirvEmitterState& state, std::uint64_t table) {
    Helpers h{state, TypeU32(state), TypeScalarU64(state), TypeF32(state), TypeBool(state)};
    const auto result2 = TypeU32Composite(state, 2u);
    const auto function = state.module.AllocateId();
    state.module.AddName(function, "rt_proto_side");
    state.module.AddFunction(spv::OpFunction, result2, function, spv::FunctionControlDontInlineMask, state.module.Type(spv::OpTypeFunction, result2, h.u32));
    const auto index = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionParameter, h.u32, index);
    const auto entry = h.label();
    const auto sideBase = h.load64(h.op(spv::OpIAdd, h.u64, h.wide(table), h.wide(8u)));
    const auto valid = h.op(spv::OpLogicalAnd, h.boolean, h.op(spv::OpINotEqual, h.boolean, index, h.uint(0xffffffffu)), h.op(spv::OpINotEqual, h.boolean, sideBase, h.wide(0u)));
    const auto none = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, result2, none, h.uint(0u), h.uint(0xffffffffu));
    const auto read = state.module.AllocateId();
    const auto joined = state.module.AllocateId();
    state.module.AddFunction(spv::OpSelectionMerge, joined, spv::SelectionControlMaskNone);
    state.module.AddFunction(spv::OpBranchConditional, valid, read, joined);
    EmitLabel(state, read);
    h.count(table, 3u);
    const auto address = h.op(spv::OpIAdd, h.u64, sideBase, h.op(spv::OpIMul, h.u64, h.widen(index), h.wide(16u)));
    const auto loaded = state.module.AllocateId();
    state.module.AddFunction(spv::OpCompositeConstruct, result2, loaded, h.load(address), h.load(h.op(spv::OpIAdd, h.u64, address, h.wide(4u))));
    const auto readEnd = state.currentLabel;
    state.module.AddFunction(spv::OpBranch, joined);
    EmitLabel(state, joined);
    const auto value = state.module.AllocateId();
    state.module.AddFunction(spv::OpPhi, result2, value, loaded, readEnd, none, entry);
    state.module.AddFunction(spv::OpReturnValue, value);
    state.module.AddFunction(spv::OpFunctionEnd);
    return function;
}

}

void DefineRtProtoFunctions(SpirvEmitterState& state) {
    bool used = false;
    for (const IrBlock* block : state.program.BlockOrder()) {
        for (const IrValue* inst : block->Instructions()) used = used || inst->Opcode() == IrOpcode::RtProtoQuery || inst->Opcode() == IrOpcode::RtProtoSide;
    }
    if (!used) return;
    const auto table = RtProto::tableAddress.load();
    if (table == 0u) throw std::runtime_error("RT prototype: the lookup table address is not set");
    state.module.EmitCapability(spv::CapabilityRayQueryKHR);
    state.module.EmitExtension("SPV_KHR_ray_query");
    state.rtProtoQueryFunction = defineQuery(state, table);
    state.rtProtoSideFunction = defineSide(state, table);
}

void EmitRtProtoQuery(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto id = state.module.AllocateId();
    std::vector<std::uint32_t> words{spv::OpFunctionCall, TypeU32Composite(state, 4u), id, state.rtProtoQueryFunction};
    for (std::uint32_t index = 0; index < 11u; ++index) words.push_back(ctx.Arg(inst, index));
    state.module.AddFunction(words);
    ctx.Define(inst, id);
}

void EmitRtProtoSide(SpirvValueEmitContext& ctx, const IrValue& inst) {
    auto& state = ctx.state;
    const auto id = state.module.AllocateId();
    state.module.AddFunction(spv::OpFunctionCall, TypeU32Composite(state, 2u), id, state.rtProtoSideFunction, ctx.Arg(inst, 0));
    ctx.Define(inst, id);
}

}
