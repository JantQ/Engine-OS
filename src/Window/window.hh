#pragma once

#include "../kernel.hh" 

struct WindowInfo {
    double xPos;
    double yPos;

    UINT32 width;
    UINT32 height;
};

class Window {
    public:
        static inline WindowInfo Console {500, 500, 200, 200};
};