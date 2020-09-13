/*
 * punycode.c from RFC 3492
 * http://www.nicemice.net/idn/
 * Adam M. Costello
 * http://www.nicemice.net/amc/
 *
 * This is ANSI C code (C89) implementing Punycode (RFC 3492).
 */

#include "stdinc.h"

#include "punycode.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits>

#ifdef TEST_PROG
#include <fstream>
#include <string>
#include <assert.h>
#endif

/*** Bootstring parameters for Punycode ***/

enum
{
  base = 36,
  tmin = 1,
  tmax = 26,
  skew = 38,
  damp = 700,
  initial_bias = 72,
  initial_n = 0x80,
  delimiter = 0x2D
};

/*
 * decode_digit(cp) returns the numeric value of a basic code
 * point (for use in representing integers) in the range 0 to
 * base-1, or base if cp is does not represent a value.
 */
static inline uint32_t decode_digit(uint32_t cp)
{
  return (cp - 48 < 10 ?
          cp - 22 : cp - 65 < 26 ?
          cp - 65 : cp - 97 < 26 ?
          cp - 97 : base);
}

static inline char encode_digit(uint32_t d)
{
  return d + (d < 26 ? 'a' : '0' - 26);
  /*  0..25 map to ASCII a..z or A..Z */
  /* 26..35 map to ASCII 0..9         */
}

static inline uint32_t adapt(uint32_t delta, uint32_t numpoints, bool firsttime)
{
  uint32_t k;

  delta = firsttime ? delta / damp : delta >> 1;
  delta += delta / numpoints;

  for (k = 0; delta > ((base - tmin) * tmax) / 2; k += base)
    delta /= base - tmin;
  return k + (base - tmin + 1) * delta / (delta + skew);
}

punycode_status punycode_encode(size_t input_length, const wchar_t *input, size_t &output_length, char *output)
{
  uint32_t n, delta, h, b, bias, j, m, q, k, t;
  size_t max_out = output_length;
  size_t out = 0;

  /* Initialize the state: */

  n = initial_n;
  delta = 0;
  bias = initial_bias;

  /* Handle the basic code points: */
  for (j = 0; j < input_length; ++j)
  {
    if ((uint32_t) input[j] < 128)
    {
      if (max_out - out < 2)
        return punycode_big_output;
      output[out++] = (char) input[j];
    }
  }

  h = b = out;

  /* h is the number of code points that have been handled, b is the
   * number of basic code points, and out is the number of characters
   * that have been output.
   */
  if (b > 0)
    output[out++] = delimiter;

  /* Main encoding loop:
   */
  while (h < input_length)
  {
    /* All non-basic code points < n have been
     * handled already.  Find the next larger one:
     */
    for (m = std::numeric_limits<uint32_t>::max(), j = 0; j < input_length; ++j)
    {
      if ((uint32_t) input[j] >= n && (uint32_t) input[j] < m)
        m = input[j];
    }

    /* Increase delta enough to advance the decoder's
     * <n,i> state to <m,0>, but guard against overflow:
     */
    uint64_t advance = uint64_t(m - n) * (h + 1);
    if (advance >> 32)
      return punycode_overflow;

    delta += (uint32_t) advance;
    n = m;

    for (j = 0; j < input_length; ++j)
    {
      if ((uint32_t) input[j] < n)
      {
        if (++delta == 0)
          return punycode_overflow;
      }

      if ((uint32_t) input[j] == n)
      {
        /* Represent delta as a generalized variable-length integer: */
        for (q = delta, k = base;; k += base)
        {
          if (out >= max_out)
            return punycode_big_output;

          t = k <= bias ? tmin :
              k >= bias + tmax ? tmax :
              k - bias;
          if (q < t)
            break;
          output[out++] = encode_digit(t + (q - t) % (base - t));
          q = (q - t) / (base - t);
        }
        output[out++] = encode_digit(q);
        bias = adapt(delta, h + 1, h == b);
        delta = 0;
        ++h;
      }
    }
    ++delta;
    ++n;
  }

  output_length = out;
  return punycode_success;
}

punycode_status punycode_decode(size_t input_length, const char *input, size_t &output_length, wchar_t *output)
{
  uint32_t n, i, bias, b, j, in, oldi, w, k, digit, t, val32;
  uint64_t val64;
  size_t max_out = output_length;
  size_t out = 0;

  /* Initialize the state: */

  n = initial_n;
  i = 0;
  bias = initial_bias;

  /* Handle the basic code points:  Let b be the number of input code
   * points before the last delimiter, or 0 if there is none, then
   * copy the first b code points to the output.
   */
  for (b = j = 0; j < input_length; ++j)
    if (input[j] == delimiter)
      b = j;

  if (b > max_out)
    return punycode_big_output;

  for (j = 0; j < b; ++j)
  {
    if ((uint32_t) input[j] >= 128)
      return punycode_bad_input;
    output[out++] = input[j];
  }

  /* Main decoding loop:  Start just after the last delimiter if any
   * basic code points were copied; start at the beginning otherwise.
   */
  for (in = b > 0 ? b + 1 : 0; in < input_length; ++out)
  {
    /* in is the index of the next character to be consumed, and
     * out is the number of code points in the output array.
     */

    /* Decode a generalized variable-length integer into delta,
     * which gets added to i.  The overflow checking is easier
     * if we increase i as we go, then subtract off its starting
     * value at the end to obtain delta.
     */
    for (oldi = i, w = 1, k = base;; k += base)
    {
      if (in >= input_length)
        return punycode_bad_input;

      digit = decode_digit(input[in++]);
      if (digit >= base)
        return punycode_bad_input;

      val64 = uint64_t(digit) * w;
      if (val64 >> 32)
        return punycode_overflow;

      val32 = i + (uint32_t) val64;
      if (val32 < i)
        return punycode_overflow;
      
      i = val32;
      t = k <= bias ? tmin :
          k >= bias + tmax ? tmax :
          k - bias;
      if (digit < t)
        break;
      
      val64 = uint64_t(w) * (base - t);
      if (val64 >> 32)
        return punycode_overflow;

      w = (uint32_t) val64;
    }

    bias = adapt(i - oldi, out + 1, oldi == 0);

    /* i was supposed to wrap around from out+1 to 0,
     * incrementing n each time, so we'll fix that now:
     */
    uint32_t dq = i / (out + 1);
    uint32_t dr = i % (out + 1);
    n += dq;
    if (n < dq)
      return punycode_overflow;
    i = dr;

    /* Insert n at position i of the output:
     */
    if (out >= max_out)
      return punycode_big_output;

    memmove(output + i + 1, output + i, (out - i) * sizeof(*output));
    output[i++] = n;
  }

  output_length = out;
  return punycode_success;
}

#if defined(TEST_PROG)

/* For testing, we'll just set some compile-time limits rather than
 * use malloc(), and set a compile-time option rather than using a
 * command-line option.
 */
#define UNICODE_MAX_LENGTH  256
#define ACE_MAX_LENGTH      256

static void usage (char **argv)
{
  fprintf (stderr,
           "\n"
           "%s -e         encodes a UTF-16 LE string read from input.txt.\n"
           "%s -d <puny>  decodes a Punycode string and writes output to output.txt.\n"
           "\n"
           "Input and output are plain text in the native character set.\n"
           "Code points are in the form u+hex separated by whitespace.\n"
           "Although the specification allows Punycode strings to contain\n"
           "any characters from the ASCII repertoire, this test code\n"
           "supports only the printable characters, and needs the Punycode\n"
           "string to be followed by a newline.\n"
           "The case of the u in u+hex is the force-to-uppercase flag.\n",
           argv[0], argv[0]);
  exit (EXIT_FAILURE);
}

#define fail(msg) __fail (__LINE__, msg)
static void __fail (unsigned line, const char *msg)
{
  fprintf (stderr, "line %u: %s", line, msg);
  exit (EXIT_FAILURE);
}

static const char too_big[]       = "input or output is too large, recompile with larger limits\n";
static const char invalid_input[] = "invalid input\n";
static const char overflow[]      = "arithmetic overflow\n";
static const char io_error[]      = "I/O error\n";

int main(int argc, char **argv)
{
  punycode_status status;
  size_t output_length;

  if (argc < 2 || argv[1][0] != '-' || argv[1][2] != 0)
     usage(argv);

  if (argv[1][1] == 'e' && argc == 2)
  {
    std::ifstream in_file("input.txt");
    char temp_buf[UNICODE_MAX_LENGTH + 1];
    wchar_t input[UNICODE_MAX_LENGTH + 1];
    char output[ACE_MAX_LENGTH + 1];
    in_file.get(temp_buf, UNICODE_MAX_LENGTH);
    size_t temp_length = in_file.gcount();
    size_t input_length = 0;
    size_t i = 0;
    if (temp_length > 2 && temp_buf[0] == (char) 0xFF && temp_buf[1] == (char) 0xFE) i += 2;
    for (; i + 1 < temp_length; i += 2)
    {
      uint32_t val = (uint8_t) temp_buf[i] | (uint8_t) temp_buf[i+1] << 8;
      if (val > ' ') input[input_length++] = val;
    }

    output_length = ACE_MAX_LENGTH;
    status = punycode_encode(input_length, input, output_length, output);
    if (status == punycode_bad_input)
       fail(invalid_input);
    if (status == punycode_big_output)
       fail(too_big);
    if (status == punycode_overflow)
       fail(overflow);
    assert(status == punycode_success);

    output[output_length] = '\0';
    puts(output);
    return EXIT_SUCCESS;
  }

  if (argv[1][1] == 'd' && argc == 3)
  {
    const char *input = argv[2];
    wchar_t output[UNICODE_MAX_LENGTH + 1];
    char temp_buf[2*(UNICODE_MAX_LENGTH + 1)];
    output_length = UNICODE_MAX_LENGTH;
    status = punycode_decode(strlen(input), input, output_length, output);

    if (status == punycode_bad_input)
       fail(invalid_input);
    if (status == punycode_big_output)
       fail(too_big);
    if (status == punycode_overflow)
       fail(overflow);
    assert(status == punycode_success);

    output[output_length++] = L'\n';
    size_t temp_length = 0;
    for (size_t i = 0; i < output_length; ++i)
    {
      temp_buf[temp_length++] = (uint8_t) output[i];
      temp_buf[temp_length++] = ((uint16_t) output[i]) >> 8;
    }

    std::ofstream out_file("output.txt", std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    char header[2] = { (char) 0xFF, (char) 0xFE };
    out_file.write(header, 2);
    out_file.write(temp_buf, temp_length);
    return EXIT_SUCCESS;
  }
  
  usage(argv);
  return EXIT_SUCCESS;
}
#endif   /* TEST_PROG */
