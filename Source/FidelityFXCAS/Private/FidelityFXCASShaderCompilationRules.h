#pragma once

#if FX_CAS_PLUGIN_ENABLED

#include "GlobalShader.h"

class FFidelityFXCASShaderCompilationRules
{
public:
	// Global method for all shaders
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		// Platform check
		if (Parameters.Platform != EShaderPlatform::SP_PCD3D_SM5        // PC Windows
			&& Parameters.Platform != EShaderPlatform::SP_XBOXONE_D3D12 // XboxOne
			&& Parameters.Platform != EShaderPlatform::SP_PS4)          // PS4
			return false;

		// Feature check
		if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5))
			return false;

		// All checks passed
		return true;
	}

	// Separate rules for compute shaders
	template <bool FP16>
	static bool ShouldCompilePermutationCS(const FGlobalShaderPermutationParameters& Parameters)
	{
		// FP16 doesn't cook on XboxOne or PS4
		if (FP16 &&
			(Parameters.Platform == EShaderPlatform::SP_XBOXONE_D3D12
			|| Parameters.Platform == EShaderPlatform::SP_PS4))
			return false;

		// Default rules
		return ShouldCompilePermutation(Parameters);
	}
};

#endif // FX_CAS_PLUGIN_ENABLED
