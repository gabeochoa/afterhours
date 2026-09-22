#!/bin/sh
# ui core is a leaf: component config/init and the pre-update bridge must not
# include animation or the ui_motion bridge. text_unit_motion.h is the one
# remaining exception; it moves with the GPU text-motion work.
set -eu
cd "$(dirname "$0")/.."
core="src/plugins/ui/component_config.h src/plugins/ui/component_init.h src/plugins/ui/utilities.h src/plugins/ui/extension.h"
if grep -n -E '#include.*(animation|ui_motion|motion_config)' $core; then
  echo "ui boundary violated: ui core includes animation" >&2
  exit 1
fi
if grep -n 'motion::' $core; then
  echo "ui boundary violated: ui core references motion::" >&2
  exit 1
fi
if grep -rn -E '#include.*(animation|ui_motion)' src/plugins/ui/rendering.h; then
  echo "ui boundary violated: rendering includes animation" >&2
  exit 1
fi
echo "ui boundary ok"
