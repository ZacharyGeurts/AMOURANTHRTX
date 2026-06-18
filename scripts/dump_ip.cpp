#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Emu.hpp"
#include <cstdio>
#include <vector>
int main(){
  std::vector<std::uint8_t> buf(FieldPlatform::FIELD_X86_DIE_UINTS*4,0);
  FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
  FieldDos::loadFloppyIntoGuest(buf.data(), FieldPlatform::DIE_HEADER_UINTS*4, FieldDos::defaultImagePath("."));
  auto* ram=FieldDos::guestRam(buf.data(), FieldPlatform::DIE_HEADER_UINTS*4);
  FieldX86Emu::Ctx ctx{};
  FieldX86Emu::runMapped(buf.data(), FieldPlatform::DIE_HEADER_UINTS*4, FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET, ctx, 1500000);
  auto*e=FieldX86Emu::emu;
  const unsigned cs=e->x86.R_CS&0xffff, ip=e->x86.R_EIP&0xffff;
  const unsigned lin=((cs<<4)+ip)&0xfffff;
  const unsigned t014d=((cs<<4)+0x14d)&0xfffff;
  std::printf("cs=%04x ip=%04x lin=%05x call@014d=",cs,ip,lin);
  for(int i=0;i<24;i++) std::printf("%02x ",ram[t014d+i]); std::putchar('\n');
  for(int i=0;i<32;i++) std::printf("%02x ",ram[lin+i]); std::putchar('\n');
  std::printf("7196: "); for(int i=0;i<8;i++) std::printf("%02x ",ram[0x7196+i]); std::putchar('\n');
}

