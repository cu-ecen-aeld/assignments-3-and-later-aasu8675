# Author: Aamir Suhail Burhan
# Description: Shell script to test device driver on native build

#!/bin/sh
echo "Starting native unload, load and driver test script"
./module_unload

make
./module_load 

../assignment-autotest/test/assignment8/drivertest.sh
echo "End of native unload, load and driver test script run"
