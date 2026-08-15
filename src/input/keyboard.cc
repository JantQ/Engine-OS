#include "keyboard.hh"

UINT16 Keyboard::ReadKey() {
    UINT8 raw = PS2_ReadScanCode();

    if (raw == SC_EXTENDED) {
        UINT8 ext = PS2_ReadScanCode();
        switch (ext) {
            case SC_ALTGR_MAKE: altGrPressed = true; return 0;
            case SC_ALTGR_BREAK: altGrPressed = false; return 0;
            case SC_LCTRL_MAKE: ctrlPressed = true; return 0;
            case SC_LCTRL_BREAK: ctrlPressed = false; return 0;
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            case 0x47: return KEY_HOME;
            case 0x4F: return KEY_END;
            case 0x53: return KEY_DELETE; 
        }
        return 0;
    }

    if (raw == SC_LCTRL_MAKE) { ctrlPressed = true; return 0; }
    if (raw == SC_LCTRL_BREAK) {ctrlPressed = false; return 0; }

    if (raw == SC_LSHIFT_MAKE || raw == SC_RSHIFT_MAKE)   { shiftPressed = true;  return 0; }
    if (raw == SC_LSHIFT_BREAK || raw == SC_RSHIFT_BREAK) { shiftPressed = false; return 0; }

    bool isRelease = raw & 0x80;
    UINT8 code = raw & 0x7F;

    if (isRelease) return 0;

    if (code == 0x01) return KEY_ESC;

    if (ctrlPressed) {
        if (code == 0x1F) return KEY_CTRL_S;
        if (code == 0x10) return KEY_CTRL_Q;
        return 0;
    }

    if (code == 0x56) {
        return altGrPressed ? '\x05' : (shiftPressed ? '>' : '<');
    }

    if (code >= sizeof(scanCodeSet1)) return 0;

    char c = 0;
    if (altGrPressed && code < sizeof(scanCodeSet1AltGr)) c = scanCodeSet1AltGr[code];
    if (c == 0 && shiftPressed) c = scanCodeSet1Shift[code];
    if (c == 0) c = scanCodeSet1[code];

    return (UINT16)(UINT8)c;
}

void Keyboard::PumpState() {
    while (HasScanCode()) {
        UINT8 raw = PS2_ReadScanCode();
        bool extended = false;

        if (raw == SC_EXTENDED) {
            raw = PS2_ReadScanCode();
            extended = true;
        }

        bool released = raw & 0x80;
        UINT8 code = raw & 0x7F;
        UINT16 key = 0;

        if (extended) {
            switch (code) {
                case 0x48: key = KEY_UP; break;
                case 0x50: key = KEY_DOWN; break;
                case 0x4B: key = KEY_LEFT; break;
                case 0x4D: key = KEY_RIGHT; break;
            }
        } else if (code == 0x01) {
            key = KEY_ESC;
        } else if (code < sizeof(scanCodeSet1)) {
            key = (UINT16)(UINT8)scanCodeSet1[code];
        }

        if (key) keysDown[key] = !released;
    }
}