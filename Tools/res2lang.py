import argparse
import re

ap = argparse.ArgumentParser()
ap.add_argument("in_filename")
ap.add_argument("out_filename")
args = ap.parse_args()

in_file = open(args.in_filename, "r", encoding="utf-8")
out_file = open(args.out_filename, "w", encoding="utf-8")

def backslash(mo):
  s = mo.group(0)
  if s == "\\r": return ""
  if s == "\\n": return "\n"
  if s == "\\t": return "\t"
  if s == '\\"': return '"'
  if s == "\\'": return "'"
  if s == "\\\\": return "\\"
  return s

match = [
  re.compile("\\\\[rnt'\"\\\\]"),
  re.compile("<resources>"),
  re.compile("</resources>"),
  re.compile(r"<string(\s+)"),
  re.compile("</string>")
]

repl = [backslash, "<Language Name=\"English\"><Strings>", "</Strings></Language>", r"<String\1", "</String>"]

for line in in_file:
  for m, r in zip(match, repl):
    line = m.sub(r, line)
  out_file.write(line)
