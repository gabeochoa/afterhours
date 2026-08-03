#!/usr/bin/env bash
#
# mk_bundle.sh - package a built executable as a desktop application bundle.
#
# Opt-in build tooling: afterhours never calls this, so you pay nothing unless
# you do. Wire it into any build system; from make that is two lines:
#
#   bundle: $(EXE)
#   	@vendor/afterhours/tools/mk_bundle.sh --exe $(EXE) \
#   	    --name MyApp --id com.example.myapp --resources output/resources
#
# Only macOS .app is implemented. Linux .desktop and Windows are dispatched to
# stubs so adding them later is additive -- see todo.md.
#
# Pairs with the files plugin: a .app puts the binary in Contents/MacOS, which
# is exactly what files::ProvidesResourcePaths keys on to find
# Contents/Resources. Bundled apps could not locate their own resources before
# that resolver understood this layout.

set -euo pipefail

die() { printf 'mk_bundle: %s\n' "$*" >&2; exit 1; }

usage() {
  sed -n '3,20p' "$0" | sed 's|^# \{0,1\}||'
  cat <<'EOF'

Required:
  --exe PATH            built executable to package
  --name NAME           display name (also the .app directory name)
  --id ID               bundle identifier, e.g. com.example.myapp

Optional:
  --out DIR             output bundle path (default: <exe dir>/<name>.app)
  --version STR         CFBundleVersion + ShortVersionString (default 0.1.0)
  --min-macos STR       LSMinimumSystemVersion (default 11.0)
  --category UTI        LSApplicationCategoryType
  --copyright STR       NSHumanReadableCopyright
  --resources DIR       copied into Contents/Resources/
  --icon FILE.icns      copied in and referenced as CFBundleIconFile
  --url-scheme SCHEME   register a URL scheme (repeatable)
  --plist-extra FILE    raw <key>/<value> XML merged into the plist dict
  --sign IDENTITY       codesign identity ("-" for ad-hoc)
  --entitlements FILE   entitlements plist, used with --sign
  --platform NAME       macos (default) | linux | windows
EOF
}

exe="" name="" ident="" out="" resources="" icon="" plist_extra=""
version="0.1.0" min_macos="11.0" category="" copyright=""
sign_id="" entitlements="" platform="macos"
url_schemes=()

while [ $# -gt 0 ]; do
  case "$1" in
    --exe)          exe="${2:-}"; shift 2 ;;
    --name)         name="${2:-}"; shift 2 ;;
    --id)           ident="${2:-}"; shift 2 ;;
    --out)          out="${2:-}"; shift 2 ;;
    --version)      version="${2:-}"; shift 2 ;;
    --min-macos)    min_macos="${2:-}"; shift 2 ;;
    --category)     category="${2:-}"; shift 2 ;;
    --copyright)    copyright="${2:-}"; shift 2 ;;
    --resources)    resources="${2:-}"; shift 2 ;;
    --icon)         icon="${2:-}"; shift 2 ;;
    --url-scheme)   url_schemes+=("${2:-}"); shift 2 ;;
    --plist-extra)  plist_extra="${2:-}"; shift 2 ;;
    --sign)         sign_id="${2:-}"; shift 2 ;;
    --entitlements) entitlements="${2:-}"; shift 2 ;;
    --platform)     platform="${2:-}"; shift 2 ;;
    -h|--help)      usage; exit 0 ;;
    *)              die "unknown argument: $1" ;;
  esac
done

[ -n "$exe" ]   || { usage >&2; die "--exe is required"; }
[ -n "$name" ]  || { usage >&2; die "--name is required"; }
[ -n "$ident" ] || { usage >&2; die "--id is required"; }
[ -f "$exe" ]   || die "executable not found: $exe"
[ -z "$resources" ] || [ -d "$resources" ] || die "resources dir not found: $resources"
[ -z "$icon" ]      || [ -f "$icon" ]      || die "icon not found: $icon"
[ -z "$plist_extra" ] || [ -f "$plist_extra" ] || die "plist extra not found: $plist_extra"

xml_escape() { sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'; }
plist_str() { printf '    <key>%s</key>\n    <string>%s</string>\n' "$1" "$(printf '%s' "$2" | xml_escape)"; }

bundle_macos() {
  local app="${out:-$(dirname "$exe")/$name.app}"
  local macos_dir="$app/Contents/MacOS"
  local res_dir="$app/Contents/Resources"

  # The binary keeps its own filename inside the bundle and CFBundleExecutable
  # is derived from it. Taking the name separately is how you get a bundle that
  # passes every check and then silently refuses to launch.
  local exe_name; exe_name="$(basename "$exe")"

  rm -rf "$app"
  mkdir -p "$macos_dir" "$res_dir"
  cp "$exe" "$macos_dir/$exe_name"
  chmod +x "$macos_dir/$exe_name"

  if [ -n "$resources" ]; then
    # Trailing slash: copy the CONTENTS of the dir, not the dir itself.
    rsync -a --delete "$resources"/ "$res_dir"/
  fi

  local icon_key=""
  if [ -n "$icon" ]; then
    cp "$icon" "$res_dir/"
    icon_key="$(basename "$icon")"
  fi

  {
    cat <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
EOF
    plist_str CFBundleExecutable "$exe_name"
    plist_str CFBundleIdentifier "$ident"
    plist_str CFBundleName "$name"
    plist_str CFBundleDisplayName "$name"
    plist_str CFBundleVersion "$version"
    plist_str CFBundleShortVersionString "$version"
    plist_str CFBundlePackageType "APPL"
    plist_str CFBundleInfoDictionaryVersion "6.0"
    plist_str LSMinimumSystemVersion "$min_macos"
    [ -n "$category" ]  && plist_str LSApplicationCategoryType "$category"
    [ -n "$copyright" ] && plist_str NSHumanReadableCopyright "$copyright"
    [ -n "$icon_key" ]  && plist_str CFBundleIconFile "$icon_key"

    # Without this the window is upscaled from 1x and looks soft on Retina --
    # a silent quality regression rather than a failure.
    printf '    <key>NSHighResolutionCapable</key>\n    <true/>\n'
    printf '    <key>NSSupportsAutomaticGraphicsSwitching</key>\n    <true/>\n'

    if [ ${#url_schemes[@]} -gt 0 ]; then
      printf '    <key>CFBundleURLTypes</key>\n    <array>\n      <dict>\n'
      printf '        <key>CFBundleURLName</key>\n        <string>%s</string>\n' "$ident"
      printf '        <key>CFBundleURLSchemes</key>\n        <array>\n'
      local scheme
      for scheme in "${url_schemes[@]}"; do
        printf '          <string>%s</string>\n' "$(printf '%s' "$scheme" | xml_escape)"
      done
      printf '        </array>\n      </dict>\n    </array>\n'
    fi

    # Escape hatch: anything not modelled above goes in verbatim, so we do not
    # grow a flag per plist key.
    [ -n "$plist_extra" ] && cat "$plist_extra"

    printf '</dict>\n</plist>\n'
  } > "$app/Contents/Info.plist"

  # A malformed plist shows up as an unexplained refusal to launch, so fail here
  # instead.
  if command -v plutil >/dev/null 2>&1; then
    plutil -lint "$app/Contents/Info.plist" >/dev/null \
      || die "generated Info.plist is malformed"
  fi

  if [ -n "$sign_id" ]; then
    if [ -n "$entitlements" ]; then
      codesign -s "$sign_id" -f --entitlements "$entitlements" "$app"
    else
      codesign -s "$sign_id" -f "$app"
    fi
  fi

  printf '%s\n' "$app"
}

bundle_linux()   { die "linux .desktop packaging is not implemented yet (todo.md)"; }
bundle_windows() { die "windows packaging is not implemented yet (todo.md)"; }

case "$platform" in
  macos)   bundle_macos ;;
  linux)   bundle_linux ;;
  windows) bundle_windows ;;
  *)       die "unknown platform: $platform" ;;
esac
