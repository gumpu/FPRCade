#ifndef HG_FCORE_H
#define HG_FCORE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t count;
    char s[];
} T_CountedString;

typedef unsigned char address_unit_t;
typedef uint16_t cell_t;
typedef int16_t signed_cell_t;
typedef cell_t dataspace_index_t;
/* A token that indicates which dictionary entry needs to be executed or
 * compiled. It points to the start of the entry
 */
typedef cell_t execution_token_t;
typedef uint8_t instruction_t;
typedef cell_t instruction_pointer_t;
typedef uint16_t word_flag_t;

#define NULL_INDEX  (0)     // Null value for dataspace_index_t
#define NULL_TOKEN  (0)     // Null value for execution_token_t
#define CELL_SIZE   (2)     // because uint16_t == 2 bytes
#define MEMORY_SIZE (65536) // 64K

/* Location in the dataspace of the various default variables  */
#define LOC_WHERE              (2)
#define LOC_STATE              (4)
#define LOC_INPUT_BUFFER       (6)
#define INPUT_BUFFER_SIZE      (256)
#define LOC_INPUT_BUFFER_INDEX (6+256)
#define LOC_INPUT_BUFFER_COUNT (6+256+2)

#define FORTH_FALSE (0x0000)
#define FORTH_TRUE  (0xFFFF)

/* Maximum length of the name of a word in a dictionary */
#define MAX_DE_NAME_LENGTH 40
/* Flags used in DictHeader */
#define F_INSTRUCTION  (1U << 0U)
#define F_COLONDEF     (1U << 1U)
#define F_IMMEDIATE    (1U << 2U)
#define F_COMPILE_ONLY (1U << 3U)

typedef struct DictHeader {
    dataspace_index_t previous;
    /* Points to the last byte of this entry */
    dataspace_index_t entry_end;
    /* Flags associated with the word.
     * i.e. direct, immediate, compile only, etc
     */
    word_flag_t flags;
    instruction_t semantics;
    uint8_t name_length;  /* excluding the '\0' */
    char name[];  /* '\0' terminated */
} T_DictHeader;

typedef struct Stack {
    dataspace_index_t top;
    dataspace_index_t where;
    dataspace_index_t bottom;
} T_Stack;

typedef struct Context {
    /* Points to the location in the data space that contains the execution
     * token of the word being executed */
    instruction_pointer_t ip;
    /* Start of data space */
    address_unit_t* dataspace;
    /* Content of STATE There are two states:
     *
     * - executing  value False
     * - compiling  value True
     */
    dataspace_index_t state;
    /* Indicates whether the inner_interpreter should continue running. Used
     * for a clean exit and exceptions */
    bool run;
    /* Head element of the dictionary */
    dataspace_index_t dict_head;
    /* The currently being compiled entry */
    dataspace_index_t dict_new;
    /* Maximum avialable data space */
    dataspace_index_t top;
    /* Above this are the three stacks */
    T_Stack control_stack;
    T_Stack data_stack;
    T_Stack return_stack;
} T_Context;

typedef instruction_pointer_t (*XTfunction_T)(T_Context*);

extern void CF_RealAssert(bool value, int line);
/* Normal assert() crashes/confuses gdb
 */
#define CF_Assert(value) CF_RealAssert(value, __LINE__)

extern cell_t pop(address_unit_t* dataspace, T_Stack* s);
extern void push(address_unit_t* dataspace, T_Stack* s, cell_t v);
extern uint8_t fetch8bits(address_unit_t* dataspace, dataspace_index_t i);
extern cell_t fetch16bits(address_unit_t* dataspace, dataspace_index_t i);
extern void store16bits(address_unit_t* dataspace, dataspace_index_t i, cell_t v);
extern void store8bits(address_unit_t* dataspace, dataspace_index_t i, uint8_t v);
extern dataspace_index_t init_stack(T_Stack* s, dataspace_index_t where, uint16_t size);
extern void fs_reset(T_Context* ctx);
extern void inner_interpreter(T_Context* context);
extern T_DictHeader* dict_get_entry(T_Context* ctx, execution_token_t xt);
extern execution_token_t dict_find(T_Context* ctx, char* name);
extern execution_token_t dict_find2(T_Context* ctx, char* string, uint8_t length);
extern uint16_t init_header(
    T_DictHeader* header,
    dataspace_index_t previous_word,
    char* name,
    word_flag_t flags);
extern dataspace_index_t add_variable(
    T_Context* ctx,
    dataspace_index_t previous_word,
    dataspace_index_t location,
    char* name
    );
extern dataspace_index_t add_word(
    T_Context* ctx,
    dataspace_index_t previous_word,
    instruction_t instruction,char* name,
    word_flag_t flags);
extern void fs_init_dictionary(T_Context* ctx);
extern dataspace_index_t allign(dataspace_index_t address);
extern void set_code(T_Context* ctx, char* code);
extern uint16_t dict_get_code_offset(T_DictHeader* header);

void decompile_dictionary(T_Context* ctx);

#endif /* HG_FCORE_H */
