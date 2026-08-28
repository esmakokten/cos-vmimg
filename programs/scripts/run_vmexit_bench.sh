#!/bin/sh
set -eu

echo "cos-vmimg: loading modules"
for mod in /modules/*.ko; do
	echo "  insmod $mod"
	insmod "$mod"
done

# The modules register misc/char devices; devtmpfs creates the nodes for us.
# If a node is missing that is a real failure, not something to paper over.
for dev in /dev/kvm-microbench /dev/kvm-fake; do
	if [ ! -c "$dev" ]; then
		echo "FATAL: $dev not created. Loaded modules:"
		cat /proc/devices
		exit 1
	fi
done

echo "cos-vmimg: running vmexit/vmresume microbenchmark"
/programs/vmexit-vmresume-microbench

echo "cos-vmimg: benchmark finished; idling."
exec /bin/sh
