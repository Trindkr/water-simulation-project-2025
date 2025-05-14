#include "WaterApplication.h"

#include <ituGL/asset/TextureCubemapLoader.h>
#include <ituGL/asset/ShaderLoader.h>
#include <ituGL/asset/ModelLoader.h>

#include <ituGL/geometry/VertexFormat.h>
#include <ituGL/texture/Texture2DObject.h>
#include <ituGL/renderer/ForwardRenderPass.h>

#include <ituGL/camera/Camera.h>
#include <ituGL/scene/SceneCamera.h>

#include <ituGL/lighting/DirectionalLight.h>
#include <ituGL/lighting/PointLight.h>
#include <ituGL/scene/SceneLight.h>

#include <ituGL/shader/ShaderUniformCollection.h>
#include <ituGL/shader/Material.h>
#include <ituGL/geometry/Model.h>
#include <ituGL/scene/SceneModel.h>

#include <ituGL/texture/FramebufferObject.h>
#include <ituGL/texture/TextureObject.h>

#include <ituGL/renderer/SkyboxRenderPass.h>
#include <ituGL/renderer/ForwardRenderPass.h>
#include <ituGL/scene/RendererSceneVisitor.h>
#include <ituGL/scene/Transform.h>
#include <ituGL/scene/ImGuiSceneVisitor.h>
#include <imgui.h>

#include <glm/gtx/transform.hpp>  

#include <numbers>
#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>
#include <iostream>
#include <glm/gtx/string_cast.hpp>


WaterApplication::WaterApplication(unsigned int x, unsigned int y)
	: Application(1920, 1080, "Water applicaiton")
	, m_renderer(GetDevice())
	, m_vertexShaderLoader(Shader::Type::VertexShader)
	, m_fragmentShaderLoader(Shader::Type::FragmentShader)
	, m_gridX(x)
	, m_gridY(y)

	, m_waterScale(glm::vec3(20.0f, 1.0f, 20.0f))

	// Water parameters
    , m_waterTroughColor(0.0f, 0.3f, 0.4f, 1.0f)  // Tropical deep blue green color  
    , m_waterSurfaceColor(0.0f, 0.6f, 0.5f, 1.0f) // Tropical vibrant blue green color  
    , m_waterPeakColor(0.7f, 0.9f, 0.8f, 1.0f)    // Tropical light blue green color
	, m_waterOpacity(0.8f)

	, m_troughLevel(0.16f)
	, m_troughBlend(0.3f)
	, m_peakLevel(0.185f)
	, m_peakBlend(0.12f)

	// Fresnel effect 
	, m_fresnelPower(3.5f)
	, m_fresnelStrength(0.8f)

	//Wave parameters
	, m_waveAmplitude(0.18f)
	, m_waveFrequency(0.4f)
	, m_wavePersistence(0.3f)
	, m_waveLacunarity(2.18f)
	, m_waveOctaves(8)
	, m_waveSpeed(0.5f)

	//underwater caustics
	, m_causticsColor(1.0f, 1.0f, 1.0f) // white color
	, m_causticsIntensity(0.2f)
	, m_causticsOffset(0.75f)
	, m_causticsScale(1.0f)
	, m_causticsSpeed(1.0f)
	, m_causticsThickness(0.4f)

	// clip plane
	, m_sandBaseHeight(-1.0f)
	, m_waterBaseHeight(2.0f)
	, m_clipPlane(glm::vec4(0.0f, 1.0f, 0.0f, -m_waterBaseHeight))

{
}

// Helper function to ensure the offscreen buffer dimensions are a power of two
unsigned int NextPowerOfTwo(unsigned int x)
{
	if (x == 0) return 1;
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return ++x;
}


void WaterApplication::Initialize()
{
	Application::Initialize();

	// Initialize DearImGUI
	m_imGui.Initialize(GetMainWindow());

	SetupOffScreenBuffer();
	InitializeDefaultMaterial();
	InitializeWaterMaterial();
	InitializeSandMaterial();
	InitializeMeshes();
	InitializeModels();

	InitializeLights();
	InitializeCamera();
	InitializeRenderer();

	//depth test
	GetDevice().EnableFeature(GL_DEPTH_TEST);
	//GetDevice().SetWireframeEnabled(true);
}

void WaterApplication::Update()
{
	Application::Update();

	const Window& window = GetMainWindow();
	
	window.GetDimensions(m_width, m_height);
	float aspectRatio = (m_height != 0) ? static_cast<float>(m_width) / m_height : 1.0f; // Default fallback

	if (m_cameraController.GetCamera())
	{
		Camera& camera = *m_cameraController.GetCamera()->GetCamera();
		camera.SetPerspectiveProjectionMatrix(static_cast<float>(std::numbers::pi) * 0.5f, aspectRatio, 0.1f, 100.0f);
	}

	// Update camera controller
	m_cameraController.Update(GetMainWindow(), GetDeltaTime());

	if (m_sandTransform)
	{
		glm::vec3 sandWorldPos = glm::vec3(m_sandTransform->GetTransformMatrix()[3]);
		m_sandBaseHeight = sandWorldPos.y;
		m_waterMaterial->SetUniformValue("SandBaseHeight", m_sandBaseHeight);
	}

	if (m_waterTransform)
	{
		glm::vec3 waterWorldPos = glm::vec3(m_waterTransform->GetTransformMatrix()[3]);
		m_waterBaseHeight = waterWorldPos.y;
		m_waterMaterial->SetUniformValue("WaterBaseHeight", m_waterBaseHeight);

		m_clipPlane = glm::vec4(0.0f, 1.0f, 0.0f, -m_waterBaseHeight);
		m_sandMaterial->SetUniformValue("ClipPlane", m_clipPlane);
	}

	// Add the scene nodes to the renderer
	RendererSceneVisitor rendererSceneVisitor(m_renderer);
	m_opaqueScene.AcceptVisitor(rendererSceneVisitor);
	m_transparentScene.AcceptVisitor(rendererSceneVisitor);

}

void WaterApplication::Render()
{
	Application::Render();

	m_offscreenFBO->Bind();
	GetDevice().Clear(true, Color(0.0f, 0.0f, 0.0f, 1.0f), true, 1.0f);

	// enable clip distance for the reflection pass
	GetDevice().EnableFeature(GL_CLIP_DISTANCE0);

	glViewport(0, 0, m_offscreenWidth, m_offscreenHeight);

	m_renderer.Reset(); 
	RendererSceneVisitor offVis(m_renderer);
	m_opaqueScene.AcceptVisitor(offVis);

	// Get the current camera
	std::shared_ptr<SceneCamera> sceneCamera = m_cameraController.GetCamera();
	Camera& camera = *sceneCamera->GetCamera();

	// copy the camera to a new one to modify it for the reflection pass
	std::shared_ptr<Camera> reflectionCam = std::make_shared<Camera>(camera); 

	// Set the reflection cam
	m_renderer.SetCurrentCamera(*reflectionCam);
	glm::vec3 originalPosition;

	// this flips the camera so it becomes mirrored across the water plane, by offseting the height and inverting the pitch
	SetOffScreenCamera(*reflectionCam, originalPosition);

	// first render pass for the offscreen framebuffer
	m_renderer.Render();


	m_renderer.SetCurrentCamera(camera); // reset to original camera

	GetDevice().DisableFeature(GL_CLIP_DISTANCE0);
	FramebufferObject::Unbind();
	glViewport(0, 0, m_width, m_height);

	GetDevice().Clear(true, Color(0.0f, 0.0f, 0.0f, 1.0f), true, 1.0f);

	m_renderer.Reset(); 
	RendererSceneVisitor onVis(m_renderer);
	m_opaqueScene.AcceptVisitor(onVis);
	m_transparentScene.AcceptVisitor(onVis);

	// rerender scene for on screen framebuffer
	m_renderer.Render(); 

	// Render the debug user interface  
	RenderGUI();
}

void WaterApplication::Cleanup()
{
	// Cleanup DearImGUI
	m_imGui.Cleanup();

	Application::Cleanup();
}


void WaterApplication::InitializeCamera()
{
	// Create the main camera
	std::shared_ptr<Camera> camera = std::make_shared<Camera>();
	camera->SetViewMatrix(glm::vec3(10, 5, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	camera->SetPerspectiveProjectionMatrix(1.0f, 1.0f, 0.1f, 100.0f);

	// Create a scene node for the camera
	std::shared_ptr<SceneCamera> sceneCamera = std::make_shared<SceneCamera>("camera", camera);

	// Add the camera node to the scene
	m_opaqueScene.AddSceneNode(sceneCamera);

	// Set the camera scene node to be controlled by the camera controller
	m_cameraController.SetCamera(sceneCamera);
}

void WaterApplication::InitializeLights()
{
	// Create a directional light and add it to the scene
	std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>();
	directionalLight->SetDirection(glm::vec3(-0.3f, -1.0f, -0.3f)); // It will be normalized inside the function
	directionalLight->SetIntensity(1.0f);
	m_opaqueScene.AddSceneNode(std::make_shared<SceneLight>("directional light", directionalLight));

	// Create a point light and add it to the scene
	//std::shared_ptr<PointLight> pointLight = std::make_shared<PointLight>();
	//pointLight->SetPosition(glm::vec3(0, 0, 0));
	//pointLight->SetDistanceAttenuation(glm::vec2(5.0f, 10.0f));
	//m_scene.AddSceneNode(std::make_shared<SceneLight>("point light", pointLight));
}
void WaterApplication::InitializeDefaultMaterial()
{
	// Load and build shader
	std::vector<const char*> vertexShaderPaths;
	vertexShaderPaths.push_back("shaders/version330.glsl");
	vertexShaderPaths.push_back("shaders/default.vert");
	Shader vertexShader = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

	std::vector<const char*> fragmentShaderPaths;
	fragmentShaderPaths.push_back("shaders/version330.glsl");
	fragmentShaderPaths.push_back("shaders/utils.glsl");
	fragmentShaderPaths.push_back("shaders/lambert-ggx.glsl");
	fragmentShaderPaths.push_back("shaders/lighting.glsl");
	fragmentShaderPaths.push_back("shaders/default_pbr.frag");
	Shader fragmentShader = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

	std::shared_ptr<ShaderProgram> shaderProgramPtr = std::make_shared<ShaderProgram>();
	shaderProgramPtr->Build(vertexShader, fragmentShader);

	// Get transform related uniform locations
	ShaderProgram::Location cameraPositionLocation = shaderProgramPtr->GetUniformLocation("CameraPosition");
	ShaderProgram::Location worldMatrixLocation = shaderProgramPtr->GetUniformLocation("WorldMatrix");
	ShaderProgram::Location viewProjMatrixLocation = shaderProgramPtr->GetUniformLocation("ViewProjMatrix");
	ShaderProgram::Location clipPlaneLocation = shaderProgramPtr->GetUniformLocation("ClipPlane");

	// Register shader with renderer
	m_renderer.RegisterShaderProgram(shaderProgramPtr,
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
		{
			if (cameraChanged)
			{
				shaderProgram.SetUniform(cameraPositionLocation, camera.ExtractTranslation());
				shaderProgram.SetUniform(viewProjMatrixLocation, camera.GetViewProjectionMatrix());
			}
			shaderProgram.SetUniform(worldMatrixLocation, worldMatrix);
			shaderProgram.SetUniform(clipPlaneLocation, m_clipPlane);
		},
		m_renderer.GetDefaultUpdateLightsFunction(*shaderProgramPtr)
	);

	// Filter out uniforms that are not material properties
	ShaderUniformCollection::NameSet filteredUniforms;
	filteredUniforms.insert("CameraPosition");
	filteredUniforms.insert("WorldMatrix");
	filteredUniforms.insert("ViewProjMatrix");
	filteredUniforms.insert("LightIndirect");
	filteredUniforms.insert("LightColor");
	filteredUniforms.insert("LightPosition");
	filteredUniforms.insert("LightDirection");
	filteredUniforms.insert("LightAttenuation");
	filteredUniforms.insert("ClipPlane");

	// Create reference material
	assert(shaderProgramPtr);
	m_defaultMaterial = std::make_shared<Material>(shaderProgramPtr, filteredUniforms);

}

void WaterApplication::InitializeWaterMaterial()
{

	std::vector<const char*> vertexShaderPaths;
	vertexShaderPaths.push_back("shaders/version330.glsl");
	vertexShaderPaths.push_back("shaders/water.vert");

	Shader waterVS = ShaderLoader(Shader::VertexShader).Load(vertexShaderPaths);

	std::vector<const char*> fragmentShaderPaths;
	fragmentShaderPaths.push_back("shaders/version330.glsl");
	fragmentShaderPaths.push_back("shaders/utils.glsl");
	fragmentShaderPaths.push_back("shaders/lambert-ggx.glsl");
	fragmentShaderPaths.push_back("shaders/lighting.glsl");
	fragmentShaderPaths.push_back("shaders/water.frag");

	Shader waterFS = ShaderLoader(Shader::FragmentShader).Load(fragmentShaderPaths);

	std::shared_ptr<ShaderProgram> waterShaderProgram = std::make_shared<ShaderProgram>();
	waterShaderProgram->Build(waterVS, waterFS);


	ShaderProgram::Location cameraPositionLocation = waterShaderProgram->GetUniformLocation("CameraPosition");
	ShaderProgram::Location worldMatrixLocation = waterShaderProgram->GetUniformLocation("WorldMatrix");
	ShaderProgram::Location viewProjMatrixLocation = waterShaderProgram->GetUniformLocation("ViewProjMatrix");
	ShaderProgram::Location timeLocation = waterShaderProgram->GetUniformLocation("Time");

	m_renderer.RegisterShaderProgram(waterShaderProgram,
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool cameraChanged)
		{
			if (cameraChanged)
			{
				shaderProgram.SetUniform(cameraPositionLocation, camera.ExtractTranslation());
				shaderProgram.SetUniform(viewProjMatrixLocation, camera.GetViewProjectionMatrix());
			}
			shaderProgram.SetUniform(worldMatrixLocation, worldMatrix);
			shaderProgram.SetUniform(timeLocation, static_cast<float>(GetTime())); // Pass the time to the shader
		},
		m_renderer.GetDefaultUpdateLightsFunction(*waterShaderProgram)
	);


	m_waterMaterial = std::make_shared<Material>(waterShaderProgram);

	// Water vertex shader uniforms
	m_waterMaterial->SetUniformValue("Opacity", m_waterOpacity);
	m_waterMaterial->SetUniformValue("TroughColor", m_waterTroughColor);
	m_waterMaterial->SetUniformValue("SurfaceColor", m_waterSurfaceColor);
	m_waterMaterial->SetUniformValue("PeakColor", m_waterPeakColor);
	m_waterMaterial->SetUniformValue("TroughLevel", m_troughLevel);
	m_waterMaterial->SetUniformValue("TroughBlend", m_troughBlend);
	m_waterMaterial->SetUniformValue("PeakLevel", m_peakLevel);
	m_waterMaterial->SetUniformValue("PeakBlend", m_peakBlend);

	// Fresnel effect uniforms
	m_waterMaterial->SetUniformValue("FresnelStrength", m_fresnelStrength);
	m_waterMaterial->SetUniformValue("FresnelPower", m_fresnelPower);

	// Water fragment shader uniforms
	m_waterMaterial->SetUniformValue("WaveAmplitude", m_waveAmplitude);
	m_waterMaterial->SetUniformValue("WaveFrequency", m_waveFrequency);
	m_waterMaterial->SetUniformValue("WavePersistence", m_wavePersistence);
	m_waterMaterial->SetUniformValue("WaveLacunarity", m_waveLacunarity);
	m_waterMaterial->SetUniformValue("WaveOctaves", m_waveOctaves);
	m_waterMaterial->SetUniformValue("WaveSpeed", m_waveSpeed);

	m_waterMaterial->SetUniformValue("SandBaseHeight", m_sandBaseHeight);
	m_waterMaterial->SetUniformValue("WaterBaseHeight", m_waterBaseHeight);

	m_waterMaterial->SetUniformValue("ReflectionTexture", m_offscreenColorTex);

	m_waterMaterial->SetBlendEquation(Material::BlendEquation::Add);
	m_waterMaterial->SetBlendParams(Material::BlendParam::SourceAlpha, Material::BlendParam::OneMinusSourceAlpha);
	m_waterMaterial->SetBlendEquation(Material::BlendEquation::Add);
	//m_waterMaterial->SetDepthWrite(false);
}

void WaterApplication::InitializeSandMaterial()
{
	Shader sandVS = m_vertexShaderLoader.Load("shaders/sand.vert");
	Shader sandFS = m_fragmentShaderLoader.Load("shaders/sand.frag");
	std::shared_ptr<ShaderProgram> sandShaderProgram = std::make_shared<ShaderProgram>();
	sandShaderProgram->Build(sandVS, sandFS);

	ShaderProgram::Location worldMatrixLocation = sandShaderProgram->GetUniformLocation("WorldMatrix");
	ShaderProgram::Location viewProjMatrixLocation = sandShaderProgram->GetUniformLocation("ViewProjMatrix");
	ShaderProgram::Location timeLocation = sandShaderProgram->GetUniformLocation("Time");

	m_renderer.RegisterShaderProgram(sandShaderProgram,
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool /*cameraChanged*/)
		{
			shaderProgram.SetUniform(worldMatrixLocation, worldMatrix);
			shaderProgram.SetUniform(viewProjMatrixLocation, camera.GetViewProjectionMatrix());
			shaderProgram.SetUniform(timeLocation, static_cast<float>(GetTime())); // Pass the time to the shader 
		},

		m_renderer.GetDefaultUpdateLightsFunction(*sandShaderProgram)
	);

	std::shared_ptr<Texture2DObject> sandTexture = Texture2DLoader::LoadTextureShared(
		"textures/sandTexture.jpg",
		TextureObject::FormatRGB,
		TextureObject::InternalFormatSRGB8,
		true,
		true
	);

	m_sandMaterial = std::make_shared<Material>(sandShaderProgram);
	m_sandMaterial->SetUniformValue("Color", glm::vec4(1.0f));
	m_sandMaterial->SetUniformValue("ColorTexture", sandTexture);

	glm::vec2 sandTextureScale(1.0f / m_waterScale.x,
		1.0f / m_waterScale.z);

	m_sandMaterial->SetUniformValue("ColorTextureScale", sandTextureScale);
	m_sandMaterial->SetUniformValue("ClipPlane", m_clipPlane);
	m_sandMaterial->SetUniformValue("CausticsColor", m_causticsColor);
	m_sandMaterial->SetUniformValue("CausticsIntensity", m_causticsIntensity);
	m_sandMaterial->SetUniformValue("CausticsOffset", m_causticsOffset);
	m_sandMaterial->SetUniformValue("CausticsScale", m_causticsScale);
	m_sandMaterial->SetUniformValue("CausticsSpeed", m_causticsSpeed);
	m_sandMaterial->SetUniformValue("CausticsThickness", m_causticsThickness);

}

void WaterApplication::InitializeMeshes()
{
	m_planeMesh = std::make_shared<Mesh>();
	CreatePlaneMesh(*m_planeMesh, m_gridX, m_gridY);
	std::cout << "Water mesh submeshes: " << m_planeMesh->GetSubmeshCount() << std::endl;
}


void WaterApplication::InitializeModels()
{
	// Set skybox texture
	m_skyboxTexture = TextureCubemapLoader::LoadTextureShared("models/skybox/sunsetSkybox.png", TextureObject::FormatRGB, TextureObject::InternalFormatSRGB8);

	m_skyboxTexture->Bind();
	float maxLod;
	m_skyboxTexture->GetParameter(TextureObject::ParameterFloat::MaxLod, maxLod);
	TextureCubemapObject::Unbind();

	m_defaultMaterial->SetUniformValue("AmbientColor", glm::vec3(0.25f));

	m_defaultMaterial->SetUniformValue("EnvironmentTexture", m_skyboxTexture);
	m_defaultMaterial->SetUniformValue("EnvironmentMaxLod", maxLod);
	m_defaultMaterial->SetUniformValue("Color", glm::vec3(1.0f));

	// Configure loader
	ModelLoader loader(m_defaultMaterial);

	// Create a new material copy for each submaterial
	loader.SetCreateMaterials(true);

	// Flip vertically textures loaded by the model loader
	loader.GetTexture2DLoader().SetFlipVertical(true);

	// Link vertex properties to attributes
	loader.SetMaterialAttribute(VertexAttribute::Semantic::Position, "VertexPosition");
	loader.SetMaterialAttribute(VertexAttribute::Semantic::Normal, "VertexNormal");
	loader.SetMaterialAttribute(VertexAttribute::Semantic::Tangent, "VertexTangent");
	loader.SetMaterialAttribute(VertexAttribute::Semantic::Bitangent, "VertexBitangent");
	loader.SetMaterialAttribute(VertexAttribute::Semantic::TexCoord0, "VertexTexCoord");

	// Link material properties to uniforms
	loader.SetMaterialProperty(ModelLoader::MaterialProperty::DiffuseColor, "Color");
	loader.SetMaterialProperty(ModelLoader::MaterialProperty::DiffuseTexture, "ColorTexture");
	loader.SetMaterialProperty(ModelLoader::MaterialProperty::NormalTexture, "NormalTexture");
	loader.SetMaterialProperty(ModelLoader::MaterialProperty::SpecularTexture, "SpecularTexture");

	// Load opaque models
	float height = m_waterBaseHeight + 1.0f;

	//Chest
	std::shared_ptr<Model> chestModel = loader.LoadShared("models/treasure_chest/treasure_chest.obj");
	std::shared_ptr<Transform> chestTransform = std::make_shared<Transform>();
	chestTransform->SetScale(glm::vec3(1.0f)); 
	chestTransform->SetTranslation(glm::vec3(10.0f, height, 10.0f));
	m_opaqueScene.AddSceneNode(std::make_shared<SceneModel>("treasure chest", chestModel, chestTransform));

	//Camera
	std::shared_ptr<Model> cameraModel = loader.LoadShared("models/camera/camera.obj");
	std::shared_ptr<Transform> cameraTransform = std::make_shared<Transform>();
	cameraTransform->SetScale(glm::vec3(10.0f)); 
	cameraTransform->SetTranslation(glm::vec3(10.0f, height, 12.0f));
	m_opaqueScene.AddSceneNode(std::make_shared<SceneModel>("camera model", cameraModel, cameraTransform));

	//Tea set
	std::shared_ptr<Model> teaSetModel = loader.LoadShared("models/tea_set/tea_set.obj");
	std::shared_ptr<Transform> teaSetTransform = std::make_shared<Transform>();
	teaSetTransform->SetScale(glm::vec3(2.0f)); 
	teaSetTransform->SetTranslation(glm::vec3(10.0f, height, 8.0f));
	m_opaqueScene.AddSceneNode(std::make_shared<SceneModel>("tea set", teaSetModel, teaSetTransform));

	//Alarm clock
	std::shared_ptr<Model> clockModel = loader.LoadShared("models/alarm_clock/alarm_clock.obj");
	std::shared_ptr<Transform> clockTransform = std::make_shared<Transform>();
	clockTransform->SetScale(glm::vec3(2.0f)); 
	clockTransform->SetTranslation(glm::vec3(10.0f, height, 6.0f));
	m_opaqueScene.AddSceneNode(std::make_shared<SceneModel>("alarm clock", clockModel, clockTransform));

	// Sand plane
	std::shared_ptr<Model> sandModel = std::make_shared<Model>(m_planeMesh);

	m_sandTransform = std::make_shared<Transform>();
	m_sandTransform->SetScale(m_waterScale); 
	m_sandTransform->SetTranslation(glm::vec3(0.0f, m_sandBaseHeight, 0.0f)); 
	sandModel->AddMaterial(m_sandMaterial);

	m_opaqueScene.AddSceneNode(std::make_shared<SceneModel>("sand plane", sandModel, m_sandTransform));

	// Load transparent models

	// Water plane
	std::shared_ptr<Model> waterModel = std::make_shared<Model>(m_planeMesh);

	m_waterTransform = std::make_shared<Transform>();
	m_waterTransform->SetScale(m_waterScale);
	m_waterTransform->SetTranslation(glm::vec3(0.0f, m_waterBaseHeight, 0.0f)); 
	
	waterModel->AddMaterial(m_waterMaterial);

	m_transparentScene.AddSceneNode(std::make_shared<SceneModel>("water plane", waterModel, m_waterTransform));

}

void WaterApplication::InitializeRenderer()
{
	m_renderer.AddRenderPass(std::make_unique<ForwardRenderPass>());
	m_renderer.AddRenderPass(std::make_unique<SkyboxRenderPass>(m_skyboxTexture));

}

void WaterApplication::SetupOffScreenBuffer()
{
	int winW, winH;
	GetMainWindow().GetDimensions(winW, winH);
	m_offscreenWidth = winW / 2; // lower res for offscreen rendering 
	m_offscreenHeight = winW / 2;

	m_offscreenWidth = NextPowerOfTwo(m_offscreenWidth);
	m_offscreenHeight = NextPowerOfTwo(m_offscreenHeight);


	// 2) color texture
	m_offscreenColorTex = std::make_shared<Texture2DObject>();
	m_offscreenColorTex->Bind();

	m_offscreenColorTex->SetParameter(TextureObject::ParameterEnum::MinFilter, GL_LINEAR);
	m_offscreenColorTex->SetParameter(TextureObject::ParameterEnum::MagFilter, GL_LINEAR);

	m_offscreenColorTex->SetImage(
		0,
		m_offscreenWidth,
		m_offscreenHeight,
		TextureObject::Format::FormatRGBA,
		TextureObject::InternalFormat::InternalFormatRGBA8);

	Texture2DObject::Unbind();

	// 3) depth texture
	m_offscreenDepthTex = std::make_shared<Texture2DObject>();
	m_offscreenDepthTex->Bind();

	m_offscreenDepthTex->SetImage(
		0,
		m_offscreenWidth,
		m_offscreenHeight,
		TextureObject::Format::FormatDepth,
		TextureObject::InternalFormat::InternalFormatDepth24);

	Texture2DObject::Unbind();

	// 4) make the FBO and attach
	m_offscreenFBO = std::make_shared<FramebufferObject>();
	m_offscreenFBO->Bind();

	// attach color
	m_offscreenFBO->SetTexture(
		FramebufferObject::Target::Both,
		FramebufferObject::Attachment::Color0,
		*m_offscreenColorTex, 0);

	// attach depth
	m_offscreenFBO->SetTexture(
		FramebufferObject::Target::Both,
		FramebufferObject::Attachment::Depth,
		*m_offscreenDepthTex, 0);

	// set the draw buffers
	std::array<FramebufferObject::Attachment, 1> drawBuffers = { FramebufferObject::Attachment::Color0 };
	m_offscreenFBO->SetDrawBuffers(drawBuffers);

	FramebufferObject::Unbind();
}

void WaterApplication::SetOffScreenCamera(Camera& camera, glm::vec3& originalPosition)
{
	// Get camera position
	originalPosition = camera.ExtractTranslation();

	// Mirror camera position over water plane
	glm::vec3 reflectionPosition = originalPosition;
	reflectionPosition.y = 2.0f * m_waterBaseHeight - originalPosition.y;

	glm::vec3 up, forward, right;
	camera.ExtractVectors(right, up, forward);

	//Invert the pitch of the camera
	forward.y = -forward.y;

	up = glm::normalize(glm::cross(right, forward));
	camera.SetViewMatrix(reflectionPosition, reflectionPosition - forward, up);
}

void WaterApplication::CreatePlaneMesh(Mesh& mesh, unsigned int gridX, unsigned int gridY)
{
	// Define the vertex structure
	struct Vertex
	{
		Vertex() = default;
		Vertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2 texCoord)
			: position(position), normal(normal), texCoord(texCoord) {
		}
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texCoord;
	};

	// Define the vertex format (should match the vertex structure)
	VertexFormat vertexFormat;
	vertexFormat.AddVertexAttribute<float>(3);
	vertexFormat.AddVertexAttribute<float>(3);
	vertexFormat.AddVertexAttribute<float>(2);

	// List of vertices (VBO)
	std::vector<Vertex> vertices;

	// List of indices (EBO)
	std::vector<unsigned int> indices;

	// Grid scale to convert the entire grid to size 1x1
	glm::vec2 scale(1.0f / (gridX - 1), 1.0f / (gridY - 1));

	// Number of columns and rows
	unsigned int columnCount = gridX;
	unsigned int rowCount = gridY;

	// Iterate over each VERTEX
	for (unsigned int j = 0; j < rowCount; ++j)
	{
		for (unsigned int i = 0; i < columnCount; ++i)
		{
			// Vertex data for this vertex only
			glm::vec3 position(i * scale.x, 0.0f, j * scale.y);
			glm::vec3 normal(0.0f, 1.0f, 0.0f);
			glm::vec2 texCoord(i, j);
			vertices.emplace_back(position, normal, texCoord);

			// Index data for quad formed by previous vertices and current
			if (i > 0 && j > 0)
			{
				unsigned int top_right = j * columnCount + i; // Current vertex
				unsigned int top_left = top_right - 1;
				unsigned int bottom_right = top_right - columnCount;
				unsigned int bottom_left = bottom_right - 1;

				//Triangle 1
				indices.push_back(top_left);
				indices.push_back(bottom_right);
				indices.push_back(bottom_left);

				//Triangle 2
				indices.push_back(bottom_right);
				indices.push_back(top_left);
				indices.push_back(top_right);
			}
		}
	}

	mesh.AddSubmesh<Vertex, unsigned int, VertexFormat::LayoutIterator>(Drawcall::Primitive::Triangles, vertices, indices,
		vertexFormat.LayoutBegin(static_cast<int>(vertices.size()), true), vertexFormat.LayoutEnd());
}

void WaterApplication::RenderGUI()
{
	m_imGui.BeginFrame();

	// Draw GUI for scene nodes, using the visitor pattern
	ImGuiSceneVisitor imGuiVisitor(m_imGui, "Scene");

	m_opaqueScene.AcceptVisitor(imGuiVisitor);
	m_transparentScene.AcceptVisitor(imGuiVisitor);
	m_cameraController.DrawGUI(m_imGui);

	if (auto window = m_imGui.UseWindow("Debug"))
	{
		m_offscreenColorTex->Bind();

		// retrieve its GLuint handle
		GLint texHandle = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &texHandle);

		Texture2DObject::Unbind();

		// cast to ImGui’s ImTextureID and draw
		ImTextureID texID = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texHandle));
		ImGui::Text("Scene preview:");
		
		ImVec2 size(m_offscreenWidth / 2, m_offscreenHeight / 2);
		ImVec2 uv0(0, 0);
		ImVec2 uv1(1, 1);
		ImGui::Image(texID, size, uv0, uv1);
	}

	if (auto window = m_imGui.UseWindow("Water window"))
	{
		if (ImGui::CollapsingHeader("Water Color"))
		{
			if (ImGui::SliderFloat("Water Opacity", &m_waterOpacity, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("Opacity", m_waterOpacity);
			}
			if (ImGui::ColorEdit3("Water Trough Color", &m_waterTroughColor[0]))
			{
				m_waterMaterial->SetUniformValue("TroughColor", m_waterTroughColor);
			}
			if (ImGui::ColorEdit3("Water Surface Color", &m_waterSurfaceColor[0]))
			{
				m_waterMaterial->SetUniformValue("SurfaceColor", m_waterSurfaceColor);
			}
			if (ImGui::ColorEdit3("Water Peak Color", &m_waterPeakColor[0]))
			{
				m_waterMaterial->SetUniformValue("PeakColor", m_waterPeakColor);
			}

			ImGui::Separator();

			if (ImGui::SliderFloat("Trough Level", &m_troughLevel, 0.001f, 1.0f))
				m_waterMaterial->SetUniformValue("TroughLevel", m_troughLevel);

			if (ImGui::SliderFloat("Peak Level", &m_peakLevel, 0.001f, 1.0f))
				m_waterMaterial->SetUniformValue("PeakLevel", m_peakLevel);

			ImGui::Separator();

			if (ImGui::SliderFloat("Trough Blend", &m_troughBlend, 0.001f, 0.5f))
				m_waterMaterial->SetUniformValue("TroughBlend", m_troughBlend);

			if (ImGui::SliderFloat("Peak Blend", &m_peakBlend, 0.001f, 0.5f))
				m_waterMaterial->SetUniformValue("PeakBlend", m_peakBlend);

		}


		ImGui::Separator();

		if (ImGui::CollapsingHeader("Wave Reflection"))
		{

			if (ImGui::SliderFloat("Fresnel Power", &m_fresnelPower, 0.0f, 10.0f))
			{
				m_waterMaterial->SetUniformValue("FresnelPower", m_fresnelPower);
			}
			if (ImGui::SliderFloat("Fresnel Strength", &m_fresnelStrength, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("FresnelStrength", m_fresnelStrength);
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Wave Parameters"))
		{
			if (ImGui::SliderFloat("Wave Amplitude", &m_waveAmplitude, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("WaveAmplitude", m_waveAmplitude);
			}
			if (ImGui::SliderFloat("Wave Frequency", &m_waveFrequency, 0.1f, 2.0f))
			{
				m_waterMaterial->SetUniformValue("WaveFrequency", m_waveFrequency);
			}
			if (ImGui::SliderFloat("Wave Persistence", &m_wavePersistence, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("WavePersistence", m_wavePersistence);
			}
			if (ImGui::SliderFloat("Wave Lacunarity", &m_waveLacunarity, 1.0f, 3.0f))
			{
				m_waterMaterial->SetUniformValue("WaveLacunarity", m_waveLacunarity);
			}
			if (ImGui::SliderInt("Wave Octaves", &m_waveOctaves, 1, 15))
			{
				m_waterMaterial->SetUniformValue("WaveOctaves", m_waveOctaves);
			}

			ImGui::Separator();

			if (ImGui::SliderFloat("Wave Speed", &m_waveSpeed, 0.0f, 10.0f))
			{
				m_waterMaterial->SetUniformValue("WaveSpeed", m_waveSpeed);
			}

		}
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Light Caustics Parameters"))
		{
			if (ImGui::ColorEdit3("Caustics Color", &m_causticsColor[0]))
			{
				m_sandMaterial->SetUniformValue("CausticsColor", m_causticsColor);
			}
			if (ImGui::SliderFloat("Caustics Intensity", &m_causticsIntensity, 0.0f, 1.0f))
			{
				m_sandMaterial->SetUniformValue("CausticsIntensity", m_causticsIntensity);
			}
			if (ImGui::SliderFloat("Caustics Offset", &m_causticsOffset, 0.0f, 1.0f))
			{
				m_sandMaterial->SetUniformValue("CausticsOffset", m_causticsOffset);
			}
			if (ImGui::SliderFloat("Caustics Scale", &m_causticsScale, 0.0f, 5.0f))
			{
				m_sandMaterial->SetUniformValue("CausticsScale", m_causticsScale);
			}
			if (ImGui::SliderFloat("Caustics Speed", &m_causticsSpeed, 0.0f, 5.0f))
			{
				m_sandMaterial->SetUniformValue("CausticsSpeed", m_causticsSpeed);
			}
			if (ImGui::SliderFloat("Caustics Thickness", &m_causticsThickness, 0.0f, 1.0f))
			{
				m_sandMaterial->SetUniformValue("CausticsThickness", m_causticsThickness);
			}

		}

	}

	m_imGui.EndFrame();
}



