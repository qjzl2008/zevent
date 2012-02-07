#include <windows.h>
int conv_utf8_to_ucs2(const char *in, size_t *inbytes,
								 wchar_t *out, 
								 size_t *outwords)
{
	long newch, mask;
	size_t expect, eating;
	int ch;

	while (*inbytes && *outwords) 
	{
		ch = (unsigned char)(*in++);
		if (!(ch & 0200)) {
			/* US-ASCII-7 plain text
			*/
			--*inbytes;
			--*outwords;
			*(out++) = ch;
		}
		else
		{
			if ((ch & 0300) != 0300) { 

				return -1;
			}
			else
			{
				mask = 0340;
				expect = 1;
				while ((ch & mask) == mask) {
					mask |= mask >> 1;
					if (++expect > 3) /* (truly 5 for ucs-4) */
						return -1;
				}
				newch = ch & ~mask;
				eating = expect + 1;
				if (*inbytes <= expect)
					return -1;

				if (expect == 1) {
					if (!(newch & 0036))
						return -1;
				}
				else {
					if (!newch && !((unsigned char)*in & 0077 & (mask << 1)))
						return -1;
					if (expect == 2) {
						if (newch == 0015 && ((unsigned char)*in & 0040))
							return -1;
					}
					else if (expect == 3) {
						if (newch > 4)
							return -1;
						if (newch == 4 && ((unsigned char)*in & 0060))
							return -1;
					}
				}
				if (*outwords < (size_t)(expect > 2) + 1) 
					break; /* buffer full */
				while (expect--)
				{
					if (((ch = (unsigned char)*(in++)) & 0300) != 0200)
						return -1;
					newch <<= 6;
					newch |= (ch & 0077);
				}
				*inbytes -= eating;
				if (newch < 0x10000) 
				{
					--*outwords;
					*(out++) = (wchar_t) newch;
				}
				else 
				{
					*outwords -= 2;
					newch -= 0x10000;
					*(out++) = (wchar_t) (0xD800 | (newch >> 10));
					*(out++) = (wchar_t) (0xDC00 | (newch & 0x03FF));                    
				}
			}
		}
	}
	return 0;
}

int conv_ucs2_to_utf8(const wchar_t *in,
								 size_t *inwords,
								 char *out,
								 size_t *outbytes)
{
	long newch,require;
	size_t need;
	char *invout;
	int ch;

	while(*inwords)
	{
		ch = (unsigned short)(*in++);
		if(ch < 0x80)
		{
			--*inwords;
			--*outbytes;
			*(out++) = (unsigned char ) ch;
		}
		else
		{
			if((ch & 0xFC00) == 0xDC00) {
				return -1;
			}
			if((ch & 0xFC00) == 0xD800) {
				if(*inwords < 2) {
					return -1;
				}

				if(((unsigned short)(*in) & 0xFC00) != 0xDC00) {
					return -1;
				}
				newch = (ch & 0x03FF) << 10 | ((unsigned short)(*in++) & 0x03FF);
				newch += 0x10000;
			}
			else {
				newch = ch;
			}

			require = newch >> 11;
			need = 1;
			while (require)
				require >>=5, ++need;
			if(need >= *outbytes)
				break;
			*inwords -= (need > 2) + 1;
			*outbytes -= need + 1;

			ch = 0200;
			out += need + 1;
			invout = out;
			while(need--) {
				ch |= ch >> 1;
				*(--invout) = (unsigned char)(0200 | (newch & 0077));
				newch >>= 6;
			}
			*(--invout) = (unsigned char)(ch | newch);
		}
	}
	return 0;
}
