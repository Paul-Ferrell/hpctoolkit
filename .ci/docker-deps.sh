#!/bin/sh -e

# Helper script to construct a deps image: a Docker image with a working Spack
# and all the dependencies pre-compiled. Since compiling dependencies and
# pushing layers are expensive operations, we try *hard* to reuse a previous
# version before actually building a brand new image, while still maintaining
# that the result is still "freshened" by the latest version of Spack.
#
# Requires a running Docker service with appropriate login credentials.
#
# Usage:
#   ./docker-deps.sh <base tag> [<previous deps tags>...] -- <output tag> [<aliases>...]

BASE="$1"
shift

# Try to find a suitable previous deps image to pull bits from. If we fail we'll
# just start from scratch instead.
PREV=
while [ -n "$1" ] && [ "$1" != "--" ]; do
  if docker pull --quiet "$1"; then
    PREV="$1"
    break
  fi
  shift
done
while [ "$1" != "--" ]; do shift; done
shift

# Pull the bits out of the previous instance, along with the hashfiles
HASH_INPUTS='/opt/senv/*/spack.lock'
if [ -n "$PREV" ]; then
  docker run --rm=true --mount="type=bind,source=`realpath .ci/`,target=/.ci" \
    "$PREV" /transfer.sh pack previous.hash previous.tar || :
fi

# Push the bits into a fresh container, rebuild and extract the new hashfiles
docker run --rm=false --cidfile=next.cont --mount="type=bind,source=`realpath .ci/`,target=/.ci" \
  "$BASE" /transfer.sh unpack next.hash previous.tar

# If the hashfiles didn't change, we consider the difference moot and just
# reuse the old image for this round too. This saves significantly on upload
# bandwith because of the layer matching.
MAINOUT="$1"
shift
if diff .ci/previous.hash .ci/next.hash; then
  echo "Spack does not appear to have changed, reusing previous image"
  docker tag "$PREV" "$MAINOUT"
else
  # Commit the refreshed image and call that the final result.
  echo "Updating deps image with new Spack results..."
  docker container commit "$(cat next.cont)" "$MAINOUT"
  docker container rm "$(cat next.cont)"
fi
docker push "$MAINOUT"

# Tag and push all the aliases for this image
for al in "$@"; do
  docker tag "$MAINOUT" "$al"
  docker push --quiet "$al"
done
