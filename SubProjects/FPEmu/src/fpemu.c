#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MEMORY_SIZE 65536
#define DSTACK_SIZE 16
#define RSTACK_SIZE 32
#define MAX_STACK_SIZE 64

/* Stack IDs */
#define DATA_STACK    0x00
#define RETURN_STACK  0x01
#define CONTROL_STACK 0x02
#define TEMP_STACK    0x03

enum ExceptionCode {
    StackOverflow = 1U,
    StackUnderflow,
    IllegalInstruction,
    IllegalStackID
};

char* exception_descriptions[] = {
    "None",
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
    struct Stack dstack;
    struct Stack rstack;
    struct Stack cstack;
    struct Stack tstack;
    uint16_t pc;
    uint16_t instruction;
};

static void cpu_reset(struct CPU_Context* c);
static void run(struct CPU_Context* c, char* memory);
static char* memory = NULL;

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
    c->dstack.size = DSTACK_SIZE;
    c->dstack.top  = 0U;
    c->rstack.size = RSTACK_SIZE;
    c->rstack.top  = 0U;
    c->pc          = 0x0000;  /* program counter */
    c->instruction = 0x0000; /* current instruction */
    c->keep_going = true;
    c->exception   = 0U;
}


struct Stack* get_stack(struct CPU_Context* c, uint16_t stack_id)
{
    struct Stack* stack;

    switch (stack_id) {
        case DATA_STACK:
            stack = &(c->dstack);
            break;
        case RETURN_STACK:
            stack = &(c->rstack);
            break;
        case CONTROL_STACK:
            stack = &(c->cstack);
            break;
        case TEMP_STACK:
            stack = &(c->tstack);
            break;
        default:
            stack = &(c->dstack);
            c->exception = IllegalStackID;
            c->keep_going = false;
    }

    return stack;
}


/*
   p   - 1/0  signed/unsigned
   ddd - data
   ss  - source stack 11/10/01/00
   tt  - target stack 11/10/01/00
       00 Data Stack
       01 Return Stack
       10 Control Stack
       11 Temp Stack
   ffff - function

   Bit
   111111
   5432109876543210
   ||||||||||||||||
   0000
   0001dddddddddddd BIF branch if false
   0010xxxxxxxxxxxx unused
   0011xxxxxxxxxxxx unused
   01dddddddddddddd ENTER
   1000ffffxxxxxxxx
       0000xxxxxxxx NOP
       0001xxxxxxxx LEAVE
       0010xxxxxxxx HALT
   1001xxxxxxxxxxxx unused
   1010xxxxxxxxxxxx unused
   1011ffffssttxxxx Stack operations
       0000         DROP ss
       0001         DUP ss
       0010         SWAP ss
       0011         MOV ss -> dd
   1100ssdddddddddd LDL LOAD Immediate low 10 bits
   1101ssddddddxxxx LDH LOAD Immediate high 6 bits
      |   |   |   |
   1110fffffxxpxxxx Operates on data stack (2 values)
       00000xx0xxxx ADDU (Unsinged)
       00000xx1xxxx ADD (Signed)
       00001xx0xxxx UMUL (Unsinged)
       00001xx1xxxx MUL (Signed)
       00010xx0xxxx AND
       00010xx1xxxx OR
       00011xx0xxxx XOR
       00011xx1xxxx
       00100xx0xxxx >> (Unsinged)
       00100xx1xxxx >> (Signed) Extends sign
       00101xx0xxxx LTU
       00101xx1xxxx LT
       00110xx0xxxx GTU
       00110xx1xxxx GT
       00111xx0xxxx LTEU
       00111xx1xxxx LTE
       01000xx0xxxx GTEU
       01000xx1xxxx GTE
       01001xx0xxxx <<
       01001xx1xxxx
       01010xxxxxxx STO.b
       01011xxxxxxx STO.w
       01100xxxxxxx STO.l
      |   |   |   |
   1111fffffxxxxxxx Operates on data stack (1 values)
       00000xxxxxxx NEG
       00111xxxxxxx NOT

 */

static void run(struct CPU_Context* c, char* memory)
{
    while(c->keep_going) {
        c->instruction = *(uint16_t*)(memory + c->pc);

        uint16_t group = (c->instruction & 0xF000);
        printf("%04x %04x\n", c->pc, c->instruction);
        switch (group) {
            case 0x1000:
                {
                    uint16_t truth_value;
                    struct Stack* stack = &(c->dstack);
                    truth_value = pop(c, stack);
                    if (truth_value) {
                        (c->pc) += 2;
                    } else {
                        int16_t offset = (c->instruction & 0x0FFF);
                        if (offset & 0x0800) {
                            /* sign extend */
                            offset = offset | 0xF000;
                            if (offset < 0) {
                                offset = (0 - offset);
                                c->pc -= (uint16_t)offset;
                            } else {
                                c->pc += offset;
                            }
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
                        case 0x0F00: /* HALT */
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
                    struct Stack* source_stack;
                    uint16_t func = (c->instruction & 0x0F00);
                    uint16_t source_stack_id = (c->instruction & 0x00C0) >> 6;
                    uint16_t dest_stack_id = (c->instruction & 0x0030) >> 4;
                    source_stack = get_stack(c, source_stack_id);
                    switch (func) {
                        case 0x0000: /* DROP */
                            (void)pop(c, source_stack);
                            break;
                        case 0x0100: /* DUP */
                            value = pop(c, source_stack);
                            push(c, source_stack, value);
                            push(c, source_stack, value);
                            break;
                        default:
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                    }
                    (c->pc) += 2;
                }
                break;
            case 0xC000: /* Load */
                {
                    struct Stack* stack;
                    uint16_t stack_id = (c->instruction & 0x0C00) >> 10;
                    uint16_t value = (c->instruction & 0x03FF);
                    stack = get_stack(c, stack_id);
                    push(c, stack, value);
                    (c->pc) += 2;
                }
                break;
            case 0xD000:
                c->exception = IllegalInstruction;
                c->keep_going = false;
                break;
            case 0xE000: /* 2 value operators */
                {
                    uint16_t func = (c->instruction & 0x0F80);
                    uint16_t is_signed = (c->instruction & 0x0010);
                    struct Stack* stack = &(c->dstack);
                    switch (func) {
                        case 0x0000: /* ADD(U) */
                            {
                               uint16_t v1, v2;

                                v1 = pop(c, stack);
                                v2 = pop(c, stack);
                                v1 = v1 + v2;
                                push(c, stack, v1);
                                // TODO Signed
                            }
                            break;
                        case 0x0300: /* GT(U) */
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
                        default:
                            c->exception = IllegalInstruction;
                            c->keep_going = false;
                    }
                    (c->pc) += 2;
                }
                break;
            default:
                c->exception = IllegalInstruction;
                c->keep_going = false;
        }
    }
    printf("Processor halted\n");
    printf("ExceptionCode: %d (%s)\n",
            c->exception, exception_descriptions[c->exception]);
    printf("Last instruction: %04X\n", c->instruction);
}

#define INPF_BUFFER_SIZE 1024

/*
 * Load an with fasm  assembled file
 */
bool load_hex(char* filename, char* memory)
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
        line = fgets(buffer, INPF_BUFFER_SIZE, inpf);
        while (line != NULL) {
            uint16_t* address;
            int location;
            int value;
            printf("%s", line);
            sscanf(line, "%x %x", &location, &value);
            printf("%x %x\n", location, value);
            address = (uint16_t*)(memory + location);
            *address = (uint16_t)value;
            line = fgets(buffer, INPF_BUFFER_SIZE, inpf);
        }
        ok = true;
    }
    return ok;
}

int main(int argc, char** argv)
{
    struct CPU_Context c;
    memory = (char *)malloc(MEMORY_SIZE);

    if (memory == NULL) {
        printf("Memory allocation failed\n");
    } else {
        explicit_bzero(memory, MEMORY_SIZE);

        if (argc > 1) {
            printf("%s\n", argv[1]);
            load_hex(argv[1], memory);
        }

        cpu_reset(&c);
        run(&c, memory);

        free(memory);
    }

    return EXIT_SUCCESS;
}

/* ------------------------ end of file -------------------------------*/
