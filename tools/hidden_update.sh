#!/bin/bash
# Installs or updates Hidden: Rebuild in a Linux dedicated server folder, from the GitHub releases.
# The server packages ship it next to srcds_linux64, and the Pterodactyl egg
# (tools/pterodactyl/egg-hidden-rebuild.json) runs it before every start.
#
#   HIDDEN_CHANNEL=release|snapshot|v1.0.1.0 ./hidden_update.sh
#
# release (the default) is the newest release, snapshot the newest build of main (the "latest"
# prerelease), anything else a release tag. Nothing is downloaded when the installed package is
# already the one the channel points to. An existing cfg/mapcycle.txt is kept, and packages have no
# cfg/motd.txt to overwrite (their default is cfg/motd_default.txt).
set -eu

cd "$(dirname "$(readlink -f "$0")")"
repo=phit/hidden-2013
channel=${HIDDEN_CHANNEL:-release}

case "$channel" in
	release)	api=releases/latest ;;
	snapshot)	api=releases/tags/latest ;;
	*)			api=releases/tags/$channel ;;
esac

installed=$(cat .hidden_package 2>/dev/null || true)

# Without a mod installed there's nothing to fall back on, so errors stop the script; otherwise the
# server starts with what it has.
fail() {
	echo "Hidden: Rebuild: $1" >&2
	if [ -f hidden2013/gameinfo.txt ]; then
		echo "Hidden: Rebuild: starting with the installed ${installed:-version}" >&2
		exit 0
	fi
	exit 1
}

url=$(curl -fsSL "https://api.github.com/repos/$repo/$api" \
	| grep -o '"browser_download_url": *"[^"]*-linux-server\.tar\.gz"' \
	| head -n 1 | sed 's/.*"\(https[^"]*\)"$/\1/') || true
[ -n "$url" ] || fail "no Linux server package found for channel '$channel'"

name=${url##*/}
if [ "$name" = "$installed" ] && [ -f hidden2013/gameinfo.txt ]; then
	echo "Hidden: Rebuild is up to date ($name)"
else
	echo "Hidden: Rebuild: installing $name"
	curl -fsSL -o .hidden_download.tar.gz "$url" || fail "download of $name failed"
	keep=
	[ -f hidden2013/cfg/mapcycle.txt ] && keep=--exclude=hidden2013/cfg/mapcycle.txt
	tar -xzf .hidden_download.tar.gz $keep || fail "extracting $name failed"
	rm -f .hidden_download.tar.gz
	echo "$name" > .hidden_package
fi

# Packages up to 1.0.1.1 shipped a motd.txt, which would hide the default; remove it unless edited.
old_motd=5406c9a6a0ab3f43ae9bec0f488d4b4fa1ce86e9af360ad22653fee682affef5
if [ -f hidden2013/motd.txt ] && [ "$(sha256sum < hidden2013/motd.txt | cut -d' ' -f1)" = "$old_motd" ]; then
	rm -f hidden2013/motd.txt
fi
