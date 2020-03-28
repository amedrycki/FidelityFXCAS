#pragma once

#include "FidelityFXCASShaderPS.h"

IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderPS, "/Plugin/FidelityFXCAS/Private/CAS_RenderPS.usf", "mainPS", SF_Pixel);

bool FFidelityFXCASShaderPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}
