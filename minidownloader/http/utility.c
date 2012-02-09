
#include <ctype.h>
#include "utility.h"
#include "error.h"


/* this function will convert a NULL terminated char * to all upper case,
   and return it in dest. Return OK on success. */
int ZNet_Str_To_Upper(const char * str, char * dest)
{
  while(*(dest++) = toupper(*(str++)))
  {}
  return OK;
}
