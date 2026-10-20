// A small CHIP-8 interpreter core. No GUI
#pragma once
#include <cstddef>
#include <cstdint>
#include <random>

// The three most common "quirks". Flip them if a ROM misbehaves.
// (Defaults match modern CHIP-8 ROMs; the original COSMAC VIP used true/true/false.)
constexpr bool SHIFT_USES_VY        = false;  // 8XY6/8XYE: shift VY (true) or VX in place (false)
constexpr bool LOADSTORE_INCREMENTS = false;  // FX55/FX65: leave I advanced (true) or unchanged (false)
constexpr bool JUMP_USES_VX         = false;  // BNNN: V0+NNN (false) or VX+NNN (true, SUPER-CHIP)

constexpr int      VIDEO_W      = 64;
constexpr int      VIDEO_H      = 32;
constexpr uint16_t START_ADDR   = 0x200;      // programs are loaded here
constexpr uint16_t FONT_ADDR    = 0x050;      // font lives here (any spot below 0x200 works)
constexpr uint32_t PIXEL_ON     = 0xFFFFFFFF;
constexpr uint32_t PIXEL_OFF    = 0xFF000000;

class Chip8 {
public:
    uint8_t  V[16]{};            // registers V0..VF (VF doubles as the flag register)
    uint8_t  memory[4096]{};     // 4 KB RAM
    uint16_t I{};                // index register
    uint16_t pc{};               // program counter
    uint16_t stack[16]{};        // return addresses
    uint8_t  sp{};               // stack pointer (number of entries in use)
    uint8_t  delayTimer{};       // counts down at 60 Hz
    uint8_t  soundTimer{};       // counts down at 60 Hz; beep while > 0
    uint8_t  keys[16]{};         // 1 = held down
    uint32_t video[VIDEO_W * VIDEO_H]{};   // framebuffer, one 32-bit pixel each
    uint16_t opcode{};           // the instruction being executed
    bool     halted = false;     // set on a bad opcode or stack error

    Chip8();
    bool loadBytes(const uint8_t* data, size_t size);
    bool loadFile(const char* path);
    void cycle();                // fetch, decode, execute ONE instruction
    void tick();                 // call at 60 Hz: decrement the timers

private:
    std::mt19937 rng;
};
