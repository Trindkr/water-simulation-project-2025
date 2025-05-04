#pragma once

#include <ituGL/application/Application.h>

#include <ituGL/scene/Scene.h>
#include <ituGL/asset/ShaderLoader.h>
#include <ituGL/geometry/Mesh.h>
#include <ituGL/renderer/Renderer.h>
#include <ituGL/camera/CameraController.h>
#include <ituGL/utils/DearImGui.h>
#include <ituGL/shader/Material.h>
#include <ituGL/scene/transform.h>

class TextureCubemapObject;
class Material;

class WaterApplication : public Application
{
public:
	WaterApplication(unsigned int x, unsigned int y);

protected:
    void Initialize() override;
    void Update() override;
    void Render() override;
    void Cleanup() override;

private:
    void InitializeCamera();
    void InitializeLights();
    void InitializeDefaultMaterial();
    void InitializeWaterMaterial();
    void InitializeSandMaterial();

	void InitializeMeshes();
    void InitializeModels();
    void InitializeRenderer();

    void RenderGUI();
    void CreatePlaneMesh(Mesh& mesh, unsigned int gridX, unsigned int gridY);

private:

    // Helper object for debug GUI
    DearImGui m_imGui;

    // Camera controller
    CameraController m_cameraController;

	// Scene for opaque objects
    Scene m_opaqueScene;
	// Scene for transparent objects
    Scene m_transparentScene;

    // Renderer
    Renderer m_renderer;

    // Skybox texture
    std::shared_ptr<TextureCubemapObject> m_skyboxTexture;

	std::shared_ptr<Transform> m_sandTransform;
	std::shared_ptr<Transform> m_waterTransform;

    // Default material
    std::shared_ptr<Material> m_defaultMaterial;
    std::shared_ptr<Material> m_waterMaterial;
    std::shared_ptr<Material> m_sandMaterial;

    ShaderLoader m_vertexShaderLoader;
    ShaderLoader m_fragmentShaderLoader;

    std::shared_ptr<Mesh> m_planeMesh;

    //uniforms
	unsigned int m_gridX, m_gridY;

    float m_waterOpacity;
	glm::vec4 m_waterTroughColor;
	glm::vec4 m_waterSurfaceColor;
	glm::vec4 m_waterPeakColor;

    float m_troughLevel;
    float m_troughBlend;

	float m_peakLevel;
	float m_peakBlend;

    float m_fresnelStrength;
	float m_fresnelPower;

	float m_waveFrequency;
    float m_waveAmplitude;
    float m_waveLacunarity;
	float m_wavePersistence; 
	int m_waveOctaves;
	float m_waveSpeed;

	float m_sandBaseHeight;
	float m_waterBaseHeight;

};