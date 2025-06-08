#ifndef HG_FDISA_H
#define HG_FDISA_H

#include <stdint.h>

extern void disassemble(
        uint16_t instruction,
        char* code,
        uint16_t address);

#endif /* HG_FDISA_H */
