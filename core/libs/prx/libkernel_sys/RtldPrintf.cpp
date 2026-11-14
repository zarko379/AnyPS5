#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int RtldStdoutDescriptor = 1;
constexpr std::size_t RtldNumberBuffer = sizeof(std::intmax_t) * 8 + 1;
constexpr char RtldLowerDigits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
constexpr char RtldUpperDigits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

struct RtldArguments {
#ifdef _WIN32
    __builtin_sysv_va_list* list;
    template<class T> T Next() { return __builtin_va_arg(*list, T); }
#else
    std::va_list* list;
    template<class T> T Next() { return va_arg(*list, T); }
#endif
};

struct RtldOutput {
    char buffer[512];
    char* cursor = buffer;
    std::size_t remain = sizeof(buffer);
    int descriptor = RtldStdoutDescriptor;

    void Flush() {
        if (remain == sizeof(buffer)) return;
        write_nid_postfix(descriptor, buffer, static_cast<std::int64_t>(sizeof(buffer) - remain));
        cursor = buffer;
        remain = sizeof(buffer);
    }

    void Put(int ch) {
        if (remain > 0) {
            *cursor++ = static_cast<char>(ch);
            remain--;
        } else {
            Flush();
        }
    }
};

char* Ksprintn(char* nbuf, std::uintmax_t num, int base, int* lenp, bool upper) {
    char* p = nbuf;
    *p = '\0';
    do {
        *++p = upper ? RtldUpperDigits[num % static_cast<unsigned>(base)] : RtldLowerDigits[num % static_cast<unsigned>(base)];
    } while (num /= static_cast<unsigned>(base));
    if (lenp) *lenp = static_cast<int>(p - nbuf);
    return p;
}

enum class RtldStep { Reswitch, Done, Nosign, Sign, Number };

int Kvprintf(const char* fmt, RtldOutput& out, int radix, RtldArguments arguments) {
    char nbuf[RtldNumberBuffer];
    bool stop = false;
    const int retval = 0;
    std::uintmax_t num = 0;
    if (fmt == nullptr) fmt = "(fmt null)\n";
    if (radix < 2 || radix > 36) radix = 10;
    for (;;) {
        char padc = ' ';
        int width = 0;
        int ch = 0;
        while ((ch = static_cast<unsigned char>(*fmt++)) != '%' || stop) {
            if (ch == '\0') return retval;
            out.Put(ch);
        }
        const char* percent = fmt - 1;
        bool qflag = false, lflag = false, ladjust = false, sharpflag = false, neg = false, sign = false, dot = false, upper = false;
        bool cflag = false, hflag = false, jflag = false, tflag = false, zflag = false;
        int dwidth = 0;
        int base = 10;
        for (;;) {
            RtldStep step = RtldStep::Done;
            switch (ch = static_cast<unsigned char>(*fmt++)) {
            case '.':
                dot = true;
                step = RtldStep::Reswitch;
                break;
            case '#':
                sharpflag = true;
                step = RtldStep::Reswitch;
                break;
            case '+':
                sign = true;
                step = RtldStep::Reswitch;
                break;
            case '-':
                ladjust = true;
                step = RtldStep::Reswitch;
                break;
            case '%':
                out.Put(ch);
                break;
            case '*':
                if (!dot) {
                    width = arguments.Next<int>();
                    if (width < 0) {
                        ladjust = !ladjust;
                        width = -width;
                    }
                } else {
                    dwidth = arguments.Next<int>();
                }
                step = RtldStep::Reswitch;
                break;
            case '0':
                if (!dot) {
                    padc = '0';
                    step = RtldStep::Reswitch;
                    break;
                }
                [[fallthrough]];
            case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
                int n = 0;
                for (;; ++fmt) {
                    n = n * 10 + ch - '0';
                    ch = *fmt;
                    if (ch < '0' || ch > '9') break;
                }
                if (dot)
                    dwidth = n;
                else
                    width = n;
                step = RtldStep::Reswitch;
                break;
            }
            case 'b': {
                num = static_cast<unsigned int>(arguments.Next<int>());
                const char* p = arguments.Next<const char*>();
                const int bitBase = *p++;
                if (bitBase < 2 || bitBase > 36) throw std::invalid_argument("rtld_printf: %b with a base outside 2..36");
                for (const char* q = Ksprintn(nbuf, num, bitBase, nullptr, false); *q;) out.Put(*q--);
                if (num == 0) break;
                bool named = false;
                while (*p) {
                    int n = *p++;
                    if (num & static_cast<unsigned int>(1u << ((n - 1) & 31))) {
                        out.Put(named ? ',' : '<');
                        for (; (n = *p) > ' '; ++p) out.Put(n);
                        named = true;
                    } else {
                        for (; *p > ' '; ++p) {
                        }
                    }
                }
                if (named) out.Put('>');
                break;
            }
            case 'c':
                out.Put(arguments.Next<int>());
                break;
            case 'D': {
                const unsigned char* up = arguments.Next<const unsigned char*>();
                const char* p = arguments.Next<const char*>();
                if (!width) width = 16;
                while (width--) {
                    out.Put(RtldLowerDigits[*up >> 4]);
                    out.Put(RtldLowerDigits[*up & 0x0f]);
                    up++;
                    if (width)
                        for (const char* q = p; *q; q++) out.Put(*q);
                }
                break;
            }
            case 'd':
            case 'i':
                base = 10;
                sign = true;
                step = RtldStep::Sign;
                break;
            case 'h':
                if (hflag) {
                    hflag = false;
                    cflag = true;
                } else {
                    hflag = true;
                }
                step = RtldStep::Reswitch;
                break;
            case 'j':
                jflag = true;
                step = RtldStep::Reswitch;
                break;
            case 'l':
                if (lflag) {
                    lflag = false;
                    qflag = true;
                } else {
                    lflag = true;
                }
                step = RtldStep::Reswitch;
                break;
            case 'n':
                if (jflag)
                    *arguments.Next<std::intmax_t*>() = retval;
                else if (qflag)
                    *arguments.Next<std::int64_t*>() = retval;
                else if (lflag)
                    *arguments.Next<std::int64_t*>() = retval;
                else if (zflag)
                    *arguments.Next<std::size_t*>() = retval;
                else if (hflag)
                    *arguments.Next<short*>() = retval;
                else if (cflag)
                    *arguments.Next<char*>() = retval;
                else
                    *arguments.Next<int*>() = retval;
                break;
            case 'o':
                base = 8;
                step = RtldStep::Nosign;
                break;
            case 'p':
                base = 16;
                sharpflag = width == 0;
                sign = false;
                num = reinterpret_cast<std::uintptr_t>(arguments.Next<void*>());
                step = RtldStep::Number;
                break;
            case 'q':
                qflag = true;
                step = RtldStep::Reswitch;
                break;
            case 'r':
                base = radix;
                step = sign ? RtldStep::Sign : RtldStep::Nosign;
                break;
            case 's': {
                const char* p = arguments.Next<const char*>();
                if (p == nullptr) p = "(null)";
                int n = 0;
                if (!dot)
                    n = static_cast<int>(std::strlen(p));
                else
                    for (n = 0; n < dwidth && p[n]; n++) {
                    }
                width -= n;
                if (!ladjust && width > 0)
                    while (width--) out.Put(padc);
                while (n--) out.Put(*p++);
                if (ladjust && width > 0)
                    while (width--) out.Put(padc);
                break;
            }
            case 't':
                tflag = true;
                step = RtldStep::Reswitch;
                break;
            case 'u':
                base = 10;
                step = RtldStep::Nosign;
                break;
            case 'X':
                upper = true;
                [[fallthrough]];
            case 'x':
                base = 16;
                step = RtldStep::Nosign;
                break;
            case 'y':
                base = 16;
                sign = true;
                step = RtldStep::Sign;
                break;
            case 'z':
                zflag = true;
                step = RtldStep::Reswitch;
                break;
            default:
                while (percent < fmt) out.Put(*percent++);
                stop = true;
                break;
            }
            if (step == RtldStep::Reswitch) continue;
            if (step == RtldStep::Nosign) {
                sign = false;
                if (jflag)
                    num = arguments.Next<std::uintmax_t>();
                else if (qflag)
                    num = arguments.Next<std::uint64_t>();
                else if (tflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::ptrdiff_t>());
                else if (lflag)
                    num = arguments.Next<std::uint64_t>();
                else if (zflag)
                    num = arguments.Next<std::size_t>();
                else if (hflag)
                    num = static_cast<unsigned short>(arguments.Next<int>());
                else if (cflag)
                    num = static_cast<unsigned char>(arguments.Next<int>());
                else
                    num = arguments.Next<unsigned int>();
                step = RtldStep::Number;
            } else if (step == RtldStep::Sign) {
                if (jflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::intmax_t>());
                else if (qflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::int64_t>());
                else if (tflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::ptrdiff_t>());
                else if (lflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::int64_t>());
                else if (zflag)
                    num = static_cast<std::uintmax_t>(arguments.Next<std::int64_t>());
                else if (hflag)
                    num = static_cast<std::uintmax_t>(static_cast<std::intmax_t>(static_cast<short>(arguments.Next<int>())));
                else if (cflag)
                    num = static_cast<std::uintmax_t>(static_cast<std::intmax_t>(static_cast<signed char>(arguments.Next<int>())));
                else
                    num = static_cast<std::uintmax_t>(static_cast<std::intmax_t>(arguments.Next<int>()));
                step = RtldStep::Number;
            }
            if (step == RtldStep::Number) {
                if (sign && static_cast<std::intmax_t>(num) < 0) {
                    neg = true;
                    num = 0 - num;
                }
                int n = 0;
                const char* p = Ksprintn(nbuf, num, base, &n, upper);
                int tmp = 0;
                if (sharpflag && num != 0) {
                    if (base == 8)
                        tmp++;
                    else if (base == 16)
                        tmp += 2;
                }
                if (neg) tmp++;
                if (!ladjust && padc == '0') dwidth = width - tmp;
                width -= tmp + (dwidth > n ? dwidth : n);
                dwidth -= n;
                if (!ladjust)
                    while (width-- > 0) out.Put(' ');
                if (neg) out.Put('-');
                if (sharpflag && num != 0) {
                    if (base == 8) {
                        out.Put('0');
                    } else if (base == 16) {
                        out.Put('0');
                        out.Put('x');
                    }
                }
                while (dwidth-- > 0) out.Put('0');
                while (*p) out.Put(*p--);
                if (ladjust)
                    while (width-- > 0) out.Put(' ');
            }
            break;
        }
    }
}

}

extern "C" {

int APS5_VABI rtld_printf_nid_postfix(const char* format, ...) {
    RtldOutput output;
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
#else
    std::va_list arguments;
    va_start(arguments, format);
#endif
    const int result = Kvprintf(format, output, 10, RtldArguments{&arguments});
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
    output.Flush();
    return result;
}

}
