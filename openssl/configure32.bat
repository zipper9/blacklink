perl Configure VC-WIN32 no-makedepend no-shared no-engine no-static-engine no-mdc2 no-ec2m no-sm2 no-sm4 no-aria no-camellia no-cast no-gost no-psk no-srp no-hw-padlock --prefix=\tmp --openssldir=\tmp

set ASMFILES=crypto\aes\aesni-x86.asm crypto\aes\vpaes-x86.asm crypto\bn\bn-586.asm crypto\bn\co-586.asm crypto\bn\x86-gf2m.asm crypto\bn\x86-mont.asm crypto\chacha\chacha-x86.asm crypto\des\crypt586.asm crypto\des\des-586.asm crypto\ec\ecp_nistz256-x86.asm crypto\md5\md5-586.asm crypto\modes\ghash-x86.asm crypto\poly1305\poly1305-x86.asm crypto\rc4\rc4-586.asm crypto\ripemd\rmd-586.asm crypto\sha\sha1-586.asm crypto\sha\sha256-586.asm crypto\sha\sha512-586.asm crypto\whrlpool\wp-mmx.asm crypto\x86cpuid.asm crypto\bf\bf-586.asm
del %ASMFILES% >NUL
nmake %ASMFILES%

set CONFFILES=include\crypto\bn_conf.h include\crypto\dso_conf.h include\openssl\opensslconf.h crypto\buildinf.h
del %CONFFILES% >NUL
nmake %CONFFILES%

for %%L in (%ASMFILES% %CONFFILES%) do (
 dos2unix -q %%L
)

set F=VC-WIN32.mak
copy /Y makefile %F% >NUL

del makefile

sed "/^PLATFORM=/i MAKE=nmake /f %F%" -i %F%
sed 's/\(-D"\(DEBUG\|NDEBUG\|_DEBUG\)\"\)//g' -i %F%
sed "/^all:/i !IF \"$(BUILD_DEBUG)\" != \"\"\nLIB_CFLAGS=$(LIB_CFLAGS) /MTd\n!ELSE\nLIB_CFLAGS=$(LIB_CFLAGS) /MT\n!ENDIF" -i %F%
sed "s/libcrypto.lib/$(LIBCRYPTO)/g;s/libssl.lib/$(LIBSSL)/g" -i %F%
sed "s/\\/O2/$(OPT_FLAGS)/g" -i %F%
sed "/^LIBS=/i !IF \"$(BUILD_DEBUG)\" != \"\"\nDBG_DEFINES=-D\"DEBUG\" -D\"_DEBUG\"\nOUTDIR=Debug\nOPT_FLAGS=/Od\n!ELSE\nDBG_DEFINES=-D\"NDEBUG\"\nOUTDIR=Release\nOPT_FLAGS=/O2\n!ENDIF\nLIBCRYPTO=$(OUTDIR)\\\\libcrypto.lib\nLIBSSL=$(OUTDIR)\\\\libssl.lib\n" -i %F%
sed "s/^CNF_CPPFLAGS=.*$/& $(DBG_DEFINES)/;s/ \\/MT / /g" -i %F%
sed "s/[-_a-z0-9\\]\+\\\\\([-_a-z0-9]\+\.obj\)/$\(OUTDIR\)\\\\\1/g" -i %F%

rem Remove unused targets
for %%S in (all _all depend test tests _tests list-tests install uninstall clean libclean distclean build_engines _build_engines build_engines_nodep build_programs _build_programs build_programs_nodep configdata.pm) do (
 sed "/^%%S:/,/^[^[:blank:]]/{/^\(%%S:\|[[:blank:]]\)/d;}" -i %F%
)

rem Add $(OUTDIR) dependency
sed "s/^$(\(LIBCRYPTO\|LIBSSL\)):\(.*\)$/$(\\1): $(OUTDIR)\\2/" -i %F%
sed "$adepend:\n\n$(OUTDIR):\n\t-mkdir $(OUTDIR)\n\nlibclean:\n\t-del /Q /F $(LIBS) ossl_static.pdb $(OUTDIR)\\\\*.obj\n\nclean: libclean\n\n" -i %F%

sed "s/^LIBS=.*$/LIBS=$(LIBCRYPTO) $(LIBSSL)/" -i %F%
sed "s/ossl_static.pdb/$(OUTDIR)\\\\&/g" -i %F%

sed "/# Install helper targets/,/# Building targets/d" -i %F%

rem Change OPENSSLDIR to ".", undefine ENGINESDIR
sed 's/-D\"OPENSSLDIR=\\\"[-A-Za-z0-9_\\]*\"\"/-D\"OPENSSLDIR=\\\".\\\"\"/' -i %F%
sed 's/-D\"ENGINESDIR=\\\"[-A-Za-z0-9_\\]*\"\"//' -i %F%

rem Remove variables
for %%S in (PERL RC RCFLAGS MT MTFLAGS ECHO PROCESSOR) do (
 sed "/^%%S=/d" -i %F%
)

rem Remove comments
sed "1,8{/\(^#\|^$\)/d}" -i %F%

rem Remove *.h targets
for %%S in (bn_conf.h dso_conf.h opensslconf.h buildinf.h) do (
 sed "/%%S:/,/^[^[:blank:]]/{/\(%%S:\|^[[:blank:]]\)/d;}" -i %F%
)
