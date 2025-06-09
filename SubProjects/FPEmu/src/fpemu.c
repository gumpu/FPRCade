#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>

#include "fdisa.h"

#define MEMORY_SIZE 65536
#define DSTACK_SIZE 16
#define RSTACK_SIZE 32
#define CSTACK_SIZE 32
#define TSTACK_SIZE 16
#define MAX_STACK_SIZE 64
#define FPEM_MAX_FILENAME_LEN 255

/* Stack IDs */
#define DATA_STACK    0x00
#define RETURN_STACK  0x01
#define CONTROL_STACK 0x02
#define TEMP_STACK    0x03

enum ExceptionCode {
    AllIsOK = 0U,
    StackOverflow,
    StackUnderflow,
    IllegalInstruction,
    IllegalStackID
};

char* exception_descriptions[] = {
    "All is OK",
    "Stack overflow",
    "Stack underflow",
    "Illegal instruction",
    "Illegal stack id"
};

struct Stack {
    uint16_t top;
    uint16_t size;
    uint16_t values[MAX_STACK_SIZE];
};

struct CPU_Context {
    bool keep_going;
    uint16_t exception;
    struct Stack data_stack;
    struct Stack return_stack;
    struct Stack control_stack;
    struct Stack temp_stack;
    uint16_t pc;
    uint16_t instruction;
    int fd_in;
    int fd_out;
};

static uint8_t* memory = NULL;

/* --------------------------------------------------------------------*/

static void push(
    struct CPU_Context* c,
    struct Stack* s,
    uint16_t value);
static uint16_t pop(
    struct CPU_Context* c,
    struct Stack* s);
static void cpu_reset(struct CPU_Context* c);
static uint16_t fetch_instruction(
    uint8_t* memory,
    uint16_t pc);
static void run(struct CPU_Context* c, uint8_t* memory);
static uint8_t hex_get_byte(char* line, unsigned n);
static bool load_hex(char* filename, uint8_t* memory);

/* --------------------------------------------------------------------*/

static void push(
        struct CPU_Context* c,
        struct Stack* s,
        uint16_t value)
{
    s->values[s->top] = value;
    (s->top)++;
    if (s->top == s->size) {
        c->keep_going = false;
        c->exception = StackOverflow;
    }
}

static uint16_t pop(
        struct CPU_Context* c,
        struct Stack* s)
{
    if (s->top == 0) {
        c->keep_going = false;
        c->exception = StackUnderflow;
        return 0xDEAD;
    } else {
        (s->top)--;
        return s->values[s->top];
    }
}


/* Perform CPU reset */
static void cpu_reset(struct CPU_Context* c)
{
    c->data_stack.size    = DSTACK_SIZE;
    c->data_stack.top     = 0U;
    c->return_stack.size  = RSTACK_SIZE;
    c->return_stack.top   = 0U;
    c->control_stack.size = CSTACK_SIZE;
    c->control_stack.top  = 0U;
    c->temp_stack.size    = TSTACK_SIZE;
    c->temp_stack.top     = 0U;

    c->pc          = 0xF000;  /* program counter */
    c->instruction = 0x0000;  /* current instruction */
    c->keep_going  = true;
    c->exception   = AllIsOK;
}

/*
 * Get pointer to the stack indicated by the stack ID.
 */
struct Stack* get_stack(struct CPU_Context* c, uint16_t stack_id)
{
    struct Stack* stack;

    switch (stack_id) {
        case DATA_STACK:
            stack = &(c->data_stack);
            break;
        case RETURN_STACK:
            stack = &(c->return_stack);
            break;
        case CONTROL_STACK:
            stack = &(c->control_stack);
            break;
        case TEMP_STACK:
            stack = &(c->temp_stack);
            break;
        default:
            stack = &(c->data_stack);
            c->exception = IllegalStackID;
            c->keep_going = false;
    }

    return stack;
}

static uint16_t fetch_instruction(
        uint8_t* memory,
        uint16_t pc)
{
    uint8_t msb;
    uint8_t lsb;
    uint16_t instruction;

    /* little endian */
    lsb = *(memory + pc);
    msb = *(memory + pc + 1);

    instruction = msb;
    instruction = (instruction << 8) + lsb;

    return instruction;
}


/*
 * run the processor
 */

static void run(struct CPU_Context* c, uint8_t* memory)
{
    printf("Running\n");
    while(c->keep_going) {
        c->instruction = fetch_instruction(memory, c->pc);

        uint16_t group = (c->instruction & 0xF000);
        printf("%04x %04x\n", c->pc, c->instruction);
        switch (group) {
            case 0x4000:
            case 0x5000:
            case 0x6000:
            case 0x7000:
                {
                    /* ENTER */
                    struct Stack* stack = &(c->return_stack);
                    uint16_t address = ((c->instruction & 0x3FFF) << 2);
                    push(c, stack, (c->pc) + 2);
                    c->pc = address;
                }
                break;
            case 0x1000: /* BIF */
                {
                    uint16_t truth_value;
                    struct Stack* stack = &(c->data_stack);
                    truth_value = pop(c, stack);
                    if (truth_value) {
                        (c->pc) += 2;
                    } else {
                        int16_t offset = (c->instruction & 0x0FFF);
                        if (offset & 0x0800) {
                            /* sign extend */
                            offset = offset | 0xF000;
                        }
                        if (offset < 0) {
                            offset = (0 - offset);
                            c->pc -= (uint16_t)offset;
                        } else {
                            c->pc += offset;
                        }
                    }
                }
                break;
            case 0x8000:
                {
                    uint16_t func = (c->instruction & 0x0F00);
                    switch (func) {
                        case 0x0000: /* NOP */
                            (c->pc) += 2;
                            break;
                        case 0x0100: /* LEAVE */
                            {
                                struct Stack* stack = &(c->return_stack);
                                c->pc = pop(c, stack);
                            }
                            break;
                        case 0x0200: /* HALT */
                            (c->pc) += 2;
                            c->keep_going = false;
                            break;
                        default:
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                    }
                }
                break;
            case 0xB000:
                {
                    uint16_t value;
                    uint16_t value2;
                    struct Stack* source_stack;
                    struct Stack* destination_stack;
                    uint16_t func = (c->instruction & 0x0F00);
                    uint16_t source_stack_id = (c->instruction & 0x00C0) >> 6;
                    uint16_t dest_stack_id   = (c->instruction & 0x0030) >> 4;
                    source_stack = get_stack(c, source_stack_id);
                    destination_stack = get_stack(c, dest_stack_id);
                    switch (func) {
                        case 0x0000: /* DROP */
                            (void)pop(c, source_stack);
                            break;
                        case 0x0100: /* DUP */
                            value = pop(c, source_stack);
                            push(c, source_stack, value);
                            push(c, source_stack, value);
                            break;
                        case 0x0200: /* SWAP */
                            value = pop(c, source_stack);
                            value2 = pop(c, source_stack);
                            push(c, destination_stack, value);
                            push(c, destination_stack, value2);
                            break;
                        case 0x0300: /* MOV  TODO */
                        default:
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                    }
                    (c->pc) += 2;
                }
                break;
            case 0xC000: /* Load:  LDL */
                {
                    struct Stack* stack;
                    uint16_t stack_id = (c->instruction & 0x0C00) >> 10;
                    uint16_t value = (c->instruction & 0x03FF);
                    stack = get_stack(c, stack_id);
                    push(c, stack, value);
                    (c->pc) += 2;
                }
                break;
            case 0xD000: /* Load: LDH */
                {
                    struct Stack* stack;
                    uint16_t value;
                    uint16_t stack_id = (c->instruction & 0x0C00) >> 10;
                    uint16_t high_bits_value = (c->instruction & 0x003F);
                    stack = get_stack(c, stack_id);
                    value = pop(c, stack);
                    value = value | (high_bits_value << 10);
                    push(c, stack, value);
                }
                break;
            case 0xE000: /* 2 value operators */
                {
                    uint16_t func = (c->instruction & 0x0F80) >> 7;
                    uint16_t is_signed = (c->instruction & 0x0010);
                    struct Stack* stack = &(c->data_stack);
                    switch (func) {
                        case 0x00: /* ADD(U) */
                            {
                                if (is_signed) {
                                    int16_t v1, v2;
                                    v1 = pop(c, stack);
                                    v2 = pop(c, stack);
                                    v1 = v1 + v2;
                                    push(c, stack, (uint16_t)v1);
                                } else {
                                    uint16_t v1, v2;
                                    v1 = pop(c, stack);
                                    v2 = pop(c, stack);
                                    v1 = v1 + v2;
                                    push(c, stack, v1);
                                }
                            }
                            break;
                        case 0x03: /* EQ */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 == v1) ? 0xFFFF : 0x0000;
                                push(c, stack, r);
                            }
                            break;
                        case 0x04: /* ASR */
                            {
                                int16_t r;
                                if (is_signed) {
                                    int16_t v1, v2;
                                    v1 = (int16_t)pop(c, stack);
                                    v2 = (int16_t)pop(c, stack);
                                    r = (v2 >> v1);
                                    push(c, stack, r);
                                } else {
                                    c->exception = IllegalInstruction;
                                    c->keep_going = false;
                                }
                            }
                            break;
                        case 0x05: /* LT / LTU */
                            {
                                c->exception = IllegalInstruction;
                                c->keep_going = false;
                            }
                            break;
                        case 0x06: /* GT(U) */
                            {
                                uint16_t r;
                                if (is_signed) {
                                    int16_t v1, v2;
                                    v1 = (int16_t)pop(c, stack);
                                    v2 = (int16_t)pop(c, stack);
                                    r = (v2 > v1) ? 0xFFFF : 0x0000;
                                } else {
                                    uint16_t v1, v2;
                                    v1 = pop(c, stack);
                                    v2 = pop(c, stack);
                                    r = (v2 > v1) ? 0xFFFF : 0x0000;
                                }
                                push(c, stack, r);
                            }
                            break;
                        case 0x08: /* GTE(U) */
                            {
                                uint16_t r;
                                if (is_signed) {
                                    int16_t v1, v2;
                                    v1 = (int16_t)pop(c, stack);
                                    v2 = (int16_t)pop(c, stack);
                                    r = (v2 >= v1) ? 0xFFFF : 0x0000;
                                } else {
                                    uint16_t v1, v2;
                                    v1 = pop(c, stack);
                                    v2 = pop(c, stack);
                                    r = (v2 >= v1) ? 0xFFFF : 0x0000;
                                }
                                push(c, stack, r);
                            }
                            break;
                        case 0x09: /* LSR */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 << v1);
                                push(c, stack, r);
                            }
                            break;
                        case 0x0A: /* LSL */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 >> v1);
                                push(c, stack, r);
                            }
                            break;
                        case 0x0B: /* STO / ISTO */
                            {
                                uint16_t address;
                                uint16_t v;
                                uint8_t size;
                                uint16_t is_io;
                                v = pop(c, stack);
                                address = pop(c, stack);
                                size = (c->instruction) & 0x03;
                                is_io = (c->instruction) & 0x40;
                                if (is_io) {
                                    if (size == 1) {
                                        if (address == 0x00) {
                                            char buffer[2];
                                            buffer[0] = v;
                                            write(c->fd_out, buffer, 1);
                                        }
                                    } else {
                                        // TODO
                                        c->exception = IllegalInstruction;
                                        c->keep_going = false;
                                    }
                                } else {
                                    /* TODO */
                                    c->exception = IllegalInstruction;
                                    c->keep_going = false;
                                }
                            }
                            break;
                        case 0x0D: /* AND */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 & v1);
                                push(c, stack, r);
                            }
                            break;
                        case 0x0E: /* OR */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 | v1);
                                push(c, stack, r);
                            }
                            break;
                        case 0x0F: /* XOR */
                            {
                                uint16_t r;
                                uint16_t v1, v2;
                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                r = (v2 ^ v1);
                                push(c, stack, r);
                            }
                            break;
                        default:
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                    }
                    (c->pc) += 2;
                }
                break;
            case 0xF000:
                {
                    uint16_t func = (c->instruction & 0x0F00) >> 8;
                    struct Stack* stack = &(c->data_stack);

                    switch (func) {
                        case 0x00: /* NEG */
                        case 0x01: /* NOT */
                            // TODO
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                            break;
                        case 0x02: /* RD / IRD */
                            {
                                uint16_t address;
                                uint8_t size;
                                uint16_t is_io;

                                address = pop(c, stack);
                                size = (c->instruction & 0x03);
                                is_io = (c->instruction) & 0x40;
                                if (is_io) {
                                    if (size == 1) {
                                        if (address == 0x0000) {

                                        }
                                    }
                                    /* TODO */
                                    c->exception = IllegalInstruction;
                                    c->keep_going = false;
                                } else {
                                    if (size == 1) { /* byte */
                                        uint8_t byte = *(memory + address);
                                        uint16_t value = (uint16_t)byte;
                                        push(c, stack, value);
                                    } else {
                                        c->exception = IllegalInstruction;
                                        c->keep_going = false;
                                    }
                                }
                            }
                            break;
                        default:
                            // TODO
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                            break;
                    }
                    (c->pc) += 2;
                    break;
                }
            default:
                c->exception = IllegalInstruction;
                c->keep_going = false;
        }
    }
    printf("Processor halted\n");
    printf("ExceptionCode: %d (%s)\n",
            c->exception, exception_descriptions[c->exception]);
    printf("Last instruction: 0x%04X\n", c->instruction);
}


static uint8_t hex_get_byte(char* line, unsigned n)
{
    char c1 = line[n];
    char c2 = line[n+1];
    uint8_t u1 = (uint8_t)((c1 > '9') ? (c1 - 'A' + 10) : (c1 - '0'));
    uint8_t u2 = (uint8_t)((c2 > '9') ? (c2 - 'A' + 10) : (c2 - '0'));
    return u1*16+u2;
}

#define INPF_BUFFER_SIZE 1024

/**
 * Load a with fasm assembled file.
 *
 * Returns true if the file was OK and was loaded correcty.
 *
 * Returns false otherwise.
 */
static bool load_hex(char* filename, uint8_t* memory)
{
    bool ok;
    FILE *inpf;
    printf("Loading %s\n", filename);
    inpf = fopen(filename, "r");
    if (inpf == NULL) {
        perror("fopen");
        ok = false;
    } else {
        char buffer[INPF_BUFFER_SIZE + 2];
        char* line;
        ok = true;
        line = fgets(buffer, INPF_BUFFER_SIZE, inpf);
        while (line != NULL) {
            uint8_t* address;
            uint16_t location;
            if (line[0] != ':') {
                /* Ignore */
            } else {
                int n = strlen(line);
                if (n < 11) {
                    fprintf(stderr, "line in .hex is too short\n");
                    ok = false;
                    break;
                } else {
                    // TODO Check-sum check
                    uint8_t count = hex_get_byte(line, 1);
                    location = hex_get_byte(line, 3);
                    location = location << 8;
                    location += hex_get_byte(line, 5);
                    uint8_t type = hex_get_byte(line, 7);
                    if (type == 0) {
                        address = (memory + location);
                        for (uint8_t i = 0; i < count; ++i) {
                            uint8_t value = hex_get_byte(line, 9 + 2*i);
                            address[i] = (uint16_t)value;
                        }
                    } else if (type == 1) {
                        /* End of data */
                        break;
                    }
                }
            }
            line = fgets(buffer, INPF_BUFFER_SIZE, inpf);
        }
        fclose(inpf);
    }

    return ok;
}


#define FPEM_WORDSIZE (60)
#define FPEM_MAX_COMMAND_LINE_SIZE (3*FPEM_WORDSIZE)

typedef struct ParameterSet {
    unsigned number_of_words;
    char operation[FPEM_WORDSIZE];
    char par1[FPEM_WORDSIZE];
    char par2[FPEM_WORDSIZE];
} ParameterSet;

static void parse_command_line(char* cl, ParameterSet* p)
{
    unsigned i = 0;
    unsigned k;

    p->number_of_words = 0;
    for (k = 0; cl[i] != '\0'; ++i, ++k) {
        p->operation[k] = cl[i];
        if (isspace(cl[i])) { ++(p->number_of_words); break; }
    }
    p->operation[k] = '\0';
    for (; isspace(cl[i]); ++i);
    for (k = 0; cl[i] != '\0'; ++i, ++k) {
        p->par1[k] = cl[i];
        if (isspace(cl[i])) { ++(p->number_of_words); break; }
    }
    p->par1[k] = '\0';
    for (; isspace(cl[i]); ++i);
    for (k = 0; cl[i] != '\0'; ++i, ++k) {
        p->par2[k] = cl[i];
        if (isspace(cl[i])) { ++(p->number_of_words); break; }
    }
    p->par2[k] = '\0';
}


static void monitor(struct CPU_Context* context, uint8_t* memory)
{
    static char code[FDA_MAX_CODE_LENGTH];
    static char commandline[FPEM_MAX_COMMAND_LINE_SIZE + 2];
    static ParameterSet p;
    bool do_monitor = true;
    uint16_t address;
    uint8_t  count;

    while (do_monitor) {
        fgets(commandline, FPEM_MAX_COMMAND_LINE_SIZE, stdin);
        char command = commandline[0];
        parse_command_line(commandline, &p);
        printf("%d: %s, %s, %s\n",
                p.number_of_words,  p.operation, p.par1, p.par2);
        switch (command) {
            case 'r':
                {
                    address = strtol(p.par1, NULL, 16);
                    printf("run %04x\n", address);
                    run(context, memory);
                }
                break;
            case 'd':
                {
                    address = strtol(p.par1, NULL, 16);
                    count = strtol(p.par2, NULL, 16);
                    printf("disassemble %04x %04x\n", address, count);
                    for (uint16_t i = 0; i < count; i += 2) {
                        uint16_t instr = fetch_instruction(memory, address + i);
                        disassemble((uint16_t)instr, code, address + i);
                        printf("%04x %04x %s\n", address + i, (uint16_t)instr, code);
                    }
                }
                break;
            case 'x':
                {
                    address = strtol(p.par1, NULL, 16);
                    count = strtol(p.par2, NULL, 16);
                    printf("hexdump %04x %04x\n", address, count);
                    for (uint16_t i = 0; i < count; ++i) {
                        printf("%04x: ", (address +  16*i));
                        for (uint16_t j = 0; j < 16; ++j) {
                            printf("%02x ", *(memory + address + j + 16*i));
                        }
                        printf("   ");
                        for (uint16_t j = 0; j < 16; ++j) {
                            char c = (*(memory + address + j + 16*i));
                            printf("%c", (isprint(c)) ? c : '.');
                        }
                        printf("\n");
                    }
                }
                break;
            case 'h':
                {
                    printf("h                   -- this message\n");
                    printf("r [address]         -- run\n");
                    printf("x <address> [count] -- examine memory\n");
                    printf("d <address> [count] -- disassemble memory\n");
                    printf("n                   -- single step\n");
                    printf("s                   -- step into \n");
                    printf("l <filename>        -- load hex file\n");
                    printf("q                   -- quit\n");
                }
                break;
            case 'n':
                break;
            case 'q':
                do_monitor = false;
                break;
            default:
                printf("%s", commandline);
        }
    }
}

static void display_usage(void)
{
    printf("%s",
           "Usage:\n"
           "   fpemu <options>\n"
           "     -h             This message\n"
           "     -r <filename>  Hex file with ram image\n"
           "     -i <filename>  Device to for serial data input\n"
           "     -o <filename>  Device to use for serial data ouput\n"
           "     -m             Start in the monitor\n"
          );
}


static void parse_options(
        int argc, char** argv,
        char* input_file_name,
        char* output_file_name,
        char* memory_image_file_name,
        bool* start_in_monitor
        )
{
    char c;
    output_file_name[0] = '\0';
    input_file_name[0] = '\0';
    *start_in_monitor = false;

    while ((c = getopt(argc, argv, "hmi:o:r:")) != EOF) {
        switch (c) {
        case 'h':
            display_usage();
            exit(EXIT_SUCCESS);
            break;
        case 'm':
            *start_in_monitor = true;
            break;
        case 'o':
            strncpy(output_file_name, optarg, FPEM_MAX_FILENAME_LEN);
            break;
        case 'i':
            strncpy(input_file_name, optarg, FPEM_MAX_FILENAME_LEN);
            break;
        case 'r':
            strncpy(memory_image_file_name, optarg, FPEM_MAX_FILENAME_LEN);
            break;
        default:
            break;
        }
    }
}

int main(int argc, char** argv)
{
    struct CPU_Context c;
    memory = (uint8_t *)malloc(MEMORY_SIZE);
    static char input_file_name[FPEM_MAX_FILENAME_LEN + 2];
    static char output_file_name[FPEM_MAX_FILENAME_LEN + 2];
    static char memory_image_file_name[FPEM_MAX_FILENAME_LEN + 2];

    if (memory == NULL) {
        printf("Memory allocation failed\n");
    } else {
        int fd_in;   /* input channel from terminal */
        int fd_out;  /* ouput channel to the terminal */
        bool start_in_monitor;

        explicit_bzero(memory, MEMORY_SIZE);
        parse_options(
                argc, argv,
                input_file_name, output_file_name,
                memory_image_file_name,
                &start_in_monitor
                );

        if ((input_file_name[0] != '\0') &&
            (output_file_name[0] != '\0') &&
            (memory_image_file_name[0] != '\0')) {

            fd_in = open(input_file_name, O_RDONLY);
            if (fd_in >= 0) {
                fd_out = open(output_file_name, O_WRONLY);
                if (fd_out >= 0) {
                    if (load_hex(memory_image_file_name, memory)) {
                        printf("Loading completed\n");
                        char* starting = "FPEMU V0.0001\r\n\r\n";
                        unsigned n = strlen(starting);
                        c.fd_in = fd_in;
                        c.fd_out = fd_out;
                        write(fd_out, starting, n);
                        cpu_reset(&c);
                        if (start_in_monitor) {
                            monitor(&c, memory);
                        } else {
                            run(&c, memory);
                        }
                    } else { fprintf(stderr, "could not load program\n"); }

                } else { perror("open:"); }
                close(fd_in);
            } else { perror("open:"); }
        } else { printf("Options -i, -o, -r are all needed\n"); }
        free(memory);
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
