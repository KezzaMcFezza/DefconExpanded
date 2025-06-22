#!/usr/bin/env bash

# for macOS - handles SDL2 and other frameworks

set -x

# check if directory exists
if [ ! -d "$1" ]; then
  echo "Error: Directory $1 does not exist"
  find . -type d | head -10
  exit 1
fi

cd "$1"

# verify were in the Contents directory
if [ ! -d "MacOS" ]; then
  echo "Error: MacOS directory not found in $(pwd)"
  find . -type d | head -10
  exit 1
fi

# verify executable exists
if [ ! -f "MacOS/defcon" ]; then
  echo "Error: Executable MacOS/defcon not found"
  find . -name "defcon" -type f
  exit 1
fi

# Create frameworks directory if it doesn't exist
FRAMEWORKS=Frameworks
mkdir -p ${FRAMEWORKS}

# Define repo framework location
REPO_FRAMEWORKS_DIR="${GITHUB_WORKSPACE}/contrib/systemIV/contrib/macosx/Frameworks"
if [ ! -d "${REPO_FRAMEWORKS_DIR}" ]; then
  REPO_FRAMEWORKS_DIR="../../contrib/systemIV/contrib/macosx/Frameworks"
fi

echo "Looking for frameworks in: ${REPO_FRAMEWORKS_DIR}"

for framework in "SDL2" "Ogg" "Vorbis"; do
  if [ -d "${REPO_FRAMEWORKS_DIR}/${framework}.framework" ]; then
    echo "Found ${framework}.framework in repo"
    cp -R "${REPO_FRAMEWORKS_DIR}/${framework}.framework" "${FRAMEWORKS}/"
  else
    echo "ERROR: ${framework}.framework not found in ${REPO_FRAMEWORKS_DIR}"
    echo "This framework is required for the application to run"
    exit 1
  fi
done

echo "Fixing framework references in executable"
for framework_dir in ${FRAMEWORKS}/*.framework; do
  framework_name=$(basename "$framework_dir" .framework)
  
  echo "Framework $framework_name structure:"
  find "$framework_dir" -type f | head -5
  
  echo "Fixing reference in executable to $framework_name"
  install_name_tool -change \
    "@rpath/${framework_name}.framework/Versions/A/${framework_name}" \
    "@executable_path/../Frameworks/${framework_name}.framework/Versions/A/${framework_name}" \
    "MacOS/defcon"
  
  if [ -f "${framework_dir}/Versions/A/${framework_name}" ]; then
    echo "Fixing id in framework $framework_name"
    install_name_tool -id \
      "@executable_path/../Frameworks/${framework_name}.framework/Versions/A/${framework_name}" \
      "${framework_dir}/Versions/A/${framework_name}"
    
    for other_framework in "SDL2" "Ogg" "Vorbis"; do
      if [ "$other_framework" != "$framework_name" ]; then
        echo "Checking for $other_framework dependency in $framework_name"
        otool -L "${framework_dir}/Versions/A/${framework_name}" | grep -q "${other_framework}" && {
          echo "Fixing dependency on $other_framework in $framework_name"
          install_name_tool -change \
            "@rpath/${other_framework}.framework/Versions/A/${other_framework}" \
            "@executable_path/../Frameworks/${other_framework}.framework/Versions/A/${other_framework}" \
            "${framework_dir}/Versions/A/${framework_name}"
        }
      fi
    done
  fi
done

echo "Final executable dependencies:"
otool -L "MacOS/defcon"