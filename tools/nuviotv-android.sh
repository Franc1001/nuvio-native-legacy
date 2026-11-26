#!/bin/bash
# NUVIO TV: the Live TV-only build of this app (-DNV_NUVIO_TV, package
# space.nuvio.tv, installs next to Nuvio). Puts the NUVIO TV name, launcher
# icon and TV banner in the Android resources, then builds like
# tools/android.sh. It EDITS tracked files (strings.xml and two PNGs): meant
# for CI and throwaway checkouts; `git checkout android/app/src/main/res`
# undoes it.
#   bash tools/nuviotv-android.sh --debug-only
set -euo pipefail
cd "$(dirname "$0")/.."
RES=android/app/src/main/res
sed -i 's|<string name="app_name">[^<]*</string>|<string name="app_name">NUVIO TV</string>|' "$RES/values/strings.xml"
grep -q '<string name="app_name">NUVIO TV</string>' "$RES/values/strings.xml"
cp android/nuviotv/ic_launcher.png "$RES/mipmap-nodpi/ic_launcher.png"
cp android/nuviotv/banner.png "$RES/drawable-nodpi/banner.png"
# The Gradle property goes in by environment (ORG_GRADLE_PROJECT_*), so
# tools/android.sh and the CI cache key that hashes it stay untouched.
env 'ORG_GRADLE_PROJECT_nuvio.nuvioTv=1' bash tools/android.sh "$@"
VER=$(sed -n 's/.*"version" *: *"\([^"]*\)".*/\1/p' deploy/app/appinfo.json | head -1)
for a in build/android/Nuvio-"$VER"-android*.apk; do
  [ -f "$a" ] && mv "$a" "${a/Nuvio-/NUVIO-TV-}"
done
ls build/android/NUVIO-TV-*.apk
