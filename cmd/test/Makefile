# Filename: src/cmd/test/Makefile
# Project: incbot
# Brief: Unit test of command, 'incbot'.
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

.PHONY: test vtest clean show-targets

test:
	if [ ! -e tmp ]; then  mkdir tmp ; fi
	../incbot < ../incbot.c > tmp/incbot.out 2>tmp/incbot.err; echo $$?
	ls -lh tmp/incbot.*
	tail tmp/incbot.err
	tail tmp/incbot.out
	@which iwyu > /dev/null 2>&1 || exit 0 ; ( cd .. && make incbot-iwyu )

vtest:
	if [ ! -e tmp ]; then  mkdir tmp ; fi
	valgrind ../incbot < ../incbot.c > tmp/incbot.out 2>tmp/incbot.err; echo $$?

clean:
	rm -rf core vgcore.* tmp tmp-*

show-targets:
	@show-makefile-targets

show-%:
	@echo $*=$($*)
