#pragma once
#include <imgui.h>
#include <vector>
#include <string>

class Window {
public:
    Window(const char* name, int width = 384, int height = 256);
    virtual ~Window();

    static std::vector<Window*> windex;

    void Show()         { m_visible = true; }
    void Hide()         { m_visible = false; }
    void Toggle()       { m_visible = !m_visible; }
    bool IsOpen() const { return m_visible; }
    const char* GetName() const { return m_name.c_str(); }

    void Render();

protected:
    virtual void OnDrawUI() = 0;

    std::string m_name;
    int m_width, m_height;
    int m_instance;
    bool m_visible;
    ImGuiWindowFlags m_uiFlags;
};