#include <ctype.h>
#include "msvc_c99.h"

int strcasecmp(char *s1,char *s2)
{
    while(toupper((unsigned char )*s1) == toupper((unsigned char)*s2++)) if(*s1++ == '\0') return 0;
    return (toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
}
