#!/bin/bash

# default configuration
CLEAN=false
BUILD_TYPE="debug"
UPDATE_ENV=false

# Parse parameters
while getopts ":cre" opt; do
  case $opt in
    c)
      CLEAN=true
      ;;
    r)
      BUILD_TYPE="release"
      ;;
    e)
      UPDATE_ENV=true
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done

# with the -c parameter, cleanup
if [ "$CLEAN" = true ]; then
  echo "Clearing build directories..."
  rm -rf ./build
  rm -rf ./install
  rm -rf ./log
fi

# Backup CMakeLists.txt automatically restores in case of abnormalities
mv CMakeLists.txt CMakeLists.txt.bak
trap 'mv CMakeLists.txt.bak CMakeLists.txt 2>/dev/null' EXIT

# Set the mixin parameter according to the build type
MIXIN_ARG=""
if [ "$BUILD_TYPE" = "release" ]; then
  echo "Using Release build"
  MIXIN_ARG="--mixin release"
else
  echo "Using Debug build"
  MIXIN_ARG="--mixin debug"
fi

colcon --log-level info \
  build \
  --event-handlers console_direct+ \
  --parallel-workers 16 \
  --cmake-args \
  -Wno-dev \
  -DBUILD_TESTING=OFF \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \

echo "Build finished"

# -e option: Write the lib path of each package under install to the LD_LIBRARY_PATH of .env
if [ "$UPDATE_ENV" = true ]; then
  ENV_FILE=".env"

  # .env does not exist, the default content is created and written
  if [ ! -f "$ENV_FILE" ]; then
    cat > "$ENV_FILE" << 'EOF'
PYTHONUNBUFFERED=1
ROS_DOMAIN_ID=1
AMENT_PREFIX_PATH=/opt/ros/humble
LD_LIBRARY_PATH=/opt/ros/humble/lib:$LD_LIBRARY_PATH
EOF
    echo "Created $ENV_FILE"
  fi

  # Collect all lib directories under install dir
  LIB_PATHS=""
  for lib_dir in ./install/*/lib; do
    if [ -d "$lib_dir" ]; then
      abs_path=$(cd "$lib_dir" && pwd)
      if [ -z "$LIB_PATHS" ]; then
        LIB_PATHS="$abs_path"
      else
        LIB_PATHS="$LIB_PATHS:$abs_path"
      fi
    fi
  done

  if [ -z "$LIB_PATHS" ]; then
    echo "Warning: The lib directory is not found under install, please compile it first with colcon_build.sh" >&2
  else
    # Extract the base values of existing LD_LIBRARY_PATH
    OLD_LINE=$(grep '^LD_LIBRARY_PATH=' "$ENV_FILE" | head -1)
    OLD_BASE=$(echo "$OLD_LINE" | sed 's/^LD_LIBRARY_PATH=//' | sed 's/:\$LD_LIBRARY_PATH$//')

    if [ -n "$OLD_BASE" ]; then
      NEW_LINE="LD_LIBRARY_PATH=${LIB_PATHS}:${OLD_BASE}:\$LD_LIBRARY_PATH"
    else
      NEW_LINE="LD_LIBRARY_PATH=${LIB_PATHS}:\$LD_LIBRARY_PATH"
    fi

    # Replace LD_LIBRARY_PATH row, leaving the other environment variables unchanged
    sed -i "s|^LD_LIBRARY_PATH=.*|${NEW_LINE}|" "$ENV_FILE"

    echo "Updated LD_LIBRARY_PATH in $ENV_FILE"
    echo "  $NEW_LINE"
  fi
fi

echo "Done"