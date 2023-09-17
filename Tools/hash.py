#!/usr/bin/env python3

import sys
import hashlib

def base32(data):
  base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
  dest = ""
  index = 0
  i = 0
  while i < len(data):
    if index > 3:
      word = data[i] & (0xFF >> index)
      index = (index + 5) & 7
      word <<= index
      if i + 1 < len(data):
        word |= data[i + 1] >> (8 - index)
      i += 1
    else:
      word = (data[i] >> (8 - (index + 5))) & 0x1F
      index = (index + 5) & 7
      if index == 0:
        i += 1
    dest += base32Alphabet[word]
  return dest

if len(sys.argv) < 2:
  sys.stderr.write("Usage: %s <path>\n" % sys.argv[0])
  sys.exit(1)

h = hashlib.sha256()
with open(sys.argv[1], "rb") as f:
  while True:
    bytes = f.read(32768)
    if bytes == b"": break
    h.update(bytes)

print(base32(h.digest()))
