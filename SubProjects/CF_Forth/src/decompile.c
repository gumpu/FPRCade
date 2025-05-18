/*
 * Decompiler for Forth definitions
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fcore.h"
#include "instructions.h"
#include "decompile.h"

/* --------------------------------------------------------------------*/

static T_DictHeader* address2header(T_Context* ctx, dataspace_index_t ip)
{
    T_DictHeader* header = NULL;
    dataspace_index_t xt;

    for (xt = ctx->dict_head; xt != 0; xt = header->previous) {
        header = dict_get_entry(ctx, xt);
        if (header->flags & F_COLONDEF) {
            uint16_t offset = dict_get_code_offset(header);
            CF_Assert(offset != 0);
            if (ip == (xt + offset)) {
                return header;
            }
        }
    }
    return NULL;
}

/*
 * Decompile a single definition
 */
void decompile(T_Context* ctx, dataspace_index_t ip)
{
    dataspace_index_t i = ip;
    instruction_t x;
    do {
        x = fetch8bits(ctx->dataspace, i);
        switch(x) {
            case OPCODE_PNUM:
                printf("%04x (NUMBER)\n", i);
                i++;
                break;
            case OPCODE_PARSE_NAME:
                printf("%04x PARSE-NAME\n", i);
                i++;
                break;
            case OPCODE_FIND2:
                printf("%04x FIND2\n", i);
                i++;
                break;
            case OPCODE_ADD:
                printf("%04x +\n", i);
                i++;
                break;
            case OPCODE_DOT:
                printf("%04x .\n", i);
                i++;
                break;
            case OPCODE_FETCH:
                printf("%04x @\n", i);
                i++;
                break;
            case OPCODE_BL:
                printf("%04x BL\n", i);
                i++;
                break;
            case OPCODE_CR:
                printf("%04x CR\n", i);
                i++;
                break;
            case OPCODE_EMIT:
                printf("%04x EMIT\n", i);
                i++;
                break;
            case OPCODE_HALT:
                printf("%04x HALT\n", i);
                i++;
                break;
            case OPCODE_NOP:
                printf("%04x NOP\n", i);
                i++;
                break;
            case OPCODE_EXECUTE:
                printf("%04x EXECUTE\n", i);
                i++;
                break;
            case OPCODE_COMPILE:
                printf("%04x COMPILE\n", i);
                i++;
                break;
            case OPCODE_JMP:
                {
                    uint16_t d = fetch16bits(ctx->dataspace, i + 1);
                    printf("%04x JMP %04x\n", i, d);
                    i += 3;
                }
                break;
            case OPCODE_JMP_FALSE:
                {
                    uint16_t d = fetch16bits(ctx->dataspace, i + 1);
                    printf("%04x JPF %04x\n", i, d);
                    i += 3;
                }
                break;
            case OPCODE_DUP:
                printf("%04x DUP\n", i);
                i++;
                break;
            case OPCODE_OVER:
                printf("%04x OVER\n", i);
                i++;
                break;
            case OPCODE_LITERAL:
                printf("%04x LITERAL\n", i);
                i++;
                break;
            case OPCODE_SWAP:
                printf("%04x SWAP\n", i);
                i++;
                break;
            case OPCODE_DROP:
                printf("%04x DROP\n", i);
                i++;
                break;
            case OPCODE_EQUAL:
                printf("%04x =\n", i);
                i++;
                break;
            case OPCODE_PACCEPT:
                printf("%04x (ACCEPT)\n", i);
                i++;
                break;
            case OPCODE_PREMAIN:
                printf("%04x (REMAIN)\n", i);
                i++;
                break;
            case OPCODE_LDN:
                {
                    uint16_t d = fetch16bits(ctx->dataspace, i + 1);
                    printf("%04x LDA %04x (%d)\n", i, d, d);
                    i += 3;
                }
                break;
            case OPCODE_ENTER:
                {
                    uint16_t d = fetch16bits(ctx->dataspace, i + 1);
                    T_DictHeader* h = address2header(ctx, d);
                    if (h != NULL) {
                        printf("%04x ENTER %04x (%s)\n", i, d, h->name);
                    } else {
                        printf("%04x ENTER %04x (%d)\n", i, d, d);
                    }
                    i += 3;
                }
                break;
            case OPCODE_LEAVE_COLON:
                printf("%04x :_LEAVE\n", i);
                i++;
                break;
            default:
                printf("%04x %02x\n", i, x);
                ++i;
        }
    }
    while (x != OPCODE_LEAVE_COLON);
}

/*
 *
 */

void decompile_dictionary(T_Context* ctx)
{
    T_DictHeader* header;
    dataspace_index_t h = ctx->dict_head;
    do {
        header = (T_DictHeader*)(ctx->dataspace+h);
        if (header->flags & F_COLONDEF) {
            printf("%s (%d)\n", header->name, header->name_length);
            dataspace_index_t ip = h + sizeof(T_DictHeader) + header->name_length + 1;
            printf("code starts at %04x\n", ip);
            decompile(ctx, ip);
        }
        h = header->previous;
    } while (h != 0);
}

/* --------------------------------------------------------------------*/

