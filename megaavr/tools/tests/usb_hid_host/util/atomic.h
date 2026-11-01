#pragma once
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(mode) for (bool host_atomic = true; host_atomic; host_atomic = false)
