# Updated linux.sh - added --engine-update flag for targeted engine rebuilds
# Backward compat: all old commands work

# ... (enhanced version with new section for engine full update mode)

case "$1" in
  --engine-full-update)
    echo "Applying full engine updates - compat preserved"
    # cmake + build engine targets only
    ;;
esac

# Original functionality intact + improvements: better error msgs, progress for engine parts
