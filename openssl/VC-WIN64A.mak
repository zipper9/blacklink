
MAKE=nmake /f VC-WIN64A.mak
PLATFORM=VC-WIN64A
SRCDIR=.
BLDDIR=.

VERSION=3.0.16
VERSION_NUMBER=3.0.16
MAJOR=3
MINOR=0

SHLIB_VERSION_NUMBER=3

!IF "$(BUILD_DEBUG)" != ""
CONFIG=Debug
DBG_DEFINES=-D"DEBUG" -D"_DEBUG"
OPT_FLAGS=/Od
!ELSE
CONFIG=Release
DBG_DEFINES=-D"NDEBUG"
OPT_FLAGS=/O2
!ENDIF
OUTDIR=..\vc16\x64\$(CONFIG)\openssl
LIBCRYPTO=libcrypto.lib
LIBSSL=libssl.lib

LIBS=$(OUTDIR)\$(LIBCRYPTO) $(OUTDIR)\$(LIBSSL)
SHLIBS=
SHLIBPDBS=
MODULES=
MODULEPDBS=
FIPSMODULE=
FIPSMODULENAME=
PROGRAMS=
PROGRAMPDBS=
SCRIPTS=

GENERATED_MANDATORY=include\crypto\bn_conf.h include\crypto\dso_conf.h include\openssl\asn1.h include\openssl\asn1t.h include\openssl\bio.h include\openssl\cmp.h include\openssl\cms.h include\openssl\conf.h include\openssl\crmf.h include\openssl\crypto.h include\openssl\ct.h include\openssl\err.h include\openssl\ess.h include\openssl\fipskey.h include\openssl\lhash.h include\openssl\ocsp.h include\openssl\opensslv.h include\openssl\pkcs12.h include\openssl\pkcs7.h include\openssl\safestack.h include\openssl\srp.h include\openssl\ssl.h include\openssl\ui.h include\openssl\x509.h include\openssl\x509_vfy.h include\openssl\x509v3.h
GENERATED=crypto\aes\aes-x86_64.asm crypto\aes\aesni-mb-x86_64.asm crypto\aes\aesni-sha1-x86_64.asm crypto\aes\aesni-sha256-x86_64.asm crypto\aes\aesni-x86_64.asm crypto\aes\bsaes-x86_64.asm crypto\aes\vpaes-x86_64.asm crypto\bn\rsaz-avx2.asm crypto\bn\rsaz-avx512.asm crypto\bn\rsaz-x86_64.asm crypto\bn\x86_64-gf2m.asm crypto\bn\x86_64-mont.asm crypto\bn\x86_64-mont5.asm crypto\buildinf.h crypto\chacha\chacha-x86_64.asm crypto\ec\ecp_nistz256-x86_64.asm crypto\ec\x25519-x86_64.asm crypto\md5\md5-x86_64.asm crypto\modes\aesni-gcm-x86_64.asm crypto\modes\ghash-x86_64.asm crypto\poly1305\poly1305-x86_64.asm crypto\rc4\rc4-md5-x86_64.asm crypto\rc4\rc4-x86_64.asm crypto\sha\keccak1600-x86_64.asm crypto\sha\sha1-mb-x86_64.asm crypto\sha\sha1-x86_64.asm crypto\sha\sha256-mb-x86_64.asm crypto\sha\sha256-x86_64.asm crypto\sha\sha512-x86_64.asm crypto\whrlpool\wp-x86_64.asm crypto\x86_64cpuid.asm providers\common\der\der_digests_gen.c providers\common\der\der_dsa_gen.c providers\common\der\der_ec_gen.c providers\common\der\der_ecx_gen.c providers\common\der\der_rsa_gen.c providers\common\der\der_wrap_gen.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_dsa.h providers\common\include\prov\der_ec.h providers\common\include\prov\der_ecx.h providers\common\include\prov\der_rsa.h providers\common\include\prov\der_wrap.h

INSTALL_LIBS="$(LIBCRYPTO)" "$(LIBSSL)"
INSTALL_SHLIBS=
INSTALL_SHLIBPDBS=
INSTALL_ENGINES=
INSTALL_ENGINEPDBS=
INSTALL_MODULES=
INSTALL_MODULEPDBS=
INSTALL_FIPSMODULE=
INSTALL_FIPSMODULECONF=providers\fipsmodule.cnf
INSTALL_PROGRAMS=
INSTALL_PROGRAMPDBS=
BIN_SCRIPTS=
MISC_SCRIPTS=
IMAGEDOCS1=
IMAGEDOCS3=
IMAGEDOCS5=
IMAGEDOCS7=
HTMLDOCS1=
HTMLDOCS3=
HTMLDOCS5=
HTMLDOCS7=
HTMLDOCS1_BLDDIRS=
HTMLDOCS3_BLDDIRS=
HTMLDOCS5_BLDDIRS=
HTMLDOCS7_BLDDIRS=

APPS_OPENSSL="apps\openssl"

# Do not edit these manually. Use Configure with --prefix or --openssldir
# to change this!  Short explanation in the top comment in Configure
INSTALLTOP_dev=
INSTALLTOP_dir=\tmp
OPENSSLDIR_dev=
OPENSSLDIR_dir=\tmp
LIBDIR=lib
MODULESDIR_dev=
MODULESDIR_dir=\tmp\lib\ossl-modules
ENGINESDIR_dev=
ENGINESDIR_dir=\tmp\lib\engines-3
!IF "$(DESTDIR)" != ""
INSTALLTOP=$(DESTDIR)$(INSTALLTOP_dir)
OPENSSLDIR=$(DESTDIR)$(OPENSSLDIR_dir)
ENGINESDIR=$(DESTDIR)$(ENGINESDIR_dir)
MODULESDIR=$(DESTDIR)$(MODULESDIR_dir)
!ELSE
INSTALLTOP=$(INSTALLTOP_dev)$(INSTALLTOP_dir)
OPENSSLDIR=$(OPENSSLDIR_dev)$(OPENSSLDIR_dir)
ENGINESDIR=$(ENGINESDIR_dev)$(ENGINESDIR_dir)
MODULESDIR=$(MODULESDIR_dev)$(MODULESDIR_dir)
!ENDIF

# $(libdir) is chosen to be compatible with the GNU coding standards
libdir=$(INSTALLTOP)\$(LIBDIR)

##### User defined commands and flags ################################

CC="cl"
CPP=$(CC) /EP /C
CPPFLAGS=
CFLAGS=/W3 /wd4090 /nologo $(OPT_FLAGS)
LD="link"
LDFLAGS=/nologo /debug
EX_LIBS=


AR="lib"
ARFLAGS= /nologo


AS="nasm"
ASFLAGS=-g



##### Special command flags ##########################################

COUTFLAG=/Fo$(OSSL_EMPTY)
LDOUTFLAG=/out:$(OSSL_EMPTY)
AROUTFLAG=/out:$(OSSL_EMPTY)
MTINFLAG=-manifest $(OSSL_EMPTY)
MTOUTFLAG=-outputresource:$(OSSL_EMPTY)
ASOUTFLAG=-o $(OSSL_EMPTY)
RCOUTFLAG=/fo$(OSSL_EMPTY)

##### Project flags ##################################################

# Variables starting with CNF_ are common variables for all product types

CNF_ASFLAGS=-Ox -f win64 -DNEAR
CNF_CPPFLAGS=-D"OPENSSL_BUILDING_OPENSSL" -D"OPENSSL_SYS_WIN32" -D"WIN32_LEAN_AND_MEAN" -D"UNICODE" -D"_UNICODE" -D"_CRT_SECURE_NO_DEPRECATE" -D"_WINSOCK_DEPRECATED_NO_WARNINGS"  $(DBG_DEFINES)
CNF_CFLAGS=/Gs0 /GF /Gy
CNF_CXXFLAGS=
CNF_LDFLAGS=
CNF_EX_LIBS=ws2_32.lib gdi32.lib advapi32.lib crypt32.lib user32.lib

# Variables starting with LIB_ are used to build library object files
# and shared libraries.
# Variables starting with DSO_ are used to build DSOs and their object files.
# Variables starting with BIN_ are used to build programs and their object
# files.

LIB_ASFLAGS=$(CNF_ASFLAGS) $(ASFLAGS)
LIB_CPPFLAGS=-D"L_ENDIAN" -D"OPENSSL_PIC" -D"OPENSSLDIR=\".\"" -D"ENGINESDIR=\"\\tmp\\lib\\engines-3\"" -D"MODULESDIR=\"\\tmp\\lib\\ossl-modules\"" $(CNF_CPPFLAGS) $(CPPFLAGS)
LIB_CFLAGS=/Zi /Fd$(OUTDIR)\ossl_static.pdb /Zl $(CNF_CFLAGS) $(CFLAGS)
LIB_LDFLAGS=/dll $(CNF_LDFLAGS) $(LDFLAGS)
LIB_EX_LIBS=$(CNF_EX_LIBS) $(EX_LIBS)
DSO_ASFLAGS=$(CNF_ASFLAGS) $(ASFLAGS)
DSO_CPPFLAGS=$(CNF_CPPFLAGS) $(CPPFLAGS)
DSO_CFLAGS=/Zi /Fddso.pdb $(CNF_CFLAGS) $(CFLAGS)
DSO_LDFLAGS=/dll $(CNF_LDFLAGS) $(LDFLAGS)
DSO_EX_LIBS=$(CNF_EX_LIBS) $(EX_LIBS)
BIN_ASFLAGS=$(CNF_ASFLAGS) $(ASFLAGS)
BIN_CPPFLAGS=$(CNF_CPPFLAGS) $(CPPFLAGS)
BIN_CFLAGS=/Zi /Fdapp.pdb $(CNF_CFLAGS) $(CFLAGS)
BIN_LDFLAGS=setargv.obj /subsystem:console /opt:ref $(CNF_LDFLAGS) $(LDFLAGS)
BIN_EX_LIBS=$(CNF_EX_LIBS) $(EX_LIBS)

# CPPFLAGS_Q is used for one thing only: to build up buildinf.h
CPPFLAGS_Q=-D"L_ENDIAN" -D"OPENSSL_PIC" -D\"OPENSSL_BUILDING_OPENSSL\" -D\"OPENSSL_SYS_WIN32\" -D\"WIN32_LEAN_AND_MEAN\" -D\"UNICODE\" -D\"_UNICODE\" -D\"_CRT_SECURE_NO_DEPRECATE\" -D\"_WINSOCK_DEPRECATED_NO_WARNINGS\"

PERLASM_SCHEME= nasm


# The main targets ###################################################

build_sw: build_generated
	$(MAKE) /$(MAKEFLAGS) depend
	$(MAKE) /$(MAKEFLAGS) _build_sw
_build_sw: build_libs_nodep build_modules_nodep  
build_libs: build_generated
	$(MAKE) /$(MAKEFLAGS) depend
	$(MAKE) /$(MAKEFLAGS) _build_libs
_build_libs: build_libs_nodep
build_modules: build_generated
	$(MAKE) /$(MAKEFLAGS) depend
	$(MAKE) /$(MAKEFLAGS) _build_modules
_build_modules: build_modules_nodep

build_generated: $(GENERATED_MANDATORY)
	@
build_libs_nodep: $(LIBS) 
	@
build_modules_nodep: $(MODULES)
	@

# Kept around for backward compatibility

# Convenience target to prebuild all generated files, not just the mandatory
# ones

!IF "$(BUILD_DEBUG)" != ""
LIB_CFLAGS=$(LIB_CFLAGS) /MTd
!ELSE
LIB_CFLAGS=$(LIB_CFLAGS) /MT
!ENDIF

run_tests:
	@
	@$(ECHO) "Tests are not supported with your chosen Configure options"
	@@rem













$(OUTDIR)\$(LIBCRYPTO): $(OUTDIR) $(OUTDIR)\aes-x86_64.obj $(OUTDIR)\aes_cfb.obj $(OUTDIR)\aes_ecb.obj $(OUTDIR)\aes_ige.obj $(OUTDIR)\aes_misc.obj $(OUTDIR)\aes_ofb.obj $(OUTDIR)\aes_wrap.obj $(OUTDIR)\aesni-mb-x86_64.obj $(OUTDIR)\aesni-sha1-x86_64.obj $(OUTDIR)\aesni-sha256-x86_64.obj $(OUTDIR)\aesni-x86_64.obj $(OUTDIR)\bsaes-x86_64.obj $(OUTDIR)\vpaes-x86_64.obj $(OUTDIR)\a_bitstr.obj $(OUTDIR)\a_d2i_fp.obj $(OUTDIR)\a_digest.obj $(OUTDIR)\a_dup.obj $(OUTDIR)\a_gentm.obj $(OUTDIR)\a_i2d_fp.obj $(OUTDIR)\a_int.obj $(OUTDIR)\a_mbstr.obj $(OUTDIR)\a_object.obj $(OUTDIR)\a_octet.obj $(OUTDIR)\a_print.obj $(OUTDIR)\a_sign.obj $(OUTDIR)\a_strex.obj $(OUTDIR)\a_strnid.obj $(OUTDIR)\a_time.obj $(OUTDIR)\a_type.obj $(OUTDIR)\a_utctm.obj $(OUTDIR)\a_utf8.obj $(OUTDIR)\a_verify.obj $(OUTDIR)\ameth_lib.obj $(OUTDIR)\asn1_err.obj $(OUTDIR)\asn1_gen.obj $(OUTDIR)\asn1_item_list.obj $(OUTDIR)\asn1_lib.obj $(OUTDIR)\asn1_parse.obj $(OUTDIR)\asn_mime.obj $(OUTDIR)\asn_moid.obj $(OUTDIR)\asn_mstbl.obj $(OUTDIR)\asn_pack.obj $(OUTDIR)\bio_asn1.obj $(OUTDIR)\bio_ndef.obj $(OUTDIR)\d2i_param.obj $(OUTDIR)\d2i_pr.obj $(OUTDIR)\d2i_pu.obj $(OUTDIR)\evp_asn1.obj $(OUTDIR)\f_int.obj $(OUTDIR)\f_string.obj $(OUTDIR)\i2d_evp.obj $(OUTDIR)\n_pkey.obj $(OUTDIR)\nsseq.obj $(OUTDIR)\p5_pbe.obj $(OUTDIR)\p5_pbev2.obj $(OUTDIR)\p5_scrypt.obj $(OUTDIR)\p8_pkey.obj $(OUTDIR)\t_bitst.obj $(OUTDIR)\t_pkey.obj $(OUTDIR)\t_spki.obj $(OUTDIR)\tasn_dec.obj $(OUTDIR)\tasn_enc.obj $(OUTDIR)\tasn_fre.obj $(OUTDIR)\tasn_new.obj $(OUTDIR)\tasn_prn.obj $(OUTDIR)\tasn_scn.obj $(OUTDIR)\tasn_typ.obj $(OUTDIR)\tasn_utl.obj $(OUTDIR)\x_algor.obj $(OUTDIR)\x_bignum.obj $(OUTDIR)\x_info.obj $(OUTDIR)\x_int64.obj $(OUTDIR)\x_long.obj $(OUTDIR)\x_pkey.obj $(OUTDIR)\x_sig.obj $(OUTDIR)\x_spki.obj $(OUTDIR)\x_val.obj $(OUTDIR)\async_null.obj $(OUTDIR)\async_posix.obj $(OUTDIR)\async_win.obj $(OUTDIR)\async.obj $(OUTDIR)\async_err.obj $(OUTDIR)\async_wait.obj $(OUTDIR)\bf_cfb64.obj $(OUTDIR)\bf_ecb.obj $(OUTDIR)\bf_enc.obj $(OUTDIR)\bf_ofb64.obj $(OUTDIR)\bf_skey.obj $(OUTDIR)\bf_buff.obj $(OUTDIR)\bf_lbuf.obj $(OUTDIR)\bf_nbio.obj $(OUTDIR)\bf_null.obj $(OUTDIR)\bf_prefix.obj $(OUTDIR)\bf_readbuff.obj $(OUTDIR)\bio_addr.obj $(OUTDIR)\bio_cb.obj $(OUTDIR)\bio_dump.obj $(OUTDIR)\bio_err.obj $(OUTDIR)\bio_lib.obj $(OUTDIR)\bio_meth.obj $(OUTDIR)\bio_print.obj $(OUTDIR)\bio_sock.obj $(OUTDIR)\bio_sock2.obj $(OUTDIR)\bss_acpt.obj $(OUTDIR)\bss_bio.obj $(OUTDIR)\bss_conn.obj $(OUTDIR)\bss_core.obj $(OUTDIR)\bss_dgram.obj $(OUTDIR)\bss_fd.obj $(OUTDIR)\bss_file.obj $(OUTDIR)\bss_log.obj $(OUTDIR)\bss_mem.obj $(OUTDIR)\bss_null.obj $(OUTDIR)\bss_sock.obj $(OUTDIR)\ossl_core_bio.obj $(OUTDIR)\bn_add.obj $(OUTDIR)\bn_asm.obj $(OUTDIR)\bn_blind.obj $(OUTDIR)\bn_const.obj $(OUTDIR)\bn_conv.obj $(OUTDIR)\bn_ctx.obj $(OUTDIR)\bn_depr.obj $(OUTDIR)\bn_dh.obj $(OUTDIR)\bn_div.obj $(OUTDIR)\bn_err.obj $(OUTDIR)\bn_exp.obj $(OUTDIR)\bn_exp2.obj $(OUTDIR)\bn_gcd.obj $(OUTDIR)\bn_gf2m.obj $(OUTDIR)\bn_intern.obj $(OUTDIR)\bn_kron.obj $(OUTDIR)\bn_lib.obj $(OUTDIR)\bn_mod.obj $(OUTDIR)\bn_mont.obj $(OUTDIR)\bn_mpi.obj $(OUTDIR)\bn_mul.obj $(OUTDIR)\bn_nist.obj $(OUTDIR)\bn_prime.obj $(OUTDIR)\bn_print.obj $(OUTDIR)\bn_rand.obj $(OUTDIR)\bn_recp.obj $(OUTDIR)\bn_rsa_fips186_4.obj $(OUTDIR)\bn_shift.obj $(OUTDIR)\bn_sqr.obj $(OUTDIR)\bn_sqrt.obj $(OUTDIR)\bn_srp.obj $(OUTDIR)\bn_word.obj $(OUTDIR)\bn_x931p.obj $(OUTDIR)\rsaz-avx2.obj $(OUTDIR)\rsaz-avx512.obj $(OUTDIR)\rsaz-x86_64.obj $(OUTDIR)\rsaz_exp.obj $(OUTDIR)\rsaz_exp_x2.obj $(OUTDIR)\x86_64-gf2m.obj $(OUTDIR)\x86_64-mont.obj $(OUTDIR)\x86_64-mont5.obj $(OUTDIR)\buf_err.obj $(OUTDIR)\buffer.obj $(OUTDIR)\chacha-x86_64.obj $(OUTDIR)\cmac.obj $(OUTDIR)\cmp_asn.obj $(OUTDIR)\cmp_client.obj $(OUTDIR)\cmp_ctx.obj $(OUTDIR)\cmp_err.obj $(OUTDIR)\cmp_hdr.obj $(OUTDIR)\cmp_http.obj $(OUTDIR)\cmp_msg.obj $(OUTDIR)\cmp_protect.obj $(OUTDIR)\cmp_server.obj $(OUTDIR)\cmp_status.obj $(OUTDIR)\cmp_util.obj $(OUTDIR)\cmp_vfy.obj $(OUTDIR)\cms_asn1.obj $(OUTDIR)\cms_att.obj $(OUTDIR)\cms_cd.obj $(OUTDIR)\cms_dd.obj $(OUTDIR)\cms_dh.obj $(OUTDIR)\cms_ec.obj $(OUTDIR)\cms_enc.obj $(OUTDIR)\cms_env.obj $(OUTDIR)\cms_err.obj $(OUTDIR)\cms_ess.obj $(OUTDIR)\cms_io.obj $(OUTDIR)\cms_kari.obj $(OUTDIR)\cms_lib.obj $(OUTDIR)\cms_pwri.obj $(OUTDIR)\cms_rsa.obj $(OUTDIR)\cms_sd.obj $(OUTDIR)\cms_smime.obj $(OUTDIR)\c_zlib.obj $(OUTDIR)\comp_err.obj $(OUTDIR)\comp_lib.obj $(OUTDIR)\conf_api.obj $(OUTDIR)\conf_def.obj $(OUTDIR)\conf_err.obj $(OUTDIR)\conf_lib.obj $(OUTDIR)\conf_mall.obj $(OUTDIR)\conf_mod.obj $(OUTDIR)\conf_sap.obj $(OUTDIR)\conf_ssl.obj $(OUTDIR)\crmf_asn.obj $(OUTDIR)\crmf_err.obj $(OUTDIR)\crmf_lib.obj $(OUTDIR)\crmf_pbm.obj $(OUTDIR)\ct_b64.obj $(OUTDIR)\ct_err.obj $(OUTDIR)\ct_log.obj $(OUTDIR)\ct_oct.obj $(OUTDIR)\ct_policy.obj $(OUTDIR)\ct_prn.obj $(OUTDIR)\ct_sct.obj $(OUTDIR)\ct_sct_ctx.obj $(OUTDIR)\ct_vfy.obj $(OUTDIR)\ct_x509v3.obj $(OUTDIR)\cbc_cksm.obj $(OUTDIR)\cbc_enc.obj $(OUTDIR)\cfb64ede.obj $(OUTDIR)\cfb64enc.obj $(OUTDIR)\cfb_enc.obj $(OUTDIR)\des_enc.obj $(OUTDIR)\ecb3_enc.obj $(OUTDIR)\ecb_enc.obj $(OUTDIR)\fcrypt.obj $(OUTDIR)\fcrypt_b.obj $(OUTDIR)\ofb64ede.obj $(OUTDIR)\ofb64enc.obj $(OUTDIR)\ofb_enc.obj $(OUTDIR)\pcbc_enc.obj $(OUTDIR)\qud_cksm.obj $(OUTDIR)\rand_key.obj $(OUTDIR)\set_key.obj $(OUTDIR)\str2key.obj $(OUTDIR)\xcbc_enc.obj $(OUTDIR)\dh_ameth.obj $(OUTDIR)\dh_asn1.obj $(OUTDIR)\dh_backend.obj $(OUTDIR)\dh_check.obj $(OUTDIR)\dh_depr.obj $(OUTDIR)\dh_err.obj $(OUTDIR)\dh_gen.obj $(OUTDIR)\dh_group_params.obj $(OUTDIR)\dh_kdf.obj $(OUTDIR)\dh_key.obj $(OUTDIR)\dh_lib.obj $(OUTDIR)\dh_meth.obj $(OUTDIR)\dh_pmeth.obj $(OUTDIR)\dh_prn.obj $(OUTDIR)\dh_rfc5114.obj $(OUTDIR)\dsa_ameth.obj $(OUTDIR)\dsa_asn1.obj $(OUTDIR)\dsa_backend.obj $(OUTDIR)\dsa_check.obj $(OUTDIR)\dsa_depr.obj $(OUTDIR)\dsa_err.obj $(OUTDIR)\dsa_gen.obj $(OUTDIR)\dsa_key.obj $(OUTDIR)\dsa_lib.obj $(OUTDIR)\dsa_meth.obj $(OUTDIR)\dsa_ossl.obj $(OUTDIR)\dsa_pmeth.obj $(OUTDIR)\dsa_prn.obj $(OUTDIR)\dsa_sign.obj $(OUTDIR)\dsa_vrf.obj $(OUTDIR)\dso_dl.obj $(OUTDIR)\dso_dlfcn.obj $(OUTDIR)\dso_err.obj $(OUTDIR)\dso_lib.obj $(OUTDIR)\dso_openssl.obj $(OUTDIR)\dso_vms.obj $(OUTDIR)\dso_win32.obj $(OUTDIR)\f_impl32.obj $(OUTDIR)\f_impl64.obj $(OUTDIR)\curve448.obj $(OUTDIR)\curve448_tables.obj $(OUTDIR)\eddsa.obj $(OUTDIR)\f_generic.obj $(OUTDIR)\scalar.obj $(OUTDIR)\curve25519.obj $(OUTDIR)\ec2_oct.obj $(OUTDIR)\ec2_smpl.obj $(OUTDIR)\ec_ameth.obj $(OUTDIR)\ec_asn1.obj $(OUTDIR)\ec_backend.obj $(OUTDIR)\ec_check.obj $(OUTDIR)\ec_curve.obj $(OUTDIR)\ec_cvt.obj $(OUTDIR)\ec_deprecated.obj $(OUTDIR)\ec_err.obj $(OUTDIR)\ec_key.obj $(OUTDIR)\ec_kmeth.obj $(OUTDIR)\ec_lib.obj $(OUTDIR)\ec_mult.obj $(OUTDIR)\ec_oct.obj $(OUTDIR)\ec_pmeth.obj $(OUTDIR)\ec_print.obj $(OUTDIR)\ecdh_kdf.obj $(OUTDIR)\ecdh_ossl.obj $(OUTDIR)\ecdsa_ossl.obj $(OUTDIR)\ecdsa_sign.obj $(OUTDIR)\ecdsa_vrf.obj $(OUTDIR)\eck_prn.obj $(OUTDIR)\ecp_mont.obj $(OUTDIR)\ecp_nist.obj $(OUTDIR)\ecp_nistz256-x86_64.obj $(OUTDIR)\ecp_nistz256.obj $(OUTDIR)\ecp_oct.obj $(OUTDIR)\ecp_smpl.obj $(OUTDIR)\ecx_backend.obj $(OUTDIR)\ecx_key.obj $(OUTDIR)\ecx_meth.obj $(OUTDIR)\x25519-x86_64.obj $(OUTDIR)\decoder_err.obj $(OUTDIR)\decoder_lib.obj $(OUTDIR)\decoder_meth.obj $(OUTDIR)\decoder_pkey.obj $(OUTDIR)\encoder_err.obj $(OUTDIR)\encoder_lib.obj $(OUTDIR)\encoder_meth.obj $(OUTDIR)\encoder_pkey.obj $(OUTDIR)\err.obj $(OUTDIR)\err_all.obj $(OUTDIR)\err_all_legacy.obj $(OUTDIR)\err_blocks.obj $(OUTDIR)\err_prn.obj $(OUTDIR)\ess_asn1.obj $(OUTDIR)\ess_err.obj $(OUTDIR)\ess_lib.obj $(OUTDIR)\asymcipher.obj $(OUTDIR)\bio_b64.obj $(OUTDIR)\bio_enc.obj $(OUTDIR)\bio_md.obj $(OUTDIR)\bio_ok.obj $(OUTDIR)\c_allc.obj $(OUTDIR)\c_alld.obj $(OUTDIR)\cmeth_lib.obj $(OUTDIR)\ctrl_params_translate.obj $(OUTDIR)\dh_ctrl.obj $(OUTDIR)\dh_support.obj $(OUTDIR)\digest.obj $(OUTDIR)\dsa_ctrl.obj $(OUTDIR)\e_aes.obj $(OUTDIR)\e_aes_cbc_hmac_sha1.obj $(OUTDIR)\e_aes_cbc_hmac_sha256.obj $(OUTDIR)\e_aria.obj $(OUTDIR)\e_bf.obj $(OUTDIR)\e_cast.obj $(OUTDIR)\e_chacha20_poly1305.obj $(OUTDIR)\e_des.obj $(OUTDIR)\e_des3.obj $(OUTDIR)\e_idea.obj $(OUTDIR)\e_null.obj $(OUTDIR)\e_old.obj $(OUTDIR)\e_rc2.obj $(OUTDIR)\e_rc4.obj $(OUTDIR)\e_rc4_hmac_md5.obj $(OUTDIR)\e_rc5.obj $(OUTDIR)\e_seed.obj $(OUTDIR)\e_sm4.obj $(OUTDIR)\e_xcbc_d.obj $(OUTDIR)\ec_ctrl.obj $(OUTDIR)\ec_support.obj $(OUTDIR)\encode.obj $(OUTDIR)\evp_cnf.obj $(OUTDIR)\evp_enc.obj $(OUTDIR)\evp_err.obj $(OUTDIR)\evp_fetch.obj $(OUTDIR)\evp_key.obj $(OUTDIR)\evp_lib.obj $(OUTDIR)\evp_pbe.obj $(OUTDIR)\evp_pkey.obj $(OUTDIR)\evp_rand.obj $(OUTDIR)\evp_utils.obj $(OUTDIR)\exchange.obj $(OUTDIR)\kdf_lib.obj $(OUTDIR)\kdf_meth.obj $(OUTDIR)\kem.obj $(OUTDIR)\keymgmt_lib.obj $(OUTDIR)\keymgmt_meth.obj $(OUTDIR)\legacy_blake2.obj $(OUTDIR)\legacy_md4.obj $(OUTDIR)\legacy_md5.obj $(OUTDIR)\legacy_md5_sha1.obj $(OUTDIR)\legacy_ripemd.obj $(OUTDIR)\legacy_sha.obj $(OUTDIR)\legacy_wp.obj $(OUTDIR)\m_null.obj $(OUTDIR)\m_sigver.obj $(OUTDIR)\mac_lib.obj $(OUTDIR)\mac_meth.obj $(OUTDIR)\names.obj $(OUTDIR)\p5_crpt.obj $(OUTDIR)\p5_crpt2.obj $(OUTDIR)\p_dec.obj $(OUTDIR)\p_enc.obj $(OUTDIR)\p_legacy.obj $(OUTDIR)\p_lib.obj $(OUTDIR)\p_open.obj $(OUTDIR)\p_seal.obj $(OUTDIR)\p_sign.obj $(OUTDIR)\p_verify.obj $(OUTDIR)\pbe_scrypt.obj $(OUTDIR)\pmeth_check.obj $(OUTDIR)\pmeth_gn.obj $(OUTDIR)\pmeth_lib.obj $(OUTDIR)\signature.obj $(OUTDIR)\ffc_backend.obj $(OUTDIR)\ffc_dh.obj $(OUTDIR)\ffc_key_generate.obj $(OUTDIR)\ffc_key_validate.obj $(OUTDIR)\ffc_params.obj $(OUTDIR)\ffc_params_generate.obj $(OUTDIR)\ffc_params_validate.obj $(OUTDIR)\hmac.obj $(OUTDIR)\http_client.obj $(OUTDIR)\http_err.obj $(OUTDIR)\http_lib.obj $(OUTDIR)\i_cbc.obj $(OUTDIR)\i_cfb64.obj $(OUTDIR)\i_ecb.obj $(OUTDIR)\i_ofb64.obj $(OUTDIR)\i_skey.obj $(OUTDIR)\kdf_err.obj $(OUTDIR)\lh_stats.obj $(OUTDIR)\lhash.obj $(OUTDIR)\asn1_dsa.obj $(OUTDIR)\bsearch.obj $(OUTDIR)\context.obj $(OUTDIR)\core_algorithm.obj $(OUTDIR)\core_fetch.obj $(OUTDIR)\core_namemap.obj $(OUTDIR)\cpt_err.obj $(OUTDIR)\cpuid.obj $(OUTDIR)\cryptlib.obj $(OUTDIR)\ctype.obj $(OUTDIR)\cversion.obj $(OUTDIR)\der_writer.obj $(OUTDIR)\ebcdic.obj $(OUTDIR)\ex_data.obj $(OUTDIR)\getenv.obj $(OUTDIR)\info.obj $(OUTDIR)\init.obj $(OUTDIR)\initthread.obj $(OUTDIR)\mem.obj $(OUTDIR)\mem_sec.obj $(OUTDIR)\o_dir.obj $(OUTDIR)\o_fopen.obj $(OUTDIR)\o_init.obj $(OUTDIR)\o_str.obj $(OUTDIR)\o_time.obj $(OUTDIR)\packet.obj $(OUTDIR)\param_build.obj $(OUTDIR)\param_build_set.obj $(OUTDIR)\params.obj $(OUTDIR)\params_dup.obj $(OUTDIR)\params_from_text.obj $(OUTDIR)\passphrase.obj $(OUTDIR)\provider.obj $(OUTDIR)\provider_child.obj $(OUTDIR)\provider_conf.obj $(OUTDIR)\provider_core.obj $(OUTDIR)\provider_predefined.obj $(OUTDIR)\punycode.obj $(OUTDIR)\self_test_core.obj $(OUTDIR)\sparse_array.obj $(OUTDIR)\threads_lib.obj $(OUTDIR)\threads_none.obj $(OUTDIR)\threads_pthread.obj $(OUTDIR)\threads_win.obj $(OUTDIR)\trace.obj $(OUTDIR)\uid.obj $(OUTDIR)\x86_64cpuid.obj $(OUTDIR)\md4_dgst.obj $(OUTDIR)\md4_one.obj $(OUTDIR)\md5-x86_64.obj $(OUTDIR)\md5_dgst.obj $(OUTDIR)\md5_one.obj $(OUTDIR)\md5_sha1.obj $(OUTDIR)\aesni-gcm-x86_64.obj $(OUTDIR)\cbc128.obj $(OUTDIR)\ccm128.obj $(OUTDIR)\cfb128.obj $(OUTDIR)\ctr128.obj $(OUTDIR)\cts128.obj $(OUTDIR)\gcm128.obj $(OUTDIR)\ghash-x86_64.obj $(OUTDIR)\ocb128.obj $(OUTDIR)\ofb128.obj $(OUTDIR)\siv128.obj $(OUTDIR)\wrap128.obj $(OUTDIR)\xts128.obj $(OUTDIR)\o_names.obj $(OUTDIR)\obj_dat.obj $(OUTDIR)\obj_err.obj $(OUTDIR)\obj_lib.obj $(OUTDIR)\obj_xref.obj $(OUTDIR)\ocsp_asn.obj $(OUTDIR)\ocsp_cl.obj $(OUTDIR)\ocsp_err.obj $(OUTDIR)\ocsp_ext.obj $(OUTDIR)\ocsp_http.obj $(OUTDIR)\ocsp_lib.obj $(OUTDIR)\ocsp_prn.obj $(OUTDIR)\ocsp_srv.obj $(OUTDIR)\ocsp_vfy.obj $(OUTDIR)\v3_ocsp.obj $(OUTDIR)\pem_all.obj $(OUTDIR)\pem_err.obj $(OUTDIR)\pem_info.obj $(OUTDIR)\pem_lib.obj $(OUTDIR)\pem_oth.obj $(OUTDIR)\pem_pk8.obj $(OUTDIR)\pem_pkey.obj $(OUTDIR)\pem_sign.obj $(OUTDIR)\pem_x509.obj $(OUTDIR)\pem_xaux.obj $(OUTDIR)\pvkfmt.obj $(OUTDIR)\p12_add.obj $(OUTDIR)\p12_asn.obj $(OUTDIR)\p12_attr.obj $(OUTDIR)\p12_crpt.obj $(OUTDIR)\p12_crt.obj $(OUTDIR)\p12_decr.obj $(OUTDIR)\p12_init.obj $(OUTDIR)\p12_key.obj $(OUTDIR)\p12_kiss.obj $(OUTDIR)\p12_mutl.obj $(OUTDIR)\p12_npas.obj $(OUTDIR)\p12_p8d.obj $(OUTDIR)\p12_p8e.obj $(OUTDIR)\p12_sbag.obj $(OUTDIR)\p12_utl.obj $(OUTDIR)\pk12err.obj $(OUTDIR)\bio_pk7.obj $(OUTDIR)\pk7_asn1.obj $(OUTDIR)\pk7_attr.obj $(OUTDIR)\pk7_doit.obj $(OUTDIR)\pk7_lib.obj $(OUTDIR)\pk7_mime.obj $(OUTDIR)\pk7_smime.obj $(OUTDIR)\pkcs7err.obj $(OUTDIR)\poly1305-x86_64.obj $(OUTDIR)\poly1305.obj $(OUTDIR)\defn_cache.obj $(OUTDIR)\property.obj $(OUTDIR)\property_err.obj $(OUTDIR)\property_parse.obj $(OUTDIR)\property_query.obj $(OUTDIR)\property_string.obj $(OUTDIR)\prov_seed.obj $(OUTDIR)\rand_deprecated.obj $(OUTDIR)\rand_err.obj $(OUTDIR)\rand_lib.obj $(OUTDIR)\rand_meth.obj $(OUTDIR)\rand_pool.obj $(OUTDIR)\randfile.obj $(OUTDIR)\rc2_cbc.obj $(OUTDIR)\rc2_ecb.obj $(OUTDIR)\rc2_skey.obj $(OUTDIR)\rc2cfb64.obj $(OUTDIR)\rc2ofb64.obj $(OUTDIR)\rc4-md5-x86_64.obj $(OUTDIR)\rc4-x86_64.obj $(OUTDIR)\rmd_dgst.obj $(OUTDIR)\rmd_one.obj $(OUTDIR)\rsa_ameth.obj $(OUTDIR)\rsa_asn1.obj $(OUTDIR)\rsa_backend.obj $(OUTDIR)\rsa_chk.obj $(OUTDIR)\rsa_crpt.obj $(OUTDIR)\rsa_depr.obj $(OUTDIR)\rsa_err.obj $(OUTDIR)\rsa_gen.obj $(OUTDIR)\rsa_lib.obj $(OUTDIR)\rsa_meth.obj $(OUTDIR)\rsa_mp.obj $(OUTDIR)\rsa_mp_names.obj $(OUTDIR)\rsa_none.obj $(OUTDIR)\rsa_oaep.obj $(OUTDIR)\rsa_ossl.obj $(OUTDIR)\rsa_pk1.obj $(OUTDIR)\rsa_pmeth.obj $(OUTDIR)\rsa_prn.obj $(OUTDIR)\rsa_pss.obj $(OUTDIR)\rsa_saos.obj $(OUTDIR)\rsa_schemes.obj $(OUTDIR)\rsa_sign.obj $(OUTDIR)\rsa_sp800_56b_check.obj $(OUTDIR)\rsa_sp800_56b_gen.obj $(OUTDIR)\rsa_x931.obj $(OUTDIR)\rsa_x931g.obj $(OUTDIR)\seed.obj $(OUTDIR)\seed_cbc.obj $(OUTDIR)\seed_cfb.obj $(OUTDIR)\seed_ecb.obj $(OUTDIR)\seed_ofb.obj $(OUTDIR)\keccak1600-x86_64.obj $(OUTDIR)\sha1-mb-x86_64.obj $(OUTDIR)\sha1-x86_64.obj $(OUTDIR)\sha1_one.obj $(OUTDIR)\sha1dgst.obj $(OUTDIR)\sha256-mb-x86_64.obj $(OUTDIR)\sha256-x86_64.obj $(OUTDIR)\sha256.obj $(OUTDIR)\sha3.obj $(OUTDIR)\sha512-x86_64.obj $(OUTDIR)\sha512.obj $(OUTDIR)\siphash.obj $(OUTDIR)\legacy_sm3.obj $(OUTDIR)\sm3.obj $(OUTDIR)\stack.obj $(OUTDIR)\store_err.obj $(OUTDIR)\store_init.obj $(OUTDIR)\store_lib.obj $(OUTDIR)\store_meth.obj $(OUTDIR)\store_register.obj $(OUTDIR)\store_result.obj $(OUTDIR)\store_strings.obj $(OUTDIR)\ts_asn1.obj $(OUTDIR)\ts_conf.obj $(OUTDIR)\ts_err.obj $(OUTDIR)\ts_lib.obj $(OUTDIR)\ts_req_print.obj $(OUTDIR)\ts_req_utils.obj $(OUTDIR)\ts_rsp_print.obj $(OUTDIR)\ts_rsp_sign.obj $(OUTDIR)\ts_rsp_utils.obj $(OUTDIR)\ts_rsp_verify.obj $(OUTDIR)\ts_verify_ctx.obj $(OUTDIR)\txt_db.obj $(OUTDIR)\ui_err.obj $(OUTDIR)\ui_lib.obj $(OUTDIR)\ui_null.obj $(OUTDIR)\ui_openssl.obj $(OUTDIR)\ui_util.obj $(OUTDIR)\wp-x86_64.obj $(OUTDIR)\wp_dgst.obj $(OUTDIR)\by_dir.obj $(OUTDIR)\by_file.obj $(OUTDIR)\by_store.obj $(OUTDIR)\pcy_cache.obj $(OUTDIR)\pcy_data.obj $(OUTDIR)\pcy_lib.obj $(OUTDIR)\pcy_map.obj $(OUTDIR)\pcy_node.obj $(OUTDIR)\pcy_tree.obj $(OUTDIR)\t_crl.obj $(OUTDIR)\t_req.obj $(OUTDIR)\t_x509.obj $(OUTDIR)\v3_addr.obj $(OUTDIR)\v3_admis.obj $(OUTDIR)\v3_akeya.obj $(OUTDIR)\v3_akid.obj $(OUTDIR)\v3_asid.obj $(OUTDIR)\v3_bcons.obj $(OUTDIR)\v3_bitst.obj $(OUTDIR)\v3_conf.obj $(OUTDIR)\v3_cpols.obj $(OUTDIR)\v3_crld.obj $(OUTDIR)\v3_enum.obj $(OUTDIR)\v3_extku.obj $(OUTDIR)\v3_genn.obj $(OUTDIR)\v3_ia5.obj $(OUTDIR)\v3_info.obj $(OUTDIR)\v3_int.obj $(OUTDIR)\v3_ist.obj $(OUTDIR)\v3_lib.obj $(OUTDIR)\v3_ncons.obj $(OUTDIR)\v3_pci.obj $(OUTDIR)\v3_pcia.obj $(OUTDIR)\v3_pcons.obj $(OUTDIR)\v3_pku.obj $(OUTDIR)\v3_pmaps.obj $(OUTDIR)\v3_prn.obj $(OUTDIR)\v3_purp.obj $(OUTDIR)\v3_san.obj $(OUTDIR)\v3_skid.obj $(OUTDIR)\v3_sxnet.obj $(OUTDIR)\v3_tlsf.obj $(OUTDIR)\v3_utf8.obj $(OUTDIR)\v3_utl.obj $(OUTDIR)\v3err.obj $(OUTDIR)\x509_att.obj $(OUTDIR)\x509_cmp.obj $(OUTDIR)\x509_d2.obj $(OUTDIR)\x509_def.obj $(OUTDIR)\x509_err.obj $(OUTDIR)\x509_ext.obj $(OUTDIR)\x509_lu.obj $(OUTDIR)\x509_meth.obj $(OUTDIR)\x509_obj.obj $(OUTDIR)\x509_r2x.obj $(OUTDIR)\x509_req.obj $(OUTDIR)\x509_set.obj $(OUTDIR)\x509_trust.obj $(OUTDIR)\x509_txt.obj $(OUTDIR)\x509_v3.obj $(OUTDIR)\x509_vfy.obj $(OUTDIR)\x509_vpm.obj $(OUTDIR)\x509cset.obj $(OUTDIR)\x509name.obj $(OUTDIR)\x509rset.obj $(OUTDIR)\x509spki.obj $(OUTDIR)\x509type.obj $(OUTDIR)\x_all.obj $(OUTDIR)\x_attrib.obj $(OUTDIR)\x_crl.obj $(OUTDIR)\x_exten.obj $(OUTDIR)\x_name.obj $(OUTDIR)\x_pubkey.obj $(OUTDIR)\x_req.obj $(OUTDIR)\x_x509.obj $(OUTDIR)\x_x509a.obj $(OUTDIR)\baseprov.obj $(OUTDIR)\defltprov.obj $(OUTDIR)\nullprov.obj $(OUTDIR)\prov_running.obj $(OUTDIR)\der_rsa_sig.obj $(OUTDIR)\bio_prov.obj $(OUTDIR)\capabilities.obj $(OUTDIR)\digest_to_nid.obj $(OUTDIR)\provider_seeding.obj $(OUTDIR)\provider_util.obj $(OUTDIR)\securitycheck.obj $(OUTDIR)\securitycheck_default.obj $(OUTDIR)\rsa_enc.obj $(OUTDIR)\cipher_aes.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha1_hw.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha256_hw.obj $(OUTDIR)\cipher_aes_ccm.obj $(OUTDIR)\cipher_aes_ccm_hw.obj $(OUTDIR)\cipher_aes_gcm.obj $(OUTDIR)\cipher_aes_gcm_hw.obj $(OUTDIR)\cipher_aes_hw.obj $(OUTDIR)\cipher_aes_ocb.obj $(OUTDIR)\cipher_aes_ocb_hw.obj $(OUTDIR)\cipher_aes_siv.obj $(OUTDIR)\cipher_aes_siv_hw.obj $(OUTDIR)\cipher_aes_wrp.obj $(OUTDIR)\cipher_aes_xts.obj $(OUTDIR)\cipher_aes_xts_fips.obj $(OUTDIR)\cipher_aes_xts_hw.obj $(OUTDIR)\cipher_chacha20.obj $(OUTDIR)\cipher_chacha20_hw.obj $(OUTDIR)\cipher_chacha20_poly1305.obj $(OUTDIR)\cipher_chacha20_poly1305_hw.obj $(OUTDIR)\cipher_cts.obj $(OUTDIR)\cipher_null.obj $(OUTDIR)\cipher_tdes.obj $(OUTDIR)\cipher_tdes_common.obj $(OUTDIR)\cipher_tdes_default.obj $(OUTDIR)\cipher_tdes_default_hw.obj $(OUTDIR)\cipher_tdes_hw.obj $(OUTDIR)\cipher_tdes_wrap.obj $(OUTDIR)\cipher_tdes_wrap_hw.obj $(OUTDIR)\blake2_prov.obj $(OUTDIR)\blake2b_prov.obj $(OUTDIR)\blake2s_prov.obj $(OUTDIR)\md5_prov.obj $(OUTDIR)\md5_sha1_prov.obj $(OUTDIR)\null_prov.obj $(OUTDIR)\ripemd_prov.obj $(OUTDIR)\sha2_prov.obj $(OUTDIR)\sha3_prov.obj $(OUTDIR)\sm3_prov.obj $(OUTDIR)\decode_der2key.obj $(OUTDIR)\decode_epki2pki.obj $(OUTDIR)\decode_msblob2key.obj $(OUTDIR)\decode_pem2der.obj $(OUTDIR)\decode_pvk2key.obj $(OUTDIR)\decode_spki2typespki.obj $(OUTDIR)\encode_key2any.obj $(OUTDIR)\encode_key2blob.obj $(OUTDIR)\encode_key2ms.obj $(OUTDIR)\encode_key2text.obj $(OUTDIR)\endecoder_common.obj $(OUTDIR)\dh_exch.obj $(OUTDIR)\ecdh_exch.obj $(OUTDIR)\ecx_exch.obj $(OUTDIR)\kdf_exch.obj $(OUTDIR)\hkdf.obj $(OUTDIR)\kbkdf.obj $(OUTDIR)\krb5kdf.obj $(OUTDIR)\pbkdf2.obj $(OUTDIR)\pbkdf2_fips.obj $(OUTDIR)\pkcs12kdf.obj $(OUTDIR)\scrypt.obj $(OUTDIR)\sshkdf.obj $(OUTDIR)\sskdf.obj $(OUTDIR)\tls1_prf.obj $(OUTDIR)\x942kdf.obj $(OUTDIR)\rsa_kem.obj $(OUTDIR)\dh_kmgmt.obj $(OUTDIR)\dsa_kmgmt.obj $(OUTDIR)\ec_kmgmt.obj $(OUTDIR)\ecx_kmgmt.obj $(OUTDIR)\kdf_legacy_kmgmt.obj $(OUTDIR)\mac_legacy_kmgmt.obj $(OUTDIR)\rsa_kmgmt.obj $(OUTDIR)\blake2b_mac.obj $(OUTDIR)\blake2s_mac.obj $(OUTDIR)\cmac_prov.obj $(OUTDIR)\gmac_prov.obj $(OUTDIR)\hmac_prov.obj $(OUTDIR)\kmac_prov.obj $(OUTDIR)\poly1305_prov.obj $(OUTDIR)\siphash_prov.obj $(OUTDIR)\crngt.obj $(OUTDIR)\drbg.obj $(OUTDIR)\drbg_ctr.obj $(OUTDIR)\drbg_hash.obj $(OUTDIR)\drbg_hmac.obj $(OUTDIR)\seed_src.obj $(OUTDIR)\test_rng.obj $(OUTDIR)\rand_cpu_x86.obj $(OUTDIR)\rand_tsc.obj $(OUTDIR)\rand_unix.obj $(OUTDIR)\rand_win.obj $(OUTDIR)\dsa_sig.obj $(OUTDIR)\ecdsa_sig.obj $(OUTDIR)\eddsa_sig.obj $(OUTDIR)\mac_legacy_sig.obj $(OUTDIR)\rsa_sig.obj $(OUTDIR)\file_store.obj $(OUTDIR)\file_store_any2obj.obj $(OUTDIR)\s3_cbc.obj $(OUTDIR)\der_digests_gen.obj $(OUTDIR)\der_dsa_gen.obj $(OUTDIR)\der_dsa_key.obj $(OUTDIR)\der_dsa_sig.obj $(OUTDIR)\der_ec_gen.obj $(OUTDIR)\der_ec_key.obj $(OUTDIR)\der_ec_sig.obj $(OUTDIR)\der_ecx_gen.obj $(OUTDIR)\der_ecx_key.obj $(OUTDIR)\der_rsa_gen.obj $(OUTDIR)\der_rsa_key.obj $(OUTDIR)\der_wrap_gen.obj $(OUTDIR)\provider_ctx.obj $(OUTDIR)\provider_err.obj $(OUTDIR)\ciphercommon.obj $(OUTDIR)\ciphercommon_block.obj $(OUTDIR)\ciphercommon_ccm.obj $(OUTDIR)\ciphercommon_ccm_hw.obj $(OUTDIR)\ciphercommon_gcm.obj $(OUTDIR)\ciphercommon_gcm_hw.obj $(OUTDIR)\ciphercommon_hw.obj $(OUTDIR)\digestcommon.obj $(OUTDIR)\tls_pad.obj
	$(AR) $(ARFLAGS) $(AROUTFLAG)$(OUTDIR)\$(LIBCRYPTO) @<<
$(OUTDIR)\aes-x86_64.obj
$(OUTDIR)\aes_cfb.obj
$(OUTDIR)\aes_ecb.obj
$(OUTDIR)\aes_ige.obj
$(OUTDIR)\aes_misc.obj
$(OUTDIR)\aes_ofb.obj
$(OUTDIR)\aes_wrap.obj
$(OUTDIR)\aesni-mb-x86_64.obj
$(OUTDIR)\aesni-sha1-x86_64.obj
$(OUTDIR)\aesni-sha256-x86_64.obj
$(OUTDIR)\aesni-x86_64.obj
$(OUTDIR)\bsaes-x86_64.obj
$(OUTDIR)\vpaes-x86_64.obj
$(OUTDIR)\a_bitstr.obj
$(OUTDIR)\a_d2i_fp.obj
$(OUTDIR)\a_digest.obj
$(OUTDIR)\a_dup.obj
$(OUTDIR)\a_gentm.obj
$(OUTDIR)\a_i2d_fp.obj
$(OUTDIR)\a_int.obj
$(OUTDIR)\a_mbstr.obj
$(OUTDIR)\a_object.obj
$(OUTDIR)\a_octet.obj
$(OUTDIR)\a_print.obj
$(OUTDIR)\a_sign.obj
$(OUTDIR)\a_strex.obj
$(OUTDIR)\a_strnid.obj
$(OUTDIR)\a_time.obj
$(OUTDIR)\a_type.obj
$(OUTDIR)\a_utctm.obj
$(OUTDIR)\a_utf8.obj
$(OUTDIR)\a_verify.obj
$(OUTDIR)\ameth_lib.obj
$(OUTDIR)\asn1_err.obj
$(OUTDIR)\asn1_gen.obj
$(OUTDIR)\asn1_item_list.obj
$(OUTDIR)\asn1_lib.obj
$(OUTDIR)\asn1_parse.obj
$(OUTDIR)\asn_mime.obj
$(OUTDIR)\asn_moid.obj
$(OUTDIR)\asn_mstbl.obj
$(OUTDIR)\asn_pack.obj
$(OUTDIR)\bio_asn1.obj
$(OUTDIR)\bio_ndef.obj
$(OUTDIR)\d2i_param.obj
$(OUTDIR)\d2i_pr.obj
$(OUTDIR)\d2i_pu.obj
$(OUTDIR)\evp_asn1.obj
$(OUTDIR)\f_int.obj
$(OUTDIR)\f_string.obj
$(OUTDIR)\i2d_evp.obj
$(OUTDIR)\n_pkey.obj
$(OUTDIR)\nsseq.obj
$(OUTDIR)\p5_pbe.obj
$(OUTDIR)\p5_pbev2.obj
$(OUTDIR)\p5_scrypt.obj
$(OUTDIR)\p8_pkey.obj
$(OUTDIR)\t_bitst.obj
$(OUTDIR)\t_pkey.obj
$(OUTDIR)\t_spki.obj
$(OUTDIR)\tasn_dec.obj
$(OUTDIR)\tasn_enc.obj
$(OUTDIR)\tasn_fre.obj
$(OUTDIR)\tasn_new.obj
$(OUTDIR)\tasn_prn.obj
$(OUTDIR)\tasn_scn.obj
$(OUTDIR)\tasn_typ.obj
$(OUTDIR)\tasn_utl.obj
$(OUTDIR)\x_algor.obj
$(OUTDIR)\x_bignum.obj
$(OUTDIR)\x_info.obj
$(OUTDIR)\x_int64.obj
$(OUTDIR)\x_long.obj
$(OUTDIR)\x_pkey.obj
$(OUTDIR)\x_sig.obj
$(OUTDIR)\x_spki.obj
$(OUTDIR)\x_val.obj
$(OUTDIR)\async_null.obj
$(OUTDIR)\async_posix.obj
$(OUTDIR)\async_win.obj
$(OUTDIR)\async.obj
$(OUTDIR)\async_err.obj
$(OUTDIR)\async_wait.obj
$(OUTDIR)\bf_cfb64.obj
$(OUTDIR)\bf_ecb.obj
$(OUTDIR)\bf_enc.obj
$(OUTDIR)\bf_ofb64.obj
$(OUTDIR)\bf_skey.obj
$(OUTDIR)\bf_buff.obj
$(OUTDIR)\bf_lbuf.obj
$(OUTDIR)\bf_nbio.obj
$(OUTDIR)\bf_null.obj
$(OUTDIR)\bf_prefix.obj
$(OUTDIR)\bf_readbuff.obj
$(OUTDIR)\bio_addr.obj
$(OUTDIR)\bio_cb.obj
$(OUTDIR)\bio_dump.obj
$(OUTDIR)\bio_err.obj
$(OUTDIR)\bio_lib.obj
$(OUTDIR)\bio_meth.obj
$(OUTDIR)\bio_print.obj
$(OUTDIR)\bio_sock.obj
$(OUTDIR)\bio_sock2.obj
$(OUTDIR)\bss_acpt.obj
$(OUTDIR)\bss_bio.obj
$(OUTDIR)\bss_conn.obj
$(OUTDIR)\bss_core.obj
$(OUTDIR)\bss_dgram.obj
$(OUTDIR)\bss_fd.obj
$(OUTDIR)\bss_file.obj
$(OUTDIR)\bss_log.obj
$(OUTDIR)\bss_mem.obj
$(OUTDIR)\bss_null.obj
$(OUTDIR)\bss_sock.obj
$(OUTDIR)\ossl_core_bio.obj
$(OUTDIR)\bn_add.obj
$(OUTDIR)\bn_asm.obj
$(OUTDIR)\bn_blind.obj
$(OUTDIR)\bn_const.obj
$(OUTDIR)\bn_conv.obj
$(OUTDIR)\bn_ctx.obj
$(OUTDIR)\bn_depr.obj
$(OUTDIR)\bn_dh.obj
$(OUTDIR)\bn_div.obj
$(OUTDIR)\bn_err.obj
$(OUTDIR)\bn_exp.obj
$(OUTDIR)\bn_exp2.obj
$(OUTDIR)\bn_gcd.obj
$(OUTDIR)\bn_gf2m.obj
$(OUTDIR)\bn_intern.obj
$(OUTDIR)\bn_kron.obj
$(OUTDIR)\bn_lib.obj
$(OUTDIR)\bn_mod.obj
$(OUTDIR)\bn_mont.obj
$(OUTDIR)\bn_mpi.obj
$(OUTDIR)\bn_mul.obj
$(OUTDIR)\bn_nist.obj
$(OUTDIR)\bn_prime.obj
$(OUTDIR)\bn_print.obj
$(OUTDIR)\bn_rand.obj
$(OUTDIR)\bn_recp.obj
$(OUTDIR)\bn_rsa_fips186_4.obj
$(OUTDIR)\bn_shift.obj
$(OUTDIR)\bn_sqr.obj
$(OUTDIR)\bn_sqrt.obj
$(OUTDIR)\bn_srp.obj
$(OUTDIR)\bn_word.obj
$(OUTDIR)\bn_x931p.obj
$(OUTDIR)\rsaz-avx2.obj
$(OUTDIR)\rsaz-avx512.obj
$(OUTDIR)\rsaz-x86_64.obj
$(OUTDIR)\rsaz_exp.obj
$(OUTDIR)\rsaz_exp_x2.obj
$(OUTDIR)\x86_64-gf2m.obj
$(OUTDIR)\x86_64-mont.obj
$(OUTDIR)\x86_64-mont5.obj
$(OUTDIR)\buf_err.obj
$(OUTDIR)\buffer.obj
$(OUTDIR)\chacha-x86_64.obj
$(OUTDIR)\cmac.obj
$(OUTDIR)\cmp_asn.obj
$(OUTDIR)\cmp_client.obj
$(OUTDIR)\cmp_ctx.obj
$(OUTDIR)\cmp_err.obj
$(OUTDIR)\cmp_hdr.obj
$(OUTDIR)\cmp_http.obj
$(OUTDIR)\cmp_msg.obj
$(OUTDIR)\cmp_protect.obj
$(OUTDIR)\cmp_server.obj
$(OUTDIR)\cmp_status.obj
$(OUTDIR)\cmp_util.obj
$(OUTDIR)\cmp_vfy.obj
$(OUTDIR)\cms_asn1.obj
$(OUTDIR)\cms_att.obj
$(OUTDIR)\cms_cd.obj
$(OUTDIR)\cms_dd.obj
$(OUTDIR)\cms_dh.obj
$(OUTDIR)\cms_ec.obj
$(OUTDIR)\cms_enc.obj
$(OUTDIR)\cms_env.obj
$(OUTDIR)\cms_err.obj
$(OUTDIR)\cms_ess.obj
$(OUTDIR)\cms_io.obj
$(OUTDIR)\cms_kari.obj
$(OUTDIR)\cms_lib.obj
$(OUTDIR)\cms_pwri.obj
$(OUTDIR)\cms_rsa.obj
$(OUTDIR)\cms_sd.obj
$(OUTDIR)\cms_smime.obj
$(OUTDIR)\c_zlib.obj
$(OUTDIR)\comp_err.obj
$(OUTDIR)\comp_lib.obj
$(OUTDIR)\conf_api.obj
$(OUTDIR)\conf_def.obj
$(OUTDIR)\conf_err.obj
$(OUTDIR)\conf_lib.obj
$(OUTDIR)\conf_mall.obj
$(OUTDIR)\conf_mod.obj
$(OUTDIR)\conf_sap.obj
$(OUTDIR)\conf_ssl.obj
$(OUTDIR)\crmf_asn.obj
$(OUTDIR)\crmf_err.obj
$(OUTDIR)\crmf_lib.obj
$(OUTDIR)\crmf_pbm.obj
$(OUTDIR)\ct_b64.obj
$(OUTDIR)\ct_err.obj
$(OUTDIR)\ct_log.obj
$(OUTDIR)\ct_oct.obj
$(OUTDIR)\ct_policy.obj
$(OUTDIR)\ct_prn.obj
$(OUTDIR)\ct_sct.obj
$(OUTDIR)\ct_sct_ctx.obj
$(OUTDIR)\ct_vfy.obj
$(OUTDIR)\ct_x509v3.obj
$(OUTDIR)\cbc_cksm.obj
$(OUTDIR)\cbc_enc.obj
$(OUTDIR)\cfb64ede.obj
$(OUTDIR)\cfb64enc.obj
$(OUTDIR)\cfb_enc.obj
$(OUTDIR)\des_enc.obj
$(OUTDIR)\ecb3_enc.obj
$(OUTDIR)\ecb_enc.obj
$(OUTDIR)\fcrypt.obj
$(OUTDIR)\fcrypt_b.obj
$(OUTDIR)\ofb64ede.obj
$(OUTDIR)\ofb64enc.obj
$(OUTDIR)\ofb_enc.obj
$(OUTDIR)\pcbc_enc.obj
$(OUTDIR)\qud_cksm.obj
$(OUTDIR)\rand_key.obj
$(OUTDIR)\set_key.obj
$(OUTDIR)\str2key.obj
$(OUTDIR)\xcbc_enc.obj
$(OUTDIR)\dh_ameth.obj
$(OUTDIR)\dh_asn1.obj
$(OUTDIR)\dh_backend.obj
$(OUTDIR)\dh_check.obj
$(OUTDIR)\dh_depr.obj
$(OUTDIR)\dh_err.obj
$(OUTDIR)\dh_gen.obj
$(OUTDIR)\dh_group_params.obj
$(OUTDIR)\dh_kdf.obj
$(OUTDIR)\dh_key.obj
$(OUTDIR)\dh_lib.obj
$(OUTDIR)\dh_meth.obj
$(OUTDIR)\dh_pmeth.obj
$(OUTDIR)\dh_prn.obj
$(OUTDIR)\dh_rfc5114.obj
$(OUTDIR)\dsa_ameth.obj
$(OUTDIR)\dsa_asn1.obj
$(OUTDIR)\dsa_backend.obj
$(OUTDIR)\dsa_check.obj
$(OUTDIR)\dsa_depr.obj
$(OUTDIR)\dsa_err.obj
$(OUTDIR)\dsa_gen.obj
$(OUTDIR)\dsa_key.obj
$(OUTDIR)\dsa_lib.obj
$(OUTDIR)\dsa_meth.obj
$(OUTDIR)\dsa_ossl.obj
$(OUTDIR)\dsa_pmeth.obj
$(OUTDIR)\dsa_prn.obj
$(OUTDIR)\dsa_sign.obj
$(OUTDIR)\dsa_vrf.obj
$(OUTDIR)\dso_dl.obj
$(OUTDIR)\dso_dlfcn.obj
$(OUTDIR)\dso_err.obj
$(OUTDIR)\dso_lib.obj
$(OUTDIR)\dso_openssl.obj
$(OUTDIR)\dso_vms.obj
$(OUTDIR)\dso_win32.obj
$(OUTDIR)\f_impl32.obj
$(OUTDIR)\f_impl64.obj
$(OUTDIR)\curve448.obj
$(OUTDIR)\curve448_tables.obj
$(OUTDIR)\eddsa.obj
$(OUTDIR)\f_generic.obj
$(OUTDIR)\scalar.obj
$(OUTDIR)\curve25519.obj
$(OUTDIR)\ec2_oct.obj
$(OUTDIR)\ec2_smpl.obj
$(OUTDIR)\ec_ameth.obj
$(OUTDIR)\ec_asn1.obj
$(OUTDIR)\ec_backend.obj
$(OUTDIR)\ec_check.obj
$(OUTDIR)\ec_curve.obj
$(OUTDIR)\ec_cvt.obj
$(OUTDIR)\ec_deprecated.obj
$(OUTDIR)\ec_err.obj
$(OUTDIR)\ec_key.obj
$(OUTDIR)\ec_kmeth.obj
$(OUTDIR)\ec_lib.obj
$(OUTDIR)\ec_mult.obj
$(OUTDIR)\ec_oct.obj
$(OUTDIR)\ec_pmeth.obj
$(OUTDIR)\ec_print.obj
$(OUTDIR)\ecdh_kdf.obj
$(OUTDIR)\ecdh_ossl.obj
$(OUTDIR)\ecdsa_ossl.obj
$(OUTDIR)\ecdsa_sign.obj
$(OUTDIR)\ecdsa_vrf.obj
$(OUTDIR)\eck_prn.obj
$(OUTDIR)\ecp_mont.obj
$(OUTDIR)\ecp_nist.obj
$(OUTDIR)\ecp_nistz256-x86_64.obj
$(OUTDIR)\ecp_nistz256.obj
$(OUTDIR)\ecp_oct.obj
$(OUTDIR)\ecp_smpl.obj
$(OUTDIR)\ecx_backend.obj
$(OUTDIR)\ecx_key.obj
$(OUTDIR)\ecx_meth.obj
$(OUTDIR)\x25519-x86_64.obj
$(OUTDIR)\decoder_err.obj
$(OUTDIR)\decoder_lib.obj
$(OUTDIR)\decoder_meth.obj
$(OUTDIR)\decoder_pkey.obj
$(OUTDIR)\encoder_err.obj
$(OUTDIR)\encoder_lib.obj
$(OUTDIR)\encoder_meth.obj
$(OUTDIR)\encoder_pkey.obj
$(OUTDIR)\err.obj
$(OUTDIR)\err_all.obj
$(OUTDIR)\err_all_legacy.obj
$(OUTDIR)\err_blocks.obj
$(OUTDIR)\err_prn.obj
$(OUTDIR)\ess_asn1.obj
$(OUTDIR)\ess_err.obj
$(OUTDIR)\ess_lib.obj
$(OUTDIR)\asymcipher.obj
$(OUTDIR)\bio_b64.obj
$(OUTDIR)\bio_enc.obj
$(OUTDIR)\bio_md.obj
$(OUTDIR)\bio_ok.obj
$(OUTDIR)\c_allc.obj
$(OUTDIR)\c_alld.obj
$(OUTDIR)\cmeth_lib.obj
$(OUTDIR)\ctrl_params_translate.obj
$(OUTDIR)\dh_ctrl.obj
$(OUTDIR)\dh_support.obj
$(OUTDIR)\digest.obj
$(OUTDIR)\dsa_ctrl.obj
$(OUTDIR)\e_aes.obj
$(OUTDIR)\e_aes_cbc_hmac_sha1.obj
$(OUTDIR)\e_aes_cbc_hmac_sha256.obj
$(OUTDIR)\e_aria.obj
$(OUTDIR)\e_bf.obj
$(OUTDIR)\e_cast.obj
$(OUTDIR)\e_chacha20_poly1305.obj
$(OUTDIR)\e_des.obj
$(OUTDIR)\e_des3.obj
$(OUTDIR)\e_idea.obj
$(OUTDIR)\e_null.obj
$(OUTDIR)\e_old.obj
$(OUTDIR)\e_rc2.obj
$(OUTDIR)\e_rc4.obj
$(OUTDIR)\e_rc4_hmac_md5.obj
$(OUTDIR)\e_rc5.obj
$(OUTDIR)\e_seed.obj
$(OUTDIR)\e_sm4.obj
$(OUTDIR)\e_xcbc_d.obj
$(OUTDIR)\ec_ctrl.obj
$(OUTDIR)\ec_support.obj
$(OUTDIR)\encode.obj
$(OUTDIR)\evp_cnf.obj
$(OUTDIR)\evp_enc.obj
$(OUTDIR)\evp_err.obj
$(OUTDIR)\evp_fetch.obj
$(OUTDIR)\evp_key.obj
$(OUTDIR)\evp_lib.obj
$(OUTDIR)\evp_pbe.obj
$(OUTDIR)\evp_pkey.obj
$(OUTDIR)\evp_rand.obj
$(OUTDIR)\evp_utils.obj
$(OUTDIR)\exchange.obj
$(OUTDIR)\kdf_lib.obj
$(OUTDIR)\kdf_meth.obj
$(OUTDIR)\kem.obj
$(OUTDIR)\keymgmt_lib.obj
$(OUTDIR)\keymgmt_meth.obj
$(OUTDIR)\legacy_blake2.obj
$(OUTDIR)\legacy_md4.obj
$(OUTDIR)\legacy_md5.obj
$(OUTDIR)\legacy_md5_sha1.obj
$(OUTDIR)\legacy_ripemd.obj
$(OUTDIR)\legacy_sha.obj
$(OUTDIR)\legacy_wp.obj
$(OUTDIR)\m_null.obj
$(OUTDIR)\m_sigver.obj
$(OUTDIR)\mac_lib.obj
$(OUTDIR)\mac_meth.obj
$(OUTDIR)\names.obj
$(OUTDIR)\p5_crpt.obj
$(OUTDIR)\p5_crpt2.obj
$(OUTDIR)\p_dec.obj
$(OUTDIR)\p_enc.obj
$(OUTDIR)\p_legacy.obj
$(OUTDIR)\p_lib.obj
$(OUTDIR)\p_open.obj
$(OUTDIR)\p_seal.obj
$(OUTDIR)\p_sign.obj
$(OUTDIR)\p_verify.obj
$(OUTDIR)\pbe_scrypt.obj
$(OUTDIR)\pmeth_check.obj
$(OUTDIR)\pmeth_gn.obj
$(OUTDIR)\pmeth_lib.obj
$(OUTDIR)\signature.obj
$(OUTDIR)\ffc_backend.obj
$(OUTDIR)\ffc_dh.obj
$(OUTDIR)\ffc_key_generate.obj
$(OUTDIR)\ffc_key_validate.obj
$(OUTDIR)\ffc_params.obj
$(OUTDIR)\ffc_params_generate.obj
$(OUTDIR)\ffc_params_validate.obj
$(OUTDIR)\hmac.obj
$(OUTDIR)\http_client.obj
$(OUTDIR)\http_err.obj
$(OUTDIR)\http_lib.obj
$(OUTDIR)\i_cbc.obj
$(OUTDIR)\i_cfb64.obj
$(OUTDIR)\i_ecb.obj
$(OUTDIR)\i_ofb64.obj
$(OUTDIR)\i_skey.obj
$(OUTDIR)\kdf_err.obj
$(OUTDIR)\lh_stats.obj
$(OUTDIR)\lhash.obj
$(OUTDIR)\asn1_dsa.obj
$(OUTDIR)\bsearch.obj
$(OUTDIR)\context.obj
$(OUTDIR)\core_algorithm.obj
$(OUTDIR)\core_fetch.obj
$(OUTDIR)\core_namemap.obj
$(OUTDIR)\cpt_err.obj
$(OUTDIR)\cpuid.obj
$(OUTDIR)\cryptlib.obj
$(OUTDIR)\ctype.obj
$(OUTDIR)\cversion.obj
$(OUTDIR)\der_writer.obj
$(OUTDIR)\ebcdic.obj
$(OUTDIR)\ex_data.obj
$(OUTDIR)\getenv.obj
$(OUTDIR)\info.obj
$(OUTDIR)\init.obj
$(OUTDIR)\initthread.obj
$(OUTDIR)\mem.obj
$(OUTDIR)\mem_sec.obj
$(OUTDIR)\o_dir.obj
$(OUTDIR)\o_fopen.obj
$(OUTDIR)\o_init.obj
$(OUTDIR)\o_str.obj
$(OUTDIR)\o_time.obj
$(OUTDIR)\packet.obj
$(OUTDIR)\param_build.obj
$(OUTDIR)\param_build_set.obj
$(OUTDIR)\params.obj
$(OUTDIR)\params_dup.obj
$(OUTDIR)\params_from_text.obj
$(OUTDIR)\passphrase.obj
$(OUTDIR)\provider.obj
$(OUTDIR)\provider_child.obj
$(OUTDIR)\provider_conf.obj
$(OUTDIR)\provider_core.obj
$(OUTDIR)\provider_predefined.obj
$(OUTDIR)\punycode.obj
$(OUTDIR)\self_test_core.obj
$(OUTDIR)\sparse_array.obj
$(OUTDIR)\threads_lib.obj
$(OUTDIR)\threads_none.obj
$(OUTDIR)\threads_pthread.obj
$(OUTDIR)\threads_win.obj
$(OUTDIR)\trace.obj
$(OUTDIR)\uid.obj
$(OUTDIR)\x86_64cpuid.obj
$(OUTDIR)\md4_dgst.obj
$(OUTDIR)\md4_one.obj
$(OUTDIR)\md5-x86_64.obj
$(OUTDIR)\md5_dgst.obj
$(OUTDIR)\md5_one.obj
$(OUTDIR)\md5_sha1.obj
$(OUTDIR)\aesni-gcm-x86_64.obj
$(OUTDIR)\cbc128.obj
$(OUTDIR)\ccm128.obj
$(OUTDIR)\cfb128.obj
$(OUTDIR)\ctr128.obj
$(OUTDIR)\cts128.obj
$(OUTDIR)\gcm128.obj
$(OUTDIR)\ghash-x86_64.obj
$(OUTDIR)\ocb128.obj
$(OUTDIR)\ofb128.obj
$(OUTDIR)\siv128.obj
$(OUTDIR)\wrap128.obj
$(OUTDIR)\xts128.obj
$(OUTDIR)\o_names.obj
$(OUTDIR)\obj_dat.obj
$(OUTDIR)\obj_err.obj
$(OUTDIR)\obj_lib.obj
$(OUTDIR)\obj_xref.obj
$(OUTDIR)\ocsp_asn.obj
$(OUTDIR)\ocsp_cl.obj
$(OUTDIR)\ocsp_err.obj
$(OUTDIR)\ocsp_ext.obj
$(OUTDIR)\ocsp_http.obj
$(OUTDIR)\ocsp_lib.obj
$(OUTDIR)\ocsp_prn.obj
$(OUTDIR)\ocsp_srv.obj
$(OUTDIR)\ocsp_vfy.obj
$(OUTDIR)\v3_ocsp.obj
$(OUTDIR)\pem_all.obj
$(OUTDIR)\pem_err.obj
$(OUTDIR)\pem_info.obj
$(OUTDIR)\pem_lib.obj
$(OUTDIR)\pem_oth.obj
$(OUTDIR)\pem_pk8.obj
$(OUTDIR)\pem_pkey.obj
$(OUTDIR)\pem_sign.obj
$(OUTDIR)\pem_x509.obj
$(OUTDIR)\pem_xaux.obj
$(OUTDIR)\pvkfmt.obj
$(OUTDIR)\p12_add.obj
$(OUTDIR)\p12_asn.obj
$(OUTDIR)\p12_attr.obj
$(OUTDIR)\p12_crpt.obj
$(OUTDIR)\p12_crt.obj
$(OUTDIR)\p12_decr.obj
$(OUTDIR)\p12_init.obj
$(OUTDIR)\p12_key.obj
$(OUTDIR)\p12_kiss.obj
$(OUTDIR)\p12_mutl.obj
$(OUTDIR)\p12_npas.obj
$(OUTDIR)\p12_p8d.obj
$(OUTDIR)\p12_p8e.obj
$(OUTDIR)\p12_sbag.obj
$(OUTDIR)\p12_utl.obj
$(OUTDIR)\pk12err.obj
$(OUTDIR)\bio_pk7.obj
$(OUTDIR)\pk7_asn1.obj
$(OUTDIR)\pk7_attr.obj
$(OUTDIR)\pk7_doit.obj
$(OUTDIR)\pk7_lib.obj
$(OUTDIR)\pk7_mime.obj
$(OUTDIR)\pk7_smime.obj
$(OUTDIR)\pkcs7err.obj
$(OUTDIR)\poly1305-x86_64.obj
$(OUTDIR)\poly1305.obj
$(OUTDIR)\defn_cache.obj
$(OUTDIR)\property.obj
$(OUTDIR)\property_err.obj
$(OUTDIR)\property_parse.obj
$(OUTDIR)\property_query.obj
$(OUTDIR)\property_string.obj
$(OUTDIR)\prov_seed.obj
$(OUTDIR)\rand_deprecated.obj
$(OUTDIR)\rand_err.obj
$(OUTDIR)\rand_lib.obj
$(OUTDIR)\rand_meth.obj
$(OUTDIR)\rand_pool.obj
$(OUTDIR)\randfile.obj
$(OUTDIR)\rc2_cbc.obj
$(OUTDIR)\rc2_ecb.obj
$(OUTDIR)\rc2_skey.obj
$(OUTDIR)\rc2cfb64.obj
$(OUTDIR)\rc2ofb64.obj
$(OUTDIR)\rc4-md5-x86_64.obj
$(OUTDIR)\rc4-x86_64.obj
$(OUTDIR)\rmd_dgst.obj
$(OUTDIR)\rmd_one.obj
$(OUTDIR)\rsa_ameth.obj
$(OUTDIR)\rsa_asn1.obj
$(OUTDIR)\rsa_backend.obj
$(OUTDIR)\rsa_chk.obj
$(OUTDIR)\rsa_crpt.obj
$(OUTDIR)\rsa_depr.obj
$(OUTDIR)\rsa_err.obj
$(OUTDIR)\rsa_gen.obj
$(OUTDIR)\rsa_lib.obj
$(OUTDIR)\rsa_meth.obj
$(OUTDIR)\rsa_mp.obj
$(OUTDIR)\rsa_mp_names.obj
$(OUTDIR)\rsa_none.obj
$(OUTDIR)\rsa_oaep.obj
$(OUTDIR)\rsa_ossl.obj
$(OUTDIR)\rsa_pk1.obj
$(OUTDIR)\rsa_pmeth.obj
$(OUTDIR)\rsa_prn.obj
$(OUTDIR)\rsa_pss.obj
$(OUTDIR)\rsa_saos.obj
$(OUTDIR)\rsa_schemes.obj
$(OUTDIR)\rsa_sign.obj
$(OUTDIR)\rsa_sp800_56b_check.obj
$(OUTDIR)\rsa_sp800_56b_gen.obj
$(OUTDIR)\rsa_x931.obj
$(OUTDIR)\rsa_x931g.obj
$(OUTDIR)\seed.obj
$(OUTDIR)\seed_cbc.obj
$(OUTDIR)\seed_cfb.obj
$(OUTDIR)\seed_ecb.obj
$(OUTDIR)\seed_ofb.obj
$(OUTDIR)\keccak1600-x86_64.obj
$(OUTDIR)\sha1-mb-x86_64.obj
$(OUTDIR)\sha1-x86_64.obj
$(OUTDIR)\sha1_one.obj
$(OUTDIR)\sha1dgst.obj
$(OUTDIR)\sha256-mb-x86_64.obj
$(OUTDIR)\sha256-x86_64.obj
$(OUTDIR)\sha256.obj
$(OUTDIR)\sha3.obj
$(OUTDIR)\sha512-x86_64.obj
$(OUTDIR)\sha512.obj
$(OUTDIR)\siphash.obj
$(OUTDIR)\legacy_sm3.obj
$(OUTDIR)\sm3.obj
$(OUTDIR)\stack.obj
$(OUTDIR)\store_err.obj
$(OUTDIR)\store_init.obj
$(OUTDIR)\store_lib.obj
$(OUTDIR)\store_meth.obj
$(OUTDIR)\store_register.obj
$(OUTDIR)\store_result.obj
$(OUTDIR)\store_strings.obj
$(OUTDIR)\ts_asn1.obj
$(OUTDIR)\ts_conf.obj
$(OUTDIR)\ts_err.obj
$(OUTDIR)\ts_lib.obj
$(OUTDIR)\ts_req_print.obj
$(OUTDIR)\ts_req_utils.obj
$(OUTDIR)\ts_rsp_print.obj
$(OUTDIR)\ts_rsp_sign.obj
$(OUTDIR)\ts_rsp_utils.obj
$(OUTDIR)\ts_rsp_verify.obj
$(OUTDIR)\ts_verify_ctx.obj
$(OUTDIR)\txt_db.obj
$(OUTDIR)\ui_err.obj
$(OUTDIR)\ui_lib.obj
$(OUTDIR)\ui_null.obj
$(OUTDIR)\ui_openssl.obj
$(OUTDIR)\ui_util.obj
$(OUTDIR)\wp-x86_64.obj
$(OUTDIR)\wp_dgst.obj
$(OUTDIR)\by_dir.obj
$(OUTDIR)\by_file.obj
$(OUTDIR)\by_store.obj
$(OUTDIR)\pcy_cache.obj
$(OUTDIR)\pcy_data.obj
$(OUTDIR)\pcy_lib.obj
$(OUTDIR)\pcy_map.obj
$(OUTDIR)\pcy_node.obj
$(OUTDIR)\pcy_tree.obj
$(OUTDIR)\t_crl.obj
$(OUTDIR)\t_req.obj
$(OUTDIR)\t_x509.obj
$(OUTDIR)\v3_addr.obj
$(OUTDIR)\v3_admis.obj
$(OUTDIR)\v3_akeya.obj
$(OUTDIR)\v3_akid.obj
$(OUTDIR)\v3_asid.obj
$(OUTDIR)\v3_bcons.obj
$(OUTDIR)\v3_bitst.obj
$(OUTDIR)\v3_conf.obj
$(OUTDIR)\v3_cpols.obj
$(OUTDIR)\v3_crld.obj
$(OUTDIR)\v3_enum.obj
$(OUTDIR)\v3_extku.obj
$(OUTDIR)\v3_genn.obj
$(OUTDIR)\v3_ia5.obj
$(OUTDIR)\v3_info.obj
$(OUTDIR)\v3_int.obj
$(OUTDIR)\v3_ist.obj
$(OUTDIR)\v3_lib.obj
$(OUTDIR)\v3_ncons.obj
$(OUTDIR)\v3_pci.obj
$(OUTDIR)\v3_pcia.obj
$(OUTDIR)\v3_pcons.obj
$(OUTDIR)\v3_pku.obj
$(OUTDIR)\v3_pmaps.obj
$(OUTDIR)\v3_prn.obj
$(OUTDIR)\v3_purp.obj
$(OUTDIR)\v3_san.obj
$(OUTDIR)\v3_skid.obj
$(OUTDIR)\v3_sxnet.obj
$(OUTDIR)\v3_tlsf.obj
$(OUTDIR)\v3_utf8.obj
$(OUTDIR)\v3_utl.obj
$(OUTDIR)\v3err.obj
$(OUTDIR)\x509_att.obj
$(OUTDIR)\x509_cmp.obj
$(OUTDIR)\x509_d2.obj
$(OUTDIR)\x509_def.obj
$(OUTDIR)\x509_err.obj
$(OUTDIR)\x509_ext.obj
$(OUTDIR)\x509_lu.obj
$(OUTDIR)\x509_meth.obj
$(OUTDIR)\x509_obj.obj
$(OUTDIR)\x509_r2x.obj
$(OUTDIR)\x509_req.obj
$(OUTDIR)\x509_set.obj
$(OUTDIR)\x509_trust.obj
$(OUTDIR)\x509_txt.obj
$(OUTDIR)\x509_v3.obj
$(OUTDIR)\x509_vfy.obj
$(OUTDIR)\x509_vpm.obj
$(OUTDIR)\x509cset.obj
$(OUTDIR)\x509name.obj
$(OUTDIR)\x509rset.obj
$(OUTDIR)\x509spki.obj
$(OUTDIR)\x509type.obj
$(OUTDIR)\x_all.obj
$(OUTDIR)\x_attrib.obj
$(OUTDIR)\x_crl.obj
$(OUTDIR)\x_exten.obj
$(OUTDIR)\x_name.obj
$(OUTDIR)\x_pubkey.obj
$(OUTDIR)\x_req.obj
$(OUTDIR)\x_x509.obj
$(OUTDIR)\x_x509a.obj
$(OUTDIR)\baseprov.obj
$(OUTDIR)\defltprov.obj
$(OUTDIR)\nullprov.obj
$(OUTDIR)\prov_running.obj
$(OUTDIR)\der_rsa_sig.obj
$(OUTDIR)\bio_prov.obj
$(OUTDIR)\capabilities.obj
$(OUTDIR)\digest_to_nid.obj
$(OUTDIR)\provider_seeding.obj
$(OUTDIR)\provider_util.obj
$(OUTDIR)\securitycheck.obj
$(OUTDIR)\securitycheck_default.obj
$(OUTDIR)\rsa_enc.obj
$(OUTDIR)\cipher_aes.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha1_hw.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha256_hw.obj
$(OUTDIR)\cipher_aes_ccm.obj
$(OUTDIR)\cipher_aes_ccm_hw.obj
$(OUTDIR)\cipher_aes_gcm.obj
$(OUTDIR)\cipher_aes_gcm_hw.obj
$(OUTDIR)\cipher_aes_hw.obj
$(OUTDIR)\cipher_aes_ocb.obj
$(OUTDIR)\cipher_aes_ocb_hw.obj
$(OUTDIR)\cipher_aes_siv.obj
$(OUTDIR)\cipher_aes_siv_hw.obj
$(OUTDIR)\cipher_aes_wrp.obj
$(OUTDIR)\cipher_aes_xts.obj
$(OUTDIR)\cipher_aes_xts_fips.obj
$(OUTDIR)\cipher_aes_xts_hw.obj
$(OUTDIR)\cipher_chacha20.obj
$(OUTDIR)\cipher_chacha20_hw.obj
$(OUTDIR)\cipher_chacha20_poly1305.obj
$(OUTDIR)\cipher_chacha20_poly1305_hw.obj
$(OUTDIR)\cipher_cts.obj
$(OUTDIR)\cipher_null.obj
$(OUTDIR)\cipher_tdes.obj
$(OUTDIR)\cipher_tdes_common.obj
$(OUTDIR)\cipher_tdes_default.obj
$(OUTDIR)\cipher_tdes_default_hw.obj
$(OUTDIR)\cipher_tdes_hw.obj
$(OUTDIR)\cipher_tdes_wrap.obj
$(OUTDIR)\cipher_tdes_wrap_hw.obj
$(OUTDIR)\blake2_prov.obj
$(OUTDIR)\blake2b_prov.obj
$(OUTDIR)\blake2s_prov.obj
$(OUTDIR)\md5_prov.obj
$(OUTDIR)\md5_sha1_prov.obj
$(OUTDIR)\null_prov.obj
$(OUTDIR)\ripemd_prov.obj
$(OUTDIR)\sha2_prov.obj
$(OUTDIR)\sha3_prov.obj
$(OUTDIR)\sm3_prov.obj
$(OUTDIR)\decode_der2key.obj
$(OUTDIR)\decode_epki2pki.obj
$(OUTDIR)\decode_msblob2key.obj
$(OUTDIR)\decode_pem2der.obj
$(OUTDIR)\decode_pvk2key.obj
$(OUTDIR)\decode_spki2typespki.obj
$(OUTDIR)\encode_key2any.obj
$(OUTDIR)\encode_key2blob.obj
$(OUTDIR)\encode_key2ms.obj
$(OUTDIR)\encode_key2text.obj
$(OUTDIR)\endecoder_common.obj
$(OUTDIR)\dh_exch.obj
$(OUTDIR)\ecdh_exch.obj
$(OUTDIR)\ecx_exch.obj
$(OUTDIR)\kdf_exch.obj
$(OUTDIR)\hkdf.obj
$(OUTDIR)\kbkdf.obj
$(OUTDIR)\krb5kdf.obj
$(OUTDIR)\pbkdf2.obj
$(OUTDIR)\pbkdf2_fips.obj
$(OUTDIR)\pkcs12kdf.obj
$(OUTDIR)\scrypt.obj
$(OUTDIR)\sshkdf.obj
$(OUTDIR)\sskdf.obj
$(OUTDIR)\tls1_prf.obj
$(OUTDIR)\x942kdf.obj
$(OUTDIR)\rsa_kem.obj
$(OUTDIR)\dh_kmgmt.obj
$(OUTDIR)\dsa_kmgmt.obj
$(OUTDIR)\ec_kmgmt.obj
$(OUTDIR)\ecx_kmgmt.obj
$(OUTDIR)\kdf_legacy_kmgmt.obj
$(OUTDIR)\mac_legacy_kmgmt.obj
$(OUTDIR)\rsa_kmgmt.obj
$(OUTDIR)\blake2b_mac.obj
$(OUTDIR)\blake2s_mac.obj
$(OUTDIR)\cmac_prov.obj
$(OUTDIR)\gmac_prov.obj
$(OUTDIR)\hmac_prov.obj
$(OUTDIR)\kmac_prov.obj
$(OUTDIR)\poly1305_prov.obj
$(OUTDIR)\siphash_prov.obj
$(OUTDIR)\crngt.obj
$(OUTDIR)\drbg.obj
$(OUTDIR)\drbg_ctr.obj
$(OUTDIR)\drbg_hash.obj
$(OUTDIR)\drbg_hmac.obj
$(OUTDIR)\seed_src.obj
$(OUTDIR)\test_rng.obj
$(OUTDIR)\rand_cpu_x86.obj
$(OUTDIR)\rand_tsc.obj
$(OUTDIR)\rand_unix.obj
$(OUTDIR)\rand_win.obj
$(OUTDIR)\dsa_sig.obj
$(OUTDIR)\ecdsa_sig.obj
$(OUTDIR)\eddsa_sig.obj
$(OUTDIR)\mac_legacy_sig.obj
$(OUTDIR)\rsa_sig.obj
$(OUTDIR)\file_store.obj
$(OUTDIR)\file_store_any2obj.obj
$(OUTDIR)\s3_cbc.obj
$(OUTDIR)\der_digests_gen.obj
$(OUTDIR)\der_dsa_gen.obj
$(OUTDIR)\der_dsa_key.obj
$(OUTDIR)\der_dsa_sig.obj
$(OUTDIR)\der_ec_gen.obj
$(OUTDIR)\der_ec_key.obj
$(OUTDIR)\der_ec_sig.obj
$(OUTDIR)\der_ecx_gen.obj
$(OUTDIR)\der_ecx_key.obj
$(OUTDIR)\der_rsa_gen.obj
$(OUTDIR)\der_rsa_key.obj
$(OUTDIR)\der_wrap_gen.obj
$(OUTDIR)\provider_ctx.obj
$(OUTDIR)\provider_err.obj
$(OUTDIR)\ciphercommon.obj
$(OUTDIR)\ciphercommon_block.obj
$(OUTDIR)\ciphercommon_ccm.obj
$(OUTDIR)\ciphercommon_ccm_hw.obj
$(OUTDIR)\ciphercommon_gcm.obj
$(OUTDIR)\ciphercommon_gcm_hw.obj
$(OUTDIR)\ciphercommon_hw.obj
$(OUTDIR)\digestcommon.obj
$(OUTDIR)\tls_pad.obj
<<
$(OUTDIR)\aes-x86_64.obj: crypto\aes\aes-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\aes-x86_64.asm"
$(OUTDIR)\aes_cfb.obj: crypto\aes\aes_cfb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_cfb.c"
$(OUTDIR)\aes_ecb.obj: crypto\aes\aes_ecb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_ecb.c"
$(OUTDIR)\aes_ige.obj: crypto\aes\aes_ige.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_ige.c"
$(OUTDIR)\aes_misc.obj: crypto\aes\aes_misc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_misc.c"
$(OUTDIR)\aes_ofb.obj: crypto\aes\aes_ofb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_ofb.c"
$(OUTDIR)\aes_wrap.obj: crypto\aes\aes_wrap.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\aes\aes_wrap.c"
$(OUTDIR)\aesni-mb-x86_64.obj: crypto\aes\aesni-mb-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\aesni-mb-x86_64.asm"
$(OUTDIR)\aesni-sha1-x86_64.obj: crypto\aes\aesni-sha1-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\aesni-sha1-x86_64.asm"
$(OUTDIR)\aesni-sha256-x86_64.obj: crypto\aes\aesni-sha256-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\aesni-sha256-x86_64.asm"
$(OUTDIR)\aesni-x86_64.obj: crypto\aes\aesni-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\aesni-x86_64.asm"
$(OUTDIR)\bsaes-x86_64.obj: crypto\aes\bsaes-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\bsaes-x86_64.asm"
$(OUTDIR)\vpaes-x86_64.obj: crypto\aes\vpaes-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\aes\vpaes-x86_64.asm"
$(OUTDIR)\a_bitstr.obj: crypto\asn1\a_bitstr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_bitstr.c"
$(OUTDIR)\a_d2i_fp.obj: crypto\asn1\a_d2i_fp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_d2i_fp.c"
$(OUTDIR)\a_digest.obj: crypto\asn1\a_digest.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_digest.c"
$(OUTDIR)\a_dup.obj: crypto\asn1\a_dup.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_dup.c"
$(OUTDIR)\a_gentm.obj: crypto\asn1\a_gentm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_gentm.c"
$(OUTDIR)\a_i2d_fp.obj: crypto\asn1\a_i2d_fp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_i2d_fp.c"
$(OUTDIR)\a_int.obj: crypto\asn1\a_int.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_int.c"
$(OUTDIR)\a_mbstr.obj: crypto\asn1\a_mbstr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_mbstr.c"
$(OUTDIR)\a_object.obj: crypto\asn1\a_object.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_object.c"
$(OUTDIR)\a_octet.obj: crypto\asn1\a_octet.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_octet.c"
$(OUTDIR)\a_print.obj: crypto\asn1\a_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_print.c"
$(OUTDIR)\a_sign.obj: crypto\asn1\a_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_sign.c"
$(OUTDIR)\a_strex.obj: crypto\asn1\a_strex.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_strex.c"
$(OUTDIR)\a_strnid.obj: crypto\asn1\a_strnid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_strnid.c"
$(OUTDIR)\a_time.obj: crypto\asn1\a_time.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_time.c"
$(OUTDIR)\a_type.obj: crypto\asn1\a_type.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_type.c"
$(OUTDIR)\a_utctm.obj: crypto\asn1\a_utctm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_utctm.c"
$(OUTDIR)\a_utf8.obj: crypto\asn1\a_utf8.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_utf8.c"
$(OUTDIR)\a_verify.obj: crypto\asn1\a_verify.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\a_verify.c"
$(OUTDIR)\ameth_lib.obj: crypto\asn1\ameth_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\ameth_lib.c"
$(OUTDIR)\asn1_err.obj: crypto\asn1\asn1_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn1_err.c"
$(OUTDIR)\asn1_gen.obj: crypto\asn1\asn1_gen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn1_gen.c"
$(OUTDIR)\asn1_item_list.obj: crypto\asn1\asn1_item_list.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn1_item_list.c"
$(OUTDIR)\asn1_lib.obj: crypto\asn1\asn1_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn1_lib.c"
$(OUTDIR)\asn1_parse.obj: crypto\asn1\asn1_parse.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn1_parse.c"
$(OUTDIR)\asn_mime.obj: crypto\asn1\asn_mime.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn_mime.c"
$(OUTDIR)\asn_moid.obj: crypto\asn1\asn_moid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn_moid.c"
$(OUTDIR)\asn_mstbl.obj: crypto\asn1\asn_mstbl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn_mstbl.c"
$(OUTDIR)\asn_pack.obj: crypto\asn1\asn_pack.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\asn_pack.c"
$(OUTDIR)\bio_asn1.obj: crypto\asn1\bio_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\bio_asn1.c"
$(OUTDIR)\bio_ndef.obj: crypto\asn1\bio_ndef.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\bio_ndef.c"
$(OUTDIR)\d2i_param.obj: crypto\asn1\d2i_param.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\d2i_param.c"
$(OUTDIR)\d2i_pr.obj: crypto\asn1\d2i_pr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\d2i_pr.c"
$(OUTDIR)\d2i_pu.obj: crypto\asn1\d2i_pu.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\d2i_pu.c"
$(OUTDIR)\evp_asn1.obj: crypto\asn1\evp_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\evp_asn1.c"
$(OUTDIR)\f_int.obj: crypto\asn1\f_int.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\f_int.c"
$(OUTDIR)\f_string.obj: crypto\asn1\f_string.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\f_string.c"
$(OUTDIR)\i2d_evp.obj: crypto\asn1\i2d_evp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\i2d_evp.c"
$(OUTDIR)\n_pkey.obj: crypto\asn1\n_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\n_pkey.c"
$(OUTDIR)\nsseq.obj: crypto\asn1\nsseq.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\nsseq.c"
$(OUTDIR)\p5_pbe.obj: crypto\asn1\p5_pbe.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\p5_pbe.c"
$(OUTDIR)\p5_pbev2.obj: crypto\asn1\p5_pbev2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\p5_pbev2.c"
$(OUTDIR)\p5_scrypt.obj: crypto\asn1\p5_scrypt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\p5_scrypt.c"
$(OUTDIR)\p8_pkey.obj: crypto\asn1\p8_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\p8_pkey.c"
$(OUTDIR)\t_bitst.obj: crypto\asn1\t_bitst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\t_bitst.c"
$(OUTDIR)\t_pkey.obj: crypto\asn1\t_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\t_pkey.c"
$(OUTDIR)\t_spki.obj: crypto\asn1\t_spki.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\t_spki.c"
$(OUTDIR)\tasn_dec.obj: crypto\asn1\tasn_dec.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_dec.c"
$(OUTDIR)\tasn_enc.obj: crypto\asn1\tasn_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_enc.c"
$(OUTDIR)\tasn_fre.obj: crypto\asn1\tasn_fre.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_fre.c"
$(OUTDIR)\tasn_new.obj: crypto\asn1\tasn_new.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_new.c"
$(OUTDIR)\tasn_prn.obj: crypto\asn1\tasn_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_prn.c"
$(OUTDIR)\tasn_scn.obj: crypto\asn1\tasn_scn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_scn.c"
$(OUTDIR)\tasn_typ.obj: crypto\asn1\tasn_typ.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_typ.c"
$(OUTDIR)\tasn_utl.obj: crypto\asn1\tasn_utl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\tasn_utl.c"
$(OUTDIR)\x_algor.obj: crypto\asn1\x_algor.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_algor.c"
$(OUTDIR)\x_bignum.obj: crypto\asn1\x_bignum.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_bignum.c"
$(OUTDIR)\x_info.obj: crypto\asn1\x_info.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_info.c"
$(OUTDIR)\x_int64.obj: crypto\asn1\x_int64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_int64.c"
$(OUTDIR)\x_long.obj: crypto\asn1\x_long.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_long.c"
$(OUTDIR)\x_pkey.obj: crypto\asn1\x_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_pkey.c"
$(OUTDIR)\x_sig.obj: crypto\asn1\x_sig.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_sig.c"
$(OUTDIR)\x_spki.obj: crypto\asn1\x_spki.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_spki.c"
$(OUTDIR)\x_val.obj: crypto\asn1\x_val.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1\x_val.c"
$(OUTDIR)\async_null.obj: crypto\async\arch\async_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\arch\async_null.c"
$(OUTDIR)\async_posix.obj: crypto\async\arch\async_posix.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\arch\async_posix.c"
$(OUTDIR)\async_win.obj: crypto\async\arch\async_win.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\arch\async_win.c"
$(OUTDIR)\async.obj: crypto\async\async.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\async.c"
$(OUTDIR)\async_err.obj: crypto\async\async_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\async_err.c"
$(OUTDIR)\async_wait.obj: crypto\async\async_wait.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\async\async_wait.c"
$(OUTDIR)\bf_cfb64.obj: crypto\bf\bf_cfb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bf\bf_cfb64.c"
$(OUTDIR)\bf_ecb.obj: crypto\bf\bf_ecb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bf\bf_ecb.c"
$(OUTDIR)\bf_enc.obj: crypto\bf\bf_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bf\bf_enc.c"
$(OUTDIR)\bf_ofb64.obj: crypto\bf\bf_ofb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bf\bf_ofb64.c"
$(OUTDIR)\bf_skey.obj: crypto\bf\bf_skey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bf\bf_skey.c"
$(OUTDIR)\bf_buff.obj: crypto\bio\bf_buff.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_buff.c"
$(OUTDIR)\bf_lbuf.obj: crypto\bio\bf_lbuf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_lbuf.c"
$(OUTDIR)\bf_nbio.obj: crypto\bio\bf_nbio.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_nbio.c"
$(OUTDIR)\bf_null.obj: crypto\bio\bf_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_null.c"
$(OUTDIR)\bf_prefix.obj: crypto\bio\bf_prefix.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_prefix.c"
$(OUTDIR)\bf_readbuff.obj: crypto\bio\bf_readbuff.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bf_readbuff.c"
$(OUTDIR)\bio_addr.obj: crypto\bio\bio_addr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_addr.c"
$(OUTDIR)\bio_cb.obj: crypto\bio\bio_cb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_cb.c"
$(OUTDIR)\bio_dump.obj: crypto\bio\bio_dump.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_dump.c"
$(OUTDIR)\bio_err.obj: crypto\bio\bio_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_err.c"
$(OUTDIR)\bio_lib.obj: crypto\bio\bio_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_lib.c"
$(OUTDIR)\bio_meth.obj: crypto\bio\bio_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_meth.c"
$(OUTDIR)\bio_print.obj: crypto\bio\bio_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_print.c"
$(OUTDIR)\bio_sock.obj: crypto\bio\bio_sock.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_sock.c"
$(OUTDIR)\bio_sock2.obj: crypto\bio\bio_sock2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bio_sock2.c"
$(OUTDIR)\bss_acpt.obj: crypto\bio\bss_acpt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_acpt.c"
$(OUTDIR)\bss_bio.obj: crypto\bio\bss_bio.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_bio.c"
$(OUTDIR)\bss_conn.obj: crypto\bio\bss_conn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_conn.c"
$(OUTDIR)\bss_core.obj: crypto\bio\bss_core.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_core.c"
$(OUTDIR)\bss_dgram.obj: crypto\bio\bss_dgram.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_dgram.c"
$(OUTDIR)\bss_fd.obj: crypto\bio\bss_fd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_fd.c"
$(OUTDIR)\bss_file.obj: crypto\bio\bss_file.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_file.c"
$(OUTDIR)\bss_log.obj: crypto\bio\bss_log.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_log.c"
$(OUTDIR)\bss_mem.obj: crypto\bio\bss_mem.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_mem.c"
$(OUTDIR)\bss_null.obj: crypto\bio\bss_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_null.c"
$(OUTDIR)\bss_sock.obj: crypto\bio\bss_sock.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\bss_sock.c"
$(OUTDIR)\ossl_core_bio.obj: crypto\bio\ossl_core_bio.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bio\ossl_core_bio.c"
$(OUTDIR)\bn_add.obj: crypto\bn\bn_add.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_add.c"
$(OUTDIR)\bn_asm.obj: crypto\bn\bn_asm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_asm.c"
$(OUTDIR)\bn_blind.obj: crypto\bn\bn_blind.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_blind.c"
$(OUTDIR)\bn_const.obj: crypto\bn\bn_const.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_const.c"
$(OUTDIR)\bn_conv.obj: crypto\bn\bn_conv.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_conv.c"
$(OUTDIR)\bn_ctx.obj: crypto\bn\bn_ctx.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_ctx.c"
$(OUTDIR)\bn_depr.obj: crypto\bn\bn_depr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_depr.c"
$(OUTDIR)\bn_dh.obj: crypto\bn\bn_dh.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_dh.c"
$(OUTDIR)\bn_div.obj: crypto\bn\bn_div.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_div.c"
$(OUTDIR)\bn_err.obj: crypto\bn\bn_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_err.c"
$(OUTDIR)\bn_exp.obj: crypto\bn\bn_exp.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_exp.c"
$(OUTDIR)\bn_exp2.obj: crypto\bn\bn_exp2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_exp2.c"
$(OUTDIR)\bn_gcd.obj: crypto\bn\bn_gcd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_gcd.c"
$(OUTDIR)\bn_gf2m.obj: crypto\bn\bn_gf2m.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_gf2m.c"
$(OUTDIR)\bn_intern.obj: crypto\bn\bn_intern.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_intern.c"
$(OUTDIR)\bn_kron.obj: crypto\bn\bn_kron.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_kron.c"
$(OUTDIR)\bn_lib.obj: crypto\bn\bn_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_lib.c"
$(OUTDIR)\bn_mod.obj: crypto\bn\bn_mod.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_mod.c"
$(OUTDIR)\bn_mont.obj: crypto\bn\bn_mont.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_mont.c"
$(OUTDIR)\bn_mpi.obj: crypto\bn\bn_mpi.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_mpi.c"
$(OUTDIR)\bn_mul.obj: crypto\bn\bn_mul.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_mul.c"
$(OUTDIR)\bn_nist.obj: crypto\bn\bn_nist.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_nist.c"
$(OUTDIR)\bn_prime.obj: crypto\bn\bn_prime.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_prime.c"
$(OUTDIR)\bn_print.obj: crypto\bn\bn_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_print.c"
$(OUTDIR)\bn_rand.obj: crypto\bn\bn_rand.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_rand.c"
$(OUTDIR)\bn_recp.obj: crypto\bn\bn_recp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_recp.c"
$(OUTDIR)\bn_rsa_fips186_4.obj: crypto\bn\bn_rsa_fips186_4.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_rsa_fips186_4.c"
$(OUTDIR)\bn_shift.obj: crypto\bn\bn_shift.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_shift.c"
$(OUTDIR)\bn_sqr.obj: crypto\bn\bn_sqr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_sqr.c"
$(OUTDIR)\bn_sqrt.obj: crypto\bn\bn_sqrt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_sqrt.c"
$(OUTDIR)\bn_srp.obj: crypto\bn\bn_srp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_srp.c"
$(OUTDIR)\bn_word.obj: crypto\bn\bn_word.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_word.c"
$(OUTDIR)\bn_x931p.obj: crypto\bn\bn_x931p.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\bn_x931p.c"
$(OUTDIR)\rsaz-avx2.obj: crypto\bn\rsaz-avx2.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\rsaz-avx2.asm"
$(OUTDIR)\rsaz-avx512.obj: crypto\bn\rsaz-avx512.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\rsaz-avx512.asm"
$(OUTDIR)\rsaz-x86_64.obj: crypto\bn\rsaz-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\rsaz-x86_64.asm"
$(OUTDIR)\rsaz_exp.obj: crypto\bn\rsaz_exp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\rsaz_exp.c"
$(OUTDIR)\rsaz_exp_x2.obj: crypto\bn\rsaz_exp_x2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bn\rsaz_exp_x2.c"
$(OUTDIR)\x86_64-gf2m.obj: crypto\bn\x86_64-gf2m.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\x86_64-gf2m.asm"
$(OUTDIR)\x86_64-mont.obj: crypto\bn\x86_64-mont.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\x86_64-mont.asm"
$(OUTDIR)\x86_64-mont5.obj: crypto\bn\x86_64-mont5.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\bn\x86_64-mont5.asm"
$(OUTDIR)\buf_err.obj: crypto\buffer\buf_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\buffer\buf_err.c"
$(OUTDIR)\buffer.obj: crypto\buffer\buffer.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\buffer\buffer.c"
$(OUTDIR)\chacha-x86_64.obj: crypto\chacha\chacha-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\chacha\chacha-x86_64.asm"
$(OUTDIR)\cmac.obj: crypto\cmac\cmac.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmac\cmac.c"
$(OUTDIR)\cmp_asn.obj: crypto\cmp\cmp_asn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_asn.c"
$(OUTDIR)\cmp_client.obj: crypto\cmp\cmp_client.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_client.c"
$(OUTDIR)\cmp_ctx.obj: crypto\cmp\cmp_ctx.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_ctx.c"
$(OUTDIR)\cmp_err.obj: crypto\cmp\cmp_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_err.c"
$(OUTDIR)\cmp_hdr.obj: crypto\cmp\cmp_hdr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_hdr.c"
$(OUTDIR)\cmp_http.obj: crypto\cmp\cmp_http.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_http.c"
$(OUTDIR)\cmp_msg.obj: crypto\cmp\cmp_msg.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_msg.c"
$(OUTDIR)\cmp_protect.obj: crypto\cmp\cmp_protect.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_protect.c"
$(OUTDIR)\cmp_server.obj: crypto\cmp\cmp_server.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_server.c"
$(OUTDIR)\cmp_status.obj: crypto\cmp\cmp_status.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_status.c"
$(OUTDIR)\cmp_util.obj: crypto\cmp\cmp_util.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_util.c"
$(OUTDIR)\cmp_vfy.obj: crypto\cmp\cmp_vfy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cmp\cmp_vfy.c"
$(OUTDIR)\cms_asn1.obj: crypto\cms\cms_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_asn1.c"
$(OUTDIR)\cms_att.obj: crypto\cms\cms_att.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_att.c"
$(OUTDIR)\cms_cd.obj: crypto\cms\cms_cd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_cd.c"
$(OUTDIR)\cms_dd.obj: crypto\cms\cms_dd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_dd.c"
$(OUTDIR)\cms_dh.obj: crypto\cms\cms_dh.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_dh.c"
$(OUTDIR)\cms_ec.obj: crypto\cms\cms_ec.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_ec.c"
$(OUTDIR)\cms_enc.obj: crypto\cms\cms_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_enc.c"
$(OUTDIR)\cms_env.obj: crypto\cms\cms_env.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_env.c"
$(OUTDIR)\cms_err.obj: crypto\cms\cms_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_err.c"
$(OUTDIR)\cms_ess.obj: crypto\cms\cms_ess.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_ess.c"
$(OUTDIR)\cms_io.obj: crypto\cms\cms_io.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_io.c"
$(OUTDIR)\cms_kari.obj: crypto\cms\cms_kari.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_kari.c"
$(OUTDIR)\cms_lib.obj: crypto\cms\cms_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_lib.c"
$(OUTDIR)\cms_pwri.obj: crypto\cms\cms_pwri.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_pwri.c"
$(OUTDIR)\cms_rsa.obj: crypto\cms\cms_rsa.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_rsa.c"
$(OUTDIR)\cms_sd.obj: crypto\cms\cms_sd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_sd.c"
$(OUTDIR)\cms_smime.obj: crypto\cms\cms_smime.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cms\cms_smime.c"
$(OUTDIR)\c_zlib.obj: crypto\comp\c_zlib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\comp\c_zlib.c"
$(OUTDIR)\comp_err.obj: crypto\comp\comp_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\comp\comp_err.c"
$(OUTDIR)\comp_lib.obj: crypto\comp\comp_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\comp\comp_lib.c"
$(OUTDIR)\conf_api.obj: crypto\conf\conf_api.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_api.c"
$(OUTDIR)\conf_def.obj: crypto\conf\conf_def.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_def.c"
$(OUTDIR)\conf_err.obj: crypto\conf\conf_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_err.c"
$(OUTDIR)\conf_lib.obj: crypto\conf\conf_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_lib.c"
$(OUTDIR)\conf_mall.obj: crypto\conf\conf_mall.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_mall.c"
$(OUTDIR)\conf_mod.obj: crypto\conf\conf_mod.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_mod.c"
$(OUTDIR)\conf_sap.obj: crypto\conf\conf_sap.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_sap.c"
$(OUTDIR)\conf_ssl.obj: crypto\conf\conf_ssl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\conf\conf_ssl.c"
$(OUTDIR)\crmf_asn.obj: crypto\crmf\crmf_asn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\crmf\crmf_asn.c"
$(OUTDIR)\crmf_err.obj: crypto\crmf\crmf_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\crmf\crmf_err.c"
$(OUTDIR)\crmf_lib.obj: crypto\crmf\crmf_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\crmf\crmf_lib.c"
$(OUTDIR)\crmf_pbm.obj: crypto\crmf\crmf_pbm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\crmf\crmf_pbm.c"
$(OUTDIR)\ct_b64.obj: crypto\ct\ct_b64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_b64.c"
$(OUTDIR)\ct_err.obj: crypto\ct\ct_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_err.c"
$(OUTDIR)\ct_log.obj: crypto\ct\ct_log.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_log.c"
$(OUTDIR)\ct_oct.obj: crypto\ct\ct_oct.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_oct.c"
$(OUTDIR)\ct_policy.obj: crypto\ct\ct_policy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_policy.c"
$(OUTDIR)\ct_prn.obj: crypto\ct\ct_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_prn.c"
$(OUTDIR)\ct_sct.obj: crypto\ct\ct_sct.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_sct.c"
$(OUTDIR)\ct_sct_ctx.obj: crypto\ct\ct_sct_ctx.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_sct_ctx.c"
$(OUTDIR)\ct_vfy.obj: crypto\ct\ct_vfy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_vfy.c"
$(OUTDIR)\ct_x509v3.obj: crypto\ct\ct_x509v3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ct\ct_x509v3.c"
$(OUTDIR)\cbc_cksm.obj: crypto\des\cbc_cksm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\cbc_cksm.c"
$(OUTDIR)\cbc_enc.obj: crypto\des\cbc_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\cbc_enc.c"
$(OUTDIR)\cfb64ede.obj: crypto\des\cfb64ede.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\cfb64ede.c"
$(OUTDIR)\cfb64enc.obj: crypto\des\cfb64enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\cfb64enc.c"
$(OUTDIR)\cfb_enc.obj: crypto\des\cfb_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\cfb_enc.c"
$(OUTDIR)\des_enc.obj: crypto\des\des_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\des_enc.c"
$(OUTDIR)\ecb3_enc.obj: crypto\des\ecb3_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\ecb3_enc.c"
$(OUTDIR)\ecb_enc.obj: crypto\des\ecb_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\ecb_enc.c"
$(OUTDIR)\fcrypt.obj: crypto\des\fcrypt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\fcrypt.c"
$(OUTDIR)\fcrypt_b.obj: crypto\des\fcrypt_b.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\fcrypt_b.c"
$(OUTDIR)\ofb64ede.obj: crypto\des\ofb64ede.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\ofb64ede.c"
$(OUTDIR)\ofb64enc.obj: crypto\des\ofb64enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\ofb64enc.c"
$(OUTDIR)\ofb_enc.obj: crypto\des\ofb_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\ofb_enc.c"
$(OUTDIR)\pcbc_enc.obj: crypto\des\pcbc_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\pcbc_enc.c"
$(OUTDIR)\qud_cksm.obj: crypto\des\qud_cksm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\qud_cksm.c"
$(OUTDIR)\rand_key.obj: crypto\des\rand_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\rand_key.c"
$(OUTDIR)\set_key.obj: crypto\des\set_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\set_key.c"
$(OUTDIR)\str2key.obj: crypto\des\str2key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\str2key.c"
$(OUTDIR)\xcbc_enc.obj: crypto\des\xcbc_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\des\xcbc_enc.c"
$(OUTDIR)\dh_ameth.obj: crypto\dh\dh_ameth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_ameth.c"
$(OUTDIR)\dh_asn1.obj: crypto\dh\dh_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_asn1.c"
$(OUTDIR)\dh_backend.obj: crypto\dh\dh_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_backend.c"
$(OUTDIR)\dh_check.obj: crypto\dh\dh_check.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_check.c"
$(OUTDIR)\dh_depr.obj: crypto\dh\dh_depr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_depr.c"
$(OUTDIR)\dh_err.obj: crypto\dh\dh_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_err.c"
$(OUTDIR)\dh_gen.obj: crypto\dh\dh_gen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_gen.c"
$(OUTDIR)\dh_group_params.obj: crypto\dh\dh_group_params.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_group_params.c"
$(OUTDIR)\dh_kdf.obj: crypto\dh\dh_kdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_kdf.c"
$(OUTDIR)\dh_key.obj: crypto\dh\dh_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_key.c"
$(OUTDIR)\dh_lib.obj: crypto\dh\dh_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_lib.c"
$(OUTDIR)\dh_meth.obj: crypto\dh\dh_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_meth.c"
$(OUTDIR)\dh_pmeth.obj: crypto\dh\dh_pmeth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_pmeth.c"
$(OUTDIR)\dh_prn.obj: crypto\dh\dh_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_prn.c"
$(OUTDIR)\dh_rfc5114.obj: crypto\dh\dh_rfc5114.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dh\dh_rfc5114.c"
$(OUTDIR)\dsa_ameth.obj: crypto\dsa\dsa_ameth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_ameth.c"
$(OUTDIR)\dsa_asn1.obj: crypto\dsa\dsa_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_asn1.c"
$(OUTDIR)\dsa_backend.obj: crypto\dsa\dsa_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_backend.c"
$(OUTDIR)\dsa_check.obj: crypto\dsa\dsa_check.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_check.c"
$(OUTDIR)\dsa_depr.obj: crypto\dsa\dsa_depr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_depr.c"
$(OUTDIR)\dsa_err.obj: crypto\dsa\dsa_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_err.c"
$(OUTDIR)\dsa_gen.obj: crypto\dsa\dsa_gen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_gen.c"
$(OUTDIR)\dsa_key.obj: crypto\dsa\dsa_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_key.c"
$(OUTDIR)\dsa_lib.obj: crypto\dsa\dsa_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_lib.c"
$(OUTDIR)\dsa_meth.obj: crypto\dsa\dsa_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_meth.c"
$(OUTDIR)\dsa_ossl.obj: crypto\dsa\dsa_ossl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_ossl.c"
$(OUTDIR)\dsa_pmeth.obj: crypto\dsa\dsa_pmeth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_pmeth.c"
$(OUTDIR)\dsa_prn.obj: crypto\dsa\dsa_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_prn.c"
$(OUTDIR)\dsa_sign.obj: crypto\dsa\dsa_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_sign.c"
$(OUTDIR)\dsa_vrf.obj: crypto\dsa\dsa_vrf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dsa\dsa_vrf.c"
$(OUTDIR)\dso_dl.obj: crypto\dso\dso_dl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_dl.c"
$(OUTDIR)\dso_dlfcn.obj: crypto\dso\dso_dlfcn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_dlfcn.c"
$(OUTDIR)\dso_err.obj: crypto\dso\dso_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_err.c"
$(OUTDIR)\dso_lib.obj: crypto\dso\dso_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_lib.c"
$(OUTDIR)\dso_openssl.obj: crypto\dso\dso_openssl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_openssl.c"
$(OUTDIR)\dso_vms.obj: crypto\dso\dso_vms.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_vms.c"
$(OUTDIR)\dso_win32.obj: crypto\dso\dso_win32.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\dso\dso_win32.c"
$(OUTDIR)\f_impl32.obj: crypto\ec\curve448\arch_32\f_impl32.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\arch_32\f_impl32.c"
$(OUTDIR)\f_impl64.obj: crypto\ec\curve448\arch_64\f_impl64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\arch_64\f_impl64.c"
$(OUTDIR)\curve448.obj: crypto\ec\curve448\curve448.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\curve448.c"
$(OUTDIR)\curve448_tables.obj: crypto\ec\curve448\curve448_tables.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\curve448_tables.c"
$(OUTDIR)\eddsa.obj: crypto\ec\curve448\eddsa.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\eddsa.c"
$(OUTDIR)\f_generic.obj: crypto\ec\curve448\f_generic.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\f_generic.c"
$(OUTDIR)\scalar.obj: crypto\ec\curve448\scalar.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve448\scalar.c"
$(OUTDIR)\curve25519.obj: crypto\ec\curve25519.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\curve25519.c"
$(OUTDIR)\ec2_oct.obj: crypto\ec\ec2_oct.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec2_oct.c"
$(OUTDIR)\ec2_smpl.obj: crypto\ec\ec2_smpl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec2_smpl.c"
$(OUTDIR)\ec_ameth.obj: crypto\ec\ec_ameth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_ameth.c"
$(OUTDIR)\ec_asn1.obj: crypto\ec\ec_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_asn1.c"
$(OUTDIR)\ec_backend.obj: crypto\ec\ec_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_backend.c"
$(OUTDIR)\ec_check.obj: crypto\ec\ec_check.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_check.c"
$(OUTDIR)\ec_curve.obj: crypto\ec\ec_curve.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_curve.c"
$(OUTDIR)\ec_cvt.obj: crypto\ec\ec_cvt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_cvt.c"
$(OUTDIR)\ec_deprecated.obj: crypto\ec\ec_deprecated.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_deprecated.c"
$(OUTDIR)\ec_err.obj: crypto\ec\ec_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_err.c"
$(OUTDIR)\ec_key.obj: crypto\ec\ec_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_key.c"
$(OUTDIR)\ec_kmeth.obj: crypto\ec\ec_kmeth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_kmeth.c"
$(OUTDIR)\ec_lib.obj: crypto\ec\ec_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_lib.c"
$(OUTDIR)\ec_mult.obj: crypto\ec\ec_mult.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_mult.c"
$(OUTDIR)\ec_oct.obj: crypto\ec\ec_oct.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_oct.c"
$(OUTDIR)\ec_pmeth.obj: crypto\ec\ec_pmeth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_pmeth.c"
$(OUTDIR)\ec_print.obj: crypto\ec\ec_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ec_print.c"
$(OUTDIR)\ecdh_kdf.obj: crypto\ec\ecdh_kdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecdh_kdf.c"
$(OUTDIR)\ecdh_ossl.obj: crypto\ec\ecdh_ossl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecdh_ossl.c"
$(OUTDIR)\ecdsa_ossl.obj: crypto\ec\ecdsa_ossl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecdsa_ossl.c"
$(OUTDIR)\ecdsa_sign.obj: crypto\ec\ecdsa_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecdsa_sign.c"
$(OUTDIR)\ecdsa_vrf.obj: crypto\ec\ecdsa_vrf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecdsa_vrf.c"
$(OUTDIR)\eck_prn.obj: crypto\ec\eck_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\eck_prn.c"
$(OUTDIR)\ecp_mont.obj: crypto\ec\ecp_mont.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecp_mont.c"
$(OUTDIR)\ecp_nist.obj: crypto\ec\ecp_nist.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecp_nist.c"
$(OUTDIR)\ecp_nistz256-x86_64.obj: crypto\ec\ecp_nistz256-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\ec\ecp_nistz256-x86_64.asm"
$(OUTDIR)\ecp_nistz256.obj: crypto\ec\ecp_nistz256.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecp_nistz256.c"
$(OUTDIR)\ecp_oct.obj: crypto\ec\ecp_oct.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecp_oct.c"
$(OUTDIR)\ecp_smpl.obj: crypto\ec\ecp_smpl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecp_smpl.c"
$(OUTDIR)\ecx_backend.obj: crypto\ec\ecx_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecx_backend.c"
$(OUTDIR)\ecx_key.obj: crypto\ec\ecx_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecx_key.c"
$(OUTDIR)\ecx_meth.obj: crypto\ec\ecx_meth.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ec\ecx_meth.c"
$(OUTDIR)\x25519-x86_64.obj: crypto\ec\x25519-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\ec\x25519-x86_64.asm"
$(OUTDIR)\decoder_err.obj: crypto\encode_decode\decoder_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\decoder_err.c"
$(OUTDIR)\decoder_lib.obj: crypto\encode_decode\decoder_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\decoder_lib.c"
$(OUTDIR)\decoder_meth.obj: crypto\encode_decode\decoder_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\decoder_meth.c"
$(OUTDIR)\decoder_pkey.obj: crypto\encode_decode\decoder_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\decoder_pkey.c"
$(OUTDIR)\encoder_err.obj: crypto\encode_decode\encoder_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\encoder_err.c"
$(OUTDIR)\encoder_lib.obj: crypto\encode_decode\encoder_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\encoder_lib.c"
$(OUTDIR)\encoder_meth.obj: crypto\encode_decode\encoder_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\encoder_meth.c"
$(OUTDIR)\encoder_pkey.obj: crypto\encode_decode\encoder_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\encode_decode\encoder_pkey.c"
$(OUTDIR)\err.obj: crypto\err\err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\err\err.c"
$(OUTDIR)\err_all.obj: crypto\err\err_all.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\err\err_all.c"
$(OUTDIR)\err_all_legacy.obj: crypto\err\err_all_legacy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\err\err_all_legacy.c"
$(OUTDIR)\err_blocks.obj: crypto\err\err_blocks.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\err\err_blocks.c"
$(OUTDIR)\err_prn.obj: crypto\err\err_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\err\err_prn.c"
$(OUTDIR)\ess_asn1.obj: crypto\ess\ess_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ess\ess_asn1.c"
$(OUTDIR)\ess_err.obj: crypto\ess\ess_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ess\ess_err.c"
$(OUTDIR)\ess_lib.obj: crypto\ess\ess_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ess\ess_lib.c"
$(OUTDIR)\asymcipher.obj: crypto\evp\asymcipher.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\asymcipher.c"
$(OUTDIR)\bio_b64.obj: crypto\evp\bio_b64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\bio_b64.c"
$(OUTDIR)\bio_enc.obj: crypto\evp\bio_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\bio_enc.c"
$(OUTDIR)\bio_md.obj: crypto\evp\bio_md.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\bio_md.c"
$(OUTDIR)\bio_ok.obj: crypto\evp\bio_ok.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\bio_ok.c"
$(OUTDIR)\c_allc.obj: crypto\evp\c_allc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\c_allc.c"
$(OUTDIR)\c_alld.obj: crypto\evp\c_alld.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\c_alld.c"
$(OUTDIR)\cmeth_lib.obj: crypto\evp\cmeth_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\cmeth_lib.c"
$(OUTDIR)\ctrl_params_translate.obj: crypto\evp\ctrl_params_translate.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\ctrl_params_translate.c"
$(OUTDIR)\dh_ctrl.obj: crypto\evp\dh_ctrl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\dh_ctrl.c"
$(OUTDIR)\dh_support.obj: crypto\evp\dh_support.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\dh_support.c"
$(OUTDIR)\digest.obj: crypto\evp\digest.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\digest.c"
$(OUTDIR)\dsa_ctrl.obj: crypto\evp\dsa_ctrl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\dsa_ctrl.c"
$(OUTDIR)\e_aes.obj: crypto\evp\e_aes.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"crypto\modes" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_aes.c"
$(OUTDIR)\e_aes_cbc_hmac_sha1.obj: crypto\evp\e_aes_cbc_hmac_sha1.c
	$(CC)  $(LIB_CFLAGS) -I"crypto\modes" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_aes_cbc_hmac_sha1.c"
$(OUTDIR)\e_aes_cbc_hmac_sha256.obj: crypto\evp\e_aes_cbc_hmac_sha256.c
	$(CC)  $(LIB_CFLAGS) -I"crypto\modes" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_aes_cbc_hmac_sha256.c"
$(OUTDIR)\e_aria.obj: crypto\evp\e_aria.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"crypto\modes" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_aria.c"
$(OUTDIR)\e_bf.obj: crypto\evp\e_bf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_bf.c"
$(OUTDIR)\e_cast.obj: crypto\evp\e_cast.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_cast.c"
$(OUTDIR)\e_chacha20_poly1305.obj: crypto\evp\e_chacha20_poly1305.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_chacha20_poly1305.c"
$(OUTDIR)\e_des.obj: crypto\evp\e_des.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_des.c"
$(OUTDIR)\e_des3.obj: crypto\evp\e_des3.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_des3.c"
$(OUTDIR)\e_idea.obj: crypto\evp\e_idea.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_idea.c"
$(OUTDIR)\e_null.obj: crypto\evp\e_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_null.c"
$(OUTDIR)\e_old.obj: crypto\evp\e_old.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_old.c"
$(OUTDIR)\e_rc2.obj: crypto\evp\e_rc2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_rc2.c"
$(OUTDIR)\e_rc4.obj: crypto\evp\e_rc4.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_rc4.c"
$(OUTDIR)\e_rc4_hmac_md5.obj: crypto\evp\e_rc4_hmac_md5.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_rc4_hmac_md5.c"
$(OUTDIR)\e_rc5.obj: crypto\evp\e_rc5.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_rc5.c"
$(OUTDIR)\e_seed.obj: crypto\evp\e_seed.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_seed.c"
$(OUTDIR)\e_sm4.obj: crypto\evp\e_sm4.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"crypto\modes" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_sm4.c"
$(OUTDIR)\e_xcbc_d.obj: crypto\evp\e_xcbc_d.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\e_xcbc_d.c"
$(OUTDIR)\ec_ctrl.obj: crypto\evp\ec_ctrl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\ec_ctrl.c"
$(OUTDIR)\ec_support.obj: crypto\evp\ec_support.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\ec_support.c"
$(OUTDIR)\encode.obj: crypto\evp\encode.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\encode.c"
$(OUTDIR)\evp_cnf.obj: crypto\evp\evp_cnf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_cnf.c"
$(OUTDIR)\evp_enc.obj: crypto\evp\evp_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_enc.c"
$(OUTDIR)\evp_err.obj: crypto\evp\evp_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_err.c"
$(OUTDIR)\evp_fetch.obj: crypto\evp\evp_fetch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_fetch.c"
$(OUTDIR)\evp_key.obj: crypto\evp\evp_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_key.c"
$(OUTDIR)\evp_lib.obj: crypto\evp\evp_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_lib.c"
$(OUTDIR)\evp_pbe.obj: crypto\evp\evp_pbe.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_pbe.c"
$(OUTDIR)\evp_pkey.obj: crypto\evp\evp_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_pkey.c"
$(OUTDIR)\evp_rand.obj: crypto\evp\evp_rand.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_rand.c"
$(OUTDIR)\evp_utils.obj: crypto\evp\evp_utils.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\evp_utils.c"
$(OUTDIR)\exchange.obj: crypto\evp\exchange.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\exchange.c"
$(OUTDIR)\kdf_lib.obj: crypto\evp\kdf_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\kdf_lib.c"
$(OUTDIR)\kdf_meth.obj: crypto\evp\kdf_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\kdf_meth.c"
$(OUTDIR)\kem.obj: crypto\evp\kem.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\kem.c"
$(OUTDIR)\keymgmt_lib.obj: crypto\evp\keymgmt_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\keymgmt_lib.c"
$(OUTDIR)\keymgmt_meth.obj: crypto\evp\keymgmt_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\keymgmt_meth.c"
$(OUTDIR)\legacy_blake2.obj: crypto\evp\legacy_blake2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_blake2.c"
$(OUTDIR)\legacy_md4.obj: crypto\evp\legacy_md4.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_md4.c"
$(OUTDIR)\legacy_md5.obj: crypto\evp\legacy_md5.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_md5.c"
$(OUTDIR)\legacy_md5_sha1.obj: crypto\evp\legacy_md5_sha1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_md5_sha1.c"
$(OUTDIR)\legacy_ripemd.obj: crypto\evp\legacy_ripemd.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_ripemd.c"
$(OUTDIR)\legacy_sha.obj: crypto\evp\legacy_sha.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_sha.c"
$(OUTDIR)\legacy_wp.obj: crypto\evp\legacy_wp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\legacy_wp.c"
$(OUTDIR)\m_null.obj: crypto\evp\m_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\m_null.c"
$(OUTDIR)\m_sigver.obj: crypto\evp\m_sigver.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\m_sigver.c"
$(OUTDIR)\mac_lib.obj: crypto\evp\mac_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\mac_lib.c"
$(OUTDIR)\mac_meth.obj: crypto\evp\mac_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\mac_meth.c"
$(OUTDIR)\names.obj: crypto\evp\names.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\names.c"
$(OUTDIR)\p5_crpt.obj: crypto\evp\p5_crpt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p5_crpt.c"
$(OUTDIR)\p5_crpt2.obj: crypto\evp\p5_crpt2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p5_crpt2.c"
$(OUTDIR)\p_dec.obj: crypto\evp\p_dec.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_dec.c"
$(OUTDIR)\p_enc.obj: crypto\evp\p_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_enc.c"
$(OUTDIR)\p_legacy.obj: crypto\evp\p_legacy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_legacy.c"
$(OUTDIR)\p_lib.obj: crypto\evp\p_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_lib.c"
$(OUTDIR)\p_open.obj: crypto\evp\p_open.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_open.c"
$(OUTDIR)\p_seal.obj: crypto\evp\p_seal.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_seal.c"
$(OUTDIR)\p_sign.obj: crypto\evp\p_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_sign.c"
$(OUTDIR)\p_verify.obj: crypto\evp\p_verify.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\p_verify.c"
$(OUTDIR)\pbe_scrypt.obj: crypto\evp\pbe_scrypt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\pbe_scrypt.c"
$(OUTDIR)\pmeth_check.obj: crypto\evp\pmeth_check.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\pmeth_check.c"
$(OUTDIR)\pmeth_gn.obj: crypto\evp\pmeth_gn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\pmeth_gn.c"
$(OUTDIR)\pmeth_lib.obj: crypto\evp\pmeth_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\pmeth_lib.c"
$(OUTDIR)\signature.obj: crypto\evp\signature.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\evp\signature.c"
$(OUTDIR)\ffc_backend.obj: crypto\ffc\ffc_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_backend.c"
$(OUTDIR)\ffc_dh.obj: crypto\ffc\ffc_dh.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_dh.c"
$(OUTDIR)\ffc_key_generate.obj: crypto\ffc\ffc_key_generate.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_key_generate.c"
$(OUTDIR)\ffc_key_validate.obj: crypto\ffc\ffc_key_validate.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_key_validate.c"
$(OUTDIR)\ffc_params.obj: crypto\ffc\ffc_params.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_params.c"
$(OUTDIR)\ffc_params_generate.obj: crypto\ffc\ffc_params_generate.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_params_generate.c"
$(OUTDIR)\ffc_params_validate.obj: crypto\ffc\ffc_params_validate.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ffc\ffc_params_validate.c"
$(OUTDIR)\hmac.obj: crypto\hmac\hmac.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\hmac\hmac.c"
$(OUTDIR)\http_client.obj: crypto\http\http_client.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\http\http_client.c"
$(OUTDIR)\http_err.obj: crypto\http\http_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\http\http_err.c"
$(OUTDIR)\http_lib.obj: crypto\http\http_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\http\http_lib.c"
$(OUTDIR)\i_cbc.obj: crypto\idea\i_cbc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\idea\i_cbc.c"
$(OUTDIR)\i_cfb64.obj: crypto\idea\i_cfb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\idea\i_cfb64.c"
$(OUTDIR)\i_ecb.obj: crypto\idea\i_ecb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\idea\i_ecb.c"
$(OUTDIR)\i_ofb64.obj: crypto\idea\i_ofb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\idea\i_ofb64.c"
$(OUTDIR)\i_skey.obj: crypto\idea\i_skey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\idea\i_skey.c"
$(OUTDIR)\kdf_err.obj: crypto\kdf\kdf_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\kdf\kdf_err.c"
$(OUTDIR)\lh_stats.obj: crypto\lhash\lh_stats.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\lhash\lh_stats.c"
$(OUTDIR)\lhash.obj: crypto\lhash\lhash.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\lhash\lhash.c"
$(OUTDIR)\asn1_dsa.obj: crypto\asn1_dsa.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\asn1_dsa.c"
$(OUTDIR)\bsearch.obj: crypto\bsearch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\bsearch.c"
$(OUTDIR)\context.obj: crypto\context.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\context.c"
$(OUTDIR)\core_algorithm.obj: crypto\core_algorithm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\core_algorithm.c"
$(OUTDIR)\core_fetch.obj: crypto\core_fetch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\core_fetch.c"
$(OUTDIR)\core_namemap.obj: crypto\core_namemap.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\core_namemap.c"
$(OUTDIR)\cpt_err.obj: crypto\cpt_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cpt_err.c"
$(OUTDIR)\cpuid.obj: crypto\cpuid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cpuid.c"
$(OUTDIR)\cryptlib.obj: crypto\cryptlib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cryptlib.c"
$(OUTDIR)\ctype.obj: crypto\ctype.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ctype.c"
$(OUTDIR)\cversion.obj: crypto\cversion.c crypto\buildinf.h
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\cversion.c"
$(OUTDIR)\der_writer.obj: crypto\der_writer.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\der_writer.c"
$(OUTDIR)\ebcdic.obj: crypto\ebcdic.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ebcdic.c"
$(OUTDIR)\ex_data.obj: crypto\ex_data.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ex_data.c"
$(OUTDIR)\getenv.obj: crypto\getenv.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\getenv.c"
$(OUTDIR)\info.obj: crypto\info.c crypto\buildinf.h
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\info.c"
$(OUTDIR)\init.obj: crypto\init.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\init.c"
$(OUTDIR)\initthread.obj: crypto\initthread.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\initthread.c"
$(OUTDIR)\mem.obj: crypto\mem.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\mem.c"
$(OUTDIR)\mem_sec.obj: crypto\mem_sec.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\mem_sec.c"
$(OUTDIR)\o_dir.obj: crypto\o_dir.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\o_dir.c"
$(OUTDIR)\o_fopen.obj: crypto\o_fopen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\o_fopen.c"
$(OUTDIR)\o_init.obj: crypto\o_init.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\o_init.c"
$(OUTDIR)\o_str.obj: crypto\o_str.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\o_str.c"
$(OUTDIR)\o_time.obj: crypto\o_time.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\o_time.c"
$(OUTDIR)\packet.obj: crypto\packet.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\packet.c"
$(OUTDIR)\param_build.obj: crypto\param_build.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\param_build.c"
$(OUTDIR)\param_build_set.obj: crypto\param_build_set.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\param_build_set.c"
$(OUTDIR)\params.obj: crypto\params.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\params.c"
$(OUTDIR)\params_dup.obj: crypto\params_dup.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\params_dup.c"
$(OUTDIR)\params_from_text.obj: crypto\params_from_text.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\params_from_text.c"
$(OUTDIR)\passphrase.obj: crypto\passphrase.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\passphrase.c"
$(OUTDIR)\provider.obj: crypto\provider.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\provider.c"
$(OUTDIR)\provider_child.obj: crypto\provider_child.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\provider_child.c"
$(OUTDIR)\provider_conf.obj: crypto\provider_conf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\provider_conf.c"
$(OUTDIR)\provider_core.obj: crypto\provider_core.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\provider_core.c"
$(OUTDIR)\provider_predefined.obj: crypto\provider_predefined.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\provider_predefined.c"
$(OUTDIR)\punycode.obj: crypto\punycode.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\punycode.c"
$(OUTDIR)\self_test_core.obj: crypto\self_test_core.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\self_test_core.c"
$(OUTDIR)\sparse_array.obj: crypto\sparse_array.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sparse_array.c"
$(OUTDIR)\threads_lib.obj: crypto\threads_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\threads_lib.c"
$(OUTDIR)\threads_none.obj: crypto\threads_none.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\threads_none.c"
$(OUTDIR)\threads_pthread.obj: crypto\threads_pthread.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\threads_pthread.c"
$(OUTDIR)\threads_win.obj: crypto\threads_win.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\threads_win.c"
$(OUTDIR)\trace.obj: crypto\trace.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\trace.c"
$(OUTDIR)\uid.obj: crypto\uid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\uid.c"
$(OUTDIR)\x86_64cpuid.obj: crypto\x86_64cpuid.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\x86_64cpuid.asm"
$(OUTDIR)\md4_dgst.obj: crypto\md4\md4_dgst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\md4\md4_dgst.c"
$(OUTDIR)\md4_one.obj: crypto\md4\md4_one.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\md4\md4_one.c"
$(OUTDIR)\md5-x86_64.obj: crypto\md5\md5-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\md5\md5-x86_64.asm"
$(OUTDIR)\md5_dgst.obj: crypto\md5\md5_dgst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\md5\md5_dgst.c"
$(OUTDIR)\md5_one.obj: crypto\md5\md5_one.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\md5\md5_one.c"
$(OUTDIR)\md5_sha1.obj: crypto\md5\md5_sha1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\md5\md5_sha1.c"
$(OUTDIR)\aesni-gcm-x86_64.obj: crypto\modes\aesni-gcm-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\modes\aesni-gcm-x86_64.asm"
$(OUTDIR)\cbc128.obj: crypto\modes\cbc128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\cbc128.c"
$(OUTDIR)\ccm128.obj: crypto\modes\ccm128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\ccm128.c"
$(OUTDIR)\cfb128.obj: crypto\modes\cfb128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\cfb128.c"
$(OUTDIR)\ctr128.obj: crypto\modes\ctr128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\ctr128.c"
$(OUTDIR)\cts128.obj: crypto\modes\cts128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\cts128.c"
$(OUTDIR)\gcm128.obj: crypto\modes\gcm128.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\gcm128.c"
$(OUTDIR)\ghash-x86_64.obj: crypto\modes\ghash-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\modes\ghash-x86_64.asm"
$(OUTDIR)\ocb128.obj: crypto\modes\ocb128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\ocb128.c"
$(OUTDIR)\ofb128.obj: crypto\modes\ofb128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\ofb128.c"
$(OUTDIR)\siv128.obj: crypto\modes\siv128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\siv128.c"
$(OUTDIR)\wrap128.obj: crypto\modes\wrap128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\wrap128.c"
$(OUTDIR)\xts128.obj: crypto\modes\xts128.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\modes\xts128.c"
$(OUTDIR)\o_names.obj: crypto\objects\o_names.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\objects\o_names.c"
$(OUTDIR)\obj_dat.obj: crypto\objects\obj_dat.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\objects\obj_dat.c"
$(OUTDIR)\obj_err.obj: crypto\objects\obj_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\objects\obj_err.c"
$(OUTDIR)\obj_lib.obj: crypto\objects\obj_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\objects\obj_lib.c"
$(OUTDIR)\obj_xref.obj: crypto\objects\obj_xref.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\objects\obj_xref.c"
$(OUTDIR)\ocsp_asn.obj: crypto\ocsp\ocsp_asn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_asn.c"
$(OUTDIR)\ocsp_cl.obj: crypto\ocsp\ocsp_cl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_cl.c"
$(OUTDIR)\ocsp_err.obj: crypto\ocsp\ocsp_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_err.c"
$(OUTDIR)\ocsp_ext.obj: crypto\ocsp\ocsp_ext.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_ext.c"
$(OUTDIR)\ocsp_http.obj: crypto\ocsp\ocsp_http.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_http.c"
$(OUTDIR)\ocsp_lib.obj: crypto\ocsp\ocsp_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_lib.c"
$(OUTDIR)\ocsp_prn.obj: crypto\ocsp\ocsp_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_prn.c"
$(OUTDIR)\ocsp_srv.obj: crypto\ocsp\ocsp_srv.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_srv.c"
$(OUTDIR)\ocsp_vfy.obj: crypto\ocsp\ocsp_vfy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\ocsp_vfy.c"
$(OUTDIR)\v3_ocsp.obj: crypto\ocsp\v3_ocsp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ocsp\v3_ocsp.c"
$(OUTDIR)\pem_all.obj: crypto\pem\pem_all.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_all.c"
$(OUTDIR)\pem_err.obj: crypto\pem\pem_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_err.c"
$(OUTDIR)\pem_info.obj: crypto\pem\pem_info.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_info.c"
$(OUTDIR)\pem_lib.obj: crypto\pem\pem_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_lib.c"
$(OUTDIR)\pem_oth.obj: crypto\pem\pem_oth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_oth.c"
$(OUTDIR)\pem_pk8.obj: crypto\pem\pem_pk8.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_pk8.c"
$(OUTDIR)\pem_pkey.obj: crypto\pem\pem_pkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_pkey.c"
$(OUTDIR)\pem_sign.obj: crypto\pem\pem_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_sign.c"
$(OUTDIR)\pem_x509.obj: crypto\pem\pem_x509.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_x509.c"
$(OUTDIR)\pem_xaux.obj: crypto\pem\pem_xaux.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pem_xaux.c"
$(OUTDIR)\pvkfmt.obj: crypto\pem\pvkfmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pem\pvkfmt.c"
$(OUTDIR)\p12_add.obj: crypto\pkcs12\p12_add.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_add.c"
$(OUTDIR)\p12_asn.obj: crypto\pkcs12\p12_asn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_asn.c"
$(OUTDIR)\p12_attr.obj: crypto\pkcs12\p12_attr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_attr.c"
$(OUTDIR)\p12_crpt.obj: crypto\pkcs12\p12_crpt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_crpt.c"
$(OUTDIR)\p12_crt.obj: crypto\pkcs12\p12_crt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_crt.c"
$(OUTDIR)\p12_decr.obj: crypto\pkcs12\p12_decr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_decr.c"
$(OUTDIR)\p12_init.obj: crypto\pkcs12\p12_init.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_init.c"
$(OUTDIR)\p12_key.obj: crypto\pkcs12\p12_key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_key.c"
$(OUTDIR)\p12_kiss.obj: crypto\pkcs12\p12_kiss.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_kiss.c"
$(OUTDIR)\p12_mutl.obj: crypto\pkcs12\p12_mutl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_mutl.c"
$(OUTDIR)\p12_npas.obj: crypto\pkcs12\p12_npas.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_npas.c"
$(OUTDIR)\p12_p8d.obj: crypto\pkcs12\p12_p8d.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_p8d.c"
$(OUTDIR)\p12_p8e.obj: crypto\pkcs12\p12_p8e.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_p8e.c"
$(OUTDIR)\p12_sbag.obj: crypto\pkcs12\p12_sbag.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_sbag.c"
$(OUTDIR)\p12_utl.obj: crypto\pkcs12\p12_utl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\p12_utl.c"
$(OUTDIR)\pk12err.obj: crypto\pkcs12\pk12err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs12\pk12err.c"
$(OUTDIR)\bio_pk7.obj: crypto\pkcs7\bio_pk7.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\bio_pk7.c"
$(OUTDIR)\pk7_asn1.obj: crypto\pkcs7\pk7_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_asn1.c"
$(OUTDIR)\pk7_attr.obj: crypto\pkcs7\pk7_attr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_attr.c"
$(OUTDIR)\pk7_doit.obj: crypto\pkcs7\pk7_doit.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_doit.c"
$(OUTDIR)\pk7_lib.obj: crypto\pkcs7\pk7_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_lib.c"
$(OUTDIR)\pk7_mime.obj: crypto\pkcs7\pk7_mime.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_mime.c"
$(OUTDIR)\pk7_smime.obj: crypto\pkcs7\pk7_smime.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pk7_smime.c"
$(OUTDIR)\pkcs7err.obj: crypto\pkcs7\pkcs7err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\pkcs7\pkcs7err.c"
$(OUTDIR)\poly1305-x86_64.obj: crypto\poly1305\poly1305-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\poly1305\poly1305-x86_64.asm"
$(OUTDIR)\poly1305.obj: crypto\poly1305\poly1305.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\poly1305\poly1305.c"
$(OUTDIR)\defn_cache.obj: crypto\property\defn_cache.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\defn_cache.c"
$(OUTDIR)\property.obj: crypto\property\property.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\property.c"
$(OUTDIR)\property_err.obj: crypto\property\property_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\property_err.c"
$(OUTDIR)\property_parse.obj: crypto\property\property_parse.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\property_parse.c"
$(OUTDIR)\property_query.obj: crypto\property\property_query.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\property_query.c"
$(OUTDIR)\property_string.obj: crypto\property\property_string.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\property\property_string.c"
$(OUTDIR)\prov_seed.obj: crypto\rand\prov_seed.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\prov_seed.c"
$(OUTDIR)\rand_deprecated.obj: crypto\rand\rand_deprecated.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\rand_deprecated.c"
$(OUTDIR)\rand_err.obj: crypto\rand\rand_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\rand_err.c"
$(OUTDIR)\rand_lib.obj: crypto\rand\rand_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\rand_lib.c"
$(OUTDIR)\rand_meth.obj: crypto\rand\rand_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\rand_meth.c"
$(OUTDIR)\rand_pool.obj: crypto\rand\rand_pool.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\rand_pool.c"
$(OUTDIR)\randfile.obj: crypto\rand\randfile.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rand\randfile.c"
$(OUTDIR)\rc2_cbc.obj: crypto\rc2\rc2_cbc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rc2\rc2_cbc.c"
$(OUTDIR)\rc2_ecb.obj: crypto\rc2\rc2_ecb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rc2\rc2_ecb.c"
$(OUTDIR)\rc2_skey.obj: crypto\rc2\rc2_skey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rc2\rc2_skey.c"
$(OUTDIR)\rc2cfb64.obj: crypto\rc2\rc2cfb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rc2\rc2cfb64.c"
$(OUTDIR)\rc2ofb64.obj: crypto\rc2\rc2ofb64.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rc2\rc2ofb64.c"
$(OUTDIR)\rc4-md5-x86_64.obj: crypto\rc4\rc4-md5-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\rc4\rc4-md5-x86_64.asm"
$(OUTDIR)\rc4-x86_64.obj: crypto\rc4\rc4-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\rc4\rc4-x86_64.asm"
$(OUTDIR)\rmd_dgst.obj: crypto\ripemd\rmd_dgst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ripemd\rmd_dgst.c"
$(OUTDIR)\rmd_one.obj: crypto\ripemd\rmd_one.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ripemd\rmd_one.c"
$(OUTDIR)\rsa_ameth.obj: crypto\rsa\rsa_ameth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_ameth.c"
$(OUTDIR)\rsa_asn1.obj: crypto\rsa\rsa_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_asn1.c"
$(OUTDIR)\rsa_backend.obj: crypto\rsa\rsa_backend.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_backend.c"
$(OUTDIR)\rsa_chk.obj: crypto\rsa\rsa_chk.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_chk.c"
$(OUTDIR)\rsa_crpt.obj: crypto\rsa\rsa_crpt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_crpt.c"
$(OUTDIR)\rsa_depr.obj: crypto\rsa\rsa_depr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_depr.c"
$(OUTDIR)\rsa_err.obj: crypto\rsa\rsa_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_err.c"
$(OUTDIR)\rsa_gen.obj: crypto\rsa\rsa_gen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_gen.c"
$(OUTDIR)\rsa_lib.obj: crypto\rsa\rsa_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_lib.c"
$(OUTDIR)\rsa_meth.obj: crypto\rsa\rsa_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_meth.c"
$(OUTDIR)\rsa_mp.obj: crypto\rsa\rsa_mp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_mp.c"
$(OUTDIR)\rsa_mp_names.obj: crypto\rsa\rsa_mp_names.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_mp_names.c"
$(OUTDIR)\rsa_none.obj: crypto\rsa\rsa_none.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_none.c"
$(OUTDIR)\rsa_oaep.obj: crypto\rsa\rsa_oaep.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_oaep.c"
$(OUTDIR)\rsa_ossl.obj: crypto\rsa\rsa_ossl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_ossl.c"
$(OUTDIR)\rsa_pk1.obj: crypto\rsa\rsa_pk1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_pk1.c"
$(OUTDIR)\rsa_pmeth.obj: crypto\rsa\rsa_pmeth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_pmeth.c"
$(OUTDIR)\rsa_prn.obj: crypto\rsa\rsa_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_prn.c"
$(OUTDIR)\rsa_pss.obj: crypto\rsa\rsa_pss.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_pss.c"
$(OUTDIR)\rsa_saos.obj: crypto\rsa\rsa_saos.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_saos.c"
$(OUTDIR)\rsa_schemes.obj: crypto\rsa\rsa_schemes.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_schemes.c"
$(OUTDIR)\rsa_sign.obj: crypto\rsa\rsa_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_sign.c"
$(OUTDIR)\rsa_sp800_56b_check.obj: crypto\rsa\rsa_sp800_56b_check.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_sp800_56b_check.c"
$(OUTDIR)\rsa_sp800_56b_gen.obj: crypto\rsa\rsa_sp800_56b_gen.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_sp800_56b_gen.c"
$(OUTDIR)\rsa_x931.obj: crypto\rsa\rsa_x931.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_x931.c"
$(OUTDIR)\rsa_x931g.obj: crypto\rsa\rsa_x931g.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\rsa\rsa_x931g.c"
$(OUTDIR)\seed.obj: crypto\seed\seed.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\seed\seed.c"
$(OUTDIR)\seed_cbc.obj: crypto\seed\seed_cbc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\seed\seed_cbc.c"
$(OUTDIR)\seed_cfb.obj: crypto\seed\seed_cfb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\seed\seed_cfb.c"
$(OUTDIR)\seed_ecb.obj: crypto\seed\seed_ecb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\seed\seed_ecb.c"
$(OUTDIR)\seed_ofb.obj: crypto\seed\seed_ofb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\seed\seed_ofb.c"
$(OUTDIR)\keccak1600-x86_64.obj: crypto\sha\keccak1600-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\keccak1600-x86_64.asm"
$(OUTDIR)\sha1-mb-x86_64.obj: crypto\sha\sha1-mb-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\sha1-mb-x86_64.asm"
$(OUTDIR)\sha1-x86_64.obj: crypto\sha\sha1-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\sha1-x86_64.asm"
$(OUTDIR)\sha1_one.obj: crypto\sha\sha1_one.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sha\sha1_one.c"
$(OUTDIR)\sha1dgst.obj: crypto\sha\sha1dgst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sha\sha1dgst.c"
$(OUTDIR)\sha256-mb-x86_64.obj: crypto\sha\sha256-mb-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\sha256-mb-x86_64.asm"
$(OUTDIR)\sha256-x86_64.obj: crypto\sha\sha256-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\sha256-x86_64.asm"
$(OUTDIR)\sha256.obj: crypto\sha\sha256.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sha\sha256.c"
$(OUTDIR)\sha3.obj: crypto\sha\sha3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sha\sha3.c"
$(OUTDIR)\sha512-x86_64.obj: crypto\sha\sha512-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\sha\sha512-x86_64.asm"
$(OUTDIR)\sha512.obj: crypto\sha\sha512.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sha\sha512.c"
$(OUTDIR)\siphash.obj: crypto\siphash\siphash.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\siphash\siphash.c"
$(OUTDIR)\legacy_sm3.obj: crypto\sm3\legacy_sm3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sm3\legacy_sm3.c"
$(OUTDIR)\sm3.obj: crypto\sm3\sm3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\sm3\sm3.c"
$(OUTDIR)\stack.obj: crypto\stack\stack.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\stack\stack.c"
$(OUTDIR)\store_err.obj: crypto\store\store_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_err.c"
$(OUTDIR)\store_init.obj: crypto\store\store_init.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_init.c"
$(OUTDIR)\store_lib.obj: crypto\store\store_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_lib.c"
$(OUTDIR)\store_meth.obj: crypto\store\store_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_meth.c"
$(OUTDIR)\store_register.obj: crypto\store\store_register.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_register.c"
$(OUTDIR)\store_result.obj: crypto\store\store_result.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_result.c"
$(OUTDIR)\store_strings.obj: crypto\store\store_strings.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\store\store_strings.c"
$(OUTDIR)\ts_asn1.obj: crypto\ts\ts_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_asn1.c"
$(OUTDIR)\ts_conf.obj: crypto\ts\ts_conf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_conf.c"
$(OUTDIR)\ts_err.obj: crypto\ts\ts_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_err.c"
$(OUTDIR)\ts_lib.obj: crypto\ts\ts_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_lib.c"
$(OUTDIR)\ts_req_print.obj: crypto\ts\ts_req_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_req_print.c"
$(OUTDIR)\ts_req_utils.obj: crypto\ts\ts_req_utils.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_req_utils.c"
$(OUTDIR)\ts_rsp_print.obj: crypto\ts\ts_rsp_print.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_rsp_print.c"
$(OUTDIR)\ts_rsp_sign.obj: crypto\ts\ts_rsp_sign.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_rsp_sign.c"
$(OUTDIR)\ts_rsp_utils.obj: crypto\ts\ts_rsp_utils.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_rsp_utils.c"
$(OUTDIR)\ts_rsp_verify.obj: crypto\ts\ts_rsp_verify.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_rsp_verify.c"
$(OUTDIR)\ts_verify_ctx.obj: crypto\ts\ts_verify_ctx.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ts\ts_verify_ctx.c"
$(OUTDIR)\txt_db.obj: crypto\txt_db\txt_db.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\txt_db\txt_db.c"
$(OUTDIR)\ui_err.obj: crypto\ui\ui_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ui\ui_err.c"
$(OUTDIR)\ui_lib.obj: crypto\ui\ui_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ui\ui_lib.c"
$(OUTDIR)\ui_null.obj: crypto\ui\ui_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ui\ui_null.c"
$(OUTDIR)\ui_openssl.obj: crypto\ui\ui_openssl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ui\ui_openssl.c"
$(OUTDIR)\ui_util.obj: crypto\ui\ui_util.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\ui\ui_util.c"
$(OUTDIR)\wp-x86_64.obj: crypto\whrlpool\wp-x86_64.asm
	$(AS)  $(LIB_ASFLAGS) $(ASOUTFLAG)$@ "crypto\whrlpool\wp-x86_64.asm"
$(OUTDIR)\wp_dgst.obj: crypto\whrlpool\wp_dgst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\whrlpool\wp_dgst.c"
$(OUTDIR)\by_dir.obj: crypto\x509\by_dir.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\by_dir.c"
$(OUTDIR)\by_file.obj: crypto\x509\by_file.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\by_file.c"
$(OUTDIR)\by_store.obj: crypto\x509\by_store.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\by_store.c"
$(OUTDIR)\pcy_cache.obj: crypto\x509\pcy_cache.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_cache.c"
$(OUTDIR)\pcy_data.obj: crypto\x509\pcy_data.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_data.c"
$(OUTDIR)\pcy_lib.obj: crypto\x509\pcy_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_lib.c"
$(OUTDIR)\pcy_map.obj: crypto\x509\pcy_map.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_map.c"
$(OUTDIR)\pcy_node.obj: crypto\x509\pcy_node.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_node.c"
$(OUTDIR)\pcy_tree.obj: crypto\x509\pcy_tree.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\pcy_tree.c"
$(OUTDIR)\t_crl.obj: crypto\x509\t_crl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\t_crl.c"
$(OUTDIR)\t_req.obj: crypto\x509\t_req.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\t_req.c"
$(OUTDIR)\t_x509.obj: crypto\x509\t_x509.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\t_x509.c"
$(OUTDIR)\v3_addr.obj: crypto\x509\v3_addr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_addr.c"
$(OUTDIR)\v3_admis.obj: crypto\x509\v3_admis.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_admis.c"
$(OUTDIR)\v3_akeya.obj: crypto\x509\v3_akeya.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_akeya.c"
$(OUTDIR)\v3_akid.obj: crypto\x509\v3_akid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_akid.c"
$(OUTDIR)\v3_asid.obj: crypto\x509\v3_asid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_asid.c"
$(OUTDIR)\v3_bcons.obj: crypto\x509\v3_bcons.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_bcons.c"
$(OUTDIR)\v3_bitst.obj: crypto\x509\v3_bitst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_bitst.c"
$(OUTDIR)\v3_conf.obj: crypto\x509\v3_conf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_conf.c"
$(OUTDIR)\v3_cpols.obj: crypto\x509\v3_cpols.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_cpols.c"
$(OUTDIR)\v3_crld.obj: crypto\x509\v3_crld.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_crld.c"
$(OUTDIR)\v3_enum.obj: crypto\x509\v3_enum.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_enum.c"
$(OUTDIR)\v3_extku.obj: crypto\x509\v3_extku.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_extku.c"
$(OUTDIR)\v3_genn.obj: crypto\x509\v3_genn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_genn.c"
$(OUTDIR)\v3_ia5.obj: crypto\x509\v3_ia5.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_ia5.c"
$(OUTDIR)\v3_info.obj: crypto\x509\v3_info.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_info.c"
$(OUTDIR)\v3_int.obj: crypto\x509\v3_int.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_int.c"
$(OUTDIR)\v3_ist.obj: crypto\x509\v3_ist.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_ist.c"
$(OUTDIR)\v3_lib.obj: crypto\x509\v3_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_lib.c"
$(OUTDIR)\v3_ncons.obj: crypto\x509\v3_ncons.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_ncons.c"
$(OUTDIR)\v3_pci.obj: crypto\x509\v3_pci.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_pci.c"
$(OUTDIR)\v3_pcia.obj: crypto\x509\v3_pcia.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_pcia.c"
$(OUTDIR)\v3_pcons.obj: crypto\x509\v3_pcons.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_pcons.c"
$(OUTDIR)\v3_pku.obj: crypto\x509\v3_pku.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_pku.c"
$(OUTDIR)\v3_pmaps.obj: crypto\x509\v3_pmaps.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_pmaps.c"
$(OUTDIR)\v3_prn.obj: crypto\x509\v3_prn.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_prn.c"
$(OUTDIR)\v3_purp.obj: crypto\x509\v3_purp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_purp.c"
$(OUTDIR)\v3_san.obj: crypto\x509\v3_san.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_san.c"
$(OUTDIR)\v3_skid.obj: crypto\x509\v3_skid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_skid.c"
$(OUTDIR)\v3_sxnet.obj: crypto\x509\v3_sxnet.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_sxnet.c"
$(OUTDIR)\v3_tlsf.obj: crypto\x509\v3_tlsf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_tlsf.c"
$(OUTDIR)\v3_utf8.obj: crypto\x509\v3_utf8.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_utf8.c"
$(OUTDIR)\v3_utl.obj: crypto\x509\v3_utl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3_utl.c"
$(OUTDIR)\v3err.obj: crypto\x509\v3err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\v3err.c"
$(OUTDIR)\x509_att.obj: crypto\x509\x509_att.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_att.c"
$(OUTDIR)\x509_cmp.obj: crypto\x509\x509_cmp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_cmp.c"
$(OUTDIR)\x509_d2.obj: crypto\x509\x509_d2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_d2.c"
$(OUTDIR)\x509_def.obj: crypto\x509\x509_def.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_def.c"
$(OUTDIR)\x509_err.obj: crypto\x509\x509_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_err.c"
$(OUTDIR)\x509_ext.obj: crypto\x509\x509_ext.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_ext.c"
$(OUTDIR)\x509_lu.obj: crypto\x509\x509_lu.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_lu.c"
$(OUTDIR)\x509_meth.obj: crypto\x509\x509_meth.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_meth.c"
$(OUTDIR)\x509_obj.obj: crypto\x509\x509_obj.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_obj.c"
$(OUTDIR)\x509_r2x.obj: crypto\x509\x509_r2x.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_r2x.c"
$(OUTDIR)\x509_req.obj: crypto\x509\x509_req.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_req.c"
$(OUTDIR)\x509_set.obj: crypto\x509\x509_set.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_set.c"
$(OUTDIR)\x509_trust.obj: crypto\x509\x509_trust.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_trust.c"
$(OUTDIR)\x509_txt.obj: crypto\x509\x509_txt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_txt.c"
$(OUTDIR)\x509_v3.obj: crypto\x509\x509_v3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_v3.c"
$(OUTDIR)\x509_vfy.obj: crypto\x509\x509_vfy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_vfy.c"
$(OUTDIR)\x509_vpm.obj: crypto\x509\x509_vpm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509_vpm.c"
$(OUTDIR)\x509cset.obj: crypto\x509\x509cset.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509cset.c"
$(OUTDIR)\x509name.obj: crypto\x509\x509name.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509name.c"
$(OUTDIR)\x509rset.obj: crypto\x509\x509rset.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509rset.c"
$(OUTDIR)\x509spki.obj: crypto\x509\x509spki.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509spki.c"
$(OUTDIR)\x509type.obj: crypto\x509\x509type.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x509type.c"
$(OUTDIR)\x_all.obj: crypto\x509\x_all.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_all.c"
$(OUTDIR)\x_attrib.obj: crypto\x509\x_attrib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_attrib.c"
$(OUTDIR)\x_crl.obj: crypto\x509\x_crl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_crl.c"
$(OUTDIR)\x_exten.obj: crypto\x509\x_exten.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_exten.c"
$(OUTDIR)\x_name.obj: crypto\x509\x_name.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_name.c"
$(OUTDIR)\x_pubkey.obj: crypto\x509\x_pubkey.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_pubkey.c"
$(OUTDIR)\x_req.obj: crypto\x509\x_req.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_req.c"
$(OUTDIR)\x_x509.obj: crypto\x509\x_x509.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_x509.c"
$(OUTDIR)\x_x509a.obj: crypto\x509\x_x509a.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "crypto\x509\x_x509a.c"
$(OUTDIR)\baseprov.obj: providers\baseprov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\baseprov.c"
$(OUTDIR)\defltprov.obj: providers\defltprov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\defltprov.c"
$(OUTDIR)\nullprov.obj: providers\nullprov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\nullprov.c"
$(OUTDIR)\prov_running.obj: providers\prov_running.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" -I"providers\common\include" -I"providers\implementations\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DGHASH_ASM -DKECCAK1600_ASM -DMD5_ASM -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DPOLY1305_ASM -DRC4_ASM -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DWHIRLPOOL_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\prov_running.c"
$(OUTDIR)\$(LIBSSL): $(OUTDIR) $(OUTDIR)\bio_ssl.obj $(OUTDIR)\d1_lib.obj $(OUTDIR)\d1_msg.obj $(OUTDIR)\d1_srtp.obj $(OUTDIR)\methods.obj $(OUTDIR)\pqueue.obj $(OUTDIR)\s3_enc.obj $(OUTDIR)\s3_lib.obj $(OUTDIR)\s3_msg.obj $(OUTDIR)\ssl_asn1.obj $(OUTDIR)\ssl_cert.obj $(OUTDIR)\ssl_ciph.obj $(OUTDIR)\ssl_conf.obj $(OUTDIR)\ssl_err.obj $(OUTDIR)\ssl_err_legacy.obj $(OUTDIR)\ssl_init.obj $(OUTDIR)\ssl_lib.obj $(OUTDIR)\ssl_mcnf.obj $(OUTDIR)\ssl_rsa.obj $(OUTDIR)\ssl_rsa_legacy.obj $(OUTDIR)\ssl_sess.obj $(OUTDIR)\ssl_stat.obj $(OUTDIR)\ssl_txt.obj $(OUTDIR)\ssl_utst.obj $(OUTDIR)\t1_enc.obj $(OUTDIR)\t1_lib.obj $(OUTDIR)\t1_trce.obj $(OUTDIR)\tls13_enc.obj $(OUTDIR)\tls_depr.obj $(OUTDIR)\tls_srp.obj $(OUTDIR)\dtls1_bitmap.obj $(OUTDIR)\rec_layer_d1.obj $(OUTDIR)\rec_layer_s3.obj $(OUTDIR)\ssl3_buffer.obj $(OUTDIR)\ssl3_record.obj $(OUTDIR)\ssl3_record_tls13.obj $(OUTDIR)\extensions.obj $(OUTDIR)\extensions_clnt.obj $(OUTDIR)\extensions_cust.obj $(OUTDIR)\extensions_srvr.obj $(OUTDIR)\statem.obj $(OUTDIR)\statem_clnt.obj $(OUTDIR)\statem_dtls.obj $(OUTDIR)\statem_lib.obj $(OUTDIR)\statem_srvr.obj
	$(AR) $(ARFLAGS) $(AROUTFLAG)$(OUTDIR)\$(LIBSSL) @<<
$(OUTDIR)\bio_ssl.obj
$(OUTDIR)\d1_lib.obj
$(OUTDIR)\d1_msg.obj
$(OUTDIR)\d1_srtp.obj
$(OUTDIR)\methods.obj
$(OUTDIR)\pqueue.obj
$(OUTDIR)\s3_enc.obj
$(OUTDIR)\s3_lib.obj
$(OUTDIR)\s3_msg.obj
$(OUTDIR)\ssl_asn1.obj
$(OUTDIR)\ssl_cert.obj
$(OUTDIR)\ssl_ciph.obj
$(OUTDIR)\ssl_conf.obj
$(OUTDIR)\ssl_err.obj
$(OUTDIR)\ssl_err_legacy.obj
$(OUTDIR)\ssl_init.obj
$(OUTDIR)\ssl_lib.obj
$(OUTDIR)\ssl_mcnf.obj
$(OUTDIR)\ssl_rsa.obj
$(OUTDIR)\ssl_rsa_legacy.obj
$(OUTDIR)\ssl_sess.obj
$(OUTDIR)\ssl_stat.obj
$(OUTDIR)\ssl_txt.obj
$(OUTDIR)\ssl_utst.obj
$(OUTDIR)\t1_enc.obj
$(OUTDIR)\t1_lib.obj
$(OUTDIR)\t1_trce.obj
$(OUTDIR)\tls13_enc.obj
$(OUTDIR)\tls_depr.obj
$(OUTDIR)\tls_srp.obj
$(OUTDIR)\dtls1_bitmap.obj
$(OUTDIR)\rec_layer_d1.obj
$(OUTDIR)\rec_layer_s3.obj
$(OUTDIR)\ssl3_buffer.obj
$(OUTDIR)\ssl3_record.obj
$(OUTDIR)\ssl3_record_tls13.obj
$(OUTDIR)\extensions.obj
$(OUTDIR)\extensions_clnt.obj
$(OUTDIR)\extensions_cust.obj
$(OUTDIR)\extensions_srvr.obj
$(OUTDIR)\statem.obj
$(OUTDIR)\statem_clnt.obj
$(OUTDIR)\statem_dtls.obj
$(OUTDIR)\statem_lib.obj
$(OUTDIR)\statem_srvr.obj
<<
$(OUTDIR)\bio_ssl.obj: ssl\bio_ssl.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\bio_ssl.c"
$(OUTDIR)\d1_lib.obj: ssl\d1_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\d1_lib.c"
$(OUTDIR)\d1_msg.obj: ssl\d1_msg.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\d1_msg.c"
$(OUTDIR)\d1_srtp.obj: ssl\d1_srtp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\d1_srtp.c"
$(OUTDIR)\methods.obj: ssl\methods.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\methods.c"
$(OUTDIR)\pqueue.obj: ssl\pqueue.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\pqueue.c"
$(OUTDIR)\s3_enc.obj: ssl\s3_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\s3_enc.c"
$(OUTDIR)\s3_lib.obj: ssl\s3_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\s3_lib.c"
$(OUTDIR)\s3_msg.obj: ssl\s3_msg.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\s3_msg.c"
$(OUTDIR)\ssl_asn1.obj: ssl\ssl_asn1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_asn1.c"
$(OUTDIR)\ssl_cert.obj: ssl\ssl_cert.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_cert.c"
$(OUTDIR)\ssl_ciph.obj: ssl\ssl_ciph.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_ciph.c"
$(OUTDIR)\ssl_conf.obj: ssl\ssl_conf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_conf.c"
$(OUTDIR)\ssl_err.obj: ssl\ssl_err.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_err.c"
$(OUTDIR)\ssl_err_legacy.obj: ssl\ssl_err_legacy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_err_legacy.c"
$(OUTDIR)\ssl_init.obj: ssl\ssl_init.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_init.c"
$(OUTDIR)\ssl_lib.obj: ssl\ssl_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_lib.c"
$(OUTDIR)\ssl_mcnf.obj: ssl\ssl_mcnf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_mcnf.c"
$(OUTDIR)\ssl_rsa.obj: ssl\ssl_rsa.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_rsa.c"
$(OUTDIR)\ssl_rsa_legacy.obj: ssl\ssl_rsa_legacy.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_rsa_legacy.c"
$(OUTDIR)\ssl_sess.obj: ssl\ssl_sess.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_sess.c"
$(OUTDIR)\ssl_stat.obj: ssl\ssl_stat.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_stat.c"
$(OUTDIR)\ssl_txt.obj: ssl\ssl_txt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_txt.c"
$(OUTDIR)\ssl_utst.obj: ssl\ssl_utst.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\ssl_utst.c"
$(OUTDIR)\t1_enc.obj: ssl\t1_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\t1_enc.c"
$(OUTDIR)\t1_lib.obj: ssl\t1_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\t1_lib.c"
$(OUTDIR)\t1_trce.obj: ssl\t1_trce.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\t1_trce.c"
$(OUTDIR)\tls13_enc.obj: ssl\tls13_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\tls13_enc.c"
$(OUTDIR)\tls_depr.obj: ssl\tls_depr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\tls_depr.c"
$(OUTDIR)\tls_srp.obj: ssl\tls_srp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\tls_srp.c"
$(OUTDIR)\dtls1_bitmap.obj: ssl\record\dtls1_bitmap.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\dtls1_bitmap.c"
$(OUTDIR)\rec_layer_d1.obj: ssl\record\rec_layer_d1.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\rec_layer_d1.c"
$(OUTDIR)\rec_layer_s3.obj: ssl\record\rec_layer_s3.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\rec_layer_s3.c"
$(OUTDIR)\ssl3_buffer.obj: ssl\record\ssl3_buffer.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\ssl3_buffer.c"
$(OUTDIR)\ssl3_record.obj: ssl\record\ssl3_record.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\ssl3_record.c"
$(OUTDIR)\ssl3_record_tls13.obj: ssl\record\ssl3_record_tls13.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\record\ssl3_record_tls13.c"
$(OUTDIR)\extensions.obj: ssl\statem\extensions.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\extensions.c"
$(OUTDIR)\extensions_clnt.obj: ssl\statem\extensions_clnt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\extensions_clnt.c"
$(OUTDIR)\extensions_cust.obj: ssl\statem\extensions_cust.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\extensions_cust.c"
$(OUTDIR)\extensions_srvr.obj: ssl\statem\extensions_srvr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\extensions_srvr.c"
$(OUTDIR)\statem.obj: ssl\statem\statem.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\statem.c"
$(OUTDIR)\statem_clnt.obj: ssl\statem\statem_clnt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\statem_clnt.c"
$(OUTDIR)\statem_dtls.obj: ssl\statem\statem_dtls.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\statem_dtls.c"
$(OUTDIR)\statem_lib.obj: ssl\statem\statem_lib.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\statem_lib.c"
$(OUTDIR)\statem_srvr.obj: ssl\statem\statem_srvr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"include" $(LIB_CPPFLAGS)  -DAES_ASM -c $(COUTFLAG)$@ "ssl\statem\statem_srvr.c"
providers\libcommon.lib: $(OUTDIR)\der_digests_gen.obj $(OUTDIR)\der_dsa_gen.obj $(OUTDIR)\der_dsa_key.obj $(OUTDIR)\der_dsa_sig.obj $(OUTDIR)\der_ec_gen.obj $(OUTDIR)\der_ec_key.obj $(OUTDIR)\der_ec_sig.obj $(OUTDIR)\der_ecx_gen.obj $(OUTDIR)\der_ecx_key.obj $(OUTDIR)\der_rsa_gen.obj $(OUTDIR)\der_rsa_key.obj $(OUTDIR)\der_wrap_gen.obj $(OUTDIR)\provider_ctx.obj $(OUTDIR)\provider_err.obj $(OUTDIR)\ciphercommon.obj $(OUTDIR)\ciphercommon_block.obj $(OUTDIR)\ciphercommon_ccm.obj $(OUTDIR)\ciphercommon_ccm_hw.obj $(OUTDIR)\ciphercommon_gcm.obj $(OUTDIR)\ciphercommon_gcm_hw.obj $(OUTDIR)\ciphercommon_hw.obj $(OUTDIR)\digestcommon.obj $(OUTDIR)\tls_pad.obj
	$(AR) $(ARFLAGS) $(AROUTFLAG)providers\libcommon.lib @<<
$(OUTDIR)\der_digests_gen.obj
$(OUTDIR)\der_dsa_gen.obj
$(OUTDIR)\der_dsa_key.obj
$(OUTDIR)\der_dsa_sig.obj
$(OUTDIR)\der_ec_gen.obj
$(OUTDIR)\der_ec_key.obj
$(OUTDIR)\der_ec_sig.obj
$(OUTDIR)\der_ecx_gen.obj
$(OUTDIR)\der_ecx_key.obj
$(OUTDIR)\der_rsa_gen.obj
$(OUTDIR)\der_rsa_key.obj
$(OUTDIR)\der_wrap_gen.obj
$(OUTDIR)\provider_ctx.obj
$(OUTDIR)\provider_err.obj
$(OUTDIR)\ciphercommon.obj
$(OUTDIR)\ciphercommon_block.obj
$(OUTDIR)\ciphercommon_ccm.obj
$(OUTDIR)\ciphercommon_ccm_hw.obj
$(OUTDIR)\ciphercommon_gcm.obj
$(OUTDIR)\ciphercommon_gcm_hw.obj
$(OUTDIR)\ciphercommon_hw.obj
$(OUTDIR)\digestcommon.obj
$(OUTDIR)\tls_pad.obj
<<
$(OUTDIR)\der_digests_gen.obj: providers\common\der\der_digests_gen.c providers\common\include\prov\der_digests.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_digests_gen.c"
$(OUTDIR)\der_dsa_gen.obj: providers\common\der\der_dsa_gen.c providers\common\include\prov\der_dsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_dsa_gen.c"
$(OUTDIR)\der_dsa_key.obj: providers\common\der\der_dsa_key.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_dsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_dsa_key.c"
$(OUTDIR)\der_dsa_sig.obj: providers\common\der\der_dsa_sig.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_dsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_dsa_sig.c"
$(OUTDIR)\der_ec_gen.obj: providers\common\der\der_ec_gen.c providers\common\include\prov\der_ec.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_ec_gen.c"
$(OUTDIR)\der_ec_key.obj: providers\common\der\der_ec_key.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_ec.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_ec_key.c"
$(OUTDIR)\der_ec_sig.obj: providers\common\der\der_ec_sig.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_ec.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_ec_sig.c"
$(OUTDIR)\der_ecx_gen.obj: providers\common\der\der_ecx_gen.c providers\common\include\prov\der_ecx.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_ecx_gen.c"
$(OUTDIR)\der_ecx_key.obj: providers\common\der\der_ecx_key.c providers\common\include\prov\der_ecx.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_ecx_key.c"
$(OUTDIR)\der_rsa_gen.obj: providers\common\der\der_rsa_gen.c providers\common\include\prov\der_rsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_rsa_gen.c"
$(OUTDIR)\der_rsa_key.obj: providers\common\der\der_rsa_key.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_rsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_rsa_key.c"
$(OUTDIR)\der_wrap_gen.obj: providers\common\der\der_wrap_gen.c providers\common\include\prov\der_wrap.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\der\der_wrap_gen.c"
$(OUTDIR)\provider_ctx.obj: providers\common\provider_ctx.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\provider_ctx.c"
$(OUTDIR)\provider_err.obj: providers\common\provider_err.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\common\provider_err.c"
$(OUTDIR)\ciphercommon.obj: providers\implementations\ciphers\ciphercommon.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon.c"
$(OUTDIR)\ciphercommon_block.obj: providers\implementations\ciphers\ciphercommon_block.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_block.c"
$(OUTDIR)\ciphercommon_ccm.obj: providers\implementations\ciphers\ciphercommon_ccm.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_ccm.c"
$(OUTDIR)\ciphercommon_ccm_hw.obj: providers\implementations\ciphers\ciphercommon_ccm_hw.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_ccm_hw.c"
$(OUTDIR)\ciphercommon_gcm.obj: providers\implementations\ciphers\ciphercommon_gcm.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_gcm.c"
$(OUTDIR)\ciphercommon_gcm_hw.obj: providers\implementations\ciphers\ciphercommon_gcm_hw.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_gcm_hw.c"
$(OUTDIR)\ciphercommon_hw.obj: providers\implementations\ciphers\ciphercommon_hw.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\ciphers\ciphercommon_hw.c"
$(OUTDIR)\digestcommon.obj: providers\implementations\digests\digestcommon.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "providers\implementations\digests\digestcommon.c"
$(OUTDIR)\tls_pad.obj: ssl\record\tls_pad.c
	$(CC)  $(LIB_CFLAGS) -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DOPENSSL_BN_ASM_GF2m -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -c $(COUTFLAG)$@ "ssl\record\tls_pad.c"
providers\libdefault.lib: $(OUTDIR)\der_rsa_sig.obj $(OUTDIR)\bio_prov.obj $(OUTDIR)\capabilities.obj $(OUTDIR)\digest_to_nid.obj $(OUTDIR)\provider_seeding.obj $(OUTDIR)\provider_util.obj $(OUTDIR)\securitycheck.obj $(OUTDIR)\securitycheck_default.obj $(OUTDIR)\rsa_enc.obj $(OUTDIR)\cipher_aes.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha1_hw.obj $(OUTDIR)\cipher_aes_cbc_hmac_sha256_hw.obj $(OUTDIR)\cipher_aes_ccm.obj $(OUTDIR)\cipher_aes_ccm_hw.obj $(OUTDIR)\cipher_aes_gcm.obj $(OUTDIR)\cipher_aes_gcm_hw.obj $(OUTDIR)\cipher_aes_hw.obj $(OUTDIR)\cipher_aes_ocb.obj $(OUTDIR)\cipher_aes_ocb_hw.obj $(OUTDIR)\cipher_aes_siv.obj $(OUTDIR)\cipher_aes_siv_hw.obj $(OUTDIR)\cipher_aes_wrp.obj $(OUTDIR)\cipher_aes_xts.obj $(OUTDIR)\cipher_aes_xts_fips.obj $(OUTDIR)\cipher_aes_xts_hw.obj $(OUTDIR)\cipher_chacha20.obj $(OUTDIR)\cipher_chacha20_hw.obj $(OUTDIR)\cipher_chacha20_poly1305.obj $(OUTDIR)\cipher_chacha20_poly1305_hw.obj $(OUTDIR)\cipher_cts.obj $(OUTDIR)\cipher_null.obj $(OUTDIR)\cipher_tdes.obj $(OUTDIR)\cipher_tdes_common.obj $(OUTDIR)\cipher_tdes_default.obj $(OUTDIR)\cipher_tdes_default_hw.obj $(OUTDIR)\cipher_tdes_hw.obj $(OUTDIR)\cipher_tdes_wrap.obj $(OUTDIR)\cipher_tdes_wrap_hw.obj $(OUTDIR)\blake2_prov.obj $(OUTDIR)\blake2b_prov.obj $(OUTDIR)\blake2s_prov.obj $(OUTDIR)\md5_prov.obj $(OUTDIR)\md5_sha1_prov.obj $(OUTDIR)\null_prov.obj $(OUTDIR)\ripemd_prov.obj $(OUTDIR)\sha2_prov.obj $(OUTDIR)\sha3_prov.obj $(OUTDIR)\sm3_prov.obj $(OUTDIR)\decode_der2key.obj $(OUTDIR)\decode_epki2pki.obj $(OUTDIR)\decode_msblob2key.obj $(OUTDIR)\decode_pem2der.obj $(OUTDIR)\decode_pvk2key.obj $(OUTDIR)\decode_spki2typespki.obj $(OUTDIR)\encode_key2any.obj $(OUTDIR)\encode_key2blob.obj $(OUTDIR)\encode_key2ms.obj $(OUTDIR)\encode_key2text.obj $(OUTDIR)\endecoder_common.obj $(OUTDIR)\dh_exch.obj $(OUTDIR)\ecdh_exch.obj $(OUTDIR)\ecx_exch.obj $(OUTDIR)\kdf_exch.obj $(OUTDIR)\hkdf.obj $(OUTDIR)\kbkdf.obj $(OUTDIR)\krb5kdf.obj $(OUTDIR)\pbkdf2.obj $(OUTDIR)\pbkdf2_fips.obj $(OUTDIR)\pkcs12kdf.obj $(OUTDIR)\scrypt.obj $(OUTDIR)\sshkdf.obj $(OUTDIR)\sskdf.obj $(OUTDIR)\tls1_prf.obj $(OUTDIR)\x942kdf.obj $(OUTDIR)\rsa_kem.obj $(OUTDIR)\dh_kmgmt.obj $(OUTDIR)\dsa_kmgmt.obj $(OUTDIR)\ec_kmgmt.obj $(OUTDIR)\ecx_kmgmt.obj $(OUTDIR)\kdf_legacy_kmgmt.obj $(OUTDIR)\mac_legacy_kmgmt.obj $(OUTDIR)\rsa_kmgmt.obj $(OUTDIR)\blake2b_mac.obj $(OUTDIR)\blake2s_mac.obj $(OUTDIR)\cmac_prov.obj $(OUTDIR)\gmac_prov.obj $(OUTDIR)\hmac_prov.obj $(OUTDIR)\kmac_prov.obj $(OUTDIR)\poly1305_prov.obj $(OUTDIR)\siphash_prov.obj $(OUTDIR)\crngt.obj $(OUTDIR)\drbg.obj $(OUTDIR)\drbg_ctr.obj $(OUTDIR)\drbg_hash.obj $(OUTDIR)\drbg_hmac.obj $(OUTDIR)\seed_src.obj $(OUTDIR)\test_rng.obj $(OUTDIR)\rand_cpu_x86.obj $(OUTDIR)\rand_tsc.obj $(OUTDIR)\rand_unix.obj $(OUTDIR)\rand_win.obj $(OUTDIR)\dsa_sig.obj $(OUTDIR)\ecdsa_sig.obj $(OUTDIR)\eddsa_sig.obj $(OUTDIR)\mac_legacy_sig.obj $(OUTDIR)\rsa_sig.obj $(OUTDIR)\file_store.obj $(OUTDIR)\file_store_any2obj.obj $(OUTDIR)\s3_cbc.obj
	$(AR) $(ARFLAGS) $(AROUTFLAG)providers\libdefault.lib @<<
$(OUTDIR)\der_rsa_sig.obj
$(OUTDIR)\bio_prov.obj
$(OUTDIR)\capabilities.obj
$(OUTDIR)\digest_to_nid.obj
$(OUTDIR)\provider_seeding.obj
$(OUTDIR)\provider_util.obj
$(OUTDIR)\securitycheck.obj
$(OUTDIR)\securitycheck_default.obj
$(OUTDIR)\rsa_enc.obj
$(OUTDIR)\cipher_aes.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha1_hw.obj
$(OUTDIR)\cipher_aes_cbc_hmac_sha256_hw.obj
$(OUTDIR)\cipher_aes_ccm.obj
$(OUTDIR)\cipher_aes_ccm_hw.obj
$(OUTDIR)\cipher_aes_gcm.obj
$(OUTDIR)\cipher_aes_gcm_hw.obj
$(OUTDIR)\cipher_aes_hw.obj
$(OUTDIR)\cipher_aes_ocb.obj
$(OUTDIR)\cipher_aes_ocb_hw.obj
$(OUTDIR)\cipher_aes_siv.obj
$(OUTDIR)\cipher_aes_siv_hw.obj
$(OUTDIR)\cipher_aes_wrp.obj
$(OUTDIR)\cipher_aes_xts.obj
$(OUTDIR)\cipher_aes_xts_fips.obj
$(OUTDIR)\cipher_aes_xts_hw.obj
$(OUTDIR)\cipher_chacha20.obj
$(OUTDIR)\cipher_chacha20_hw.obj
$(OUTDIR)\cipher_chacha20_poly1305.obj
$(OUTDIR)\cipher_chacha20_poly1305_hw.obj
$(OUTDIR)\cipher_cts.obj
$(OUTDIR)\cipher_null.obj
$(OUTDIR)\cipher_tdes.obj
$(OUTDIR)\cipher_tdes_common.obj
$(OUTDIR)\cipher_tdes_default.obj
$(OUTDIR)\cipher_tdes_default_hw.obj
$(OUTDIR)\cipher_tdes_hw.obj
$(OUTDIR)\cipher_tdes_wrap.obj
$(OUTDIR)\cipher_tdes_wrap_hw.obj
$(OUTDIR)\blake2_prov.obj
$(OUTDIR)\blake2b_prov.obj
$(OUTDIR)\blake2s_prov.obj
$(OUTDIR)\md5_prov.obj
$(OUTDIR)\md5_sha1_prov.obj
$(OUTDIR)\null_prov.obj
$(OUTDIR)\ripemd_prov.obj
$(OUTDIR)\sha2_prov.obj
$(OUTDIR)\sha3_prov.obj
$(OUTDIR)\sm3_prov.obj
$(OUTDIR)\decode_der2key.obj
$(OUTDIR)\decode_epki2pki.obj
$(OUTDIR)\decode_msblob2key.obj
$(OUTDIR)\decode_pem2der.obj
$(OUTDIR)\decode_pvk2key.obj
$(OUTDIR)\decode_spki2typespki.obj
$(OUTDIR)\encode_key2any.obj
$(OUTDIR)\encode_key2blob.obj
$(OUTDIR)\encode_key2ms.obj
$(OUTDIR)\encode_key2text.obj
$(OUTDIR)\endecoder_common.obj
$(OUTDIR)\dh_exch.obj
$(OUTDIR)\ecdh_exch.obj
$(OUTDIR)\ecx_exch.obj
$(OUTDIR)\kdf_exch.obj
$(OUTDIR)\hkdf.obj
$(OUTDIR)\kbkdf.obj
$(OUTDIR)\krb5kdf.obj
$(OUTDIR)\pbkdf2.obj
$(OUTDIR)\pbkdf2_fips.obj
$(OUTDIR)\pkcs12kdf.obj
$(OUTDIR)\scrypt.obj
$(OUTDIR)\sshkdf.obj
$(OUTDIR)\sskdf.obj
$(OUTDIR)\tls1_prf.obj
$(OUTDIR)\x942kdf.obj
$(OUTDIR)\rsa_kem.obj
$(OUTDIR)\dh_kmgmt.obj
$(OUTDIR)\dsa_kmgmt.obj
$(OUTDIR)\ec_kmgmt.obj
$(OUTDIR)\ecx_kmgmt.obj
$(OUTDIR)\kdf_legacy_kmgmt.obj
$(OUTDIR)\mac_legacy_kmgmt.obj
$(OUTDIR)\rsa_kmgmt.obj
$(OUTDIR)\blake2b_mac.obj
$(OUTDIR)\blake2s_mac.obj
$(OUTDIR)\cmac_prov.obj
$(OUTDIR)\gmac_prov.obj
$(OUTDIR)\hmac_prov.obj
$(OUTDIR)\kmac_prov.obj
$(OUTDIR)\poly1305_prov.obj
$(OUTDIR)\siphash_prov.obj
$(OUTDIR)\crngt.obj
$(OUTDIR)\drbg.obj
$(OUTDIR)\drbg_ctr.obj
$(OUTDIR)\drbg_hash.obj
$(OUTDIR)\drbg_hmac.obj
$(OUTDIR)\seed_src.obj
$(OUTDIR)\test_rng.obj
$(OUTDIR)\rand_cpu_x86.obj
$(OUTDIR)\rand_tsc.obj
$(OUTDIR)\rand_unix.obj
$(OUTDIR)\rand_win.obj
$(OUTDIR)\dsa_sig.obj
$(OUTDIR)\ecdsa_sig.obj
$(OUTDIR)\eddsa_sig.obj
$(OUTDIR)\mac_legacy_sig.obj
$(OUTDIR)\rsa_sig.obj
$(OUTDIR)\file_store.obj
$(OUTDIR)\file_store_any2obj.obj
$(OUTDIR)\s3_cbc.obj
<<
$(OUTDIR)\der_rsa_sig.obj: providers\common\der\der_rsa_sig.c providers\common\include\prov\der_digests.h providers\common\include\prov\der_rsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\der\der_rsa_sig.c"
$(OUTDIR)\bio_prov.obj: providers\common\bio_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\bio_prov.c"
$(OUTDIR)\capabilities.obj: providers\common\capabilities.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\capabilities.c"
$(OUTDIR)\digest_to_nid.obj: providers\common\digest_to_nid.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\digest_to_nid.c"
$(OUTDIR)\provider_seeding.obj: providers\common\provider_seeding.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\provider_seeding.c"
$(OUTDIR)\provider_util.obj: providers\common\provider_util.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\provider_util.c"
$(OUTDIR)\securitycheck.obj: providers\common\securitycheck.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\securitycheck.c"
$(OUTDIR)\securitycheck_default.obj: providers\common\securitycheck_default.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\common\securitycheck_default.c"
$(OUTDIR)\rsa_enc.obj: providers\implementations\asymciphers\rsa_enc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\asymciphers\rsa_enc.c"
$(OUTDIR)\cipher_aes.obj: providers\implementations\ciphers\cipher_aes.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes.c"
$(OUTDIR)\cipher_aes_cbc_hmac_sha.obj: providers\implementations\ciphers\cipher_aes_cbc_hmac_sha.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_cbc_hmac_sha.c"
$(OUTDIR)\cipher_aes_cbc_hmac_sha1_hw.obj: providers\implementations\ciphers\cipher_aes_cbc_hmac_sha1_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_cbc_hmac_sha1_hw.c"
$(OUTDIR)\cipher_aes_cbc_hmac_sha256_hw.obj: providers\implementations\ciphers\cipher_aes_cbc_hmac_sha256_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_cbc_hmac_sha256_hw.c"
$(OUTDIR)\cipher_aes_ccm.obj: providers\implementations\ciphers\cipher_aes_ccm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_ccm.c"
$(OUTDIR)\cipher_aes_ccm_hw.obj: providers\implementations\ciphers\cipher_aes_ccm_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_ccm_hw.c"
$(OUTDIR)\cipher_aes_gcm.obj: providers\implementations\ciphers\cipher_aes_gcm.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_gcm.c"
$(OUTDIR)\cipher_aes_gcm_hw.obj: providers\implementations\ciphers\cipher_aes_gcm_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_gcm_hw.c"
$(OUTDIR)\cipher_aes_hw.obj: providers\implementations\ciphers\cipher_aes_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_hw.c"
$(OUTDIR)\cipher_aes_ocb.obj: providers\implementations\ciphers\cipher_aes_ocb.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_ocb.c"
$(OUTDIR)\cipher_aes_ocb_hw.obj: providers\implementations\ciphers\cipher_aes_ocb_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_ocb_hw.c"
$(OUTDIR)\cipher_aes_siv.obj: providers\implementations\ciphers\cipher_aes_siv.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_siv.c"
$(OUTDIR)\cipher_aes_siv_hw.obj: providers\implementations\ciphers\cipher_aes_siv_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_siv_hw.c"
$(OUTDIR)\cipher_aes_wrp.obj: providers\implementations\ciphers\cipher_aes_wrp.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_wrp.c"
$(OUTDIR)\cipher_aes_xts.obj: providers\implementations\ciphers\cipher_aes_xts.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_xts.c"
$(OUTDIR)\cipher_aes_xts_fips.obj: providers\implementations\ciphers\cipher_aes_xts_fips.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_xts_fips.c"
$(OUTDIR)\cipher_aes_xts_hw.obj: providers\implementations\ciphers\cipher_aes_xts_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_aes_xts_hw.c"
$(OUTDIR)\cipher_chacha20.obj: providers\implementations\ciphers\cipher_chacha20.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_chacha20.c"
$(OUTDIR)\cipher_chacha20_hw.obj: providers\implementations\ciphers\cipher_chacha20_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_chacha20_hw.c"
$(OUTDIR)\cipher_chacha20_poly1305.obj: providers\implementations\ciphers\cipher_chacha20_poly1305.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_chacha20_poly1305.c"
$(OUTDIR)\cipher_chacha20_poly1305_hw.obj: providers\implementations\ciphers\cipher_chacha20_poly1305_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_chacha20_poly1305_hw.c"
$(OUTDIR)\cipher_cts.obj: providers\implementations\ciphers\cipher_cts.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_cts.c"
$(OUTDIR)\cipher_null.obj: providers\implementations\ciphers\cipher_null.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_null.c"
$(OUTDIR)\cipher_tdes.obj: providers\implementations\ciphers\cipher_tdes.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes.c"
$(OUTDIR)\cipher_tdes_common.obj: providers\implementations\ciphers\cipher_tdes_common.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_common.c"
$(OUTDIR)\cipher_tdes_default.obj: providers\implementations\ciphers\cipher_tdes_default.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_default.c"
$(OUTDIR)\cipher_tdes_default_hw.obj: providers\implementations\ciphers\cipher_tdes_default_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_default_hw.c"
$(OUTDIR)\cipher_tdes_hw.obj: providers\implementations\ciphers\cipher_tdes_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_hw.c"
$(OUTDIR)\cipher_tdes_wrap.obj: providers\implementations\ciphers\cipher_tdes_wrap.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_wrap.c"
$(OUTDIR)\cipher_tdes_wrap_hw.obj: providers\implementations\ciphers\cipher_tdes_wrap_hw.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\ciphers\cipher_tdes_wrap_hw.c"
$(OUTDIR)\blake2_prov.obj: providers\implementations\digests\blake2_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\blake2_prov.c"
$(OUTDIR)\blake2b_prov.obj: providers\implementations\digests\blake2b_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\blake2b_prov.c"
$(OUTDIR)\blake2s_prov.obj: providers\implementations\digests\blake2s_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\blake2s_prov.c"
$(OUTDIR)\md5_prov.obj: providers\implementations\digests\md5_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\md5_prov.c"
$(OUTDIR)\md5_sha1_prov.obj: providers\implementations\digests\md5_sha1_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\md5_sha1_prov.c"
$(OUTDIR)\null_prov.obj: providers\implementations\digests\null_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\null_prov.c"
$(OUTDIR)\ripemd_prov.obj: providers\implementations\digests\ripemd_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\ripemd_prov.c"
$(OUTDIR)\sha2_prov.obj: providers\implementations\digests\sha2_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\sha2_prov.c"
$(OUTDIR)\sha3_prov.obj: providers\implementations\digests\sha3_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\sha3_prov.c"
$(OUTDIR)\sm3_prov.obj: providers\implementations\digests\sm3_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\digests\sm3_prov.c"
$(OUTDIR)\decode_der2key.obj: providers\implementations\encode_decode\decode_der2key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_der2key.c"
$(OUTDIR)\decode_epki2pki.obj: providers\implementations\encode_decode\decode_epki2pki.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_epki2pki.c"
$(OUTDIR)\decode_msblob2key.obj: providers\implementations\encode_decode\decode_msblob2key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_msblob2key.c"
$(OUTDIR)\decode_pem2der.obj: providers\implementations\encode_decode\decode_pem2der.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_pem2der.c"
$(OUTDIR)\decode_pvk2key.obj: providers\implementations\encode_decode\decode_pvk2key.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_pvk2key.c"
$(OUTDIR)\decode_spki2typespki.obj: providers\implementations\encode_decode\decode_spki2typespki.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\decode_spki2typespki.c"
$(OUTDIR)\encode_key2any.obj: providers\implementations\encode_decode\encode_key2any.c providers\common\include\prov\der_rsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\encode_key2any.c"
$(OUTDIR)\encode_key2blob.obj: providers\implementations\encode_decode\encode_key2blob.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\encode_key2blob.c"
$(OUTDIR)\encode_key2ms.obj: providers\implementations\encode_decode\encode_key2ms.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\encode_key2ms.c"
$(OUTDIR)\encode_key2text.obj: providers\implementations\encode_decode\encode_key2text.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\encode_key2text.c"
$(OUTDIR)\endecoder_common.obj: providers\implementations\encode_decode\endecoder_common.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\encode_decode\endecoder_common.c"
$(OUTDIR)\dh_exch.obj: providers\implementations\exchange\dh_exch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\exchange\dh_exch.c"
$(OUTDIR)\ecdh_exch.obj: providers\implementations\exchange\ecdh_exch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\exchange\ecdh_exch.c"
$(OUTDIR)\ecx_exch.obj: providers\implementations\exchange\ecx_exch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\exchange\ecx_exch.c"
$(OUTDIR)\kdf_exch.obj: providers\implementations\exchange\kdf_exch.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\exchange\kdf_exch.c"
$(OUTDIR)\hkdf.obj: providers\implementations\kdfs\hkdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\hkdf.c"
$(OUTDIR)\kbkdf.obj: providers\implementations\kdfs\kbkdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\kbkdf.c"
$(OUTDIR)\krb5kdf.obj: providers\implementations\kdfs\krb5kdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\krb5kdf.c"
$(OUTDIR)\pbkdf2.obj: providers\implementations\kdfs\pbkdf2.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\pbkdf2.c"
$(OUTDIR)\pbkdf2_fips.obj: providers\implementations\kdfs\pbkdf2_fips.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\pbkdf2_fips.c"
$(OUTDIR)\pkcs12kdf.obj: providers\implementations\kdfs\pkcs12kdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\pkcs12kdf.c"
$(OUTDIR)\scrypt.obj: providers\implementations\kdfs\scrypt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\scrypt.c"
$(OUTDIR)\sshkdf.obj: providers\implementations\kdfs\sshkdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\sshkdf.c"
$(OUTDIR)\sskdf.obj: providers\implementations\kdfs\sskdf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\sskdf.c"
$(OUTDIR)\tls1_prf.obj: providers\implementations\kdfs\tls1_prf.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\tls1_prf.c"
$(OUTDIR)\x942kdf.obj: providers\implementations\kdfs\x942kdf.c providers\common\include\prov\der_wrap.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kdfs\x942kdf.c"
$(OUTDIR)\rsa_kem.obj: providers\implementations\kem\rsa_kem.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\kem\rsa_kem.c"
$(OUTDIR)\dh_kmgmt.obj: providers\implementations\keymgmt\dh_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\dh_kmgmt.c"
$(OUTDIR)\dsa_kmgmt.obj: providers\implementations\keymgmt\dsa_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\dsa_kmgmt.c"
$(OUTDIR)\ec_kmgmt.obj: providers\implementations\keymgmt\ec_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\ec_kmgmt.c"
$(OUTDIR)\ecx_kmgmt.obj: providers\implementations\keymgmt\ecx_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\ecx_kmgmt.c"
$(OUTDIR)\kdf_legacy_kmgmt.obj: providers\implementations\keymgmt\kdf_legacy_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\kdf_legacy_kmgmt.c"
$(OUTDIR)\mac_legacy_kmgmt.obj: providers\implementations\keymgmt\mac_legacy_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\mac_legacy_kmgmt.c"
$(OUTDIR)\rsa_kmgmt.obj: providers\implementations\keymgmt\rsa_kmgmt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\keymgmt\rsa_kmgmt.c"
$(OUTDIR)\blake2b_mac.obj: providers\implementations\macs\blake2b_mac.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\blake2b_mac.c"
$(OUTDIR)\blake2s_mac.obj: providers\implementations\macs\blake2s_mac.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\blake2s_mac.c"
$(OUTDIR)\cmac_prov.obj: providers\implementations\macs\cmac_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\cmac_prov.c"
$(OUTDIR)\gmac_prov.obj: providers\implementations\macs\gmac_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\gmac_prov.c"
$(OUTDIR)\hmac_prov.obj: providers\implementations\macs\hmac_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\hmac_prov.c"
$(OUTDIR)\kmac_prov.obj: providers\implementations\macs\kmac_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\kmac_prov.c"
$(OUTDIR)\poly1305_prov.obj: providers\implementations\macs\poly1305_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\poly1305_prov.c"
$(OUTDIR)\siphash_prov.obj: providers\implementations\macs\siphash_prov.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\macs\siphash_prov.c"
$(OUTDIR)\crngt.obj: providers\implementations\rands\crngt.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\crngt.c"
$(OUTDIR)\drbg.obj: providers\implementations\rands\drbg.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\drbg.c"
$(OUTDIR)\drbg_ctr.obj: providers\implementations\rands\drbg_ctr.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\drbg_ctr.c"
$(OUTDIR)\drbg_hash.obj: providers\implementations\rands\drbg_hash.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\drbg_hash.c"
$(OUTDIR)\drbg_hmac.obj: providers\implementations\rands\drbg_hmac.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\drbg_hmac.c"
$(OUTDIR)\seed_src.obj: providers\implementations\rands\seed_src.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\seed_src.c"
$(OUTDIR)\test_rng.obj: providers\implementations\rands\test_rng.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\test_rng.c"
$(OUTDIR)\rand_cpu_x86.obj: providers\implementations\rands\seeding\rand_cpu_x86.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\seeding\rand_cpu_x86.c"
$(OUTDIR)\rand_tsc.obj: providers\implementations\rands\seeding\rand_tsc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\seeding\rand_tsc.c"
$(OUTDIR)\rand_unix.obj: providers\implementations\rands\seeding\rand_unix.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\seeding\rand_unix.c"
$(OUTDIR)\rand_win.obj: providers\implementations\rands\seeding\rand_win.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\rands\seeding\rand_win.c"
$(OUTDIR)\dsa_sig.obj: providers\implementations\signature\dsa_sig.c providers\common\include\prov\der_dsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\signature\dsa_sig.c"
$(OUTDIR)\ecdsa_sig.obj: providers\implementations\signature\ecdsa_sig.c providers\common\include\prov\der_ec.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\signature\ecdsa_sig.c"
$(OUTDIR)\eddsa_sig.obj: providers\implementations\signature\eddsa_sig.c providers\common\include\prov\der_ecx.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\signature\eddsa_sig.c"
$(OUTDIR)\mac_legacy_sig.obj: providers\implementations\signature\mac_legacy_sig.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\signature\mac_legacy_sig.c"
$(OUTDIR)\rsa_sig.obj: providers\implementations\signature\rsa_sig.c providers\common\include\prov\der_rsa.h
	$(CC)  $(LIB_CFLAGS) -I"providers\common\include\prov" -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\signature\rsa_sig.c"
$(OUTDIR)\file_store.obj: providers\implementations\storemgmt\file_store.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\storemgmt\file_store.c"
$(OUTDIR)\file_store_any2obj.obj: providers\implementations\storemgmt\file_store_any2obj.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "providers\implementations\storemgmt\file_store_any2obj.c"
$(OUTDIR)\s3_cbc.obj: ssl\s3_cbc.c
	$(CC)  $(LIB_CFLAGS) -I"." -I"crypto" -I"include" -I"providers\implementations\include" -I"providers\common\include" $(LIB_CPPFLAGS)  -DAES_ASM -DBSAES_ASM -DECP_NISTZ256_ASM -DKECCAK1600_ASM -DOPENSSL_CPUID_OBJ -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DVPAES_ASM -DX25519_ASM -c $(COUTFLAG)$@ "ssl\s3_cbc.c"
depend:

$(OUTDIR):
	-mkdir ..\vc16
	-mkdir ..\vc16\x64
	-mkdir ..\vc16\x64\$(CONFIG)
	-mkdir $(OUTDIR)

libclean:
	-del /Q /F $(LIBS) $(OUTDIR)\ossl_static.pdb $(OUTDIR)\*.obj

clean: libclean


