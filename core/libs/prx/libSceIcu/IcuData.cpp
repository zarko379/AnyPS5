#if defined(_WIN32)
#define ICU_DATA_SECTION ".section .rdata,\"dr\"\n"
#define ICU_DATA_SYMBOL "icudt68_dat"
#define ICU_DATA_VISIBILITY ""
#elif defined(__APPLE__)
#define ICU_DATA_SECTION ".section __TEXT,__const\n"
#define ICU_DATA_SYMBOL "_icudt68_dat"
#define ICU_DATA_VISIBILITY ".private_extern " ICU_DATA_SYMBOL "\n"
#else
#define ICU_DATA_SECTION ".section .rodata\n"
#define ICU_DATA_SYMBOL "icudt68_dat"
#define ICU_DATA_VISIBILITY ".hidden " ICU_DATA_SYMBOL "\n"
#endif

asm(ICU_DATA_SECTION
    ".balign 16\n"
    ".globl " ICU_DATA_SYMBOL "\n"
    ICU_DATA_VISIBILITY
    ICU_DATA_SYMBOL ":\n"
    ".incbin \"" ICU_DATA_FILE "\"\n"
    ".text\n");
