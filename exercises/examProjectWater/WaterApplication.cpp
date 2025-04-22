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


WaterApplication::WaterApplication()
	: Application(1024, 1024, "Water applicaiton")
	, m_renderer(GetDevice())
	, m_vertexShaderLoader(Shader::Type::VertexShader)
	, m_fragmentShaderLoader(Shader::Type::FragmentShader)
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
	InitializeMaterial();
	InitializeMeshes();
	InitializeModels();
	InitializeRenderer();

	//Enable depth test
	GetDevice().EnableFeature(GL_DEPTH_TEST);
	//GetDevice().DisableFeature(GL_CULL_FACE); 
}

void WaterApplication::Update()
{
	Application::Update();

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
void WaterApplication::InitializeMaterial()
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

	Shader waterVS = m_vertexShaderLoader.Load("shaders/water.vert");
	Shader waterFS = m_fragmentShaderLoader.Load("shaders/water.frag");
	std::shared_ptr<ShaderProgram> waterShaderProgram = std::make_shared<ShaderProgram>();
	waterShaderProgram->Build(waterVS, waterFS);

	ShaderProgram::Location waterWorldMatrixLocation = waterShaderProgram->GetUniformLocation("WorldMatrix"); 
	ShaderProgram::Location waterViewProjMatrixLocation = waterShaderProgram->GetUniformLocation("ViewProjMatrix"); 

	m_renderer.RegisterShaderProgram(waterShaderProgram, 
		[=](const ShaderProgram& shaderProgram, const glm::mat4& worldMatrix, const Camera& camera, bool /*cameraChanged*/) 
		{
			shaderProgram.SetUniform(waterWorldMatrixLocation, worldMatrix); 
			shaderProgram.SetUniform(waterViewProjMatrixLocation, camera.GetViewProjectionMatrix()); 
		},
		m_renderer.GetDefaultUpdateLightsFunction(*waterShaderProgram) 
	);

	// Water material
	m_waterMaterial = std::make_shared<Material>(waterShaderProgram);
	m_waterMaterial->SetUniformValue("Color", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // Bright red, fully opaque
	//m_waterMaterial->SetUniformValue("ColorTexture", m_waterTexture);
	//m_waterMaterial->SetUniformValue("ColorTextureScale", glm::vec2(0.0625f));
	m_waterMaterial->SetBlendEquation(Material::BlendEquation::Add);
	m_waterMaterial->SetBlendParams(Material::BlendParam::SourceAlpha, Material::BlendParam::OneMinusSourceAlpha);
}

void WaterApplication::InitializeMeshes()
{
	m_waterMesh = std::make_shared<Mesh>();
	CreateTerrainMesh(*m_waterMesh, 128, 128);
	std::cout << "Water mesh submeshes: " << m_waterMesh->GetSubmeshCount() << std::endl;
}

void WaterApplication::InitializeModels()
{
	m_skyboxTexture = TextureCubemapLoader::LoadTextureShared("models/skybox/defaultCubemap.png", TextureObject::FormatRGB, TextureObject::InternalFormatSRGB8);

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

	// Load models
	std::shared_ptr<Model> chestModel = loader.LoadShared("models/treasure_chest/treasure_chest.obj");
	m_scene.AddSceneNode(std::make_shared<SceneModel>("treasure chest", chestModel));

	//std::shared_ptr<Model> cameraModel = loader.LoadShared("models/camera/camera.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("camera model", cameraModel));

	//std::shared_ptr<Model> teaSetModel = loader.LoadShared("models/tea_set/tea_set.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("tea set", teaSetModel));

	//std::shared_ptr<Model> clockModel = loader.LoadShared("models/alarm_clock/alarm_clock.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("alarm clock", clockModel));

	std::shared_ptr<Model> waterModel = std::make_shared<Model>(m_waterMesh);

	std::shared_ptr<Transform> waterTransform = std::make_shared<Transform>();
	waterTransform->SetScale(glm::vec3(10.0f)); // Make it larger
	waterTransform->SetTranslation(glm::vec3(0.0f, -0.15f, 0.0f)); // Lower it slightly if needed
	waterModel->AddMaterial(m_waterMaterial);

	m_scene.AddSceneNode(std::make_shared<SceneModel>("water model", waterModel, waterTransform));
}

void WaterApplication::CreateTerrainMesh(Mesh& mesh, unsigned int gridX, unsigned int gridY)
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
	m_renderer.AddRenderPass(std::make_unique<ForwardRenderPass>());
	m_renderer.AddRenderPass(std::make_unique<SkyboxRenderPass>(m_skyboxTexture));
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

	}

	m_imGui.EndFrame();
}



