#!/bin/sh

case "$1" in
	start)
		echo "Starting aesdsocket daemon"
		start-stop-daemon -S -n aesdsocket -a /home/hardik/Desktop/Assignment-1/assignment-1-HardikMinochaESE-1/server/aesdsocket -- -d
		;;
	stop)
		echo "Stopping aesdsocket daemon"
		start-stop-daemon -K -n aesdsocket
		;;
	*)
		echo "Usage $0 {start|stop}"
	exit 1
esac

exit 0
