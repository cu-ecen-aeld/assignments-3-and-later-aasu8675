#!/bin/sh
# Author: Aamir Suhail Burhan

if [ $# -ne 1 ]; then
  echo "Please provide exactly 1 argument."
  exit 1
fi

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
        echo "Usage: $0 start/stop"
        echo "Invalid argument: $1"
        exit 1
esac

exit 0
