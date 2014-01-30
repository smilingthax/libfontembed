#include "cff.h"
#include "cff_int.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// NOTE: only support Type 2 charstrings for now

// TODO cff_read_dict
  // TODO:   add ,maxlen to read_integer(?), read_real and read_dict_token   ... write_?
  // cff_dict_get(dict,key...)

  // TODO? cff_write_dict,  cff_add_to_dict(?)

//   DICT:  only two, privateDict and topDict.

//  - get_from_dict(dict*,operator data structure)  // has to return default, if not there. TODO?! check/assert if op is valid for this dict?

//  ? cff_index_realloc(count)  cff_index_add_data(size?) [? only for r/w data]

#include <math.h>
double cff_read_real(const char **buf) // {{{   NAN on error
{
  int iA,iB=0;

  char tmp[250];
  while (1) {
    const unsigned char b=**buf;
    const unsigned char n1=b&0x0f;
    const unsigned char n[2]={b>>4, n1};
    for (iA=0;iA<2;iA++) {
      switch (n[iA]) {
      case 0x0a:
        tmp[iB++]='.';
        break;
      case 0x0b:
        tmp[iB++]='E';
        tmp[iB++]='+';
        break;
      case 0x0c:
        tmp[iB++]='E';
        tmp[iB++]='-';
        break;
      case 0x0d:
        assert(0);
        break;
      case 0x0e:
        tmp[iB++]='-';
        break;
      case 0x0f:
        tmp[iB++]=0;
        break;
      default: // 0-9
        tmp[iB++]=n[iA]+'0';
        break;
      }
    }
    (*buf)++;
    assert(iB<200);
    if (iB>=200) {
      return NAN;
    }
    if (n1==0xf) {
      break;
    }
  }
  char *end;
  double ret=strtod(tmp,&end);
  assert(!*end);
  if (*end) {
    return NAN;
  }
  return ret;
}
// }}}

// writes max 10 chars
void cff_write_real(char **buf,double value) // {{{
{
  char tmp[20];
  int res=snprintf(tmp,250,"%G",value);
  assert(res<15);
  int iA,pos=0;
  for (iA=0;iA<res;iA++) {
    char t=tmp[iA];
    if ( (t>='0')&&(t<='9') ) {
      t-='0';
    } else if (t=='-') {
      t=0xe;
    } else if (t=='.') {
      t=0xa;
    } else if (t=='E') {
      if (tmp[iA+1]=='-') {
        iA++;
        t=0xc;
      } else if (tmp[iA+1]=='+') {
        iA++;
        t=0xb;
      } else { // snprintf does not produce this...
        assert(0);
        t=0xb;
      }
      if (tmp[iA+1]=='0') {
        iA++;
      }
    } else {
      assert(0);
      break;
    }
    if (pos==0) {
      (**buf)=t<<4;
    } else {
      *((*buf)++)|=t;
    }
    pos^=1;
  }
  // end marker
  if (pos==0) {
    *((*buf)++)=0xff;
  } else {
    *((*buf)++)|=0xf;
  }
}
// }}}

int32_t cff_read_integer(const char **buf) // {{{
{
  const char *b=*buf;
  const unsigned char b0=*b;
  if (b0==28) { // size 3
    (*buf)+=3;
    return (b[1]<<8)|
           (unsigned char)b[2];
  } else if (b0==29) { // size 5
    (*buf)+=5;
    return (b[1]<<24)|
           ((unsigned char)b[2]<<16)|
           ((unsigned char)b[3]<<8)|
           (unsigned char)b[4];
  } else if (b0<32) {
    // 30 real number, 31 reserved, <28 opval/reserved
  } else if (b0<247) { // size 1
    (*buf)++;
    return (int)b0-139;
  } else if (b0<251) { // size 2
    (*buf)+=2;
    return (b0-247)*256+(unsigned char)b[1]+108;
  } else if (b0<255) { // size 2
    (*buf)+=2;
    return -(b0-251)*256-(unsigned char)b[1]-108;
  } // else: 255 reserved
  assert(0);
  return 0;
}
// }}}

#include "cff_tables.h"

// error: .type==CFF_VAL_ERROR
struct _CFF_VALUE cff_read_dict_token(const char **buf) // {{{
{
  struct _CFF_VALUE ret;
  const char *b=*buf;
  const unsigned char b0=*b;
  if (b0<22) { // operator, size 1 or size 2
    int opvalue=b0;
    (*buf)++;
    if (opvalue==12) {
      opvalue=(opvalue<<8)|((unsigned char)b[1]);
      (*buf)++;
    }
    ret.op=cffop_get_by_value(opvalue);
    if (!ret.op) {
      ret.type=CFF_VAL_ERROR;
    } else {
      ret.type=CFF_VAL_OP;
    }
  } else if (b0==30) { // real, varsize
    (*buf)++;
    ret.real=cff_read_real(buf);
    if (isnan(ret.real)) {
      ret.type=CFF_VAL_ERROR;
    } else {
      ret.type=CFF_VAL_REAL;
    }
  } else if ( (b0>=28)&&(b0!=31)&&(b0<=254) ) {
    ret.type=CFF_VAL_INT;
    ret.number=cff_read_integer(buf);
  } else { // 22-27,31,255 reserved
    assert(0);
    ret.type=CFF_VAL_ERROR;
  }
  return ret;
}
// }}}

void cff_write_integer(char **buf,int32_t value) // {{{
{
  char *b=*buf;
  if ( (value>=-107)&&(value<=107) ) { // size 1
    b[0]=value+139;
    (*buf)++;
  } else if ( (value>=108)&&(value<=1131) ) { // size 2
    value-=108;
    b[0]=(value>>8)+247;
    b[1]=value&0xff;
    (*buf)+=2;
  } else if ( (value>=-1131)&&(value<=-108) ) { // size 2
    value=-value-108;
    b[0]=(value>>8)+251;
    b[1]=value&0xff;
    (*buf)+=2;
  } else if ( (value>=-32768)&&(value<=32767) ) { // size 3
    b[0]=28;
    b[1]=(value>>8)&0xff;
    b[2]=value&0xff;
    (*buf)+=3;
  } else { // size 5
    b[0]=29;
    b[1]=(value>>24)&0xff;
    b[2]=(value>>16)&0xff;
    b[3]=(value>>8)&0xff;
    b[4]=value&0xff;
    (*buf)+=5;
  }
}
// }}}

void cff_write_dict_token(char **buf,const struct _CFF_VALUE *value) // {{{
{
  assert(value);
  switch (value->type) {
  case CFF_VAL_ERROR:
  default:
    assert(0);
    return;
  case CFF_VAL_OP:
    if (value->op->value>=0x0c00) {
      *((*buf)++)=12;
      *((*buf)++)=(value->op->value)&0xff;
    } else {
      *((*buf)++)=value->op->value;
    }
    break;
  case CFF_VAL_INT:
    cff_write_integer(buf,value->number);
    break;
  case CFF_VAL_REAL:
    *((*buf)++)=30;
    cff_write_real(buf,value->real);
    break;
  }
}
// }}}

// TODO: length check
// TODO: basic type check (SID must be int<65000, etc...)
// TODO? verify dict type?
// TODO: verify empty stack at the end (last token is an op)
struct _CFF_DICT *cff_read_dict(const char *start,int length) // {{ {
{
  assert(start);
  const char *end=start+length;

  struct _CFF_DICT *ret=malloc(sizeof(struct _CFF_DICT)+20*sizeof(struct _CFF_VALUE));
  if (!ret) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return NULL;
  }
  ret->alloc=20;
  ret->len=0;
  while (start<end) {
    if (ret->len>=ret->alloc) {
      ret->alloc+=20;
      struct _CFF_DICT *tmp=realloc(ret,sizeof(struct _CFF_DICT)+ret->alloc*sizeof(struct _CFF_VALUE));
      if (!tmp) {
        fprintf(stderr,"Bad realloc: %s\n", strerror(errno));
        free(ret);
        return NULL;
      }
      ret=tmp;
    }
// FIXME:  length check
    ret->data[ret->len]=cff_read_dict_token(&start);
    if (ret->data[ret->len].type==CFF_VAL_ERROR) {
      fprintf(stderr,"Bad DICT token\n");
      free(ret);
      return NULL;
    }
    ret->len++;
  }
  return ret;
}
// }} }

  // Dict: (const char *dictStart,int length)
  //   maybe load into better datastructure,  either a general ADT, or two specific ADTs, one for Top Dict and one for Private Dict
  // general dict is easier (to populate)  ... but we're in plain C
    // idea: just { int key(op value) [or even CFF_OPS *]; union { ...value... } }[]   and qsort AFTER populating it  -> still fast lookup
const char *cff_get_topdict(/*???topdict[fontname]???,*/const char *key)
{
  const struct CFF_OPS *res=cffop_get_by_name(key);
  if (!res) {
    assert(0);
    return NULL;
  }
  // find
/*
  int last=0;
  for (int iB=0;iB<topdict.size();iB++) {
    if (topdict[iB]==12) {
      iB++;
      if ( (opnames[iA].value>21)&&
           (topdict[iB]==(opnames[iA].value&0xff)) ) { // found
        return topdict+last;
      }
      last=iB+1;
    } else if (topdict[iB]<22) {
      if (topdict[iB]==opnames[iA].value) { // found
        return topdict+last;
      }
      last=iB+1;
    }
  }
*/
  return res->defval;  // or NULL
}

const char *cff_read_privateDICT(const char *key)
{
return NULL;
}

void cff_load()
{
  // Name INDEX
//  names=cff_read_INDEX;
//  assert(names.count>=1);     // NOTE: .count==1  MUST for OTF
//  ... *names[iA]==0 -> deleted

  // Top DICT INDEX
//  topdict=cff_read_INDEX;
//  ... topdict[iA] for names[iA]...
//  td=cff_read_DICT(topdict[iA]);
}

/*

Free:
Encodings
Charsets
FDSelect  // only CIF
CharString INDEX
Font DICT INDEX // CID   (=per-font Private DICT)
Private DICT
Local Subr INDEX
Copyright and Trademark

*/


const char *cff_get_string(CFF_FILE *cff,int sid,int *retlen) // {{{  or NULL
{
  assert(cff);
  assert( (sid>=0)&&(sid<=65000) );
  if (sid<=390) {
    const char *ret=cff_get_stdstr(sid);
    if (retlen) {
      *retlen=strlen(ret);
    }
    return ret;
  }
  sid-=391;
  if (cff->stringIndex.count==0) {
    return NULL;
  }
  assert(cff->stringIndex.offSize!=OFFERR);
  if (retlen) {
    *retlen=cff_index_length(&cff->stringIndex,sid);
  }
  return cff_index_get(&cff->stringIndex,sid);
}
// }}}

/* --- */

static void cff_index_free_data(struct CFF_Index *idx) // {{{
{
  assert(idx);
  if (idx) {
    free(idx->offset);
  }
}
// }}}

static int cff_load_index(CFF_FILE *cff,int pos,struct CFF_Index *idx) // {{{
{
  assert(idx);
  assert(idx->count==0); // not already initialized

  idx->offset=NULL;
  if (pos+2>=cff->length) {
    return -1;
  }
  const char *buf=cff->font+pos;
  idx->count=cff_read_Card16(&buf);
  if (idx->count==0) {
    return buf-cff->font; // pos+2
  }

  if (pos+3+idx->count+1>=cff->length) {
    return -1;
  }
  idx->offSize=cff_read_OffSize(&buf);
  if (idx->offSize==OFFERR) {
    return -1;
  } else if (pos+3+(idx->count+1)*idx->offSize>=cff->length) {
    return -1;
  }

  idx->offset=malloc((idx->count+1)*sizeof(*idx->offset));
  if (!idx->offset) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return -1;
  }

  uint32_t last=1;
  int iA;
  const int len=idx->count+1;
  for (iA=0;iA<len;iA++) {
    const uint32_t res=cff_read_Offset(&buf,idx->offSize);
    if (res<last) {
      free(idx->offset);
      idx->offset=NULL;
      fprintf(stderr,"Bad INDEX\n");
      return -1;
    }
    last=res;
    idx->offset[iA]=res;
  }

  idx->dataStart=buf;

  // pos+3+(idx->count+1)*idx->offSize+last-1;
  const int ret=buf+last-1-cff->font;
  if (ret>cff->length) { // == is ok (file ends directly after us)
    return -1;
  }
  return ret;
}
// }}}


static int cff_read_header(CFF_FILE *cff) // {{{
{
  const char *buf=cff->font;
  if (cff->length<3) {
    fprintf(stderr,"Not a cff font / too short\n");
    return -1;
  }
  cff->majorVersion=cff_read_Card8(&buf);
  cff->minorVersion=cff_read_Card8(&buf);
  uint8_t hdrSize=cff_read_Card8(&buf);
  cff->absOffSize=cff_read_OffSize(&buf);

  if (hdrSize<4) {
    fprintf(stderr,"cff header not recognized\n");
    return -1;
  }
  if (cff->majorVersion!=1) {
    fprintf(stderr,"Unsupported cff font version\n");
    return -1;
  }
  assert(buf-cff->font<=hdrSize);
  return hdrSize;
}
// }}}

// TODO?
static int cff_do_load(CFF_FILE *cff,int pos) // {{{
{
  assert(cff);

  pos=cff_load_index(cff,pos,&cff->nameIndex);
  if (pos<0) {
    fprintf(stderr,"Could not load Name INDEX\n");
    return -1;
  }

  pos=cff_load_index(cff,pos,&cff->topDictIndex);
  if (pos<0) {
    fprintf(stderr,"Could not load Top Dict INDEX\n");
    return -1;
  }

  pos=cff_load_index(cff,pos,&cff->stringIndex);
  if (pos<0) {
    fprintf(stderr,"Could not load String INDEX\n");
    return -1;
  }

  pos=cff_load_index(cff,pos,&cff->globalSubrIndex);
  if (pos<0) {
    fprintf(stderr,"Could not load Global Subr INDEX\n");
    return -1;
  }

  // ? encodings (via topIndex)    - or: on first access
  // ? charsets (via topIndex)

  return pos;
}
// }}}

/* --- */

static CFF_FILE *cff_new() // {{{
{
  CFF_FILE *ret=calloc(1,sizeof(CFF_FILE));
  if (!ret) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
  }
  return ret;
}
// }}}

CFF_FILE *cff_load2(const char *buf,size_t len,int take) // {{{
{
  assert(buf);
  CFF_FILE *ret=cff_new();
  if (ret) {
    if (take) {
      ret->flags|=CFF_F_FREE_DATA;
    }
    ret->font=buf;
    ret->length=len;

    int pos=cff_read_header(ret);
    if (pos>=0) {
      pos=cff_do_load(ret,pos);
    }
    if (pos<0) {
      cff_close(ret);
      return NULL;
    }
  } else if (take) {
    free((char *)buf);
  }
  return ret;
}
// }}}

  // idea: population mechanism (if (!there): load)  [but we don't know many lengths]
  // two cases: a) full buffer already loaded -> pointer
  //            b) memory must be allocated (and freed)
CFF_FILE *cff_load_file(FILE *f) // {{{
{
#define BLOCKSIZE 1024
  char *buf=NULL;
  int pos=0,len=0;
  while (1) {
    if (pos>=len) {
      len+=BLOCKSIZE;
      char *tmp=realloc(buf,len);
      if (!tmp) {
        fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
        free(buf);
        return NULL;
      }
      buf=tmp;
    }
    size_t res=fread(buf+pos,1,len-pos,f);
    if (res==0) {
      if (!feof(f)) {
        fprintf(stderr,"File read error: %s\n", strerror(errno));
        free(buf);
        return NULL;
      }
      break;
    }
    pos+=res;
  }
  return cff_load2(buf,pos,1);
#undef BLOCKSIZE
}
// }}}

CFF_FILE *cff_load_filename(const char *filename) // {{{
{
  FILE *f=fopen(filename,"rb");
  if (!f) {
    fprintf(stderr,"Could not open \"%s\": %s\n", filename, strerror(errno));
    return NULL;
  }
  CFF_FILE *ret=cff_load_file(f);
  fclose(f);
  return ret;
}
// }}}

void cff_close(CFF_FILE *cff) // {{{
{
  assert(cff);
  if (cff) {
    cff_index_free_data(&cff->globalSubrIndex);
    cff_index_free_data(&cff->stringIndex);
    cff_index_free_data(&cff->topDictIndex);
    cff_index_free_data(&cff->nameIndex);
    if (cff->flags&CFF_F_FREE_DATA) {
      free((char *)cff->font);
    }
    free(cff);
  }
}
// }}}

