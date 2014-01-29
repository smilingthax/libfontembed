#include <assert.h>
#include "cff.h"

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

int main(int argc,char **argv)
{
  FILE *f;
  const char *fn="ee.cff";

  if ((f=fopen(fn,"rb"))==NULL) {
    fprintf(stderr,"Could not open font: %m\n");
    return 1;
  }

  CFF_FILE *cff=cff_load_file(f);
  assert(cff);

  fclose(f);

// ...

  cff_close(cff);

//  test_ints();
//  test_real(100000000.);
//  test_real(-2.25);
//  test_real(1.140541e-05);

//  printf("%d",cff_find_stdstr("a"));

  return 0;
}
