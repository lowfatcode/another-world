/* 
  resource definitions are stored in MEMLIST.BIN

  each record is twenty bytes long and numbers are stored as
  big endian
*/

#include <string.h>
#include <algorithm>

#include "virtual-machine.hpp"

namespace another_world {

  #define REG_RANDOM_SEED 0x3c
  #define THREAD_INACTIVE 0xffff

  // helpers to fetch values stored in big endian and convert
  // them into stdint types
  uint16_t read_uint16_bigendian(const void *p) {
    const uint8_t* b = (const uint8_t*)p;
    return (b[0] << 8) | b[1];
  }

  uint32_t read_uint32_bigendian(const void* p) { 
    const uint8_t* b = (const uint8_t*)p;
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
  }

  bool (*read_file)(std::wstring filename, uint32_t offset, uint32_t length, char* buffer) = nullptr;
  void (*debug)(std::string message) = nullptr;
  void (*update_screen)(uint8_t* buffer) = nullptr;
  void (*tick_yield)(uint32_t ticks) = nullptr;

  std::vector<Resource *> resources;

  uint8_t heap[HEAP_SIZE];
  uint32_t heap_offset = 0;
  
  uint8_t vram1[320 * 200 / 2];
  uint8_t vram2[320 * 200 / 2];
  uint8_t vram3[320 * 200 / 2];

  uint8_t* vram[3] = {
    vram1,
    vram2,
    vram3
  };


  // each chapter of the game has a set of fixed resources related to it.
  // these include the vm code, video 1, video 2, and palette
  struct ChapterResource {
    uint8_t palette;
    uint8_t code;
    uint8_t background;
    uint8_t characters;
  };

  ChapterResource chapter_resources[10] = {
    {0x14, 0x15, 0x16, 0x00},
    {0x17, 0x18, 0x19, 0x00},
    {0x1a, 0x1b, 0x1c, 0x11},
    {0x1e, 0x1e, 0x1f, 0x11},
    {0x20, 0x21, 0x22, 0x11},
    {0x23, 0x24, 0x25, 0x00},
    {0x26, 0x27, 0x28, 0x11},
    {0x29, 0x2a, 0x2b, 0x11},
    {0x7d, 0x7e, 0x7f, 0x00},
    {0x7d, 0x7e, 0x7f, 0x00}
  };

  // load the resource definitions from MEMLIST.BIN
  // you must provide a pointer to a buffer than contains the
  // file contents
  void load_resource_list() {

    // TODO: move file access out of here by requiring a basic set 
    // of system calls to be provided
    uint8_t memlist[2940];
    uint8_t *p = memlist;

    read_file(L"memlist.bin", 0, 2940, (char*)memlist);

    while(p[0] != Resource::State::END_OF_MEMLIST) {
      Resource *resource = new Resource();

      // each memlist entry (resource) contains 20 bytes:
      //
      //  0     : state
      //  1     : type
      //  2 -  6: unknown (always zero)
      //  7     : bank id
      //  8 - 11: bank data start offset
      // 12 - 15: packed size
      // 16 - 19: unpacked size

      resource->state       = (Resource::State)p[0];
      resource->type        = (Resource::Type)p[1];
      resource->bank_id     = p[7];
      resource->bank_offset = read_uint32_bigendian(p + 8);
      resource->packed_size = read_uint32_bigendian(p + 12);
      resource->size        = read_uint32_bigendian(p + 16);

      resources.push_back(resource);

      p += 20;
    }
  }

  // loads all resources that are currently in the NEEDS_LOADING state
  void load_needed_resources() {
    for(auto resource : resources) {
      if(resource->state == Resource::State::NEEDS_LOADING) { 

        uint8_t *destination;
        if(resource->type == Resource::Type::IMAGE) {
          destination = vram[0];
        } else {
          destination = heap + heap_offset;
          heap_offset += resource->size;
        }

        debug("Loading resource of type " + std::to_string(resource->type) + " at offset " + std::to_string(heap_offset));
        resource->load(destination);      

        if(resource->type == Resource::Type::IMAGE) {
          /* copy image to video buffer */
          resource->state = Resource::State::NOT_NEEDED;
        } else {
          resource->state = Resource::State::LOADED;
        }
      }
    }
  }

  bool Resource::load(uint8_t *destination) {
    static std::wstring hex[16] = {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"a", L"b", L"c", L"d", L"e", L"f"};  

    // TODO: move file access out of here by requiring a basic set 
    // of system calls to be provided
    std::wstring bank_filename = L"bank0" + hex[this->bank_id];
    read_file(bank_filename, this->bank_offset, this->packed_size, (char*)destination);

    if(this->packed_size != this->size) {
      ByteKiller bk;    
      bool success = bk.unpack(destination, this->packed_size);
      if (success) {
        debug("- Unpacked successfully");
      }
      else {
        debug("- Unpacking failed");
      }

    }

    this->data = destination;
    /*
    if (this->type == Resource::Type::BYTECODE) {
      debug("DUMP SCRIPT");
      for (uint32_t i = 0; i < this->size; i++) {
        debug(std::to_string(this->data[i]));
      }
      debug("END DUMP SCRIPT");
    }*/

    return true;
  }

  void VirtualMachine::init() {
    memset(registers, 0, REGISTER_COUNT * sizeof(int16_t));
    registers[0x54] = 0x81; // TODO: erm?
    registers[REG_RANDOM_SEED] = rand();
  }

  void VirtualMachine::initialise_chapter(uint16_t id) {
    /* TODO: player->stop();
	  mixer->stopAll();*/

    // reset the heap and resource states
    heap_offset = 0;
    for(auto resource : resources) {
      resource->state = Resource::State::NOT_NEEDED;
    }

    // according to Eric Chahi's original notes the chapters are:
    //
    // 16000 = Title
    // 16001 = Intro
    // 16002 = Cave
    // 16003 = Prison
    // 16004 = Citadel?
    // 16005 = Arena
    // 16006 = ???
    // 16007 = Final
    // 16008 = ???
    // 16009 = ???
    
    chapter_id = id - 16000;

    registers[0xE4] = 0x14; // TODO: erm?
  
    palette = resources[chapter_resources[chapter_id].palette];
    code = resources[chapter_resources[chapter_id].code];
    background = resources[chapter_resources[chapter_id].background];  

    // load the chapter resources
    palette->state = Resource::State::NEEDS_LOADING;
    code->state = Resource::State::NEEDS_LOADING;
    background->state = Resource::State::NEEDS_LOADING;

    if(chapter_resources[chapter_id].characters) {
      characters = resources[chapter_resources[chapter_id].characters];
      characters->state = Resource::State::NEEDS_LOADING;
    }

    load_needed_resources();

    // set all thread program counters to 0xffff (inactive)
    memset(program_counter, 0xff, sizeof(program_counter));

    // reset program counter for first thread
    program_counter[0] = 0; 
  }

  uint8_t VirtualMachine::fetch_byte(uint8_t *b, uint16_t* c) {
    uint8_t v = b[*c];
    (*c)++;
    return v;
  }

  uint8_t VirtualMachine::fetch_byte(uint16_t *pc) {
    uint8_t v = code->data[*pc];
    (*pc)++;
    return v;
  }

  uint16_t VirtualMachine::fetch_word(uint16_t *pc) {
    uint16_t v = read_uint16_bigendian(&code->data[*pc]);
    (*pc)++;
    (*pc)++;
    return v;
  }

  

  /**
 * Draw a polygon from a std::vector<point> list of points.
 *
 * \param[in] points `std::vector<point>` of points describing the polygon.
 */
  void polygon(uint8_t *target, uint8_t color, Point *points, uint8_t point_count) {
    static int32_t nodes[256]; // maximum allowed number of nodes per scanline for polygon rendering

    Rect clip = { 0, 0, 319, 119 };
    int16_t miny = points[0].y, maxy = points[0].y;

    color &= 0x0f;
    color = color | (color << 4);

    for (uint16_t i = 1; i < point_count; i++) {
      miny = std::min(miny, points[i].y);
      maxy = std::max(maxy, points[i].y);
    }

    // for each scanline within the polygon bounds (clipped to clip rect)
    Point p;

    for (p.y = std::max(clip.y, miny); p.y <= std::min(int16_t(clip.y + clip.h), maxy); p.y++) {
      uint8_t n = 0;
      for (uint16_t i = 0; i < point_count; i++) {
        uint16_t j = (i + 1) % point_count;
        int32_t sy = points[i].y;
        int32_t ey = points[j].y;
        int32_t fy = p.y;
        if ((sy < fy && ey >= fy) || (ey < fy && sy >= fy)) {
          int32_t sx = points[i].x;
          int32_t ex = points[j].x;
          int32_t px = int32_t(sx + float(fy - sy) / float(ey - sy) * float(ex - sx));

          nodes[n++] = px < clip.x ? clip.x : (px >= clip.x + clip.w ? clip.x + clip.w - 1 : px);
        }
      }

      uint16_t i = 0;
      while (i < n - 1) {
        if (nodes[i] > nodes[i + 1]) {
          int32_t s = nodes[i]; nodes[i] = nodes[i + 1]; nodes[i + 1] = s;
          if (i) i--;
        }
        else {
          i++;
        }
      }

      for (uint16_t i = 0; i < n; i += 2) {
        // TODO: make this faster
        for (uint16_t x = nodes[i]; x < nodes[i + 1] - nodes[i]; x++) {
          if (x & 0b1) {
            target[(p.y * 160) + (x / 2)] &= 0xf0;
            target[(p.y * 160) + (x / 2)] |= color & 0x0f;
          } else {
            target[(p.y * 160) + (x / 2)] &= 0x0f;
            target[(p.y * 160) + (x / 2)] |= color & 0xf0;
          }          
        }
      }      
    }
  }

  void VirtualMachine::draw_polygon(uint8_t color, Point p, uint8_t *buffer, uint16_t offset) {
    static Point points[64];

    uint16_t width = fetch_byte(buffer, &offset);
    uint16_t height = fetch_byte(buffer, &offset);
    uint8_t point_count = fetch_byte(buffer, &offset);
    debug("  - " + std::to_string(point_count) + " points");

    for (uint8_t i = 0; i < point_count; i++) {
      points[i].x = fetch_byte(buffer, &offset) + p.x - (width / 2);
      points[i].y = fetch_byte(buffer, &offset) + p.y - (height / 2);
    }

    polygon(vram[1], color, points, point_count);
  }

  void VirtualMachine::tick() {
    debug("Start of thread execution");
    // TODO: switch part if needed (can't this be done in the op code processing?)
  
        //  //Check if a part switch has been requested.
        // 	if (res->requestedNextPart != 0) {
        // 		initForPart(res->requestedNextPart);
        // 		res->requestedNextPart = 0;
        // 	}


    // TODO: perform a jump operation if needed (can't this be done in the op code processing?)
        // for (int threadId = 0; threadId < VM_NUM_THREADS; threadId++) {

        // 	vmIsChannelActive[CURR_STATE][threadId] = vmIsChannelActive[REQUESTED_STATE][threadId];

        // 	uint16_t n = threadsData[REQUESTED_PC_OFFSET][threadId];

        // 	if (n != VM_NO_SETVEC_REQUESTED) {

        // 		threadsData[PC_OFFSET][threadId] = (n == 0xFFFE) ? VM_INACTIVE_THREAD : n;
        // 		threadsData[REQUESTED_PC_OFFSET][threadId] = VM_NO_SETVEC_REQUESTED;
        // 	}
        // }

    // TODO: handle input and player update
	    //	vm.inp_updatePlayer();
	    //	processInput();

    std::string opcode_names[29] = {
      "movl", "movr", "addr", "addl", 
      "call", "ret", "pause thread", "jmp", 
      "create thread", "jnz", "cjmp", "set palette", 
      "reset thread", "select vram", "fill vram", "copy vram", 
      "vram to screen", "kill thread", "draw text", "subr", 
      "andl", "orl", "shll", "shrl", 
      "play sound", "load resource", "play music"
    };

    uint32_t ticks = 0;

	  //	vm.hostFrame();
    for(uint8_t i = 0; i < THREAD_COUNT; i++) {
      if(program_counter[i] != THREAD_INACTIVE) {
        // execute thread
        debug("Switch to thread " + std::to_string(i));

        uint16_t *pc = &program_counter[i];

        bool next_thread = false;
        while(!next_thread) {
          ticks++;

          

          uint8_t opcode = fetch_byte(pc);

          std::string opcode_name = "invalid";
          if (opcode <= 0x1a) {
            opcode_name = opcode_names[opcode];
          } else if (opcode < 0x40) {
            // invalid
          } else if (opcode < 0x80) {
            opcode_name = "draw polygon sprite";
          } else {
            opcode_name = "draw polygon background";
          }


          debug(": " + opcode_name + " (" + std::to_string(opcode) + ") [" + std::to_string(i) + "]");

          if(opcode & 0x80) {
            // calculate offset into polygon data to start drawing from
            // the remaining bits of the opcode (hence masking with 0x7f) 
            // contain the high byte off the offset, and the next byte 
            // contains the low byte.
            uint16_t offset = (((opcode & 0x7f) << 8) | fetch_byte(pc)) * 2;    // contains offset for polygon data in cinematic data resource    
            
            // absolute position of shape (added to relative positions later)
            Point pos = { fetch_byte(pc), fetch_byte(pc) };

            // slightly weird one this. if the y value is greater than 199
            // then the extra is added onto the x value. i assume this is because
            // the screen resolution is 320 pixels but a byte can only hold 
            // numbers up to 255. this "hack" allows bigger numbers (up to 311) to 
            // be represented in the x byte (at the cost that it can only happen
            // when y is greater than 199 (so is effectively clamped to the 
            // bottom of the screen).
            if (pos.y > 199) {
              pos.x += pos.y - 199;
              pos.y = 199;
            }
            
            uint8_t polygon_header = fetch_byte(background->data, &offset);
            
            if (polygon_header & 0xc0) {
              // draw a single polygon, the colour is encoded into the header
              uint8_t color = polygon_header & 0x3f;
              draw_polygon(color, pos, background->data, offset);
            } else {
              //draw_polygon_list();
            }

            // 
            //vram[0][x / 2 + y * 160] = 0xff;
            // readAndDrawPolygon using data in cinematic resource
            tick_yield(ticks);

            continue;
          }

          if(opcode & 0x40) {
            debug("- Not implemented! Polygons");
            // special polygon opcodes
            uint16_t off = ((opcode << 8) | fetch_byte(pc)) * 2;    // contains offset for polygon data in cinematic      
            fetch_byte(pc);

            if (!(opcode & 0x20)) 
            {
              if (!(opcode & 0x10))  // 0001 0000 is set
              {
                fetch_byte(pc);
                //x = (x << 8) | _scriptPtr.fetchByte();
              } else {
                //x = vmVariables[x];
              }
            } 
            else 
            {
              if (opcode & 0x10) { // 0001 0000 is set
                //x += 0x100;
              }
            }

            fetch_byte(pc);

            if (!(opcode & 8))  // 0000 1000 is set
            {
              if (!(opcode & 4)) { // 0000 0100 is set
              fetch_byte(pc);
                //y = (y << 8) | _scriptPtr.fetchByte();
              } else {
                //y = vmVariables[y];
              }
            }

            fetch_byte(pc);

            if (!(opcode & 2))  // 0000 0010 is set
            {
              if (!(opcode & 1)) // 0000 0001 is set
              {
                (*pc)--; // move the program counter backwards 1?!?!! // TODO: WHY?!
                //--_scriptPtr.pc;
                //zoom = 0x40;
              } 
              else 
              {
                //zoom = vmVariables[zoom];
              }
            } 
            else 
            {
            
              if (opcode & 1) { // 0000 0001 is set
                //res->_useSegVideo2 = true;
                (*pc)--; // move the program counter backwards 1?!?!! // TODO: WHY?!
                //--_scriptPtr.pc;
                //zoom = 0x40;
              }
            }      

            continue;
          }

          switch(opcode) {
            case 0x00: {
              // move literal into register
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] = v;          
              break;
            }

            case 0x01: {
              // copy register into register
              uint8_t d = fetch_byte(pc);
              uint8_t s = fetch_byte(pc);
              registers[d] = registers[s];
              break;
            }

            case 0x02: {
              // add register to register
              uint8_t d = fetch_byte(pc);
              uint8_t s = fetch_byte(pc);
              registers[d] += registers[s];
              break;
            }

            case 0x03: {
              // add literal to register
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] += v;          
              break;
            }

            case 0x04: {
              debug("Call new location - stack size " + std::to_string(call_stack.size()));

              // save the current byte code position on the call stack
              // and jump to the new specific position
              int16_t o = fetch_word(pc);
              call_stack.push_back(*pc);
              *pc = o;

              break;
            }

            case 0x05: {
              debug("Return previous location - stack size " + std::to_string(call_stack.size()));

              // return to the last bytecode position on the call stack
              int16_t o = call_stack.back();
              call_stack.pop_back();
              *pc = o;

              break;
            }

            case 0x06: {
              // pause thread, jumps to next iteration of thread loop
              
              // TODO: ? do we want to do this to ensure next time this thread runs 
              // it hits the pause instruction again? if we don't then the program
              // counter will have passed the pause instruction and the thread will
              // execute the next opcode next time round. suspect we do...
              //(*pc)--; 

              next_thread = true;
              break;
            }

            case 0x07: {
              // jump to new position in byte code
              int16_t o = fetch_word(pc);
              *pc = o;
              break;
            }

            case 0x08: {
              // create thread - sets up another thread to run
              uint8_t thread_id = fetch_byte(pc);
              int16_t start_pc = fetch_word(pc);

              debug("Create thread " + std::to_string(thread_id) + " starting at " + std::to_string(start_pc));
              program_counter[thread_id] = start_pc;

              break;
            }

            case 0x09: {
              // decrement register and jump if not zero
              uint8_t r = fetch_byte(pc);
              int16_t o = fetch_word(pc);

              registers[r]--;

              if(registers[r] != 0) {
                *pc = o;
              }

              break;
            }

            case 0x0a: {
              // conditional jump based on whether expression (determined by `t`)
              // is true or false
              uint8_t t = fetch_byte(pc);          
              int16_t a = registers[fetch_byte(pc)];
              int16_t b;
              uint8_t c = fetch_byte(pc);
              int16_t o = fetch_word(pc);

              if(t & 0x80) {
                // register to register comparison
                b = registers[c];         
              } else if (t & 0x40) {
                // register to 16-bit literal comparison
                b = (c << 8) | fetch_byte(pc);   
              }else{
                // register to 8-bit literal comparison
                b = c;
              }

              bool result = false;
            
              // mask out just the expression bits
              t &= 0b111;
              if(t == 0) { result = a == b; }
              if(t == 1) { result = a != b; }
              if(t == 2) { result = a  > b; }
              if(t == 3) { result = a >= b; }
              if(t == 4) { result = a  < b; }
              if(t == 5) { result = a <= b; }

              if(result) {
                *pc = o;
              }           

              break;
            }

            case 0x0b: {
              // perform a palette change
              uint8_t palette_id = fetch_byte(pc);

              // TODO: from Eric Chahi's original notes the second byte of
              // this instruction appears to be a "speed" for the
              // palette change - but then parts of the notes are
              // crossed out suggesting it was never implemented?
              uint8_t speed = fetch_byte(pc);

              // TODO: set_palette(palette_id);

              break;
            }

            case 0x0c: {
              debug("- Not implemented! Reset thread");
              // op_resetThread
              uint8_t thread_id = fetch_byte(pc);
              uint8_t i = fetch_byte(pc);
              uint8_t a = fetch_byte(pc);
              break;
            }

 // _curPagePtr1 is the backbuffer 
 // _curPagePtr2 is the frontbuffer
 // _curPagePtr3 is the background builder.

            case 0x0d: {
              // select working video buffer             
              uint8_t id = fetch_byte(pc);
              active_vram = vram[id];
              break;
            }

            case 0x0e: {
              // clears an entire video buffer with the specified palette colour              
              uint8_t id = fetch_byte(pc);

              if (id >= 3) {
                id = 0;
              }

              uint8_t color = fetch_byte(pc);
              color |= color << 4;
              memset(vram[id], color, 320 * 200 / 2);

              break;
            }

            case 0x0f: {
              // copy one video buffer into another
              uint8_t src_id = fetch_byte(pc);
              uint8_t dest_id = fetch_byte(pc);

              if (src_id < 3 && dest_id < 3) {  // TODO: this is wrong
                memcpy(vram[dest_id], vram[src_id], 320 * 200 / 2);
              }

              // TODO: this should support vertical scrolling by looking the
              // value in register VM_VARIABLE_SCROLL_Y
              // e.g. video->copyPage(srcPageId, dstPageId, vmVariables[VM_VARIABLE_SCROLL_Y]);

              break;
            }

            case 0x10: {
              // copy video buffer into display backbuffer
              uint8_t id = fetch_byte(pc);

              // from Eric Chahi's notes:
              // "si n == 255 on flip invisi et visi"

              if (id == 0xff) {
                uint8_t* t = vram[0];
                vram[0] = vram[1];
                vram[1] = t;
              } else {
                update_screen(vram[id]);
              }

              break;
            }

            case 0x11: {
              // kill thread by marking it inactive and moving to the next thread
              *pc = 0xffff;
              next_thread = true;
              break;
            }

            case 0x12: {
              debug("- Not implemented! Draw string");
              // op_drawString
              uint16_t string_id = fetch_word(pc);
              uint8_t x = fetch_byte(pc);
              uint8_t y = fetch_byte(pc);
              uint8_t colour = fetch_byte(pc);

              if (string_id < string_table.size()) {
                const std::string& string_entry = string_table.at(string_id);
                debug("- " + string_entry);
              }
              else {
                debug("! tried to draw invalid string " + std::to_string(string_id));
              }
              
              

              // TODO: make this work?

              break;
            }

            case 0x13: {          
              // subtract register from register
              uint8_t d = fetch_byte(pc);
              uint8_t s = fetch_byte(pc);
              registers[d] -= registers[s];           
              break;
            }

            case 0x14: {
              // boolean and register with literal
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] = (uint16_t)registers[r] & v;
              break;
            }

            case 0x15: {
              // boolean or register with literal
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] = (uint16_t)registers[r] | v;
              break;
            }

            case 0x16: {
              // left shift register by value
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] = (uint16_t)registers[r] << v;
              break;
            }

            case 0x17: {
              // right shift register by value
              uint8_t r = fetch_byte(pc);
              int16_t v = fetch_word(pc);
              registers[r] = (uint16_t)registers[r] >> v;
              break;
            }

            case 0x18: {
              debug("- Not implemented! Play sound");
              // op_playSound
              fetch_word(pc);
              fetch_byte(pc);
              fetch_byte(pc);
              fetch_byte(pc);
              break;
            }

            case 0x19: {
              // load resources or switch game part
              uint16_t i = fetch_word(pc);

              if (i == 0) {
                // TODO: quit?? 
              } else {
                if (i <= resources.size()) {
                  resources[i]->state = Resource::State::NEEDS_LOADING;
                  load_needed_resources();
                }
                else {
                  debug("Switch to chapter " + std::to_string(i));
                  initialise_chapter(i);
                }
              }
              
              break;
            }

            case 0x1a: {
              debug("- Not implemented! Play music");
              // op_playMusic
              fetch_word(pc);
              fetch_word(pc);
              fetch_byte(pc);
              break;
            }

            default: {
              debug("- Invalid opcode " + std::to_string(opcode) + " on thread " + std::to_string(i));
              break;
            }
          }
        }
      }
    }

    debug("End of thread execution");
  }
}