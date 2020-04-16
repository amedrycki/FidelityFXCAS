#pragma once

#if FX_CAS_PLUGIN_ENABLED

#include "GlobalShader.h"

class FFidelityFXCASShaderCompilationRules
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Platform check
		if (Parameters.Platform != EShaderPlatform::SP_PCD3D_SM5)
			return false;

		// Feature check
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
			return false;

		// All checks passed
		return true;
	}
};

#endif // FX_CAS_PLUGIN_ENABLED
