#ifndef HG_INSTRUCTIONS_H
#define HG_INSTRUCTIONS_H

#include "fcore.h"

extern instruction_pointer_t instr_halt(T_Context* ctx);
extern instruction_pointer_t instr_enter(T_Context* ctx);
extern instruction_pointer_t instr_leave_colon(T_Context* ctx);
extern instruction_pointer_t instr_add(T_Context* ctx);
extern instruction_pointer_t instr_load_number(T_Context* ctx);
extern instruction_pointer_t instr_allot(T_Context* ctx);
extern instruction_pointer_t instr_compile(T_Context* ctx);
extern instruction_pointer_t instr_execute(T_Context* ctx);
extern instruction_pointer_t instr_here(T_Context* ctx);
extern instruction_pointer_t instr_dot(T_Context* ctx);
extern instruction_pointer_t instr_emit(T_Context* ctx);
extern instruction_pointer_t instr_bl(T_Context* ctx);
extern instruction_pointer_t instr_dup(T_Context* ctx);
extern instruction_pointer_t instr_drop(T_Context* ctx);
extern instruction_pointer_t instr_equal(T_Context* ctx);
extern instruction_pointer_t instr_fetch(T_Context* ctx);
extern instruction_pointer_t instr_store(T_Context* cTX);
extern instruction_pointer_t instr_nop(T_Context* ctx);
extern instruction_pointer_t instr_jmp(T_Context* ctx);
extern instruction_pointer_t instr_jmp_false(T_Context* ctx);
extern instruction_pointer_t instr_begin(T_Context* ctx);
extern instruction_pointer_t instr_again(T_Context* ctx);
extern instruction_pointer_t instr_if(T_Context* ctx);
extern instruction_pointer_t instr_else(T_Context* ctx);
extern instruction_pointer_t instr_then(T_Context* ctx);
extern instruction_pointer_t instr_colon(T_Context* ctx);
extern instruction_pointer_t instr_parse_name(T_Context* ctx);
extern instruction_pointer_t instr_semicolon(T_Context* ctx);
extern instruction_pointer_t instr_literal(T_Context* ctx);
extern instruction_pointer_t instr_premain(T_Context* ctx);
extern instruction_pointer_t instr_paccept(T_Context* ctx);
extern instruction_pointer_t instr_cr(T_Context* ctx);
extern instruction_pointer_t instr_find2(T_Context* ctx);
extern instruction_pointer_t instr_pnumber(T_Context* ctx);
extern instruction_pointer_t instr_over(T_Context* ctx);
extern instruction_pointer_t instr_swap(T_Context* ctx);
extern instruction_pointer_t instr_decompile(T_Context* ctx);

#define OPCODE_HALT 0
#define OPCODE_LDN  1
#define OPCODE_ENTER 2
#define OPCODE_ADD  3
#define OPCODE_COMPILE 4
#define OPCODE_EXECUTE 5
#define OPCODE_HERE 6
#define OPCODE_DOT  7
#define OPCODE_ALLOT 8
#define OPCODE_FETCH 9
#define OPCODE_STORE 10
#define OPCODE_LEAVE_COLON 11
#define OPCODE_DUP 12
#define OPCODE_DROP 13
#define OPCODE_EQUAL 14
#define OPCODE_EMIT 15
#define OPCODE_BL 16
#define OPCODE_JMP 17
#define OPCODE_NOP 18
#define OPCODE_JMP_FALSE 19
#define OPCODE_BEGIN 20
#define OPCODE_AGAIN 21
#define OPCODE_IF 22
#define OPCODE_ELSE 23
#define OPCODE_THEN 24
#define OPCODE_COLON 25
#define OPCODE_SEMICOLON 26
#define OPCODE_PARSE_NAME 27
#define OPCODE_LITERAL 28
#define OPCODE_PREMAIN 29
#define OPCODE_PACCEPT 30
#define OPCODE_CR 31
#define OPCODE_FIND2 32
#define OPCODE_PNUM 33
#define OPCODE_OVER 34
#define OPCODE_SWAP 35
#define OPCODE_DECOMPILE 36

#define INSTRUCTION_COUNT (256)
extern XTfunction_T semantic_table[INSTRUCTION_COUNT];

#endif /* HG_INSTRUCTIONS_H */
