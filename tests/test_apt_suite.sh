#!/bin/bash
# test_apt_suite.sh: scripts/apt-suite.sh, kernel.openbeak.net's testing and released suites:
# a new kernel goes to testing only (a test unit sees it, a released unit doesn't), promote
# moves it to the released suite keeping one kernel there, and testing is left empty. With a
# throwaway key, in a Debian Trixie container (Docker) unless it's root in one already:
#   bash tests/test_apt_suite.sh
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
if [ "$(id -u)" -ne 0 ] || [ ! -e /.dockerenv ]; then
	# i386, as an OpenFrame is, so apt takes the repository's i386 index as a unit does. As
	# openframe-sprite's CI runs it: the default seccomp profile stops 32-bit programs' newer
	# system calls on GitHub's runners (the network, for one).
	exec docker run --rm --security-opt seccomp=unconfined --platform linux/386 -v "$HERE:/src:ro" \
		i386/debian:trixie bash /src/tests/test_apt_suite.sh
fi
FAILS=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; [ -n "${2:-}" ] && sed 's/^/       /' <<< "$2" | tail -12; FAILS=$((FAILS + 1)); }
export DEBIAN_FRONTEND=noninteractive
apt-get -qq update >/dev/null 2>&1; apt-get -qq install -y dpkg-dev apt-utils gnupg >/dev/null 2>&1 ||
	{ echo "FAIL couldn't install the tools (dpkg-dev, apt-utils, gnupg): no network?"; exit 1; }
# Already in a container that isn't i386 (a CI job's own): apt takes i386 only, as a unit does
[ "$(dpkg --print-architecture)" = i386 ] || echo 'APT::Architecture "i386";' > /etc/apt/apt.conf.d/99i386
export GNUPGHOME=/tmp/gnupg; mkdir -m 700 -p $GNUPGHOME
gpg --batch --quiet --passphrase '' --quick-gen-key "test <t@t>" rsa2048 sign never >/dev/null 2>&1
W=/work; mkdir -p $W && cd $W
deb() { # name version
	d=/tmp/b-$1-$2; mkdir -p $d/DEBIAN
	printf 'Package: %s\nVersion: %s\nArchitecture: i386\nMaintainer: t <t@t>\nDescription: test\n' "$1" "$2" > $d/DEBIAN/control
	dpkg-deb --build --root-owner-group $d /tmp/k/$1_$2_i386.deb >/dev/null
}
mkdir -p /tmp/k
deb linux-image-openframe-6.18 6.18.64; deb linux-image-6.18.64-openframe 6.18.64-6
S=/src/scripts/apt-suite.sh
bash $S add 6.18 /tmp/k/*.deb >/dev/null; bash $S promote 6.18 >/dev/null
rm /tmp/k/*; deb linux-image-openframe-6.18 6.18.65; deb linux-image-6.18.65-openframe 6.18.65-1
bash $S add 6.18 /tmp/k/*.deb >/dev/null && ok "6.18.65 to testing" || bad "6.18.65 to testing"
gpg --export > /tmp/key.gpg
unit() { echo "$@" | tr ' ' '\n' | sed "s#.*#deb [signed-by=/tmp/key.gpg] file:$W/apt-repo & 6.18#" > /etc/apt/sources.list.d/t.list
	rm -f /etc/apt/sources.list.d/debian.sources; apt-get -qq update 2>&1; apt-cache policy linux-image-openframe-6.18 | sed -n 's/^  Candidate: //p'; }
[ "$(unit trixie | tail -1)" = 6.18.64 ] && ok "a released unit stays on 6.18.64" || bad "a released unit stays on 6.18.64" "$(unit trixie)"
[ "$(unit trixie trixie-testing | tail -1)" = 6.18.65 ] && ok "a test unit gets 6.18.65" || bad "a test unit gets 6.18.65" "$(unit trixie trixie-testing)"
[ "$(bash $S waiting 6.18 | sort | tr '\n' ,)" = "linux-image-6.18.65-openframe 6.18.65-1,linux-image-openframe-6.18 6.18.65," ] && ok "waiting lists it" || bad "waiting lists it" "$(bash $S waiting 6.18)"
bash $S promote 6.18 >/dev/null
[ "$(ls apt-repo/pool/6.18 | tr '\n' ,)" = "linux-image-6.18.65-openframe_6.18.65-1_i386.deb,linux-image-openframe-6.18_6.18.65_i386.deb," ] && [ -z "$(ls apt-repo/pool/testing-6.18)" ] &&
	ok "promote leaves one kernel, the new one, and testing empty" || bad "promote leaves one kernel" "$(ls -R apt-repo/pool)"
[ "$(unit trixie | tail -1)" = 6.18.65 ] && ok "every unit gets 6.18.65" || bad "every unit gets 6.18.65" "$(unit trixie)"
grep -q "^Codename: trixie-testing" apt-repo/dists/trixie-testing/Release && grep -q "^Origin: OpenFrame Kernel" apt-repo/dists/trixie-testing/Release &&
	ok "testing is Origin OpenFrame Kernel, codename trixie-testing (unattended-upgrades allows it)" || bad "testing's Release" "$(cat apt-repo/dists/trixie-testing/Release)"
echo
[ "$FAILS" -eq 0 ] && echo "all passed" || echo "$FAILS failed"
exit "$FAILS"
