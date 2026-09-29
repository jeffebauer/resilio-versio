#pragma once

// Optional per-section timing for the M3 profile firmware. Host-neutral: the
// Core only calls a function pointer that a host may set. Compiled out unless
// RV_PROFILE_HOOKS is defined (the profile firmware only), so the release
// firmware, the Plugin and the Renderer pay nothing.
//
// mark(s) means "section s just ended": the host charges the time since the
// previous mark to s.

namespace rv::prof {

enum Section { kControl, kDriveIn, kSplash, kTilt, kSpringA, kSpringB, kSpringC, kOutput, kNumSections };

#if defined(RV_PROFILE_HOOKS)
extern void (*markHook)(int section);
inline void mark(Section s)
{
    if (markHook) markHook(int(s));
}
#else
inline void mark(Section) {}
#endif

} // namespace rv::prof
