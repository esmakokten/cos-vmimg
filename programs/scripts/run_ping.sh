#!/bin/sh
set -eu
echo "cos-vmimg: running ping"
/programs/ping
echo "cos-vmimg: ping finished; idling."
exec /bin/sh
