# Filename: src/cmd/Makefile
# Project: incbot
# Brief: `incbot` main program / executable
#
# Copyright (C) 2016 Guy Shaw
# Written by Guy Shaw <gshaw@acm.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

PROGRAM := incbot
SRCS = $(PROGRAM).c
OBJS = $(PROGRAM).o
LIBS := ../libincbot/libincbot.a  ../libcf/libcf.a  ../libcscript/libcscript.a

CC := gcc
CONFIG :=
CFLAGS := -g -Wall -Wextra
CPPFLAGS := -I../inc

.PHONY: all test vtest splint clean-test clean

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(CONFIG) $(OBJS) $(LIBS)

test: $(PROGRAM)
	@cd test && make test

vtest: $(PROGRAM)
	@cd test && make vtest

incbot-iwyu:
	iwyu -Xiwyu --verbose=3 $(CPPFLAGS) $(SRCS) > incbot-iwyu 2>&1 ; \
	    test -s incbot-iwyu
	fix-iwyu incbot-iwyu > incbot-iwyu-fixed

splint:
	splint +gnuextensions $(CPPFLAGS) $(SRCS)

clean-test:
	cd test && make clean
	rm -f incbot-iwyu incbot-iwyu-fixed

clean: clean-test
	rm -f $(PROGRAM) core a.out mmv *.o *.a
	rm -f test_?? T.??
	rm -f *,FAILED

show-targets:
	@show-makefile-targets

show-%:
	@echo $*=$($*)
