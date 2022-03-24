#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-only

START_DIR=$PWD
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	cp "$SCRIPT_DIR"/*.py "$START_DIR"
fi

if [ -d ../../src/python/build/lib.* ]; then
	pushd ../../src/python/build/lib.*
	export PYTHONPATH="$PYTHONPATH:$(pwd)"
	popd
fi

./ftests.py -l 10 -L "$START_DIR/ftests.py.log" -n Libcg"$RANDOM"
RET=$?

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests.py.log
fi

exit $RET
