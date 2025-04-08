#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-only

AUTOMAKE_SKIPPED=77
AUTOMAKE_HARD_ERROR=99

START_DIR=$PWD
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	cp "$SCRIPT_DIR"/*.py "$START_DIR"
fi

PYTHON_LIBRARY_PATH=(../../src/python/build/lib*)
if [ -d  "${PYTHON_LIBRARY_PATH[0]}" ]; then
	pushd "${PYTHON_LIBRARY_PATH[0]}" || exit $AUTOMAKE_HARD_ERROR
	PYTHONPATH="$PYTHONPATH:$(pwd)"
	export PYTHONPATH
	popd || exit $AUTOMAKE_HARD_ERROR
fi

./ftests.py -l 10 -L "$START_DIR/ftests-container.py.log" -n Libcg"$RANDOM"
RET=$?

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests-container.py.log
fi

if [[ $RET -ne $AUTOMAKE_SKIPPED ]] && [[ $RET -ne 0 ]]; then
	# always return errors from the first test run
	exit $RET
fi

if [[ $RET -eq 0 ]]; then
	exit 0
fi

if [[ $RET -eq $AUTOMAKE_SKIPPED ]]; then
	exit $AUTOMAKE_SKIPPED
fi

# I don't think we should ever get here, but better safe than sorry
exit $AUTOMAKE_HARD_ERROR
