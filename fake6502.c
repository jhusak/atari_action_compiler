/*
 * Fake6502 -- MOS6502 CPU Emulator
 *
 * Copyright © 2011-2013 Mike Chambers
 * Copyright © 2024 Ivo van poorten
 *
 * This file is licensed under the terms of the 2-clause BSD license. Please
 * see the LICENSE file in the root project directory for the full text.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "fake6502.h"

static void (*addrtable[256])();
static void (*optable[256])();

uint16_t PC;
uint8_t SP, A, X, Y;
bool C, Z, I, D, V, N;
static uint16_t ea;
static uint8_t opcode;

// ------------------ Flags ---------------------------------------------------

static inline void calcZ  (uint8_t  x) { Z = !x; }
static inline void calcN  (uint8_t  x) { N = x & 0x80; }
static inline void calcZN (uint8_t x)  { calcZ(x), calcN(x); }
static inline void calcC  (uint16_t x) { C = x & 0xff00; }
static inline void calcCZN(uint16_t x) { calcC(x), calcZN(x); }

static inline void calcV(uint16_t result, uint8_t accu, uint16_t value) {
    V = (result ^ accu) & (result ^ value) & 0x80;
}

void setP(uint8_t x) {
    N=x&0x80, V=x&0x40, D=x&8, I=x&4, Z=x&2, C=x&1;
}

uint8_t getP(void){ return (N<<7)|(V<<6)|(1<<5)|(0<<4)|(D<<3)|(I<<2)|(Z<<1)|C;}

// ----------------------------------------------------------------------------


static void push8(uint8_t pushval) { write6502(0x0100 + SP--, pushval); }
static uint8_t pull8() { return read6502(0x0100 + ++SP); }

static void push16(uint16_t pushval) {
	push8(pushval>>8);
	push8(pushval);
}
uint16_t pull16() {
    return pull8() | (pull8()<<8);
}

uint16_t read6502word(uint16_t addr) {
    return read6502(addr) | (read6502(addr+1) << 8);
}

// ------------------ Addressing modes ----------------------------------------

static void imp()  { }
static void acc()  { }
static void imm()  { ea = PC++; }
static void zp()   { ea = read6502(PC++); }
static void zpx()  { ea = (read6502(PC++) + X) & 0xff; }
static void zpy()  { ea = (read6502(PC++) + Y) & 0xff; }
static void abso() { ea = read6502word(PC); PC += 2; }
static void rel()  { ea = PC+1; ea += (int8_t )read6502(PC++); }
static void absx() { ea = read6502word(PC); ea += X; PC += 2; }
static void absy() { ea = read6502word(PC); ea += Y; PC += 2; }
static void ind() {
    ea = read6502word(PC);
    uint16_t ea2 = (ea & 0xff00) | ((ea + 1) & 0xff); // page wrap bug!
    ea = read6502(ea) | (((uint16_t)read6502(ea2)) << 8);
    //printf("EA: %04x\n",ea);
    PC += 2;
}

static void indx() {
    ea = ((read6502(PC++) + X) & 0xff);             // page wraparound
    ea = read6502(ea) | (read6502((ea+1) & 0xff) << 8);
}

static void indy() { // (indirect),Y
    ea = read6502(PC++);
    ea = read6502(ea) | (read6502((ea+1) & 0xff) << 8);  // page wrap
    ea += Y;
}

// ----------------------------------------------------------------------------

static inline uint16_t getvalue() {
    return addrtable[opcode] == acc ? A : read6502(ea);
}

static inline void putvalue(uint16_t saveval) {
    if (addrtable[opcode] == acc) A = saveval; else write6502(ea, saveval);
}

// ------------------ Opcodes -------------------------------------------------

static void and() { calcZN(A = A & getvalue()); }
static void eor() { A = A ^ getvalue(); calcZN(A); }
static void ora() { A |= getvalue(); calcZN(A); }

static void branch(bool condition) {
    if (condition) {
        PC = ea;
    }
}

static void bcc() { branch(!C); }
static void bcs() { branch( C); }
static void bne() { branch(!Z); }
static void beq() { branch( Z); }
static void bpl() { branch(!N); }
static void bmi() { branch( N); }
static void bvc() { branch(!V); }
static void bvs() { branch( V); }

static void clc() { C = 0; }
static void sec() { C = 1; }
static void cld() { D = 0; }
static void sed() { D = 1; }
static void cli() { I = 0; }
static void sei() { I = 1; }
static void clv() { V = 0; }

static void inx() { calcZN(++X); }
static void iny() { calcZN(++Y); }
static void dex() { calcZN(--X); }
static void dey() { calcZN(--Y); }

static void jmp() { PC = ea; }
static void jsr() { push16(PC - 1); PC = ea; }

static void lda() { A = getvalue(); calcZN(A); }
static void ldx() { X = getvalue(); calcZN(X); }
static void ldy() { Y = getvalue(); calcZN(Y); }
static void sta() { putvalue(A); }
static void stx() { putvalue(X); }
static void sty() { putvalue(Y); }

static inline void compare(uint8_t reg, uint8_t value) {
    calcN(reg - value);
    C = reg >= value;
    Z = reg == value;
}
static void cmp() { compare(A, getvalue()); }
static void cpx() { compare(X, getvalue()); }
static void cpy() { compare(Y, getvalue()); }

static void pha() { push8(A); }
static void php() { push8(getP() | 0x10); }
static void pla() { A = pull8(); calcZN(A); }
static void plp() { uint8_t P = pull8(); setP(P); }

static void rti() { uint8_t P = pull8(); setP(P); PC = pull16(); }
static void rts() { PC = pull16() + 1; }

static void tax() { X = A; calcZN(X); }
static void tay() { Y = A; calcZN(Y); }
static void tsx() { X = SP; calcZN(X); }
static void txa() { A = X; calcZN(A); }
static void txs() { SP = X; }
static void tya() { A = Y; calcZN(A); }

static void bit() {
    uint16_t value = getvalue();
    calcZ(A & value);
    N = value & 0x80;
    V = value & 0x40;
}

static void brk() {
    push16(++PC);                 // address before next instruction
    php();
    I = 1;
    PC = read6502word(0xfffe);
}

static void dec() {
    uint16_t result = getvalue() - 1;
    calcZN(result);
    putvalue(result);
}

static void inc() {
    uint16_t result = getvalue() + 1;
    calcZN(result);
    putvalue(result);
}

static void asl() {
    uint16_t result = getvalue() << 1;
    calcCZN(result);
    putvalue(result);
}

static void lsr() {
    uint16_t value = getvalue();
    uint16_t result = value >> 1;
    C = value & 1;
    calcZN(result);
    putvalue(result);
}

static void rol() {
    uint16_t result = (getvalue() << 1) | C;
    calcCZN(result);
    putvalue(result);
}

static void ror() {
    uint16_t value = getvalue();
    uint16_t result = (value >> 1) | (C << 7);
    C = value & 1;
    calcZN(result);
    putvalue(result);
}

static void nop() {
}

static void adc() {
    
    uint16_t value = getvalue();
    uint16_t result = A + value + C;
    calcZ(result);

    if (!D) {
        calcC(result);
        calcV(result, A, value);
        calcN(result);
    } else {
        result = (A & 0x0f) + (value & 0x0f) + C;
        if (result >= 0x0a) result = ((result + 0x06) & 0x0f) + 0x10;
        result += (A & 0xf0) + (value & 0xf0);
        calcN(result);
        calcV(result, A, value);
        if (result >= 0xa0) result += 0x60;
        calcC(result);
    }

    A = result;
}

static void sbc() {
    bool cC = C;
    
    uint16_t value = getvalue() ^ 0xff;
    uint16_t result = A + value + C;
    calcCZN(result);
    calcV(result, A, value);

    if (D) {
        uint16_t AL, B;
        B = value ^ 0xff;
        AL = (A & 0x0f) - (B & 0x0f) + cC - 1;
        if(AL & 0x8000)  AL =  ((AL - 0x06) & 0x0f) - 0x10;
        result = (A & 0xf0) - (B & 0xf0) + AL;
        if(result & 0x8000) result -= 0x60;
    }

    A = result;
}

// ------------------ Stable undocumented opcodes -----------------------------

static void SLO() { asl(); ora(); }
static void RLA() { rol(); and(); }
static void SRE() { lsr(); eor(); }
static void RRA() { ror(); adc(); }
static void SAX() { putvalue(A & X); }
static void LAX() { lda(); ldx(); }
static void DCP() { dec(); cmp(); }
static void ISC() { inc(); sbc(); }
static void ANC() { and(); C = A & 0x80; }
static void ALR() { and(); C = A & 1; A >>= 1; calcZN(A); }
static void LAS() { calcZN(SP = A = X = getvalue() & SP); }
static void JAM() { nop(); }


static void ARR() {
    and();

    uint8_t inA = A;

    A >>= 1;
    A |= C << 7;
    calcZN(A);

    if (!D) {
        C = A & 0x40;
        V = C ^ ((A >> 5) & 1);
    } else {
        V = (A ^ inA) & 0x40;
        if (((inA & 0x0f) + (inA & 0x01)) > 0x05)
            A = (A & 0xf0) | ((A + 0x06) & 0x0f);
        if ((uint16_t)inA + (inA & 0x10) >= 0x60) {
            A += 0x60;
            C = 1;
        } else {
            C = 0;
        }
    }
}

static void SBX() {
    uint8_t value = getvalue();
    X &= A;
    compare(X, value);
    X -= value;
}

// ------------------ Unstable undocumented opcodes ---------------------------

static void SHA() { putvalue(A & X & ((ea >> 8) + 1)); }
static void SHX() {
    uint8_t value = X & (((ea - Y) >> 8) + 1);
    if (((ea - Y) & 0xff) + Y > 0xff)
        ea = (ea & 0xff) | value << 8;
    putvalue(value);
}
static void SHY() {
    uint8_t value = Y & (((ea-X) >> 8) + 1);
    if (((ea - X) & 0xff) + X > 0xff)
        ea = (ea & 0xff) | value << 8;
    putvalue(value);
}
static void TAS() { SP = A & X; putvalue(SP & ((ea >> 8) + 1));
}

// ------------------ Magic constants undocumented opcodes --------------------

static void ANE() { A = (A | 0xef) & X & getvalue(); calcZN(A); }
static void LXA() { A = X = ( A | 0xee) & getvalue(); calcZN(A); }

// ----------------------------------------------------------------------------

#define MAKE_ADDR_TABLE(t) 	\
t = {\
/* 0    1   2    3   4   5   6   7   8    9   A    B    C    D    E    F */\
  imp,indx,imp,indx, zp, zp, zp, zp,imp, imm,acc, imm,abso,abso,abso,abso, /* 0*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx, /* 1*/\
 abso,indx,imp,indx, zp, zp, zp, zp,imp, imm,acc, imm,abso,abso,abso,abso, /* 2*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx, /* 3*/\
  imp,indx,imp,indx, zp, zp, zp, zp,imp, imm,acc, imm,abso,abso,abso,abso, /* 4*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx, /* 5*/\
  imp,indx,imp,indx, zp, zp, zp, zp,imp, imm,acc, imm, ind,abso,abso,abso, /* 6*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx, /* 7*/\
  imm,indx,imm,indx, zp, zp, zp, zp,imp, imm,imp, imm,abso,abso,abso,abso, /* 8*/\
  rel,indy,imp,indy,zpx,zpx,zpy,zpy,imp,absy,imp,absy,absx,absx,absy,absy, /* 9*/\
  imm,indx,imm,indx, zp, zp, zp, zp,imp, imm,imp, imm,abso,abso,abso,abso, /* A*/\
  rel,indy,imp,indy,zpx,zpx,zpy,zpy,imp,absy,imp,absy,absx,absx,absy,absy, /* B*/\
  imm,indx,imm,indx, zp, zp, zp, zp,imp, imm,imp, imm,abso,abso,abso,abso, /* C*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx, /* D*/\
  imm,indx,imm,indx, zp, zp, zp, zp,imp, imm,imp, imm,abso,abso,abso,abso, /* E*/\
  rel,indy,imp,indy,zpx,zpx,zpx,zpx,imp,absy,imp,absy,absx,absx,absx,absx  /* F*/\
}

MAKE_ADDR_TABLE(static void (*addrtable[256])(void));

#define imp  1
#define acc  1
#define imm  2
#define zp   2
#define zpx  2
#define zpy  2
#define abso 3
#define absx 3
#define absy 3
#define rel  2
#define ind  3
#define indx 2
#define indy 2

MAKE_ADDR_TABLE(uint8_t lentable[256]);

#define MAKE_OPER_TABLE(table) 	\
static table  = {\
/*   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */\
    M(brk),M(ora),M(JAM),M(SLO),M(nop),M(ora),M(asl),M(SLO),M(php),M(ora),M(asl),M(ANC),M(nop),M(ora),M(asl),M(SLO), /* 0*/\
    M(bpl),M(ora),M(JAM),M(SLO),M(nop),M(ora),M(asl),M(SLO),M(clc),M(ora),M(nop),M(SLO),M(nop),M(ora),M(asl),M(SLO), /* 1*/\
    M(jsr),M(and),M(JAM),M(RLA),M(bit),M(and),M(rol),M(RLA),M(plp),M(and),M(rol),M(ANC),M(bit),M(and),M(rol),M(RLA), /* 2*/\
    M(bmi),M(and),M(JAM),M(RLA),M(nop),M(and),M(rol),M(RLA),M(sec),M(and),M(nop),M(RLA),M(nop),M(and),M(rol),M(RLA), /* 3*/\
    M(rti),M(eor),M(JAM),M(SRE),M(nop),M(eor),M(lsr),M(SRE),M(pha),M(eor),M(lsr),M(ALR),M(jmp),M(eor),M(lsr),M(SRE), /* 4*/\
    M(bvc),M(eor),M(JAM),M(SRE),M(nop),M(eor),M(lsr),M(SRE),M(cli),M(eor),M(nop),M(SRE),M(nop),M(eor),M(lsr),M(SRE), /* 5*/\
    M(rts),M(adc),M(JAM),M(RRA),M(nop),M(adc),M(ror),M(RRA),M(pla),M(adc),M(ror),M(ARR),M(jmp),M(adc),M(ror),M(RRA), /* 6*/\
    M(bvs),M(adc),M(JAM),M(RRA),M(nop),M(adc),M(ror),M(RRA),M(sei),M(adc),M(nop),M(RRA),M(nop),M(adc),M(ror),M(RRA), /* 7*/\
    M(nop),M(sta),M(nop),M(SAX),M(sty),M(sta),M(stx),M(SAX),M(dey),M(nop),M(txa),M(ANE),M(sty),M(sta),M(stx),M(SAX), /* 8*/\
    M(bcc),M(sta),M(JAM),M(SHA),M(sty),M(sta),M(stx),M(SAX),M(tya),M(sta),M(txs),M(TAS),M(SHY),M(sta),M(SHX),M(SHA), /* 9*/\
    M(ldy),M(lda),M(ldx),M(LAX),M(ldy),M(lda),M(ldx),M(LAX),M(tay),M(lda),M(tax),M(LXA),M(ldy),M(lda),M(ldx),M(LAX), /* A*/\
    M(bcs),M(lda),M(JAM),M(LAX),M(ldy),M(lda),M(ldx),M(LAX),M(clv),M(lda),M(tsx),M(LAS),M(ldy),M(lda),M(ldx),M(LAX), /* B*/\
    M(cpy),M(cmp),M(nop),M(DCP),M(cpy),M(cmp),M(dec),M(DCP),M(iny),M(cmp),M(dex),M(SBX),M(cpy),M(cmp),M(dec),M(DCP), /* C*/\
    M(bne),M(cmp),M(JAM),M(DCP),M(nop),M(cmp),M(dec),M(DCP),M(cld),M(cmp),M(nop),M(DCP),M(nop),M(cmp),M(dec),M(DCP), /* D*/\
    M(cpx),M(sbc),M(nop),M(ISC),M(cpx),M(sbc),M(inc),M(ISC),M(inx),M(sbc),M(nop),M(sbc),M(cpx),M(sbc),M(inc),M(ISC), /* E*/\
    M(beq),M(sbc),M(JAM),M(ISC),M(nop),M(sbc),M(inc),M(ISC),M(sed),M(sbc),M(nop),M(ISC),M(nop),M(sbc),M(inc),M(ISC)  /* F*/\
};
#define M(a)	a

MAKE_OPER_TABLE(void (*optable[256])());

int nmi6502() {
    push16(PC);
    push8(getP());
    I = 1;
    PC = read6502word(0xfffa);
    return 7;
}

int reset6502() {
    PC = read6502word(0xfffc);
    A = X = Y = C = Z = I = D = V = N = 0;
    SP = 0xFD;
    return 7;
}

int irq6502() {
    push16(PC);
    push8(getP());
    I = 1;
    PC = read6502word(0xfffe);
    return 7;
}

int step6502() {
    opcode = read6502(PC++);

    (*addrtable[opcode])();
    (*optable[opcode])();

    return 0;
}
