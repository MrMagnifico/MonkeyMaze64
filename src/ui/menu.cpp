#include "menu.h"

#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <nativefiledialog/nfd.h>
DISABLE_WARNINGS_POP()

#include <filesystem>
#include <iostream>

Menu::Menu(Scene& scene, RenderConfig& renderConfig, LightManager& lightManager) : 
    m_scene(scene),
    m_renderConfig(renderConfig),
    m_lightManager(lightManager) {}

void Menu::draw() {
    ImGui::Begin("Debug Controls");
    ImGui::BeginTabBar("Categories");
    
    drawCameraTab();
    drawMeshTab();
    drawLightTab();

    ImGui::EndTabBar();
    ImGui::End();
}

void Menu::drawCameraTab() {
    if (ImGui::BeginTabItem("Camera")) {
        ImGui::DragFloat("Movement Speed", &m_renderConfig.moveSpeed, 0.001f, 0.01f, 0.09f);
        ImGui::DragFloat("Look Speed", &m_renderConfig.lookSpeed, 0.0001f, 0.0005f, 0.0050f);
        ImGui::DragFloat("FOV (Vertical)", &m_renderConfig.verticalFOV, 1.0f, 30.0f, 180.0f);
        ImGui::DragFloat("Zoomed FOV (Vertical)", &m_renderConfig.zoomedVerticalFOV, 1.0f, 20.0f, 120.0f);
        ImGui::Checkbox("Constrain Vertical Movement", &m_renderConfig.constrainVertical);
        ImGui::EndTabItem();
    }
}

void Menu::addMesh() {
    nfdchar_t *outPath = NULL;
    nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath );
        
    if (result == NFD_OKAY) {
        std::filesystem::path objPath(outPath);
        m_scene.addMesh(objPath);
        free(outPath);
    } else if (result == NFD_CANCEL) { std::cout << "Model loading cancelled" << std::endl; }
      else { std::cerr << "Model loading error" << std::endl; }
}

void Menu::drawMeshTab() {
    if (ImGui::BeginTabItem("Meshes")) {
        // Add / remove controls
        if (ImGui::Button("Add")) { addMesh(); }
        if (ImGui::Button("Remove selected")) {
            if (selectedMesh < m_scene.numMeshes()) {
                m_scene.removeMesh(selectedMesh);
                selectedMesh = 0U;
            }
        }
        ImGui::NewLine();

        // Selection controls
        std::vector<std::string> options;
        for (size_t meshIdx = 0U; meshIdx < m_scene.numMeshes(); meshIdx++) { options.push_back("Mesh " + std::to_string(meshIdx + 1)); }
        std::vector<const char*> optionsPointers;
        std::transform(std::begin(options), std::end(options), std::back_inserter(optionsPointers),
            [](const auto& str) { return str.c_str(); });
        ImGui::Combo("Selected mesh", (int*) (&selectedMesh), optionsPointers.data(), static_cast<int>(optionsPointers.size()));

        // Selected mesh controls
        if (m_scene.numMeshes() > 0U) {
            ImGui::DragFloat3("Scale", glm::value_ptr(m_scene.transformParams[selectedMesh].scale), 0.05f);
            ImGui::DragFloat3("Rotate", glm::value_ptr(m_scene.transformParams[selectedMesh].rotate), 1.0f, 0.0f, 360.0f);
            ImGui::DragFloat3("Translate", glm::value_ptr(m_scene.transformParams[selectedMesh].translate), 0.05f);
        }

        ImGui::EndTabItem();
    }
}

void Menu::drawPointLightControls() {
    // Add / remove controls
    if (ImGui::Button("Add")) { m_lightManager.addPointLight({ glm::vec4(0.0f, 0.0f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f) }); }
    if (ImGui::Button("Remove selected")) {
        if (selectedPointLight < m_lightManager.numPointLights()) {
            m_lightManager.removePointLight(selectedPointLight);
            selectedPointLight = 0U;
        }
    }
    ImGui::NewLine();

    // Selection controls
    std::vector<std::string> options;
    for (size_t pointLightIdx = 0U; pointLightIdx < m_lightManager.numPointLights(); pointLightIdx++) { options.push_back("Point light " + std::to_string(pointLightIdx + 1)); }
    std::vector<const char*> optionsPointers;
    std::transform(std::begin(options), std::end(options), std::back_inserter(optionsPointers),
        [](const auto& str) { return str.c_str(); });
    ImGui::Combo("Selected point light", (int*) (&selectedPointLight), optionsPointers.data(), static_cast<int>(optionsPointers.size()));

    // Selected point light controls
    if (m_lightManager.numPointLights() > 0U) {
        PointLight& selectedLight = m_lightManager.pointLightAt(selectedPointLight);
        ImGui::ColorEdit3("Colour", glm::value_ptr(selectedLight.color));
        ImGui::DragFloat3("Position", glm::value_ptr(selectedLight.position), 0.05f);
    }
}

void Menu::drawLightTab() {
    if (ImGui::BeginTabItem("Lights")) {
        ImGui::Text("Point lights");
        drawPointLightControls();

        ImGui::NewLine();
        ImGui::Separator();

        ImGui::Text("Planar lights");
        // TODO: Add planar light controls

        ImGui::EndTabItem();
    }
}
