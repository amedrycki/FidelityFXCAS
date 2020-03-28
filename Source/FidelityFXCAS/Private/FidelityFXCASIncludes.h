#pragma once

// Since ffx_*.ush files don't have #pragma once we wrap those includes in a separate header file

#include <stdint.h>
#define A_CPU 1
#include "../Shaders/Private/ffx_a.ush"
#include "../Shaders/Private/ffx_cas.ush"
