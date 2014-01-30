#include "cff_tables.h"
#include <string.h>
#include <stdlib.h>

//  static const struct CFF_OPS cffop_##name={#name,value,defval,flags}
#define CFF_OP(name,value,defval,flags) \
  {#name,value,defval,flags}

#define CFF_TOPOP(name,value,type,defval) \
  CFF_OP(name,value,defval,CFFOP_DICT_TOP|CFFOP_TYPE_##type)

#define CFF_TOPFIOP(name,value,type,defval) \
  CFF_OP(name,value,defval,CFFOP_DICT_FONTINFO|CFFOP_TYPE_##type)

#define CFF_PRIVOP(name,value,type,defval) \
  CFF_OP(name,value,defval,CFFOP_DICT_PRIV|CFFOP_TYPE_##type)

// values 0..21
static const struct CFF_OPS cff_ops1[]={
  CFF_TOPFIOP(version,0,SID,NULL),
  CFF_TOPFIOP(Notice,1,SID,NULL),
  CFF_TOPFIOP(FullName,2,SID,NULL),
  CFF_TOPFIOP(FamilyName,3,SID,NULL),
  CFF_TOPFIOP(Weight,4,SID,NULL),
  CFF_TOPOP(FontBBox,5,ARRAY,"\x8b\x8b\x8b\x8b"), // 0 0 0 0
  CFF_PRIVOP(BlueValues,6,DELTA,NULL),
  CFF_PRIVOP(OtherBlues,7,DELTA,NULL),
  CFF_PRIVOP(FamilyBlues,8,DELTA,NULL),
  CFF_PRIVOP(FamilyOtherBlues,9,DELTA,NULL),
  CFF_PRIVOP(StdHW,10,NUMBER,NULL),
  CFF_PRIVOP(StdVW,11,NUMBER,NULL),
  {NULL}, // 12 is escape
  CFF_TOPOP(UniqueID,13,NUMBER,NULL),
  CFF_TOPOP(XUID,14,ARRAY,NULL),
  CFF_TOPOP(charset,15,NUMBER,"\x8b"),
  CFF_TOPOP(Encoding,16,NUMBER,"\x8b"),
  CFF_TOPOP(CharStrings,17,NUMBER,NULL),
  CFF_TOPOP(Private,18,NUMNUM,NULL),
  CFF_PRIVOP(Subrs,19,NUMBER,NULL),
  CFF_PRIVOP(defaultWidthX,20,NUMBER,"\x8b"),
  CFF_PRIVOP(nominalWidthX,21,NUMBER,"\x8b")
};

// values 0x0c00..0x0c26
static const struct CFF_OPS cff_ops2[]={
  CFF_TOPFIOP(Copyright,0x0c00,SID,NULL),
  CFF_TOPFIOP(isFixedPitch,0x0c01,BOOL,"\x8b"), // 0 (false)
  CFF_TOPFIOP(ItalicAngle,0x0c02,NUMBER,"\x8b"),
  CFF_TOPFIOP(UnderlinePosition,0x0c03,NUMBER,"\x27"), // -100
  CFF_TOPFIOP(UnderlineThickness,0x0c04,NUMBER,"\xbd"), // 50
  CFF_TOPOP(PaintType,0x0c05,NUMBER,"\x8b"),
  CFF_TOPOP(CharstringType,0x0c06,NUMBER,"\x8d"), // 2
  CFF_TOPOP(FontMatrix,0x0c07,ARRAY,"\x1e\x1c\x3f\x8d\x8d\x1e\x1c\x3f\x8d\x8d"), // 0.001 0 0 0.001 0 0
  CFF_TOPOP(StrokeWidth,0x0c08,NUMBER,"\x8b"),
  CFF_PRIVOP(BlueScale,0x0c09,NUMBER,"\x1e\xa0\x39\x62\x5f"), // 0.039625
  CFF_PRIVOP(BlueShift,0x0c0a,NUMBER,"\x92"), // 7
  CFF_PRIVOP(BlueFuzz,0x0c0b,NUMBER,"\x8c"), // 1
  CFF_PRIVOP(StemSnapH,0x0c0c,DELTA,NULL),
  CFF_PRIVOP(StemSnapV,0x0c0d,DELTA,NULL),
  CFF_PRIVOP(ForceBold,0x0c0e,BOOL,"\x8b"),
  {NULL},{NULL},  // 0x0c0f, 0x0c10  reserved
  CFF_PRIVOP(LanguageGroup,0x0c11,NUMBER,"\x8b"),
  CFF_PRIVOP(ExpansionFactor,0x0c12,NUMBER,"\x1e\xa0\x6f"), // 0.06
  CFF_PRIVOP(initialRandomSeed,0x0c13,NUMBER,"\x8b"),
  CFF_TOPOP(SyntheticBase,0x0c14,NUMBER,NULL),
  CFF_TOPOP(PostScript,0x0c15,SID,NULL),
  CFF_TOPOP(BaseFontName,0x0c16,SID,NULL),
  CFF_TOPOP(BaseFontBlend,0x0c17,DELTA,NULL),
  {NULL},{NULL},{NULL},{NULL},{NULL},{NULL}, // 0x0c18..0x0c1d  reserved

  // CID
  CFF_TOPOP(ROS,0x0c1e,SIDSIDNUM,NULL),
  CFF_TOPOP(CIDFontVersion,0x0c1f,NUMBER,"\x8b"),
  CFF_TOPOP(CIDFontRevision,0x0c20,NUMBER,"\x8b"),
  CFF_TOPOP(CIDFontType,0x0c21,NUMBER,"\x8b"),
  CFF_TOPOP(CIDCount,0x0c22,NUMBER,"\x1c\x22\x11"), // 8720
  CFF_TOPOP(UIDBase,0x0c23,NUMBER,NULL),
  CFF_TOPOP(FDArray,0x0c24,NUMBER,NULL),
  CFF_TOPOP(FDSelect,0x0c25,NUMBER,NULL),
  CFF_TOPOP(FontName,0x0c26,SID,NULL)
};

// sorted by name
static const struct CFF_OPS *cff_opnames[]={
  cff_ops2+0x17, // BaseFontBlend
  cff_ops2+0x16, // BaseFontName
  cff_ops2+0x0b, // BlueFuzz
  cff_ops2+0x09, // BlueScale
  cff_ops2+0x0a, // BlueShift
  cff_ops1+6,    // BlueValues

  cff_ops2+0x22, // CIDCount
  cff_ops2+0x20, // CIDFontRevision
  cff_ops2+0x21, // CIDFontType
  cff_ops2+0x1f, // CIDFontVersion
  cff_ops1+17,   // CharStrings
  cff_ops2+0x06, // CharstringType
  cff_ops2+0x00, // Copyright

  cff_ops1+16,   // Encoding
  cff_ops2+0x12, // ExpansionFactor

  cff_ops2+0x24, // FDArray
  cff_ops2+0x25, // FDSelect
  cff_ops1+8,    // FamilyBlues
  cff_ops1+3,    // FamilyName
  cff_ops1+9,    // FamilyOtherBlues
  cff_ops1+5,    // FontBBox
  cff_ops2+0x07, // FontMatrix
  cff_ops2+0x26, // FontName
  cff_ops2+0x0e, // ForceBold
  cff_ops1+2,    // FullName

  cff_ops2+0x02, // ItalicAngle
  cff_ops2+0x11, // LanguageGroup
  cff_ops1+1,    // Notice
  cff_ops1+7,    // OtherBlues

  cff_ops2+0x05, // PaintType
  cff_ops2+0x15, // PostScript
  cff_ops1+18,   // Private

  cff_ops2+0x1e, // ROS

  cff_ops1+10,   // StdHW
  cff_ops1+11,   // StdVW
  cff_ops2+0x0c, // StemSnapH
  cff_ops2+0x0d, // StemSnapV
  cff_ops2+0x08, // StrokeWidth
  cff_ops1+19,   // Subrs
  cff_ops2+0x14, // SyntheticBase

  cff_ops2+0x23, // UIDBase
  cff_ops2+0x03, // UnderlinePosition
  cff_ops2+0x04, // UnderlineThickness
  cff_ops1+13,   // UniqueID

  cff_ops1+4,    // Weight
  cff_ops1+14,   // XUID

  cff_ops1+15,   // charset
  cff_ops1+20,   // defaultWidthX
  cff_ops2+0x13, // initialRandomSeed
  cff_ops2+0x01, // isFixedPitch
  cff_ops1+21,   // nominalWidthX
  cff_ops1+0    // version
};

const struct CFF_OPS *cffop_get_by_value(int value) // {{{
{
  if ( (value>=0)&&(value<sizeof(cff_ops1)/sizeof(*cff_ops1)) ) {
    return &cff_ops1[value];
  }
  value-=0x0c00;
  if ( (value>=0)&&(value<sizeof(cff_ops2)/sizeof(*cff_ops2)) ) {
    return &cff_ops2[value];
  }
  return NULL;
}
// }}}

int cffop_cmp(const void *a,const void *b) // {{{
{
  const struct CFF_OPS *oa=a;
  const struct CFF_OPS *ob=b;
  return strcmp(oa->name,ob->name);
}
// }}}

const struct CFF_OPS *cffop_get_by_name(const char *name) // {{{
{
  const struct CFF_OPS key={name};
  return bsearch(&key,cff_opnames,sizeof(cff_opnames)/sizeof(*cff_opnames),sizeof(key),cffop_cmp);
}
// }}}

static const char *cff_stdstr[]={
  ".notdef", "space", "exclam", "quotedbl",  "numbersign", "dollar", "percent", "ampersand",
  "quoteright", "parenleft", "parenright", "asterisk",  "plus", "comma", "hyphen", "period",
  "slash", "zero", "one", "two",  "three", "four", "five", "six",
  "seven", "eight", "nine", "colon",  "semicolon", "less", "equal", "greater",
  "question", "at", "A", "B",  "C", "D", "E", "F",
  "G", "H", "I", "J",  "K", "L", "M", "N",
  "O", "P", "Q", "R",  "S", "T", "U", "V",
  "W", "X", "Y", "Z",  "bracketleft", "backslash", "bracketright", "asciicircum",
  "underscore", "quoteleft", "a", "b",  "c", "d", "e", "f",
  "g", "h", "i", "j",  "k", "l", "m", "n",
  "o", "p", "q", "r",  "s", "t", "u", "v",
  "w", "x", "y", "z",  "braceleft", "bar", "braceright", "asciitilde",
  "exclamdown", "cent", "sterling", "fraction",  "yen", "florin", "section", "currency",
  "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",  "guilsinglright", "fi", "fl", "endash",
  "dagger", "daggerdbl", "peridcentered", "paragraph",  "bullet", "quotesinglebase", "quotedblbase", "quotedblright",
  "guillemotright", "ellipsis", "perthousand", "questiondown",  "grave", "acute", "circumflex", "tilde",
  "macron", "breve", "dotaccent", "dieresis",  "ring", "cedilla", "hungarumlaut", "ogonek",
  "caron", "emdash", "AE", "ordfeminine",  "Lslash", "Oslash", "OE", "ordmasculine",
  "ae", "dotlessi", "lslash", "oslash",  "oe", "germandbls", "onesuperior", "logicalnot",
  "mu", "trademark", "Eth", "onehalf",  "plusminus", "Thorn", "onequarter", "divide",
  "brokenbar", "degree", "thorn", "threequarters",  "twosuperior", "registered", "minus", "eth",
  "multiply", "threesuperior", "copyright", "Aacute",  "Acircumflex", "Adieresis", "Agrave", "Aring",
  "Atilde", "Ccedilla", "Eacute", "Ecircumflex",  "Edieresis", "Egrave", "Iacute", "Icircumflex",
  "Idieresis", "Igrave", "Ntilde", "Oacute",  "Ocircumflex", "Odieresis", "Ograve", "Otilde",
  "Scaron", "Uacute", "Ucircumflex", "Udieresis",  "Ugrave", "Yacute", "Ydieresis", "Zcaron",
  "aacute", "acircumflex", "adieresis", "agrave",  "aring", "atilde", "ccedilla", "eacute",
  "ecircumflex", "edieresis", "egrave", "iacute",  "icircumflex", "idieresis", "igrave", "ntilde",
  "oacute", "ocircumflex", "odieresis", "ograve",  "otilde", "scaron", "uacute", "ucircumflex",
  "udieresis", "ugrave", "yacute", "ydieresis",  "zcaron", "exclamsmall", "Hungarumlautsmall", "dollaroldstyle",
  "dollarsuperior", "ampersandsmall", "Acutesmall", "parenleftsuperior",
   "parenrightsuperior", "twodotenleader", "onedotenleader", "zerooldstyle",
  "oneoldstyle", "twooldstyle", "threeoldstyle", "fouroldstyle",  "fiveoldstyle", "sixoldstyle", "sevenoldstyle", "eightoldstyle",
  "nineoldstyle", "commasuperior", "threequartersemdash", "periodsuperior",  "questionsmall", "asuperior", "bsuperior", "centsuperior",
  "dsuperior", "esuperior", "isuperior", "lsuperior",  "msuperior", "nsuperior", "osuperior", "rsuperior",
  "ssuperior", "tsuperior", "ff", "ffi",  "ffl", "parenleftinferior", "parenrightinferior", "Circumflexsmall",
  "hyphensuperior", "Gravesmall", "Asmall", "Bsmall",  "Csmall", "Dsmall", "Esmall", "Fsmall",
  "Gsmall", "Hsmall", "Ismall", "Jsmall",  "Ksmall", "Lsmall", "Msmall", "Nsmall",
  "Osmall", "Psmall", "Qsmall", "Rsmall",  "Ssmall", "Tsmall", "Usmall", "Vsmall",
  "Wsmall", "Xsmall", "Ysmall", "Zsmall",  "colonmonetary", "onefitted", "rupiah", "Tildesmall",
  "exclamdownsmall", "centoldstyle", "Lslashsmall", "Scaronsmall",  "Zcaronsmall", "Dieresissmall", "Brevesmall", "Caronsmall",
  "Dotaccentsmall", "Macronsmall", "figuredash", "hypheninferior",  "Ogoneksmall", "Ringsmall", "Cedillasmall", "questiondownsmall",
  "oneeighth", "threeeighths", "fiveeighths", "seveneighths",  "onethird", "twothird", "zerosuperior", "foursuperior",
  "fivesuperior", "sixsuperior", "sevensuperior", "eightsuperior",  "ninesuperior", "zeroinferior", "oneinferior", "twoinferior",
  "threeinferior", "fourinferior", "fiveinferior", "sixinferior",  "seveninferior", "eightinferior", "nineinferior", "centinferior",
  "dollarinferior", "periodinferior", "commainferior", "Agravesmall",  "Aacutesmall", "Acircumflexsmall", "Atildesmall", "Adieresissmall",
  "Aringsmall", "AEsmall", "Ccedillasmall", "Egravesmall",  "Eacutesmall", "Ecircumflexsmall", "Edieresissmall", "Igravesmall",
  "Iacutesmall", "Icircumflexsmall", "Idieresissmall", "Ethsmall",  "Ntildesmall", "Ogravesmall", "Oacutesmall", "Ocircumflexsmall",
  "Otildesmall", "Odieresissmall", "OEsmall", "Oslashsmall",  "Ugravesmall", "Uacutesmall", "Ucircumflexsmall", "Udieresissmall",
  "Yacutesmall", "Thornsmall", "Ydieresissmall", "001.000",  "001.001", "001.002", "001.003", "Black",
  "Bold", "Book", "Light", "Medium",  "Regular", "Roman", "Semibold"
};

int cff_pstrcmp(const void *a,const void *b) // {{{
{
  const char ** const *pa=a;
  const char ** const *pb=b;
  return strcmp(**pa,**pb);
}
// }}}

static const char **cff_rev_stdstr[sizeof(cff_stdstr)/sizeof(*cff_stdstr)]={0};

static void init_rev_stdstr() // {{{
{
  if (*cff_rev_stdstr) {
    return;
  }
  int iA;
  for (iA=0;iA<sizeof(cff_stdstr)/sizeof(*cff_stdstr);iA++) {
    cff_rev_stdstr[iA]=cff_stdstr+iA;
  }
  qsort(cff_rev_stdstr,sizeof(cff_rev_stdstr)/sizeof(*cff_rev_stdstr),sizeof(*cff_rev_stdstr),cff_pstrcmp);
}
// }}}

const char *cff_get_stdstr(int sid) // {{{
{
  if ( (sid<0)||(sid>sizeof(cff_stdstr)/sizeof(*cff_stdstr)) ) {
    return NULL;
  }
  return cff_stdstr[sid];
}
// }}}

int cff_find_stdstr(const char *str) // {{{
{
  // assert(str);
  init_rev_stdstr();
  const char **key=&str;
  const char ***res=bsearch(&key,cff_rev_stdstr,sizeof(cff_rev_stdstr)/sizeof(*cff_rev_stdstr),sizeof(*cff_rev_stdstr),cff_pstrcmp);
  if (!res) {
    return -1;
  }
  return *res-cff_stdstr;
}
// }}}

