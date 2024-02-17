CHECKPATCH_PATH="../linux/scripts/checkpatch.pl"

find . -type f -name "*.c" -o -name "*.h" | xargs perl $CHECKPATCH_PATH -f --no-tree --fix-inplace
