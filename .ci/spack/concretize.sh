#!/bin/bash -e

# Common script to set up the Spack environments used in CI and concretize them
# for installation. This doesn't do the installation, just the setup.
#
# Usage:
#   ./concretize.sh <output environments dir>

CISPACK_DIR="$(realpath "`dirname "$0"`")"
ENVS_DIR="$1"
shift

for envfn in "$CISPACK_DIR"/*.yml; do
  env="$ENVS_DIR/`basename $envfn .yml`"
  mkdir -p "$env"
  cp "$envfn" "$env"/spack.yaml
  spack -e "$env" concretize --fresh
done
