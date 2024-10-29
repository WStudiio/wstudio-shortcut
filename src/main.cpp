#include <iostream>
#include "shortcut.h"

void handleEvent(Shortcut::EventType type, int code)
{
    switch (type)
    {
    case Shortcut::EventType::KeyDown:
        std::cout << "Key Down: " << code << std::endl;
        break;
    case Shortcut::EventType::KeyUp:
        std::cout << "Key Up: " << code << std::endl;
        break;
    case Shortcut::EventType::MouseDown:
        std::cout << "Mouse Button Down: " << code << std::endl;
        break;
    case Shortcut::EventType::MouseUp:
        std::cout << "Mouse Button Up: " << code << std::endl;
        break;
    }
}

int main()
{
    try
    {
        Shortcut::startListening(handleEvent, 0); // 0 para monitorar todas as teclas e botões

        std::cout << "Pressione qualquer tecla ou botão do mouse para testar. Pressione Ctrl+C para sair." << std::endl;

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        Shortcut::stopListening();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Erro: " << ex.what() << std::endl;
    }

    return 0;
}
