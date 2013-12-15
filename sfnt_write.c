#include "sfnt.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "sfnt_int.h"

#ifdef WOFF_SUPPORT
  #include <zlib.h>
#endif


static void otf_bsearch_params(int num, // {{{
                               int recordSize,
                               int *searchRange,
                               int *entrySelector,
                               int *rangeShift)
{
  assert(num>0);
  assert(searchRange);
  assert(entrySelector);
  assert(rangeShift);

  int iA,iB;
  for (iA=1,iB=0;iA<=num;iA<<=1,iB++) {}

  *searchRange=iA*recordSize/2;
  *entrySelector=iB-1;
  *rangeShift=num*recordSize-*searchRange;
}
// }}}

static char *otf_write_compressed_raw(const char *in,unsigned int length,unsigned long *compLength) // {{{
{
  assert(compLength);
#ifdef WOFF_SUPPORT
  *compLength=compressBound(length); // known to be > otf_padded(length)
  char *tmp=malloc(sizeof(char)*(*compLength));
  if (!tmp) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    *compLength=-1;
    return NULL;
  }
  if (compress2((unsigned char *)tmp,compLength,(const unsigned char *)in,length,9)!=Z_OK) {
    fprintf(stderr,"Compression failed\n");
    free(tmp);
    *compLength=-1;
    return NULL;
  }
  if (*compLength<length) {
    return tmp; // compressed
  }
  free(tmp);
#endif
  *compLength=length; // uncompressed
  return NULL;
}
// }}}


/* windows "works best" with the following ordering:
  head, hhea, maxp, OS/2, hmtx, LTSH, VDMX, hdmx, cmap, fpgm, prep, cvt, loca, glyf, kern, name, post, gasp, PCLT, DSIG
or for CFF:
  head, hhea, maxp, OS/2, name, cmap, post, CFF, (other tables, as convenient)
*/
#define NUM_PRIO 20
static const struct { int prio; unsigned int tag; } otf_tagorder_win[]={ // {{{
  {19,OTF_TAG('D','S','I','G')},
  { 5,OTF_TAG('L','T','S','H')},
  { 3,OTF_TAG('O','S','/','2')},
  {18,OTF_TAG('P','C','L','T')},
  { 6,OTF_TAG('V','D','M','X')},
  { 8,OTF_TAG('c','m','a','p')},
  {11,OTF_TAG('c','v','t',' ')},
  { 9,OTF_TAG('f','p','g','m')},
  {17,OTF_TAG('g','a','s','p')},
  {13,OTF_TAG('g','l','y','f')},
  { 7,OTF_TAG('h','d','m','x')},
  { 0,OTF_TAG('h','e','a','d')},
  { 1,OTF_TAG('h','h','e','a')},
  { 4,OTF_TAG('h','m','t','x')},
  {14,OTF_TAG('k','e','r','n')},
  {12,OTF_TAG('l','o','c','a')},
  { 2,OTF_TAG('m','a','x','p')},
  {15,OTF_TAG('n','a','m','e')},
  {16,OTF_TAG('p','o','s','t')},
  {10,OTF_TAG('p','r','e','p')}};
// }}}

void otf_tagorder_win_sort(const struct _OTF_WRITE_TABLE *tables,int *order,int numTables) // {{{
{
  int priolist[NUM_PRIO]={0,};

  // reverse intersection of both sorted arrays
  int iA=numTables-1,iB=sizeof(otf_tagorder_win)/sizeof(otf_tagorder_win[0])-1;
  int ret=numTables-1;
  while ( (iA>=0)&&(iB>=0) ) {
    if (tables[iA].tag==otf_tagorder_win[iB].tag) {
      priolist[otf_tagorder_win[iB--].prio]=1+iA--;
    } else if (tables[iA].tag>otf_tagorder_win[iB].tag) { // no order known: put unchanged at end of result
      order[ret--]=iA--;
    } else { // <
      iB--;
    }
  }
  for (iA=NUM_PRIO-1;iA>=0;iA--) { // pick the matched tables up in sorted order (bucketsort principle)
    if (priolist[iA]) {
      order[ret--]=priolist[iA]-1;
    }
  }
}
// }}}

void otf_tagorder_tag_sort(const struct _OTF_WRITE_TABLE *tables,int *order,int numTables) // {{{
{
  int iA;
  for (iA=0;iA<numTables;iA++) {
    order[iA]=iA; // tables is already (must be) tag sorted
  }
}
// }}}

struct _OTF_KV {
  unsigned int key;
  unsigned int value;
};

static int otf_kv_compare(const void *a,const void *b) // {{{
{
  const unsigned int aa=((const struct _OTF_KV *)a)->key;
  const unsigned int bb=((const struct _OTF_KV *)b)->key;
  if (aa<bb) {
    return -1;
  } else if (aa>bb) {
    return 1;
  }
  return 0;
}
// }}}

void otf_tagorder_offset_sort(const OTF_FILE *otf,int *order) // {{{
{
  int iA;
  struct _OTF_KV tmp[otf->numTables]; // or tmp=malloc(sizeof(struct _OTF_KV)*otf->numTables); assert(tmp);
  for (iA=0;iA<otf->numTables;iA++) {
    tmp[iA].key=otf->tables[iA].offset;
    tmp[iA].value=iA;
  }
  qsort(tmp,otf->numTables,sizeof(struct _OTF_KV),otf_kv_compare); // qsort_r / qsort_s makes portability hard ...
  for (iA=0;iA<otf->numTables;iA++) {
    order[iA]=tmp[iA].value;
  }
}
// }}}


static void action_free(struct _OTF_WRITE_TABLE *self) // {{{
{
  assert(self);
  free(self->info.data);
  self->info.data=NULL;
}
// }}}

static void action_nop(struct _OTF_WRITE_TABLE *self) // {{{
{
}
// }}}

// TODO? copy_block(otf->f,table->offset,otf_padded(table->length),output,context);
// problem: PS currently depends on single-output.  also checksum not possible
static int action_copy_load(struct _OTF_WRITE_TABLE *self) // {{{
{
  OTF_FILE *otf=self->args.copy.otf;
  const OTF_DIRENT *table=otf->tables+self->args.copy.table_no;

  char *data=otf_read_table(otf,NULL,table);
  if (!data) {
    return -1;
  }

  assert(!self->info.data);
  self->info.data=data;
  return 0;
}
// }}}

static int action_hlp_compress(struct _OTF_WRITE_TABLE *self) // {{{
{
  unsigned long compLength;
  char *comp=otf_write_compressed_raw(self->info.data,self->info.table.length,&compLength);
  if (comp) { // compressed
    (*self->info.free)(self);
    if (otf_padded(compLength)>compLength) { // clear padding bytes
      memset(comp+compLength,0,otf_padded(compLength)-compLength);
    }
    self->info.data=comp;
    self->info.table.compLength=compLength;
    self->info.free=action_free;
  } else if (compLength==-1) { // error
    (*self->info.free)(self);
    return -1;
  } // else: leave uncompressed
  return 0;
}
// }}}

static int action_load_data(struct _OTF_WRITE_TABLE *self) // {{{
{
  if (!self->info.data) {
    assert(self->info.load);
    if (!self->info.load) {
      return -1;
    }
    const int res=(*self->info.load)(self);
    if (res<0) {
      return -1;
    }
  }
  return 0;
}
// }}}

void otf_action_copy(struct _OTF_WRITE_TABLE *self) // {{{
{
  OTF_FILE *otf=self->args.copy.otf;
  const OTF_DIRENT *table=otf->tables+self->args.copy.table_no;

  // get checksum and unpadded length
  self->info.table.tag=self->tag;
  self->info.table.checkSum=table->checkSum;
  self->info.table.offset=0; // later
  self->info.table.length=table->length; // unpadded
  self->info.table.compLength=table->length; // uncompressed

  self->info.data=NULL;
  self->info.load=action_copy_load;
  self->info.free=action_free;
}
// }}}

void otf_action_replace(struct _OTF_WRITE_TABLE *self) // {{{
{
  char *data=self->args.replace.data;
  int length=self->args.replace.length;

  // get checksum and unpadded length
  self->info.table.tag=self->tag;
  self->info.table.checkSum=otf_checksum(data,otf_padded(length));
  self->info.table.offset=0; // later
  self->info.table.length=length; // unpadded
  self->info.table.compLength=length; // uncompressed

  assert(data);
  if (otf_padded(length)>length) { // clear padding bytes
    memset(data+length,0,otf_padded(length)-length);
  }
  self->info.data=data;
  self->info.load=NULL;
  self->info.free=action_nop;
}
// }}}


/*
struct {
  struct _OTF_WRITE_INFO otw;

  const struct _OTF_WRITE_WOFF *woff;

  OUTPUT_FN output;
  void *context;

// internal
  // sortingMode
int *order; // ?? better as param   // but this allows sorting "beforehand"
} OTF_WRITE_SFNT;
*/

static int otf_write_header(const struct _OTF_WRITE_INFO *otw,char *out) // {{{
{
  // header
  set_ULONG(out,otw->version);
  set_USHORT(out+4,otw->numTables);
  int a,b,c;
  otf_bsearch_params(otw->numTables,16,&a,&b,&c);
  set_USHORT(out+6,a);
  set_USHORT(out+8,b);
  set_USHORT(out+10,c);

  // directory
  int ret=12,iA;
  for (iA=0;iA<otw->numTables;iA++) {
    const OTF_DIRENT *table=&otw->tables[iA].info.table;

    set_ULONG(out+ret,table->tag);
    set_ULONG(out+ret+4,table->checkSum);
    set_ULONG(out+ret+8,table->offset);
    set_ULONG(out+ret+12,table->length);
    ret+=16;
  }
  return ret;
}
// }}}

static int otf_prepare_woff_header(OTF_WOFF_HEADER *hdr,struct _OTF_WRITE_WOFF *woff,unsigned int origLength,unsigned int sfntCompLength) // {{{
{
  hdr->origLength=origLength;
  hdr->majorVersion=woff->majorVersion;
  hdr->minorVersion=woff->minorVersion;

  if ( (woff->metaData)&&(woff->metaLength>0) ) {
    unsigned long compLength;
    woff->metaCompData=otf_write_compressed_raw(woff->metaData,woff->metaLength,&compLength);
    if ( (!woff->metaCompData)&&(compLength==-1) ) { // error
      return -1;
    }
    hdr->metaOffset=sfntCompLength;
    hdr->metaLength=compLength;
    hdr->metaOrigLength=woff->metaLength;
  } else {
    hdr->metaOffset=0;
    hdr->metaLength=0;
    hdr->metaOrigLength=0;
  }
  if ( (woff->privData)&&(woff->privLength>0) ) {
    hdr->privOffset=sfntCompLength+otf_padded(hdr->metaLength);
    hdr->privLength=woff->privLength;
  } else {
    hdr->privOffset=0;
    hdr->privLength=0;
  }

  hdr->length=sfntCompLength+otf_padded(hdr->metaLength)+hdr->privLength;
  return 0;
}
// }}}

static int otf_write_woff_header(const struct _OTF_WRITE_INFO *otw,const OTF_WOFF_HEADER *woff,OUTPUT_FN output,void *context) // {{{
{
  char out[44];

  // header
  set_ULONG(out,OTF_TAG('w','O','F','F'));
  set_ULONG(out+4,otw->version);
  set_ULONG(out+8,woff->length);
  set_USHORT(out+12,otw->numTables);
  set_USHORT(out+14,0); // reserved, must be 0

  set_ULONG(out+16,woff->origLength);
  set_USHORT(out+20,woff->majorVersion);
  set_USHORT(out+22,woff->minorVersion);
  set_ULONG(out+24,woff->metaOffset);
  set_ULONG(out+28,woff->metaLength);
  set_ULONG(out+32,woff->metaOrigLength);
  set_ULONG(out+36,woff->privOffset);
  set_ULONG(out+30,woff->privLength);

  (*output)(out,44,context);

  // directory
  int ret=44,iA;
  for (iA=0;iA<otw->numTables;iA++) {
    const OTF_DIRENT *table=&otw->tables[iA].info.table;

    set_ULONG(out,table->tag);
    set_ULONG(out+4,table->offset);
    set_ULONG(out+8,table->compLength);
    set_ULONG(out+12,table->length);
    set_ULONG(out+16,table->checkSum);

    (*output)(out,20,context);
    ret+=20;
  }

  return ret;
}
// }}}

static int otf_write_woff_footer(struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context) // {{{
{
  int ret=0;
  if (woff->metaCompData) {
    (*output)(woff->metaCompData,woff->metaLength,context);
    ret+=woff->metaLength;
  } else if (woff->metaData) {
    (*output)(woff->metaData,woff->metaLength,context);
    ret+=woff->metaLength;
  }
  const int extrapad=otf_padded(woff->metaLength)-woff->metaLength;
  if (extrapad) {
    const char pad[4]={0,0,0,0};
    (*output)(pad,extrapad,context);
    ret+=extrapad;
  }
  if (woff->privData) {
    (*output)(woff->privData,woff->privLength,context);
    ret+=woff->privLength;
  }
  return ret;
}
// }}}

// will change otw!
int otf_write_sfnt(struct _OTF_WRITE_INFO *otw,struct _OTF_WRITE_WOFF *woff,OUTPUT_FN output,void *context) // {{{
{
  int iA;
  const int sfntStartSize=12+16*otw->numTables;

  int *order=malloc(sizeof(int)*otw->numTables); // temporary
  char *sfntStart=malloc(sfntStartSize);
  if ( (!order)||(!sfntStart) ) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    free(order);
    free(sfntStart);
    return -1;
  }

  if (1) { // sort tables
    otf_tagorder_win_sort(otw->tables,order,otw->numTables);
  } else {
    otf_tagorder_tag_sort(otw->tables,order,otw->numTables);
  }

  // first pass: calculate table directory / offsets and checksums
  unsigned int globalSum=0;
  int offset=sfntStartSize;
  int headAt=-1;
  for (iA=0;iA<otw->numTables;iA++) {
    struct _OTF_WRITE_TABLE *self=&otw->tables[order[iA]];

    if (self->tag==OTF_TAG('h','e','a','d')) {
      headAt=order[iA];
    }

    (*self->action)(self);
    self->info.table.offset=offset;

    offset+=otf_padded(self->info.table.length);
    globalSum+=self->info.table.checkSum;
  }

  // the sfnt header
  const int res=otf_write_header(otw,sfntStart);
  assert(res==sfntStartSize);
  globalSum+=otf_checksum(sfntStart,sfntStartSize);

  int ret=0;
  if (!woff) { // simple sfnt
    // write header + directory
    ret=sfntStartSize;
    (*output)(sfntStart,ret,context);
  }
  free(sfntStart);

  // fix head table checksum
  if (headAt!=-1) {
    struct _OTF_WRITE_TABLE *self=&otw->tables[headAt];
    if (action_load_data(self)!=0) {
      ret=-1;
      goto done;
    }
    set_ULONG(self->info.data+8,0xb1b0afba-globalSum); // fix global checksum
    // TODO? >modify head time-stamp?
  }

  // pass 1.5: compress data, write woff header
  if (woff) {
    const int origLength=offset;
    const int woffStartSize=44+20*otw->numTables;

    offset=woffStartSize;
    for (iA=0;iA<otw->numTables;iA++) {
      struct _OTF_WRITE_TABLE *self=&otw->tables[order[iA]];
      if ( (action_load_data(self)!=0)||
           (action_hlp_compress(self)!=0) ) {
        ret=-1;
        goto done;
      }
      self->info.table.offset=offset;
      offset+=otf_padded(self->info.table.compLength);
    }

    OTF_WOFF_HEADER woffHdr;
    if (otf_prepare_woff_header(&woffHdr,woff,origLength,offset)!=0) {
      ret=-1;
      goto done;
    }
    offset=woffHdr.length;
    ret=otf_write_woff_header(otw,&woffHdr,output,context);
    assert(ret==woffStartSize);
  }

  // second pass: write tables
  for (iA=0;iA<otw->numTables;iA++) {
    struct _OTF_WRITE_TABLE *self=&otw->tables[order[iA]];
    if (action_load_data(self)!=0) {
      ret=-1;
      goto done;
    }

    const int pad_len=otf_padded(self->info.table.compLength);
    (*output)(self->info.data,pad_len,context);

    (*self->info.free)(self);
    ret+=pad_len;
  }

  if (woff) {
    ret+=otf_write_woff_footer(woff,output,context);
  }
  assert(ret==offset);

done:
  if (ret==-1) {
    for (iA=0;iA<otw->numTables;iA++) {
      (*otw->tables[iA].info.free)(&otw->tables[iA]);
    }
  }
  if (woff) {
    free(woff->metaCompData);
    woff->metaCompData=NULL;
  }
  free(order);
  return ret;
}
// }}}

// TODO: can be removed
int otf_write_woff(struct _OTF_WRITE_INFO *otw,const char *metaData,unsigned int metaLength,const char *privData,unsigned int privLength,OUTPUT_FN output,void *context) // {{{
{
  struct _OTF_WRITE_WOFF woff;

  woff.majorVersion=0; // TODO?
  woff.minorVersion=0;
  woff.metaData=metaData;
  woff.metaLength=metaLength;
  woff.privData=privData;
  woff.privLength=privLength;
  woff.metaCompData=NULL;

  return otf_write_sfnt(otw,&woff,output,context);
}
// }}}

