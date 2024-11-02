#include "shortcut.h"
#include <stdexcept>
#include <Windows.h>
#include <set>
#include <string>
#include <vector>
#include <mutex>

namespace Shortcut
{
    // Declaração da função GetErrorMessage antes de usá-la
    std::string GetErrorMessage(DWORD error);

    // Classe RAII para gerenciar os hooks
    class HookManager
    {
    public:
        HookManager() : keyboardHook(NULL), mouseHook(NULL) {}
        ~HookManager()
        {
            if (keyboardHook)
                UnhookWindowsHookEx(keyboardHook);
            if (mouseHook)
                UnhookWindowsHookEx(mouseHook);
        }

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
                throw std::runtime_error("Failed to install keyboard hook. Error: " + GetErrorMessage(error));
            }

            mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseProc, NULL, 0);
            if (!mouseHook)
            {
                DWORD error = GetLastError();
                UnhookWindowsHookEx(keyboardHook);
                keyboardHook = NULL;
                throw std::runtime_error("Failed to install mouse hook. Error: " + GetErrorMessage(error));
            }
        }

    private:
        HHOOK keyboardHook;
        HHOOK mouseHook;
    };

    static HookManager hookManager;
    static EventCallback userCallback;
    static unsigned int monitoredKey = 0;
    static std::set<int> pressedKeys;
    static std::vector<std::pair<EventType, int>> eventQueue;
    static std::mutex eventMutex; // Mutex para proteger a fila de eventos
    static std::mutex pressedKeysMutex; // Mutex para proteger pressedKeys
    static std::mutex monitoredKeyMutex; // Mutex para proteger monitoredKey
    static UINT_PTR timerId = 0;

    std::string GetErrorMessage(DWORD error)
    {
        char* messageBuffer = nullptr;
        size_t size = FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);
        return message;
    }

    void ProcessEventQueue()
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        for (const auto& event : eventQueue)
        {
            userCallback(event.first, event.second);
        }
        eventQueue.clear();
    }

    void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
    {
        ProcessEventQueue();
    }

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
                    std::lock_guard<std::mutex> eventLock(eventMutex);
                    eventQueue.emplace_back(EventType::KeyDown, keyCode);
                }
                else if (wParam == WM_KEYUP)
                {
                    pressedKeys.erase(keyCode);
                    std::lock_guard<std::mutex> eventLock(eventMutex);
                    eventQueue.emplace_back(EventType::KeyUp, keyCode);
                }
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto mouseInfo = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
            int mouseButton = 0;

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
            eventQueue.emplace_back((wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN) ? EventType::MouseDown : EventType::MouseUp, mouseButton);
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    void startListening(const EventCallback& callback, int specificKey)
    {
        userCallback = callback;
        {
            std::lock_guard<std::mutex> lock(monitoredKeyMutex);
            monitoredKey = specificKey;
        }

        hookManager.installHooks(KeyboardProc, MouseProc);

        // Cria um temporizador que dispara a cada 100ms para processar a fila de eventos
        timerId = SetTimer(NULL, 0, 100, TimerProc);
    }

    [[deprecated("stopListening is no longer needed and managed automatically by HookManager.")]]
    void stopListening()
    {
        if (timerId)
        {
            KillTimer(NULL, timerId);
            timerId = 0;
        }
    }
} // namespace Shortcut
