#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "fontfile.h"
#include "sfnt_int.h" // TODO...?

static int parse_uni(const char **str,int *is_wildcard) // {{{
{
  const char *end=*str;
  int iA,ret=0,wildcard=1;

  for (iA=0;(*end)&&(iA<6);end++,iA++) {
    if (*end=='?') {
      wildcard*=16;
      ret*=16;
    } else if (wildcard>1) {
      ret=-1;
      break;
    } else if ( (*end>='0')&&(*end<='9') ) {
      ret=16*ret+(*end-'0');
    } else if ( (*end>='a')&&(*end<='f') ) {
      ret=16*ret+10+(*end-'a');
    } else if ( (*end>='A')&&(*end<='F') ) {
      ret=16*ret+10+(*end-'A');
    } else {
      break;
    }
  }
  if (  (wildcard>0x100000)||
        ( (ret==0x100000)&&(wildcard>0x10000) )  ) {
    ret=-1;
  } else if (is_wildcard) {
    *is_wildcard=wildcard-1;
  } else if (wildcard>1) {
    ret=-1;
  }
  if ( (iA==0)||(iA>=6)||(ret<0)||(ret>0x10ffff) ) {
    fprintf(stderr,"Parse error in unicode point\n");
    return -1;
  }
  *str=end;

  return ret;
}
// }}}

static void set_range(OTF_FILE *otf,BITSET subset,int start,int end) // {{{
{
//fprintf(stderr,"%d - %d\n",start,end);
  for (;start<=end;start++) {
    bit_set(subset,otf_from_unicode(otf,start));
  }
}
// }}}

// U+0-0d,U+5fa4, U+23??     // TODO? ,'asdf'
static BITSET parse_ranges(OTF_FILE *otf,const char *str) // {{{
{
  BITSET ret=bitset_new(otf->numGlyphs);
  if (!ret) {
    return NULL;
  }

  while (*str==' ') str++;
  while (*str) {
    if ( ((str[0]|0x20)=='u')&&(str[1]=='+') ) {
      int is_wildcard;
      str+=2;
      const int unistart=parse_uni(&str,&is_wildcard);
      if (unistart<0) {
        goto fail;
      }
      if (*str=='-') {
        if (is_wildcard>0) {
          goto fail;
        }
        str++;
        const int uniend=parse_uni(&str,NULL);
        if (uniend<0) {
          goto fail;
        } else if (uniend<unistart) {
          fprintf(stderr,"Reverse range is not allowed\n");
          goto fail;
        }
        set_range(otf,ret,unistart,uniend);
      } else if (is_wildcard) {
        set_range(otf,ret,unistart,unistart+is_wildcard);
      } else {
        bit_set(ret,otf_from_unicode(otf,unistart));
      }
    } else {
      break;
    }
    while (*str==' ') str++;
    if (*str==',') {
      str++;
      while (*str==' ') str++;
      if (!*str) {
        goto fail;
      }
    }
  }

  if (!*str) {
    return ret;
  }
fail:
  free(ret);
  return NULL;
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
                   "  -s [subset-ranges]: e.g. U+0-0d,U+5fa4,U+23??\n",  // TODO? ,'asdf'
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
    if (!subset) {
      return 2;
    }
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
    if (is_CFF(otf)) {
      fprintf(stderr,"Note: Glyph subsetting not implemented yet for CFF-type files\n");
      res=otf_subset_cff2(otf,subset,woffptr,file_outfn,f);
    } else {
      res=otf_subset2(otf,subset,woffptr,file_outfn,f);
    }
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
