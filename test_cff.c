#include <assert.h>
#include "cff.h"
#include "cff_int.h"
#include "sfnt.h"

#include "cff_tables.h"

void dump(const char *buf,const char *end) // {{{
{
  for (;buf<end;buf++) {
    printf("%02x ",(unsigned char)*buf);
  }
  printf("\n");
}
// }}}

void test_ints() // {{{
{
  const char *bufs[]={
    "\x8b",  // 0
    "\xef",  // 100
    "\x27",  // -100
    "\xfa\x7c",  // 1000
    "\xfe\x7c",  // -1000
    "\x1c\x27\x10",  // 10000
    "\x1c\xd8\xf0",  // -10000
    "\x1d\x00\x01\x86\xa0",  // 100000
    "\x1d\xff\xfe\x79\x60",  // -100000
    NULL
  };
  char out[10];
  int iA;
  for (iA=0;bufs[iA];iA++) {
    const char *tmp=bufs[iA];
    int32_t val=cff_read_integer(&tmp);
    printf("%d\n",val);
    char *o=out;
    cff_write_integer(&o,val);
    dump(out,o);
//    assert(memcmp(out,bufs[iA],o-out)==0);
  }
}
// }}}

void test_real(double v) // {{{
{
  char buf[10],*b=buf;
  cff_write_real(&b,v);
  dump(buf,b);

  const char *c=buf;
  double res=cff_read_real(&c);
  printf("%g\n",res);
}
// }}}

// TODO? void dump_header(CFF_FILE *cff)  major/minor/(hdrSize)/(abs)offSize

// sorted
  // TODO? special case:  name[0]=NUL
void dump_name_index(CFF_FILE *cff) // {{{
{
  printf("Name index: %d entries\n",
         cff->nameIndex.count);
  if (cff->nameIndex.count==0) {
    return;
  }
  int iA;
  for (iA=0;iA<cff->nameIndex.count;iA++) {
    printf("  %d: %.*s\n",
           iA,
           cff_index_length(&cff->nameIndex,iA),
           cff_index_get(&cff->nameIndex,iA));
  }
}
// }}}

void dump_string_index(CFF_FILE *cff) // {{{
{
  printf("String index: %d extra entries\n",
         cff->stringIndex.count);
  if (cff->stringIndex.count==0) {
    return;
  }
  int iA;
  for (iA=0;iA<cff->stringIndex.count;iA++) {
    printf("  %d: %.*s\n",
           iA+391,
           cff_index_length(&cff->stringIndex,iA),
           cff_index_get(&cff->stringIndex,iA));
  }
}
// }}}


void dump_op(CFF_FILE *cff,struct _CFF_VALUE *value,int args) // {{{
{
  printf(" : ");
  switch (value->op->flags&CFFOP_TYPEMASK) {
  case CFFOP_TYPE_NUMBER:
    assert(args==1);
    // already shown, int or real
    break;
  case CFFOP_TYPE_BOOL:
    assert(args==1);
    assert(value[-1].type==CFF_VAL_INT);
    if (value[-1].number==0) {
      printf("false");
    } else if (value[-1].number==1) {
      printf("true");
    } else {
      printf("[error]");
    }
    break;
  case CFFOP_TYPE_SID:
    assert(args==1);
    assert(value[-1].type==CFF_VAL_INT);
    {
      int len;
      const char *str=cff_get_string(cff,value[-1].number,&len);
      printf("%.*s",len,str);
    }
    break;
  case CFFOP_TYPE_ARRAY:
    // TODO
    printf("[array size %d]",args);
    break;
  case CFFOP_TYPE_DELTA: // TODO
    printf("[delta size %d]",args);
    break;
  case CFFOP_TYPE_NUMNUM:
    printf("[todo: NUM NUM]");
    break;
  case CFFOP_TYPE_SIDSIDNUM:
    printf("[todo: SID SID NUM]");
    break;
  default:
    printf("[error]");
  }
}
// }}}

void dump_value(CFF_FILE *cff,struct _CFF_VALUE *value,int args) // {{{
{
  if (!value) {
    printf("null");
    return;
  }
  switch (value->type) {
  case CFF_VAL_ERROR:
    printf("ERROR");
    break;
  case CFF_VAL_OP:
    printf(" %s",value->op->name);
    if (args>=0) {
      dump_op(cff,value,args);
    }
    printf("\n");
    break;
  case CFF_VAL_INT:
    printf(" %d",value->number);
    break;
  case CFF_VAL_REAL:
    printf(" %gf",value->real);
    break;
  default:
    assert(0);
  }
}
// }}}

void dump_dict(CFF_FILE *cff,struct _CFF_DICT *dict) // {{{
{
  int iA,lastoppos=-1;
  printf("<\n");
  for (iA=0;iA<dict->len;iA++) {
    dump_value(cff,&dict->data[iA],iA-lastoppos-1);
    if (dict->data[iA].type==CFF_VAL_OP) {
      lastoppos=iA;
    }
  }
  printf(">\n");
}
// }}}

 // TODO? into sfnt.c ?
#include <string.h>
int is_sfnt_cff(FILE *f) // {{{
{
  char buf[8];
  const int res=fread(buf,1,8,f);
  rewind(f); // esp. for cff_load

  if (res<8) {
    return 0;
  }
  if (memcmp(buf,"OTTO",4)==0) {
    return 1;
  } else if (memcmp(buf,"wOFFOTTO",8)==0) {
    return 1;
  }
  return 0;
}
// }}}

int main(int argc,char **argv)
{
  FILE *f;
  const char *fn="ee.cff";

  if (argc==2) {
    fn=argv[1];
  }

  if ((f=fopen(fn,"rb"))==NULL) {
    fprintf(stderr,"Could not open font: %m\n");
    return 1;
  }

  CFF_FILE *cff;
  if (is_sfnt_cff(f)) {
    OTF_FILE *otf=otf_load2(f,-1);
    assert(otf);

    int len;
    char *buf=otf_get_table(otf,OTF_TAG('C','F','F',' '),&len);
    otf_close(otf);
    assert(buf);

    cff=cff_load2(buf,len,1);
  } else {
    cff=cff_load_file(f);
    fclose(f);
  }
  assert(cff);

  dump_name_index(cff);
//  dump_string_index(cff);
// ...

  assert(cff->nameIndex.count>0);
  struct _CFF_DICT *dict=cff_read_dict(
    cff_index_get(&cff->topDictIndex,0),
    cff_index_length(&cff->topDictIndex,0));
  assert(dict);
  dump_dict(cff,dict);

  cff_close(cff);

//  test_ints();
//  test_real(100000000.);
//  test_real(-2.25);
//  test_real(1.140541e-05);

//  printf("%d",cff_find_stdstr("a"));

  return 0;
}
