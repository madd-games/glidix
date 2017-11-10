#! /bin/sh
# tests.sh
# Run unit tests to check the validity of the code.
export TEST_CFLAGS="-D__UNIT_TEST__"
srcdir="`dirname $0`"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[1;34m'
NC='\033[0m' # No Color

echo "*** TESTS START ***" > tests.log
echo "Source directory:    $srcdir" >> tests.log
echo "Date of test:        `date`" >> tests.log

for testdir in $srcdir/tests/*
do
	echo "*** CATEGORY `basename $testdir`" >> tests.log
	echo "${BLUE}TESTING CATEGORY '`basename $testdir`'...${NC}"
	
	for testpath in $testdir/*.sh
	do
		printf "${BLUE}TEST${NC} '`basename $testpath`'... "
		sh $testpath >>tests.log 2>&1 || {
			echo "${RED}FAILED${NC}"
			echo "${RED}UNIT TESTS FAILED!${NC}"
			echo "${RED}See tests.log for more info.${NC}"
			exit
		}
		
		echo "${GREEN}OK${NC}"
	done
done

# Keep this at the very end!
echo "${GREEN}ALL UNIT TESTS PASSED.${NC}"
