perl Configure VC-WIN32-ARM no-makedepend no-shared no-engine no-static-engine no-mdc2 no-ec2m no-sm2 no-sm4 no-aria no-camellia no-cast no-gost no-psk no-srp no-hw-padlock --prefix=\tmp --openssldir=\tmp

set CONFFILES=include\crypto\bn_conf.h include\crypto\dso_conf.h include\openssl\opensslconf.h crypto\buildinf.h
del %CONFFILES% >NUL
nmake %CONFFILES%

set F=VC-WIN32-ARM.mak
copy /Y makefile %F% >NUL

del makefile

sed "/^PLATFORM=/i MAKE=nmake /f %F%" -i %F%
sed 's/\(-D"\(DEBUG\|NDEBUG\|_DEBUG\)\"\)//g' -i %F%
sed "/^all:/i !IF \"$(BUILD_DEBUG)\" != \"\"\nLIB_CFLAGS=$(LIB_CFLAGS) /MTd\n!ELSE\nLIB_CFLAGS=$(LIB_CFLAGS) /MT\n!ENDIF" -i %F%
sed "s/libcrypto.lib/$(LIBCRYPTO)/g;s/libssl.lib/$(LIBSSL)/g" -i %F%
sed "s/\\/O2/$(OPT_FLAGS)/g" -i %F%
sed "/^LIBS=/i !IF \"$(BUILD_DEBUG)\" != \"\"\nCONFIG=Debug\nDBG_DEFINES=-D\"DEBUG\" -D\"_DEBUG\"\nOPT_FLAGS=/Od\n!ELSE\nCONFIG=Release\nDBG_DEFINES=-D\"NDEBUG\"\nOPT_FLAGS=/O2\n!ENDIF\nOUTDIR=..\\\\vc16\\\\ARM\\\\$(CONFIG)\\\\openssl\nLIBCRYPTO=$(OUTDIR)\\\\libcrypto.lib\nLIBSSL=$(OUTDIR)\\\\libssl.lib\n" -i %F%
sed "s/^CNF_CPPFLAGS=.*$/& $(DBG_DEFINES)/;s/ \\/MT / /g" -i %F%
sed "s/[-_a-z0-9\\]\+\\\\\([-_a-z0-9]\+\.obj\)/$\(OUTDIR\)\\\\\1/g" -i %F%

rem Remove unused targets
for %%S in (all _all depend test tests _tests list-tests install uninstall clean libclean distclean build_engines _build_engines build_engines_nodep build_programs _build_programs build_programs_nodep configdata.pm "reconfigure reconf") do call :remove_target %%S
for %%S in (%ASMFILES%) do call :remove_target %%S

rem Add $(OUTDIR) dependency
sed "s/^$(\(LIBCRYPTO\|LIBSSL\)):\(.*\)$/$(\\1): $(OUTDIR)\\2/" -i %F%
sed "$adepend:\n\n$(OUTDIR):\n\t-mkdir ..\\\\vc16\n\t-mkdir ..\\\\vc16\\\\ARM\n\t-mkdir ..\\\\vc16\\\\ARM\\\\$(CONFIG)\n\t-mkdir $(OUTDIR)\n\nlibclean:\n\t-del /Q /F $(LIBS) ossl_static.pdb $(OUTDIR)\\\\*.obj\n\nclean: libclean\n\n" -i %F%

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
for %%S in (%CONFFILES%) do call :remove_target %%S

goto :end

:remove_target
set target=%1%
set target=%target:\=\\\\%
set target=%target:"=%
sed "/^%target%:/,/^[^[:blank:]]/{/^\(%target%:\|[[:blank:]]\)/d;}" -i %F%
goto :eof

:end
