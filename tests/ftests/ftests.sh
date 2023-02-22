#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-only

START_DIR=$PWD
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# synchronize between different github runners running on
# same VM's, this will stop runners from stomping over
# each other's run.
LIBCGROUP_RUN_DIR="/var/run/libcgroup/"
RUNNER_LOCK_FILE="/var/run/libcgroup/github-runner.lock"
RUNNER_SLEEP_SECS=300		# sleep for 5 minutes
RUNNER_MAX_TRIES=10		# Abort after 50 minutes, if we don't chance to run

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	cp "$SCRIPT_DIR"/*.py "$START_DIR"
fi

if [ -d ../../src/python/build/lib.* ]; then
	pushd ../../src/python/build/lib.*
	export PYTHONPATH="$PYTHONPATH:$(pwd)"
	popd
fi

# If other runners are running then the file exists
# let's wait for 5 minutes
time_waited=0
pretty_time=0
while [ -f "$RUNNER_LOCK_FILE" ]; do
	if [ "$RUNNER_MAX_TRIES" -le 0 ]; then
		echo "Unable to get lock to run the ftests, aborting"
		exit 1
	fi

	RUNNER_MAX_TRIES=$(( RUNNER_MAX_TRIES - 1 ))
	sleep "$RUNNER_SLEEP_SECS"

	time_waited=$(( time_waited + RUNNER_SLEEP_SECS ))
	pretty_time=$(echo $time_waited | awk '{printf "%d:%02d:%02d", $1/3600, ($1/60)%60, $1%60}')
	echo "[$pretty_time] Waiting on other runners to complete, $RUNNER_MAX_TRIES retries left"
done
# take the lock and start executing
sudo mkdir -p "$LIBCGROUP_RUN_DIR"
sudo touch "$RUNNER_LOCK_FILE"

./ftests.py -l 10 -L "$START_DIR/ftests.py.log" -n Libcg"$RANDOM"
RET=$?

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests.py.log
fi

exit $RET
