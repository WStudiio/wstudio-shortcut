#include "shortcut.h"
#include <stdexcept>
#include <Windows.h>
#include <set>
#include <string>
#include <deque>
#include <mutex>
#include <iostream> // Para logs

namespace Shortcut
{
    // Declarações antecipadas de funções auxiliares
    std::string GetErrorMessage(DWORD error);
    LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    // Classe RAII para gerenciar o Timer
    class TimerManager
    {
    public:
        TimerManager() : timerId(0) {}
        ~TimerManager()
        {
            stopTimer(); // Garante que o timer seja parado ao destruir a instância
        }

        // Inicia o timer com o intervalo fornecido, executando a função de callback periodicamente
        void startTimer(UINT interval, TIMERPROC timerProc)
        {
            if (timerId != 0)
            {
                std::cerr << "Timer is already running." << std::endl;
                return;
            }
            timerId = SetTimer(NULL, 0, interval, timerProc);
            if (!timerId)
            {
                throw std::runtime_error("Failed to start timer.");
            }
        }

        // Para o timer, se ele estiver ativo
        void stopTimer()
        {
            if (timerId)
            {
                KillTimer(NULL, timerId);
                timerId = 0;
                std::cout << "Timer stopped successfully." << std::endl;
            }
        }

    private:
        UINT_PTR timerId; // ID do timer para controle
    };

    // Classe RAII para gerenciar hooks de teclado e mouse
    class HookManager
    {
    public:
        HookManager() : keyboardHook(NULL), mouseHook(NULL) {}
        ~HookManager()
        {
            // Remove o hook do teclado, se estiver instalado
            if (keyboardHook)
            {
                if (UnhookWindowsHookEx(keyboardHook))
                    std::cout << "Keyboard hook successfully removed." << std::endl;
                else
                    std::cerr << "Failed to remove keyboard hook." << std::endl;
            }
            // Remove o hook do mouse, se estiver instalado
            if (mouseHook)
            {
                if (UnhookWindowsHookEx(mouseHook))
                    std::cout << "Mouse hook successfully removed." << std::endl;
                else
                    std::cerr << "Failed to remove mouse hook." << std::endl;
            }
        }

        // Instala os hooks globais de teclado e mouse
        void installHooks(HOOKPROC keyboardProc, HOOKPROC mouseProc)
        {
            if (keyboardHook || mouseHook)
            {
                throw std::runtime_error("Hooks are already installed.");
            }

            keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardProc, NULL, 0);
            if (!keyboardHook)
            {
                DWORD error = GetLastError();
                std::cerr << "Failed to install keyboard hook. Error: " << GetErrorMessage(error) << std::endl;
                throw std::runtime_error("Failed to install keyboard hook.");
            }
            else
            {
                std::cout << "Keyboard hook installed successfully." << std::endl;
            }

            mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseProc, NULL, 0);
            if (!mouseHook)
            {
                DWORD error = GetLastError();
                UnhookWindowsHookEx(keyboardHook); // Limpa o hook do teclado se o hook do mouse falhar
                keyboardHook = NULL;
                std::cerr << "Failed to install mouse hook. Error: " << GetErrorMessage(error) << std::endl;
                throw std::runtime_error("Failed to install mouse hook.");
            }
            else
            {
                std::cout << "Mouse hook installed successfully." << std::endl;
            }
        }

    private:
        HHOOK keyboardHook; // Handle para o hook do teclado
        HHOOK mouseHook;    // Handle para o hook do mouse
    };

    // Instâncias globais para gerenciamento de hooks e timer
    static HookManager hookManager;
    static TimerManager timerManager;
    static EventCallback userCallback; // Callback do usuário para eventos
    static unsigned int monitoredKey = 0; // Tecla específica para monitorar (0 = todas as teclas)
    static std::set<int> pressedKeys; // Teclas atualmente pressionadas
    static std::deque<std::pair<EventType, int>> eventQueue; // Fila de eventos de entrada
    static std::mutex eventMutex; // Mutex para proteger o acesso à fila de eventos
    static std::mutex pressedKeysMutex; // Mutex para proteger o conjunto de teclas pressionadas
    static std::mutex monitoredKeyMutex; // Mutex para proteger o código da tecla monitorada

    // Obtém uma mensagem de erro do Windows
    std::string GetErrorMessage(DWORD error)
    {
        char* messageBuffer = nullptr;
        size_t size = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer); // Libera o buffer de mensagem
        return message;
    }

    // Processa a fila de eventos, chamando o callback do usuário
    void ProcessEventQueue()
    {
        std::deque<std::pair<EventType, int>> localQueue;
        {
            std::lock_guard<std::mutex> lock(eventMutex); // Bloqueio mínimo para evitar atrasos
            localQueue.swap(eventQueue);
        }

        // Processa os eventos fora do bloqueio
        for (const auto& event : localQueue)
        {
            userCallback(event.first, event.second);
        }
    }

    // Callback de timer para processar eventos periodicamente
    void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        ProcessEventQueue();
    }

    // Callback do hook de teclado
    LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto keyInfo = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            unsigned int keyCode = keyInfo->vkCode;

            std::lock_guard<std::mutex> keyLock(monitoredKeyMutex);
            if (monitoredKey == 0 || keyCode == monitoredKey)
            {
                std::lock_guard<std::mutex> lock(pressedKeysMutex);
                if (wParam == WM_KEYDOWN && pressedKeys.find(keyCode) == pressedKeys.end())
                {
                    pressedKeys.insert(keyCode);
                    {
                        std::lock_guard<std::mutex> eventLock(eventMutex);
                        eventQueue.emplace_back(EventType::KeyDown, keyCode);
                    }
                }
                else if (wParam == WM_KEYUP)
                {
                    pressedKeys.erase(keyCode);
                    {
                        std::lock_guard<std::mutex> eventLock(eventMutex);
                        eventQueue.emplace_back(EventType::KeyUp, keyCode);
                    }
                }
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // Callback do hook de mouse
    LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto mouseInfo = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            int mouseButton = 0;

            // Identifica o botão do mouse com base no evento
            switch (wParam)
            {
            case WM_LBUTTONDOWN: mouseButton = 1; break;
            case WM_LBUTTONUP: mouseButton = 1; break;
            case WM_RBUTTONDOWN: mouseButton = 2; break;
            case WM_RBUTTONUP: mouseButton = 2; break;
            case WM_MBUTTONDOWN: mouseButton = 3; break;
            case WM_MBUTTONUP: mouseButton = 3; break;
            case WM_XBUTTONDOWN: mouseButton = (HIWORD(mouseInfo->mouseData) == XBUTTON1) ? 4 : 5; break;
            case WM_XBUTTONUP: mouseButton = (HIWORD(mouseInfo->mouseData) == XBUTTON1) ? 4 : 5; break;
            default: return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            std::lock_guard<std::mutex> lock(eventMutex);
            eventQueue.emplace_back(
                (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN) ? EventType::MouseDown : EventType::MouseUp,
                mouseButton);
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    // Inicia a escuta de eventos
    void startListening(const EventCallback& callback, int specificKey)
    {
        try
        {
            std::cout << "Starting to listen for events..." << std::endl;
            userCallback = callback;
            {
                std::lock_guard<std::mutex> lock(monitoredKeyMutex);
                monitoredKey = specificKey;
            }

            hookManager.installHooks(KeyboardProc, MouseProc);
            // Ajuste o intervalo do timer conforme necessário para otimizar o desempenho
            timerManager.startTimer(100, TimerProc);
            std::cout << "Listening started successfully." << std::endl;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error while starting to listen: " << ex.what() << std::endl;
        }
    }

    // Para a escuta de eventos
    void stopListening()
    {
        timerManager.stopTimer();
    }
} // namespace Shortcut
