#!/bin/bash
set -e

cd "$(dirname "$0")/.."

echo "Building SpeciesID..."
make clean
make all
make test
