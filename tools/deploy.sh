#!/usr/bin/env bash
# deploy.sh <game> [--journal]  — uploads build/<game>.vpk to Vita via VitaShell FTP.
# --journal also uploads games/<game>/journal/*.jpg (from tools/journal/make_journal.py)
# to ux0:data/goldbox/<game>/journal/.
set -euo pipefail

GAME=${1:-gateway}
JOURNAL=${2:-}
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VPK="$REPO_ROOT/build/$GAME/$GAME.vpk"

if [ -z "${VITA_IP:-}" ]; then
  echo "ERROR: VITA_IP is not set. Example: export VITA_IP=192.168.1.42" >&2
  exit 1
fi

if [ ! -f "$VPK" ]; then
  echo "ERROR: $VPK not found. Run tools/build.sh $GAME first." >&2
  exit 1
fi

echo "Uploading $VPK → ftp://$VITA_IP:1337/ux0:/$GAME.vpk"
curl --ftp-create-dirs -T "$VPK" "ftp://$VITA_IP:1337/ux0:/$GAME.vpk"
if [ "$JOURNAL" = "--journal" ]; then
  JDIR="$REPO_ROOT/games/$GAME/journal"
  ls "$JDIR"/*.jpg >/dev/null 2>&1 || { echo "ERROR: no images in $JDIR. Run python3 tools/journal/make_journal.py $GAME <Journal.pdf>." >&2; exit 1; }
  echo "Uploading journal images → ux0:data/goldbox/$GAME/journal/"
  for f in "$JDIR"/*.jpg; do
    curl -s --ftp-create-dirs -T "$f" "ftp://$VITA_IP:1337/ux0:/data/goldbox/$GAME/journal/$(basename "$f")"
  done
fi
echo "Done. Install from VitaShell: ux0:/$GAME.vpk"
