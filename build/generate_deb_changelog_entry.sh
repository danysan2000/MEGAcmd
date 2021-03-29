#!/bin/bash

##
 # @file src/build/generate_changelog_entry.sh
 # @brief Processes the input file and prints DEB ChangeLog entry
 #
 # (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 #
 # This file is part of the MEGAcmd.
 #
 # MEGAcmd is distributed in the hope that it will be useful,
 # but WITHOUT ANY WARRANTY; without even the implied warranty of
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 #
 # @copyright Simplified (2-clause) BSD License.
 #
 # You should have received a copy of the license along with this
 # program.
##

if [ "$#" -ne 2 ]; then
    echo " $0 [version] [input file path]"
    exit 1
fi

in_file="$2"

out1=$(printf "#include <iostream>
using namespace std;
#include \"$2\"
int main() {
cout << megacmd::megacmdchangelog << endl;
}" | g++ -x c++ - -o /tmp/printChangeLogMcmd && /tmp/printChangeLogMcmd | awk '{print "  * "$0}' && rm /tmp/printChangeLogMcmd )

# print ChangeLog entry
NOW=$(LANG=C date -R)
echo "megacmd ($1) stable; urgency=low"
echo ""
echo "$out1" | sed 's#\\"#"#g'
echo ""
echo " -- MEGA Team <linux@mega.co.nz>  $NOW"
echo ""
