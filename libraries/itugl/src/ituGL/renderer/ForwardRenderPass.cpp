#include <ituGL/renderer/ForwardRenderPass.h>

#include <ituGL/camera/Camera.h>
#include <ituGL/shader/Material.h>
#include <ituGL/geometry/VertexArrayObject.h>
#include <ituGL/renderer/Renderer.h>
#include <iostream>

ForwardRenderPass::ForwardRenderPass()
    : ForwardRenderPass(0)
{
}

ForwardRenderPass::ForwardRenderPass(int drawcallCollectionIndex)
    : m_drawcallCollectionIndex(drawcallCollectionIndex)
{
}
//
//void ForwardRenderPass::Render()
//{
//    Renderer& renderer = GetRenderer();
//
//    const Camera& camera = renderer.GetCurrentCamera();
//    const auto& lights = renderer.GetLights();
//    const auto& drawcallCollection = renderer.GetDrawcalls(m_drawcallCollectionIndex);
//
//    // for all drawcalls
//    for (const Renderer::DrawcallInfo& drawcallInfo : drawcallCollection)
//    {
//        // Prepare drawcall states
//        renderer.PrepareDrawcall(drawcallInfo);
//
//        std::shared_ptr<const ShaderProgram> shaderProgram = drawcallInfo.material.GetShaderProgram();
//
//        //for all lights
//        bool first = true;
//        unsigned int lightIndex = 0;
//        while (renderer.UpdateLights(shaderProgram, lights, lightIndex))
//        {
//            // Set the renderstates
//            renderer.SetLightingRenderStates(first);
//
//            // Draw
//            drawcallInfo.drawcall.Draw();
//
//            first = false;
//        }
//    }
//}

void ForwardRenderPass::Render()
{
    Renderer& renderer = GetRenderer();

    const Camera& camera = renderer.GetCurrentCamera();
    const auto& lights = renderer.GetLights();
    const auto& drawcallCollection = renderer.GetDrawcalls(m_drawcallCollectionIndex);

    /*std::cout << "ForwardRenderPass (index " << m_drawcallCollectionIndex << ") has "
        << drawcallCollection.size() << " drawcalls." << std::endl;*/

    // Tell the renderer which kind of pass this is
    bool isTransparentPass = (m_drawcallCollectionIndex == 1);

    for (const Renderer::DrawcallInfo& drawcallInfo : drawcallCollection)
    {
        // Prepare drawcall states
        renderer.PrepareDrawcall(drawcallInfo);

        std::shared_ptr<const ShaderProgram> shaderProgram = drawcallInfo.material.GetShaderProgram();

        unsigned int lightIndex = 0;
        bool first = true;
        while (renderer.UpdateLights(shaderProgram, lights, lightIndex))
        {
            renderer.SetLightingRenderStates(isTransparentPass, first);
            drawcallInfo.drawcall.Draw();
            first = false;
        }
    }
}
