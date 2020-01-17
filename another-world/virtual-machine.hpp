#pragma once

#include <vector>
#include <map>
#include <string>
#include <stdint.h>
#include <stdarg.h>


#include "byte-killer.hpp"

namespace another_world {

  #define HEAP_SIZE 200 * 1024
  #define REGISTER_COUNT 256
  #define THREAD_COUNT 64

  extern uint16_t read_uint16_bigendian(const void* p);
  extern uint32_t read_uint32_bigendian(const void* p);

  extern bool (*read_file)(std::wstring filename, uint32_t offset, uint32_t length, char* buffer);
  extern void (*debug)(const char *fmt, ...);
	extern void (*debug_log)(const char *fmt, ...);
  extern void (*update_screen)(uint8_t *buffer);
	extern void (*set_palette)(uint16_t* palette);
	extern void (*debug_yield)();

  extern uint8_t heap[HEAP_SIZE];

	// three buffers for 320 x 200 (4 bits per pixel)
	extern uint8_t vram1[320 * 200 / 2];
	extern uint8_t vram2[320 * 200 / 2];
	extern uint8_t vram3[320 * 200 / 2];

	extern uint8_t* vram[3];

	struct Point {
		int16_t x;
		int16_t y;
	};

	struct Rect {
		int16_t x;
		int16_t y;
		int16_t w;
		int16_t h;
	};

  struct Resource {
    enum State { NOT_NEEDED = 0, LOADED = 1, NEEDS_LOADING = 2, END_OF_MEMLIST = 0xff };
    enum Type { SOUND = 0, MUSIC = 1, IMAGE = 2, PALETTE = 3, BYTECODE = 4, POLYGON = 5, BANK = 6 };

    State     state;
    Type      type;
    uint8_t   bank_id;
    uint32_t  bank_offset;
    uint16_t  packed_size;
    uint16_t  size;
    uint8_t  *data;

    bool load(uint8_t* destination);
  };
  
  extern std::vector<Resource*> resources;

  struct VirtualMachine {
    int16_t   registers[REGISTER_COUNT];
    uint16_t  program_counter[THREAD_COUNT];
    uint8_t   chapter_id;
    std::vector<uint16_t> call_stack;

    Resource *palette;
    Resource *code;
    Resource *background;
    Resource *characters;

    uint8_t *working_vram = vram[0];
		uint8_t *visible_vram = vram[0];

    void init();
    void initialise_chapter(uint16_t id);
    void execute_threads();

    uint8_t fetch_byte(uint16_t* pc);
		uint8_t fetch_byte(uint8_t* b, uint32_t* c);
    uint16_t fetch_word(uint16_t* pc);
		uint16_t fetch_word(uint8_t* b, uint32_t* c);

		uint8_t* get_vram_from_id(uint8_t id);
		void draw_shape(uint8_t color, Point pos, uint8_t zoom, uint8_t* buffer, uint32_t *offset);
		void draw_polygon(uint8_t color, Point pos, uint8_t zoom, uint8_t* buffer, uint32_t *offset);
		void draw_polygon_group(uint8_t color, Point pos, uint8_t zoom, uint8_t* buffer, uint32_t *offset);
		void polygon(uint8_t* target, uint8_t color, Point* points, uint8_t point_count);
		void point(uint8_t* target, uint8_t color, Point* point);

  };

  void load_resource_list();
  void load_needed_resources();

	const std::string opcode_names[29] = {
		"movi",   // 0x00   movi  d0, #1234
		"mov",    // 0x01   mov   d0, d1
		"add",    // 0x02   add   d0, d1
		"addi",   // 0x03   addi  d0, #1234
		"call",   // 0x04   call  #1234
		"ret",    // 0x05   ret
		"brk",    // 0x06   brk
		"jmp",    // 0x07   jmp   #1234
		"svec",   // 0x08   svec  #12, #1234
		"djnz",   // 0x09   djnz  d0, #1234
		"cjmp",   // 0x0a   cjmp  #12, d0, d1 or #1234, #1234
		"pal",    // 0x0b   pal   #12, #12
		"???",    // 0x0c   ???   #12, #12, #12
		"setws",  // 0x0d   setws #12
		"vclr",   // 0x0e   vclr  #12, #12
		"vcpy",   // 0x0f   vcpy  #12, #12
		"vshw",   // 0x10   vshw  #12
		"kill",   // 0x11   kill
		"text",   // 0x12   text  #1234, #12, #12, #12
		"sub",    // 0x13   sub   d0, d1
		"andi",   // 0x14   andi  d0, #1234
		"ori",    // 0x15   ori   d0, #1234
		"shli",   // 0x16   shli  d0, #1234 
		"shri",   // 0x17   shri  d0, #1234 
		"snd",    // 0x18   snd   #1234, #12, #12, #12
		"load",   // 0x19   load  #1234
		"music"   // 0x1a   music #1234, #1234, #12
	};

	const std::map<uint16_t, std::string> string_table = {
	{ 0x001, "P E A N U T  3000" },
	{ 0x002, "Copyright  } 1990 Peanut Computer, Inc.\nAll rights reserved.\n\nCDOS Version 5.01" },
	{ 0x003, "2" },
	{ 0x004, "3" },
	{ 0x005, "." },
	{ 0x006, "A" },
	{ 0x007, "@" },
	{ 0x008, "PEANUT 3000" },
	{ 0x00A, "R" },
	{ 0x00B, "U" },
	{ 0x00C, "N" },
	{ 0x00D, "P" },
	{ 0x00E, "R" },
	{ 0x00F, "O" },
	{ 0x010, "J" },
	{ 0x011, "E" },
	{ 0x012, "C" },
	{ 0x013, "T" },
	{ 0x014, "Shield 9A.5f Ok" },
	{ 0x015, "Flux % 5.0177 Ok" },
	{ 0x016, "CDI Vector ok" },
	{ 0x017, " %%%ddd ok" },
	{ 0x018, "Race-Track ok" },
	{ 0x019, "SYNCHROTRON" },
	{ 0x01A, "E: 23%\ng: .005\n\nRK: 77.2L\n\nopt: g+\n\n Shield:\n1: OFF\n2: ON\n3: ON\n\nP~: 1\n" },
	{ 0x01B, "ON" },
	{ 0x01C, "-" },
	{ 0x021, "|" },
	{ 0x022, "--- Theoretical study ---" },
	{ 0x023, " THE EXPERIMENT WILL BEGIN IN    SECONDS" },
	{ 0x024, "  20" },
	{ 0x025, "  19" },
	{ 0x026, "  18" },
	{ 0x027, "  4" },
	{ 0x028, "  3" },
	{ 0x029, "  2" },
	{ 0x02A, "  1" },
	{ 0x02B, "  0" },
	{ 0x02C, "L E T ' S   G O" },
	{ 0x031, "- Phase 0:\nINJECTION of particles\ninto synchrotron" },
	{ 0x032, "- Phase 1:\nParticle ACCELERATION." },
	{ 0x033, "- Phase 2:\nEJECTION of particles\non the shield." },
	{ 0x034, "A  N  A  L  Y  S  I  S" },
	{ 0x035, "- RESULT:\nProbability of creating:\n ANTIMATTER: 91.V %\n NEUTRINO 27:  0.04 %\n NEUTRINO 424: 18 %\n" },
	{ 0x036, "   Practical verification Y/N ?" },
	{ 0x037, "SURE ?" },
	{ 0x038, "MODIFICATION OF PARAMETERS\nRELATING TO PARTICLE\nACCELERATOR (SYNCHROTRON)." },
	{ 0x039, "       RUN EXPERIMENT ?" },
	{ 0x03C, "t---t" },
	{ 0x03D, "000 ~" },
	{ 0x03E, ".20x14dd" },
	{ 0x03F, "gj5r5r" },
	{ 0x040, "tilgor 25%" },
	{ 0x041, "12% 33% checked" },
	{ 0x042, "D=4.2158005584" },
	{ 0x043, "d=10.00001" },
	{ 0x044, "+" },
	{ 0x045, "*" },
	{ 0x046, "% 304" },
	{ 0x047, "gurgle 21" },
	{ 0x048, "{{{{" },
	{ 0x049, "Delphine Software" },
	{ 0x04A, "By Eric Chahi" },
	{ 0x04B, "  5" },
	{ 0x04C, "  17" },
	{ 0x12C, "0" },
	{ 0x12D, "1" },
	{ 0x12E, "2" },
	{ 0x12F, "3" },
	{ 0x130, "4" },
	{ 0x131, "5" },
	{ 0x132, "6" },
	{ 0x133, "7" },
	{ 0x134, "8" },
	{ 0x135, "9" },
	{ 0x136, "A" },
	{ 0x137, "B" },
	{ 0x138, "C" },
	{ 0x139, "D" },
	{ 0x13A, "E" },
	{ 0x13B, "F" },
	{ 0x13C, "        ACCESS CODE:" },
	{ 0x13D, "PRESS BUTTON OR RETURN TO CONTINUE" },
	{ 0x13E, "   ENTER ACCESS CODE" },
	{ 0x13F, "   INVALID PASSWORD !" },
	{ 0x140, "ANNULER" },
	{ 0x141, "      INSERT DISK ?\n\n\n\n\n\n\n\n\nPRESS ANY KEY TO CONTINUE" },
	{ 0x142, " SELECT SYMBOLS CORRESPONDING TO\n THE POSITION\n ON THE CODE WHEEL" },
	{ 0x143, "    LOADING..." },
	{ 0x144, "              ERROR" },
	{ 0x15E, "LDKD" },
	{ 0x15F, "HTDC" },
	{ 0x160, "CLLD" },
	{ 0x161, "FXLC" },
	{ 0x162, "KRFK" },
	{ 0x163, "XDDJ" },
	{ 0x164, "LBKG" },
	{ 0x165, "KLFB" },
	{ 0x166, "TTCT" },
	{ 0x167, "DDRX" },
	{ 0x168, "TBHK" },
	{ 0x169, "BRTD" },
	{ 0x16A, "CKJL" },
	{ 0x16B, "LFCK" },
	{ 0x16C, "BFLX" },
	{ 0x16D, "XJRT" },
	{ 0x16E, "HRTB" },
	{ 0x16F, "HBHK" },
	{ 0x170, "JCGB" },
	{ 0x171, "HHFL" },
	{ 0x172, "TFBB" },
	{ 0x173, "TXHF" },
	{ 0x174, "JHJL" },
	{ 0x181, " BY" },
	{ 0x182, "ERIC CHAHI" },
	{ 0x183, "         MUSIC AND SOUND EFFECTS" },
	{ 0x184, " " },
	{ 0x185, "JEAN-FRANCOIS FREITAS" },
	{ 0x186, "IBM PC VERSION" },
	{ 0x187, "      BY" },
	{ 0x188, " DANIEL MORAIS" },
	{ 0x18B, "       THEN PRESS FIRE" },
	{ 0x18C, " PUT THE PADDLE ON THE UPPER LEFT CORNER" },
	{ 0x18D, "PUT THE PADDLE IN CENTRAL POSITION" },
	{ 0x18E, "PUT THE PADDLE ON THE LOWER RIGHT CORNER" },
	{ 0x258, "      Designed by ..... Eric Chahi" },
	{ 0x259, "    Programmed by...... Eric Chahi" },
	{ 0x25A, "      Artwork ......... Eric Chahi" },
	{ 0x25B, "Music by ........ Jean-francois Freitas" },
	{ 0x25C, "            Sound effects" },
	{ 0x25D, "        Jean-Francois Freitas\n             Eric Chahi" },
	{ 0x263, "              Thanks To" },
	{ 0x264, "           Jesus Martinez\n\n          Daniel Morais\n\n        Frederic Savoir\n\n      Cecile Chahi\n\n    Philippe Delamarre\n\n  Philippe Ulrich\n\nSebastien Berthet\n\nPierre Gousseau" },
	{ 0x265, "Now Go Out Of This World" },
	{ 0x190, "Good evening professor." },
	{ 0x191, "I see you have driven here in your\nFerrari." },
	{ 0x192, "IDENTIFICATION" },
	{ 0x193, "Monsieur est en parfaite sante." },
	{ 0x194, "Y\n" },
	{ 0x193, "AU BOULOT !!!\n" }
	};
}