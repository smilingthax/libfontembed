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

// NOTE: you probably want otf_get_table()
char *otf_read_table(OTF_FILE *otf,char *buf,const OTF_DIRENT *table);

struct _OTF_WRITE_TABLE;
struct _OTF_ACTION {
  char *data;
  OTF_DIRENT table;
  int (*load)(struct _OTF_WRITE_TABLE *self); // 0 on success
  void (*free)(struct _OTF_WRITE_TABLE *self);
};

struct _OTF_WRITE_TABLE {
  unsigned long tag;
  void (*action)(struct _OTF_WRITE_TABLE *self);
  union {
    struct {
      OTF_FILE *otf;
      int table_no;
    } copy;
    struct {
      char *data; // will be modified: padding=0, and to fix head csum
      int length; // data has to have enough extra space for padding!
    } replace;
  };

  // internal
  struct _OTF_ACTION info;
};

void otf_action_copy(struct _OTF_WRITE_TABLE *self);
void otf_action_replace(struct _OTF_WRITE_TABLE *self);

struct _OTF_WRITE_WOFF {
  unsigned short majorVersion,minorVersion;

  const char *metaData;
  unsigned int metaLength;
  const char *privData;
  unsigned int privLength;

  // private
  char *metaCompData;
};

int *otf_tagorder_win_sort(const struct _OTF_WRITE_TABLE *tables,int numTables);
int *otf_tagorder_offset_sort(const OTF_FILE *otf);

struct _OTF_WRITE_INFO {
  unsigned int version;
  unsigned short numTables;
  struct _OTF_WRITE_TABLE *tables;
  int *order; // if special sorting is desired; NULL: sorted by tags
};

// will change otw!;  woff can be NULL
int otf_write_sfnt(struct _OTF_WRITE_INFO *otw,struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context);

/** from sfnt_subset.c: **/

// otw {0,}-terminated, will be modified; returns numTables for otf_write_sfnt
int otf_intersect_tables(OTF_FILE *otf,struct _OTF_WRITE_TABLE *otw);

int otf_copy_sfnt(OTF_FILE *otf,struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context); // woff can be NULL
int otf_subset2(OTF_FILE *otf,BITSET glyphs,struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context); // returns number of bytes written
int otf_subset_cff2(OTF_FILE *otf,BITSET glyphs,struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context);

#endif
