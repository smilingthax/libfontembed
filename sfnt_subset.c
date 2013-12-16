#include "sfnt.h"
#include "sfnt_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "bitset.h"

int otf_ttc_extract(OTF_FILE *otf,OUTPUT_FN output,void *context) // {{{
{
  assert(otf);
  assert(output);
  assert(otf->numTTC);
  int iA;

  struct _OTF_WRITE_INFO otw={
    .version=otf->version,
    .numTables=otf->numTables
  };
  otw.tables=malloc(sizeof(struct _OTF_WRITE_TABLE)*otf->numTables);
  if (!otw.tables) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }

  // just copy everything
  for (iA=0;iA<otf->numTables;iA++) {
    otw.tables[iA].tag=otf->tables[iA].tag;
    otw.tables[iA].action=otf_action_copy;
    otw.tables[iA].args.copy.otf=otf;
    otw.tables[iA].args.copy.table_no=iA;
  }
  otw.order=otf_tagorder_offset_sort(otf);

  const int ret=otf_write_sfnt(&otw,NULL,output,context);

  free(otw.order);
  free(otw.tables);
  return ret;
}
// }}}

// otw {0,}-terminated, will be modified; returns numTables for otf_write_sfnt
int otf_intersect_tables(OTF_FILE *otf,struct _OTF_WRITE_TABLE *tables) // {{{
{
  int iA,iB,numTables=0;
  for (iA=0,iB=0;(iA<otf->numTables)&&(tables[iB].tag);) {
    if (otf->tables[iA].tag==tables[iB].tag) {
      if (tables[iB].action==otf_action_copy) {
        tables[iB].args.copy.table_no=iA; // original table location found.
      }
      if (iB!=numTables) { // >, actually
        memmove(tables+numTables,tables+iB,sizeof(struct _OTF_WRITE_TABLE));
      }
      iA++;
      iB++;
      numTables++;
    } else if (otf->tables[iA].tag<tables[iB].tag) {
      iA++;
    } else { // not in otf->tables
      if (tables[iB].action!=otf_action_copy) { // keep
        if (iB!=numTables) { // >, actually
          memmove(tables+numTables,tables+iB,sizeof(struct _OTF_WRITE_TABLE));
        }
        numTables++;
      } // else delete
      iB++;
    }
  }
  return numTables;
}
// }}}


// include components (set bit in >glyphs) of currently loaded compound glyph (with >curgid)
// returns additional space requirements (when bits below >donegid are touched)
static int otf_subset_glyf(OTF_FILE *otf,int curgid,int donegid,BITSET glyphs) // {{{
{
  int ret=0;
  if (get_SHORT(otf->gly)>=0) { // not composite
    return ret; // done
  }

  char *cur=otf->gly+10;

  unsigned short flags;
  do {
    flags=get_USHORT(cur);
    const unsigned short sub_gid=get_USHORT(cur+2);
    assert(sub_gid<otf->numGlyphs);
    if (!bit_check(glyphs,sub_gid)) {
      // bad: temporarily load sub glyph
      const int len=otf_get_glyph(otf,sub_gid);
      assert(len>0);
      bit_set(glyphs,sub_gid);
      if (sub_gid<donegid) {
        ret+=len;
        ret+=otf_subset_glyf(otf,sub_gid,donegid,glyphs); // composite of composites?, e.g. in DejaVu
      }
      const int res=otf_get_glyph(otf,curgid); // reload current glyph
      assert(res);
    }

    // skip parameters
    cur+=6;
    if (flags&0x01) {
      cur+=2;
    }
    if (flags&0x08) {
      cur+=2;
    } else if (flags&0x40) {
      cur+=4;
    } else if (flags&0x80) {
      cur+=8;
    }
  } while (flags&0x20); // more components

  return ret;
}
// }}}

// TODO: cmap only required in non-CID context
int otf_subset(OTF_FILE *otf,BITSET glyphs,OUTPUT_FN output,void *context) // {{{ - returns number of bytes written
{
  assert(otf);
  assert(glyphs);
  assert(output);

  int iA,b,c;

  // first pass: include all required glyphs
  bit_set(glyphs,0); // .notdef always required
  int glyfSize=0;
  for (iA=0,b=0,c=1;iA<otf->numGlyphs;iA++,c<<=1) {
    if (!c) {
      b++;
      c=1;
    }
    if (glyphs[b]&c) {
      int len=otf_get_glyph(otf,iA);
      if (len<0) {
        assert(0);
        return -1;
      } else if (len>0) {
        glyfSize+=len;
        len=otf_subset_glyf(otf,iA,iA,glyphs);
        if (len<0) {
          assert(0);
          return -1;
        }
        glyfSize+=len;
      }
    }
  }

  // second pass: calculate new glyf and loca
  int locaSize=(otf->numGlyphs+1)*(otf->indexToLocFormat+1)*2;

  char *new_loca=malloc(otf_padded(locaSize));
  char *new_glyf=malloc(otf_padded(glyfSize));
  if ( (!new_loca)||(!new_glyf) ) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    assert(0);
    free(new_loca);
    free(new_glyf);
    return -1;
  }

  int offset=0;
  for (iA=0,b=0,c=1;iA<otf->numGlyphs;iA++,c<<=1) {
    if (!c) {
      b++;
      c=1;
    }

    assert(offset%2==0);
    // TODO? change format? if glyfSize<0x20000
    if (otf->indexToLocFormat==0) {
      set_USHORT(new_loca+iA*2,offset/2);
    } else { // ==1
      set_ULONG(new_loca+iA*4,offset);
    }

    if (glyphs[b]&c) {
      const int len=otf_get_glyph(otf,iA);
      assert(len>=0);
      memcpy(new_glyf+offset,otf->gly,len);
      offset+=len;
    }
  }
  // last entry
  if (otf->indexToLocFormat==0) {
    set_USHORT(new_loca+otf->numGlyphs*2,offset/2);
  } else { // ==1
    set_ULONG(new_loca+otf->numGlyphs*4,offset);
  }
  assert(offset==glyfSize);

  // determine new tables.
  struct _OTF_WRITE_TABLE tables[]={ // sorted
    // TODO: cmap only required in non-CID context   or always in CFF
      {OTF_TAG('c','m','a','p'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('c','v','t',' '),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('f','p','g','m'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('g','l','y','f'),otf_action_replace,.args.replace={new_glyf,glyfSize}},
      {OTF_TAG('h','e','a','d'),otf_action_copy,.args.copy={otf,}}, // _copy_head
      {OTF_TAG('h','h','e','a'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('h','m','t','x'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('l','o','c','a'),otf_action_replace,.args.replace={new_loca,locaSize}},
      {OTF_TAG('m','a','x','p'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('n','a','m','e'),otf_action_copy,.args.copy={otf,}},
      {OTF_TAG('p','r','e','p'),otf_action_copy,.args.copy={otf,}},
      // vhea vmtx (never used in PDF, but possible in PS>=3011)
      {0,0,}};

  // and write them
  int numTables=otf_intersect_tables(otf,tables);
  struct _OTF_WRITE_INFO otw={
    .version=otf->version,
    .numTables=numTables,
    .tables=tables,
    .order=otf_tagorder_win_sort(tables,numTables)
  };
  const int ret=otf_write_sfnt(&otw,NULL,output,context);

  free(otw.order);
  free(new_loca);
  free(new_glyf);
  return ret;

  //TODO ? reduce cmap [to (1,0) ;-)]
  //TODO (cmap for non-cid)
}
// }}}

// TODO no subsetting actually done (for now)
int otf_subset_cff(OTF_FILE *otf,BITSET glyphs,OUTPUT_FN output,void *context) // {{{ - returns number of bytes written
{
  assert(otf);
  assert(output);

// TODO char *new_cff=cff_subset(...);

  // determine new tables.
  struct _OTF_WRITE_TABLE tables[]={
      {OTF_TAG('C','F','F',' '),otf_action_copy,.args.copy={otf,}},
//      {OTF_TAG('C','F','F',' '),otf_action_replace,new_glyf,glyfSize},
      {OTF_TAG('c','m','a','p'),otf_action_copy,.args.copy={otf,}},
#if 0 // not actually needed!
      {OTF_TAG('c','v','t',' '),otf_action_copy,otf,},
      {OTF_TAG('f','p','g','m'),otf_action_copy,otf,},
      {OTF_TAG('h','e','a','d'),otf_action_copy,otf,}, // _copy_head
      {OTF_TAG('h','h','e','a'),otf_action_copy,otf,},
      {OTF_TAG('h','m','t','x'),otf_action_copy,otf,},
      {OTF_TAG('m','a','x','p'),otf_action_copy,otf,},
      {OTF_TAG('n','a','m','e'),otf_action_copy,otf,},
      {OTF_TAG('p','r','e','p'),otf_action_copy,otf,},
#endif
      {0,0,}};

  // and write them
  const int numTables=otf_intersect_tables(otf,tables);
  struct _OTF_WRITE_INFO otw={
    .version=otf->version,
    .numTables=numTables,
    .tables=tables,
    .order=otf_tagorder_win_sort(tables,numTables)
  };
  const int ret=otf_write_sfnt(&otw,NULL,output,context);

  free(otw.order);
//  free(new_cff);
  return ret;
}
// }}}

//int copy_block(FILE *f,long pos,int length,OUTPUT_FN output,void *context);  // copied bytes or -1 (also on premature EOF)

static int copy_block(FILE *f,long pos,int length,OUTPUT_FN output,void *context) // {{{
{
  assert(f);
  assert(output);

  char buf[4096];
  int iA,ret;

  ret=fseek(f,pos,SEEK_SET);
  if (ret==-1) {
    fprintf(stderr,"Seek failed: %s\n", strerror(errno));
    return -1;
  }
  ret=0;
  while (length>4096) {
    iA=fread(buf,1,4096,f);
    if (iA<4096) {
      return -1;
    }
    (*output)(buf,iA,context);
    ret+=iA;
    length-=iA;
  };
  iA=fread(buf,1,length,f);
  if (iA<length) {
    return -1;
  }
  (*output)(buf,iA,context);
  ret+=iA;

  return ret;
}
// }}}

int otf_cff_extract(OTF_FILE *otf,OUTPUT_FN output,void *context) // {{{ - returns number of bytes written
{
  assert(otf);
  assert(output);

  if (otf->flags&OTF_F_WOFF) {
    int len;
    char *data=otf_get_table(otf,OTF_TAG('C','F','F',' '),&len);
    if (!data) {
      return -1;
    }
    (*output)(data,len,context);
    return len;
  }

  int idx=otf_find_table(otf,OTF_TAG('C','F','F',' '));
  if (idx==-1) {
    return -1;
  }
  const OTF_DIRENT *table=otf->tables+idx;

  return copy_block(otf->f,table->offset,table->length,output,context);
}
// }}}

// CFF *otf_get_cff(); // not load, but create by "substream"-in ctor
#if 0 // TODO elsewhere : char *cff_subset(...);
  // first pass: include all required glyphs
  bit_set(glyphs,0); // .notdef always required
  int glyfSize=0;
  for (iA=0,b=0,c=1;iA<otf->numGlyphs;iA++,c<<=1) {
    if (!c) {
      b++;
      c=1;
    }
    if (glyphs[b]&c) {
// TODO: cff_glyph
    }
  }

  // second pass: calculate new glyf and loca
  char *new_cff=malloc(cffSize);
  if (!new_cff) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    assert(0);
    return -1;
  }

  int offset=0;
  for (iA=0,b=0,c=1;iA<otf->numGlyphs;iA++,c<<=1) {
    if (!c) {
      b++;
      c=1;
    }
    if (glyphs[b]&c) {
//...
    }
  }
  return new_cff;
#endif
