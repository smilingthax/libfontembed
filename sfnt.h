#ifndef _SFNT_H
#define _SFNT_H

#include <stdio.h>

typedef struct {
  unsigned int tag;
  unsigned int checkSum;
  unsigned int offset;
  unsigned int length;
  unsigned int compLength; // for WOFF
} OTF_DIRENT;

typedef struct {
  unsigned int length;
  unsigned int origLength;
  unsigned short majorVersion,minorVersion;
  unsigned int metaOffset;
  unsigned int metaLength;
  unsigned int metaOrigLength;
  unsigned int privOffset;
  unsigned int privLength;
} OTF_WOFF_HEADER;

typedef struct {
  FILE *f;
  int flags;
  unsigned int numTTC,useTTC;

  unsigned int version;
  OTF_WOFF_HEADER woff;

  unsigned short numTables;
  OTF_DIRENT *tables;

  unsigned short unitsPerEm;
  unsigned short indexToLocFormat; // 0=short, 1=long
  unsigned short numGlyphs;

  // optionally loaded data
  unsigned int *glyphOffsets;
  unsigned short numberOfHMetrics;
  char *hmtx,*name,*cmap;
  const char *unimap; // ptr to (3,1) or (3,0) cmap start

  // single glyf buffer, allocated large enough by otf_load_more()
  char *gly;
  OTF_DIRENT *glyfTable; // direct access
  char *glyf; // even more direct access...

} OTF_FILE;
#define OTF_F_CACHE_GLYF   0x00001
#define OTF_F_WOFF         0x08000
#define OTF_F_FMT_CFF      0x10000
#define OTF_F_DO_CHECKSUM  0x40000

// to load TTC collections: append e.g. "/3" for the third font in the file.
OTF_FILE *otf_load(const char *file);
OTF_FILE *otf_load2(FILE *f,int use_ttc/*=-1*/); // takes f
void otf_close(OTF_FILE *otf);

#define OTF_TAG(a,b,c,d)  (unsigned int)( ((a)<<24)|((b)<<16)|((c)<<8)|(d) )
#define OTF_UNTAG(a)  (((unsigned int)(a)>>24)&0xff),(((unsigned int)(a)>>16)&0xff),\
                      (((unsigned int)(a)>>8)&0xff),(((unsigned int)(a))&0xff)

char *otf_get_table(OTF_FILE *otf,unsigned int tag,int *ret_len);

int otf_get_width(OTF_FILE *otf,unsigned short gid);
const char *otf_get_name(OTF_FILE *otf,int platformID,int encodingID,int languageID,int nameID,int *ret_len);
int otf_get_glyph(OTF_FILE *otf,unsigned short gid);
unsigned short otf_from_unicode(OTF_FILE *otf,int unicode);

#include "bitset.h"
#include "iofn.h"

// TODO?! allow glyphs==NULL for non-subsetting table reduction?
int otf_subset(OTF_FILE *otf,BITSET glyphs,OUTPUT_FN output,void *context);
int otf_ttc_extract(OTF_FILE *otf,OUTPUT_FN output,void *context);
int otf_subset_cff(OTF_FILE *otf,BITSET glyphs,OUTPUT_FN output,void *context);
int otf_cff_extract(OTF_FILE *otf,OUTPUT_FN output,void *context);

#endif
