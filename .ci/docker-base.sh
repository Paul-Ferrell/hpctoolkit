#!/bin/sh -e

# Helper script to construct a base image, which has a few standard packages and
# a working Spack, but no compiled dependencies. Thus the "base" of the CI
# platform within Docker.
#
# Requires a running Docker service with appropriate login credentials.
#
# Usage:
#   ./docker-base.sh <OS identifier> [<previous base tags>...] -- <output tag> [<aliases>...]
#   NOTE: Only the aliases are pushed, the output tag is not.

OS="$1"
shift

# Construct the base image hashfile to detect configuration changes
(cd .ci/docker && md5sum Dockerfile."$OS" transfer.sh) > .ci/docker/hash

# Check if one of the fallback tags has a matching configuration, if it does
# we can short-circuit and just use that.
MACTCHING=
while [ -n "$1" ] && [ "$1" != "--" ]; do
  if docker pull --quiet "$1" && docker run --rm=true "$1" cat /hash > previous.hash; then
    if diff previous.hash .ci/docker/hash; then
      echo "Cached base image $1 is still valid, reusing."
      MATCHING="$1"
      break
    fi
  fi
  shift
done
while [ "$1" != "--" ]; do shift; done
shift

# If we found a fallback match, just reuse that image and carry on.
MAINOUT="$1"
shift
if [ -n "$MATCHING" ]; then
  docker tag "$MATCHING" "$MAINOUT"
else
  echo "Rebuilding image from scratch..."
  docker build --tag "$MAINOUT" --file=.ci/docker/Dockerfile."$OS" .ci/docker/
fi

# Tag and push all the aliases for this image
for al in "$@"; do
  docker tag "$MAINOUT" "$al"
  docker push --quiet "$al"
done
