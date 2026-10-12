
#pragma once

#ifndef PLUS4_HOT_CODE_SRAM
#define PLUS4_HOT_CODE_SRAM 0
#endif

#ifdef PLUS4_HOT_CODE
#undef PLUS4_HOT_CODE
#endif

#if PLUS4_HOT_CODE_SRAM
#define PLUS4_HOT_CODE(name_) \
    __attribute__((section(".time_critical.plus4_" #name_), noinline))
#else
#define PLUS4_HOT_CODE(name_)
#endif
