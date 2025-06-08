#ifndef HG_FDISA_H
#define HG_FDISA_H

#include <stdint.h>

#define FDA_MAX_CODE_LENGTH (80)

extern void disassemble(
        uint16_t instruction,
        char* code,
        uint16_t address);

#endif /* HG_FDISA_H */
