#pragma once

#include <stddef.h>

namespace spectrum_core
{

constexpr size_t kBandCount = 24;

bool begin();
void end();
bool poll(float *bandsOut, size_t bandCount, float *rmsOut);

} // namespace spectrum_core
