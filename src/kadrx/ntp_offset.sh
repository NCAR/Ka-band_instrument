#!/bin/sh
#
# Print the current NTP offset reported via "ntpq -p", in ms.
# If there is a problem using ntpq or missing/bad NTP synchronization,
# the script prints nothing and exits with status 1. Otherwise, the
# real offset in ms is printed and the script exits with status 0.
#
ntpq="/usr/sbin/ntpq"

# Get the status of the currently selected peer via 'ntpq -p'. The line for
# the currently selected peer has a '*' as the first character.
peerline=`$ntpq -p 2>/dev/null | egrep '^\*'`
if [[ -z $peerline ]]; then
  # There's no currently selected peer, which can happen if ntpd has started
  # but hasn't run long enough to select a peer, so take the last peer, if any.
  peerline=`$ntpq -p 2>/dev/null | tail -1`
  
  # If we still found no peer, exit with status 1.
  if [[ -z $peerline ]]; then
    echo "Failed to get peer information using '$ntpq -p'"
    exit 1
  fi
fi

# Parse the peer line
  
# Strip the first character from the line, since it may be a space (which
# can otherwise shift fields after the whitespace substitution below...)
peerline=`echo $peerline | sed -r 's/^.//g'`

# Compress groups of one or more spaces into single spaces
peerline=`echo $peerline | sed -r 's/ +/ /g'`

# The reach is in field 7 of the line. If reach is zero, it's been a long
# time since we've seen the time server, so consider it a problem...
reach=`echo $peerline | cut -d' ' -f7`
if [[ $reach == 0 ]]; then
  # The peer name/IP address is in field 1
  peername=`echo $peerline | cut -d' ' -f1`
  echo "NTP peer '$peername' has not been reached in last 8 tries."
  exit 1
fi

# The time offset in ms is field 9 of the line. Print it.
offset=`echo $peerline | cut -d' ' -f9`
echo $offset
