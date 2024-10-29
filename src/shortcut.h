#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <functional>
#include <Windows.h>

namespace Shortcut
{

    enum class EventType
    {
        KeyDown,
        KeyUp,
        MouseDown,
        MouseUp
    };

    using EventCallback = std::function<void(EventType, int)>;

    void startListening(const EventCallback &callback, int specificKey = 0);
    void stopListening();

} // namespace Shortcut

#endif // SHORTCUT_H
