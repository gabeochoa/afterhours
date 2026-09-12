#include "native_dialogs.h"

#ifndef __APPLE__
namespace afterhours::native_dialogs {
Result show_native(const Request &) { return Unsupported{}; }
}
#endif
