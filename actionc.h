#define FOPEN(f,name,opt)	f = fopen(name, opt); \
    if (!f) fatal("cannot open file " #name);

extern uint8_t action_lib[];
extern int bankA000_offset;
void fatal(const char *msg);
uint8_t read6502(uint16_t address);
uint16_t read6502word(uint16_t address);
