#include "sfnt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "sfnt_int.h"

#ifdef WOFF_SUPPORT
  #include <zlib.h>
#endif


// TODO?
// get_SHORT(head+48) // fontDirectionHint
/* reqd. Tables: cmap, head, hhea, hmtx, maxp, name, OS/2, post
 OTF: glyf,loca [cvt,fpgm,prep]
 */

static char *otf_bsearch(char *table, // {{{
                         const char *target,int len,
                         int searchRange,
                         int entrySelector,
                         int rangeShift,
                         int lower_bound) // return lower_bound, if !=0
{
  char *ret=table+rangeShift;
  if (memcmp(target,ret,len)<0) {
    ret=table;
  }

  for (;entrySelector>0;entrySelector--) {
    searchRange>>=1;
    ret+=searchRange;
    if (memcmp(target,ret,len)<0) {
      ret-=searchRange;
    }
  }
  const int result=memcmp(target,ret,len);
  if (result==0) {
    return ret;
  } else if (lower_bound) {
    if (result>0) {
      return ret+searchRange;
    }
    return ret;
  }
  return NULL; // not found;
}
// }}}

static OTF_FILE *otf_new(FILE *f) // {{{
{
  assert(f);

  OTF_FILE *ret;
  ret=calloc(1,sizeof(OTF_FILE));
  if (ret) {
    ret->f=f;
    ret->version=0x00010000;
  }

  return ret;
}
// }}}

static int otf_read_raw(FILE *f,char *buf,long pos,int length) // {{{
{
  const int res=fseek(f,pos,SEEK_SET);
  if (res==-1) {
    fprintf(stderr,"Seek failed: %s\n", strerror(errno));
    return -1;
  }
  return fread(buf,1,length,f);
}
// }}}

static int otf_read_compressed_raw(FILE *f,char *buf,long pos,unsigned long length,int compLength) // {{{
{
#ifdef WOFF_SUPPORT
  char *tmp=malloc(sizeof(char)*compLength);
  if (!tmp) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }
  const int res=otf_read_raw(f,tmp,pos,compLength);
  if (res!=compLength) {
    fprintf(stderr,"Short read\n");
    free(tmp);
    return -1;
  }
  if (uncompress((unsigned char *)buf,&length,(const unsigned char *)tmp,compLength)!=Z_OK) {
    fprintf(stderr,"Decompression failed\n");
    free(tmp);
    return -1;
  }
  free(tmp);
  return length;
#else
  fprintf(stderr,"Cannot read compressed table, WOFF_SUPPORT has not been compiled in\n");
  return -1;
#endif
}
// }}}

// uncompressed read, buf!=NULL, w/o padding
static char *otf_read(OTF_FILE *otf,char *buf,long pos,int length) // {{{
{
  assert(length>0);
  const int res=otf_read_raw(otf->f,buf,pos,length);
  if (res!=length) {
    fprintf(stderr,"Short read\n");
    return NULL;
  }
  return buf;
}
// }}}

// NOTE: you probably want otf_get_table()
// will alloc if >buf ==NULL; otherwise buf must be big enough to hold uncompressed length + padding
// returns >buf, or NULL on error
char *otf_read_table(OTF_FILE *otf,char *buf,const OTF_DIRENT *table) // {{{
{
  char *ours=NULL;

  if (table->length==0) {
    return buf;
  } else if ( (table->compLength<0)||(table->length<table->compLength) ) {
    assert(0);
    return NULL;
  }

  const int pad_len=otf_padded(table->length); // for checksum...
  if (!buf) {
    ours=buf=malloc(sizeof(char)*pad_len);
    if (!buf) {
      fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
      return NULL;
    }
  }

  int res;
  if (table->compLength<table->length) {
    res=otf_read_compressed_raw(otf->f,buf,table->offset,pad_len,table->compLength);
  } else {
    res=otf_read_raw(otf->f,buf,table->offset,pad_len);
  }
  if (res==pad_len) {
    return buf;
  } else if (res==table->length) {
    // (file/uncompressed) length is not a multiple of 4, fill missing padding with zero
    memset(buf+res,0,pad_len-res);
    return buf;
  } else if (res!=-1) {
    fprintf(stderr,"Short read\n");
  }
  free(ours);
  return NULL;
}
// }}}


static int otf_get_ttc_start(OTF_FILE *otf,int use_ttc) // {{{
{
  char buf[4];

  if (!otf->numTTC) { // >0 if TTC...
    return 0;
  }

  int pos=0;
  if ( (use_ttc<0)||(use_ttc>=otf->numTTC)||
       (!otf_read(otf,buf,pos+12+4*use_ttc,4)) ) {
    fprintf(stderr,"Bad TTC subfont number\n");
    return -1;
  }
  return get_ULONG(buf);
}
// }}}

static int otf_load_header(OTF_FILE *otf,int pos) // {{{
{
  char buf[12];
  if (!otf_read(otf,buf,pos,12)) {
    fprintf(stderr,"Not a ttf font\n");
    return -1;
  }
  otf->version=get_ULONG(buf);
  otf->numTables=get_USHORT(buf+4);
  // don't need the other fields...
  pos+=12;
  return pos;
}
// }}}

static int otf_load_ttc(OTF_FILE *otf,int use_ttc) // {{{
{
  char buf[12];
  if (!otf_read(otf,buf,0,12)) {
    fprintf(stderr,"Not a TTC font\n");
    return -1;
  }

  const unsigned int ttc_version=get_ULONG(buf+4);
  if ( (ttc_version!=0x00010000)&&(ttc_version!=0x00020000) ) {
    fprintf(stderr,"Unsupported TTC version\n");
    return -1;
  }
  otf->numTTC=get_ULONG(buf+8);
  otf->useTTC=use_ttc;

  const int pos=otf_get_ttc_start(otf,use_ttc);
  if (pos<0) {
    return -1;
  }
  return otf_load_header(otf,pos);
}
// }}}

static int otf_load_woff_header(OTF_FILE *otf) // {{{  returns file position  or -1
{
  char buf[44];
  if (!otf_read(otf,buf,0,44)) {
    fprintf(stderr,"Not a wOFF font\n");
    return -1;
  }
  if (get_USHORT(buf+14)!=0) {
    fprintf(stderr,"Unsupported wOFF font\n");
    return -1;
  }
  // buf[0] alread processed: wOFF
  otf->version=get_ULONG(buf+4);
  otf->woff.length=get_ULONG(buf+8);
  otf->numTables=get_USHORT(buf+12);
  otf->woff.origLength=get_ULONG(buf+16);
  otf->woff.majorVersion=get_USHORT(buf+20);
  otf->woff.minorVersion=get_USHORT(buf+22);
  otf->woff.metaOffset=get_ULONG(buf+24);
  otf->woff.metaLength=get_ULONG(buf+28);
  otf->woff.metaOrigLength=get_ULONG(buf+32);
  otf->woff.privOffset=get_ULONG(buf+36);
  otf->woff.privLength=get_ULONG(buf+40);
  otf->flags|=OTF_F_WOFF;
  otf->flags|=OTF_F_CACHE_GLYF;
  return 44;
}
// }}}

static int otf_load_directory(OTF_FILE *otf,int pos) // {{{
{
  int iA;
  char buf[16];

  for (iA=0;iA<otf->numTables;iA++) {
    if (!otf_read(otf,buf,pos,16)) {
      return -1;
    }
    otf->tables[iA].tag=get_ULONG(buf);
    otf->tables[iA].checkSum=get_ULONG(buf+4);
    otf->tables[iA].offset=get_ULONG(buf+8);
    otf->tables[iA].length=get_ULONG(buf+12);
    otf->tables[iA].compLength=otf->tables[iA].length;
    pos+=16;
  }
  return pos;
}
// }}}

static int otf_load_woff_directory(OTF_FILE *otf,int pos) // {{{
{
  int iA;
  char buf[20];

  for (iA=0;iA<otf->numTables;iA++) {
    if (!otf_read(otf,buf,pos,20)) {
      return -1;
    }
    otf->tables[iA].tag=get_ULONG(buf);
    otf->tables[iA].offset=get_ULONG(buf+4);
    otf->tables[iA].compLength=get_ULONG(buf+8);
    otf->tables[iA].length=get_ULONG(buf+12);
    otf->tables[iA].checkSum=get_ULONG(buf+16);
    pos+=20;
  }
  return pos;
}
// }}}

  // WOFF TODO?  global checksum
static int otf_do_load(OTF_FILE *otf,int pos) // {{{
{
  int iA;

  // {{{ check version
  if (otf->version==0x00010000) { // 1.0 truetype
  } else if (otf->version==OTF_TAG('O','T','T','O')) { // OTF(CFF)
    otf->flags|=OTF_F_FMT_CFF;
  } else if (otf->version==OTF_TAG('t','r','u','e')) { // (old mac)
  } else if (otf->version==OTF_TAG('t','y','p','1')) { // sfnt wrapped type1
    fprintf(stderr,"Unsupported ttf font (typ1)\n");
    return -1;
  } else {
    fprintf(stderr,"Not a ttf font (unknown version)\n");
    return -1;
  }
  // }}}

  // {{{ read directory
  otf->tables=malloc(sizeof(OTF_DIRENT)*otf->numTables);
  if (!otf->tables) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }
  if (otf->flags&OTF_F_WOFF) {
    pos=otf_load_woff_directory(otf,pos);
  } else {
    pos=otf_load_directory(otf,pos);
  }
  if (pos==-1) {
    return -1;
  }

  // check that opentype and truetype is not mixed
  for (iA=0;iA<otf->numTables;iA++) {
    if ( (otf->tables[iA].tag==OTF_TAG('C','F','F',' '))&&
         (!is_CFF(otf)) ) {
      fprintf(stderr,"Wrong magic\n");
      return -1;
    } else if ( (otf->tables[iA].tag==OTF_TAG('g','l','y','p'))&&
                (is_CFF(otf)) ) {
      fprintf(stderr,"Wrong magic\n");
      return -1;
    }
  }
  // }}}

//  otf->flags|=OTF_F_CACHE_GLYF; // i.e. always?
//  otf->flags|=OTF_F_DO_CHECKSUM;
  // {{{ check head table
  int len=0;
  char *head=otf_get_table(otf,OTF_TAG('h','e','a','d'),&len);
  if ( (!head)||
       (get_ULONG(head+0)!=0x00010000)||  // version
       (len!=54)||
       (get_ULONG(head+12)!=0x5F0F3CF5)|| // magic
       (get_SHORT(head+52)!=0x0000) ) {   // glyphDataFormat
    fprintf(stderr,"Unsupported OTF font / head table\n");
    free(head);
    return -1;
  }
  // }}}
  otf->unitsPerEm=get_USHORT(head+18);
  otf->indexToLocFormat=get_SHORT(head+50);

  // {{{ checksum whole file -- not for WOFF: checksum has to be calculated on UNCOMPRESSED content
  if ( (otf->flags&OTF_F_DO_CHECKSUM)&&((otf->flags&OTF_F_WOFF)==0) ) {
    unsigned int csum=0;
    char tmp[1024];
    rewind(otf->f);
    while (!feof(otf->f)) {
      len=fread(tmp,1,1024,otf->f);
      if (len&3) { // zero padding reqd.
        memset(tmp+len,0,4-(len&3));
      }
      csum+=otf_checksum(tmp,len);
    }
    if (csum!=0xb1b0afba) {
      fprintf(stderr,"Wrong global checksum\n");
      free(head);
      return -1;
    }
  }
  // }}}
  free(head);

  // {{{ read maxp table / numGlyphs
  char *maxp=otf_get_table(otf,OTF_TAG('m','a','x','p'),&len);
  if (maxp) {
    const unsigned int maxp_version=get_ULONG(maxp);
    if ( (maxp_version==0x00005000)&&(len==6)&&
         (is_CFF(otf)) ) { // version 0.5, only CFF
      otf->numGlyphs=get_USHORT(maxp+4);
    } else if ( (maxp_version==0x00010000)&&(len==32)&&
                (!is_CFF(otf)) ) { // version 1.0, only TTF
      otf->numGlyphs=get_USHORT(maxp+4);
    } else {
      fprintf(stderr,"Unsupported OTF font / maxp table\n");
      free(maxp);
      return -1;
    }
    free(maxp);
  }
  // }}}

  return pos;
}
// }}}

OTF_FILE *otf_load(const char *file) // {{{
{
  FILE *f;
  OTF_FILE *otf;

  int use_ttc=-1;
  if ((f=fopen(file,"rb"))==NULL) {
    // check for TTC
    char *tmp=strrchr(file,'/'),*end;
    if (tmp) {
      use_ttc=strtoul(tmp+1,&end,10);
      if (!*end) {
        end=malloc((tmp-file+1)*sizeof(char));
        if (!end) {
          fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
          return NULL;
        }
        strncpy(end,file,tmp-file);
        end[tmp-file]=0;
        f=fopen(end,"rb");
        free(end);
      }
    }
    if (!f) {
      fprintf(stderr,"Could not open \"%s\": %s\n", file, strerror(errno));
      return NULL;
    }
  }
  otf=otf_new(f);
  if (!otf) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    fclose(f);
    return NULL;
  }

  char buf[4];
  if (!otf_read(otf,buf,0,4)) {
    fprintf(stderr,"Not a ttf font / too short\n");
    otf_close(otf);
    return NULL;
  }
  const unsigned int version=get_ULONG(buf);

  int pos;
  if (version==OTF_TAG('w','O','F','F')) { // WOFF does not currently support TTC
    pos=otf_load_woff_header(otf);
  } else if (version==OTF_TAG('t','t','c','f')) { // TTC
    pos=otf_load_ttc(otf,use_ttc);
  } else {
    pos=otf_load_header(otf,0);
  }

  if (pos!=-1) {
    pos=otf_do_load(otf,pos);
  }

  if (pos==-1) {
    otf_close(otf);
    return NULL;
  }
  return otf;
}
// }}}

void otf_close(OTF_FILE *otf) // {{{
{
  assert(otf);
  if (otf) {
    if (otf->flags&OTF_F_CACHE_GLYF) {
      free(otf->glyf);
    } else {
      free(otf->gly);
    }
    free(otf->cmap);
    free(otf->name);
    free(otf->hmtx);
    free(otf->glyphOffsets);
    fclose(otf->f);
    free(otf->tables);
    free(otf);
  }
}
// }}}

static int otf_dirent_tag_compare(const void *a,const void *b) // {{{
{
  const unsigned int aa=((const OTF_DIRENT *)a)->tag;
  const unsigned int bb=((const OTF_DIRENT *)b)->tag;
  if (aa<bb) {
    return -1;
  } else if (aa>bb) {
    return 1;
  }
  return 0;
}
// }}}

int otf_find_table(OTF_FILE *otf,unsigned int tag) // {{{  - table_index  or -1 on error
{
#if 0
  // binary search would require raw table
  int pos=0;  // BUG: not for TTC/WOFF
  char buf[12];
  if (!otf_read(otf,buf,pos,12)) {
    return -1;
  }
  pos=12;
  const unsigned int numTables=get_USHORT(buf+4);
  char *tables=malloc(16*numTables);
  if (!tables) {
    return -1;
  }
  if (!otf_read(otf,tables,pos,16*numTables)) {
    free(tables);
    return -1;
  }
  char target[]={(tag>>24),(tag>>16),(tag>>8),tag};
  //  assert(get_USHORT(buf+6)+get_USHORT(buf+10)==16*numTables);
  char *result=otf_bsearch(tables,target,4,
                           get_USHORT(buf+6),
                           get_USHORT(buf+8),
                           get_USHORT(buf+10),0);
  free(tables);
  if (result) {
    return (result-tables)/16;
  }
#elif 1
  OTF_DIRENT key={.tag=tag},*res;
  res=bsearch(&key,otf->tables,otf->numTables,sizeof(otf->tables[0]),otf_dirent_tag_compare);
  if (res) {
    return (res-otf->tables);
  }
#else
  int iA;
  for (iA=0;iA<otf->numTables;iA++) {
    if (otf->tables[iA].tag==tag) {
      return iA;
    }
  }
#endif
  return -1;
}
// }}}

char *otf_get_table(OTF_FILE *otf,unsigned int tag,int *ret_len) // {{{
{
  assert(otf);
  assert(ret_len);

  const int idx=otf_find_table(otf,tag);
  if (idx==-1) {
    return NULL;
  }
  const OTF_DIRENT *table=otf->tables+idx;

  char *ret=otf_read_table(otf,NULL,table);
  if (!ret) {
    return NULL;
  }
  if (otf->flags&OTF_F_DO_CHECKSUM) {
    unsigned int csum=otf_checksum(ret,table->length);
    if (tag==OTF_TAG('h','e','a','d')) { // special case
      csum-=get_ULONG(ret+8);
    }
    if (csum!=table->checkSum) {
      fprintf(stderr,"Wrong checksum for %c%c%c%c\n",OTF_UNTAG(tag));
      free(ret);
      return NULL;
    }
  }
  *ret_len=table->length;
  return ret;
}
// }}}

int otf_load_glyf(OTF_FILE *otf) // {{{  - 0 on success
{
  assert(!is_CFF(otf)); // not for CFF
  int iA,len;
  // {{{ find glyf table
  iA=otf_find_table(otf,OTF_TAG('g','l','y','f'));
  if (iA==-1) {
    fprintf(stderr,"Unsupported OTF font / glyf table\n");
    return -1;
  }
  otf->glyfTable=otf->tables+iA;
  // }}}

  // {{{ read loca table
  char *loca=otf_get_table(otf,OTF_TAG('l','o','c','a'),&len);
  if ( (!loca)||
       (otf->indexToLocFormat>=2)||
       (otf_padded(len)!=otf_padded((otf->numGlyphs+1)*(otf->indexToLocFormat+1)*2)) ) {
    fprintf(stderr,"Unsupported OTF font / loca table\n");
    free(loca);
    return -1;
  }
  if (otf->glyphOffsets) {
    free(otf->glyphOffsets);
    assert(0);
  }
  otf->glyphOffsets=malloc((otf->numGlyphs+1)*sizeof(unsigned int));
  if (!otf->glyphOffsets) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }
  if (otf->indexToLocFormat==0) {
    for (iA=0;iA<=otf->numGlyphs;iA++) {
      otf->glyphOffsets[iA]=get_USHORT(loca+iA*2)*2;
    }
  } else { // indexToLocFormat==1
    for (iA=0;iA<=otf->numGlyphs;iA++) {
      otf->glyphOffsets[iA]=get_ULONG(loca+iA*4);
    }
  }
  free(loca);
  if (otf->glyphOffsets[otf->numGlyphs]>otf->glyfTable->length) {
    fprintf(stderr,"Bad loca table\n");
    return -1;
  }
  // }}}

  if (otf->glyf) {
    free(otf->glyf);
    assert(0);
    otf->glyf=NULL;
  }
  if (otf->flags&OTF_F_CACHE_GLYF) {
    otf->glyf=otf_get_table(otf,OTF_TAG('g','l','y','f'),&len);
    if (!otf->glyf) {
      return -1;
    }

    if (otf->gly) {
      free(otf->gly);
      assert(0);
      otf->gly=NULL;
    }
    return 0;
  }

  // {{{ allocate otf->gly slot
  int maxGlyfLen=0;  // no single glyf takes more space
  for (iA=1;iA<=otf->numGlyphs;iA++) {
    const int glyfLen=otf->glyphOffsets[iA]-otf->glyphOffsets[iA-1];
    if (glyfLen<0) {
      fprintf(stderr,"Bad loca table: glyph len %d\n",glyfLen);
      return -1;
    }
    if (maxGlyfLen<glyfLen) {
      maxGlyfLen=glyfLen;
    }
  }
  if (otf->gly) {
    free(otf->gly);
    assert(0);
  }
  otf->gly=malloc(maxGlyfLen*sizeof(char));
  if (!otf->gly) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }
  // }}}

  return 0;
}
// }}}

int otf_load_more(OTF_FILE *otf) // {{{  - 0 on success   => hhea,hmtx,name,[glyf]
{
  int iA;

  int len;
  if (!is_CFF(otf)) { // not for CFF
    if (otf_load_glyf(otf)==-1) {
      return -1;
    }
  }

  // {{{ read hhea table
  char *hhea=otf_get_table(otf,OTF_TAG('h','h','e','a'),&len);
  if ( (!hhea)||
       (get_ULONG(hhea)!=0x00010000)|| // version
       (len!=36)||
       (get_SHORT(hhea+32)!=0) ) { // metric format
    fprintf(stderr,"Unsupported OTF font / hhea table\n");
    free(hhea);
    return -1;
  }
  otf->numberOfHMetrics=get_USHORT(hhea+34);
  free(hhea);
  // }}}

  // {{{ read hmtx table
  char *hmtx=otf_get_table(otf,OTF_TAG('h','m','t','x'),&len);
  if ( (!hmtx)||
       (len!=otf->numberOfHMetrics*2+otf->numGlyphs*2) ) {
    fprintf(stderr,"Unsupported OTF font / hmtx table\n");
    free(hmtx);
    return -1;
  }
  if (otf->hmtx) {
    free(otf->hmtx);
    assert(0);
  }
  otf->hmtx=hmtx;
  // }}}

  // {{{ read name table
  char *name=otf_get_table(otf,OTF_TAG('n','a','m','e'),&len);
  if ( (!name)||
       (get_USHORT(name)!=0x0000)|| // version
       (len<get_USHORT(name+2)*12+6)||
       (len<=get_USHORT(name+4)) ) {
    fprintf(stderr,"Unsupported OTF font / name table \n");
    free(name);
    return -1;
  }
  // check bounds
  int name_count=get_USHORT(name+2);
  const char *nstore=name+get_USHORT(name+4);
  for (iA=0;iA<name_count;iA++) {
    const char *nrec=name+6+12*iA;
    if (nstore-name+get_USHORT(nrec+10)+get_USHORT(nrec+8)>len) {
      fprintf(stderr,"Bad name table \n");
      free(name);
      return -1;
    }
  }
  if (otf->name) {
    free(otf->name);
    assert(0);
  }
  otf->name=name;
  // }}}

  return 0;
}
// }}}

int otf_load_cmap(OTF_FILE *otf) // {{{  - 0 on success
{
  int iA;
  int len;

  char *cmap=otf_get_table(otf,OTF_TAG('c','m','a','p'),&len);
  if ( (!cmap)||
       (get_USHORT(cmap)!=0x0000)|| // version
       (len<get_USHORT(cmap+2)*8+4) ) {
    fprintf(stderr,"Unsupported OTF font / cmap table \n");
    free(cmap);
    assert(0);
    return -1;
  }
  // check bounds, find (3,0) or (3,1) [TODO?]
  const int numTables=get_USHORT(cmap+2);
  for (iA=0;iA<numTables;iA++) {
    const char *nrec=cmap+4+8*iA;
    const unsigned int offset=get_ULONG(nrec+4);
    const char *ndata=cmap+offset;
    if ( (ndata<cmap+4+8*numTables)||
         (offset>=len)||
         (offset+get_USHORT(ndata+2)>len) ) {
      fprintf(stderr,"Bad cmap table\n");
      free(cmap);
      assert(0);
      return -1;
    }
    if ( (get_USHORT(nrec)==3)&&
         (get_USHORT(nrec+2)<=1)&&
         (get_USHORT(ndata)==4)&&
         (get_USHORT(ndata+4)==0) ) {
      otf->unimap=ndata;
    }
  }
  if (otf->cmap) {
    free(otf->cmap);
    assert(0);
  }
  otf->cmap=cmap;

  return 0;
}
// }}}

int otf_get_width(OTF_FILE *otf,unsigned short gid) // {{{  -1 on error
{
  assert(otf);

  if (gid>=otf->numGlyphs) {
    return -1;
  }

  // ensure hmtx is there
  if (!otf->hmtx) {
    if (otf_load_more(otf)!=0) {
      assert(0);
      return -1;
    }
  }

  return get_width_fast(otf,gid);
#if 0
  if (gid>=otf->numberOfHMetrics) {
    return get_USHORT(otf->hmtx+(otf->numberOfHMetrics-1)*2);
    // TODO? lsb=get_SHORT(otf->hmtx+otf->numberOfHMetrics*2+gid*2);  // lsb: left_side_bearing (also in table)
  }
  return get_USHORT(otf->hmtx+gid*4);
  // TODO? lsb=get_SHORT(otf->hmtx+gid*4+2);
#endif
}
// }}}

static int otf_name_compare(const void *a,const void *b) // {{{
{
  return memcmp(a,b,8);
}
// }}}

const char *otf_get_name(OTF_FILE *otf,int platformID,int encodingID,int languageID,int nameID,int *ret_len) // {{{
{
  assert(otf);
  assert(ret_len);

  // ensure name is there
  if (!otf->name) {
    if (otf_load_more(otf)!=0) {
      *ret_len=-1;
      assert(0);
      return NULL;
    }
  }

  char key[8];
  set_USHORT(key,platformID);
  set_USHORT(key+2,encodingID);
  set_USHORT(key+4,languageID);
  set_USHORT(key+6,nameID);

  char *res=bsearch(key,otf->name+6,get_USHORT(otf->name+2),12,otf_name_compare);
  if (res) {
    *ret_len=get_USHORT(res+8);
    int npos=get_USHORT(res+10);
    const char *nstore=otf->name+get_USHORT(otf->name+4);
    return nstore+npos;
  }
  *ret_len=0;
  return NULL;
}
// }}}

int otf_get_glyph(OTF_FILE *otf,unsigned short gid) // {{{ result in >otf->gly, returns length, -1 on error
{
  assert(otf);
  assert(!is_CFF(otf)); // not for CFF

  if (gid>=otf->numGlyphs) {
    return -1;
  }

  // ensure >glyphOffsets is there
  if (!otf->glyphOffsets) {
    if (otf_load_more(otf)!=0) {
      assert(0);
      return -1;
    }
  }

  const int len=otf->glyphOffsets[gid+1]-otf->glyphOffsets[gid];
  if (len==0) {
    return 0;
  }

  assert(otf->glyfTable->length>=otf->glyphOffsets[gid+1]);
  if (otf->glyf) { // i.e. OTF_F_CACHE_GLYF
    otf->gly=otf->glyf+otf->glyphOffsets[gid];
  } else if (otf->gly) {
    if (otf->flags&OTF_F_WOFF) {
      fprintf(stderr,"OTF_F_CACHE_GLYF is needed to read glyphs from wOFF files\n");
      return -1;
    }
    if (!otf_read_raw(otf->f,otf->gly,
                  otf->glyfTable->offset+otf->glyphOffsets[gid],len)) {
      return -1;
    }
  } else {
    assert(0);
    return -1;
  }

  return len;
}
// }}}

unsigned short otf_from_unicode(OTF_FILE *otf,int unicode) // {{{ 0 = missing
{
  assert(otf);
  assert( (unicode>=0)&&(unicode<65536) );
//  assert(!is_CFF(otf)); // not for CFF, other method!

  // ensure >cmap and >unimap is there
  if (!otf->cmap) {
    if (otf_load_cmap(otf)!=0) {
      assert(0);
      return 0; // TODO?
    }
  }
  if (!otf->unimap) {
    fprintf(stderr,"Unicode (3,1) cmap in format 4 not found\n");
    return 0;
  }

#if 0
  // linear search is cache friendly and should be quite fast
#else
  const unsigned short segCountX2=get_USHORT(otf->unimap+6);
  char target[]={unicode>>8,unicode}; // set_USHORT(target,unicode);
  char *result=otf_bsearch((char *)otf->unimap+14,target,2,
                           get_USHORT(otf->unimap+8),
                           get_USHORT(otf->unimap+10),
                           get_USHORT(otf->unimap+12),1);
  if (result>=otf->unimap+14+segCountX2) { // outside of endCode[segCount]
    assert(0); // bad font, no 0xffff sentinel
    return 0;
  }

  result+=2+segCountX2; // jump over padding into startCode[segCount]
  const unsigned short startCode=get_USHORT(result);
  if (startCode>unicode) {
    return 0;
  }
  result+=2*segCountX2;
  const unsigned short rangeOffset=get_USHORT(result);
  if (rangeOffset) {
    return get_USHORT(result+rangeOffset+2*(unicode-startCode)); // the so called "obscure indexing trick" into glyphIdArray[]
    // NOTE: this is according to apple spec; microsoft says we must add delta (probably incorrect; fonts probably have delta==0)
  } else {
    const short delta=get_SHORT(result-segCountX2);
    return (delta+unicode)&0xffff;
  }
#endif
}
// }}}

