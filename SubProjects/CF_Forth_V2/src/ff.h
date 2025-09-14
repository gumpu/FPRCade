#ifndef HG_FF_H
#define HG_FF_H

#include <stdint.h>
#include <stdbool.h>

#define FF_MEMORY_SIZE (65536)
#define FF_CELL_SIZE (2)

/**
 * Default FORTH-79 string type,
 * Strings can be max 127 characters long. */
typedef struct {
    int8_t count;
    char s[];
} T_PackedString;

/* Location in the dataspace of the various default variables  */
/* Location 0x0000 is reserved */
#define FF_LOC_HERE               (1*FF_CELL_SIZE)   /* HERE */
#define FF_LOC_STATE              (2*FF_CELL_SIZE)   /* STATE */
#define FF_LOC_BASE               (3*FF_CELL_SIZE)   /* BASE */
#define FF_LOC_INPUT_BUFFER_INDEX (4*FF_CELL_SIZE)   /* >IN */
#define FF_LOC_INPUT_BUFFER_COUNT (5*FF_CELL_SIZE)
#define FF_LOC_WORD_BUFFER        (6*FF_CELL_SIZE)
#define FF_INPUT_BUFFER_SIZE      (1024)
#define FF_WORD_BUFFER_SIZE       (1+127)
#define FF_LOC_INPUT_BUFFER       (FF_LOC_WORD_BUFFER+FF_WORD_BUFFER_SIZE)
#define FF_SYSTEM_VARIABLES_END   (FF_LOC_INPUT_BUFFER+FF_INPUT_BUFFER_SIZE)

#define FF_RETURN_STACK_SIZE (24)
#define FF_CONTROL_STACK_SIZE (20)
#define FF_DATA_STACK_SIZE (32)

#define FF_STATE_COMPILING (1)
#define FF_STATE_INTERPRETING (0)

typedef uint16_t unsigned_cell_type;
typedef uint16_t cell_type;
typedef int16_t  signed_cell_type;
typedef uint16_t index_type;

/* There can be 65535 instructions.  Each instruction is a number that is an
 * index into a table of function pointers.  The function it points to is the
 * function that implements the instructions behaviour.
 */
typedef cell_type           instruction_type;
/* pointer to an instruction in memory */
typedef unsigned_cell_type  instruction_pointer_type;
/* Smallest unit that can be addressed */
typedef unsigned char       address_unit_type;
typedef unsigned_cell_type  address_type;

/* A token that indicates which dictionary entry needs to be executed or
 * compiled. It points to the start of the entry
 */
typedef cell_type execution_token_type;

/**
 * Flags associated with the word:
 *
 * - immediate,  do not compile but run immediately
 * - colon_def_only, can only be used inside a colon definition
 * - compile_only, can only be used with in compile mode
 */
typedef uint16_t word_flag_type;

#define EF_MACHINE_CODE     (1 << 0)
#define EF_IMMEDIATE        (1 << 1)
#define EF_COLON_DEF_ONLY   (1 << 2)
#define EF_COMPILE_ONLY     (1 << 3)
#define EF_HIDDEN           (1 << 4)

#define FORTH_FALSE (0x0000)
#define FORTH_TRUE  (0x0001)

/* Maximum length of the name of a word in a dictionary */
#define FF_MAX_DE_NAME_LENGTH 31

typedef enum {
    eOP_SPACE = 0,
    eOP_CREATE,
    eOP_CREATE_RT,
    eOP_WORD,
    eOP_ALLOT,
    eOP_ENTER,
    eOP_EXIT,
    eOP_EXECUTE,
    eOP_JUMP,
    eOP_JUMP_IF,
    eOP_PUSH,
    eOP_DROP,
    eOP_FIND,
    eOP_STATE,
    eOP_QUERY,
    eOP_COLON,
    eOP_SEMICOLON,
    eOP_COMMA,
    eOP_DOT,
    eOP_BEGIN,
    eOP_THEN,
    eOP_IF,
    eOP_ELSE,
    eOP_WHILE,
    eOP_REPEAT,
    eOP_AGAIN,
    eOP_LITERAL,
    eOP_PLITERAL,
    eOP_EMIT,
    eOP_ABORT,
    eOP_IMMEDIATE,
    eOP_QIMMEDIATE,
    eOP_BYE,
    eOP_PLUS,
    eOP_AT,
    eOP_C_AT,
    eOP_PASS,
    eOP_EQUAL,
    eOP_DUP,
    eOP_BNUMBER,
    eOP_WORDBUFFER,

    /* This always needs to be the last entry */
    eOP_MAX_OP_CODE
} T_OpCode;

typedef struct Stack {
    address_type top;
    address_type where;
    address_type bottom;
} T_Stack;

typedef struct DictHeader {
    /* Pointer to the previous entry in the dictionary If this is ZERO, this
     * is the final entry */
    address_type previous;
    word_flag_type flags;
    /* Semantics; This uses 4 bytes and can be optimized by
     * using a different type */
    T_OpCode code_field;      /* Code to execute for the word */
    address_type does_code;   /* Needed for does> */
    /* The word itself */
    T_PackedString name;
} T_DictHeader;

#define DE_PREVIOUS (0)
#define DE_FLAGS (2)
#define DE_CODE_FIELD (4)
#define DE_DOES_CODE (8)
#define DE_NAME (10)
#define DE_SIZE_MIN (12)

typedef struct Context {
    /* Points to the location in the data space that contains the execution
     * token of the word being executed */
    instruction_pointer_type ip;
    /* Start of data space */
    address_unit_type* dataspace;
    /* Indicates whether the inner_interpreter should continue running. Used
     * for a clean exit and exceptions */
    bool run;

    /* Head element of the dictionary */
    address_type dict_head;

    /* Top of the avialable data space */
    address_type top;

    /* The stacks */
    T_Stack control_stack;
    T_Stack data_stack;
    T_Stack return_stack;
} T_Context;

typedef instruction_pointer_type (*function_type)(T_Context*, address_type);

typedef address_type (*decomp_fp_type)(
        T_Context*, T_DictHeader* header, address_type);

extern void CF_RealAssert(bool value, int line);
/* Normal assert() crashes/confuses gdb
 */
#define CF_Assert(value) CF_RealAssert(value, __LINE__)

#endif /* HG_FF_H */
