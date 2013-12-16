#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "fontfile.h"
#include "sfnt_int.h" // TODO...?

// TODO:   ... subset code point parser  ... needs conversion from unicode to glyph
BITSET parse_ranges(OTF_FILE *otf,const char *str) // {{{
{
fprintf(stderr,"Subsetting not implemented yet\n");
return NULL;
  BITSET ret=bitset_new(otf->numGlyphs);
  // TODO
  return ret;
}
// }}}

static void file_outfn(const char *buf,int len,void *context) // {{{
{
  FILE *f=(FILE *)context;
  if (fwrite(buf,1,len,f)!=len) {
    fprintf(stderr,"Short write: %m\n");
    assert(0);
    return;
  }
}
// }}}

int main(int argc,char **argv)
{
  int opt;
  int usage=0;
  const char *fontfile=NULL;
  const char *woffout=NULL,*sfntout=NULL;

  BITSET subset=NULL;
  const char *subsetstr=NULL;

  while ((opt=getopt(argc, argv, "hw:t:s:"))!=-1) {
    switch (opt) {
    case 'w':
      woffout=optarg;
      break;
    case 't':
      sfntout=optarg;
      break;
    case 's':
      subsetstr=optarg;
      break;
    case 'h':
    default:
      usage=1;
      break;
    }
  }
  if (optind+1==argc) {
    fontfile=argv[optind];
  } else {
    fprintf(stderr,"Error: Expected fontfile\n");
    usage=1;
  }
  if ( (woffout)&&(sfntout) ) {
    fprintf(stderr,"Error: Only one of -w and -t may be given\n");
    usage=1;
  }

  if (usage) {
    fprintf(stderr,"\nUsage: %s infile [-w|-t] outfile\n"
                   "  -h: Help\n"
                   "  -w wofffile: Output as WOFF (compressed) to file\n"
                   "  -t sfntfile: Output as SFNT (uncompressed) to file\n"
                   "  -s [subset-ranges]: TODO\n",
                   argv[0]);
    return 1;
  }

  OTF_FILE *otf=otf_load(fontfile);
  if (!otf) {
    fprintf(stderr,"Error: Could not load fontfile\n");
    return 2;
  }

  if (subsetstr) {
    subset=parse_ranges(otf,subsetstr);
  }

  FONTFILE *ff=fontfile_open_sfnt(otf);
  if (!ff) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    free(subset);
    return 2;
  }

  FILE *f=NULL;
  struct _OTF_WRITE_WOFF woff={0,0},*woffptr=NULL; // version; no metaData, no privData
  if (woffout) {
    f=fopen(woffout,"wb");
    woffptr=&woff;
  } else if (sfntout) {
    f=fopen(sfntout,"wb");
  } else {
    // NO-OP: no output file given
    fprintf(stderr,"Nothing to do\n");
    fontfile_close(ff);
    free(subset);
    return 3;
  }
  if (!f) {
    fprintf(stderr,"Error: Could not open output fontfile\n");
    fontfile_close(ff);
    free(subset);
    return 2;
  }

  int res;
  if (subset) {
    res=otf_subset2(otf,subset,woffptr,file_outfn,f);
  } else {
    res=otf_copy_sfnt(otf,woffptr,file_outfn,f);
  }

  fclose(f);
  fontfile_close(ff);
  free(subset);

  if (res<0) {
    return 4;
  }
  return 0;
}
