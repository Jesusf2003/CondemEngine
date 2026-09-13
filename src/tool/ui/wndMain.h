#pragma once
#include <SDL3/SDL.h>

struct AppConfig {
    const char* title = "Suite de Utilidades";
    int width = 1280;
    int height = 720;
};

class WndMain {
public:
    WndMain(const AppConfig& config);
    ~WndMain();

    bool Init();
    void Run();
    void Close() { m_running = false; }

private:
    void ProcessEvents();
    void StartFrame();
    void RenderMainMenuBar();
    void RenderMainToolbar();
    void EndFrame();

    AppConfig m_config;
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool m_running = false;
};