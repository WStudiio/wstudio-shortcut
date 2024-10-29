#include <napi.h>
#include "shortcut.h"

// Callback global para o evento
Napi::ThreadSafeFunction tsCallback;

// Função que vai enviar os eventos para o lado JavaScript
void EventCallback(Shortcut::EventType type, int code)
{
    if (tsCallback)
    {
        tsCallback.BlockingCall([type, code](Napi::Env env, Napi::Function jsCallback)
                                { jsCallback.Call({Napi::Number::New(env, static_cast<int>(type)),
                                                   Napi::Number::New(env, code)}); });
    }
}

// Função startListening exposta ao Node.js
Napi::Value StartListening(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();

    if (!info[0].IsFunction())
    {
        Napi::TypeError::New(env, "Expected a function as the first argument").ThrowAsJavaScriptException();
        return env.Null();
    }

    tsCallback = Napi::ThreadSafeFunction::New(
        env,
        info[0].As<Napi::Function>(),
        "Shortcut Event Callback",
        0, // Max queue size (0 = unlimited)
        1  // Initial thread count
    );

    int specificKey = 0;
    if (info.Length() > 1 && info[1].IsNumber())
    {
        specificKey = info[1].As<Napi::Number>().Int32Value();
    }

    Shortcut::startListening(EventCallback, specificKey);
    return Napi::Boolean::New(env, true);
}

// Função stopListening exposta ao Node.js
Napi::Value StopListening(const Napi::CallbackInfo &info)
{
    Shortcut::stopListening();
    if (tsCallback)
    {
        tsCallback.Release();
        tsCallback = nullptr;
    }
    return Napi::Boolean::New(info.Env(), true);
}

// Função de inicialização do módulo Node.js
Napi::Object Init(Napi::Env env, Napi::Object exports)
{
    exports.Set(Napi::String::New(env, "startListening"), Napi::Function::New(env, StartListening));
    exports.Set(Napi::String::New(env, "stopListening"), Napi::Function::New(env, StopListening));
    return exports;
}

NODE_API_MODULE(shortcut, Init)
