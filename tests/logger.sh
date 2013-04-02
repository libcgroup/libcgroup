#!/bin/bash
# Test various log levels

export LOGFILE=
export RET=0
unset CGROUP_LOGLEVEL

function run_logger()
{
	LOGFILE=`mktemp`
	echo "Running CGROUP_LOGLEVEL=$CGROUP_LOGLEVEL logger $* >$LOGFILE"
	./logger $* >$LOGFILE
}

function assert_grep()
{
	if ! grep "$@" <$LOGFILE >/dev/null; then
		echo "Error: expecting $* in output"
		RET=1
	fi
}

function assert_not_grep()
{
	if grep "$@" <$LOGFILE >/dev/null; then
		echo "Error: unexptected $* in output"
		RET=1
	fi
}

# CGROUP_LOGLEVEL is case-insensitive
CGROUP_LOGLEVEL=DeBuG run_logger -1
assert_grep "^DEBUG message"
assert_grep "^INFO message"
assert_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# missing CGROUP_LOGLEVEL leads to ERRORs only
run_logger -1
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_not_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# wrong CGROUP_LOGLEVEL leads to ERRORs only
CGROUP_LOGLEVEL=xyz run_logger -1
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_not_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# numeric CGROUP_LOGLEVEL
CGROUP_LOGLEVEL=3 run_logger -1
assert_not_grep "^DEBUG message"
assert_grep "^INFO message"
assert_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# errors only CGROUP_LOGLEVEL
CGROUP_LOGLEVEL=ERROR run_logger -1
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_not_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# no CGROUP_LOGLEVEL -> DEBUG
run_logger 4
assert_grep "^DEBUG message"
assert_grep "^INFO message"
assert_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# no CGROUP_LOGLEVEL -> INFO
run_logger 3
assert_not_grep "^DEBUG message"
assert_grep "^INFO message"
assert_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# no CGROUP_LOGLEVEL -> WARN
run_logger 2
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# no CGROUP_LOGLEVEL -> ERROR
run_logger 1
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_not_grep "^WARNING message"
assert_grep "^ERROR message"
rm $LOGFILE

# no CGROUP_LOGLEVEL -> nothing
run_logger 0
assert_not_grep "^DEBUG message"
assert_not_grep "^INFO message"
assert_not_grep "^WARNING message"
assert_not_grep "^ERROR message"
rm $LOGFILE


# custom logger -> DEBUG
run_logger custom 4
assert_grep "^custom: DEBUG message"
assert_grep "^custom: INFO message"
assert_grep "^custom: WARNING message"
assert_grep "^custom: ERROR message"
rm $LOGFILE

# custom logger -> INFO
run_logger custom 3
assert_not_grep "^custom: DEBUG message"
assert_grep "^custom: INFO message"
assert_grep "^custom: WARNING message"
assert_grep "^custom: ERROR message"
rm $LOGFILE

# custom logger -> WARN
run_logger custom 2
assert_not_grep "^custom: DEBUG message"
assert_not_grep "^custom: INFO message"
assert_grep "^custom: WARNING message"
assert_grep "^custom: ERROR message"
rm $LOGFILE

exit $RET

