/*

Author: Nate Killoran
2025 - 2026

#README

C/C++ Register Based Virtual Machine
Everything is contained in this one file
written completely from scratch, including all the string parsing

Video explaining the basic VM outline and the tests:
https://www.youtube.com/watch?v=Pp9-9z29x30

This is the third language I've written (previous 2 were created based off the book 'Crafting Interpreters' by Robert Nystrom)
I wanted to explore VM performance and design my own architecture from scratch.
Eventually this will power a hidden magic system in the game I'm creating, also from scratch.
The player will be able to write simple instructions that execute spells for various effects in game.

# COMPILING

To build it, copy to a .cpp file, such as 'vmtest.cpp' , and run the windows developer console,
navigate to the direction its saved in, and compile it
such as doing: 
'cl vmtest.cpp' OR 'clang vmtest.cpp' (clang will output to a.exe)
'./vmtest'


# WHAT HAPPENS
What will happen, is all the tests will run after another, printing all the operations and test results
Once those run, you will enter the repl mode, where you can type in your own commands to the console
So run the program, see the initial printf dump, and then type:

# REPL EXAMPLE (press enter after every line you type in)

LOAD $0 #1  
LOAD $1 #2
ADD $0 $1       (number stored in register 1 is added to register 2)
/registers      (this prints the values stored in every registe, register 0 should now hold 3 (1+2=3))
/program        (prints the hex representation of all the instructions so far)
/clear          (clears the registers and instructions)
/quit           (quits out of the application)



*/


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#if defined(__clang__)
    #define Assert(Expression) if(!(Expression)) { abort(); }
#elif defined(_MSC_VER)
    #define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#elif defined(__GNUC__)
    #define Assert(Expression) if(!(Expression)) { abort(); }
#endif

#define s64 signed long long int
#define u64 unsigned long long int
#define s32 signed long int
#define u32 unsigned long int
#define b32 unsigned long int
#define s16 signed short
#define u16 unsigned short
#define u8 unsigned char
#define s8 signed char
#define f32 float


#define MAX_ENTRIES 256
#define MAX_BUCKETS 4

static inline int handmade_strlen(const char* given){
    int size = 0;
    while(given[size] != '\0'/*  && given[size] != '\n' */){
        size++;
    } 
    return size;
}


static inline size_t handmade_strcpy(char* dest, const char* src){
    char* original_dest = dest;
    size_t count = 0;
    while(*src != '\0'){
        *dest = *src;
        dest++;
        src++;
        count++;
    }
    *dest = '\0';

    return count;
}


static inline size_t handmade_len_strcpy(char* dest, const char* src, size_t len){
    char* original_dest = dest;
    size_t count = 0;
    while(*src != '\0' && count < len){
        *dest = *src;
        dest++;
        src++;
        count++;
    }
    *dest = '\0';

    return count;
}



static inline bool handmade_strcmp(const char* given, const char* compared){
    size_t given_len = handmade_strlen(given);
    size_t compared_len = handmade_strlen(compared);
    
    if(given_len != compared_len)return false;
    
    for (size_t i = 0; i < given_len; i++)
    {
        if(given[i] != compared[i]){
            return false;
        }
    }
    return true;
}

static inline bool handmade_len_strcmp(const char* given, const char* compared, size_t len){
    
    for (size_t i = 0; i < len; i++)
    {
        if(given[i] != compared[i]){
            return false;
        }
    }
    return true;
}

static inline size_t handmade_strcat(char* given, const char* str1, const char* str2){
    size_t offset = handmade_strcpy(given, str1);
    offset += handmade_strcpy(given + offset, str2);
    return offset;
}

bool isNumeric(char c){
    bool result = (c >= '0' && c <= '9');
    return result;
}

bool isAlpha(char c){
    bool result = (c >= 'a' && c <= 'z' ||
                   c >= 'A' && c <= 'Z');
    return result;
}

bool isEndOfLine(char c){
    bool result = (c == '\n' || c == '\r');
    return result;
}

bool isWhiteSpace(char c){
    bool result = (c == ' ' || c == '\t' || isEndOfLine(c));
    return result;
}

static inline int lenToWhitespace(const char* given){
    int size = 0;
    while(given[size] != '\0'  && !isWhiteSpace(given[size])){
        size++;
    } 
    return size;
}

static inline size_t lenToNonNumber(const char* given){
    size_t size = 0;
    while(given[size] == '.' || given[size] == '-'  || given[size] == '+'  || isNumeric(given[size])){
        size++;
    } 
    return size;
}

int string_to_int(const char* str, const char** endOfNum){
    int result = 0; 
    const char* current = str;
    if(!current || *current == '\0' || *current == '/'){ //early return
        *endOfNum = current;
        return result;
    }

    bool negative = false;
    
    if(*current == '-'){
        negative = true;
        current++;
    }

    else if(*current == '+'){
        negative = false;
        current++;
    }
    
    const char* p = current;
    int integer = 0;
    while(isNumeric(*p)){
        //parse whole number, go as long as we have numbers

        if(integer > (INT_MAX - (*p - '0')) / 10){
            result = negative ? INT_MIN : INT_MAX;
            *endOfNum = p;
            return result;
        }

        integer = integer * 10 + (*p - '0');
        p++;
    }
    if(*p == '.'){
        float faction = 0.0f;
        float divisor = 1.0f;
        p++;
        while(isNumeric(*p)){
            //TODO: maybe add some checks to restrict the chars we parse if the fractional part has too many chars
            divisor *= 10.0f;
            faction = faction * 10.0f + (*p - '0');
            p++;            

        }

        //combine the parts
        float value = (float)integer + (faction / divisor);

        if(negative)value = -value;

        //we have a float, just convert to an int
        result = (int)value;

    }else{
        //we finished parsing the number
        int value = integer;
        if(negative)value = -value;
        result = value;
    }

    *endOfNum = p;
    
    return result;

}


uint32_t hash_string_len(const char* key, int len) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < len && key[i]; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }
    return (hash & MAX_ENTRIES - 1);
}



enum label_types {
    label_const_data,
    label_data,         //something like .LC0 for a variable
    label_code,         //something like main: to declare a function
    label_reference,    //the label name itself used in an instruction
};

struct symbol_table_entry {
    char* name; //where in the script its defined
    u32 nameLen;
    label_types type;
    u32 byteOffset; //where the data is, if its a code label its in the bytecode, if its data its in the data table
    b32 defined;
};

struct symbol_table {
    symbol_table_entry entries[MAX_ENTRIES][MAX_BUCKETS];
    u32 entry_count[MAX_ENTRIES];
    u32 total_entry_count;

};

void flush_env(symbol_table* env) {
    memset(env, 0, sizeof(symbol_table));
}

//use linear probing eventually, fixed buckets will break in the worst case of only buckets + 1 entries
symbol_table_entry* pushAssemblerSymbolTable(symbol_table* table, char* key, u32 keyLen) {
    if (key == NULL) {
        printf("push_env() given key is NULL!\n");
        return 0;
    }
    uint32_t hash = hash_string_len(key, keyLen);
    uint32_t bucket = 0;


    for (bucket = 0; bucket < MAX_BUCKETS; bucket++) {
        if (table->entries[hash][bucket].name == nullptr) {//found unoccupied bucket
            table->entries[hash][bucket].name = key;
            table->entry_count[hash]++;
            table->total_entry_count++;
            // printf("successfully added key %s at hash %u, entry count at hash: %d, total_entry_count: %d\n", key, hash, env->entry_count[hash], env->total_entry_count);
            return table->entries[hash] + bucket;
        }
        else if (handmade_len_strcmp(table->entries[hash][bucket].name, key, keyLen)) {
            // printf("KEY %s ALREADY EXISTS AT HASH %d IN SCOPE! ERROR!\n", key, hash);
            // return 0;
            // //allow redefinition
            // table->entries[hash][bucket].byteOffset = var;
            // printf("key %s ALREADY EXISTS at hash %u, entry count at hash: %d, total_entry_count: %d\n", key, hash, env->entry_count[hash], env->total_entry_count);
            return table->entries[hash] + bucket;
        }
    }

    printf("ERROR! symbol_table AT HASH %d FULL!!\n", hash);
    return 0;
}




int assign_symbol(symbol_table* table, char* key, u32 keyLen, u32 byteOffset) {
    if (key == NULL) {
        printf("push_env() given key is NULL!\n");
        return 0;
    }
    uint32_t hash = hash_string_len(key, keyLen);
    uint32_t bucket = 0;

    for (bucket = 0; bucket < MAX_BUCKETS; bucket++) {
        if (table->entries[hash][bucket].name == '\0') {//found unoccupied bucket
        }
        else if (handmade_strcmp(table->entries[hash][bucket].name, key)) {
            // printf("KEY %s ALREADY EXISTS AT HASH %d IN SCOPE! ERROR!\n", key, hash);
            // return 0;
            //allow redefinition
            table->entries[hash][bucket].byteOffset = byteOffset;
            // printf("key %s REASSIGNED at hash %u, entry count at hash: %d, total_entry_count: %d assigned to: ", key, hash, env->entry_count[hash], env->total_entry_count);
            // printf("key %s = ", key, hash, env->entry_count[hash], env->total_entry_count);
            // print_variable(env->entries[hash][bucket].var);
            return 1;
        }
    }

    printf("ERROR! symbol_table AT HASH %d NOT FOUND!!\n", hash);
    return 0;
}

symbol_table_entry* getAssemblerSymbolTableEntry(symbol_table* table, char* key, u32 keyLen) {
    if (key == NULL) {
        printf("get_env() given key is NULL!\n");
        return 0;
    }
    uint32_t hash = hash_string_len(key, keyLen);
    uint32_t bucket = 0;


    for (bucket = 0; bucket < MAX_BUCKETS; bucket++) {
        if (table->entries[hash][bucket].name == '\0') {//found unoccupied bucket
        }
        else if (handmade_len_strcmp(table->entries[hash][bucket].name, key, keyLen)) {
            // printf("KEY %s ALREADY EXISTS AT HASH %d IN SCOPE! ERROR!\n", key, hash);
            // return 0;
            //allow redefinition
            return table->entries[hash] + bucket;
        }
    }

    char tempBuffer[32];
    handmade_len_strcpy(tempBuffer, key, keyLen);
    printf("ERROR! COULDN'T FIND KEY %s at HASH %d!!\n", tempBuffer, hash);
    return 0;
}


#define MAX_REGISTERS 32
#define REGSP (MAX_REGISTERS - 1) //last register is the stack pointer
#define REGFP (MAX_REGISTERS - 2) //penultimate register is the frame pointer

#define MAX_BYTECODE 4096
#define MAX_MEM 256
#define STACK_START (MAX_MEM - 4)//each entry on the stack is 4 bytes
#define MAX_JUMPS 32 //no way to determine if we are in an infinite loop, no way to know when to reset jumps, so we hard limit how many jumps a program can make
#define MAX_REPL_BUFFER 2048
#define MAX_VM_MEM (8 * 1024 * 1024) //8 MB MAX

enum generic_opcode {
    GEN_LOAD,
    GEN_ADD,
    GEN_SUB,
    GEN_MUL,
    GEN_DIV,
    GEN_JEQ,
    GEN_JNE,
    GEN_EQ,
    GEN_JMP,
    GEN_COUNT,
};

enum Opcode {
    OP_HLT = 0, //halt

    OP_LOAD,
    OP_LOAD_REG_TO_REG,
    OP_LOAD_IMM_TO_REG,
    OP_LOAD_LABEL_TO_REG,
    OP_LOAD_REG_ADDR_TO_REG,
    OP_LOAD_OFFSET_REG_ADDR_TO_REG,
    OP_LOAD_REG_TO_REG_ADDR,
    OP_LOAD_CONST,
    OP_LOAD_DATA_ADDR_TO_ADDR,

    OP_LOAD_REG_TO_OFFSET_REG_ADDR,
    OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR,
    OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR,

    OP_LOAD_OFFSET_REG_ADDRESS_TO_REG,
    OP_LOAD_REG_TO_OFFSET_REG_ADDRESS,

    OP_ADD_REG_TO_REG,
    OP_SUB_REG_TO_REG,
    OP_MUL_REG_TO_REG,
    OP_DIV_REG_TO_REG,

    OP_ADD_CONSTANT_TO_REG,

    OP_JMP,
    OP_JMPF,
    OP_JMPB,

    OP_JMP_CONSTANT,
    OP_JMP_LABEL,
    OP_JEQ_CONSTANT,
    OP_JNE_CONSTANT,
    OP_JEQ_REG_TO_REG_CONSTANT,

    OP_JEQ_REGISTER_ADDRESS,

    OP_EQ,
    OP_NEQ,
    OP_GT,
    OP_LT,
    OP_GTQ,
    OP_LTQ,
    OP_JEQ_REG,//jump if equal
    OP_ALOC,

    OP_EQ_INDIRECT_REG_TO_REG,
    OP_EQ_CONST_TO_REG,

    OP_INC,
    OP_DEC,

    OP_PRT,
    OP_PRT_ADDRESS,
    OP_PRT_REG,

    OP_PUSH_REG,
    OP_POP_REG,
    OP_CALL,
    OP_RET,
    OP_SYSCALL,

    //probably not worth it, do we ever push multiple values at once?
    // OP_PUSH_REG_2,
    // OP_PUSH_REG_3,
    // OP_POP_REG_2,
    // OP_POP_REG_3,

    OP_COUNT, //just a placeholder to see how many opcodes we are at
    OP_ILGL = 255, //illegal

};

const char* opcodeStr(Opcode code){
    switch(code){
        case OP_HLT:{return "OP_HLT";}break;
        case OP_LOAD:{return "OP_LOAD";}break;
        case OP_LOAD_REG_TO_REG:{return "OP_LOAD_REG_TO_REG";}break;
        case OP_LOAD_IMM_TO_REG:{return "OP_LOAD_IMM_TO_REG";}break;
        case OP_LOAD_LABEL_TO_REG:{return "OP_LOAD_LABEL_TO_REG";}break;
        case OP_LOAD_REG_ADDR_TO_REG:{return "OP_LOAD_REG_ADDR_TO_REG";}break;
        case OP_LOAD_OFFSET_REG_ADDR_TO_REG:{return "OP_LOAD_OFFSET_REG_ADDR_TO_REG";}break;
        case OP_LOAD_REG_TO_REG_ADDR:{return "OP_LOAD_REG_TO_REG_ADDR";}break;
        case OP_LOAD_CONST:{return "OP_LOAD_CONST";}break;
        case OP_LOAD_DATA_ADDR_TO_ADDR:{return "OP_LOAD_DATA_ADDR_TO_ADDR";}break;
        case OP_LOAD_REG_TO_OFFSET_REG_ADDR:{return "OP_LOAD_REG_TO_OFFSET_REG_ADDR";}break;
        case OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR:{return "OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR";}break;
        case OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR:{return "OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR";}break;
        case OP_LOAD_OFFSET_REG_ADDRESS_TO_REG:{return "OP_LOAD_OFFSET_REG_ADDRESS_TO_REG";}break;
        case OP_LOAD_REG_TO_OFFSET_REG_ADDRESS:{return "OP_LOAD_REG_TO_OFFSET_REG_ADDRESS";}break;
        case OP_ADD_REG_TO_REG:{return "OP_ADD_REG_TO_REG";}break;
        case OP_SUB_REG_TO_REG:{return "OP_SUB_REG_TO_REG";}break;
        case OP_MUL_REG_TO_REG:{return "OP_MUL_REG_TO_REG";}break;
        case OP_DIV_REG_TO_REG:{return "OP_DIV_REG_TO_REG";}break;
        case OP_ADD_CONSTANT_TO_REG:{return "OP_ADD_CONSTANT_TO_REG";}break;
        case OP_JMP:{return "OP_JMP";}break;
        case OP_JMPF:{return "OP_JMPF";}break;
        case OP_JMPB:{return "OP_JMPB";}break;
        case OP_JMP_CONSTANT:{return "OP_JMP_CONSTANT";}break;
        case OP_JMP_LABEL:{return "OP_JMP_LABEL";}break;
        case OP_JEQ_CONSTANT:{return "OP_JEQ_CONSTANT";}break;
        case OP_JNE_CONSTANT:{return "OP_JNE_CONSTANT";}break;
        case OP_JEQ_REG_TO_REG_CONSTANT:{return "OP_JEQ_REG_TO_REG_CONSTANT";}break;
        case OP_JEQ_REGISTER_ADDRESS:{return "OP_JEQ_REGISTER_ADDRESS";}break;
        case OP_EQ:{return "OP_EQ";}break;
        case OP_NEQ:{return "OP_NEQ";}break;
        case OP_GT:{return "OP_GT";}break;
        case OP_LT:{return "OP_LT";}break;
        case OP_GTQ:{return "OP_GTQ";}break;
        case OP_LTQ:{return "OP_LTQ";}break;
        case OP_JEQ_REG:{return "OP_JEQ_REG";}break;
        case OP_ALOC:{return "OP_ALOC";}break;
        case OP_EQ_INDIRECT_REG_TO_REG:{return "OP_EQ_INDIRECT_REG_TO_REG";}break;
        case OP_EQ_CONST_TO_REG:{return "OP_EQ_CONST_TO_REG";}break;
        case OP_INC:{return "OP_INC";}break;
        case OP_DEC:{return "OP_DEC";}break;
        case OP_PRT:{return "OP_PRT";}break;
        case OP_PRT_ADDRESS:{return "OP_PRT_ADDRESS";}break;
        case OP_PRT_REG:{return "OP_PRT_REG";}break;
        case OP_PUSH_REG:{return "OP_PUSH_REG";}break;
        case OP_POP_REG:{return "OP_POP_REG";}break;
        case OP_CALL:{return "OP_CALL";}break;
        case OP_RET:{return "OP_RET";}break;
        case OP_SYSCALL:{return "OP_SYSCALL";}break;
        case OP_COUNT:{return "OP_COUNT";}break;
        case OP_ILGL:{return "OP_ILGL";}break;
        default:{return "";}break;
    }
    return "";
}

enum addressing_mode {
    ADDR_NONE,
    ADDR_REG, //$0
    ADDR_IMM, //#123 or 123
    ADDR_LABEL, //testLabel
    ADDR_REG_INDIRECT, //[$0]
    ADDR_REG_OFFSET, //[$0 + 4]
    ADDR_IMM_OFFSET, //[4]
    ADDR_MODE_COUNT, //used as a null value when looking up into the table
};



//ELF like header
//labor instruction executable header
//its a virtual machine, its not real, its a LIE ;^)
struct LIE {
    u32 magic;//magic number/identifier for the format, ASCII for ALIE, 0x414c4945
    u32 version;
    u64 codeStart;

};

struct AssemblerBackPatch {
    u32 byteCodeLocation;
    symbol_table_entry* hashEntry;
};


struct VM {
    s32 registers[MAX_REGISTERS];
    u8 bytecode[MAX_BYTECODE]; //the 'program' is stored here
    u32 byteCount;

    u8 mem[MAX_MEM];
    u32 memSize;

    u32 pc; //program counter, tracks which byte is executing
    u32 remainder;
    LIE lie; //header for the code, should be contained in the bytecode

    u32 jumpCount;
    bool equalFlag;

    symbol_table table;

    AssemblerBackPatch backPatchTable[256];
    u32 backPatchTableSize;//locations in the bytecode where we need to backpatch with the label we find
    u8 opLookupTable[GEN_COUNT][ADDR_MODE_COUNT][ADDR_MODE_COUNT][ADDR_MODE_COUNT];//very wasteful, but fits in a few KB, make into a hashmap
    int instructionsExecuted;
};





enum TokenTypes {
    //single character tokens
    TOK_LEFT_PAREN, TOK_RIGHT_PAREN,
    TOK_LEFT_BRACE, TOK_RIGHT_BRACE,
    TOK_LEFT_BRACKET, TOK_RIGHT_BRACKET,
    TOK_COMMA, TOK_DOT, TOK_MINUS, TOK_PLUS,
    TOK_SEMICOLON, TOK_COLON, TOK_SLASH, TOK_STAR,
    //one or two character tokens
    TOK_BANG, TOK_BANG_EQUAL,
    TOK_EQUAL, TOK_EQUAL_EQUAL,
    TOK_GREATER, TOK_GREATER_EQUAL,
    TOK_LESS, TOK_LESS_EQUAL,
    //literals
    TOK_IDENTIFIER, TOK_STRING, TOK_NUMBER, TOK_HEX,
    //keywords
    TOK_AND, TOK_STRUCT, TOK_ELSE, TOK_FALSE,
    TOK_FOR, TOK_FUN, TOK_IF, TOK_NIL, TOK_OR,
    TOK_PRINT, TOK_RETURN,
    TOK_TRUE, TOK_VAR, TOK_WHILE,

    //assembler tokens
    TOK_INSTRUCTION,
    TOK_LOAD, TOK_ADD, TOK_SUB, TOK_MUL, TOK_DIV, TOK_JMP,
    TOK_JMPF, TOK_JMPB, TOK_JEQ, TOK_JNE,
    TOK_EQ,
    TOK_NEQ,
    TOK_GT,
    TOK_LT,
    TOK_GTQ,
    TOK_LTQ,
    TOK_ALOC,
    TOK_HLT,
    TOK_INC,
    TOK_DEC,
    TOK_LABEL_USAGE,
    TOK_DIRECTIVE,
    TOK_RESB, //reserve bytes
    //repl commands
    TOK_COMMAND_REGISTERS, TOK_COMMAND_QUIT, TOK_COMMAND_HISTORY, TOK_COMMAND_PROGRAM,
    TOK_PRT,
    TOK_PUSH,
    TOK_POP,
    TOK_CALL,
    TOK_RET,
    TOK_SYSCALL,
    //assembler tokens
    TOK_REGISTER, TOK_INSTRUCTION_VALUE, TOK_RAW,

    TOK_ERROR, TOK_EOF, TOK_NEWLINE,
};


struct Token {
    TokenTypes type;
    const char* start;
    int length;
    int line;
};

struct Parser {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;

};

struct Scanner {
    char* start;
    char* current;
    int line;
    char* lines[256];
};

void printScannerLine(Scanner* scanner, int line){
    char temp[256] = {};
    char* str = scanner->lines[line];
    int count = 0;
    //skip initial whitespace between newline and first char
    while(*str != 0 && (*str == ' ' || *str == '\t')){
        str++;
    }
    while(*str != 0 && *str != '\n'){
        temp[count] = *str;
        str++;
        count++;
        Assert(count < 256);
    }   
                                      //BOLD RED                        //RESET
    printf("%s%-38s || LINE : %d%s\n","\033[1;31m", temp, line,"\033[0m");
}


enum Precedence{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,        // or
    PREC_AND,   //and
    PREC_EQUALITY,// == !=
    PREC_COMPARISON,//  < > <= >=
    PREC_TERM,// + -
    PREC_FACTOR,//* /
    PREC_UNARY,// ! - 
    PREC_CALL,//. ()
    PREC_PRIMARY,
};

typedef void (*ParseFn)(VM* vm, Parser* parser, Scanner* scanner, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
}ParseRule;




struct REPL {
    Scanner scanner;
    Parser parser;
    VM vm;
    char history[MAX_REPL_BUFFER * MAX_REPL_BUFFER]; //stores each line submitted in the current session
    size_t historyLines;
};



void init_opcode_lookup(VM* vm) {
    memset(vm->opLookupTable, 255, sizeof(vm->opLookupTable));

    vm->opLookupTable[GEN_LOAD][ADDR_REG][ADDR_REG][ADDR_NONE] = OP_LOAD_REG_TO_REG;
    vm->opLookupTable[GEN_LOAD][ADDR_REG][ADDR_IMM][ADDR_NONE] = OP_LOAD_IMM_TO_REG;
    vm->opLookupTable[GEN_LOAD][ADDR_REG][ADDR_LABEL][ADDR_NONE] = OP_LOAD_IMM_TO_REG;
    vm->opLookupTable[GEN_LOAD][ADDR_REG][ADDR_REG_INDIRECT][ADDR_NONE] = OP_LOAD_REG_ADDR_TO_REG;
    vm->opLookupTable[GEN_LOAD][ADDR_REG][ADDR_REG_OFFSET][ADDR_NONE] = OP_LOAD_OFFSET_REG_ADDR_TO_REG;
    vm->opLookupTable[GEN_LOAD][ADDR_REG_INDIRECT][ADDR_REG][ADDR_NONE] = OP_LOAD_REG_TO_REG_ADDR;
    vm->opLookupTable[GEN_LOAD][ADDR_REG_OFFSET][ADDR_REG][ADDR_NONE] = OP_LOAD_REG_TO_OFFSET_REG_ADDR;
    vm->opLookupTable[GEN_LOAD][ADDR_REG_INDIRECT][ADDR_REG_INDIRECT][ADDR_NONE] = OP_LOAD_DATA_ADDR_TO_ADDR;
    vm->opLookupTable[GEN_LOAD][ADDR_REG_INDIRECT][ADDR_REG_OFFSET][ADDR_NONE] = OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR;
    vm->opLookupTable[GEN_LOAD][ADDR_REG_OFFSET][ADDR_REG_INDIRECT][ADDR_NONE] = OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR;
    vm->opLookupTable[GEN_ADD][ADDR_REG][ADDR_REG][ADDR_REG] = OP_ADD_REG_TO_REG;
    vm->opLookupTable[GEN_SUB][ADDR_REG][ADDR_REG][ADDR_REG] = OP_SUB_REG_TO_REG;
    vm->opLookupTable[GEN_MUL][ADDR_REG][ADDR_REG][ADDR_REG] = OP_MUL_REG_TO_REG;
    vm->opLookupTable[GEN_DIV][ADDR_REG][ADDR_REG][ADDR_REG] = OP_DIV_REG_TO_REG;
    vm->opLookupTable[GEN_JEQ][ADDR_REG][ADDR_NONE][ADDR_NONE] = OP_JEQ_REG;
    vm->opLookupTable[GEN_JEQ][ADDR_IMM][ADDR_NONE][ADDR_NONE] = OP_JEQ_CONSTANT;
    vm->opLookupTable[GEN_JNE][ADDR_IMM][ADDR_NONE][ADDR_NONE] = OP_JNE_CONSTANT;
    vm->opLookupTable[GEN_JEQ][ADDR_REG][ADDR_REG][ADDR_IMM] = OP_JEQ_REG_TO_REG_CONSTANT;
    vm->opLookupTable[GEN_EQ][ADDR_REG][ADDR_REG][ADDR_NONE] = OP_EQ;
    vm->opLookupTable[GEN_EQ][ADDR_REG_INDIRECT][ADDR_REG][ADDR_NONE] = OP_EQ_INDIRECT_REG_TO_REG;
    vm->opLookupTable[GEN_EQ][ADDR_IMM][ADDR_REG][ADDR_NONE] = OP_EQ_CONST_TO_REG;
    vm->opLookupTable[GEN_JMP][ADDR_REG][ADDR_NONE][ADDR_NONE] = OP_JMP;
    vm->opLookupTable[GEN_JMP][ADDR_IMM][ADDR_NONE][ADDR_NONE] = OP_JMP_CONSTANT;
    vm->opLookupTable[GEN_JMP][ADDR_LABEL][ADDR_NONE][ADDR_NONE] = OP_JMP_LABEL;
}


void reset_vm(VM* vm) {
    memset(vm, 0, sizeof(VM));
    init_opcode_lookup(vm);
    vm->registers[REGSP] = STACK_START;
}


Opcode decodeOpcode(VM* vm) {
    return (Opcode)vm->bytecode[vm->pc++];
}


inline u8 nextByte(VM& vm) {
    u8 val = ((vm.bytecode[vm.pc]));
    vm.pc += 1;
    return val;
}


inline u16 next2Bytes(VM& vm) {
    u16 val = ((vm.bytecode[vm.pc] << 8) | (vm.bytecode[vm.pc + 1]));
    vm.pc += 2;
    return val;
}

inline u32 next3Bytes(VM& vm) {
    u32 val = ((vm.bytecode[vm.pc] << 16) | (vm.bytecode[vm.pc + 1] << 8) | (vm.bytecode[vm.pc + 2]));
    vm.pc += 3;
    return val;
}

inline void vmMemError(VM& vm, const char* message, u32 instructionLocation, s32 memLocation, u32 maxMem) {
    printf("[VM ERROR]: %s, instruction: %lu, memory address: %ld, max memory size: %lu\n", message, instructionLocation, memLocation, maxMem);
}

inline void vmError(VM& vm, const char* message, u32 instructionLocation) {
    printf("[VM ERROR]: %s, instruction: %lu\n", message, instructionLocation);
}

inline bool executeInstruction(VM& vm) {
    u32 currentByte = vm.pc;
    if (vm.pc >= vm.byteCount) {
        printf("program counter: %lu, exceeds byteCount: %lu, returning\n", vm.pc, vm.byteCount);
        return true;
    }

    switch (vm.bytecode[vm.pc++]) {
      
    case OP_HLT: {
        printf("%2lu: HLT ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        return true;
    }break;
    case OP_ILGL: {
        printf("%2lu: IGL ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        return true;
    }break;
    case OP_LOAD_REG_TO_REG: {
        printf("%2lu: LOAD REG2REG ENCOUNTERED at pc %lu   :    ", currentByte, vm.pc - 1);
        u8 reg1 = vm.bytecode[vm.pc++];
        u8 reg2 = nextByte(vm);
        vm.pc++;
        vm.registers[reg1] = vm.registers[reg2];
        printf("LOAD $%u $%u\n", reg1, reg2);
        return false;
    }break;
    case OP_LOAD_IMM_TO_REG: {
        printf("%2lu: LOAD ENCOUNTERED at pc %2lu   :    ", currentByte, vm.pc - 1);
        u8 reg = vm.bytecode[vm.pc++];
        // u8 num1 = ((vm.bytecode[vm.pc]));
        // u8 num2 = ((vm.bytecode[vm.pc+1]));
        // u16 val = ((vm.bytecode[vm.pc] << 8) | (vm.bytecode[vm.pc+1]));
        // vm.pc += 2;
        u16 val = next2Bytes(vm);
        vm.registers[reg] = val;
        printf("LOAD $%u #%u\n", reg, val);
        return false;
    }break;
    case OP_ADD_REG_TO_REG: {
        printf("%2lu: ADD ENCOUNTERED at pc %2lu    :    ", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        u8 destreg = nextByte(vm);
        printf("ADD $%u $%u $%u, \t %lu + %lu = %lu\n", reg1, reg2, destreg, vm.registers[reg1], vm.registers[reg2], vm.registers[reg1] + vm.registers[reg2]);
        vm.registers[destreg] = vm.registers[reg1] + vm.registers[reg2];
        return false;

    }break;
    case OP_SUB_REG_TO_REG: {
        printf("%2lu: SUB ENCOUNTERED at pc %2lu   :    ", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        u8 destreg = nextByte(vm);
        printf("SUB $%u $%u $%u, \t %lu - %lu = %lu\n", reg1, reg2, destreg, vm.registers[reg1], vm.registers[reg2], vm.registers[reg1] - vm.registers[reg2]);
        vm.registers[destreg] = vm.registers[reg1] - vm.registers[reg2];
        return false;

    }break;
    case OP_MUL_REG_TO_REG: {
        printf("%2lu: MUL ENCOUNTERED at pc %lu  :     ", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        u8 destreg = nextByte(vm);
        printf("MUL $%u $%u $%u, \t %lu * %lu = %lu\n", reg1, reg2, destreg, vm.registers[reg1], vm.registers[reg2], vm.registers[reg1] * vm.registers[reg2]);
        vm.registers[destreg] = vm.registers[reg1] * vm.registers[reg2];
        return false;

    }break;
    case OP_DIV_REG_TO_REG: {
        printf("%2lu: DIV ENCOUNTERED at pc %lu   :    ", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        u8 destreg = nextByte(vm);
        if (vm.registers[reg2] == 0) {
            printf("$%ld is 0!\n", vm.registers[reg2]);
            vmError(vm, "DIVISION BY 0!", currentByte);
            return true;
        }
        vm.remainder = vm.registers[reg1] % vm.registers[reg2];
        vm.registers[29] = vm.remainder; //just make register 29 the result of the modulow for now
        vm.registers[destreg] = vm.registers[reg1] / vm.registers[reg2];
        printf("DIV $%u $%u $%u, \t %lu / %lu = %lu remainder %lu\n", reg1, reg2, destreg, vm.registers[reg1], vm.registers[reg2], vm.registers[reg1] / vm.registers[reg2], vm.remainder);
        return false;

    }break;
    case OP_JMP: {
        printf("%2lu: JMP ENCOUNTERED at pc %lu, jumpCount: %lu \n", currentByte, vm.pc - 1, vm.jumpCount + 1);
        u8 reg = nextByte(vm);
        vm.pc = vm.registers[reg];
        vm.jumpCount++;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            vmError(vm, "JUMPED TO INVALID MEMORY", currentByte);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            vmError(vm, "MAX JUMPS REACHED!", currentByte);
            return true;
        }
        return false;
    }break;
    case OP_JMPF: {
        printf("%2lu: JMPF ENCOUNTERED at pc %lu, jumpCount: %lu\n", currentByte, vm.pc - 1, vm.jumpCount + 1);
        u8 reg = nextByte(vm);
        vm.pc += vm.registers[reg] - 2;// - 2 to account for the first 2 instructions we've already executed
        vm.jumpCount++;
        printf("JMPF %lu\n", vm.registers[reg]);
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            vmError(vm, "JUMPED TO INVALID MEMORY", currentByte);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            vmError(vm, "MAX JUMPS REACHED!", currentByte);
            return true;
        }
        return false;
    }break;
    case OP_JMPB: {
        printf("%2lu: JMPB ENCOUNTERED at pc %lu, jumpCount: %lu\n", currentByte, vm.pc - 1, vm.jumpCount + 1);
        u8 reg = nextByte(vm);
        vm.pc -= vm.registers[reg] + 2;// - 2 to account for the first 2 instructions we've already executed
        printf("JMPB %lu\n", vm.registers[reg]);
        vm.jumpCount++;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            vmError(vm, "JUMPED TO INVALID MEMORY", currentByte);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            vmError(vm, "MAX JUMPS REACHED!", currentByte);
            return true;
        }
        return false;
    }break;
    case OP_EQ: {
        printf("%2lu: EQ ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        if (vm.registers[reg1] == vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;
    case OP_NEQ: {
        printf("%2lu: NEQ ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        if (vm.registers[reg1] != vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;

    case OP_GT: {
        printf("%2lu: GT ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        if (vm.registers[reg1] > vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;
    case OP_LT: {
        printf("%2lu: LT ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        printf("$%d < $%d = %d < %d\n", reg1, reg2, vm.registers[reg1], vm.registers[reg2]);
        if (vm.registers[reg1] < vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;
    case OP_GTQ: {
        printf("%2lu: GTQ ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        if (vm.registers[reg1] >= vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;
    case OP_LTQ: {
        printf("%2lu: LTQ ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        if (vm.registers[reg1] <= vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;

    case OP_JEQ_REG: {
        printf("%2lu: JEQ ENCOUNTERED at pc %lu, jumpCount: %lu   :   ", currentByte, vm.pc - 1, vm.jumpCount);
        u8 reg1 = nextByte(vm);
        s32 target = vm.registers[reg1];
        printf("JEQ %lu, equalFlag: %d\n", target, vm.equalFlag);
        if (vm.equalFlag) {
            vm.equalFlag = false;
            vm.jumpCount++;
            printf("equalFlag is TRUE, JUMPING!\n");
            vm.pc = target;
            if (vm.pc >= vm.byteCount) {
                printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
                return true;
            }
            if (vm.jumpCount >= MAX_JUMPS) {
                printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
                return true;
            }
        }
        else {
            vm.equalFlag = false;
            printf("equalFlag is FALSE, not jumping\n");
            vm.pc += 2;//need to pad out to the next instruction
        }

        return false;
    }break;

    case OP_ALOC: {
        // printf("ALOC ENCOUNTERED at pc %u\n", vm.pc-1);
        // u8 reg1 = nextByte(vm);
        // s32 val = vm.registers[reg1];
        // vm.pc+=2;//need to pad out to the next instruction
        // if(vm.heapSize + val >= MAX_VM_MEM){
        //     printf("CANNOT ALLOCATE MORE MEMORY! QUITTING\n");
        //     return true;
        // }
        // vm.heapSize += val;
        return false;
    }break;

    case OP_INC: {
        u8 reg1 = nextByte(vm);
        Assert(reg1 >= 0 && reg1 < 32);
        vm.registers[reg1]++;
        printf("%2lu: INC ENCOUNTERED at pc %lu, vm.registers[$%u] is now %ld\n", currentByte, currentByte, reg1, vm.registers[reg1]);
        vm.pc += 2;//need to pad out to the next instruction
        return false;
    }break;
    case OP_DEC: {
        u8 reg1 = nextByte(vm);
        Assert(reg1 >= 0 && reg1 < 32);
        vm.registers[reg1]--;
        printf("%2lu: DEC ENCOUNTERED at pc %lu, vm.registers[$%u] is now %ld\n", currentByte, currentByte, reg1, vm.registers[reg1]);
        vm.pc += 2;//need to pad out to the next instruction
        return false;
    }break;

    //LOAD [$0 + 4] [$1]
    case OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR: {
        printf("%2lu: LOAD REG ADDR TO OFFSET REG ADDR ENCOUNTERED at pc %lu :    ", currentByte, vm.pc - 1);

        u8 reg1 = vm.bytecode[vm.pc++];
        u8 offset = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];

        if (vm.registers[reg1] + offset >= MAX_MEM || vm.registers[reg1] + offset < 0) {
            printf("LOAD memory offset addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg1] + offset, MAX_MEM);
            return true;
        }

        if (vm.registers[reg2] >= MAX_MEM || vm.registers[reg2] < 0) {
            printf("LOAD memory addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg2], MAX_MEM);
            return true;
        }


        vm.mem[vm.registers[reg1] + offset] = vm.mem[vm.registers[reg2]];

        printf("LOAD [$%u + %u] [$%u]\n", reg1, offset, reg2);
        return false;
    }break;

    //LOAD $0 [$1 + 4]
    case OP_LOAD_OFFSET_REG_ADDR_TO_REG: {
        printf("%2lu: LOAD OFFSET REG ADDR TO REG ENCOUNTERED at pc %lu :    ", currentByte, vm.pc - 1);

        u8 reg1 = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];
        u8 offset = vm.bytecode[vm.pc++];

        vm.registers[reg1] = vm.mem[vm.registers[reg2] + offset];

        if (vm.registers[reg2] + offset >= MAX_MEM || vm.registers[reg2] + offset < 0) {
            printf("LOAD memory offset addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg2] + offset, MAX_MEM);
            return true;
        }
        printf("LOAD $%u [$%u + %u]\n", reg1, reg2, offset);
        return false;
    }break;
    //LOAD [$1 + 4] $0
    case OP_LOAD_REG_TO_OFFSET_REG_ADDR: {
        printf("%2lu: OP_LOAD_REG_TO_OFFSET_REG_ADDR ENCOUNTERED at pc %lu :    ", currentByte, vm.pc - 1);
        u8 reg1 = vm.bytecode[vm.pc++];
        u8 offset = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];
        vm.mem[vm.registers[reg1] + offset] = vm.registers[reg2];
        if (vm.registers[reg1] + offset >= MAX_MEM || vm.registers[reg1] + offset < 0) {
            printf("LOAD memory offset addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg2] + offset, MAX_MEM);
            return true;
        }
        printf("LOAD [$%u + %u] $%u \n", reg1, offset, reg2);
        return false;
    }break;

    case OP_LOAD_REG_TO_REG_ADDR:{
        printf("%2lu: LOAD REG TO REG ADDR ENCOUNTERED at pc %lu :    ", currentByte, vm.pc - 1);
        u8 reg1 = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];
        u8 offset = vm.bytecode[vm.pc++];

        if (vm.registers[reg1] + offset >= MAX_MEM || vm.registers[reg1] + offset < 0) {
            printf("LOAD memory offset addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg1] + offset, MAX_MEM);
            return true;
        }

        if (vm.registers[reg2] >= MAX_MEM || vm.registers[reg2] < 0) {
            printf("LOAD memory addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg2], MAX_MEM);
            return true;
        }

        vm.mem[vm.registers[reg1] + offset] = vm.registers[reg2];
        printf("LOAD [$%u + %u] $%u \n", reg1, offset, reg2);
        return false;
    }break;
    //LOAD [$0] [$1 + 4]
    case OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR: {
        printf("%2lu: LOAD OFFSET REG ADDR TO REG ADDR ENCOUNTERED at pc %lu :    ", currentByte, vm.pc - 1);
        u8 reg1 = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];
        u8 offset = vm.bytecode[vm.pc++];

        if (vm.registers[reg2] + offset >= MAX_MEM || vm.registers[reg2] + offset < 0) {
            printf("LOAD memory offset addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg2] + offset, MAX_MEM);
            return true;
        }

        if (vm.registers[reg1] >= MAX_MEM || vm.registers[reg1] < 0) {
            printf("LOAD memory addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, vm.registers[reg1], MAX_MEM);
            return true;
        }

        vm.mem[vm.registers[reg1]] = vm.mem[vm.registers[reg2] + offset];
        printf("LOAD [$%u] [$%u + %u]\n", reg1, reg2, offset);
        return false;
    }break;


    case OP_LOAD_DATA_ADDR_TO_ADDR: {
        // __debugbreak();
        printf("%2lu: LOAD DATA ADDRESS TO ADDRESS ENCOUNTERED at pc %lu   :    ", currentByte, vm.pc - 1);

        u8 reg1 = vm.bytecode[vm.pc++];
        u8 reg2 = vm.bytecode[vm.pc++];
        vm.pc++;


        s32 val1 = vm.registers[reg1];
        s32 val2 = vm.registers[reg2];

        // vm.registers[reg] = val;

        if (val1 >= MAX_MEM || val1 < 0) {
            printf("LOAD memory addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, val1, MAX_MEM);
            return true;
        }

        if (val2 >= MAX_MEM || val2 < 0) {
            printf("LOAD memory addressing error!\n");
            vmMemError(vm, "Attempting to address memory out of bounds!", currentByte, val2, MAX_MEM);
            return true;
        }

        printf("LOAD DATA ADDRESS TO ADDRESS [$%u] [$%u] | %c set with %c\n", reg1, reg2, vm.mem[val1], vm.mem[val2]);
        vm.mem[val1] = vm.mem[val2];
        return false;
    }break;

    case OP_JMP_CONSTANT: {
        // __debugbreak();
        printf("%2lu: JMP CONSTANT ENCOUNTERED at pc %lu   :    ", currentByte, vm.pc - 1);
        u32 target = next2Bytes(vm);
        vm.jumpCount++;
        printf("equalFlag is TRUE, JUMPING!\n");
        vm.pc = target;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            return true;
        }
        return false;
    }break;


    case OP_JMP_LABEL: {//need to differentiate from regular constant since labels get backpatched with all 4 bytes
        // __debugbreak();
        printf("%2lu: JMP LABEL ENCOUNTERED at pc %lu   :    ", currentByte, vm.pc - 1);
        u32 target = next3Bytes(vm);
        vm.jumpCount++;
        printf("equalFlag is TRUE, JUMPING!\n");
        vm.pc = target;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            return true;
        }
        return false;
    }break;

    case OP_JEQ_CONSTANT: {
        // __debugbreak();
        printf("%2lu: JEQ ENCOUNTERED at pc %lu, jumpCount: %lu   :    ", currentByte, vm.pc - 1, vm.jumpCount);
        u16 target = next2Bytes(vm);

        printf("JEQ %u, equalFlag: %d, target: %d\n", target, vm.equalFlag, target);
        if (vm.equalFlag) {
            vm.equalFlag = false;
            vm.jumpCount++;
            printf("equalFlag is TRUE, JUMPING!\n");
            vm.pc = target;
            if (vm.pc >= vm.byteCount) {
                printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
                return true;
            }
            if (vm.jumpCount >= MAX_JUMPS) {
                printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
                return true;
            }
        }
        else {
            vm.equalFlag = false;
            printf("equalFlag is FALSE, not jumping\n");
            vm.pc += 1;//need to pad out to the next instruction
        }

        return false;
    }break;

    case OP_JNE_CONSTANT: {
        // __debugbreak();
        printf("%2lu: JNE ENCOUNTERED at pc %lu, jumpCount: %lu   :    ", currentByte, vm.pc - 1, vm.jumpCount);
        u16 target = next2Bytes(vm);
        printf("JNE %u, equalFlag: %d, target: %d\n", target, vm.equalFlag, target);
        if (!vm.equalFlag) {
            vm.jumpCount++;
            printf("equalFlag is FALSE, JUMPING!\n");
            vm.pc = target;
            if (vm.pc >= vm.byteCount) {
                printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
                return true;
            }
            if (vm.jumpCount >= MAX_JUMPS) {
                printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
                return true;
            }
        }
        else {
            vm.equalFlag = false;
            printf("equalFlag is FALSE, not jumping\n");
            vm.pc += 1;//need to pad out to the next instruction
        }
        return false;
    }break;

    case OP_JEQ_REG_TO_REG_CONSTANT: {
        // __debugbreak();
        printf("%2lu: JEQ REG TO REG ENCOUNTERED at pc %lu, jumpCount: %lu   :    ", currentByte, vm.pc - 1, vm.jumpCount);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        u8 target = nextByte(vm);
        if (vm.registers[reg1] == vm.registers[reg2]) {
            vm.equalFlag = false;
            vm.jumpCount++;
            printf("$%d == $%d, JUMPING!\n", reg1, reg2);
            vm.pc = target;
            if (vm.pc >= vm.byteCount) {
                printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
                return true;
            }
            if (vm.jumpCount >= MAX_JUMPS) {
                printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
                return true;
            }
        }
        else {
            vm.equalFlag = false;
            printf("$%d != $%d, not jumping!\n", reg1, reg2);
        }

        return false;
    }break;

    case OP_EQ_CONST_TO_REG: {
        printf("%2lu: OP_EQ_CONST_TO_REG ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u16 val = next2Bytes(vm);
        if (vm.registers[reg1] == val)vm.equalFlag = true;
        else vm.equalFlag = false;
        return false;
    }break;

    case OP_EQ_INDIRECT_REG_TO_REG: {
        printf("%2lu: OP_EQ_INDIRECT_REG_TO_REG ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);
        u8 reg1 = nextByte(vm);
        u8 reg2 = nextByte(vm);
        s32 val1 = vm.registers[reg1];

        if (vm.mem[val1] == vm.registers[reg2])vm.equalFlag = true;
        else vm.equalFlag = false;
        vm.pc++;//need to pad out to the next instruction
        return false;
    }break;

    case OP_PRT_ADDRESS: {
        // __debugbreak();
        printf("%2lu: PRT ADDRESS ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);

        u8 reg1 = vm.bytecode[vm.pc++];
        s32 val1 = vm.registers[reg1];
        vm.pc += 2;
        printf("PRT: %s\n", (char*)vm.mem + val1);
        return false;
    }break;
    case OP_PRT_REG: {
        // __debugbreak();
        printf("%2lu: PRT REG ENCOUNTERED at pc %lu\n", currentByte, vm.pc - 1);

        u8 reg1 = vm.bytecode[vm.pc++];
        s32 val1 = vm.registers[reg1];
        vm.pc += 2;
        printf("PRT: %ld\n", val1);
        return false;
    }break;
                   //TODO: the way we manipulate the stack will not port to big endian hardware, will need to test on it if we ever need to 
    case OP_PUSH_REG: {
        u8 reg1 = nextByte(vm);

        *((s32*)(vm.mem + vm.registers[REGSP])) = vm.registers[reg1];

        if ((vm.registers[REGSP] - 4) < 0) {//0 or wherever the labels end
            __debugbreak();
            vmError(vm, "POP ERROR! STACK TOO LARGE!", currentByte);
            return true;
        }
        Assert(vm.registers[REGSP] >= 0);

        vm.registers[REGSP] -= 4;//move the stack in sections of 4 bytes
        printf("%2lu: PUSH ENCOUNTERED at pc %lu, pushed %ld onto stack\n", currentByte, currentByte, vm.registers[reg1]);

        vm.pc += 2;
        return false;
    }break;
    case OP_POP_REG: {

        u8 reg1 = nextByte(vm);

        if ((vm.registers[REGSP] + 4) > (MAX_MEM - 4)) {
            __debugbreak();
            vmError(vm, "POP ERROR! STACK TOO SMALL!", currentByte);
            return true;
        }
        Assert(vm.registers[REGSP] <= (MAX_MEM - 4));

        vm.registers[REGSP] += 4;//move the stack in sections of 4 bytes
        vm.registers[reg1] = *((s32*)(vm.mem + vm.registers[REGSP]));
        printf("%2lu: POP ENCOUNTERED at pc %lu, popped %ld onto $%u\n", currentByte, currentByte, vm.registers[reg1], reg1);



        vm.pc += 2;

        return false;
    }break;

    case OP_CALL: {
        printf("%2lu: CALL ENCOUNTERED at pc %lu\n", currentByte, currentByte);

        //push next instruction location to the stack and then jump
        *((s32*)(vm.mem + vm.registers[REGSP])) = (vm.pc - 1) + 4;

        if ((vm.registers[REGSP] - 4) < 0) {//0 or wherever the labels end
            __debugbreak();
            vmError(vm, "POP ERROR! STACK TOO LARGE!", currentByte);
            return true;
        }
        Assert(vm.registers[REGSP] >= 0);
        vm.registers[REGSP] -= 4;//move the stack in sections of 4 bytes


        u32 target = next3Bytes(vm);
        vm.pc = target;
        vm.jumpCount++;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            vmError(vm, "JUMPED TO INVALID MEMORY", currentByte);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            vmError(vm, "MAX JUMPS REACHED!", currentByte);
            return true;
        }

        return false;
    }break;

    case OP_RET: {
        printf("%2lu: RET ENCOUNTERED at pc %lu\n", currentByte, currentByte);

        if ((vm.registers[REGSP] + 4) > (MAX_MEM - 4)) {
            __debugbreak();
            vmError(vm, "POP ERROR! STACK TOO SMALL!", currentByte);
            return true;
        }
        Assert(vm.registers[REGSP] <= (MAX_MEM - 4));

        vm.registers[REGSP] += 4;//move the stack in sections of 4 bytes

        u32 target = *((s32*)(vm.mem + vm.registers[REGSP]));

        //do we need to increment the jump count for returns?
        vm.pc = target;
        vm.jumpCount++;
        if (vm.pc >= vm.byteCount) {
            printf("JUMPED TO INVALID MEMORY %lu, EXITING\n", vm.pc);
            vmError(vm, "JUMPED TO INVALID MEMORY", currentByte);
            return true;
        }
        if (vm.jumpCount >= MAX_JUMPS) {
            printf("MAX JUMPS %u REACHED! EXITING EXECUTION!\n", MAX_JUMPS);
            vmError(vm, "MAX JUMPS REACHED!", currentByte);
            return true;
        }



        return false;
    }break;

    case OP_SYSCALL: {
        printf("%2lu: SYSCALL ENCOUNTERED at pc %lu\n", currentByte, currentByte);

        vm.pc += 3;

        switch (vm.registers[0]) {
        case 0: {

            printf("arg1 (reg1): %ld\n", vm.registers[1]);
            switch (vm.registers[1]) {
            case 42: {
                printf("42 is the meaning of life\n");
            }break;
            case 13: {
                printf("CAST PROJECTILE\n");
            }break;
            case 14: {
                printf("CAST AREA OF EFFECT\n");
            }break;

            default: {}

            }
            printf("arg2 (reg2): %ld\n", vm.registers[2]);
        }break;
        default: {
            printf("unhandled syscall parameter\n");
        }
        }
        return false;
    }break;
                   // case OP_EXAMPLE:{
                   //     return false;
                   // }break;


    default:
        printf("UNKNOWN OPCODE! %u %s\n", vm.bytecode[vm.pc], opcodeStr((Opcode)vm.bytecode[vm.pc]));
        Assert(!"invalid opcode!");
        return true;
    }
}

void vm_run(VM& vm, Scanner* scanner = NULL) {
    bool isDone = false;
    vm.instructionsExecuted = 0;
    while (!isDone) {

        if(scanner){
            printf("EXECUTION COUNT: %d || ", vm.instructionsExecuted);
            printScannerLine(scanner, (vm.pc/4)+1);
        }

        isDone = executeInstruction(vm);
        
        vm.instructionsExecuted++;
    }
}

void vm_run_once(VM& vm) {
    bool isDone = executeInstruction(vm);
}

void test_reset_vm() {
    VM vm = {};
    reset_vm(&vm);

    for (int i = 0; i < 32; i++)Assert(vm.registers[i] == 0);
}




// PARSER START


const char* tok_to_str(TokenTypes type) {
    switch (type) {
        case TOK_LEFT_PAREN: return "TOK_LEFT_PAREN ";
        case TOK_RIGHT_PAREN: return "TOK_RIGHT_PAREN ";
        case TOK_LEFT_BRACE: return "TOK_LEFT_BRACE ";
        case TOK_RIGHT_BRACE: return "TOK_RIGHT_BRACE ";
        case TOK_LEFT_BRACKET: return "TOK_LEFT_BRACKET ";
        case TOK_RIGHT_BRACKET: return "TOK_RIGHT_BRACKET ";
        case TOK_COMMA: return "TOK_COMMA ";
        case TOK_DOT: return "TOK_DOT ";
        case TOK_MINUS: return "TOK_MINUS ";
        case TOK_PLUS: return "TOK_PLUS ";
        case TOK_SEMICOLON: return "TOK_SEMICOLON ";
        case TOK_COLON: return "TOK_COLON ";
        case TOK_SLASH: return "TOK_SLASH ";
        case TOK_STAR: return "TOK_STAR ";
        case TOK_BANG: return "TOK_BANG ";
        case TOK_BANG_EQUAL: return "TOK_BANG_EQUAL ";
        case TOK_EQUAL: return "TOK_EQUAL ";
        case TOK_EQUAL_EQUAL: return "TOK_EQUAL_EQUAL ";
        case TOK_GREATER: return "TOK_GREATER ";
        case TOK_GREATER_EQUAL: return "TOK_GREATER_EQUAL ";
        case TOK_LESS: return "TOK_LESS ";
        case TOK_LESS_EQUAL: return "TOK_LESS_EQUAL ";
        case TOK_IDENTIFIER: return "TOK_IDENTIFIER  ";
        case TOK_SYSCALL: return "TOK_SYSCALL  ";
        case TOK_STRING: return "TOK_STRING  ";
        case TOK_NUMBER: return "TOK_NUMBER ";
        case TOK_HEX: return "TOK_HEX ";
        case TOK_AND: return "TOK_AND ";
        case TOK_ELSE: return "TOK_ELSE ";
        case TOK_FALSE: return "TOK_FALSE ";
        case TOK_FOR: return "TOK_FOR ";
        case TOK_FUN: return "TOK_FUN ";
        case TOK_IF: return "TOK_IF ";
        case TOK_NIL: return "TOK_NIL  ";
        case TOK_OR: return "TOK_OR ";
        case TOK_PRINT: return "TOK_PRINT ";
        case TOK_RETURN: return "TOK_RETURN ";
        case TOK_TRUE: return "TOK_TRUE ";
        case TOK_VAR: return "TOK_VAR ";
        case TOK_WHILE: return "TOK_WHILE ";
        case TOK_STRUCT: return "TOK_STRUCT ";
        case TOK_ERROR: return "TOK_ERROR ";
        case TOK_INSTRUCTION: return "TOK_INSTRUCTION";

        case TOK_LOAD: return "TOK_LOAD";
        case TOK_ADD: return "TOK_ADD";
        case TOK_SUB: return "TOK_SUB";
        case TOK_MUL: return "TOK_MUL";
        case TOK_DIV: return "TOK_DIV";
        case TOK_JMP: return "TOK_JMP";
        case TOK_JMPF: return "TOK_JMPF";
        case TOK_JMPB: return "TOK_JMPB";
        case TOK_JEQ: return "TOK_JEQ";
        case TOK_JNE: return "TOK_JNE";
        case TOK_EQ: return "TOK_EQ";
        case TOK_NEQ: return "TOK_NEQ";
        case TOK_GT: return "TOK_GT";
        case TOK_LT: return "TOK_LT";
        case TOK_GTQ: return "TOK_GTQ";
        case TOK_LTQ: return "TOK_LTQ";
        case TOK_ALOC: return "TOK_ALOC";
        case TOK_HLT: return "TOK_HLT";
        case TOK_INC: return "TOK_INC";
        case TOK_DEC: return "TOK_DEC";
        case TOK_LABEL_USAGE: return "TOK_LABEL_USAGE";
        case TOK_DIRECTIVE: return "TOK_DIRECTIVE";
        case TOK_RESB: return "TOK_RESB";

        case TOK_COMMAND_REGISTERS: return "TOK_COMMAND_REGISTERS";
        case TOK_COMMAND_QUIT: return "TOK_COMMAND_QUIT";
        case TOK_COMMAND_HISTORY: return "TOK_COMMAND_HISTORY";
        case TOK_COMMAND_PROGRAM: return "TOK_COMMAND_PROGRAM";
        case TOK_PRT: return "TOK_PRT";
        case TOK_PUSH: return "TOK_PUSH";
        case TOK_POP: return "TOK_POP";
        case TOK_CALL: return "TOK_CALL";
        case TOK_RET: return "TOK_RET";

        case TOK_REGISTER: return "TOK_REGISTER";
        case TOK_INSTRUCTION_VALUE: return "TOK_INSTRUCTION_VALUE";
        case TOK_RAW: return "TOK_RAW";

        case TOK_EOF: {
            // __debugbreak();
            return "TOK_EOF ";
        }
        case TOK_NEWLINE: { return "TOK_NEWLINE "; }
                    defult: return "NONE????";
        default:{}break;
    }

    return "NONE????";

}



inline bool isAtEnd(Scanner* scanner) {
    return (*scanner->current == '\0');
}

inline char charPeek(Scanner* scanner) {
    return *scanner->current;
}


static bool TOK_match(Scanner* scanner, char expected) {
    if (isAtEnd(scanner))return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static Token makeToken(Scanner* scanner, TokenTypes type) {
    printf("making token: %24s on line %d\n", tok_to_str(type), scanner->line);
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;
    return token;
}

static Token errorToken(Scanner* scanner, const char* message) {
    Token token;
    token.type = TOK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;
    return token;
}



static TokenTypes checkKeyword(Scanner* scanner, s32 start, s32 length, const char* rest, TokenTypes type) {
    int test = scanner->current - scanner->start;
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0) {
        return type;
    }
    return TOK_IDENTIFIER;
}

static bool checkReplKeyword(Scanner* scanner, s32 start, s32 length, const char* rest) {
    if (scanner->current - scanner->start == start + length && memcmp(scanner->start + start, rest, length) == 0) {
        return true;
    }
    return false;
}

char scannerAdvance(Scanner* scanner) {
    scanner->current++;
    return (scanner->current[-1]);
}

inline char charPeekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}


static void skipWhitespace(Scanner* scanner) {
    for (;;) {
        char c = charPeek(scanner);
        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            scannerAdvance(scanner);
            break;
            // case '\n'://handle newline as an explicit token to know when the next instruction begins
            //     scanner->line++;
            //     scannerAdvance(scanner);
            //     break;
        case ';': {
            while (charPeek(scanner) != '\n' && !isAtEnd(scanner)) scannerAdvance(scanner);
        }break;
        case '/':
            if (charPeekNext(scanner) == '/') {
                //a comment goes until the end of the line
                while (charPeek(scanner) != '\n' && !isAtEnd(scanner)) scannerAdvance(scanner);
            }
            else {
                return;
            }
        default:
            return;
        }
    }
}



inline bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F');
}

uint8_t hexCharToValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;//should never happen if isHexDigit is checked first
}

int parseHex(Scanner* scanner, VM* vm) {
    u8 hexVals[4] = {};
    for (u32 i = 0; i < 4; i++) {
        skipWhitespace(scanner);
        if (isHexDigit(*scanner->current) && isHexDigit(*(scanner->current + 1))) {
            hexVals[i] = (hexCharToValue(*scanner->current) << 4) | hexCharToValue(*(scanner->current + 1));
            scanner->current += 2;
        }
        else {
            return -1;
        }
    }
    memcpy(vm->bytecode + vm->byteCount, hexVals, sizeof(u8) * 4);
    vm->byteCount += 4;

    return 0;
}

static TokenTypes InstructionType(Scanner* scanner) {
    switch (scanner->start[0]) {

    case 'A': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'L': return checkKeyword(scanner, 2, 2, "OC", TOK_ALOC);
            case 'D': return checkKeyword(scanner, 2, 1, "D", TOK_ADD);
            }
        }
    }break;
    case 'a': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'l': return checkKeyword(scanner, 2, 2, "oc", TOK_ALOC);
            case 'd': return checkKeyword(scanner, 2, 1, "d", TOK_ADD);
            }
        }
    }break;

    case 'C': return checkKeyword(scanner, 1, 3, "ALL", TOK_CALL);
    case 'c': return checkKeyword(scanner, 1, 3, "all", TOK_CALL);



    case 'D': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'I': return checkKeyword(scanner, 2, 1, "V", TOK_DIV);
            case 'E': return checkKeyword(scanner, 2, 1, "C", TOK_DEC);
            }
        }
    }break;
    case 'd': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'i': return checkKeyword(scanner, 2, 1, "v", TOK_DIV);
            case 'e': return checkKeyword(scanner, 2, 1, "c", TOK_DEC);
            }
        }
    }break;

    case 'E': return checkKeyword(scanner, 1, 1, "Q", TOK_EQ);
    case 'e': return checkKeyword(scanner, 1, 1, "q", TOK_EQ);


    case 'f': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {}
        }
    }break;


    case 'G': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'T': {
                if (scanner->current - scanner->start == 2) {
                    return TOK_GT;
                }
                if (scanner->current - scanner->start == 3) {
                    switch (scanner->start[2]) {
                    case 'Q': return TOK_GTQ;
                    }
                }
            }
            }
        }
    }break;


    case 'g': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 't': {
                if (scanner->current - scanner->start == 2) {
                    return TOK_GT;
                }
                if (scanner->current - scanner->start == 3) {
                    switch (scanner->start[2]) {
                    case 'q': return TOK_GTQ;
                    }
                }
            }
            }
        }
    }break;

    case 'H': return checkKeyword(scanner, 1, 2, "LT", TOK_HLT);
    case 'h': return checkKeyword(scanner, 1, 2, "lt", TOK_HLT);

    case 'I': return checkKeyword(scanner, 1, 2, "NC", TOK_INC);
    case 'i': return checkKeyword(scanner, 1, 2, "nc", TOK_INC);



    case 'J': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'M': {
                if (scanner->current - scanner->start > 2) {
                    switch (scanner->start[2]) {
                    case 'P': {
                        // Base JMP instruction
                        if (scanner->current - scanner->start == 3) {
                            return TOK_JMP;
                        }
                        // Check for extensions like JMPF or JMPB
                        if (scanner->current - scanner->start == 4) {
                            switch (scanner->start[3]) {
                            case 'F': return TOK_JMPF; // Jump forward
                            case 'B': return TOK_JMPB; // Jump backward
                            }
                        }
                    }
                    }
                }
            }
            case 'E': return checkKeyword(scanner, 2, 1, "Q", TOK_JEQ);
            case 'N': return checkKeyword(scanner, 2, 1, "E", TOK_JNE);
            }
        }
    } break;

    case 'j': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'm': {
                if (scanner->current - scanner->start > 2) {
                    switch (scanner->start[2]) {
                    case 'p': {
                        // Base JMP instruction
                        if (scanner->current - scanner->start == 3) {
                            return TOK_JMP;
                        }
                        // Check for extensions like JMPF or JMPB
                        if (scanner->current - scanner->start == 4) {
                            switch (scanner->start[3]) {
                            case 'f': return TOK_JMPF; // Jump forward
                            case 'b': return TOK_JMPB; // Jump backward
                            }
                        }
                    }
                    }
                }
            }
            case 'e': return checkKeyword(scanner, 2, 1, "q", TOK_JEQ);
            case 'n': return checkKeyword(scanner, 2, 1, "e", TOK_JNE);

            }
        }
    } break;


    case 'L': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'O': return checkKeyword(scanner, 2, 2, "AD", TOK_LOAD);
            case 'T': {
                if (scanner->current - scanner->start == 2) {
                    return TOK_LT;
                }
                if (scanner->current - scanner->start == 3) {
                    switch (scanner->start[2]) {
                    case 'Q': return TOK_LTQ;
                    }
                }
            }
            }
        }
    }break;
    case 'l': {
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'o': return checkKeyword(scanner, 2, 2, "ad", TOK_LOAD);
            case 't': {
                if (scanner->current - scanner->start == 2) {
                    return TOK_LT;
                }
                if (scanner->current - scanner->start == 3) {
                    switch (scanner->start[2]) {
                    case 'q': return TOK_LTQ;
                    }
                }
            }
            }
        }
    }break;



    case 'M': return checkKeyword(scanner, 1, 2, "UL", TOK_MUL);
    case 'm': return checkKeyword(scanner, 1, 2, "ul", TOK_MUL);

    case 'N': return checkKeyword(scanner, 1, 2, "EQ", TOK_NEQ);
    case 'n': return checkKeyword(scanner, 1, 2, "eq", TOK_NEQ);

    case 'P':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'O': return checkKeyword(scanner, 2, 1, "P", TOK_POP);
            case 'R': return checkKeyword(scanner, 2, 1, "T", TOK_PRT);
            case 'U': return checkKeyword(scanner, 2, 2, "SH", TOK_PUSH);
            }
        }break;
    case 'p':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'o': return checkKeyword(scanner, 2, 1, "p", TOK_POP);
            case 'r': return checkKeyword(scanner, 2, 1, "t", TOK_PRT);
            case 'u': return checkKeyword(scanner, 2, 2, "sh", TOK_PUSH);
            }
        }break;

    case 'R':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'A': return checkKeyword(scanner, 2, 1, "W", TOK_RAW);
            case 'E': {
                if (scanner->current - scanner->start > 2) {
                    switch (scanner->start[2]) {
                    case 'T': {
                        if (scanner->current - scanner->start == 3) {
                            return TOK_RET;
                        }
                    }break;

                    case 'S': return checkKeyword(scanner, 3, 1, "B", TOK_RESB);
                    }
                }
            }break;
            }
        }break;
    case 'r':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'a': return checkKeyword(scanner, 2, 1, "w", TOK_RAW);
            case 'e': {
                if (scanner->current - scanner->start > 2) {
                    switch (scanner->start[2]) {
                    case 't': {
                        if (scanner->current - scanner->start == 3) {
                            return TOK_RET;
                        }
                    }break;

                    case 's': return checkKeyword(scanner, 3, 1, "b", TOK_RESB);
                    }
                }
            }break;
            }
        }break;


    case 'S':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'U': return checkKeyword(scanner, 2, 1, "B", TOK_SUB);
            case 'Y': return checkKeyword(scanner, 2, 5, "SCALL", TOK_SYSCALL);
            }
        }
    case 's':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'u': return checkKeyword(scanner, 2, 1, "b", TOK_SUB);
            case 'y': return checkKeyword(scanner, 2, 5, "scall", TOK_SYSCALL);
            }
        }

    case 't':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            }
        }
    }

    return TOK_IDENTIFIER;
}



static TokenTypes identifierType(Scanner* scanner) {
    switch (scanner->start[0]) {
    case 'a': return checkKeyword(scanner, 1, 2, "nd", TOK_AND);
    case 'e': return checkKeyword(scanner, 1, 3, "lse", TOK_ELSE);
    case 'f':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'a': return checkKeyword(scanner, 2, 3, "lse", TOK_FALSE);
            case 'o': return checkKeyword(scanner, 2, 1, "r", TOK_FOR);
            case 'u': return checkKeyword(scanner, 2, 1, "n", TOK_FUN);
            }
        }break;
    case 'i': return checkKeyword(scanner, 1, 1, "f", TOK_IF);
    case 'l': return checkKeyword(scanner, 1, 3, "oad", TOK_LOAD);
    case 'L': return checkKeyword(scanner, 1, 3, "OAD", TOK_LOAD);
    case 'n': return checkKeyword(scanner, 1, 2, "il", TOK_NIL);
    case 'o': return checkKeyword(scanner, 1, 1, "r", TOK_OR);
    case 'p': return checkKeyword(scanner, 1, 4, "rint", TOK_PRINT);
    case 'R': return checkKeyword(scanner, 1, 3, "AW", TOK_RAW);
    case 'r':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'e': return checkKeyword(scanner, 1, 4, "turn", TOK_RETURN);
            case 'a': return checkKeyword(scanner, 1, 2, "aw", TOK_RAW);
            }
        }
    case 't':
        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[1]) {
            case 'r': return checkKeyword(scanner, 2, 2, "ue", TOK_TRUE);
            }
        }
    case 'v': return checkKeyword(scanner, 1, 2, "ar", TOK_VAR);
    case 'w': return checkKeyword(scanner, 1, 4, "hile", TOK_WHILE);
    }

    return TOK_IDENTIFIER;
}

static Token identifier(Scanner* scanner) {
    while (isAlpha(charPeek(scanner)) || isNumeric(charPeek(scanner))) scannerAdvance(scanner);
    return makeToken(scanner, identifierType(scanner));
}


static Token instruction(Scanner* scanner) {
    while (isAlpha(charPeek(scanner)) || isNumeric(charPeek(scanner))) scannerAdvance(scanner);
    return makeToken(scanner, InstructionType(scanner));
}

static Token number(Scanner* scanner) {
    if ((charPeek(scanner) == 'x' || charPeek(scanner) == 'X')) {
        printf("HEX FOUND!\n");
        scannerAdvance(scanner);
        if (!isHexDigit(charPeek(scanner))) {
            return errorToken(scanner, "Unexpected character in hex value.");
        }
        while (isHexDigit(charPeek(scanner))) scannerAdvance(scanner);
        return makeToken(scanner, TOK_HEX);
    }

    while (isNumeric(charPeek(scanner))) scannerAdvance(scanner);

    //look for a fractional part
    if (charPeek(scanner) == '.' && isNumeric(charPeekNext(scanner))) {
        //consume the "."
        scannerAdvance(scanner);
        while (isNumeric(charPeek(scanner))) scannerAdvance(scanner);
    }
    return makeToken(scanner, TOK_NUMBER);
}

static Token string(Scanner* scanner) {
    while (charPeek(scanner) != '"' && !isAtEnd(scanner)) {
        if (charPeek(scanner) == '\n'){
            scanner->line++;
        } 
        scannerAdvance(scanner);
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string");

    //the closing quote
    scannerAdvance(scanner);
    return makeToken(scanner, TOK_STRING);
}


Token scanToken(Scanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;
    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOK_EOF);
    }

    char c = scannerAdvance(scanner);
    //TODO:
    //could add a check here for slashes/dots/stuff to denote repl commands

    //would probably want a different scanToken function for assembly/higher language parser
    // if(isAlpha(c)) return identifier(scanner);

    if (isAlpha(c))return instruction(scanner);
    if (isNumeric(c)) return number(scanner);


    switch (c) {
    case '(': return makeToken(scanner, TOK_LEFT_PAREN);
    case ')': return makeToken(scanner, TOK_RIGHT_PAREN);
    case '{': return makeToken(scanner, TOK_LEFT_BRACE);
    case '}': return makeToken(scanner, TOK_RIGHT_BRACE);
    case '[': return makeToken(scanner, TOK_LEFT_BRACKET);
    case ']': return makeToken(scanner, TOK_RIGHT_BRACKET);
    case ':': return makeToken(scanner, TOK_COLON);
    case '@': return makeToken(scanner, TOK_LABEL_USAGE);
    case ',': return makeToken(scanner, TOK_COMMA);
    case '.': return makeToken(scanner, TOK_DOT);
    case '-': return makeToken(scanner, TOK_MINUS);
    case '+': return makeToken(scanner, TOK_PLUS);
    case '/': return makeToken(scanner, TOK_SLASH);
    case '*': return makeToken(scanner, TOK_STAR);
    case '\n':return makeToken(scanner, TOK_NEWLINE);

    case '!':
        return makeToken(scanner, TOK_match(scanner, '=') ? TOK_BANG_EQUAL : TOK_BANG);
    case '=':
        return makeToken(scanner, TOK_match(scanner, '=') ? TOK_EQUAL_EQUAL : TOK_EQUAL);
    case '<':
        return makeToken(scanner, TOK_match(scanner, '=') ? TOK_LESS_EQUAL : TOK_LESS);
    case '>':
        return makeToken(scanner, TOK_match(scanner, '=') ? TOK_GREATER_EQUAL : TOK_GREATER);
    case '"': return string(scanner);

    case '`': {
        printf("MAKE REPL COMMAND HERE!\n");
    }
    case '$': return makeToken(scanner, TOK_REGISTER);
    case '#': return makeToken(scanner, TOK_INSTRUCTION_VALUE);

    }

    return errorToken(scanner, "Unexpected character.");
}



static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOK_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOK_ERROR) {
        //nothing
    }
    else {
        fprintf(stderr, "at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}


static void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}


static void parser_advance(Parser* parser, Scanner* scanner) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOK_ERROR)break;

        errorAtCurrent(parser, parser->current.start);

    }
}

static bool parser_consume(Parser* parser, Scanner* scanner, TokenTypes type, const char* message) {
    if (parser->current.type == type) {
        parser_advance(parser, scanner);
        return true;
    }
    errorAtCurrent(parser, message);
    return false;
}

inline void parseAdvance(Parser* parser, Scanner* scanner) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = scanToken(scanner);
        if (parser->current.type != TOK_ERROR)break;
        errorAtCurrent(parser, parser->current.start);
    }

}

inline bool tokenCheck(Parser* parser, TokenTypes type) {
    return parser->current.type == type;
}

inline bool tokenMatch(Parser* parser, Scanner* scanner, TokenTypes type) {
    if (!tokenCheck(parser, type))return false;
    parseAdvance(parser, scanner);
    return true;
}

static void synchronize(Parser* parser, Scanner* scanner) {
    parser->panicMode = false;
    while (parser->current.type != TOK_EOF) {
        if (parser->previous.type == TOK_SEMICOLON)return;
        switch (parser->current.type) {
        case TOK_FUN:
        case TOK_VAR:
        case TOK_FOR:
        case TOK_IF:
        case TOK_WHILE:
        case TOK_PRINT:
        case TOK_RETURN:
            return;
        default:
            ;//do nothing
        }
        parseAdvance(parser, scanner);

    }

}

#define EMIT_INSTRUCTION()\
    u32 offset = vm->byteCount;\
    vm->bytecode[offset+0] = code;\
    vm->bytecode[offset+1] = bytes[0];\
    vm->bytecode[offset+2] = bytes[1];\
    vm->bytecode[offset+3] = bytes[2];\
    vm->byteCount += 4;

#define CheckReg(reg) if(((reg) < 0) || ((reg) >= MAX_REGISTERS)) {error(parser, "REGISTER NUMBER TOO HIGH!\n");return;}


#define PARSE_REGISTER(bytes)\
    parseAdvance(parser, scanner);\
    parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");\
    u32 reg = string_to_int(parser->previous.start, &end);\
    CheckReg(reg);\
    bytes = reg;\


#define CheckByte(byte) if((byte) > 0xff) {error(parser, "Offset too large! needs to be within 1 byte!"); return;}

#define PARSE_BYTE(byte)\
parser_consume(parser, scanner, TOK_NUMBER, "Expected Number!");\
int instructionValue = string_to_int(parser->previous.start, &end);\
CheckByte(instructionValue);\
byte = instructionValue & 0xff;


inline void parseRaw(VM* vm, Parser* parser, Scanner* scanner) {
    TokenTypes instructionType = parser->previous.type;
    parseAdvance(parser, scanner);

    if (!tokenCheck(parser, TOK_HEX)) {
        error(parser, "RAW instruction needs to be followed by HEXADECIMAL VALUES!\n");
        return;
    }

    int nybbles = 0; //need 8 nybbles
    u8 hex[4] = {};
    if (parser->current.length == 10) {//0x + 8 chars 
        hex[0] = (hexCharToValue(*(parser->current.start + 2)) << 4);
        hex[0] |= (hexCharToValue(*(parser->current.start + 3)));

        hex[1] = (hexCharToValue(*(parser->current.start + 4)) << 4);
        hex[1] |= (hexCharToValue(*(parser->current.start + 5)));

        hex[2] = (hexCharToValue(*(parser->current.start + 6)) << 4);
        hex[2] |= (hexCharToValue(*(parser->current.start + 7)));

        hex[3] = (hexCharToValue(*(parser->current.start + 8)) << 4);
        hex[3] |= (hexCharToValue(*(parser->current.start + 9)));
        nybbles = 8;
    }
    else {
        int arrayEntry = 0; //need 8 nybbles
        int i = 0;
        for (u32 bytes = 0; bytes < 4; bytes++) {
            if (nybbles >= 8)break;
            int c = 0;
            if (bytes > 0) {
                parser_consume(parser, scanner, TOK_HEX, "Expected HEX as operand");
                if (!tokenCheck(parser, TOK_HEX)) {
                    error(parser, "RAW instruction needs to be followed by HEXADECIMAL VALUES!\n");
                    return;
                }
                if ((parser->current.length - 2 & 1)) {
                    error(parser, "HEX VALUE MUST HAVE EVEN NUMBER OF CHARACTERS!\n");
                    return;
                }
                if ((parser->current.length - 2 > (8 - nybbles))) {
                    error(parser, "CUMULATIVE HEX INSTRUCTION TOO LARGE!\n");
                    return;
                }

            }

            while (c < parser->current.length - 2 && nybbles < 8) {
                if (c & 1) {
                    hex[arrayEntry] = (hex[arrayEntry] << 4) | (hexCharToValue(*(parser->current.start + 2 + c)));

                }
                else {
                    hex[arrayEntry] = (hexCharToValue(*(parser->current.start + 2 + c)));
                }
                nybbles++;
                c++;
                arrayEntry += i++;
                i = i & 1;
            }

        }

    }

    if (nybbles != 8) {
        error(parser, "RAW instruction MUST be followed by 4 bytes of HEX!");
        return;
    }
    if (hex[0] == 1 && hex[1] >= MAX_REGISTERS) {
        error(parser, "[raw] LOAD instruction register must be within 0 - 31!");
        return;
    }

    // // printf("%x %x\n", hex1, hex2);
    u32 offset = vm->byteCount;
    vm->bytecode[offset + 0] = hex[0];
    vm->bytecode[offset + 1] = hex[1];
    vm->bytecode[offset + 2] = hex[2];
    vm->bytecode[offset + 3] = hex[3];
    vm->byteCount += 4;


}

int parseRegister(VM* vm, Parser* parser, Scanner* scanner, int* regs) {

    const char* end = NULL;
    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as first operand"))return -1;
    int reg1 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return -1;

    if (reg1 < 0 || reg1 >= MAX_REGISTERS) {
        error(parser, "REGISTER 1 TOO HIGH!\n");
        return -1;
    }

    regs[0] = reg1;

    return 0;
}

int parse2Registers(VM* vm, Parser* parser, Scanner* scanner, int* regs) {

    const char* end = NULL;
    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as first operand"))return -1;
    int reg1 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return -1;

    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as second operand"))return -1;
    int reg2 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after second register symbol"))return -1;

    if (reg1 < 0 || reg1 >= MAX_REGISTERS) {
        error(parser, "REGISTER 1 TOO HIGH!\n");
        return -1;
    }
    if (reg2 < 0 || reg2 >= MAX_REGISTERS) {
        error(parser, "REGISTER 2 TOO HIGH!\n");
        return -1;
    }

    regs[0] = reg1;
    regs[1] = reg2;

    return 0;
}

int parse3Registers(VM* vm, Parser* parser, Scanner* scanner, int* regs) {

    const char* end = NULL;
    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as first operand"))return -1;
    int reg1 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return -1;

    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as second operand"))return -1;
    int reg2 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after second register symbol"))return -1;

    if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as third operand"))return -1;
    int reg3 = string_to_int(parser->current.start, &end);
    if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after third register symbol"))return -1;

    if (reg1 < 0 || reg1 >= MAX_REGISTERS) {
        error(parser, "REGISTER 1 TOO HIGH!\n");
        return -1;
    }
    if (reg2 < 0 || reg2 >= MAX_REGISTERS) {
        error(parser, "REGISTER 2 TOO HIGH!\n");
        return -1;
    }
    if (reg3 < 0 || reg3 >= MAX_REGISTERS) {
        error(parser, "REGISTER 3 TOO HIGH!\n");
        return -1;
    }


    regs[0] = reg1;
    regs[1] = reg2;
    regs[2] = reg3;

    return 0;
}

int parseRegisterAndValue(VM* vm, Parser* parser, Scanner* scanner, int* vals) {
    parser_consume(parser, scanner, TOK_REGISTER, "Expected register as first operand");
    // int registerNumber = (int)strtol(parser->current.start, NULL, 10);
    const char* end = NULL;
    parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");

    u32 registerNumber = string_to_int(parser->previous.start, &end);

    if (registerNumber < 0 || registerNumber >= MAX_REGISTERS) {
        error(parser, "REGISTER NUMBER TOO HIGH!\n");
        return -1;
    }

    vals[0] = registerNumber;

    parser_consume(parser, scanner, TOK_INSTRUCTION_VALUE, "Expected number as second operand");
    //this will mask off negative numbers, need to handle it better later
    parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");

    int instructionValue = string_to_int(parser->previous.start, &end) & 0x0000FFFF;

    // int instructionValue =((int)strtol(parser->current.start, NULL, 10)) & 0x0000FFFF;//can only use 16 bytes for the value

    vals[1] = (instructionValue) >> 8;

    vals[2] = instructionValue & 0xff;

    return 0;
}


struct operand {
    addressing_mode mode;
    union {
        uint8_t reg; //for registers
        uint16_t immediate;
        struct label { //for labels/backpatching
            symbol_table_entry* entry;
            uint16_t value;
        }label;

        struct indirect {
            uint8_t reg;
            uint8_t offset;
        }indirect;
    };
};


struct vm_instruction {
    uint8_t operation;
    operand src;
    operand dst;
    operand res;
    uint8_t final_opcode; //gets determined based on addressing modes
};

//opcode lookup table, maps (operation, srcMode, dstMode) -> opcode
struct opcode_mapping {
    uint8_t operation;
    addressing_mode srcMode;
    addressing_mode dstMode;
    uint8_t opcode;
};

//will this make hot reloading painful if its in the game layer?
// static opcode_mapping opcode_table[] = {
//     {OP_LOAD, ADDR_REG,             ADDR_REG,           OP_LOAD_REG_TO_REG                  },
//     {OP_LOAD, ADDR_IMM,             ADDR_REG,           OP_LOAD_IMM_TO_REG                  },
//     {OP_LOAD, ADDR_LABEL,           ADDR_REG,           OP_LOAD_IMM_TO_REG/*OP_LOAD_LABEL_TO_REG*/                },
//     {OP_LOAD, ADDR_REG_INDIRECT,    ADDR_REG,           OP_LOAD_REG_ADDR_TO_REG             },
//     {OP_LOAD, ADDR_REG_OFFSET,      ADDR_REG,           OP_LOAD_OFFSET_REG_ADDR_TO_REG      },
//     {OP_LOAD, ADDR_REG,             ADDR_REG_INDIRECT,  OP_LOAD_REG_TO_REG_ADDR             },
//     {OP_LOAD, ADDR_REG,             ADDR_REG_OFFSET,    OP_LOAD_REG_TO_OFFSET_REG_ADDR      },
//     {OP_LOAD, ADDR_REG_INDIRECT,    ADDR_REG_INDIRECT,  OP_LOAD_DATA_ADDR_TO_ADDR     },
//     {OP_LOAD, ADDR_REG_OFFSET,      ADDR_REG_INDIRECT,  OP_LOAD_OFFSET_REG_ADDR_TO_REG_ADDR },
//     {OP_LOAD, ADDR_REG_INDIRECT,    ADDR_REG_OFFSET,    OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR },

// };

void emit_instruction_bytes(VM* vm, vm_instruction* inst) {

    int byteCount = vm->byteCount;
    vm->byteCount += 4;

    vm->bytecode[byteCount++] = inst->final_opcode;
    switch (inst->dst.mode) {
    case ADDR_REG: {
        vm->bytecode[byteCount++] = inst->dst.reg;
    }break;
    case ADDR_IMM: {
        vm->bytecode[byteCount++] = (inst->dst.immediate >> 8) & 0xFF;
        vm->bytecode[byteCount++] = inst->dst.immediate & 0XFF;
    }break;
    case ADDR_REG_INDIRECT: {
        vm->bytecode[byteCount++] = inst->dst.indirect.reg;
    }break;
    case ADDR_REG_OFFSET: {
        vm->bytecode[byteCount++] = inst->dst.indirect.reg;
        vm->bytecode[byteCount++] = inst->dst.indirect.offset;
    }break;
    default: {}break;
    }
    switch (inst->src.mode) {
    case ADDR_REG: {
        vm->bytecode[byteCount++] = inst->src.reg;
    }break;
    case ADDR_IMM: {
        vm->bytecode[byteCount++] = (inst->src.immediate >> 8) & 0xFF;
        vm->bytecode[byteCount++] = inst->src.immediate & 0XFF;

    }break;
    case ADDR_LABEL: {
        vm->bytecode[byteCount++] = (inst->src.label.value >> 8) & 0xFF;
        vm->bytecode[byteCount++] = inst->src.label.value & 0XFF;
    }break;
    case ADDR_REG_INDIRECT: {
        vm->bytecode[byteCount++] = inst->src.indirect.reg;
    }break;
    case ADDR_REG_OFFSET: {
        vm->bytecode[byteCount++] = inst->src.indirect.reg;
        vm->bytecode[byteCount++] = inst->src.indirect.offset;
    }break;
    default: {}break;
    }
    switch (inst->res.mode) {
    case ADDR_REG: {
        vm->bytecode[byteCount++] = inst->res.reg;
    }break;
    case ADDR_REG_INDIRECT: {
        vm->bytecode[byteCount++] = inst->res.indirect.reg;
    }break;
    case ADDR_IMM: {
        vm->bytecode[byteCount++] = inst->res.immediate;
    }break;
    default: {}break;
    }

}

operand parse_operand(Parser* parser, Scanner* scanner, VM* vm) {
    operand operand = {};
    const char* end = NULL;

    s32 bytes[3] = {};

    switch (parser->current.type) {
    case TOK_REGISTER: {
        operand.mode = ADDR_REG;
        parseAdvance(parser, scanner);
        parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");
        u32 reg = string_to_int(parser->previous.start, &end);
        if (((reg) < 0) || ((reg) >= MAX_REGISTERS)) { error(parser, "REGISTER NUMBER TOO HIGH!\n");return operand; }
        operand.reg = reg;
    }break;



    case TOK_LEFT_BRACKET: {
        parseAdvance(parser, scanner);

        switch (parser->current.type) {

        case TOK_REGISTER: {

            parseAdvance(parser, scanner);
            parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");
            u32 reg = string_to_int(parser->previous.start, &end);
            if (((reg) < 0) || ((reg) >= MAX_REGISTERS)) { error(parser, "REGISTER NUMBER TOO HIGH!\n");return operand; }
            operand.reg = reg;


            if (parser->current.type == TOK_PLUS) {
                operand.mode = ADDR_REG_OFFSET;
                parseAdvance(parser, scanner);
                switch (parser->current.type) {
                case TOK_INSTRUCTION_VALUE:
                    parseAdvance(parser, scanner);//fall through
                case TOK_NUMBER: {
                    parser_consume(parser, scanner, TOK_NUMBER, "Expected Number!");
                    s32 instructionValue = string_to_int(parser->previous.start, &end);
                    if ((instructionValue) > 0xff) { error(parser, "Offset too large! needs to be within 1 byte!"); return operand; }
                    operand.indirect.offset = instructionValue & 0xff;
                }break;
                default:
                    error(parser, "EXPECTED NUMBER AFTER + FOR ADDRESS OFFSET!");
                    return operand;
                }
            }
            else {
                operand.mode = ADDR_REG_INDIRECT;
            }


            if (!parser_consume(parser, scanner, TOK_RIGHT_BRACKET, "Expected right bracket register symbol")) { return operand; }

        }break;

        default: {
            Assert(!"Unhandled LOAD case!");
        }break;

        }
        int fuckTheDebugger = 0;
    }break;//end brackets/indirect addressing

    case TOK_IDENTIFIER: {

        operand.mode = ADDR_LABEL;
        char* labelStart = scanner->start;
        char* labelEnd = scanner->current;
        u32 len = labelEnd - labelStart;

        parser_consume(parser, scanner, TOK_IDENTIFIER, "Expected label as second operand");
        //dont worry about backpatching yet
        //TODO: HANDLE BACKPATCHING HERE!!

        u32 val = 0;
        symbol_table_entry* entry = pushAssemblerSymbolTable(&vm->table, labelStart, len);
        if (entry) {

            if (entry->defined) {
                operand.label.value = entry->byteOffset;
            }
            else {
                printf("label is not yet defined\n");
                AssemblerBackPatch patch = {};
                patch.hashEntry = entry;
                patch.byteCodeLocation = vm->byteCount + 1;//JMP location + vm->byteCount
                vm->backPatchTable[vm->backPatchTableSize++] = patch;
                operand.label.value = 0;
            }
        }
        else {
            Assert(!"Invalid symbol table entry??");
        }

    }break;

    case TOK_INSTRUCTION_VALUE: {
        parser_consume(parser, scanner, TOK_INSTRUCTION_VALUE, "Expected number following immediate symbol");
        parser_consume(parser, scanner, TOK_NUMBER, "Expected number after register symbol");

    }//drop into number parsing
    case TOK_NUMBER: {
        operand.mode = ADDR_IMM;

        s32 instructionValue = string_to_int(parser->previous.start, &end) & 0x0000FFFF;
        operand.immediate = instructionValue;
    }break;


    default: {
        error(parser, "unhandled argument!");
        return operand;
    }break;
    }

    return operand;
}


u8 lookup_opcode(VM* vm, u8 operation, addressing_mode arg1, addressing_mode arg2, addressing_mode arg3) {
#if 0
    //arg1 is src, arg2 is dst, so LOAD would look like LOAD arg2(dst) arg1(src), needed to change it for the 4d table 
    for (int i = 0; opcode_table[i].operation != 0; i++) {
        if (opcode_table[i].operation == operation &&
            opcode_table[i].srcMode == arg1 &&
            opcode_table[i].dstMode == arg2) {
            return opcode_table[i].opcode;
        }
    }
    return 255;
#else //this is in a function in case we later want to use a hash map instead of a stupid large 4D table
    return vm->opLookupTable[operation][arg1][arg2][arg3];
#endif
}




// };

inline void parseLOAD(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    Token instructionToken = parser->current;
    parseAdvance(parser, scanner);

    vm_instruction inst = { 0 };
    inst.operation = GEN_LOAD;
    // Parse destination operand
    inst.dst = parse_operand(parser, scanner, vm);
    if (parser->hadError) return;

    // Parse source operand  
    inst.src = parse_operand(parser, scanner, vm);
    if (parser->hadError) return;

    inst.final_opcode = lookup_opcode(vm, inst.operation, inst.dst.mode, inst.src.mode, ADDR_NONE);

    //if we want to reorder operands to reduce symmetric instructions from taking up the instruction count
#if 0
    if (inst.final_opcode == OP_LOAD_REG_ADDR_TO_OFFSET_REG_ADDR) {
        //swap dst and src
        operand temp = inst.dst;
        inst.dst = inst.src;
        inst.src = temp;
    }
#endif

    if (inst.final_opcode == 255) {
        errorAt(parser, &instructionToken, "PARSE LOAD invalid addressing mode combination");
        return;
    }
    emit_instruction_bytes(vm, &inst);
    return;
}


inline void parseMATH(VM* vm, Parser* parser, Scanner* scanner, generic_opcode code) {
    Token instructionToken = parser->current;
    parseAdvance(parser, scanner);

    vm_instruction inst = { 0 };
    inst.operation = code;
    // Parse destination operand
    inst.dst = parse_operand(parser, scanner, vm);
    if (parser->hadError) return;

    // Parse source operand  
    inst.src = parse_operand(parser, scanner, vm);
    if (parser->hadError) return;


    switch (parser->current.type) {
    case TOK_REGISTER:
    case TOK_LEFT_BRACKET://fall through, so that we can omit the result operand in certain cases
        inst.res = parse_operand(parser, scanner, vm);
        if (parser->hadError) return;
        break;
    default: {
        inst.res = inst.dst;
    }
    }

    //LOAD doesnt have a third operand, just use the  ADDR_REG as a null value 
    inst.final_opcode = lookup_opcode(vm, inst.operation, inst.dst.mode, inst.src.mode, inst.res.mode);
    if (inst.final_opcode == 255) {
        errorAt(parser, &instructionToken, "PARSE MATH invalid addressing mode combination");
        return;
    }
    emit_instruction_bytes(vm, &inst);
    return;
}

//can either accept a single operand, or 3
inline void parseGEN(VM* vm, Parser* parser, Scanner* scanner, generic_opcode code) {
    Token instructionToken = parser->current;
    parseAdvance(parser, scanner);

    vm_instruction inst = { 0 };
    inst.operation = code;
    // Parse destination operand
    inst.dst = parse_operand(parser, scanner, vm);
    if (parser->hadError) return;

    switch (parser->current.type) {
    case TOK_REGISTER:
    case TOK_INSTRUCTION_VALUE:
    case TOK_NUMBER:
    case TOK_LEFT_BRACKET://fall through, so that we can omit the result operand in certain cases
        inst.src = parse_operand(parser, scanner, vm);
        if (parser->hadError) return;
        break;
    default: {
        inst.src.mode = ADDR_NONE;
    }
    }
    switch (parser->current.type) {
    case TOK_REGISTER:
    case TOK_INSTRUCTION_VALUE://fall through, so that we can omit the result operand in certain cases
    case TOK_NUMBER:
    case TOK_LEFT_BRACKET:
        inst.res = parse_operand(parser, scanner, vm);
        if (parser->hadError) return;
        break;
    default: {
        inst.res.mode = ADDR_NONE;
    }
    }

    inst.final_opcode = lookup_opcode(vm, inst.operation, inst.dst.mode, inst.src.mode, inst.res.mode);
    if (inst.final_opcode == 255) {
        errorAt(parser, &instructionToken, "PARSE GEN invalid addressing mode combination");
        return;
    }
    emit_instruction_bytes(vm, &inst);
    return;
}

inline void parseRegAndVal(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);

    int bytes[3] = {};
    int result = parseRegisterAndValue(vm, parser, scanner, bytes);

    if (result < 0)return;

    EMIT_INSTRUCTION();
}

inline void parse3Regs(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);

    int bytes[3] = {};
    int result = parse3Registers(vm, parser, scanner, bytes);

    if (result < 0)return;
    EMIT_INSTRUCTION();
}

inline void parse2Regs(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);

    int regs[2] = {};
    int result = parse2Registers(vm, parser, scanner, regs);

    if (result < 0)return;

    // printf("%x %x\n", hex1, hex2);
    u32 offset = vm->byteCount;
    vm->bytecode[offset + 0] = code;
    vm->bytecode[offset + 1] = regs[0];
    vm->bytecode[offset + 2] = regs[1];
    vm->byteCount += 4;
}


inline void parsePUSH(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);
    int bytes[3] = {};
    const char* end = NULL;

    switch (parser->current.type) {

    case TOK_REGISTER: {
        parseAdvance(parser, scanner);
        int reg1 = string_to_int(parser->current.start, &end);
        if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return;
        bytes[0] = reg1;

    }break;

    default:
        error(parser, "unhandled OP_PUSH CASE!");
        Assert(!"unhandled OP_PUSH CASE!");
    }

    EMIT_INSTRUCTION();
}

inline void parseSYSCALL(VM* vm, Parser* parser, Scanner* scanner) {
    parseAdvance(parser, scanner);
    int bytes[3] = {};
    Opcode code = OP_SYSCALL;


    EMIT_INSTRUCTION();
}

inline void parsePOP(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);
    int bytes[3] = {};
    const char* end = NULL;

    switch (parser->current.type) {

    case TOK_REGISTER: {
        parseAdvance(parser, scanner);
        int reg1 = string_to_int(parser->current.start, &end);
        if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return;
        bytes[0] = reg1;

    }break;

    default:
        error(parser, "unhandled OP_PUSH CASE!");
        Assert(!"unhandled OP_PUSH CASE!");
    }

    EMIT_INSTRUCTION();
}

inline void parseCALL(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);
    int bytes[3] = {};
    const char* end = NULL;

    switch (parser->current.type) {

    case TOK_IDENTIFIER: {
        char* labelStart = scanner->start;
        char* labelEnd = scanner->current;
        u32 len = labelEnd - labelStart;

        parseAdvance(parser, scanner);

        u32 val = 0;
        symbol_table_entry* entry = pushAssemblerSymbolTable(&vm->table, labelStart, len);
        if (entry) {
            if (entry->defined) {
                val = entry->byteOffset;
            }
            else {
                printf("label is not yet defined\n");
                AssemblerBackPatch patch = {};
                patch.hashEntry = entry;
                patch.byteCodeLocation = vm->byteCount + 1;//JMP location + vm->byteCount
                vm->backPatchTable[vm->backPatchTableSize++] = patch;
            }
        }
        else {
            Assert(!"Invalid symbol table entry??");
        }

        bytes[0] = (val) >> 16;
        bytes[1] = (val) >> 8;
        bytes[2] = val & 0xff;

    }break;

    default:
        error(parser, "unhandled OP_CALL CASE! Expected label name!");
        Assert(!"unhandled OP_CALL CASE!");
    }

    EMIT_INSTRUCTION();
}

inline void parseRET(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);
    int bytes[3] = {};
    const char* end = NULL;
    EMIT_INSTRUCTION();
}


inline void parsePRT(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {

    parseAdvance(parser, scanner);

    int bytes[3] = {};
    const char* end = NULL;

    switch (parser->current.type) {
        case TOK_REGISTER: {
            parseAdvance(parser, scanner);
            if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return;
            int reg1 = string_to_int(parser->previous.start, &end);
            bytes[0] = reg1;
            code = OP_PRT_REG;
        }break;

        case TOK_LEFT_BRACKET: {
            parseAdvance(parser, scanner);

            const char* end = NULL;
            if (!parser_consume(parser, scanner, TOK_REGISTER, "Expected register as first operand"))return;
            if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected number after first register symbol"))return;
            int reg1 = string_to_int(parser->previous.start, &end);
            if (reg1 < 0 || reg1 >= MAX_REGISTERS) {
                error(parser, "REGISTER 1 TOO HIGH!\n");
                return;
            }
            bytes[0] = reg1;
            code = OP_PRT_ADDRESS;
            if (!parser_consume(parser, scanner, TOK_RIGHT_BRACKET, "Expected closing bracket"))return;
        }break;

        case TOK_INSTRUCTION_VALUE: {
            Assert(!"unhandled case!");
        }break;

        default:{}break;
    }

    EMIT_INSTRUCTION();
}


inline void parseReg(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    parseAdvance(parser, scanner);

    int regs[1] = {};
    int result = parseRegister(vm, parser, scanner, regs);

    if (result < 0)return;

    // printf("%x %x\n", hex1, hex2);
    u32 offset = vm->byteCount;
    vm->bytecode[offset + 0] = code;
    vm->bytecode[offset + 1] = regs[0];
    vm->byteCount += 4;
}



inline void parseLabelDec(VM* vm, Parser* parser, Scanner* scanner) {
    if (parser->previous.type != TOK_IDENTIFIER) {
        error(parser, "expected identifier before colon for label declaration!");
        return;
    }
    Assert(!"add label to symbol table!\n");
    parseAdvance(parser, scanner);
}

inline void parseLabelCode(VM* vm, Parser* parser, Scanner* scanner) {
    char* labelStart = scanner->start;
    char* labelEnd = scanner->current;
    parseAdvance(parser, scanner);
    if (!parser_consume(parser, scanner, TOK_COLON, "Expected colon after label definition at start of line"))return;


    u32 len = labelEnd - labelStart;

    symbol_table_entry entry = {};
    // entry.name          = scanner->start;
    entry.name = labelStart;
    entry.nameLen = len;
    entry.type = label_types::label_code;
    entry.byteOffset = vm->byteCount;
    entry.defined = true;


    if (len > 32) {
        errorAtCurrent(parser, "Label is too long! must be under 32 characters long!");
        return;
    }

    symbol_table_entry* tableEntry = pushAssemblerSymbolTable(&vm->table, labelStart, len);
    if (tableEntry) {
        *tableEntry = entry;
    }
    // Assert(!"figure out label use here!\n");
    //so this is a function that we jump to

}




inline void parseLabelData(VM* vm, Parser* parser, Scanner* scanner) {

    parseAdvance(parser, scanner);
    char* labelStart = scanner->start;
    char* labelEnd = scanner->current;

    if (!parser_consume(parser, scanner, TOK_IDENTIFIER, "Expected identifier name after '.' definition at start of line"))return;



    u32 len = labelEnd - labelStart;


    if (len > 32) {
        errorAtCurrent(parser, "Label is too long! must be under 32 characters long!");
        return;
    }


    u32 dataLocation = 0;

    switch (parser->current.type) {
    case TOK_STRING: {//create stretch of static memory for a string

        if (parser->current.length - 2 > 256) {
            errorAtCurrent(parser, "String is too long! must be under 256 characters long!");
            return;
        }

        dataLocation = vm->memSize;
        memcpy(vm->mem + vm->memSize, parser->current.start + 1, parser->current.length - 2);
        *(vm->mem + vm->memSize + parser->current.length - 1) = 0;
        vm->memSize += parser->current.length - 1;
        parseAdvance(parser, scanner);
        int fuckTheDebugger = 0;

    }break;

    case TOK_RESB: {//allocates data in const data segment (should we instead use a different variable data block and do a runtime check?)
        const char* end = NULL;
        parseAdvance(parser, scanner);
        if (!parser_consume(parser, scanner, TOK_NUMBER, "Expected numeric value after RESB"))return;
        int val = string_to_int(parser->previous.start, &end);
        dataLocation = vm->memSize;
        memset(vm->mem + vm->memSize, 0, val);
        vm->memSize += val;
        int fuckTheDebugger = 0;

    }break;

    default: {
        Assert(!"unhandled label case!");
    }break;
    }

    symbol_table_entry entry = {};
    entry.name = labelStart;
    entry.nameLen = len;
    entry.type = label_types::label_data;
    entry.byteOffset = dataLocation;
    entry.defined = true;



    symbol_table_entry* tableEntry = pushAssemblerSymbolTable(&vm->table, labelStart, len);
    if (tableEntry) {
        *tableEntry = entry;
    }
    else {
        Assert(!"Is this something we should be worried about?");
        error(parser, "Couldn't assign label to the table?");
        return;
    }




    // Assert(!"figure out label use here!\n");
    //so this is a function that we jump to

}


inline void parseSingleInstruction(VM* vm, Parser* parser, Scanner* scanner, Opcode code) {
    TokenTypes instructionType = parser->previous.type;
    parseAdvance(parser, scanner);

    // printf("%x %x\n", hex1, hex2);
    u32 offset = vm->byteCount;
    vm->bytecode[offset + 0] = code;
    vm->byteCount += 4;
}


int repl_command(REPL* repl) {
    Scanner* scanner = &repl->scanner;
    VM& vm = repl->vm;
    char c = scannerAdvance(scanner);

    TokenTypes type = {};

    //assuming we've hit an identifier
    while (isAlpha(charPeek(scanner)) || isNumeric(charPeek(scanner))) scannerAdvance(scanner);

    switch (c) {
    case '/': {//repl command

        scanner->start++;

        if (scanner->current - scanner->start > 1) {
            switch (scanner->start[0]) {
            case 'c': {
                if (checkReplKeyword(scanner, 1, 4, "lear")) {
                    printf("clearing registers and bytecode!\n");
                    repl->vm.byteCount = 0;
                    repl->vm.pc = 0;
                    for (u32 i = 0; i < 32; i++) {//exclude the last command which is just .history
                        vm.registers[i] = 0;
                    }
                    return 0;
                }
            }break;
            case 'q': {
                if (checkReplKeyword(scanner, 1, 3, "uit")) {
                    printf("THANKS BYE\n");
                    return 1;
                }
            }break;
            case 'h': {
                if (checkReplKeyword(scanner, 1, 6, "istory")) {
                    for (int i = 0; i < repl->historyLines - 1; i++) {//exclude the last command which is just .history
                        printf("%u: %s", i, repl->history + (i * MAX_REPL_BUFFER));
                    }
                }
            }break;
            case 'p': {
                if (checkReplKeyword(scanner, 1, 6, "rogram")) {
                    for (u32 i = 0; i < vm.byteCount; i += 4) {//exclude the last command which is just .history
                        printf("%2x %2x %2x %2x\n", vm.bytecode[i + 0], vm.bytecode[i + 1], vm.bytecode[i + 2], vm.bytecode[i + 3]);
                    }
                }
            }break;
            case 'r': {
                if (checkReplKeyword(scanner, 1, 8, "egisters")) {
                    printf("REGISTERS:\n");
                    for (u32 i = 0; i < 16; i++) {//exclude the last command which is just .history
                        printf(" %5ld   ", i);
                    }
                    printf("\n");
                    for (u32 i = 0; i < 16; i++) {//exclude the last command which is just .history
                        printf("[%6ld] ", vm.registers[i]);
                    }
                    printf("\n");

                    for (u32 i = 16; i < 32; i++) {//exclude the last command which is just .history
                        if (i == 31) {
                            printf(" %5ld (STACK PTR)  ", i);
                        }
                        else {
                            printf(" %5ld   ", i);
                        }
                    }
                    printf("\n");
                    for (u32 i = 16; i < 32; i++) {//exclude the last command which is just .history
                        printf("[%6ld] ", vm.registers[i]);
                    }
                    printf("\n");

                }
            }break;
            }
        }

    }break;
    }
    return 0;
}


void parseInstruction(VM* vm, Parser* parser, Scanner* scanner) {
    switch (parser->current.type) {
    case TOK_LOAD: { parseLOAD(vm, parser, scanner, Opcode::OP_LOAD); }break;
    case TOK_ADD: { parseMATH(vm, parser, scanner, generic_opcode::GEN_ADD); }break;
    case TOK_SUB: { parseMATH(vm, parser, scanner, generic_opcode::GEN_SUB); }break;
    case TOK_MUL: { parseMATH(vm, parser, scanner, generic_opcode::GEN_MUL); }break;
    case TOK_DIV: { parseMATH(vm, parser, scanner, generic_opcode::GEN_DIV); }break;

    case TOK_EQ: { parseGEN(vm, parser, scanner, generic_opcode::GEN_EQ); }break;
    case TOK_NEQ: { parse2Regs(vm, parser, scanner, Opcode::OP_NEQ); }break;
    case TOK_GT: { parse2Regs(vm, parser, scanner, Opcode::OP_GT); }break;
    case TOK_LT: { parse2Regs(vm, parser, scanner, Opcode::OP_LT); }break;
    case TOK_GTQ: { parse2Regs(vm, parser, scanner, Opcode::OP_GTQ); }break;
    case TOK_LTQ: { parse2Regs(vm, parser, scanner, Opcode::OP_LTQ); }break;

    case TOK_JEQ: { parseGEN(vm, parser, scanner, generic_opcode::GEN_JEQ); }break;
    case TOK_JNE: { parseGEN(vm, parser, scanner, generic_opcode::GEN_JNE); }break;
    case TOK_JMP: { parseGEN(vm, parser, scanner, generic_opcode::GEN_JMP); }break;
    case TOK_JMPF: { parseReg(vm, parser, scanner, Opcode::OP_JMPF); }break;
    case TOK_JMPB: { parseReg(vm, parser, scanner, Opcode::OP_JMPB); }break;

    case TOK_INC: { parseReg(vm, parser, scanner, Opcode::OP_INC); }break;
    case TOK_DEC: { parseReg(vm, parser, scanner, Opcode::OP_DEC); }break;

    case TOK_PRT: { parsePRT(vm, parser, scanner, Opcode::OP_PRT); }break;

    case TOK_PUSH: { parsePUSH(vm, parser, scanner, Opcode::OP_PUSH_REG); }break;
    case TOK_POP: { parsePOP(vm, parser, scanner, Opcode::OP_POP_REG); }break;
    case TOK_SYSCALL: { parseSYSCALL(vm, parser, scanner); }break;

    case TOK_COLON: {
        // parseLabelDec(vm, parser, scanner);
        Assert(!"parse label function declaration here!");

    }break;

    case TOK_HLT: { parseSingleInstruction(vm, parser, scanner, Opcode::OP_HLT); }break;


    case TOK_RAW: { parseRaw(vm, parser, scanner); }break;

    case TOK_SEMICOLON: {
        printf("parsed ';' begin comment!");
        while (charPeek(scanner) != '\n' && !isAtEnd(scanner)) scannerAdvance(scanner); //consume all whitespace on the line
        parseAdvance(parser, scanner);
    }break;

    case TOK_IDENTIFIER: {
        printf("parsed identifier at start of line! this is a location in code to jump to\n");
        //this means the label is a function defining code that can be jumped to
        parseLabelCode(vm, parser, scanner);
    }break;

    case TOK_DOT: {
        printf("parsed label variable declaration at start of line!\n");
        // parseLabelDec(vm, parser, scanner);
        parseLabelData(vm, parser, scanner);
    }break;

    case TOK_EOF: { printf("end of file!\n"); }break;

    case TOK_CALL: {
        parseCALL(vm, parser, scanner, Opcode::OP_CALL);
    }break;
    case TOK_RET: {
        parseRET(vm, parser, scanner, Opcode::OP_RET);
    }break;

    default:
        error(parser, "unrecognized instruction type!");
    }
}

int eval_repl_entry(REPL* repl, char* buffer) {
    Scanner* scanner = &repl->scanner;
    Parser* parser = &repl->parser;
    scanner->lines[1] = buffer;
    repl->vm.backPatchTableSize = 0;
    printScannerLine(scanner, scanner->line);//print line 1

    skipWhitespace(scanner);
    if (*scanner->current == '/') {
        int result = repl_command(repl);
        return result;
    }
    else {

        u32 byteCount = repl->vm.byteCount;
        parseAdvance(parser, scanner);
        Token curTok = parser->current;
        while ((curTok.type != TOK_EOF) && !parser->hadError) {
            parseInstruction(&repl->vm, &repl->parser, &repl->scanner);


            if (parser->current.type == TOK_NEWLINE) {
                while (parser->current.type == TOK_NEWLINE) {
                    scanner->line++;
                    Assert(scanner->line < 256);//max line count for now
                    scanner->lines[scanner->line] = scanner->current;
                    printScannerLine(scanner, scanner->line);
                    parser_advance(parser, scanner);
                }
            }

            else if (parser->current.type == TOK_EOF) {
                printf("END OF FILE REACHED!\n");
            }
            else {
                errorAtCurrent(parser, "Expected next instruction on a new line!");
            }
            curTok = parser->current;

        }

        //resolve backpatching
        VM* vm = &repl->vm;
        if (vm->backPatchTableSize && !parser->hadError) {
            u32 size = vm->backPatchTableSize;
            for (u32 i = 0; i < size; i++) {

                char* labelStart = scanner->start;
                char* labelEnd = scanner->current;
                u32 len = labelEnd - labelStart;



                AssemblerBackPatch* patch = vm->backPatchTable + i;
                symbol_table_entry* entry = patch->hashEntry;

                Assert(entry);
                if (!entry->defined) {
                    char temp[32];
                    handmade_len_strcpy(temp, entry->name, entry->nameLen);
                    printf("label %s was never defined!\n", temp);
                    error(parser, "label was never defined");
                    break;
                }
                switch (entry->type) {
                case label_types::label_data: {
                    Assert(!"handle data labels!");
                    vm->backPatchTableSize--;
                }break;
                case label_types::label_code: {
                    // Assert(!"handle code labels!");
                    //for now we'll treat code addresses as very large
                    vm->bytecode[patch->byteCodeLocation + 0] = entry->byteOffset >> 16;
                    vm->bytecode[patch->byteCodeLocation + 1] = entry->byteOffset >> 8;
                    vm->bytecode[patch->byteCodeLocation + 2] = entry->byteOffset & 0xff;
                    vm->backPatchTableSize--;
                    // Assert(!"Implement more OP Codes depending on the context of what was patched, can probably handle that when we initially parse the OPcode");
                }break;
                default:{}break;
                }


            }
        }

        if (vm->backPatchTableSize) {
            error(parser, "back patch table not fully resolved after parsing!");
            Assert(!"Need to handle this case, backpatching hasn't been fully implemented in all cases yet");
        }


        if (byteCount != repl->vm.byteCount && !parser->hadError) {
            //assume there is always an instruction to execute after we parse, depends on how we want the REPL to work
            // executeInstruction(repl->vm);
            vm_run(repl->vm, scanner);
        }
        else if (parser->hadError) {
            printf("Error in parser! instructions discarded!\n");
        }

    }

    return 0;

}


void test_label_code(REPL* repl) {
    reset_vm(&repl->vm);

    const char* command = "jmp label\n label:\n LOAD $0 #1\n jmp label";
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    //we will continuously load 1 into register 0, until we hit the jump max count, and exit
    //1 will be loaded into register 0 32 times, so the final result will be 1
    Assert(repl->vm.registers[0] == 1);
}


void test_label_data(REPL* repl) {
    reset_vm(&repl->vm);
    const char* command = ".label1 \"hello\" \n .label2 \" world! \"\n .label3 resb 256\n \
    LOAD $0 label1\n\
    LOAD $1 label2\n\
    LOAD $2 label3\n\
    LOAD [$2] [$0]\n\
    INC $2\n\
    INC $0\n\
    EQ [$0] $3\n\
    JEQ #36\n\
    JMP #12\n\
    LOAD [$2] [$1]\n\
    INC $2\n\
    INC $1\n\
    EQ [$1] $3\n\
    JEQ #60\n\
    JMP #36\n\
    LOAD $2 label3\n\
    PRT [$2]\n\
    ";

    // for(int i = 0; i < handmade_strlen(command); i++){
    //     printf("%c\n", command[i]);
    // }
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    //register 0 will be incremented 5 times as it iterates over all the characters in 'hello'
    //so we expect 5 to be in register 0
    Assert(repl->vm.registers[0] == 5);
}

void test_stack(REPL* repl) {
    reset_vm(&repl->vm);
    //factorial
    const char* command = " \
    LOAD $0 #12 ;0 \n\
    LOAD $2 $0  ;4 \n\
    PUSH $0     ;8 \n\
    POP $0      ;12\n\
    LOAD $1 #1  ;16\n\
    EQ $0 $1    ;20\n\
    JEQ #44     ;24\n\
    PUSH $0     ;28\n\
    DEC $0      ;32\n\
    PUSH $0     ;36\n\
    JMP #12     ;40\n\
    POP $1      ;44\n\
    MUL $0 $1 $0;48\n\
    NEQ $1 $2   ;52\n\
    JEQ #44     ;56\n\
    PRT $0      ;60\n\
    ";

    // for(int i = 0; i < handmade_strlen(command); i++){
    //     printf("%c\n", command[i]);
    // }
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    Assert(repl->vm.registers[0] == 479001600); //factorial of 12 is 479001600
}




void test_framePointer(REPL* repl) {
    reset_vm(&repl->vm);

    const char* command = "\
    LOAD $0 #2          ;0  \n\
    LOAD $1 #3          ;4  \n\
    PUSH $0             ;8  stack is now at 4\n\
    PUSH $1             ;12 stack is now at 8\n\
    CALL addtest        ;16 pushes return address onto stack, stack is now at 12\n\
    HLT                 ;20 \n\
                            \n\
    addtest:                \n\
    PUSH $30            ;24 save old frame pointer to stack, now at 16  \n\
    LOAD $30 $31        ;28 set frame pointer to current stack pointer  \n\
    LOAD $0 [$30 + 16]  ;32                                             \n\
    LOAD $1 [$30 + 12]  ;36                                             \n\
    ADD $0 $1 $0        ;40                                             \n\
    LOAD $31 $30        ;44 restore stack pointer (cleanup locals)      \n\
    POP $30             ;48 restore to old frame pointer                \n\
    RET                 ;52                                             \n\
    ";

    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    Assert(repl->vm.registers[0] == 5); //adds 2 + 3
}


void test_syscall(REPL* repl) {
    reset_vm(&repl->vm);

    const char* command = "\
    LOAD $0 #0          ;0  \n\
    LOAD $1 #13         ;4  \n\
    SYSCALL             ;8  \n\
    ";

    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);
    Assert(repl->vm.registers[0] == 0);
}



void test_fib(REPL* repl) {
    reset_vm(&repl->vm);
    //fib, the count (starting at 0) is loaded to register 2
    // const char* command = "\
    // LOAD $2 #8      ;0 \n\
    // LOAD $0 #0      ;4 \n\
    // LOAD $1 #1      ;8 \n\
    // LOAD $3 #1      ;12\n\
    // JEQ $2 $0 #52   ;16 check if given count is 0\n\
    // JEQ $2 $1 #52   ;20 check if given count is 1\n\
    // ADD  $0 $1 $0   ;24\n\
    // INC $3          ;28\n\
    // JEQ $3 $2 #52   ;32\n\
    // ADD  $1 $0 $1   ;36\n\
    // INC $3          ;40\n\
    // JEQ $3 $2 #52   ;44\n\
    // JMP #24         ;48\n\
    // PRT $0          ;52\n\
    // PRT $1          ;56\n\
    // LOAD $4 #2      ;60\n\
    // LOAD $5 #0      ;64\n\
    // DIV $3 $4 $3    ;68 remainder is sent to register 29\n\
    // JEQ $29 $5 #80  ;72 if given count is even, the final term is already in register 0\n\
    // LOAD $0 $1      ;76\n\
    // ";

    const char* command = "\
    LOAD $2 #8      ;0 \n\
    LOAD $0 #0      ;4 \n\
    LOAD $1 #1      ;8 \n\
    LOAD $3 #0      ;12\n\
    JEQ $0 $2 #48   ;16\n\
    JEQ $1 $2 #48   ;20\n\
    PUSH $0         ;24\n\
    ADD $0 $1 $0    ;28\n\
    POP $1          ;32\n\
    INC $3          ;36\n\
    JEQ $3 $2 #48   ;40\n\
    JMP #24         ;44\n\
    PRT $0          ;48\n\
    ";


    // for(int i = 0; i < handmade_strlen(command); i++){
    //     printf("%c\n", command[i]);
    // }
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    Assert(repl->vm.registers[0] == 21); //fib
}



void test_repl_reg_val(const char* command, u32 reg, int val) {
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);
    buffer[len] = 0;

    REPL* repl = (REPL*)malloc(sizeof(REPL));

    reset_vm(&repl->vm);

    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);

    Assert(repl->vm.registers[reg] == val);

    free(repl);//, sizeof(REPL)

}

void test_compiler(REPL* repl) {
    reset_vm(&repl->vm);

    const char* command = "\
        LOAD $0  #12        ;0\n\
        SUB $31 $0          ;4\n\
        LOAD $30 $31        ;8\n\
        LOAD  $0 #1         ;12\n\
        LOAD  [$30 + 4] $0  ;16\n\
        LOAD  $0 #2         ;20\n\
        LOAD  [$30 + 8] $0  ;24\n\
        LOAD  $0 #0         ;28\n\
        LOAD  [$30 + 12] $0 ;32\n\
        LOAD  $0 [$30 + 4]  ;36\n\
        LOAD  $1 [$30 + 8]  ;40\n\
        ADD $0 $1           ;44\n\
        LOAD  [$30 + 12] $0 ;48\n\
    ";

    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);
    Assert(repl->vm.registers[0] == 3);
}



void test_forloop(REPL* repl) {
    reset_vm(&repl->vm);

    //locals are x and i 
    #if 0
    const char* command = "\
        LOAD $0  #8         ;0 \n\
        SUB $31 $0          ;4 \n\
        LOAD $30 $31        ;8 \n\
        LOAD  $0 #0         ;12\n\
        LOAD  [$30 + 4] $0  ;16\n\
        LOAD  $0 #0         ;20\n\
        LOAD  [$30 + 8] $0  ;24\n\
        LOAD $1 [$30 + 8]   ;28\n\
        ;ONLY HANDLES exact comparison, no greater than or less then \n\
        LOAD $2 #6          ;32\n\
        JEQ $1 $2 #72       ;36\n\
        LOAD $0 [$30 + 4]   ;40\n\
        LOAD $3 #1          ;44\n\
        ADD  $0 $3          ;48\n\
        LOAD [$30 + 4] $0   ;52\n\
        ;for loops can ONLY handle prefix ++ for now and generate an INC operator\n\
        INC $1              ;56\n\
        LOAD [$30 + 8] $1   ;60\n\
        LOAD $4 #36         ;64\n\
        JMPB $4             ;68\n\
        HLT                 ;72\n\
    ";
    #else
    const char* command = "\
        LOAD     $0          #16        ;0  \n\
        SUB      $31         $0         ;4  \n\
        LOAD     $30         $31        ;8  \n\
        LOAD     $0          #0         ;12 \n\
        LOAD    [$30 + 4 ]   $0         ;16 \n\
        LOAD     $0          #1         ;20 \n\
        LOAD    [$30 + 8 ]   $0         ;24 \n\
        LOAD     $0          #0         ;28 \n\
        LOAD    [$30 + 12]   $0         ;32 \n\
        LOAD     $0         [$30 + 12]  ;36 \n\
        LOAD     $1          #7         ;40 \n\
        LT       $0          $1         ;44 \n\
        JNE      #100                 ;48 \n\
        LOAD     $2         [$30 + 8 ]  ;52 \n\
        LOAD    [$30 + 16]   $2         ;56 \n\
        LOAD     $2         [$30 + 8 ]  ;60 \n\
        LOAD     $3         [$30 + 4 ]  ;64 \n\
        ADD      $2          $3         ;68 \n\
        LOAD    [$30 + 8 ]   $2         ;72 \n\
        LOAD     $4         [$30 + 16]  ;76 \n\
        LOAD    [$30 + 4 ]   $4         ;80 \n\
        INC      $0                     ;84 \n\
        LOAD    [$30 + 16]   $0         ;88 \n\
        LOAD     $5          #56        ;92 \n\
        JMPB     $5                     ;96 \n\
";
    #endif
    size_t len = handmade_strlen(command);
    Assert(len < MAX_REPL_BUFFER);
    char buffer[MAX_REPL_BUFFER];
    memcpy(buffer, command, len);

    buffer[len] = 0;
    Scanner* scanner = &repl->scanner;
    repl->parser = {}; //clear 
    repl->scanner = {}; //clear 
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    eval_repl_entry(repl, buffer);
    Assert(repl->vm.registers[2] == 21);
}


void vm_repl() {
    char buffer[MAX_REPL_BUFFER];
    REPL* repl = (REPL*)malloc(sizeof(REPL));
    reset_vm(&repl->vm);

    Scanner* scanner = &repl->scanner;
    Parser* parser = &repl->parser;
    scanner->line = 1;
    scanner->current = buffer;
    scanner->start = buffer;

    memset(buffer, 0, sizeof(char) * MAX_REPL_BUFFER);

    bool isRunning = true;

        repl->vm.pc = 0;
        repl->vm.byteCount = 0;
    while (isRunning) {
        printf("> ");
        fflush(stdout);

        if (fgets(buffer, sizeof(char) * MAX_REPL_BUFFER, stdin) == NULL) {
            printf("Error reading input, please try again\n");
            continue;
        }
        scanner->current = buffer;
        scanner->start = buffer;
        parser->hadError = false;

        repl->vm.backPatchTableSize = 0;


        //copy the current entry into the history
        memcpy(repl->history + repl->historyLines * MAX_REPL_BUFFER, buffer, sizeof(char) * MAX_REPL_BUFFER);
        repl->historyLines++;

        int result = eval_repl_entry(repl, buffer);
        //code to exit successfully/quit command
        if (result == 1) break;

#if 0

        for (;;) {//parser loop 
            break;
            int result = parseHex(scanner, &vm);
            if (result == 0) {
                executeInstruction(vm);
                printf("parsed hex, breaking early\n");
                break;
            }
            else {
                printf("failed to parse hex!\n");
            }
            if (isAtEnd(scanner) || *scanner->current == '\n') {
                printf("REACHED END OF INPUT!\n");
                break;
            }
            skipWhitespace(scanner);
            if (*scanner->current == '/' || *scanner->current == '.') {
                repl_command(repl);
            }
        }
#endif



        memset(buffer, 0, sizeof(char) * MAX_REPL_BUFFER);
    }



    free(repl);//, sizeof(REPL)
}


void vm_test() {
    printf("vm test!\n");
    size_t mem_size = sizeof(VM);
    VM* vm = (VM*)malloc(mem_size);
    reset_vm(vm);
    #if 1
    //TESTING
    test_repl_reg_val("LOAD $0 #1000\n", 0, 1000);
    test_repl_reg_val("LOAD $0 #1000\n LOAD $1 #1000\n ADD $0 $1 $2\n", 2, 2000);
    test_repl_reg_val("LOAD $0 #1000\n LOAD $1 #1000\n ADD $0 $1   \n", 0, 2000);
    test_repl_reg_val("LOAD $0 #20  \n LOAD $1 #2   \n SUB $0 $1 $2\n", 2, 18);
    test_repl_reg_val("LOAD $0 #2000\n LOAD $1 #2   \n MUL $0 $1 $2\n", 2, 4000);
    test_repl_reg_val("LOAD $0 #2000\n LOAD $1 #1000\n DIV $0 $1 $2\n", 2, 2);

    //jump to instruction 20 which loads 2 to reg0. reg0 = 2
    test_repl_reg_val(" LOAD $0 #1\n  LOAD $1 #1\n    LOAD $2 #20\n  EQ $0 $1\n    JEQ $2\n   LOAD $0 #2\n ", 0, 2);

    //jump to instruction 4 if equal, add again, reg1 = 2
    test_repl_reg_val(" LOAD $2 #4\n LOAD $0 #1\n ADD $0 $1 $1\n  EQ $0 $1\n   JEQ $2\n  ", 1, 2);

    //jump back until max jumps reached, which is 16, reg1 should have been added enough times to be 16
    test_repl_reg_val(" LOAD $2 #8\n LOAD $0 #1\n ADD $0 $1 $1\n  EQ $0 $1\n   JMPB $2\n  ", 1, MAX_JUMPS);

    //jump past add instruction, reg1 should be 0
    test_repl_reg_val(" LOAD $2 #8\n LOAD $0 #1\n JMPF $2\n ADD $0 $1 $1\n  ", 1, 0);

    //jumps forward before we add to reg1, reg1 = 0
    test_repl_reg_val("LOAD $2 #20\n   LOAD $0 #1\n    NEQ $0 $1\n     JEQ $2\n        ADD $0 $1 $1\n  ", 1, 0);

    //jumps forward before we add to reg1, reg1 = 0
    test_repl_reg_val(" LOAD $2 #20\n    LOAD $0 #1\n     GT $0 $1\n       JEQ $2\n         ADD $0 $1 $1\n   ", 1, 0);

    //jumps forward before we add to reg1, reg1 = 0
    test_repl_reg_val(" LOAD $2 #20\n    LOAD $0 #1\n     LT $1 $0\n       JEQ $2\n         ADD $0 $1 $1\n    ", 1, 0);

    //reg0 = 1, reg1 = 0, if(reg0 < reg1), jump past add, so reg1 = 0
    test_repl_reg_val(" LOAD $2 #20\n LOAD $0 #1\n  LTQ $1 $0\n    JEQ $2\n ADD $0 $1 $1\n    ", 1, 0);

    //reg0 = 1, reg1 = 0, if(reg0 > reg1), dont jump past add, so reg1 = 1
    test_repl_reg_val(" LOAD $2 #20\n    LOAD $0 #1\n     GTQ $1 $0\n      JEQ $2\n         ADD $0 $1 $1\n   ", 1, 1);

    //inc and dec test, reg0 + 1  + 1-1 = 1
    test_repl_reg_val("INC $0\n INC $0\n DEC $0\n   ", 0, 1);

    // test_repl_aloc();
    //END TESTING
    printf("current OPcode count: %d\n", Opcode::OP_COUNT);
    Assert(Opcode::OP_COUNT < 254); //make sure we are within 1 byte of opcode size
    #endif
    REPL* repl = (REPL*)malloc(sizeof(REPL));
    test_label_code(repl);
    test_label_data(repl);
    test_stack(repl);
    test_fib(repl);
    test_framePointer(repl);
    test_syscall(repl);
    test_compiler(repl);
    test_forloop(repl);
    free(repl);//, sizeof(REPL)

    // vm_run(*vm);


    vm_repl();

    free(vm);//, mem_size

}





int main(){
    vm_test();
}



