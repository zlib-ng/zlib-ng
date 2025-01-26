#!/usr/bin/bash
set -ex

TMPDIR="$(mktemp -d)"

if [ -f actions-runner.Dockerfile ]; then
    MODE=1
    cp actions-runner.Dockerfile actions-runner entrypoint $TMPDIR
    cd $TMPDIR
else
    MODE=2
    cd $TMPDIR
    wget https://raw.githubusercontent.com/zlib-ng/zlib-ng/refs/heads/develop/arch/s390/self-hosted-builder/actions-runner.Dockerfile
    wget https://raw.githubusercontent.com/zlib-ng/zlib-ng/refs/heads/develop/arch/s390/self-hosted-builder/actions-runner
    wget https://raw.githubusercontent.com/zlib-ng/zlib-ng/refs/heads/develop/arch/s390/self-hosted-builder/entrypoint
fi

# Copy rpms needed to workaround VX compiler bug, ref #1852
mkdir clang
cp /clang-19/*.rpm clang/

# Stop service
systemctl stop actions-runner || true

# Delete old container
podman container rm gaplib-actions-runner || true

# Delete old image
podman image rm localhost/zlib-ng/actions-runner || true

# Build new image
podman build --squash -f actions-runner.Dockerfile --tag zlib-ng/actions-runner . 2>&1 | tee /var/log/actions-runner-build.log

# Create new container
podman create --replace --name=gaplib-actions-runner --env-file=/etc/actions-runner --init \
       --volume=actions-runner-temp:/home/actions-runner zlib-ng/actions-runner 2>&1 | tee -a /var/log/actions-runner-build.log

# Start service
systemctl start actions-runner || true

# Cleanup
podman image prune -af || true

# Clean up tempfile
if [ "$MODE" == "2" ] ; then
    cd $TMPDIR
    rm actions-runner.Dockerfile
    rm actions-runner
    rm entrypoint
    rm -rf clang
    cd ..
    rmdir $TMPDIR
    echo "Deleted tempfiles."
fi
