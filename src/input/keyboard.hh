#pragma once

#include "../kernel.hh"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64


#define SC_LSHIFT_MAKE  0x2A
#define SC_LSHIFT_BREAK 0xAA
#define SC_RSHIFT_MAKE  0x36
#define SC_RSHIFT_BREAK 0xB6
#define SC_EXTENDED     0xE0
#define SC_ALTGR_MAKE   0x38 
#define SC_ALTGR_BREAK  0xB8  

class Keyboard {
   public:

   static inline UINT8 inb(UINT16 port) {
      UINT8 result;
      __asm__ __volatile__("inb %1, %0" : "=a"(result) : "Nd"(port));
      return result;
   }

   static inline void outb(UINT16 port, UINT8 value) {
      __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
   }

   static UINT8 PS2_ReadScanCode() {
      while (!(inb(PS2_STATUS_PORT) & 0x01)) { }
      return inb(PS2_DATA_PORT);
   }

   static constexpr char scanCodeSet1[] = {
   0, 0 /*Esc*/, '1','2','3','4','5','6','7','8','9','0','+', 0 /* ´ UnSupported*/, '\b' /*Backspace*/,
   0 /*Tab*/, 'q','w','e','r','t','y','u','i','o','p','\x01' /*Å*/, 0 /*¨ dead key*/, '\n' /*Enter*/,
   0 /*Ctrl*/, 'a','s','d','f','g','h','j','k','l','\x03' /*Ö*/,'\x02' /*Ä*/, 0 /* ' UnSupported*/,
   0 /*LShift*/, '\'' /*ISO extra key*/, 'z','x','c','v','b','n','m',',','.','-', 0 /*RShift*/,
   '*', 0 /*Alt*/, ' ' /*Space*/};


   static constexpr char scanCodeSet1Shift[] = {
   0, 0, '!','"','#', 0 /*unsupported ¤*/, '%','&','/','(',')','=','?', 0,
   0, 0, 'Q','W','E','R','T','Y','U','I','O','P','\x01', 0 /*¨*/, 0,
   0, 'A','S','D','F','G','H','J','K','L','\x03','\x02', 0,
   0, 0, 'Z','X','C','V','B','N','M',';',':','_', 0,
   '*', 0, ' '};

   static constexpr char scanCodeSet1AltGr[] = {
   0,0,0,'@',0,'$',0,'&','{','[',']','}','\x04',0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,'~',0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,
   0, '\x05','\x05',0,0,0,0,0,0,0,0,0,0,
   0,0,0};


   static inline bool shiftPressed = false;
   static inline bool altGrPressed = false;

   static inline char ReadChar() {
      UINT8 raw = PS2_ReadScanCode();

      if (raw == SC_EXTENDED) {
         UINT8 ext = PS2_ReadScanCode();
         if (ext == SC_ALTGR_MAKE)  { altGrPressed = true;  return 0; }
         if (ext == SC_ALTGR_BREAK) { altGrPressed = false; return 0; }
         return 0;
      }

      if (raw == SC_LSHIFT_MAKE || raw == SC_RSHIFT_MAKE)   { shiftPressed = true;  return 0; }
      if (raw == SC_LSHIFT_BREAK || raw == SC_RSHIFT_BREAK) { shiftPressed = false; return 0; }

      bool isRelease = raw & 0x80;
      UINT8 code = raw & 0x7F;

      if (isRelease) return 0;

      if (code == 0x56) {
         return altGrPressed ? '\x05' : (shiftPressed ? '>' : '<');
      }

      if (code >= sizeof(scanCodeSet1)) return 0;

      char c = 0;
      if (altGrPressed && code < sizeof(scanCodeSet1AltGr)) c = scanCodeSet1AltGr[code];
      if (c == 0 && shiftPressed) c = scanCodeSet1Shift[code];
      if (c == 0) c = scanCodeSet1[code];

      return c;
   } 

   static inline char PollChar() {
      if (!HasScanCode()) return 0;
      return ReadChar();
   }

   static inline bool HasScanCode() {
      return inb(PS2_STATUS_PORT) & 0x01;
   }
   private:


};