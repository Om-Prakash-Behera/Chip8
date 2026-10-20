#include "chip8.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

// 16 hex digits, 5 bytes each (4 pixels wide, 5 tall; only the top 4 bits of each byte are used)
static const uint8_t FONT[80] = {
    0xF0,0x90,0x90,0x90,0xF0, 0x20,0x60,0x20,0x20,0x70, 0xF0,0x10,0xF0,0x80,0xF0, 0xF0,0x10,0xF0,0x10,0xF0,
    0x90,0x90,0xF0,0x10,0x10, 0xF0,0x80,0xF0,0x10,0xF0, 0xF0,0x80,0xF0,0x90,0xF0, 0xF0,0x10,0x20,0x40,0x40,
    0xF0,0x90,0xF0,0x90,0xF0, 0xF0,0x90,0xF0,0x10,0xF0, 0xF0,0x90,0xF0,0x90,0x90, 0xE0,0x90,0xE0,0x90,0xE0,
    0xF0,0x80,0x80,0x80,0xF0, 0xE0,0x90,0x90,0x90,0xE0, 0xF0,0x80,0xF0,0x80,0xF0, 0xF0,0x80,0xF0,0x80,0x80};

Chip8::Chip8() : rng(std::random_device{}()) {
    pc = START_ADDR;
    std::memcpy(&memory[FONT_ADDR], FONT, sizeof FONT);
    for (auto& p : video) p = PIXEL_OFF;
}

bool Chip8::loadBytes(const uint8_t* data, size_t size) {
    if (size > sizeof(memory) - START_ADDR) return false;
    std::memcpy(&memory[START_ADDR], data, size);
    return true;
}

bool Chip8::loadFile(const char* path) {
    std::string actual_path = std::string(RESOURCES_PATH) + path;
    std::ifstream f(actual_path.c_str(), std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return loadBytes(rom.data(), rom.size());
}

void Chip8::tick() {
    if (delayTimer > 0) --delayTimer;
    if (soundTimer > 0) --soundTimer;
}

void Chip8::cycle() {
    if (halted) return;

    // 1. FETCH: an instruction is two bytes, high byte first.
    opcode = (memory[pc & 0xFFF] << 8) | memory[(pc + 1) & 0xFFF];
    pc = (pc + 2) & 0xFFF;

    // 2. DECODE: slice the opcode into the pieces instructions use.
    const uint8_t  x   = (opcode >> 8) & 0xF;
    const uint8_t  y   = (opcode >> 4) & 0xF;
    const uint8_t  n   = opcode & 0xF;
    const uint8_t  nn  = opcode & 0xFF;
    const uint16_t nnn = opcode & 0xFFF;

    // 3. EXECUTE
    switch (opcode >> 12) {
    case 0x0:
        if (opcode == 0x00E0) {                              // clear screen
            for (auto& p : video) p = PIXEL_OFF;
        } else if (opcode == 0x00EE) {                       // return from subroutine
            if (sp == 0) { halted = true; std::fprintf(stderr, "stack underflow\n"); return; }
            pc = stack[--sp];
        } else {
            halted = true;
        }
        break;

    case 0x1: pc = nnn; break;                               // jump

    case 0x2:                                                // call subroutine
        if (sp >= 16) { halted = true; std::fprintf(stderr, "stack overflow\n"); return; }
        stack[sp++] = pc;
        pc = nnn;
        break;

    case 0x3: if (V[x] == nn)   pc = (pc + 2) & 0xFFF; break;    // skip if VX == NN
    case 0x4: if (V[x] != nn)   pc = (pc + 2) & 0xFFF; break;    // skip if VX != NN
    case 0x5: if (V[x] == V[y]) pc = (pc + 2) & 0xFFF; break;    // skip if VX == VY
    case 0x9: if (V[x] != V[y]) pc = (pc + 2) & 0xFFF; break;    // skip if VX != VY
    case 0x6: V[x] = nn; break;                                  // set VX
    case 0x7: V[x] += nn; break;                                 // add to VX (no flag)

    case 0x8:
        switch (n) {
        case 0x0: V[x]  = V[y]; break;
        case 0x1: V[x] |= V[y]; break;
        case 0x2: V[x] &= V[y]; break;
        case 0x3: V[x] ^= V[y]; break;
        case 0x4: {                                              // add, VF = carry
            unsigned sum = V[x] + V[y];
            V[x] = sum & 0xFF;
            V[0xF] = sum > 0xFF;      // set the flag LAST so it wins when x == F
            break;
        }
        case 0x5: { bool ok = V[x] >= V[y]; V[x] = V[x] - V[y]; V[0xF] = ok; break; }   // VX = VX - VY
        case 0x7: { bool ok = V[y] >= V[x]; V[x] = V[y] - V[x]; V[0xF] = ok; break; }   // VX = VY - VX
        case 0x6: {
            uint8_t src = SHIFT_USES_VY ? V[y] : V[x];
            V[x] = src >> 1;
            V[0xF] = src & 1;
            break;
        }
        case 0xE: {
            uint8_t src = SHIFT_USES_VY ? V[y] : V[x];
            V[x] = src << 1;
            V[0xF] = src >> 7;
            break;
        }
        default: halted = true; break;
        }
        break;

    case 0xA: I = nnn; break;                                    // set index
    case 0xB: pc = (nnn + (JUMP_USES_VX ? V[x] : V[0])) & 0xFFF; break;
    case 0xC: V[x] = (rng() & 0xFF) & nn; break;                 // random AND NN

    case 0xD: {                                                  // draw sprite
        const int px0 = V[x] % VIDEO_W;      // the start position wraps...
        const int py0 = V[y] % VIDEO_H;
        V[0xF] = 0;
        for (int row = 0; row < n; ++row) {
            const int py = py0 + row;
            if (py >= VIDEO_H) break;        // ...but the sprite itself is clipped at the edge
            const uint8_t bits = memory[(I + row) & 0xFFF];
            for (int col = 0; col < 8; ++col) {
                const int px = px0 + col;
                if (px >= VIDEO_W) break;
                if (bits & (0x80 >> col)) {
                    uint32_t& pixel = video[py * VIDEO_W + px];
                    if (pixel == PIXEL_ON) V[0xF] = 1;           // a lit pixel was turned off
                    pixel = (pixel == PIXEL_ON) ? PIXEL_OFF : PIXEL_ON;   // XOR
                }
            }
        }
        break;
    }

    case 0xE:
        if      (nn == 0x9E) { if (keys[V[x] & 0xF])  pc = (pc + 2) & 0xFFF; }   // skip if key down
        else if (nn == 0xA1) { if (!keys[V[x] & 0xF]) pc = (pc + 2) & 0xFFF; }   // skip if key up
        else halted = true;
        break;

    case 0xF:
        switch (nn) {
        case 0x07: V[x] = delayTimer; break;
        case 0x15: delayTimer = V[x]; break;
        case 0x18: soundTimer = V[x]; break;
        case 0x1E: I += V[x]; break;
        case 0x0A: {                                             // wait for a key press
            bool pressed = false;
            for (int k = 0; k < 16; ++k) if (keys[k]) { V[x] = k; pressed = true; break; }
            if (!pressed) pc = (pc - 2) & 0xFFF;                 // not yet: run this instruction again
            break;
        }
        case 0x29: I = FONT_ADDR + (V[x] & 0xF) * 5; break;      // point I at a font digit
        case 0x33:                                               // binary -> decimal digits
            memory[I & 0xFFF]       = V[x] / 100;
            memory[(I + 1) & 0xFFF] = (V[x] / 10) % 10;
            memory[(I + 2) & 0xFFF] = V[x] % 10;
            break;
        case 0x55:                                               // store V0..VX
            for (int i = 0; i <= x; ++i) memory[(I + i) & 0xFFF] = V[i];
            if (LOADSTORE_INCREMENTS) I += x + 1;
            break;
        case 0x65:                                               // load V0..VX
            for (int i = 0; i <= x; ++i) V[i] = memory[(I + i) & 0xFFF];
            if (LOADSTORE_INCREMENTS) I += x + 1;
            break;
        default: halted = true; break;
        }
        break;
    }
    if (halted) std::fprintf(stderr, "halted on opcode %04X (pc=%03X)\n", opcode, (pc - 2) & 0xFFF);
}
