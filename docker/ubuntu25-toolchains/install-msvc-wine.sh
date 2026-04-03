#!/usr/bin/env bash
set -euo pipefail

dest=/opt/msvc
repo_url=https://github.com/mstorsjo/msvc-wine.git
repo_ref=a99ef92104b066f3ccc5937a65608961459f3a48
vsdownload_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dest)
            dest="$2"
            shift 2
            ;;
        --repo-url)
            repo_url="$2"
            shift 2
            ;;
        --repo-ref)
            repo_ref="$2"
            shift 2
            ;;
        *)
            vsdownload_args+=("$1")
            shift
            ;;
    esac
done

tmpdir="$(mktemp -d)"
cleanup() {
    rm -rf "${tmpdir}"
}
trap cleanup EXIT

git clone --depth=1 "${repo_url}" "${tmpdir}/msvc-wine"
git -C "${tmpdir}/msvc-wine" fetch --depth=1 origin "${repo_ref}"
git -C "${tmpdir}/msvc-wine" checkout --detach FETCH_HEAD

python3 "${tmpdir}/msvc-wine/vsdownload.py" \
    --accept-license \
    --dest "${dest}" \
    "${vsdownload_args[@]}"

sh "${tmpdir}/msvc-wine/install.sh" "${dest}"

cat <<'EOF'
MSVC toolchain installation completed via msvc-wine.

The default wrapper tool directories are under:
  /opt/msvc/bin/x86
  /opt/msvc/bin/x64
  /opt/msvc/bin/arm
  /opt/msvc/bin/arm64

Add the target directory you want to PATH before invoking cl, link, lib, or
other MSVC tools from Linux build tooling.
EOF
