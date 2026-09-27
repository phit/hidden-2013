#!/bin/sh
# Lower-cases the file and folder names in a Hidden: Source Beta 4b folder, once, for Linux.
#
# The engine lower-cases every path it looks up, but Beta 4b has names with capitals
# (materials/Wall, materials/Decals, models/player/IRIS_Torso.vmt, ...), which Linux's
# case-sensitive file system then doesn't find. Windows doesn't care about case, so the renamed
# folder still works there.
#
#   sh lowercase_beta4b.sh [path/to/hidden]     default: the hidden folder next to hidden2013
set -eu

here=$(dirname "$(readlink -f "$0")")
dir=${1:-$here/../hidden}
if [ ! -f "$dir/gameinfo.txt" ]; then
	echo "No Hidden: Source Beta 4b folder at $dir; pass its path." >&2
	exit 1
fi

renamed=0
kept=0
# Deepest first, so a folder's contents are done before the folder itself.
find "$dir" -depth -name '*[A-Z]*' > /tmp/lowercase_beta4b.$$
while IFS= read -r path; do
	name=$(basename "$path")
	lower=$(printf '%s' "$name" | tr '[:upper:]' '[:lower:]')
	target=$(dirname "$path")/$lower
	if [ ! -e "$target" ]; then
		mv "$path" "$target"
		renamed=$((renamed + 1))
	elif [ -d "$path" ] && [ -d "$target" ]; then
		# Both spellings exist as folders: move the contents over.
		find "$path" -mindepth 1 -maxdepth 1 -exec mv -n {} "$target"/ \;
		if rmdir "$path" 2>/dev/null; then
			renamed=$((renamed + 1))
		else
			echo "Kept $path: names in it clash with $target" >&2
			kept=$((kept + 1))
		fi
	else
		echo "Kept $path: $target exists" >&2
		kept=$((kept + 1))
	fi
done < /tmp/lowercase_beta4b.$$
rm -f /tmp/lowercase_beta4b.$$

echo "Lower-cased $renamed names in $dir ($kept kept)."
