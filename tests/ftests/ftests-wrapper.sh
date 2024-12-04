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

./ftests.py -l 10 -L "$START_DIR/ftests.py.log" -n Libcg"$RANDOM"
RET1=$?

./ftests.py -l 10 -L "$START_DIR/ftests-nocontainer.py.log" --no-container \
	-n Libcg"$RANDOM"
RET2=$?

if [ -z "$srcdir" ]; then
	# $srcdir is set by automake but will likely be empty when run by hand and
	# that's fine
	srcdir=""
else
	srcdir=$srcdir"/"
fi

sudo cp "$srcdir../../src/libcgroup_systemd_idle_thread" /bin
sudo PYTHONPATH="$PYTHONPATH" ./ftests.py -l 10 -s "sudo" \
	-L "$START_DIR/ftests-nocontainer.py.sudo.log" --no-container -n Libcg"$RANDOM"
RET3=$?
sudo rm /bin/libcgroup_systemd_idle_thread

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests.py.log
	rm -f ftests-nocontainer.py.log
	rm -f ftests-nocontainer.py.sudo.log
fi

if [[ $RET1 -ne $AUTOMAKE_SKIPPED ]] && [[ $RET1 -ne 0 ]]; then
	# always return errors from the first test run
	exit $RET1
fi
if [[ $RET2 -ne $AUTOMAKE_SKIPPED ]] && [[ $RET2 -ne 0 ]]; then
	# return errors from the second test run
	exit $RET2
fi
if [[ $RET3 -ne $AUTOMAKE_SKIPPED ]] && [[ $RET3 -ne 0 ]]; then
	# return errors from the third test run
	exit $RET3
fi

if [[ $RET1 -eq 0 ]] || [[ $RET2 -eq 0 ]] || [[ $RET3 -eq 0 ]]; then
	exit 0
fi

if [[ $RET1 -eq $AUTOMAKE_SKIPPED ]] || [[ $RET2 -eq $AUTOMAKE_SKIPPED ]] ||
   [[ $RET3 -eq $AUTOMAKE_SKIPPED ]]; then
	exit $AUTOMAKE_SKIPPED
fi

# I don't think we should ever get here, but better safe than sorry
exit $AUTOMAKE_HARD_ERROR
