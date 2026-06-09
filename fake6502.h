#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

extern uint16_t PC;
extern uint8_t SP, A, X, Y, status;
void setP(uint8_t x);
uint8_t getP(void);

#define CPU_SetN	N=TRUE
#define CPU_ClrN	N=FALSE
extern bool C, Z, I, D, V, N;
extern uint8_t read6502(uint16_t address);
extern void write6502(uint16_t address, uint8_t value);
extern uint16_t  pull16(void);
extern uint16_t read6502word(uint16_t addr);
extern void write6502(uint16_t address,uint8_t val);
extern void write6502word(uint16_t addr, uint16_t val);
void diss(uint8_t a, uint8_t x, uint8_t y);

int nmi6502(void);
int reset6502(void);
int irq6502(void);
int step6502(void);
