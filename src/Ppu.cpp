#include "Ppu.hpp"
#include "NES.hpp"
#include <bitset>
#include <iostream>
#include <memory>
#include <cmath>

#define UNUSED(x) (void)(x)

Ppu::Ppu() {
    clear();
}

void Ppu::setNESHandle(NES& nes) & {
    this->nes = std::make_shared<NES>(nes);
}

constexpr inline bool inRange(const uint16_t& min, const uint16_t& max, const uint16_t& val) {
    return val <= max && val >= min;
}

typename Ppu::PatternTableT Ppu::getPatternTile(const uint16_t& tileAddress) const {
    if (tileAddress >= 0x2000 - 0xF) throw std::runtime_error("Given tile address it not a pattern table address");

    PatternTableT tile{};
    // Create a line of a tile
    // For every bit position of left and right, set it to the two bits equivalent position
    // in a 16 bit field, ex if left -> 0 , right -> 1, then bitPos(line) = 10 in the u16 type
    // But note that setting left to right immeditely to 0 and 1 reverses the line, instead set to bits 16 and 15
    auto createLine = [](const uint8_t& left, const uint8_t& right) -> uint16_t {
            uint16_t line = 0;
            for (short bitPos = 0, topBitLoc = 0; bitPos != 8; bitPos++, topBitLoc+=2) {
                const uint16_t pow2 = static_cast<uint16_t>(std::pow(2, bitPos)); // select which bit to choose
                // Take the bit and move to 1's position, then move it to the line's bits bottom down
                line |= ( (static_cast<uint16_t>(right) & pow2) >> bitPos) << (15 - topBitLoc);
                line |= ( (static_cast<uint16_t>(left) & pow2) >> bitPos) << (15 - topBitLoc - 1);
            }
            return line;
    };

    for (unsigned i = 0; i != 8; i++) {
        tile[i % 8] = createLine(nes->ppu.memory[tileAddress + i], nes->ppu.memory[tileAddress + i + 8]);
    }
    return tile;
}

void Ppu::stdDrawPatternTile(const uint16_t& tileAddress) const {
    PatternTableT tile = getPatternTile(tileAddress);
    for (uint8_t y = 0; y != 8; y++) {
        uint16_t line = tile[y];
        for (uint8_t x = 0; x != 8; x++) {
            uint8_t pixel = ( line >> (x * 2) ) & 0b11;
            std::cout << static_cast<int>(pixel);
        }
        std::cout << "\n";
    }
    std::cout << std::endl;

}

void Ppu::vRamWrite(const uint16_t& adr, const uint8_t& val) {
    if (inRange(0x3000, 0x3EFF, adr))
        memory[0x2000 + adr % 1000] = val;
    else if (inRange(0x3F20, 0x3FFF, adr))
        memory[0x3F00 + adr % 0x20] = val;
    else
        memory[adr] = val;
}

uint8_t Ppu::vRamRead(const uint16_t& adr) const {
    if (inRange(0x3000, 0x3EFF, adr))
        return memory[0x2000 + adr % 3000];
    else if (inRange(0x3F20, 0x3FFF, adr))
        return memory[0x3F00 + adr % 0x20];
    else
        return memory[adr];
}

// https://wiki.nesdev.com/w/index.php/PPU_registers
// https://wiki.nesdev.com/w/index.php/PPU_scrolling
uint8_t Ppu::readRegister(const uint16_t& adr) {
    switch(adr) {
        case 0x2002: { // Status < read
            uint8_t stat = PpuStatus.asByte();
            PpuStatus.sOverflow = 0;
            writeToggle = 0;
            /* TODO :
             * Set at dot 1 of line 241 (the line *after* the post-render
             * line); cleared after reading $2002 and at dot 1 of the
             * pre-render line.
             */
            return stat;
        }
        case 0x2004: // OAM data <> Read/Write
            return OAM[OamAddr];
        case 0x2007: // Data <> Read/Write
            return vRamRead(vAdr);
        default:
            std::cerr << "Error Read to address " << std::hex << "0x" << adr << " was detected\n";
            throw std::runtime_error("Attempted read to non PPU register or to a writeonly register of (dec) " + std::to_string(adr));
    }
}

// Same sources as readRegister
void Ppu::writeRegister(const uint16_t& adr, const uint8_t& val) {
    switch(adr) {
        case 0x2000: { // Controller > Write
            PpuCtrl.fromByte(val);
            // t: ...xx.. ........ = d: ......xx
            uint8_t bits2 = val & 0b11;
            vTempAdr &= ~0xC00; // clear the spot where these bits will go
            vTempAdr |= static_cast<uint16_t>(bits2) << 10; // move the bits into bit 10, 11
            break;
        }
        case 0x2001: // Mask > Write
            PpuMask.fromByte(val);
            break;
        case 0x2003: // OAM address > Write
            OamAddr = val;
            break;
        case 0x2004: // OAM data <> Read/Write
            OAM[OamAddr++] = val;
            break;
        case 0x2005: // Scroll >> write x2
            if (writeToggle == 0) { // first write is X
                vTempAdr &= ~0x1F; // clear and set the first 5 bits of tempVram to the byte
                vTempAdr |= val >> 3;
                fineXScroll = val & 0b111;
                writeToggle = 1;
                scrollPos = (~0x00FF & scrollPos) | val;
            }
            else { // second write is Y
                vTempAdr &= ~0x73E0; // fill in order of CBA..HG FED...., d=HGFEDCBA
                vTempAdr |= static_cast<uint16_t>(val & 0b111) << 12; // fil CBA
                vTempAdr |= static_cast<uint16_t>(val & 0xC0) << 2; // fill HG
                vTempAdr |= static_cast<uint16_t>(val & 0x38) << 2; // fill FED
                writeToggle = 0;
                scrollPos = static_cast<uint16_t>( (~0xFF00 & scrollPos) | static_cast<uint16_t>(val) << 8 );
            }
            break;
        case 0x2006: // Address >> write x2
            if (writeToggle == 0) { // load high byte of address
                vTempAdr = static_cast<uint16_t>( (vTempAdr & ~0x3F00) | (static_cast<uint16_t>(val & 0x3F) << 8) );
                writeToggle = 1;
            }
            else {
                vTempAdr = (vTempAdr & ~0xFF) | val;
                vAdr = vTempAdr;
                writeToggle = 0;
            }
            break;
        case 0x2007: // Data <> Read/Write
            // Note that the scrolling details are not implemented
            vRamWrite(vAdr, val);
            vAdr += PpuCtrl.increment == 0 ? 1 : 32;
            break;
        case 0x4014: {// OAM DMA > Write
                // Read/Write from cpu's XX00-XXFF, XX=val, to OAM
                uint16_t start = static_cast<uint16_t>(static_cast<uint16_t>(val) << 8),
                        end = start | 0xFF;
                while (start != end) {
                    OAM[OamAddr++] = nes->cpu.memory.read(start++);
                }
                break;
            }
        default:
            std::cerr << "Error Write to address " << std::hex << "0x" << adr << " was detected\n";
            throw std::runtime_error("Attempted write to non PPU register or to a readonly register of (dec) " + std::to_string(adr));
    }
}

// Both these functions address finding comes from here:
// https://wiki.nesdev.com/w/index.php/PPU_scrolling#Tile_and_attribute_fetching

void Ppu::fetchNameTableByte() {
    uint16_t tileAddress = 0x2000 | (vAdr & 0x0FFF);
    nameTable = vRamRead(tileAddress);
}

void Ppu::fetchAttrTableByte() {
    uint16_t attrAddress = 0x23C0 | (vAdr & 0x0C00) | ((vAdr >> 4) & 0x38) | ((vAdr >> 2) & 0x07);
    attrTable = vRamRead(attrAddress);
}

// Refer to https://wiki.nesdev.com/w/index.php/PPU_pattern_tables on left/right bit planes

void Ppu::fetchTableLowByte() {
    // TODO : Scroll Y from first 3 bits in vAdr to use in scrolling
    uint16_t address = PpuCtrl.bkgrdTile * 0x1000 + // Which pattern table to use
            + nameTable * 16; // which specific 8x8 CHR to use in the table, * 16 because both left and right tables are 8 bytes, 16 bytes to skip to next one.
    patternTableLow = vRamRead(address);
}
void Ppu::fetchTableHighByte() {
    uint16_t address = PpuCtrl.bkgrdTile * 0x1000 + nameTable * 16;
    patternTableHigh = vRamRead(address + 8); // high pattern table is 8 bytes after the low
}


void Ppu::clear() {
    PpuCtrl.clear();
    PpuMask.clear();
    PpuStatus.clear();
    std::fill(memory.begin(), memory.end(), 0);
    std::fill(OAM.begin(), OAM.end(), 0);
    OamAddr = scrollPos = 0;
    scanline = vAdr = vTempAdr = fineXScroll = writeToggle = 0;
}


void Ppu::runCycle() {

    if (inRange(0, 256, cycle)) { // Data for the current scanline, note that the first 2 tiles are already filled
        switch (cycle % 8) {
            case 1:
                fetchNameTableByte();
                break;
            case 3:
                fetchAttrTableByte();
                break;
            case 5:
                fetchTableLowByte();
                break;
            case 7:
                fetchTableHighByte();
                break;
        }
    }

}









uint8_t Ppu::PPUCTRL::asByte() const noexcept {
    uint8_t byte = 0;
    byte |= nameTable;
    byte |= increment << 2;
    byte |= spriteTile << 3;
    byte |= bkgrdTile << 4;
    byte |= spriteSz << 5;
    byte |= masterSlave << 6;
    byte |= NMI << 7;
    return byte;
}

void Ppu::PPUCTRL::fromByte(const uint8_t& byte) noexcept {
    nameTable = byte & 0b11;
    increment = (byte & 0b100) >> 2;
    spriteTile = (byte & 0x8) >> 3;
    bkgrdTile = (byte & 0x10) >> 4;
    spriteSz = (byte & 0x20) >> 5;
    masterSlave = (byte & 0x40) >> 6;
    NMI = (byte & 0x80) >> 7;
}

void Ppu::PPUCTRL::clear() noexcept {
    nameTable = increment = spriteTile = bkgrdTile = spriteSz = masterSlave = NMI = 0;
}

uint8_t Ppu::PPUMASK::asByte() const noexcept {
    uint8_t byte = 0;
    byte |= greyScale;
    byte |= bkgrdLeftEnable << 1;
    byte |= spriteLeftEnable << 2;
    byte |= bkgrdEnable << 3;
    byte |= spriteEnable << 4;
    byte |= red << 5;
    byte |= green << 6;
    byte |= blue << 7;
    return byte;
}

void Ppu::PPUMASK::fromByte(const uint8_t& byte) noexcept {
    greyScale = byte & 1;
    bkgrdLeftEnable = (byte & 2) >> 1;
    spriteLeftEnable = (byte & 0b100) >> 2;
    bkgrdEnable = (byte & 0x8) >> 3;
    spriteEnable = (byte & 0x10) >> 4;
    red = (byte & 0x20) >> 5;
    green = (byte & 0x40) >> 6;
    blue = (byte & 0x80) >> 7;
}

void Ppu::PPUMASK::clear() noexcept {
    greyScale = bkgrdLeftEnable = spriteLeftEnable = bkgrdEnable = spriteEnable = red = green = blue = 0;
}

uint8_t Ppu::PPUSTATUS::asByte() const noexcept {
    uint8_t byte = 0;
    byte |= sOverflow << 5;
    byte |= sprite0Hit << 6;
    byte |= vblank << 7;
    return byte;
}

void Ppu::PPUSTATUS::fromByte(const uint8_t& byte) noexcept {
    sOverflow = (byte & 0x20) >> 5;
    sprite0Hit = (byte & 0x40) >> 6;
    vblank = (byte & 0x80) >> 7;
}

void Ppu::PPUSTATUS::clear() noexcept {
    sOverflow = sprite0Hit = vblank = 0;
}
