#!/bin/bash
set -e

GITLAB_PATH=pandora:5050/robert/vimixos
#PLATFORMS="linux/amd64,linux/arm64"
PLATFORMS="linux/amd64"
CA_CERT=/home/robert/.config/gitlab_ssl/ca.crt
IMAGE_BASE_NAME=ci-ubuntu3

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
  -t $GITLAB_PATH/$IMAGE_BASE_NAME:26.04 \
  -f tools/docker/ubuntu2604 \
  --push .

echo "done"
