#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a .
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K --signal TERM --oknodo -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac

exit 0