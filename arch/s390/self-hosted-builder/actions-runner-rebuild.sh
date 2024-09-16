#!/usr/bin/bash
set -ex

if [ ! -f /etc/actions-runner ]; then
    echo "Error: /etc/actions-runner env file not found"
    exit 1
fi

# Use local file if run interactively, otherwise wget the current one.
if [ -t 0 ] ; then
    if [ ! -f actions-runner.Dockerfile ]; then
        echo "Error: actions-runner.Dockerfile not found"
        exit 1
    fi
    DOCKERFILE=actions-runner.Dockerfile
else
    DOCKERFILE="$(mktemp)"
    wget https://raw.githubusercontent.com/zlib-ng/zlib-ng/refs/heads/develop/arch/s390/self-hosted-builder/actions-runner.Dockerfile -O $DOCKERFILE
fi

# Stop service
systemctl stop actions-runner

# Delete container
podman container rm gaplib-actions-runner

# Delete image
podman image rm localhost/zlib-ng/actions-runner

# Build image
podman build --squash -f $DOCKERFILE --tag zlib-ng/actions-runner .

# Create container
podman create --replace --name=gaplib-actions-runner --env-file=/etc/actions-runner --init --volume=actions-runner-temp:/home/actions-runner zlib-ng/actions-runner

# Start service
systemctl start actions-runner

# Clean up tempfile
if [ ! -t 0 ] ; then
    rm $DOCKERFILE
    echo "Deleted dockerfile $DOCKERFILE"
fi
