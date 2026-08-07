#pragma once

#include "../kernel.hh"

class Crc {
    public:
        static UINT32 Crc32(const void *data, UINTN lenght) {
            const UINT8 *pointer = (const UINT8*)data;
            UINT32 crc = 0xFFFFFFFF;

            for (UINTN i = 0; i < lenght; i++) {
                crc ^= pointer[i];

                for (UINT32 b = 0; b < 8; b++) {
                    if (crc & 1) {
                        crc = (crc >> 1) ^ 0xEDB88320u;
                    }
                    else {
                        crc >>= 1;
                    }
                }
            }

            return ~crc;
        }
};