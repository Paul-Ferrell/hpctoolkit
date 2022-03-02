#!/bin/bash -ex

# Helper script to instantate or update a Spack in the current (project)
# directory. Presumes the contents of .spackcache will be cached between runs
# on the same (or an identical) machine.
#
# This script is intended for shell executors, where the dependencies can't be
# built AOT for certain.
#
# Usage:
#   ./shell-init.sh <compiler string> <additional specs...>
#   source .spackuse

rm -f .spackuse
persist() {
  echo "$@" >> .spackuse
  eval "$@"
}

mkdir -p .spackcache/

# Determine the proper Spack spec for the compiler, so it can be used later
compiler="$1"
shift
compiler_spec=
case "$compiler" in
system*)
  # Special case(s), just use the system compilers
  ;;
gcc@*)
  compiler_spec="$compiler"
  ;;
clang@*)
  compiler_spec="llvm+$compiler"
  ;;
*)
  echo "Unrecognized compiler spec!" >&2
  exit 2
  ;;
esac

# Update the Spack cache to the latest
if ! [ -d .spackcache/bare/ ]; then
  git clone --depth=10 --single-branch --no-tags \
    --branch=develop --bare https://github.com/spack/spack.git .spackcache/bare/
fi
git --git-dir=.spackcache/bare fetch --verbose

# Expanded the cached Spack into an actual instance
git clone .spackcache/bare/ .spack
mkdir -p .spack-mcache/ .spackcache/bootstrap/
ln --symbolic "`realpath .spackcache/bootstrap/`" .spack-mcache/bootstrap
persist 'export SPACK_DISABLE_LOCAL_CONFIG=1'
persist 'export SPACK_USER_CACHE_PATH="`realpath .spack-mcache/`"'
persist 'export PATH="`realpath .spack/bin`:$PATH"'

# Connect up the main build cache
mkdir -p .spackcache/bcache
spack mirror add bcache .spackcache/bcache
if ! [ -e .spackcache/key ]; then
  spack gpg create --export-secret .spackcache/key 'HPCToolkit CI Spack Buildcache' 'nobody@nowhere.com'
fi
spack gpg trust .spackcache/key

# Connect up the per-compiler build cache. It uses the same key as the main cache.
mkdir -p .spackcache-percc/bcache
spack mirror add ccbcache .spackcache-percc/bcache

# Validate that Spack is working properly and is bootstrapped
spack bootstrap enable
spack spec zlib

# If we use a special compiler, it needs to be used for the dependencies as well
# to keep everything consistent (C++ mostly). So set that up first.
if [ -n "$compiler_spec" ]; then
  # First check if the system already has a compatible compiler. If it does we
  # don't need to do anything.
  spack compiler find
  if spack compiler info "$compiler" > .tmp.ccinfo; then
    echo "Using system compiler for $compiler"
  else
    # Install the compiler itself
    if ! spack install --yes-to-all --reuse --cache-only "$compiler_spec"; then
      spack install --yes-to-all --reuse --fail-fast "$compiler_spec"
      spack buildcache create --rebuild-index --allow-root --rel --force --mirror-name bcache "$compiler_spec"
    fi

    # Let Spack add it to the compilers.yaml, and double-check that the result
    # looks like we would expect.
    bin="`spack location -i "$compiler_spec"`"/bin/
    # NOTE: Spack load breaks the search mechanism, so limit fiddling to PATH
    PATH="$bin:$PATH" spack compiler find
    spack compiler info "$compiler" > .tmp.ccinfo
    grep --fixed-strings "cc = $bin" .tmp.ccinfo
    grep --fixed-strings "cxx = $bin" .tmp.ccinfo
    grep --fixed-strings "f77 = $bin" .tmp.ccinfo
  fi

  # Get the full version for the compiler we want to be using
  spack compiler info "$compiler" > .tmp.ccinfo
  spec=`grep --max-count=1 '^[^[:space:]]\+:$' .tmp.ccinfo`
  spec="${spec%%:}"
  rm .tmp.ccinfo

  # Set the concretization so we only use the compiler we want
  spack config add "packages:all:compiler:[$spec]"

  # Remove every compiler other than the one we want. The Spack concretizer has
  # a bit of trouble obeying our instructions, so we make it easy for it.
  spack compiler list > .tmp.ccinfo
  grep '@' .tmp.ccinfo | while read line; do
    if [ "$line" != "$spec" ]; then
      spack compiler rm --all "$line" ||:
    fi
  done
  rm .tmp.ccinfo

  spack clean --misc-cache
fi

# Now set up the environments. Since we have a compiler set it should concretize
# to use that compiler like we would expect.
.ci/spack/concretize.sh .senv/

# Install each of the environments. We do this first to try to reuse
# dependencies in the extra specs.
for env in .senv/*; do
  if ! spack -e "$env" install --yes-to-all --only-concrete --cache-only; then
    spack -e "$env" install --yes-to-all --only-concrete --fail-fast
    # NOTE: Save cache loading time by storing in the per-compiler cache.
    spack -e "$env" buildcache create --rebuild-index --allow-root --rel --force --mirror-name ccbcache
  fi
done

# Install the extra specs.
if [ -n "$*" ]; then
  if ! spack install --yes-to-all --cache-only "$@"; then
    spack install --yes-to-all --fail-fast "$@"
    # NOTE: Save cache loading time by storing in the per-compiler cache.
    spack buildcache create --rebuild-index --allow-root --rel --force --mirror-name ccbcache "$@"
  fi
fi

# Build a CI compiler spec for the requested compiler. We may not have built it
# ourselves, so extract all the bits from the compiler.yaml.
if [ -n "$compiler_spec" ]; then
  spack compiler info "$compiler" > .tmp.ccinfo
  cc="`grep --only-matching --max-count=1 'cc = .*' .tmp.ccinfo`"
  cxx="`grep --only-matching --max-count=1 'cxx = .*' .tmp.ccinfo`"
  mkdir -p .ccspec/
  echo "${cc##cc = } ${cxx##cxx = }" > .ccspec/"$compiler"
fi

# Clean up the build cache: anything that hasn't been touched in a while can go.
# If we needed it we will rebuild like usual.
find .spackcache/bcache .spackcache-percc/bcache -atime +7 -type f -exec rm '{}' +
spack buildcache update-index -d .spackcache/bcache
