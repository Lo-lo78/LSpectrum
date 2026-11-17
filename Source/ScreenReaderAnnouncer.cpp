// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ScreenReaderAnnouncer.h"

#if JUCE_WINDOWS
 #include <Windows.h>
 #include <UIAutomation.h>
 #include <oleauto.h>
#endif

namespace lspectrum
{
bool announceToActiveScreenReader (juce::Component& source, const juce::String& message, bool preserveOrder)
{

   #if JUCE_WINDOWS
    if (! UiaClientsAreListening())
        return false;

    auto* handler = source.getAccessibilityHandler();
    if (handler == nullptr)
        return false;

    auto* unknown = reinterpret_cast<IUnknown*> (handler->getNativeImplementation());
    if (unknown == nullptr)
        return false;

    IRawElementProviderSimple* provider = nullptr;
    if (FAILED (unknown->QueryInterface (__uuidof (IRawElementProviderSimple),
                                         reinterpret_cast<void**> (&provider))))
        return false;

    auto* text = SysAllocString (message.toWideCharPointer());
    auto* activity = SysAllocString (L"LSpectrum");
    const auto hr = UiaRaiseNotificationEvent (provider,
                               NotificationKind_Other,
                               preserveOrder ? NotificationProcessing_ImportantAll
                                             : NotificationProcessing_ImportantMostRecent,
                               text,
                               activity);
    SysFreeString (text);
    SysFreeString (activity);
    provider->Release();
    return SUCCEEDED (hr);
   #else
    if (auto* handler = source.getAccessibilityHandler())
    {
        handler->notifyAccessibilityEvent (juce::AccessibilityEvent::titleChanged);
        return true;
    }
    return false;
   #endif
}
}
