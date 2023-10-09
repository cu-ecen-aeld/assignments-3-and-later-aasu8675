#!/bin/sh
# Author: Aamir Suhail Burhan

case "$1" in
    start)
        echo "Starting daemon"
        start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping daemon"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $ ./aesdsocket-start-stop.sh start/stop"
        exit 1
esac

exit 0
