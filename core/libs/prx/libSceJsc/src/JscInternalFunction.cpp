// Derived from Source/JavaScriptCore/runtime/InternalFunction.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libSceJsc/include/JscRuntimeExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using Jsc::JscAbi::CallData;
using Jsc::JscAbi::CallDataType;
using Jsc::JscAbi::InternalFunction;

InternalFunction& Checked(InternalFunction* cell, const char* message) {
    if (!cell) throw std::invalid_argument(message);
    return *cell;
}

}

extern "C" {

CallData APS5_VABI _ZN3JSC16InternalFunction11getCallDataEPNS_6JSCellE(InternalFunction* cell) {
    const InternalFunction& function = Checked(cell, "JSC::InternalFunction::getCallData requires a cell");
    return {CallDataType::Native, function.functionForCall, 0};
}

CallData APS5_VABI _ZN3JSC16InternalFunction16getConstructDataEPNS_6JSCellE(InternalFunction* cell) {
    const InternalFunction& function = Checked(cell, "JSC::InternalFunction::getConstructData requires a cell");
    const auto hostConstructor = reinterpret_cast<std::uintptr_t>(&_ZN3JSC29callHostFunctionAsConstructorEPNS_14JSGlobalObjectEPNS_9CallFrameE);
    if (function.functionForConstruct == hostConstructor) return {CallDataType::None, 0, 0};
    return {CallDataType::Native, function.functionForConstruct, 0};
}

}
