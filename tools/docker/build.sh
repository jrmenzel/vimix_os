#!/bin/bash
set -e

GITLAB_PATH=pandora:5050/robert/vimixos
#PLATFORMS="linux/amd64,linux/arm64"
PLATFORMS="linux/amd64"
CA_CERT=~/.config/gitlab_ssl/ca.crt

echo "Building CI images for platforms: $PLATFORMS"

echo "remove old builder if it exists"
docker buildx rm mybuilder || true

echo "creating new builder"
docker buildx create \
  --name mybuilder \
  --driver docker-container \
  --config tools/docker/buildkitd.toml \
  --driver-opt "network=host" \
  --use

echo "booting builder"
docker buildx inspect --bootstrap

echo "copying CA cert to builder"
docker cp $CA_CERT buildx_buildkit_mybuilder0:/etc/buildkit/certs/pandora-ca.crt

echo "restart builder"
docker restart buildx_buildkit_mybuilder0

echo "building and pushing images for Ubuntu 26.04"
docker buildx build \
  --platform $PLATFORMS \
  -t $GITLAB_PATH/ci-image:26.04 \
  -f tools/docker/ubuntu2604 \
  --push .

echo "building and pushing images for Ubuntu 25.04"
docker buildx build \
  --platform $PLATFORMS \
  -t $GITLAB_PATH/ci-image:25.04 \
  -f tools/docker/ubuntu2504 \
  --push .

echo "building and pushing images for Ubuntu 24.04"
docker buildx build \
  --platform $PLATFORMS \
  -t $GITLAB_PATH/ci-image:24.04 \
  -f tools/docker/ubuntu2404 \
  --push .

echo "done"
