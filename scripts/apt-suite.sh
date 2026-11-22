#!/bin/bash
# apt-suite.sh: kernel.openbeak.net's two suites, for build.yml (publish) and promote.yml. Run
# from the repository's root with the repo branch in apt-repo/ and the signing key imported.
#
#   apt-suite.sh add <component> <deb>...   kernel packages into the testing suite
#   apt-suite.sh promote <component>        the testing suite's kernel into trixie (released)
#   apt-suite.sh waiting <component>        what's in testing: package and version, a line each
#   apt-suite.sh listing                    the repository's index.html
#
# trixie is what every OpenFrame takes; trixie-testing what test units take as well
# (OF-BOOT/settings/test.txt). A component is a kernel series (6.18), each with its own pool:
# pool/6.18 for trixie, pool/testing-6.18 for trixie-testing. Each holds one kernel: the
# latest metapackage, image and headers (older ones are in the GitHub releases).
set -eu
REPO=apt-repo

# Keeps only the latest of each kind of kernel package in a pool, as apt would treat old
# metapackage versions as downgrade candidates (and stop autoremove cleaning old kernels).
prune() {
	local pool=$1
	ls "$pool/linux-image-openframe-"*.deb 2>/dev/null | sort -rV | tail -n +2 | xargs -r rm -v
	ls "$pool/linux-image-"[0-9]*.deb 2>/dev/null | sort -rV | tail -n +2 | xargs -r rm -v
	ls "$pool/linux-headers"*.deb 2>/dev/null | sort -rV | tail -n +2 | xargs -r rm -v
	rm -fv "$pool/linux-libc-dev"*.deb
}

# Indexes suite $1 (trixie or trixie-testing): every component it has a pool for.
index() {
	local suite=$1 prefix="" label="OpenFrame Kernel" desc="OpenFrame Kernel Packages" comps="" c
	[ "$suite" = trixie-testing ] && { prefix="testing-"; label="OpenFrame Kernel testing"; desc="OpenFrame Kernel Packages before release, for test units"; }
	(
		cd "$REPO"
		for pool in pool/${prefix}[0-9]*; do
			[ -d "$pool" ] || continue
			c=${pool#pool/$prefix}
			mkdir -p "dists/$suite/$c/binary-i386"
			dpkg-scanpackages --arch i386 "$pool" > "dists/$suite/$c/binary-i386/Packages" 2>/dev/null
			gzip -9c "dists/$suite/$c/binary-i386/Packages" > "dists/$suite/$c/binary-i386/Packages.gz"
			comps="$comps $c"
		done
		[ -n "$comps" ] || return 0
		apt-ftparchive \
			-o APT::FTPArchive::Release::Origin="OpenFrame Kernel" \
			-o APT::FTPArchive::Release::Label="$label" \
			-o APT::FTPArchive::Release::Suite="$suite" \
			-o APT::FTPArchive::Release::Codename="$suite" \
			-o APT::FTPArchive::Release::Architectures="i386" \
			-o "APT::FTPArchive::Release::Components=${comps# }" \
			-o APT::FTPArchive::Release::Description="$desc" \
			release "dists/$suite" > "dists/$suite/Release.new"
		mv -f "dists/$suite/Release.new" "dists/$suite/Release"
		key=$(gpg --list-keys --with-colons | awk -F: '/^pub/ { print $5; exit }')
		gpg --batch --yes --default-key "$key" --clearsign -o "dists/$suite/InRelease" "dists/$suite/Release"
		gpg --batch --yes --default-key "$key" --detach-sign --armor -o "dists/$suite/Release.gpg" "dists/$suite/Release"
	)
}

case "${1:-}" in
add)
	c=$2; shift 2
	mkdir -p "$REPO/pool/testing-$c"
	cp "$@" "$REPO/pool/testing-$c/"
	prune "$REPO/pool/testing-$c"
	gpg --export > "$REPO/key.gpg"
	index trixie-testing
	;;
promote)
	c=$2
	shopt -s nullglob
	debs=("$REPO/pool/testing-$c"/*.deb)
	[ ${#debs[@]} -gt 0 ] || { echo "nothing in testing for $c"; exit 0; }
	mkdir -p "$REPO/pool/$c"
	for d in "${debs[@]}"; do echo "promoted ${d##*/}"; mv "$d" "$REPO/pool/$c/"; done
	prune "$REPO/pool/$c"
	index trixie
	index trixie-testing
	;;
waiting)
	shopt -s nullglob
	for d in "$REPO/pool/testing-$2"/*.deb; do printf '%s %s\n' "$(dpkg-deb -f "$d" Package)" "$(dpkg-deb -f "$d" Version)"; done
	;;
listing)
	human() {
		if [ "$1" -ge 1048576 ]; then echo "$(($1 / 1048576))M"
		elif [ "$1" -ge 1024 ]; then echo "$(($1 / 1024))K"
		else echo "$1"; fi
	}
	cd "$REPO"
	{
		printf '<!DOCTYPE html>\n<html>\n<head><title>Index of /</title></head>\n<body>\n<h1>Index of /</h1><hr><pre>'
		for f in pool/*/*.deb; do
			[ -f "$f" ] || continue
			name=${f##*/}
			pad=$((51 - ${#name})); [ "$pad" -lt 1 ] && pad=1
			printf '\n<a href="%s">%s</a>%*s%s  %8s' "$f" "$name" "$pad" '' \
				"$(date -d "@$(stat -c %Y "$f")" '+%d-%b-%Y %H:%M')" "$(human "$(stat -c %s "$f")")"
		done
		printf '\n</pre><hr></body>\n</html>\n'
	} > index.html
	;;
*)
	sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
	;;
esac
