#ifndef _CFF_TABLES_H
#define _CFF_TABLES_H

struct CFF_OPS {
  const char *name;
  int value;
  const char *defval;
  int flags;
};

#define CFFOP_DICT_TOP      0x0000
#define CFFOP_DICT_FONTINFO 0x0200 // includes DICT_TOP
#define CFFOP_DICT_PRIV     0x0100
#define CFFOP_DICTMASK      0x0100
  // ...? types?

#define CFFOP_TYPE_NUMBER    0x0001
#define CFFOP_TYPE_BOOL      0x0002
#define CFFOP_TYPE_SID       0x0003
#define CFFOP_TYPE_ARRAY     0x0004
#define CFFOP_TYPE_DELTA     0x0005
#define CFFOP_TYPE_NUMNUM    0x0009 // two numbers
#define CFFOP_TYPE_SIDSIDNUM 0x000b // SID+SID+num (for CID: ROS)
#define CFFOP_TYPEMASK       0x000f

static inline int cffop_is_topdict(int flags) // {{{
{
  return (flags&CFFOP_DICTMASK)==CFFOP_DICT_TOP;
}
// }}}
static inline int cffop_is_fontinfo(int flags) // {{{
{
  return !!(flags&CFFOP_DICT_FONTINFO);
}
// }}}
static inline int cffop_is_privdict(int flags) // {{{
{
  return (flags&CFFOP_DICTMASK)==CFFOP_DICT_PRIV;
}
// }}}

const struct CFF_OPS *cffop_get_by_value(int value);
const struct CFF_OPS *cffop_get_by_name(const char *name);

const char *cff_get_stdstr(int sid); // or NULL
int cff_find_stdstr(const char *str); // or -1

#endif
