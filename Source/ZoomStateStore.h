#pragma once

#include "audioplugins/common/hui/ZoomController.h"

#include <string>

/**
   Framework-free holder for the persisted "zoom" DPF state value.

   Validation is delegated to ZoomController::parseState (junk -> -1), so the
   stored text is always one of the canonical pill indices ("0".."3") and
   defaults to "0" (100%). Kept out of BastosPluginAdapter so it can be unit
   tested without DPF.
 */
class ZoomStateStore
{
public:
    // Returns true and stores the canonical text if `value` is a valid zoom
    // index; otherwise leaves the stored value untouched and returns false.
    bool set(const char* value)
    {
        const int index = audioplugins::common::hui::ZoomController::parseState(value);
        if (index < 0)
            return false;
        text_ = audioplugins::common::hui::ZoomController::stateText(index);
        return true;
    }

    const char* get() const noexcept { return text_.c_str(); }

private:
    std::string text_ = "0";
};
