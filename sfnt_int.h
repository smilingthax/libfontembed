#ifndef _SFNT_INT_H
#define _SFNT_INT_H

static inline unsigned short get_USHORT(const char *buf) // {{{
{
  return ((unsigned char)buf[0]<<8)|((unsigned char)buf[1]);
}
// }}}
static inline short get_SHORT(const char *buf) // {{{
{
  return (buf[0]<<8)|((unsigned char)buf[1]);
}
// }}}
static inline unsigned int get_UINT24(const char *buf) // {{{
{
  return ((unsigned char)buf[0]<<16)|
         ((unsigned char)buf[1]<<8)|
         ((unsigned char)buf[2]);
}
// }}}
static inline unsigned int get_ULONG(const char *buf) // {{{
{
  return ((unsigned char)buf[0]<<24)|
         ((unsigned char)buf[1]<<16)|
         ((unsigned char)buf[2]<<8)|
         ((unsigned char)buf[3]);
}
// }}}
static inline int get_LONG(const char *buf) // {{{
{
  return (buf[0]<<24)|
         ((unsigned char)buf[1]<<16)|
         ((unsigned char)buf[2]<<8)|
         ((unsigned char)buf[3]);
}
// }}}

static inline void set_USHORT(char *buf,unsigned short val) // {{{
{
  buf[0]=val>>8;
  buf[1]=val&0xff;
}
// }}}
static inline void set_ULONG(char *buf,unsigned int val) // {{{
{
  buf[0]=val>>24;
  buf[1]=(val>>16)&0xff;
  buf[2]=(val>>8)&0xff;
  buf[3]=val&0xff;
}
//  }}}

static inline int otf_padded(int length) // {{{
{
  return (length+3)&~3;
}
//  }}}

static inline unsigned int otf_checksum(const char *buf, unsigned int len) // {{{
{
  unsigned int ret=0;
  for (len=(len+3)/4;len>0;len--,buf+=4) {
    ret+=get_ULONG(buf);
  }
  return ret;
}
// }}}
static inline int get_width_fast(OTF_FILE *otf,int gid) // {{{
{
  if (gid>=otf->numberOfHMetrics) {
    return get_USHORT(otf->hmtx+(otf->numberOfHMetrics-1)*4);
  } else {
    return get_USHORT(otf->hmtx+gid*4);
  }
}
// }}}

static inline int is_CFF(OTF_FILE *otf) // {{{
{
  return !!(otf->flags&OTF_F_FMT_CFF);
}
// }}}

int otf_load_glyf(OTF_FILE *otf); //  - 0 on success
int otf_load_more(OTF_FILE *otf); //  - 0 on success

int otf_find_table(OTF_FILE *otf,unsigned int tag); // - table_index  or -1 on error

struct _OTF_WRITE;
struct _OTF_ACTION {
  char *data;
  OTF_DIRENT table;
  int (*load)(struct _OTF_WRITE *self); // 0 on success
  void (*free)(struct _OTF_WRITE *self);
};

struct _OTF_WRITE {
  unsigned long tag;
  void (*action)(struct _OTF_WRITE *self);
  union {
    struct {
      OTF_FILE *otf;
      int table_no;
    } copy;
    struct {
      char *data; // will be modified: padding=0, and to fix head csum
      int length; // data has to have enough extra space for padding!
    } replace;
  } args;
  struct _OTF_ACTION info;
};

void otf_action_copy(struct _OTF_WRITE *self);
void otf_action_replace(struct _OTF_WRITE *self);

// will change otw!
// Note: woffHdr is for internal use by otf_write_woff().
int otf_write_sfnt(struct _OTF_WRITE *otw,unsigned int version,int numTables,OTF_WOFF_HEADER *woffHdr,OUTPUT_FN output,void *context);

int otf_write_woff(struct _OTF_WRITE *otw,unsigned int version,int numTables,const char *metaData,unsigned int metaLength,const char *privData,unsigned int privLength,OUTPUT_FN output,void *context);

/** from sfnt_subset.c: **/

// otw {0,}-terminated, will be modified; returns numTables for otf_write_sfnt
int otf_intersect_tables(OTF_FILE *otf,struct _OTF_WRITE *otw);

#endif
