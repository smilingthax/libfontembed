#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "fontfile.h"

// we do not support a tag name with \0
static unsigned int parse_tag(const char *name) // {{{
{
  assert(name);

  if (strlen(name)!=4) {
    fprintf(stderr,"Error: Tag name must have exactly 4 letters");
    return 0;
  }

  return OTF_TAG(name[0],name[1],name[2],name[3]);
}
// }}}

static void list_tables(OTF_FILE *otf) // {{{
{
  assert(otf);
  printf("File has %d tables:\n",otf->numTables);

  int iA;
  for (iA=0;iA<otf->numTables;iA++) {
    if (otf->flags&OTF_F_WOFF) {
      printf("  %c%c%c%c %6d(%6d) @%6d\n",
             OTF_UNTAG(otf->tables[iA].tag),
             otf->tables[iA].length,otf->tables[iA].compLength,
             otf->tables[iA].offset);
    } else {
      printf("  %c%c%c%c %6d @%6d\n",
             OTF_UNTAG(otf->tables[iA].tag),
             otf->tables[iA].length,otf->tables[iA].offset);
    }
  }
}
// }}}

int main(int argc,char **argv)
{
  int opt;
  int usage=0;
  unsigned int tag=0;
  const char *fontfile=NULL;

  while ((opt=getopt(argc, argv, "ht:"))!=-1) {
    switch (opt) {
    case 't':
      tag=parse_tag(optarg);
      if (!tag) {
        return 3;
      }
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

  if (usage) {
    fprintf(stderr,"\nUsage: %s fontfile\n"
                   "  -h: Help\n"
                   "  -t [table tag]: Dump table\n",
                   argv[0]);
    return 1;
  }

  OTF_FILE *otf=otf_load(fontfile);
  if (!otf) {
    fprintf(stderr,"Error: Could not load fontfile\n");
    return 2;
  }

  FONTFILE *ff=fontfile_open_sfnt(otf);
  if (!ff) {
    fprintf(stderr,"Bad alloc: %s\n", strerror(errno));
    return 2;
  }

  if (tag) {
    int len;
    char *data=otf_get_table(otf,tag,&len);
    if (!data) {
      fprintf(stderr,"Error: Could not find table\n");
      return 3;
    }
    if (isatty(fileno(stdout))) {
      printf("Table has %d bytes\n",len);
    } else {
      fwrite(data,1,len,stdout);
    }
    free(data);
  } else {
    list_tables(otf);
  }

  fontfile_close(ff);

  return 0;
}
