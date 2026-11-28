#pragma once
#include "powder/granular.hpp"
class UWorld;

// Registers explicit fixed box test fixtures; ordinary scenery stays excluded.
void ConfigureFixedMixerObstacles(UWorld* World,powder::GranularConfig& Config);
