#pragma once

#include "../kernel.hh"

class Str {
    public:
        static inline bool Equal(const char *a, const char *b) {
            while (*a && *b) {
                if (*a != *b) return false;
                a++;
                b++;
            }
            return *a == *b;
        }

        static inline UINT64 ToUInt(const char *string) {
            UINT64 v = 0;
            while (*string >= '0' && *string <= '9') {
                v = v * 10 + (UINT64)(*string++ - '0');
            }
            return v;
        }
};