#ifndef _CFF_INT_H
#define _CFF_INT_H

#include <assert.h>

/*
CFF Data Types:

  Card8   uint8_t
  Card16  uint16_t
  Offset  uint32_t  // TODO? int32?
  OffSize uint8_t   // 1-4
  SID     uint16_t  // string id
*/

static inline uint8_t cff_read_Card8(const char **buf) // {{{
{
  return *((*buf)++);
}
// }}}
static inline uint16_t cff_read_Card16(const char **buf) // {{{
{
  const char *b=*buf;
  uint16_t ret=((unsigned char)b[0]<<8)|((unsigned char)b[1]);
  (*buf)+=2;
  return ret;
}
// }}}
static inline uint32_t cff_read_Card24(const char **buf) // {{{
{
  const char *b=*buf;
  uint32_t ret=((unsigned char)b[0]<<16)|
               ((unsigned char)b[1]<<8)|
               ((unsigned char)b[2]);
  (*buf)+=3;
  return ret;
}
// }}}
static inline uint32_t cff_read_Card32(const char **buf) // {{{
{
  const char *b=*buf;
  uint32_t ret=((unsigned char)b[0]<<24)|
               ((unsigned char)b[1]<<16)|
               ((unsigned char)b[2]<<8)|
               ((unsigned char)b[3]);
  (*buf)+=4;
  return ret;
}
// }}}
static inline CFF_OffSize cff_read_OffSize(const char **buf) // {{{
{
  uint8_t res=cff_read_Card8(buf);
  if ( (res<1)||(res>4) ) {
    return OFFERR;
  }
  return (CFF_OffSize)res;
}
// }}}
static inline uint32_t cff_read_Offset(const char **buf,CFF_OffSize offSize) // {{{
{
  switch (offSize) {
  case 1:
    return cff_read_Card8(buf);
  case 2:
    return cff_read_Card16(buf);
  case 3:
    return cff_read_Card24(buf);
  case 4:
    return cff_read_Card32(buf);
  default:
    break;
  }
  assert(0);
  return 0;
}
// }}}

static inline void cff_write_Card8(char **buf,uint8_t val) // {{{
{
  *((*buf)++)=val;
}
// }}}
static inline void cff_write_Card16(char **buf,uint16_t val) // {{{
{
  char *b=*buf;
  b[0]=(val>>8)&0xff;
  b[1]=val&0xff;
  (*buf)+=2;
}
// }}}
static inline void cff_write_Card24(char **buf,uint32_t val) // {{{
{
  char *b=*buf;
  b[0]=(val>>16)&0xff;
  b[1]=(val>>8)&0xff;
  b[2]=val&0xff;
  (*buf)+=3;
}
// }}}
static inline void cff_write_Card32(char **buf,uint32_t val) // {{{
{
  char *b=*buf;
  b[0]=val>>24;
  b[1]=(val>>16)&0xff;
  b[2]=(val>>8)&0xff;
  b[3]=val&0xff;
  (*buf)+=4;
}
// }}}
static inline void cff_write_OffSize(char **buf,CFF_OffSize val) // {{{
{
  cff_write_Card8(buf,val);
}
// }}}
static inline void cff_write_Offset(char **buf,CFF_OffSize offSize,uint32_t val) // {{{
{
  switch (offSize) {
  case 1:
    return cff_write_Card8(buf,val);
  case 2:
    return cff_write_Card16(buf,val);
  case 3:
    return cff_write_Card24(buf,val);
  case 4:
    return cff_write_Card32(buf,val);
  default:
    break;
  }
  assert(0);
}
// }}}

static inline const char *cff_index_get(struct CFF_Index *idx,int entry) // {{{
{
  if ( (entry<0)||(entry>=idx->count) ) {
    return NULL;
  }
  return idx->dataStart+idx->offset[entry]-1;
}
// }}}
static inline int cff_index_length(struct CFF_Index *idx,int entry) // {{{
{
  if ( (entry<0)||(entry>=idx->count) ) {
    return -1;
  }
  return idx->offset[entry+1]-idx->offset[entry];
}
// }}}

#endif
