#include "wndMain.h"
#include "Window.h"
#include "cmdlib.h"

class WndWelcome : Window {};

int editor_main(int argc, char **argv)
{
    AppConfig config;
    config.title = "Condem Engine";
    config.width = 900;
    config.height = 720;

    g_argc = argc;
    g_argv = argv;

    WndMain app(config);

    if (!app.Init())
    {
        std::cerr << "Error crítico: No se pudo inicializar la aplicación nativa." << std::endl;
        return -1;
    }

    app.Run();

    return 0;
}