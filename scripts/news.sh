#!/bin/bash
# scripts/news.sh <from> <to> <version>: the owners' notes for a kernel build, the "News:" lines of the commits after <from> up to <to> (all of them, if <from> is empty), each as "<version>, a tab, the note", newest first. A kernel's version comes from upstream and a revision, not from the commits, so build.yml keys each build's notes to the build (news/linux-image-<x.y.z>-openframe.news beside the pool), from the commit the released kernel was built from (news/kernel.commits) to this one's. The lines are read as openframe-linux's ci/news.sh reads them.
set -eu
from=$1 to=$2 ver=$3
range=$to; [ -z "$from" ] || range="$from..$to"
git log --format=%H "$range" | while read -r hash; do
	git log -1 --format=%B "$hash" | awk '
		function out() { if (n != "") print n; n = "" }
		/^News:/ { out(); n = $0; sub(/^News:[ \t]*/, "", n); next }
		n != "" && (/^[ \t]*$/ || /^(Co-Authored-By|Claude-Session|Signed-off-by):/) { out(); next }
		n != "" { l = $0; sub(/^[ \t]+/, "", l); n = n " " l }
		END { out() }' | while IFS= read -r note; do printf '%s\t%s\n' "$ver" "$note"; done
done
