#include "Window.h"
#include <algorithm>

std::vector<Window*> Window::windex;

Window::Window(const char* name, int width, int height)
    : m_name(name), m_width(width), m_height(height), m_visible(true), m_uiFlags(0) {
    
    m_instance = (int)std::count_if(windex.begin(), windex.end(), [name](Window* w) {
        return w->m_name == name;
    });

    windex.push_back(this);
}

Window::~Window() {
    windex.erase(std::find(windex.begin(), windex.end(), this));
}

void Window::Render() {
    if (!m_visible) return;

    ImGui::SetNextWindowSize(ImVec2((float)m_width, (float)m_height), ImGuiCond_FirstUseEver);
    
    std::string uniqueID = m_name + "##" + std::to_string(m_instance);

    if (ImGui::Begin(uniqueID.c_str(), &m_visible, m_uiFlags)) {
        OnDrawUI();
    }
    ImGui::End();
}