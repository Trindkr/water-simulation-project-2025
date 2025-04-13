#include "WaterApplication.h"

#include <ituGL/asset/TextureCubemapLoader.h>
#include <ituGL/asset/ShaderLoader.h>
#include <ituGL/asset/ModelLoader.h>

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

#include <ituGL/scene/ImGuiSceneVisitor.h>
#include <imgui.h>

#include <ituGL/geometry/Mesh.h>
#include <ituGL/geometry/VertexFormat.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#define STB_PERLIN_IMPLEMENTATION
#include <stb_perlin.h>

WaterApplication::WaterApplication()
	: Application(1024, 1024, "Water applicaiton")
	, m_renderer(GetDevice())

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
	InitializeModels();
	InitializeRenderer();
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

	/*std::shared_ptr<Mesh> mesh = CreatePlaneMesh(128, 128, false, 10.0f, 10.0f);
	std::shared_ptr<Model> planeModel = std::make_shared<Model>(mesh);
	planeModel->AddMaterial(m_defaultMaterial); 
	
	m_scene.AddSceneNode(std::make_shared<SceneModel>("plane", planeModel));*/
	
	std::shared_ptr<Model> cubeModel = CreateCubeModel(); 
	m_scene.AddSceneNode(std::make_shared<SceneModel>("cube", cubeModel));



	//std::shared_ptr<Model> cameraModel = loader.LoadShared("models/camera/camera.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("camera model", cameraModel));

	//std::shared_ptr<Model> teaSetModel = loader.LoadShared("models/tea_set/tea_set.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("tea set", teaSetModel));

	//std::shared_ptr<Model> clockModel = loader.LoadShared("models/alarm_clock/alarm_clock.obj");
	//m_scene.AddSceneNode(std::make_shared<SceneModel>("alarm clock", clockModel));
}
std::shared_ptr<Model> WaterApplication::CreateCubeModel()
{
	struct CubeVertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 texcoord;
	};

	std::vector<CubeVertex> cubeVertices = {
		// Front face
		{{-0.5f, -0.5f,  0.5f}, {0, 0, 1}, {0, 0}},
		{{ 0.5f, -0.5f,  0.5f}, {0, 0, 1}, {1, 0}},
		{{ 0.5f,  0.5f,  0.5f}, {0, 0, 1}, {1, 1}},
		{{-0.5f,  0.5f,  0.5f}, {0, 0, 1}, {0, 1}},
		// Back face
		{{ 0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
		{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
		{{-0.5f,  0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
		{{ 0.5f,  0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
		// Left face
		{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
		{{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 0}},
		{{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 1}},
		{{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
		// Right face
		{{ 0.5f, -0.5f,  0.5f}, {1, 0, 0}, {0, 0}},
		{{ 0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
		{{ 0.5f,  0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
		{{ 0.5f,  0.5f,  0.5f}, {1, 0, 0}, {0, 1}},
		// Top face
		{{-0.5f,  0.5f,  0.5f}, {0, 1, 0}, {0, 0}},
		{{ 0.5f,  0.5f,  0.5f}, {0, 1, 0}, {1, 0}},
		{{ 0.5f,  0.5f, -0.5f}, {0, 1, 0}, {1, 1}},
		{{-0.5f,  0.5f, -0.5f}, {0, 1, 0}, {0, 1}},
		// Bottom face
		{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 0}},
		{{ 0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0}},
		{{ 0.5f, -0.5f,  0.5f}, {0, -1, 0}, {1, 1}},
		{{-0.5f, -0.5f,  0.5f}, {0, -1, 0}, {0, 1}},
	};

	std::vector<unsigned int> cubeIndices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		12, 13, 14, 14, 15, 12,
		16, 17, 18, 18, 19, 16,
		20, 21, 22, 22, 23, 20,
	};

	VertexBufferObject vbo;
	vbo.Bind();
	vbo.AllocateData(std::span(cubeVertices));

	ElementBufferObject ebo;
	ebo.Bind();
	ebo.AllocateData(std::span(cubeIndices));

	VertexArrayObject vao;
	vao.Bind();

	VertexFormat format;
	format.AddVertexAttribute(Data::Type::Float, 3, false, VertexAttribute::Semantic::Position);
	format.AddVertexAttribute(Data::Type::Float, 3, false, VertexAttribute::Semantic::Normal);
	format.AddVertexAttribute(Data::Type::Float, 2, false, VertexAttribute::Semantic::TexCoord0);

	unsigned int location = 0;
	for (auto it = format.LayoutBegin(static_cast<int>(std::size(cubeVertices)), true); it != format.LayoutEnd(); it++)
	{
		vao.SetAttribute(location, it->GetAttribute(), it->GetOffset(), it->GetStride());
		location += it->GetAttribute().GetLocationSize();
	}

	ebo.Bind();

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
	unsigned int vaoIndex = mesh->AddVertexArray();
	mesh->AddSubmesh(vaoIndex, Drawcall::Primitive::Triangles, 0, static_cast<GLsizei>(std::size(cubeIndices)), Data::Type::UInt);

	std::shared_ptr<Model> model = std::make_shared<Model>(mesh);
	model->AddMaterial(m_defaultMaterial);

	return model;
}

std::shared_ptr<Mesh> WaterApplication::CreatePlaneMesh(int vertexWidth, int vertexHeight, bool generateTerrain, float worldWidth, float worldHeight)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	int colCount = vertexWidth;
	int rowCount = vertexHeight;

	float startX = -worldWidth * 0.5f;
	float startZ = -worldHeight * 0.5f;

	for (int j = 0; j < rowCount; ++j)
	{
		for (int i = 0; i < colCount; ++i)
		{
			float fx = static_cast<float>(i) / (colCount - 1);
			float fz = static_cast<float>(j) / (rowCount - 1);

			float x = startX + fx * worldWidth;
			float z = startZ + fz * worldHeight;
			float y = generateTerrain ? PerlinHeight(x, z) : 0.0f;

			Vertex v;
			v.position = glm::vec3(x, y, z);
			v.texCoord = glm::vec2(fx, fz);
			v.normal = glm::vec3(0.0f, 1.0f, 0.0f); // placeholder

			vertices.push_back(v);
		}
	}

	// Indices (same as before)
	for (int j = 0; j < rowCount - 1; ++j)
	{
		for (int i = 0; i < colCount - 1; ++i)
		{
			unsigned int topLeft = j * colCount + i;
			unsigned int topRight = topLeft + 1;
			unsigned int bottomLeft = topLeft + colCount;
			unsigned int bottomRight = bottomLeft + 1;

			indices.push_back(topLeft);
			indices.push_back(bottomLeft);
			indices.push_back(topRight);

			indices.push_back(topRight);
			indices.push_back(bottomLeft);
			indices.push_back(bottomRight);
		}
	}

	// Optional normal generation (copied from your terrain version)
	if (generateTerrain)
	{
		for (int j = 0; j < rowCount; ++j)
		{
			for (int i = 0; i < colCount; ++i)
			{
				unsigned int index = j * colCount + i;

				int prevX = i > 0 ? index - 1 : index;
				int nextX = i < colCount - 1 ? index + 1 : index;
				float dx = vertices[nextX].position.y - vertices[prevX].position.y;
				float deltaX = vertices[nextX].position.x - vertices[prevX].position.x;

				int prevZ = j > 0 ? index - colCount : index;
				int nextZ = j < rowCount - 1 ? index + colCount : index;
				float dz = vertices[nextZ].position.y - vertices[prevZ].position.y;
				float deltaZ = vertices[nextZ].position.z - vertices[prevZ].position.z;

				glm::vec3 normal = glm::normalize(glm::vec3(dx / deltaX, 1.0f, dz / deltaZ));
				vertices[index].normal = normal;
			}
		}
	}

	// Create and upload vertex buffer
	VertexBufferObject vbo;
	vbo.Bind();
	vbo.AllocateData(std::span(vertices));

	// Create and upload index buffer
	ElementBufferObject ebo;
	ebo.Bind();
	ebo.AllocateData(std::span(indices));

	// Create and bind VAO
	VertexArrayObject vao;
	vao.Bind();

	// Set up vertex attributes
	VertexFormat vertexFormat;
	vertexFormat.AddVertexAttribute(Data::Type::Float, 3, false, VertexAttribute::Semantic::Position);
	vertexFormat.AddVertexAttribute(Data::Type::Float, 2, false, VertexAttribute::Semantic::TexCoord0);

	unsigned int location = 0;
	Mesh::SemanticMap semantics = {
		{ VertexAttribute::Semantic::Position, 0 },
		{ VertexAttribute::Semantic::TexCoord0, 1 }
	};

	if (generateTerrain)
	{
		semantics[VertexAttribute::Semantic::Normal] = 2;
	}

	for (auto it = vertexFormat.LayoutBegin(static_cast<int>(vertices.size()), true);
		it != vertexFormat.LayoutEnd(); it++)
	{
		vao.SetAttribute(location, it->GetAttribute(), it->GetOffset(), it->GetStride());
		location += it->GetAttribute().GetLocationSize();
	}

	// Rebind index buffer (important in OpenGL to store it in the VAO)
	ebo.Bind();

	// Create the Mesh and add VAO + Submesh
	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();

	// Register VAO and add submesh with draw call
	unsigned int vaoIndex = mesh->AddVertexArray();
	mesh->AddSubmesh(vaoIndex, Drawcall::Primitive::Triangles, 0,
		static_cast<GLsizei>(indices.size()), Data::Type::UInt);

	return mesh;
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



