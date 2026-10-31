#!/bin/bash
set -euo pipefail

installer_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec bash "$installer_dir/install-posix.sh" linux "$@"
