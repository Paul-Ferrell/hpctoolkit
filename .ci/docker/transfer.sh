#!/bin/bash -ex

# Script used to transfer Spack pieces between deps Docker images. The unpack
# phase also freshens the Spack and installs everything needed.
#
# Presumes .ci is mounted to /.ci within the container, path arguments are
# relative to this directory (even if absolute).
#
# Usage:
#    ./transfer.sh (pack|unpack) <output hash> <out/input tarball>

HASH="$2"
TARBALL="$3"

HASH_INPUTS='/opt/senv/*/spack.lock'
case "$1" in
pack)
  # Pack up the installed packages
  tar cf /.ci/"$TARBALL" /opt/spack/opt

  # Hash the important bits
  md5sum $HASH_INPUTS > /.ci/"$HASH" || :
  cat /hash >> /.ci/"$HASH"
  ;;
unpack)
  # If the tarball exists, unpack and make sure Spack understands it
  if [ -e /.ci/"$TARBALL" ]; then
    tar xf /.ci/"$TARBALL"
    rm /.ci/"$TARBALL"
    spack reindex
  fi

  # Refresh Spack to the latest version
  git -C /opt/spack pull --ff-only
  spack reindex

  # Mark all installations as implicit so only the bits we need for the deps
  # image are still left over.
  spack mark --all --implicit

  # Set up the environments and install them as needed.
  /.ci/spack/concretize.sh /opt/senv
  for env in /opt/senv/*; do
    spack -e "$env" install --yes-to-all --only-concrete --fail-fast
  done

  # Clean up the Spack state to minimize the size
  spack gc --yes-to-all
  spack clean --all

  # Hash the important bits
  md5sum $HASH_INPUTS > /.ci/"$HASH"
  cat /hash >> /.ci/"$HASH"
  ;;
*)
  echo "Unknown phase $1!" >&2
  exit 2
esac
