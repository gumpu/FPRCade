#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>

#define FDA_MAX_CODE_LENGTH (80)
#define FDA_MAX_FILENAME_LEN (180)

void disassemble(uint16_t instruction, char* code, uint16_t address)
{
    char stack_letters[6] = { 'd', 'r', 'c', 't', '?', '?' };
    char size_letters[6]  = { '?', 'b', 'w', '?', 'l', '?' };

    uint16_t group = (instruction & 0xF000);
    switch (group) {
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000: /* ENTER */
            {
                uint16_t address = (instruction & 0x3FFF) << 2;
                sprintf(code, "%s $%04x", "enter", address);
            }
            break;
        case 0x1000: /* BIF */
            {
                uint16_t new_address;
                int16_t offset = instruction & 0x0FFF;
                if (offset & 0x0800) {
                    /* sign extend */
                    offset = offset | 0xF000;
                }
                if (offset < 0) {
                    offset = (0 - offset);
                    new_address = address - (uint16_t)offset;
                } else {
                    new_address = address + (uint16_t)offset;
                }
                sprintf(code, "%s $%04x", "bif", new_address);
            }
            break;
        case 0x8000:
            {
                uint8_t func = (instruction & 0x0F00) >> 8;
                switch (func) {
                    case 0x00: /* NOP */
                        sprintf(code, "%s", "nop");
                        break;
                    case 0x01: /* LEAVE */
                        sprintf(code, "%s", "leave");
                        break;
                    case 0x02: /* HALT */
                        sprintf(code, "%s", "halt");
                        break;
                    default:
                        sprintf(code, "%s", "?");
                }
            }
            break;
        case 0xB000:
            {
                uint16_t func = (instruction & 0x0F00);
                uint16_t source_stack_id = (instruction & 0x00C0) >> 6;
                uint16_t dest_stack_id   = (instruction & 0x0030) >> 4;

                char source_letter = stack_letters[source_stack_id];
                char dest_letter = stack_letters[dest_stack_id];

                switch (func) {
                    case 0x0000: /* DROP */
                        sprintf(code, "%s %c", "drop", source_letter);
                        break;
                    case 0x0100: /* DUP */
                        sprintf(code, "%s %c %c",
                                "DUP", source_letter, dest_letter);
                        break;
                    case 0x0200: /* SWAP */
                        sprintf(code, "%s %c", "swap", source_letter);
                        break;
                    case 0x0300: /* MOV */
                        sprintf(code, "%s %c %c", "mov",
                                source_letter, dest_letter);
                        break;
                    default:
                        sprintf(code, "%s", "?");
                }
                break;
            }
        case 0xC000: /* Load:  LDL */
            {
                uint16_t stack_id = (instruction & 0x0C00) >> 10;
                uint16_t value = instruction & 0x03FF;
                char dest_letter = stack_letters[stack_id];
                sprintf(code, "%s %c $%04x", "ldl", dest_letter, value);
            }
            break;
        case 0xD000: /* Load: LDH */
            {
                uint16_t stack_id = (instruction & 0x0C00) >> 10;
                uint16_t value = instruction & 0x03F;
                char dest_letter = stack_letters[stack_id];
                sprintf(code, "%s %c $%04x", "ldh", dest_letter, value);
            }
            break;
        case 0xE000: /* 2 value operators */
            {
                uint8_t func = (instruction & 0x0F80) >> 7;
                if (func == 0x0B) {
                    uint8_t size;
                    bool is_io;
                    size = instruction & 0x03;
                    is_io = instruction & 0x40;
                    char size_letter = size_letters[size];
                    if (is_io) {
                        sprintf(code, "%s %c", "isto", size_letter);
                    } else {
                        sprintf(code, "%s %c", "sto", size_letter);
                    }
                } else {
                    bool is_signed = instruction & 0x0010;
                    char* template = NULL;
                    if (is_signed) {
                        template = "%su";
                    } else {
                        template = "%s";
                    }
                    switch (func) {
                        case 0x00: /* ADD(U) */
                            sprintf(code, template, "add");
                            break;
                        case 0x03:
                            sprintf(code, "%s", "eq");
                            break;
                        case 0x04:
                            sprintf(code, "%s", "asr");
                            break;
                        case 0x05:
                            sprintf(code, template, "lt");
                            break;
                        case 0x06:
                            sprintf(code, template, "gt");
                            break;
                        case 0x08:
                            sprintf(code, template, "gte");
                            break;
                        case 0x09:
                            sprintf(code, "%s", "lsr");
                            break;
                        case 0x0A:
                            sprintf(code, "%s", "lsl");
                            break;
                        case 0x0D:
                            sprintf(code, "%s", "and");
                            break;
                        case 0x0E:
                            sprintf(code, "%s", "or");
                            break;
                        case 0x0F:
                            sprintf(code, "%s", "xor");
                            break;
                    }
                }
            }
            break;
        case 0xF000:
            {
                uint16_t func = (instruction & 0x0F00) >> 8;
                switch (func) {
                    case 0x00:
                        sprintf(code, "%s", "neg");
                        break;
                    case 0x01:
                        sprintf(code, "%s", "not");
                        break;
                    case 0x02:
                        {
                            uint8_t size = instruction & 0x03;
                            bool is_io = instruction & 0x40;
                            char size_letter = size_letters[size];
                            if (is_io) {
                                sprintf(code, "%s %c", "ird", size_letter);
                            } else {
                                sprintf(code, "%s %c", "rd", size_letter);
                            }
                        }
                        break;
                    default:
                        sprintf(code, "%s", "?");
                }
            }
            break;
        default:
    }
}


static void display_usage()
{
}

static void parse_options(
        int argc, char** argv,
        char* input_file_name)
{
    char c;
    input_file_name[0] = '\0';

    while ((c = getopt(argc, argv, "hi:o:r:")) != EOF) {
        switch (c) {
            case 'h':
                display_usage();
                exit(EXIT_SUCCESS);
                break;
            default:
                break;

        }
    }
}


int main(int argc, char** argv)
{
    static char input_file_name[FDA_MAX_FILENAME_LEN + 2];
    static char code[FDA_MAX_CODE_LENGTH];

    parse_options(argc, argv, input_file_name);
    for (uint32_t i = 0x0000; i < 0xFFFF; ++i) {
        disassemble((uint16_t)i, code, 0x8000);
        printf("%04x %04x %s\n", 0x8000, (uint16_t)i, code);
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
