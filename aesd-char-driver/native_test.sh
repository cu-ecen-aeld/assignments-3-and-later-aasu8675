# Author: Aamir Suhail Burhan
# Description: Shell script to test device driver on native build

#!/bin/sh
echo "Starting native unload, load and driver test script"
make clean

./aesdchar_unload

make

./aesdchar_load 

../assignment-autotest/test/assignment9/drivertest.sh
echo "End of native unload, load and driver test script run"
