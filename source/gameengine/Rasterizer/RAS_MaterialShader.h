#ifndef __RAS_MATERIAL_SHADER_H__
#define __RAS_MATERIAL_SHADER_H__

#include "RAS_GameObject.h" // For RAS_GameObject::LayersInfo.

class RAS_MeshUser;
class RAS_Rasterizer;

class RAS_MaterialShader
{
public:
	RAS_MaterialShader() = default;
	virtual ~RAS_MaterialShader() = default;

	/// Return true when the shader can be bound.
	virtual bool IsValid() const = 0;
	// Bind the shader and mainly update global uniforms.
	virtual void Activate(RAS_Rasterizer *rasty) = 0;
	/// Unbind the shader.
	virtual void Desactivate() = 0;
	/// Update the shader with mesh user data as model matrix.
	virtual void Update(RAS_Rasterizer *rasty, RAS_MeshUser *meshUser) = 0;
};

#endif  // __RAS_MATERIAL_SHADER_H__
