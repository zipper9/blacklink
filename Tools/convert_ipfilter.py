#!/usr/bin/env python3

import sys
import re

if len(sys.argv) != 3:
  print("Usage: <input_file> <output_file>", file=sys.stderr)
  sys.exit(1)

in_file = open(sys.argv[1], "r", encoding="utf-8", newline='')
out_file = open(sys.argv[2], "w", encoding="utf-8", newline='')

r = re.compile(r"^(\d\d\d)\.(\d\d\d)\.(\d\d\d)\.(\d\d\d) - (\d\d\d)\.(\d\d\d)\.(\d\d\d)\.(\d\d\d) , \d\d\d ,(.*)$")
r2 = re.compile(r"""(\\+(['"&]))""")

def strip_lz(a):
  a = a.lstrip("0")
  return a if a else "0"

def get_ip(a1, a2, a3, a4):
  return strip_lz(a1) + "." + strip_lz(a2) + "." + strip_lz(a3) + "." + strip_lz(a4)

def get_description(s):
  s = s.lstrip()
  s = s.rstrip("\\")
  s = r2.sub("\\2", s)
  s = s.replace("&amp;", "&")
  if not s: s = "<Empty>"
  return s

count = 0

for line in in_file:
  line = line.strip()
  if line.startswith("#") or len(line) == 0:
    continue
  m = r.search(line)
  if not m:
    print("Invalid line: " + line, file=sys.stderr)
    sys.exit(1)
  start_ip = get_ip(m.group(1), m.group(2), m.group(3), m.group(4))
  end_ip = get_ip(m.group(5), m.group(6), m.group(7), m.group(8))
  desc = get_description(m.group(9))
  print("%s-%s %s" % (start_ip, end_ip, desc), file=out_file)
  count += 1

print("Converted %d lines" % count)
