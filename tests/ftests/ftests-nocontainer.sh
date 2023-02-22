#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-only

AUTOMAKE_SKIPPED=77
AUTOMAKE_HARD_ERROR=99

START_DIR=$PWD
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
RUNNER_LOCK_FILE="/var/run/libcgroup/github-runner.lock"

# the lock file is removed after executing no-container
# test cases, whereas the lock is taken by the ftests.sh
function cleanup()
{
	sudo rm -f "$RUNNER_LOCK_FILE"
	exit "$1"
}

# ftests.sh should have taken the lock, if not
# something is out of order, let's abort
if [ ! -f "$RUNNER_LOCK_FILE" ]; then
	echo "Lock file $RUNNER_LOCK_FILE missing, aborting"
	exit 1
fi

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	cp "$SCRIPT_DIR"/*.py "$START_DIR"
fi

if [ -d ../../src/python/build/lib.* ]; then
	pushd ../../src/python/build/lib.*
	export PYTHONPATH="$PYTHONPATH:$(pwd)"
	popd
fi

./ftests.py -l 10 -L "$START_DIR/ftests-nocontainer.py.log" --no-container \
	-n Libcg"$RANDOM"
RET1=$?

pushd ../../src || cleanup $AUTOMAKE_HARD_ERROR
PATH="$PATH:$(pwd)"
export PATH
popd || cleanp $AUTOMAKE_HARD_ERROR

sudo PATH=$PATH PYTHONPATH=$PYTHONPATH ./ftests.py -l 10 -s "sudo" \
	-L "$START_DIR/ftests-nocontainer.py.sudo.log" --no-container -n Libcg"$RANDOM"
RET2=$?

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests-nocontainer.py.log
	rm -f ftests-nocontainer.py.sudo.log
fi


if [[ $RET1 -ne $AUTOMAKE_SKIPPED ]] && [[ $RET1 -ne 0 ]]; then
	# always return errors from the first test run
	cleanup $RET1
fi
if [[ $RET2 -ne $AUTOMAKE_SKIPPED ]] && [[ $RET2 -ne 0 ]]; then
	# return errors from the second test run
	cleanup $RET2
fi

if [[ $RET1 -eq 0 ]] || [[ $RET2 -eq 0 ]]; then
	cleanup 0
fi

if [[ $RET1 -eq $AUTOMAKE_SKIPPED ]] || [[ $RET2 -eq $AUTOMAKE_SKIPPED ]]; then
	cleanup $AUTOMAKE_SKIPPED
fi

# I don't think we should ever get here, but better safe than sorry
cleanup $AUTOMAKE_HARD_ERROR
