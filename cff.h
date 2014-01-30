#ifndef _CFF_H
#define _CFF_H

#include <stdio.h>
#include <stdint.h>

typedef enum {
  OFFERR=0,
  OFFSIZE8=1,
  OFFSIZE16=2,
  OFFSIZE24=3,
  OFFSIZE32=4,
} CFF_OffSize;

struct CFF_Index {
  uint16_t count;
  CFF_OffSize offSize; // !=0, except for count==0
  uint32_t *offset;
  const char *dataStart;
};

typedef struct {
  const char *font;
  size_t length;
  int flags;

  uint8_t majorVersion;
  uint8_t minorVersion;
  CFF_OffSize absOffSize;

  struct CFF_Index nameIndex;
  struct CFF_Index topDictIndex;
  struct CFF_Index stringIndex;
  struct CFF_Index globalSubrIndex;

} CFF_FILE;

#define CFF_F_FREE_DATA 0x0001

CFF_FILE *cff_load2(const char *buf,size_t len,int take);
CFF_FILE *cff_load_file(FILE *f);
CFF_FILE *cff_load_filename(const char *filename);
void cff_close(CFF_FILE *cff);

// cff_int ?
double cff_read_real(const char **buf);
int32_t cff_read_integer(const char **buf);

// struct _CFF_VALUE cff_read_dict_token(const char **buf);

void cff_write_real(char **,double value);
void cff_write_integer(char **,int32_t value);

// void cff_write_dict_token(char **buf,const struct _CFF_VALUE *value);

#endif
