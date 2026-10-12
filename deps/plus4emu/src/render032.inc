// Included inside namespace Plus4 by render.cpp; experimental, default off.
#if PLUS4_RENDER032
namespace {
// High nibble bits select foreground, one byte mask per pixel (little endian).
constexpr uint32_t characterMasks032[16] = {
  0x00000000,0xff000000,0x00ff0000,0xffff0000,
  0x0000ff00,0xff00ff00,0x00ffff00,0xffffff00,
  0x000000ff,0xff0000ff,0x00ff00ff,0xffff00ff,
  0x0000ffff,0xff00ffff,0x00ffffff,0xffffffff
};
}
PLUS4EMU_REGPARM2 void TED7360::render_char_packed032(TED7360& ted,int nextCharCnt)
{
  // A new character entering within the four-pixel group needs the original
  // detailed shift timing. Only the no-crossing cases use packed output.
  if (unsigned(nextCharCnt) - 1u < 3u) {
    render_char_std(ted,nextCharCnt);
    return;
  }
  uint8_t a,b,f;
  if (nextCharCnt == 0) {
    ted.shiftRegisterCharacter = ted.currentCharacter;
    a=ted.shiftRegisterCharacter.attr_();
    b=ted.shiftRegisterCharacter.bitmap_();
    f=ted.shiftRegisterCharacter.flags_();
    if(a&0x80) b &= (f&0xf0)|(f>>4)|ted.flashState;
    ted.shiftRegisterCharacter.bitmap_()=uint8_t(b<<4);
  } else {
    a=ted.shiftRegisterCharacter.attr_();
    b=ted.shiftRegisterCharacter.bitmap_();
    f=ted.shiftRegisterCharacter.flags_();
    ted.shiftRegisterCharacter.bitmap_()=uint8_t(b<<4);
  }
  if((ted.shiftRegisterCharacter.char_() & (f<<4)) & 0x80) b ^= 0xff;
  b ^= f & ted.flashState;
  // First pixel uses the delayed background; remaining pixels use FF15.
  const uint32_t background=uint32_t(ted.colorRegisters[0]) |
                            (uint32_t(ted.tedRegisters[0x15])*0x01010100u);
  const uint32_t foreground=uint32_t(a)*0x01010101u;
  const uint32_t pixels=background ^ ((foreground^background)&characterMasks032[b>>4]);
  ted.prv_video_buf_pos=ted.video_buf_pos;
  uint8_t* out=ted.video_buf+ted.video_buf_pos;
  ted.video_buf_pos+=5;
  out[0]=ted.videoOutputFlags|2;
  std::memcpy(out+1,&pixels,sizeof(pixels));
}
#endif
