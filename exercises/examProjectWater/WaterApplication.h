#pragma once

#include <ituGL/application/Application.h>
#include <ituGL/scene/Scene.h>
#include <ituGL/texture/FramebufferObject.h>
#include <ituGL/renderer/Renderer.h>
#include <ituGL/camera/CameraController.h>
#include <ituGL/utils/DearImGui.h>
#include <array>

class TextureCubemapObject;
class Material;

class WaterApplication : public Application
{
public:
	WaterApplication();

protected:
    void Initialize() override;
    void Update() override;
    void Render() override;
    void Cleanup() override;

private:
    void InitializeCamera();
    void InitializeLights();
    void InitializeMaterial();
    void InitializeModels();
    void InitializeRenderer();

    void RenderGUI();
    std::shared_ptr<Mesh> CreatePlaneMesh(int vertexWidth, int vertexHeight, bool generateTerrain, float worldWidth, float worldHeight);
    std::shared_ptr<Model> CreateCubeModel();

private:
    // Helper object for debug GUI
    DearImGui m_imGui;

    // Camera controller
    CameraController m_cameraController;

    // Global scene
    Scene m_scene;

    // Renderer
    Renderer m_renderer;

    // Skybox texture
    std::shared_ptr<TextureCubemapObject> m_skyboxTexture;

    // Default material
    std::shared_ptr<Material> m_defaultMaterial;
};