@echo off

for /f %%# in ('WMIC Path Win32_LocalTime Get /Format:value') do @for /f %%@ in ("%%#") do @set %%@

set "month=0%month%"
set "month=%month:~-2%"

set "day=0%day%"
set "day=%day:~-2%"

"C:\Program Files\7-Zip\7z.exe" a -r -t7z -m0=lzma -mx=9 -mfb=512 -md=1024m -ms=on blacklink_backup_%year%%month%%day%.7z -i@src_include.txt -x@src_exclude.txt
