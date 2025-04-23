#include <ituGL/scene/RendererSceneVisitor.h>

#include <ituGL/renderer/Renderer.h>
#include <ituGL/scene/SceneCamera.h>
#include <ituGL/scene/SceneLight.h>
#include <ituGL/scene/SceneModel.h>
#include <ituGL/scene/Transform.h>

#include <ituGL/geometry/Model.h>
#include <ituGL/shader/Material.h>
#include <iostream>

RendererSceneVisitor::RendererSceneVisitor(Renderer& renderer) : m_renderer(renderer)
{
}

void RendererSceneVisitor::VisitCamera(SceneCamera& sceneCamera)
{
    assert(!m_renderer.HasCamera()); // Currently, only one camera per scene supported
    m_renderer.SetCurrentCamera(*sceneCamera.GetCamera());
}

void RendererSceneVisitor::VisitLight(SceneLight& sceneLight)
{
    m_renderer.AddLight(*sceneLight.GetLight());
}

void RendererSceneVisitor::VisitModel(SceneModel& sceneModel)
{
    assert(sceneModel.GetTransform());
    std::shared_ptr<Model> model = sceneModel.GetModel();
    glm::mat4 worldMatrix = sceneModel.GetTransform()->GetTransformMatrix();

    bool isTransparent = false;

    for (unsigned int i = 0; i < model->GetMaterialCount(); ++i)
    {
        if (model->GetMaterial(i).IsTransparent())
        {
            isTransparent = true;
            break;
        }
    }
    /*std::cout << "Model '" << sceneModel.GetName() << "' is "
        << (isTransparent ? "transparent" : "opaque") << std::endl;*/
    std::cout << "Visiting model: " << sceneModel.GetName() << std::endl;
    int collectionIndex = isTransparent ? 1 : 0;
    m_renderer.AddModelToCollection(*model, worldMatrix, collectionIndex);
}


//void RendererSceneVisitor::VisitModel(SceneModel& sceneModel)
//{
//    assert(sceneModel.GetTransform());
//    m_renderer.AddModel(*sceneModel.GetModel(), sceneModel.GetTransform()->GetTransformMatrix());
//}
