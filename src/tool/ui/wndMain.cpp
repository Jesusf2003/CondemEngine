#include "wndMain.h"
#include "Window.h"
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#if defined(__APPLE__)
#include <SDL3/SDL_opengl.h>
#else
#include <SDL3/SDL_opengl.h>
#endif
#include <iostream>

int g_nScreenWidth;
int g_nScreenHeight;

WndMain::WndMain(const AppConfig &config) : m_config(config) {}

WndMain::~WndMain()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_glContext)
        SDL_GL_DestroyContext(m_glContext);
    if (m_window)
        SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool WndMain::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return false;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    const char *glsl_version = "#version 150";
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    const char *glsl_version = "#version 130";
#endif

    m_window = SDL_CreateWindow(m_config.title, m_config.width, m_config.height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window)
        return false;

    SDL_Surface* iconSurface = SDL_LoadBMP("icon.bmp");
    if (iconSurface)
    {
        SDL_SetWindowIcon(m_window, iconSurface);
        SDL_DestroySurface(iconSurface); // Liberamos la superficie de la memoria RAM
    }
    else
    {
        std::cout << "No se pudo cargar el archivo del ícono: " << SDL_GetError() << std::endl;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext)
        return false;

    SDL_GL_MakeCurrent(m_window, m_glContext);
    SDL_GL_SetSwapInterval(1); // VSync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(m_window, m_glContext);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return true;
}

void WndMain::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);

        if (event.type == SDL_EVENT_QUIT)
            m_running = false;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_window))
        {
            m_running = false;
        }
    }
}

void WndMain::StartFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport();
}

void WndMain::RenderMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Salir"))
                Close();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Game configs"))
            {
                std::cout << "Loading Game configs..." << std::endl;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void WndMain::RenderMainToolbar()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    float menuBarHeight = ImGui::GetFrameHeight();
    float toolbarHeight = menuBarHeight + 8.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + menuBarHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbarHeight));

    if (ImGui::Begin("##toolbar", nullptr, window_flags))
    {
        if (ImGui::BeginMenuBar())
        {

            ImVec2 buttonSize(24.0f, 0.0f);

            if (ImGui::Button("Save", buttonSize))
            {
                std::cout << "Guardando proyecto..." << std::endl;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Guardar (Ctrl+S)");

            ImGui::EndMenuBar();
        }
        ImGui::End();
    }

    ImGui::PopStyleVar(3);
}

void WndMain::EndFrame()
{
    ImGui::Render();

    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
    glClearColor(0.15f, 0.16f, 0.21f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(m_window);
}

void WndMain::Run()
{
    m_running = true;
    while (m_running)
    {
        ProcessEvents();
        StartFrame();

        RenderMainMenuBar();
        for (Window *w : Window::windex)
        {
            w->Render();
        }

        EndFrame();
    }
}