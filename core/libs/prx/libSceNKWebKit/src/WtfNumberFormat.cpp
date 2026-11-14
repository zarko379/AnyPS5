// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Derived from Source/WTF/wtf/dtoa/double-conversion.cc (WebKit c55364d53f97)

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <system_error>

#include "WtfTextAbi.hpp"

namespace {

constexpr int DecimalInShortestLow = -6;
constexpr int DecimalInShortestHigh = 21;
constexpr int MaxFixedDigitsAfterPoint = 100;
constexpr double FirstNonFixed = 1e21;
constexpr int MinPrecisionDigits = 1;
constexpr int MaxPrecisionDigits = 100;
constexpr int MaxLeadingPaddingZeroesInPrecisionMode = 6;
constexpr int MaxTrailingPaddingZeroesInPrecisionMode = 0;

std::string SpecialValue(double value) {
    if (std::isnan(value)) return "NaN";
    return value < 0 ? "-Infinity" : "Infinity";
}

void CreateDecimalRepresentation(std::string& result, const std::string& digits, int decimalPoint, int digitsAfterPoint) {
    int length = static_cast<int>(digits.size());
    if (decimalPoint <= 0) {
        result += '0';
        if (digitsAfterPoint > 0) {
            result += '.';
            result.append(-decimalPoint, '0');
            result += digits;
            result.append(digitsAfterPoint - -decimalPoint - length, '0');
        }
    } else if (decimalPoint >= length) {
        result += digits;
        result.append(decimalPoint - length, '0');
        if (digitsAfterPoint > 0) {
            result += '.';
            result.append(digitsAfterPoint, '0');
        }
    } else {
        result.append(digits, 0, decimalPoint);
        result += '.';
        result.append(digits, decimalPoint);
        result.append(digitsAfterPoint - (length - decimalPoint), '0');
    }
}

void CreateExponentialRepresentation(std::string& result, const std::string& digits, int exponent) {
    result += digits[0];
    if (digits.size() != 1) {
        result += '.';
        result.append(digits, 1);
    }
    result += 'e';
    if (exponent < 0) {
        result += '-';
        exponent = -exponent;
    } else {
        result += '+';
    }
    result += std::to_string(exponent);
}

}  // namespace

namespace NKWebKit::WtfAbi {

std::string NumberToString(float number) {
    if (!std::isfinite(number)) return SpecialValue(number);
    if (number == 0) return "0";
    char buffer[64];
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(number), std::chars_format::scientific);
    if (converted.ec != std::errc{}) throw std::runtime_error("WTF float formatting failed");
    std::string digits;
    const char* cursor = buffer;
    for (; cursor != converted.ptr && *cursor != 'e'; ++cursor) {
        if (*cursor != '.') digits += *cursor;
    }
    int exponent = 0;
    if (cursor == converted.ptr || std::from_chars(cursor + (cursor[1] == '+' ? 2 : 1), converted.ptr, exponent).ec != std::errc{})
        throw std::runtime_error("WTF float exponent formatting failed");
    std::string result = number < 0 ? "-" : "";
    int decimalPoint = exponent + 1;
    int length = static_cast<int>(digits.size());
    if (DecimalInShortestLow <= exponent && exponent < DecimalInShortestHigh)
        CreateDecimalRepresentation(result, digits, decimalPoint, length > decimalPoint ? length - decimalPoint : 0);
    else
        CreateExponentialRepresentation(result, digits, exponent);
    return result;
}

std::string NumberToFixedWidthString(double number, unsigned decimalPlaces) {
    if (!std::isfinite(number)) return SpecialValue(number);
    if (decimalPlaces > MaxFixedDigitsAfterPoint) return std::string();
    if (number >= FirstNonFixed || number <= -FirstNonFixed) return std::string();
    char buffer[1200];
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(number), std::chars_format::fixed, 1100);
    if (converted.ec != std::errc{}) throw std::runtime_error("WTF fixed formatting failed");
    std::string exact(buffer, converted.ptr);
    auto point = exact.find('.');
    std::string digits = exact.substr(0, point) + exact.substr(point + 1, decimalPlaces);
    if (exact[point + 1 + decimalPlaces] >= '5') {
        std::size_t index = digits.size();
        while (index && digits[index - 1] == '9') digits[--index] = '0';
        if (index) ++digits[index - 1];
        else digits.insert(digits.begin(), '1');
    }
    std::size_t leadingZeros = 0;
    while (leadingZeros < digits.size() && digits[leadingZeros] == '0') ++leadingZeros;
    int decimalPoint = static_cast<int>(digits.size() - decimalPlaces) - static_cast<int>(leadingZeros);
    digits.erase(0, leadingZeros);
    while (!digits.empty() && digits.back() == '0') digits.pop_back();
    if (digits.empty()) decimalPoint = -static_cast<int>(decimalPlaces);
    std::string result = std::signbit(number) && number != 0.0 ? "-" : "";
    CreateDecimalRepresentation(result, digits, decimalPoint, static_cast<int>(decimalPlaces));
    return result;
}

std::string NumberToFixedPrecisionString(double number, int precision, bool truncateTrailingZeros) {
    std::string result;
    if (!std::isfinite(number)) {
        result = SpecialValue(number);
    } else if (precision >= MinPrecisionDigits && precision <= MaxPrecisionDigits) {
        std::string digits;
        int exponent = 0;
        if (number == 0.0) {
            digits = "0";
        } else {
            char buffer[1024];
            constexpr int exactPrecision = 800;
            auto converted = std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(number), std::chars_format::scientific, exactPrecision);
            if (converted.ec != std::errc{}) throw std::runtime_error("WTF precision formatting failed");
            const char* cursor = buffer;
            for (; cursor != converted.ptr && *cursor != 'e'; ++cursor) {
                if (*cursor != '.') digits += *cursor;
            }
            if (cursor == converted.ptr || std::from_chars(cursor + (cursor[1] == '+' ? 2 : 1), converted.ptr, exponent).ec != std::errc{})
                throw std::runtime_error("WTF precision exponent formatting failed");
            std::size_t keepCount = static_cast<std::size_t>(precision);
            bool roundUp = digits.size() > keepCount && digits[keepCount] >= '5';
            digits.resize(keepCount, '0');
            if (roundUp) {
                std::size_t index = keepCount;
                while (index && digits[index - 1] == '9') digits[--index] = '0';
                if (index) {
                    ++digits[index - 1];
                } else {
                    digits[0] = '1';
                    ++exponent;
                }
            }
            while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
        }
        if (std::signbit(number) && number != 0.0) result += '-';
        int decimalPoint = exponent + 1;
        if (-decimalPoint + 1 > MaxLeadingPaddingZeroesInPrecisionMode || decimalPoint - precision > MaxTrailingPaddingZeroesInPrecisionMode) {
            digits.resize(static_cast<std::size_t>(precision), '0');
            CreateExponentialRepresentation(result, digits, decimalPoint - 1);
        } else {
            CreateDecimalRepresentation(result, digits, decimalPoint, precision - decimalPoint > 0 ? precision - decimalPoint : 0);
        }
    }
    if (truncateTrailingZeros) {
        std::size_t point = result.find('.');
        if (point != std::string::npos) {
            std::size_t pastMantissa = result.find('e', point + 1);
            if (pastMantissa == std::string::npos) pastMantissa = result.size();
            std::size_t truncatedLength = pastMantissa;
            while (truncatedLength > point + 1 && result[truncatedLength - 1] == '0') --truncatedLength;
            if (truncatedLength != pastMantissa) {
                if (truncatedLength == point + 1) truncatedLength = point;
                result.erase(truncatedLength, pastMantissa - truncatedLength);
            }
        }
    }
    return result;
}

}  // namespace NKWebKit::WtfAbi
