#
# Makefile.am for core/agent-dummy/testsuite
#

AM_CXXFLAGS = -DY2LOG=\"agent-dummy\"

AUTOMAKE_OPTIONS = dejagnu

INCLUDES =		\
	${Y2UTIL_CFLAGS}

noinst_PROGRAMS = y2ag_dummy

y2ag_dummy_SOURCES = y2ag_dummy.cc
y2ag_dummy_LDADD = ${AGENT_LIBADD}
y2ag_dummy_LDFLAGS =				\
	-Xlinker --whole-archive		\
	../src/libpy2ag_dummy.la		\
	-Xlinker --no-whole-archive
