#!/usr/bin/env bash
# scripts/evl-build.sh
#
# Build and optionally test SeRTial against the RaTOS EVL platform.
#
# Two build paths:
#
#   --cross [preset]   Cross-compile on the HOST using the RaTOS ISAR SDK
#                      (default preset: evl-cross). The SDK is downloaded and
#                      extracted automatically on first use.
#                      Suitable for CI compile-checks without QEMU.
#
#   --cross [preset] --test
#                      Cross-compile, then boot the RaTOS QEMU image, deploy
#                      the built test binaries, and run ctest inside the guest.
#
# Usage examples:
#
#   # CI default: cross-compile only (no QEMU)
#   scripts/evl-build.sh --cross
#
#   # Cross-compile + run all tests in QEMU:
#   scripts/evl-build.sh --cross --test
#
#   # Use a specific SDK location:
#   EVL_SDK_DIR=/opt/ratos-sdk scripts/evl-build.sh --cross
#
#   # Override the pinned RaTOS release:
#   RATOS_RELEASE_TAG=v0.0.14 scripts/evl-build.sh --cross --test
#
#   # Use pre-downloaded QEMU artifacts:
#   scripts/evl-build.sh --cross --test \
#       --ext4 /path/to/ratos.ext4 \
#       --kernel /path/to/vmlinuz \
#       --initrd /path/to/initrd.img
#
# Prerequisites:
#   Host:   gh (authenticated), patchelf, cmake, ninja/make
#   QEMU:   qemu-system-x86_64, rsync, ssh, ssh-keygen
#
# Environment variables (set in .sertial.env; override in .sertial.env.local):
#   RATOS_IMAGE_REF       ratos-dev-image reference (informational)
#   RATOS_RELEASE_REPO    "owner/repo" for gh CLI artifact download
#   RATOS_RELEASE_TAG     pinned release tag (e.g. v0.0.13)
#   RATOS_RELEASE_TOKEN   GitHub PAT with actions:read scope (CI secret)
#   QEMU_MEMORY           memory for QEMU guest (default: 4G)
#   QEMU_CPUS             CPUs for QEMU guest (default: 4)
#   SSH_PORT              host port forwarded to guest SSH (default: 22222)
#   EVL_SDK_DIR           SDK path (default: .evl-cache/sdk)

set -euo pipefail

# ---------------------------------------------------------------------------
# Load .sertial.env as defaults (exported variables take precedence)
# ---------------------------------------------------------------------------
_load_env() {
    local envfile="$1"
    [[ -f "$envfile" ]] || return 0
    local line key val
    while IFS= read -r line || [[ -n "$line" ]]; do
        [[ "$line" =~ ^[[:space:]]*(#|$) ]] && continue
        key="${line%%=*}"
        val="${line#*=}"
        key="${key//[[:space:]]/}"
        [[ -z "$key" ]] && continue
        [[ -v "$key" ]] || printf -v "$key" '%s' "$val"
    done < "$envfile"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

_load_env "${REPO_ROOT}/.sertial.env"
_load_env "${REPO_ROOT}/.sertial.env.local"

cd "$REPO_ROOT"

# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------
EXT4_PATH=""
KERNEL_PATH=""
INITRD_PATH=""
RATOS_RELEASE_REPO="${RATOS_RELEASE_REPO:-}"
RATOS_RUN_ID="${RATOS_RUN_ID:-}"
RATOS_RELEASE_TAG="${RATOS_RELEASE_TAG:-}"
QEMU_MEMORY="${QEMU_MEMORY:-4G}"
QEMU_CPUS="${QEMU_CPUS:-4}"
SSH_PORT="${SSH_PORT:-22222}"
EVL_SDK_DIR="${EVL_SDK_DIR:-.evl-cache/sdk}"

DO_CROSS=""
DO_TEST=0

WORK_DIR="$(mktemp -d /tmp/sertial-evl-XXXXXX)"

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
usage() {
    grep '^#' "$0" | sed 's/^# \?//' | head -50
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ext4)   EXT4_PATH="$(realpath "$2")";   shift 2 ;;
        --kernel) KERNEL_PATH="$(realpath "$2")"; shift 2 ;;
        --initrd) INITRD_PATH="$(realpath "$2")"; shift 2 ;;
        --run-id) RATOS_RUN_ID="$2";              shift 2 ;;
        --tag)    RATOS_RELEASE_TAG="$2";         shift 2 ;;
        --sdk-dir) EVL_SDK_DIR="$2";              shift 2 ;;
        --cross)
            if [[ $# -gt 1 && "$2" != --* ]]; then
                DO_CROSS="$2"; shift 2
            else
                DO_CROSS="evl-cross"; shift 1
            fi
            ;;
        --test)   DO_TEST=1; shift ;;
        --help|-h) usage ;;
        *) echo "Unknown argument: $1" >&2; usage ;;
    esac
done

# Default: cross-compile only
[[ -z "$DO_CROSS" ]] && DO_CROSS="evl-cross"

# Resolve relative EVL_SDK_DIR against REPO_ROOT
[[ "$EVL_SDK_DIR" != /* ]] && EVL_SDK_DIR="$REPO_ROOT/$EVL_SDK_DIR"

CACHE_DIR="$REPO_ROOT/.evl-cache"

if [[ -n "${RATOS_RELEASE_TOKEN:-}" ]]; then
    export GH_TOKEN="$RATOS_RELEASE_TOKEN"
fi

# ---------------------------------------------------------------------------
# Cleanup on exit
# ---------------------------------------------------------------------------
cleanup() {
    if [[ -f "$WORK_DIR/qemu.pid" ]]; then
        local pid
        pid="$(cat "$WORK_DIR/qemu.pid")"
        if kill -0 "$pid" 2>/dev/null; then
            echo "Stopping QEMU (pid $pid)..."
            kill -9 "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    fi
    if [[ -f "$WORK_DIR/loop_mounted" ]]; then
        sudo umount "$WORK_DIR/mnt" 2>/dev/null || true
        rm -f "$WORK_DIR/loop_mounted"
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Cross-compile on host
# ---------------------------------------------------------------------------
SDK_KEY_FILE="${CACHE_DIR}/.sdk_cache_key"
_SDK_KEY=""
[[ -n "$RATOS_RELEASE_TAG" ]] && _SDK_KEY="tag:${RATOS_RELEASE_TAG}"

_SDK_STALE=0
if [[ ! -d "$EVL_SDK_DIR/usr" ]]; then
    _SDK_STALE=1
    echo "RaTOS SDK not found at ${EVL_SDK_DIR} — will download and extract..."
elif [[ -n "$_SDK_KEY" && \
      ( ! -f "$SDK_KEY_FILE" || \
        "$(cat "$SDK_KEY_FILE" 2>/dev/null)" != "$_SDK_KEY" ) ]]; then
    _SDK_STALE=1
    echo "RaTOS SDK present but does not match ${_SDK_KEY} — re-extracting..."
fi

if [[ "$_SDK_STALE" -eq 1 ]]; then
    if [[ -z "$RATOS_RELEASE_REPO" || -z "$RATOS_RELEASE_TAG" ]]; then
        echo "ERROR: RaTOS SDK not found and RATOS_RELEASE_TAG/RATOS_RELEASE_REPO are not set." >&2
        echo "       Set them in .sertial.env or pass --tag <version>." >&2
        exit 1
    fi

    mkdir -p "$CACHE_DIR"
    SDK_CACHE_FILE="${CACHE_DIR}/ratos-dev-sdk-container-amd64.xz"
    if [[ ! -f "$SDK_CACHE_FILE" ]]; then
        echo "Downloading RaTOS SDK for ${RATOS_RELEASE_TAG}..."
        gh release download "$RATOS_RELEASE_TAG" \
            --repo "$RATOS_RELEASE_REPO" \
            --pattern "ratos-dev-sdk-container-amd64*" \
            --dir "$CACHE_DIR" \
            --clobber
        SDK_CACHE_FILE="$(ls "$CACHE_DIR"/ratos-dev-sdk-container-amd64* | head -1)"
    else
        echo "Using cached SDK archive: ${SDK_CACHE_FILE}"
    fi

    echo "Extracting RaTOS SDK to ${EVL_SDK_DIR} ..."
    rm -rf "$EVL_SDK_DIR"
    mkdir -p "$EVL_SDK_DIR"
    tar -xJf "$SDK_CACHE_FILE" -C "$EVL_SDK_DIR" --strip-components=1 2>/dev/null || true
    if [[ ! -d "$EVL_SDK_DIR/usr/include" ]]; then
        echo "ERROR: SDK extraction failed — usr/include not found in ${EVL_SDK_DIR}" >&2
        exit 1
    fi

    # Relocate SDK binaries and gcc wrapper
    if command -v patchelf &>/dev/null; then
        "$EVL_SDK_DIR/relocate-sdk.sh" 2>/dev/null || \
            sed -i "s|^GCC_SYSROOT=.*|GCC_SYSROOT=\"${EVL_SDK_DIR}\"|" \
                "$EVL_SDK_DIR/usr/bin/gcc-sysroot-wrapper.sh"
    else
        echo "WARNING: patchelf not found — patching gcc wrapper manually." >&2
        sed -i "s|^GCC_SYSROOT=.*|GCC_SYSROOT=\"${EVL_SDK_DIR}\"|" \
            "$EVL_SDK_DIR/usr/bin/gcc-sysroot-wrapper.sh"
    fi

    [[ -n "$_SDK_KEY" ]] && echo "$_SDK_KEY" > "$SDK_KEY_FILE"
    echo "SDK ready at ${EVL_SDK_DIR}."
else
    echo "Using RaTOS SDK at ${EVL_SDK_DIR} (${_SDK_KEY:-unversioned})"
fi

export EVL_SDK_DIR

echo "Building with preset '${DO_CROSS}' against RaTOS SDK..."
cmake --preset "${DO_CROSS}"
cmake --build --preset "${DO_CROSS}" --parallel "$(nproc)"
echo "Cross-compile complete."

# Skip QEMU if only a compile check was requested.
if [[ "$DO_TEST" -eq 0 ]]; then
    echo "No --test flag — skipping QEMU."
    exit 0
fi

# ---------------------------------------------------------------------------
# Download QEMU artifacts with persistent cache
# ---------------------------------------------------------------------------
_do_download() {
    local dl_dir="$1" run_id="$2" release_tag="$3"
    mkdir -p "$dl_dir"
    if [[ -n "$run_id" ]]; then
        echo "Downloading QEMU artifacts from workflow run ${run_id}..."
        gh run download "$run_id" \
            --repo "$RATOS_RELEASE_REPO" \
            --name ratos-evl-artifacts \
            --dir "$dl_dir"
    else
        echo "Downloading QEMU artifacts from release tag ${release_tag}..."
        gh release download "$release_tag" \
            --repo "$RATOS_RELEASE_REPO" \
            --pattern "vmlinuz" \
            --pattern "initrd.img" \
            --pattern "ratos-evl-image-container-amd64.ext4.gz" \
            --dir "$dl_dir"
    fi
    if compgen -G "$dl_dir/*.ext4.gz" > /dev/null; then
        local gz
        gz="$(ls "$dl_dir"/*.ext4.gz | head -1)"
        echo "Decompressing ${gz} ..."
        gunzip "$gz"
    fi
}

if [[ -z "$EXT4_PATH" || -z "$KERNEL_PATH" || -z "$INITRD_PATH" ]]; then
    if [[ -z "$RATOS_RELEASE_REPO" ]]; then
        echo "ERROR: RATOS_RELEASE_REPO not set. Set it in .sertial.env." >&2
        exit 1
    fi

    CACHE_KEY=""
    if [[ -n "$RATOS_RELEASE_TAG" ]]; then
        CACHE_KEY="tag:${RATOS_RELEASE_TAG}"
    elif [[ -n "$RATOS_RUN_ID" ]]; then
        CACHE_KEY="run:${RATOS_RUN_ID}"
    else
        echo "No RATOS_RELEASE_TAG set — locating latest successful run on main..."
        RATOS_RUN_ID="$(gh run list \
            --repo "$RATOS_RELEASE_REPO" \
            --workflow build-and-publish.yml \
            --branch main \
            --status success \
            --limit 1 \
            --json databaseId \
            --jq '.[0].databaseId')"
        if [[ -z "$RATOS_RUN_ID" || "$RATOS_RUN_ID" == "null" ]]; then
            echo "ERROR: No successful RaTOS build found." >&2
            exit 1
        fi
        CACHE_KEY="run:${RATOS_RUN_ID}"
    fi

    CACHED_KEY="${CACHE_DIR}/.cache_key"
    CACHED_EXT4="${CACHE_DIR}/ratos.ext4"
    CACHED_KERNEL="${CACHE_DIR}/vmlinuz"
    CACHED_INITRD="${CACHE_DIR}/initrd.img"

    if [[ -f "$CACHED_KEY" && "$(cat "$CACHED_KEY")" == "$CACHE_KEY" \
          && -f "$CACHED_EXT4" && -f "$CACHED_KERNEL" && -f "$CACHED_INITRD" ]]; then
        echo "Using cached QEMU artifacts ($CACHE_KEY)"
    else
        echo "Cache miss ($CACHE_KEY) — downloading..."
        rm -f "$CACHED_EXT4" "$CACHED_KERNEL" "$CACHED_INITRD" "$CACHED_KEY"
        rm -f "${CACHE_DIR}"/*.ext4.gz 2>/dev/null || true
        mkdir -p "$CACHE_DIR"

        if [[ "$CACHE_KEY" == tag:* ]]; then
            _do_download "$CACHE_DIR" "" "$RATOS_RELEASE_TAG"
        else
            _do_download "$CACHE_DIR" "$RATOS_RUN_ID" ""
        fi

        [[ -f "$CACHED_EXT4" ]]   || mv "$(ls "$CACHE_DIR"/*.ext4   | head -1)" "$CACHED_EXT4"
        [[ -f "$CACHED_KERNEL" ]] || mv "$(ls "$CACHE_DIR"/*vmlinuz  | head -1)" "$CACHED_KERNEL"
        [[ -f "$CACHED_INITRD" ]] || mv "$(ls "$CACHE_DIR"/*initrd*  | head -1)" "$CACHED_INITRD"
        echo "$CACHE_KEY" > "$CACHED_KEY"
    fi

    EXT4_PATH="$CACHED_EXT4"
    KERNEL_PATH="$CACHED_KERNEL"
    INITRD_PATH="$CACHED_INITRD"
fi

echo "Using ext4  : $EXT4_PATH"
echo "Using kernel: $KERNEL_PATH"
echo "Using initrd: $INITRD_PATH"

# ---------------------------------------------------------------------------
# Prepare a writable copy of the ext4 image
# ---------------------------------------------------------------------------
EXT4_COPY="$WORK_DIR/ratos.ext4"
echo "Copying ext4 image to $EXT4_COPY ..."
cp "$EXT4_PATH" "$EXT4_COPY"
truncate -s "+256M" "$EXT4_COPY"
resize2fs "$EXT4_COPY" 2>/dev/null

# ---------------------------------------------------------------------------
# Inject SSH public key
# ---------------------------------------------------------------------------
SSH_KEY="$WORK_DIR/ci_key"
ssh-keygen -t ed25519 -N "" -f "$SSH_KEY" -q

MOUNT_POINT="$WORK_DIR/mnt"
mkdir -p "$MOUNT_POINT"
sudo mount -o loop "$EXT4_COPY" "$MOUNT_POINT"
touch "$WORK_DIR/loop_mounted"
sudo mkdir -p "$MOUNT_POINT/root/.ssh"
sudo cp "${SSH_KEY}.pub" "$MOUNT_POINT/root/.ssh/authorized_keys"
sudo chmod 700 "$MOUNT_POINT/root/.ssh"
sudo chmod 600 "$MOUNT_POINT/root/.ssh/authorized_keys"
printf 'ulimit -H -t unlimited 2>/dev/null || true\nulimit -t unlimited 2>/dev/null || true\n' \
    | sudo tee "$MOUNT_POINT/etc/profile.d/no-cpu-limit.sh" > /dev/null
sudo chmod 644 "$MOUNT_POINT/etc/profile.d/no-cpu-limit.sh"
sudo umount "$MOUNT_POINT"
rm -f "$WORK_DIR/loop_mounted"

# ---------------------------------------------------------------------------
# Boot QEMU
# ---------------------------------------------------------------------------
if [[ -w /dev/kvm ]]; then
    CPU_ARGS="-cpu host -enable-kvm"
    echo "KVM acceleration enabled."
else
    CPU_ARGS="-cpu qemu64"
    echo "WARNING: /dev/kvm not accessible — running without KVM." >&2
fi

pkill -f "hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22" 2>/dev/null || true

echo "Starting QEMU..."
# shellcheck disable=SC2086
qemu-system-x86_64 \
    ${CPU_ARGS} \
    -smp "$QEMU_CPUS" \
    -m "$QEMU_MEMORY" \
    -machine q35 \
    -kernel "$KERNEL_PATH" \
    -initrd "$INITRD_PATH" \
    -drive "file=${EXT4_COPY},discard=unmap,if=none,id=disk,format=raw" \
    -device ide-hd,drive=disk \
    -append "root=/dev/sda rw rootwait console=ttyS0" \
    -nic "user,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22,model=e1000" \
    -device virtio-rng-pci \
    -serial "file:${WORK_DIR}/qemu-serial.log" \
    -monitor none \
    -nographic \
    < /dev/null \
    > "$WORK_DIR/qemu.log" 2>&1 &
echo $! > "$WORK_DIR/qemu.pid"

# ---------------------------------------------------------------------------
# Wait for SSH
# ---------------------------------------------------------------------------
SSH_OPTS="-i ${SSH_KEY} -o StrictHostKeyChecking=no -o BatchMode=yes -o ConnectTimeout=10 -o UserKnownHostsFile=/dev/null -p ${SSH_PORT}"
echo "Waiting for guest SSH (up to 3 minutes)..."
for i in $(seq 1 18); do
    if ! kill -0 "$(cat "$WORK_DIR/qemu.pid")" 2>/dev/null; then
        echo "ERROR: QEMU died unexpectedly!" >&2
        tail -20 "$WORK_DIR/qemu.log" >&2
        exit 1
    fi
    if ssh ${SSH_OPTS} root@127.0.0.1 true 2>/dev/null; then
        echo "SSH ready after ~$((i * 10))s"
        break
    fi
    [[ "$i" -eq 18 ]] && { echo "ERROR: SSH timeout after 3 minutes" >&2; tail -20 "$WORK_DIR/qemu-serial.log" >&2; exit 1; }
    echo "Waiting... (~$((i * 10))s)"
    sleep 10
done

# ---------------------------------------------------------------------------
# Deploy source + cross-compiled binaries to guest, configure, run ctest
# ---------------------------------------------------------------------------
echo "Deploying source to guest /root/SeRTial/ ..."
rsync -az --delete \
    --exclude=build --exclude=.git --exclude=.evl-cache \
    -e "ssh ${SSH_OPTS}" \
    ./ "root@127.0.0.1:/root/SeRTial/"

echo "Deploying build/${DO_CROSS}/ to guest /root/SeRTial/build/evl/ ..."
ssh ${SSH_OPTS} root@127.0.0.1 mkdir -p "/root/SeRTial/build/evl"
rsync -az \
    -e "ssh ${SSH_OPTS}" \
    "build/${DO_CROSS}/" "root@127.0.0.1:/root/SeRTial/build/evl/"

echo "Configuring on guest (generates CTestTestfile.cmake)..."
ssh ${SSH_OPTS} root@127.0.0.1 bash -lc "
    set -euo pipefail
    cd /root/SeRTial
    rm -f build/evl/CMakeCache.txt build/evl/CMakeFiles/cmake.check_cache
    cmake --preset evl 2>&1
"

echo "Running ctest --preset evl on EVL guest..."
ssh ${SSH_OPTS} root@127.0.0.1 bash -lc "
    set -euo pipefail
    cd /root/SeRTial
    ctest --preset evl --timeout 120 2>&1
"

echo "All EVL tests passed."
