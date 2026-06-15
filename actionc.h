#define FOPEN(f,name,opt)	f = fopen(name, opt); \
    if (!f) fatal("cannot open file " #name);

void fatal(const char *msg);
uint8_t read6502(uint16_t address);
uint16_t read6502word(uint16_t address);
