#include "cpu32.h"

#include <Tempest/Log>
#include <unordered_set>
#include <cstring>

#include "directmemory.h"

using namespace Tempest;
using namespace Compatibility;

Cpu32::Cpu32(DirectMemory* ikarus, Mem32& mem32):ikarus(ikarus), mem32(mem32){
  stack.reserve(1024);
  }

void Cpu32::exec(const ptr32_t basePc, const uint8_t* ins, size_t len) {
  std::vector<uint8_t> dbg{ins, ins+len};

  ptr32_t pc = 0;
  while(pc<len) {
    if(exec1Byte(basePc, pc, ins, len))
      pc += 1;
    else if(exec2Byte(basePc, pc, ins, len))
      pc += 2;
    else if(exec3Byte(basePc, pc, ins, len))
      pc += 3;
    else {
      auto op = readUInt32(ins, pc, len);
      Log::e("ASMINT: unknown instruction: ", int32_t(op));
      return;
      }
    }
  stack.clear();
  }

bool Cpu32::exec1Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* ins, size_t len) {
  enum Op1:uint8_t {
    ASMINT_OP_movImToEAX   = 184,  //0xB8
    ASMINT_OP_movImToECX   = 185,  //0xB9
    ASMINT_OP_movImToEDX   = 186,  //0xBA
    ASMINT_OP_pushIm       = 104,  //0x68
    ASMINT_OP_call         = 232,  //0xE8
    ASMINT_OP_retn         = 195,  //0xC3
    ASMINT_OP_nop          = 144,  //0x90
    ASMINT_OP_jmp          = 233,  //0xE9
    ASMINT_OP_cld          = 252,  //0xFC
    ASMINT_OP_repz         = 243,  //0xF3
    ASMINT_OP_cmpsb        = 166,  //0xA6
    ASMINT_OP_pushEAX      =  80,  //0x50
    ASMINT_OP_pushECX      =  81,  //0x51
    ASMINT_OP_popEAX       =  88,  //0x58
    ASMINT_OP_popECX       =  89,  //0x59
    ASMINT_OP_pusha        =  96,  //0x60
    ASMINT_OP_popa         =  97,  //0x61
    ASMINT_OP_movMemToEAX  = 161,  //0xA1
    ASMINT_OP_movALToMem   = 162,  //0xA2
    };

  const Op1 op = Op1(readUInt8(ins, pc, len));
  switch(op) {
    case ASMINT_OP_pushIm: {
      auto imm = readUInt32(ins, pc+1, len);
      stack.push_back(imm);
      pc += 4;
      return true;
      }
    case ASMINT_OP_movImToEAX:{
      eax = readUInt32(ins, pc+1, len);
      pc += 4;
      return true;
      }
    case ASMINT_OP_movImToECX: {
      ecx = readUInt32(ins, pc+1, len);
      pc += 4;
      return true;
      }
    case ASMINT_OP_movImToEDX: {
      edx = readUInt32(ins, pc+1, len);
      pc += 4;
      return true;
      }
    case ASMINT_OP_call: {
      ptr32_t calle = readUInt32(ins, pc+1, len);
      calle = basePc+pc+calle+5;
      pc += 4;
      callFunction(calle);
      return true;
      }
    case ASMINT_OP_retn: {
      //NOTE: 0xC3 is regular return
      pc = ptr32_t(len);
      return true;
      }
    case ASMINT_OP_nop:
      break;
    case ASMINT_OP_movMemToEAX: {
      ptr32_t ptr = readUInt32(ins, pc+1, len);
      pc += 4;
      eax = uint32_t(mem32.readInt(ptr));
      return true;
      }
    case ASMINT_OP_pushEAX: {
      stack.push_back(eax);
      return true;
      }
    case ASMINT_OP_jmp:
    case ASMINT_OP_cld:
    case ASMINT_OP_repz:
    case ASMINT_OP_cmpsb:
    case ASMINT_OP_pushECX:
    case ASMINT_OP_popEAX:
    case ASMINT_OP_popECX:
    case ASMINT_OP_pusha:
    case ASMINT_OP_popa:
    case ASMINT_OP_movALToMem:
      Log::e("ASMINT: unimplemented instruction: ", int32_t(op));
      return true;
    }
  return false;
  }

bool Cpu32::exec2Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* ins, size_t len) {
  enum Op2:uint16_t {
    ASMINT_OP_movEAXToMem     =  1417, //0x0589
    ASMINT_OP_movEAXToAL      =   138, //0x008A
    ASMINT_OP_movCLToEAX      =  2184, //0x0888
    ASMINT_OP_floatStoreToMem =  7641, //0x1DD9
    ASMINT_OP_addImToESP      = 50307, //0xC483
    ASMINT_OP_movMemToECX     =  3467, //0x0D8B
    ASMINT_OP_movMemToCL      =  3466, //0x0D8A
    ASMINT_OP_movMemToEDX     =  5515, //0x158B
    ASMINT_OP_movMemToEDI     = 15755, //0x3D8B
    ASMINT_OP_movMemToESI     = 13707, //0x358B
    ASMINT_OP_movECXtoEAX     = 49547, //0xC18B
    ASMINT_OP_movESPtoEAX     = 50315, //0xC48B
    ASMINT_OP_movEAXtoECX     = 49545, //0xC189
    ASMINT_OP_movEBXtoEAX     = 55433, //0xD889
    ASMINT_OP_movEBPtoEAX     = 50571, //0xC58B
    ASMINT_OP_movEDItoEAX     = 51083, //0xC78B
    ASMINT_OP_addImToEAX      = 49283, //0xC083
    };

  const Op2 op = Op2(readUInt16(ins, pc, len));
  switch(op) {
    case ASMINT_OP_movEAXToMem: {
      ptr32_t mem = readUInt32(ins, pc+2, len);
      pc += 4;
      mem32.writeInt(mem, int32_t(eax));
      return true;
      }
    case ASMINT_OP_addImToESP: {
      //TODO: propper stack maintanance
      uint8_t imm = readUInt8(ins, pc+2, len);
      pc  += 1;
      esp += imm;
      return true;
      }
    case ASMINT_OP_movMemToECX: {
      ptr32_t ptr = readUInt32(ins, pc+2, len);
      pc += 4;
      ecx = uint32_t(mem32.readInt(ptr));
      return true;
      }
    case ASMINT_OP_movEAXToAL:
    case ASMINT_OP_movCLToEAX:
    case ASMINT_OP_floatStoreToMem:
    case ASMINT_OP_movMemToCL:
    case ASMINT_OP_movMemToEDX:
    case ASMINT_OP_movMemToEDI:
    case ASMINT_OP_movMemToESI:
    case ASMINT_OP_movECXtoEAX:
    case ASMINT_OP_movESPtoEAX:
    case ASMINT_OP_movEAXtoECX:
    case ASMINT_OP_movEBXtoEAX:
    case ASMINT_OP_movEBPtoEAX:
    case ASMINT_OP_movEDItoEAX:
    case ASMINT_OP_addImToEAX:
      Log::e("ASMINT: unimplemented instruction: ", int32_t(op));
      return true;
    }
  return false;
  }

bool Cpu32::exec3Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* ins, size_t len) {
  enum Op3:uint32_t {
    ASMINT_OP_setzMem = 365583, //0x05940F
    };
  const Op3 op = Op3(readUInt24(ins, pc, len));
  switch(op) {
    case ASMINT_OP_setzMem:
      Log::e("ASMINT: unimplemented instruction: ", int32_t(op));
      return true;
    }
  return false;
  }

uint8_t Cpu32::readUInt8(const uint8_t* code, size_t offset, size_t len) {
  if(offset>=len)
    return 0;
  uint8_t ret = 0;
  std::memcpy(&ret, code+offset, sizeof(uint8_t));
  return ret;
  }

uint16_t Cpu32::readUInt16(const uint8_t* code, size_t offset, size_t len) {
  if(offset+2>len)
    return 0;
  uint16_t ret = 0;
  std::memcpy(&ret, code+offset, sizeof(uint16_t));
  return ret;
  }

uint32_t Cpu32::readUInt24(const uint8_t* code, size_t offset, size_t len) {
  if(offset+3>len)
    return 0;
  uint32_t ret = 0;
  std::memcpy(&ret, code+offset, 3);
  return ret;
  }

uint32_t Cpu32::readUInt32(const uint8_t* code, size_t offset, size_t len) {
  if(offset+4>len)
    return 0;
  uint32_t ret = 0;
  std::memcpy(&ret, code+offset, sizeof(uint32_t));
  return ret;
  }

void Cpu32::register_stdcall_inner(ptr32_t addr, std::function<void (Cpu32&)> f) {
  stdcall_Overrides[addr] = std::move(f);
  }

void Cpu32::callFunction(ptr32_t func) {
  if(func==0x1) {
    //TODO: better representation of *.dll in memory
    Log::d("ASMINT_OP_call: won't invoke function obtained by GetProcAddress ");
    return;
    }

  auto fn = stdcall_Overrides.find(func);
  if(fn!=stdcall_Overrides.end()) {
    fn->second(*this);
    return;
    }

  static std::unordered_set<ptr32_t> once;
  if(!once.insert(func).second)
    return;

  auto str = ikarus!=nullptr ? ikarus->demangleAddress(func) : "";
  if(!str.empty())
    Log::d("ASMINT_OP_call: unknown external function: \"", str, "\"");else
    Log::d("ASMINT_OP_call: unknown external function: ", func);
  }

std::string Cpu32::popString() {
  if(stack.size()==0)
    return 0;
  auto ptr = stack.back();
  stack.pop_back();

  auto mem = mem32.deref<const zString>(ptr);
  if(mem==nullptr || mem->len<=0)
    return "";
  auto chr = reinterpret_cast<const char*>(mem32.deref(mem->ptr, uint32_t(mem->len)));
  if(chr==nullptr)
    return "";
  std::string ret(chr, size_t(mem->len));
  return ret;
  }
