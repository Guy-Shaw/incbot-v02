# Filename: src/Makefile
# Project: incbot
# Brief: Top-lovel Makefile for incbot -- build libraries and commands
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

DIST_BASE := /home/shaw/v/psdk/dist
DIST_EXE  := $(DIST_BASE)/share/bin
DIST_LIB  := $(DIST_BASE)/share/lib/incbot

.PHONY: all test clean show-targets

all:
	cd libincbot && make
	cd libcf && make
	cd libcscript && make
	cd cmd && make

test:
	@cd cmd && make test | grep -v -E ': (Entering|Leaving) directory'

clean:
	cd libincbot && make clean
	cd libcf && make clean
	cd libcscript && make clean
	cd cmd && make clean

diff-table:
	diff -u $(DIST_LIB)/id-table table/id-table

install-table:
	test -d $(DIST_BASE)
	test -d $(DIST_BASE)/share
	test -d $(DIST_BASE)/share/lib/incbot || mkdir -p $(DIST_BASE)/share/lib/incbot
	cp -p table/id-table $(DIST_LIB)/id-table

show-targets:
	@show-makefile-targets

show-%:
	@echo $*=$($*)
