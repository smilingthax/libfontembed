#ifndef _CFF_TABLES_H
#define _CFF_TABLES_H

struct CFF_OPS {
  const char *name;
  int value;
  const char *defval;
  int flags;
};

#define CFFOP_DICT_TOP      0x0000
#define CFFOP_DICT_FONTINFO 0x0002 // includes DICT_TOP
#define CFFOP_DICT_PRIV     0x0001
#define CFFOP_DICT_MASK     0x0001
  // ...? types?

static inline int cff_is_topdict(int flags) // {{{
{
  return (flags&CFFOP_DICT_MASK)==CFFOP_DICT_TOP;
}
// }}}
static inline int cff_is_fontinfo(int flags) // {{{
{
  return !!(flags&CFFOP_DICT_FONTINFO);
}
// }}}
static inline int cff_is_privdict(int flags) // {{{
{
  return (flags&CFFOP_DICT_MASK)==CFFOP_DICT_PRIV;
}
// }}}

const struct CFF_OPS *cffop_get_by_value(int value);
const struct CFF_OPS *cffop_get_by_name(const char *name);

const char *cff_get_stdstr(int sid); // or NULL
int cff_find_stdstr(const char *str); // or -1

#endif
