AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/include/unavi @jansson_CFLAGS@ -Wno-deprecated -Wno-invalid-offsetof

noinst_PROGRAMS = test_single_cycle test_scycle_read test_list test_wspec_cycle test_rwspec_cycle\
				  test_mworker test_mworker_aes test_ujom test_ujom_ser test_ssid_codec

test_single_cycle_SOURCES = test_single_cycle.cpp
test_single_cycle_LDADD = ../src/.libs/libunavi.la
test_single_cycle_CPPFLAGS = $(AM_CPPFLAGS)

test_scycle_read_SOURCES = test_scycle_read.cpp
test_scycle_read_LDADD = ../src/.libs/libunavi.la
test_scycle_read_CPPFLAGS = $(AM_CPPFLAGS)

test_list_SOURCES = test_list.cpp
test_list_LDADD = ../src/.libs/libunavi.la
test_list_CPPFLAGS = $(AM_CPPFLAGS)

test_wspec_cycle_SOURCES = test_wspec_cycle.cpp
test_wspec_cycle_LDADD = ../src/.libs/libunavi.la
test_wspec_cycle_CPPFLAGS = $(AM_CPPFLAGS)

test_rwspec_cycle_SOURCES = test_rwspec_cycle.cpp
test_rwspec_cycle_LDADD = ../src/.libs/libunavi.la
test_rwspec_cycle_CPPFLAGS = $(AM_CPPFLAGS)

test_mworker_SOURCES = test_mworker.cpp
test_mworker_LDADD = ../src/.libs/libunavi.la
test_mworker_CPPFLAGS = $(AM_CPPFLAGS)

test_mworker_aes_SOURCES = test_mworker_aes.cpp
test_mworker_aes_LDADD = ../src/.libs/libunavi.la -lssl
test_mworker_aes_CPPFLAGS = $(AM_CPPFLAGS)

test_ujom_SOURCES = test_ujom.cpp
test_ujom_LDADD = ../src/.libs/libunavi.la -lssl
test_ujom_CPPFLAGS = $(AM_CPPFLAGS)

test_ujom_ser_SOURCES = test_ujom_ser.cpp
test_ujom_ser_LDADD = ../src/.libs/libunavi.la -lssl
test_ujom_ser_CPPFLAGS = $(AM_CPPFLAGS)

test_ssid_codec_SOURCES = test_ssid_codec.cpp
test_ssid_codec_LDADD = ../src/.libs/libunavi.la -lssl
test_ssid_codec_CPPFLAGS = $(AM_CPPFLAGS)

