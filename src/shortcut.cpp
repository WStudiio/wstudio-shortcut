#include "shortcut.h"
#include <stdexcept>
#include <Windows.h>
#include <set>

namespace Shortcut
{
    static HHOOK keyboardHook = NULL;
    static HHOOK mouseHook = NULL;
    static EventCallback userCallback;
    static unsigned int monitoredKey = 0;

    // Set para monitorar quais teclas estão pressionadas
    static std::set<int> pressedKeys;

    LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto keyInfo = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
            unsigned int keyCode = keyInfo->vkCode; // Converter para unsigned int

            if (monitoredKey == 0 || keyCode == monitoredKey)
            {
                if (wParam == WM_KEYDOWN)
                {
                    if (pressedKeys.find(keyCode) == pressedKeys.end())
                    {
                        pressedKeys.insert(keyCode);
                        userCallback(EventType::KeyDown, keyCode);
                    }
                }
                else if (wParam == WM_KEYUP)
                {
                    pressedKeys.erase(keyCode);
                    userCallback(EventType::KeyUp, keyCode);
                }
            }
        }
        return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
    }

    LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode == HC_ACTION)
        {
            auto mouseInfo = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
            int mouseButton = 0;

            switch (wParam)
            {
            case WM_LBUTTONDOWN:
                mouseButton = 1;
                userCallback(EventType::MouseDown, mouseButton);
                break;
            case WM_LBUTTONUP:
                mouseButton = 1;
                userCallback(EventType::MouseUp, mouseButton);
                break;
            case WM_RBUTTONDOWN:
                mouseButton = 2;
                userCallback(EventType::MouseDown, mouseButton);
                break;
            case WM_RBUTTONUP:
                mouseButton = 2;
                userCallback(EventType::MouseUp, mouseButton);
                break;
            case WM_MBUTTONDOWN:
                mouseButton = 3;
                userCallback(EventType::MouseDown, mouseButton);
                break;
            case WM_MBUTTONUP:
                mouseButton = 3;
                userCallback(EventType::MouseUp, mouseButton);
                break;
            case WM_XBUTTONDOWN:
                mouseButton = (HIWORD(mouseInfo->mouseData) == XBUTTON1) ? 4 : 5;
                userCallback(EventType::MouseDown, mouseButton);
                break;
            case WM_XBUTTONUP:
                mouseButton = (HIWORD(mouseInfo->mouseData) == XBUTTON1) ? 4 : 5;
                userCallback(EventType::MouseUp, mouseButton);
                break;
            }
        }
        return CallNextHookEx(mouseHook, nCode, wParam, lParam);
    }

    void startListening(const EventCallback &callback, int specificKey)
    {
        userCallback = callback;
        monitoredKey = specificKey;

        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
        mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);

        if (!keyboardHook || !mouseHook)
        {
            throw std::runtime_error("Failed to install hooks");
        }
    }

    void stopListening()
    {
        if (keyboardHook)
            UnhookWindowsHookEx(keyboardHook);
        if (mouseHook)
            UnhookWindowsHookEx(mouseHook);
        keyboardHook = NULL;
        mouseHook = NULL;
    }
} // namespace Shortcut
