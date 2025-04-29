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

#include <ituGL/renderer/SkyboxRenderPass.h>
#include <ituGL/renderer/ForwardRenderPass.h>
#include <ituGL/scene/RendererSceneVisitor.h>
#include <ituGL/scene/Transform.h>
#include <ituGL/scene/ImGuiSceneVisitor.h>
#include <imgui.h>

#include <glm/gtx/transform.hpp>  // for matrix transformations

#include <numbers>
#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>
#include <iostream>


WaterApplication::WaterApplication(unsigned int x, unsigned int y)
	: Application(1920, 1080, "Water applicaiton")
	, m_renderer(GetDevice())
	, m_vertexShaderLoader(Shader::Type::VertexShader)
	, m_fragmentShaderLoader(Shader::Type::FragmentShader)
	, m_gridX(x)
	, m_gridY(y)

	// Water parameters
	, m_waterOpacity(0.8f)
	, m_waterTroughColor(0.0f, 0.1f, 0.3f, 1.0f)  // Deep blue color
	, m_waterSurfaceColor(0.0f, 0.4f, 0.6f, 1.0f) // Green color
	, m_waterPeakColor(0.8f, 0.9f, 1.0f, 1.0f) // White color
	, m_troughThreshold(-0.01f)
	, m_troughTransition(0.15f)
	, m_peakThreshold(0.08f)
	, m_peakTransition(0.05f)

	, m_fresnelPower(0.5f)
	, m_fresnelStrength(0.8f)

	//Wave parameters
	, m_waveAmplitude(0.025f)
	, m_waveFrequency(1.07f)
	, m_wavePersistence(0.3f)
	, m_waveLacunarity(2.18f)
	, m_waveOctaves(8)
	, m_waveSpeed(0.5f)

{
}

struct Vertex
{
	glm::vec3 position;
	glm::vec2 texCoord;
	glm::vec3 normal;
};

float PerlinHeight(float x, float y)
{
	return stb_perlin_fbm_noise3(x * 2, y * 2, 0.0f, 1.9f, 0.5f, 8) * 0.5f;
}

void WaterApplication::Initialize()
{
	Application::Initialize();

	// Initialize DearImGUI
	m_imGui.Initialize(GetMainWindow());

	InitializeCamera();
	InitializeLights();
	InitializeDefaultMaterial();
	InitializeWaterMaterial();
	InitializeSandMaterial();
	InitializeMeshes();
	InitializeModels();
	InitializeRenderer();

	//GetDevice().DisableFeature(GL_CULL_FACE); 
	//GetDevice().SetWireframeEnabled(true);
}

void WaterApplication::Update()
{
	Application::Update();

	const Window& window = GetMainWindow();
	int width, height;
	window.GetDimensions(width, height);
	float aspectRatio = (height != 0) ? static_cast<float>(width) / height : 1.0f; // Default fallback

	if (m_cameraController.GetCamera())
	{
		Camera& camera = *m_cameraController.GetCamera()->GetCamera();
		camera.SetPerspectiveProjectionMatrix(static_cast<float>(std::numbers::pi) * 0.5f, aspectRatio, 0.1f, 100.0f);
	}

	// Update camera controller
	m_cameraController.Update(GetMainWindow(), GetDeltaTime());

	// Add the scene nodes to the renderer
	RendererSceneVisitor rendererSceneVisitor(m_renderer);
	m_scene.AcceptVisitor(rendererSceneVisitor);

}

void WaterApplication::Render()
{
	Application::Render();

	GetDevice().Clear(true, Color(0.0f, 0.0f, 0.0f, 1.0f), true, 1.0f);

	// Render the scene
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
	camera->SetViewMatrix(glm::vec3(-1, 1, 1), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	camera->SetPerspectiveProjectionMatrix(1.0f, 1.0f, 0.1f, 100.0f);

	// Create a scene node for the camera
	std::shared_ptr<SceneCamera> sceneCamera = std::make_shared<SceneCamera>("camera", camera);

	// Add the camera node to the scene
	m_scene.AddSceneNode(sceneCamera);

	// Set the camera scene node to be controlled by the camera controller
	m_cameraController.SetCamera(sceneCamera);
}

void WaterApplication::InitializeLights()
{
	// Create a directional light and add it to the scene
	std::shared_ptr<DirectionalLight> directionalLight = std::make_shared<DirectionalLight>();
	directionalLight->SetDirection(glm::vec3(-0.3f, -1.0f, -0.3f)); // It will be normalized inside the function
	directionalLight->SetIntensity(3.0f);
	m_scene.AddSceneNode(std::make_shared<SceneLight>("directional light", directionalLight));

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

	//Shader waterVS = m_vertexShaderLoader.Load("shaders/water.vert");
	//Shader waterFS = m_fragmentShaderLoader.Load("shaders/water.frag");
	std::shared_ptr<ShaderProgram> waterShaderProgram = std::make_shared<ShaderProgram>();
	waterShaderProgram->Build(waterVS, waterFS);

	ShaderProgram::Location cameraPositionLocation = waterShaderProgram->GetUniformLocation("CameraPosition");
	ShaderProgram::Location worldMatrixLocation = waterShaderProgram->GetUniformLocation("WorldMatrix");
	ShaderProgram::Location viewProjMatrixLocation = waterShaderProgram->GetUniformLocation("ViewProjMatrix");
	ShaderProgram::Location timeLocation = waterShaderProgram->GetUniformLocation("Time");

	m_renderer.RegisterShaderProgram(waterShaderProgram,
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool /*cameraChanged*/)
		{
			shaderProgram.SetUniform(worldMatrixLocation, worldMatrix);
			shaderProgram.SetUniform(viewProjMatrixLocation, camera.GetViewProjectionMatrix());
			shaderProgram.SetUniform(timeLocation, static_cast<float>(GetTime())); // Pass the time to the shader
			shaderProgram.SetUniform(cameraPositionLocation, camera.ExtractTranslation()); 
		},
		m_renderer.GetDefaultUpdateLightsFunction(*waterShaderProgram)
	);

	ShaderUniformCollection::NameSet filteredUniforms;
	filteredUniforms.insert("WorldMatrix");
	filteredUniforms.insert("ViewProjMatrix");
	filteredUniforms.insert("Time");
	filteredUniforms.insert("CameraPosition");

	m_waterMaterial = std::make_shared<Material>(waterShaderProgram, filteredUniforms);

	// Water vertex shader uniforms
	m_waterMaterial->SetUniformValue("Opacity", m_waterOpacity);
	m_waterMaterial->SetUniformValue("TroughColor", m_waterTroughColor);
	m_waterMaterial->SetUniformValue("SurfaceColor", m_waterSurfaceColor);
	m_waterMaterial->SetUniformValue("PeakColor", m_waterPeakColor);
	m_waterMaterial->SetUniformValue("TroughThreshold", m_troughThreshold);
	m_waterMaterial->SetUniformValue("TroughTransition", m_troughTransition);
	m_waterMaterial->SetUniformValue("PeakThreshold", m_peakThreshold);
	m_waterMaterial->SetUniformValue("PeakTransition", m_peakTransition);

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


	m_waterMaterial->SetBlendEquation(Material::BlendEquation::Add);
	//m_waterMaterial->SetBlendParams(Material::BlendParam::SourceAlpha, Material::BlendParam::OneMinusSourceAlpha);
	m_waterMaterial->SetBlendParams(
		Material::BlendParam::SourceAlpha,           // Source color factor
		Material::BlendParam::OneMinusSourceAlpha,    // Destination color factor
		Material::BlendParam::SourceAlpha,            // Source alpha factor
		Material::BlendParam::OneMinusSourceAlpha     // Destination alpha factor
	);
	m_waterMaterial->SetDepthWrite(false);
}

void WaterApplication::InitializeSandMaterial()
{
	Shader sandVS = m_vertexShaderLoader.Load("shaders/sand.vert");
	Shader sandFS = m_fragmentShaderLoader.Load("shaders/sand.frag");
	std::shared_ptr<ShaderProgram> sandShaderProgram = std::make_shared<ShaderProgram>();
	sandShaderProgram->Build(sandVS, sandFS);

	ShaderProgram::Location worldMatrixLocation = sandShaderProgram->GetUniformLocation("WorldMatrix");
	ShaderProgram::Location viewProjMatrixLocation = sandShaderProgram->GetUniformLocation("ViewProjMatrix");

	m_renderer.RegisterShaderProgram(sandShaderProgram,
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool /*cameraChanged*/)
		{
			shaderProgram.SetUniform(worldMatrixLocation, worldMatrix);
			shaderProgram.SetUniform(viewProjMatrixLocation, camera.GetViewProjectionMatrix());
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
	m_sandMaterial->SetUniformValue("ColorTextureScale", glm::vec2(0.01f));

}


void WaterApplication::InitializeMeshes()
{
	m_planeMesh = std::make_shared<Mesh>();
	CreatePlaneMesh(*m_planeMesh, m_gridX, m_gridY);
	std::cout << "Water mesh submeshes: " << m_planeMesh->GetSubmeshCount() << std::endl;
}

void WaterApplication::InitializeModels()
{
	m_skyboxTexture = TextureCubemapLoader::LoadTextureShared("models/skybox/skyCubemap.png", TextureObject::FormatRGB, TextureObject::InternalFormatSRGB8);

	m_skyboxTexture->Bind();
	float maxLod;
	m_skyboxTexture->GetParameter(TextureObject::ParameterFloat::MaxLod, maxLod);
	TextureCubemapObject::Unbind();

	m_defaultMaterial->SetUniformValue("AmbientColor", glm::vec3(0.25f));

	m_defaultMaterial->SetUniformValue("EnvironmentTexture", m_skyboxTexture);
	m_defaultMaterial->SetUniformValue("EnvironmentMaxLod", maxLod);
	m_defaultMaterial->SetUniformValue("Color", glm::vec3(1.0f));

	//m_waterMaterial->SetUniformValue("EnvironmentTexture", m_skyboxTexture);
	//m_waterMaterial->SetUniformValue("EnvironmentMaxLod", maxLod);

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

	// Load models
	std::shared_ptr<Model> chestModel = loader.LoadShared("models/treasure_chest/treasure_chest.obj");
	m_scene.AddSceneNode(std::make_shared<SceneModel>("treasure chest", chestModel));

	//std::shared_ptr<Model> cameraModel = loader.LoadShared("models/camera/camera.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("camera model", cameraModel));

	//std::shared_ptr<Model> teaSetModel = loader.LoadShared("models/tea_set/tea_set.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("tea set", teaSetModel));

	//std::shared_ptr<Model> clockModel = loader.LoadShared("models/alarm_clock/alarm_clock.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("alarm clock", clockModel));


	std::shared_ptr<Model> sandModel = std::make_shared<Model>(m_planeMesh);

	std::shared_ptr<Transform> sandTransform = std::make_shared<Transform>();
	sandTransform->SetScale(glm::vec3(5.0f)); // Make it larger
	sandTransform->SetTranslation(glm::vec3(0.0f, -1.0f, 0.0f)); // Lower it slightly if needed
	sandModel->AddMaterial(m_sandMaterial);

	m_scene.AddSceneNode(std::make_shared<SceneModel>("sand plane", sandModel, sandTransform));


	std::shared_ptr<Model> waterModel = std::make_shared<Model>(m_planeMesh);

	std::shared_ptr<Transform> waterTransform = std::make_shared<Transform>();
	waterTransform->SetScale(glm::vec3(5.0f));
	waterTransform->SetTranslation(glm::vec3(0.0f, -0.15f, 0.0f)); // Lower it slightly if needed
	waterModel->AddMaterial(m_waterMaterial);

	m_scene.AddSceneNode(std::make_shared<SceneModel>("water plane", waterModel, waterTransform));



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
		vertexFormat.LayoutBegin(static_cast<int>(vertices.size()), true /* interleaved */), vertexFormat.LayoutEnd());
}


void WaterApplication::InitializeRenderer()
{
	m_renderer.AddRenderPass(std::make_unique<SkyboxRenderPass>(m_skyboxTexture));
	m_renderer.AddRenderPass(std::make_unique<ForwardRenderPass>(0));

}

void WaterApplication::RenderGUI()
{
	m_imGui.BeginFrame();

	// Draw GUI for scene nodes, using the visitor pattern
	ImGuiSceneVisitor imGuiVisitor(m_imGui, "Scene");
	m_scene.AcceptVisitor(imGuiVisitor);

	// Draw GUI for camera controller
	m_cameraController.DrawGUI(m_imGui);

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
				m_waterMaterial->SetUniformValue("DeepColor", m_waterTroughColor);
			}
			if (ImGui::ColorEdit3("Water Surface Color", &m_waterSurfaceColor[0]))
			{
				m_waterMaterial->SetUniformValue("ShallowColor", m_waterSurfaceColor);
			}
			if (ImGui::ColorEdit3("Water Peak Color", &m_waterPeakColor[0]))
			{
				m_waterMaterial->SetUniformValue("PeakColor", m_waterPeakColor);
			}
			if (ImGui::SliderFloat("Trough Threshold", &m_troughThreshold, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("TroughThreshold", m_troughThreshold);
			}
			if (ImGui::SliderFloat("Trough Transition", &m_troughTransition, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("TroughTransition", m_troughTransition);
			}
			if (ImGui::SliderFloat("Peak Threshold", &m_peakThreshold, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("PeakThreshold", m_peakThreshold);
			}
			if (ImGui::SliderFloat("Peak Transition", &m_peakTransition, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("PeakTransition", m_peakTransition);
			}
			if (ImGui::SliderFloat("Fresnel Power", &m_fresnelPower, 0.0f, 10.0f))
			{
				m_waterMaterial->SetUniformValue("FresnelPower", m_fresnelPower);
			}
			if (ImGui::SliderFloat("Fresnel Strength", &m_fresnelStrength, 0.0f, 1.0f))
			{
				m_waterMaterial->SetUniformValue("FresnelStrength", m_fresnelStrength);
			}
		}
		if (ImGui::CollapsingHeader("Wave Parameters"))
		{
			if (ImGui::SliderFloat("Wave Amplitude", &m_waveAmplitude, 0.0f, 0.2f))
			{
				m_waterMaterial->SetUniformValue("WaveAmplitude", m_waveAmplitude);
			}
			if (ImGui::SliderFloat("Wave Frequency", &m_waveFrequency, 0.1f, 10.0f))
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
			if (ImGui::SliderFloat("Wave Speed", &m_waveSpeed, 0.0f, 10.0f))
			{
				m_waterMaterial->SetUniformValue("WaveSpeed", m_waveSpeed);
			}

		}
	}

	m_imGui.EndFrame();
}



