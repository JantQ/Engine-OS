#pragma once

#include "../kernel.hh"
#include "../IO/io.hh"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64


#define SC_LSHIFT_MAKE  0x2A
#define SC_LSHIFT_BREAK 0xAA
#define SC_RSHIFT_MAKE  0x36
#define SC_RSHIFT_BREAK 0xB6
#define SC_EXTENDED     0xE0
#define SC_ALTGR_MAKE   0x38
#define SC_ALTGR_BREAK  0xB8
#define SC_LCTRL_MAKE   0x1D
#define SC_LCTRL_BREAK  0x9D

#define KEY_UP      0x100
#define KEY_DOWN    0x101
#define KEY_LEFT    0x102
#define KEY_RIGHT   0x103
#define KEY_HOME    0x104
#define KEY_END     0x105
#define KEY_DELETE  0x106
#define KEY_ESC     0x107
#define KEY_CTRL_S  0x108
#define KEY_CTRL_Q  0x109


class Keyboard {
   public:

   static UINT8 PS2_ReadScanCode() {
      while (!(IO::inb(PS2_STATUS_PORT) & 0x01)) { }
      return IO::inb(PS2_DATA_PORT);
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
   static inline bool ctrlPressed = false;

   static UINT16 ReadKey();

   static inline UINT16 PollKey() {
      if (!HasScanCode()) return 0;
      return ReadKey();
   }  
   
   static inline char PollChar() {
      UINT16 key = PollKey();
      return key < 0x100 ? (char)key : 0;
   }


   static inline bool HasScanCode() {
      return IO::inb(PS2_STATUS_PORT) & 0x01;
   }
   private:


};