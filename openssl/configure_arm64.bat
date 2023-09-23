@echo off

start /wait /B sed 2>NUL
if errorlevel 2 goto :sed_error

start /wait /B perl -v 2>NUL
if errorlevel 2 goto :perl_error

sed "s/SUBDIRS=crypto ssl apps util tools fuzz providers doc/SUBDIRS=crypto ssl providers/" -i build.info

perl Configure VC-WIN64-ARM no-makedepend no-shared no-engine no-static-engine no-mdc2 no-ec2m no-sm2 no-sm4 no-aria no-camellia no-cast no-gost no-psk no-srp no-hw-padlock no-tests no-legacy --prefix=\tmp --openssldir=\tmp
if errorlevel 1 goto :configure_error

set CONFFILES=crypto\buildinf.h include\crypto\bn_conf.h include\crypto\dso_conf.h include\openssl\asn1.h include\openssl\asn1t.h include\openssl\bio.h include\openssl\cmp.h include\openssl\cms.h include\openssl\conf.h include\openssl\crmf.h include\openssl\crypto.h include\openssl\ct.h include\openssl\err.h include\openssl\ess.h include\openssl\fipskey.h include\openssl\lhash.h include\openssl\ocsp.h include\openssl\opensslv.h include\openssl\pkcs12.h include\openssl\pkcs7.h include\openssl\safestack.h include\openssl\srp.h include\openssl\ssl.h include\openssl\ui.h include\openssl\x509.h include\openssl\x509_vfy.h include\openssl\x509v3.h
del %CONFFILES% >NUL
nmake %CONFFILES%
if errorlevel 1 goto :gen_error

set GENFILES=providers\common\der\der_digests_gen.c providers\common\der\der_dsa_gen.c providers\common\der\der_ec_gen.c providers\common\der\der_ecx_gen.c providers\common\der\der_rsa_gen.c providers\common\der\der_wrap_gen.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_dsa.h providers\common\include\prov\der_ec.h providers\common\include\prov\der_ecx.h providers\common\include\prov\der_rsa.h providers\common\include\prov\der_wrap.h
del %GENFILES% >NUL
nmake %GENFILES%
if errorlevel 1 goto :gen_error

for %%L in (%CONFFILES% %GENFILES%) do (
 dos2unix -q %%L
)

set F=VC-WIN64-ARM.mak
copy /Y makefile %F% >NUL

del makefile

sed "/^PLATFORM=/i MAKE=nmake /f %F%" -i %F%
sed 's/\(-D"\(DEBUG\|NDEBUG\|_DEBUG\)\"\)//g' -i %F%
sed "/^all:/i !IF \"$(BUILD_DEBUG)\" != \"\"\nLIB_CFLAGS=$(LIB_CFLAGS) /MTd\n!ELSE\nLIB_CFLAGS=$(LIB_CFLAGS) /MT\n!ENDIF" -i %F%
sed "s/libcrypto\.lib/$(LIBCRYPTO)/g;s/libssl\.lib/$(LIBSSL)/g" -i %F%
sed "s/\\/O2/$(OPT_FLAGS)/g" -i %F%
sed "/^LIBS=/i !IF \"$(BUILD_DEBUG)\" != \"\"\nCONFIG=Debug\nDBG_DEFINES=-D\"DEBUG\" -D\"_DEBUG\"\nOPT_FLAGS=/Od\n!ELSE\nCONFIG=Release\nDBG_DEFINES=-D\"NDEBUG\"\nOPT_FLAGS=/O2\n!ENDIF\nOUTDIR=..\\\\vc16\\\\ARM64\\\\$(CONFIG)\\\\openssl\nLIBCRYPTO=libcrypto.lib\nLIBSSL=libssl.lib\n" -i %F%
sed "s/^CNF_CPPFLAGS=.*$/& $(DBG_DEFINES)/;s/ \\/MT / /g" -i %F%
sed -e "s/\(libcrypto\|libssl\|libdefault\|libcommon\)-lib-//g" -i %F%
sed "s/[-_a-z0-9\\]\+\\\\\([-_a-zA-Z0-9\(\)\$]\+\.obj\)/$\(OUTDIR\)\\\\\1/g" -i %F%

rem Remove unused targets
for %%S in (all _all depend makefile test tests _tests list-tests install uninstall clean libclean distclean build_engines _build_engines build_engines_nodep build_programs _build_programs build_programs_nodep configdata.pm build_html_docs build_docs build_all_generated "reconfigure reconf" "build_apps build_tests") do call :remove_target %%S
for %%S in (%GENFILES%) do call :remove_target %%S
for %%S in (%CONFFILES%) do call :remove_target %%S

sed "s/build_programs_nodep//g" -i %F%
sed "s/copy-utils//g" -i %F%

rem Add $(OUTDIR) dependency
sed "s/^$(\(LIBCRYPTO\|LIBSSL\)):\(.*\)$/$(\\1): $(OUTDIR)\\2/" -i %F%
sed "$adepend:\n\n$(OUTDIR):\n\t-mkdir ..\\\\vc16\n\t-mkdir ..\\\\vc16\\\\ARM64\n\t-mkdir ..\\\\vc16\\\\ARM64\\\\$(CONFIG)\n\t-mkdir $(OUTDIR)\n\nlibclean:\n\t-del /Q /F $(LIBS) ossl_static.pdb $(OUTDIR)\\\\*.obj\n\nclean: libclean\n\n" -i %F%

sed "s/^LIBS=.*$/LIBS=$(OUTDIR)\\\\$(LIBCRYPTO) $(OUTDIR)\\\\$(LIBSSL)/" -i %F%
sed "s/ossl_static.pdb/$(OUTDIR)\\\\&/g" -i %F%

sed "s/^$(LIBCRYPTO):/$(OUTDIR)\\\\&/" -i %F%
sed "s/$(AROUTFLAG)$(LIBCRYPTO)/$(AROUTFLAG)$(OUTDIR)\\\\$(LIBCRYPTO)/" -i %F%
sed "s/^$(LIBSSL):/$(OUTDIR)\\\\&/" -i %F%
sed "s/$(AROUTFLAG)$(LIBSSL)/$(AROUTFLAG)$(OUTDIR)\\\\$(LIBSSL)/" -i %F%

sed "/# Install helper targets/,/# Building targets/d" -i %F%
sed "/FIPSKEY=/d" -i %F%

rem Change OPENSSLDIR to "."
sed 's/-D\"OPENSSLDIR=\\\"[-A-Za-z0-9_\\]*\"\"/-D\"OPENSSLDIR=\\\".\\\"\"/' -i %F%

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
exit 0

:configure_error
echo Error generating makefile
exit 1

:gen_error
echo Error generating files
exit 1

:sed_error
echo 'sed' not found
exit 1

:perl_error
echo 'perl' not found
exit 1
