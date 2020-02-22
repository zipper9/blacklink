/*
 * punycode from RFC 3492
 * http://www.nicemice.net/idn/
 * Adam M. Costello
 * http://www.nicemice.net/amc/
 */


#ifndef _PUNYCODE_H
#define _PUNYCODE_H

#include <stdlib.h>    /* size_t */

enum punycode_status
{
	punycode_success,
	punycode_bad_input,      /* Input is invalid.                       */
	punycode_big_output,     /* Output would exceed the space provided. */
	punycode_overflow        /* Input needs wider integers to process.  */
};

punycode_status punycode_encode(size_t input_length, const wchar_t *input, size_t &output_length, char *output);
punycode_status punycode_decode(size_t input_length, const char *input, size_t &output_length, wchar_t *output);

#endif
