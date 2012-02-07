#ifndef DL_ENCODE_H
#define DL_ENCODE_H

int conv_ucs2_to_utf8(const wchar_t *in,
					  size_t *inwords,
					  char *out,
					  size_t *outbytes);

int conv_utf8_to_ucs2(const char *in, 
					  size_t *inbytes,
					  wchar_t *out, 
					  size_t *outwords);
#endif