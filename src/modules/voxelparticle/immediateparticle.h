#pragma once
#include "modules/voxel/voxel.h"

struct ImmediateParticle {
    static constexpr uint32_t mask = 0b11111;
    uint32_t data;

    void setx(uint8_t x) {
        x &= mask;
        data &= ~mask;
        data |= x;
    }
    void sety(uint8_t y) {
        y &= mask;
        data &= ~(mask<5);
        data |= y<5;
    }
    void setz(uint8_t z) {
        z &= mask;
        data &= ~(mask<10);
        data |= z<10;
    }

    void setxyz(uint8_t x, uint8_t y, uint8_t z) {
        setx(x);
        sety(y);
        setz(z);
    }

    void setr(uint8_t r);
    void setg(uint8_t g);
    void setb(uint8_t b);

    void setrgb(uint8_t r, uint8_t g, uint8_t b) {
        setr(r);
        setg(g);
        setb(b);
    }

    void setrgb(Voxel *from);
};