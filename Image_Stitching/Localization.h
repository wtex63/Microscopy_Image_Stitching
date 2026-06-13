#pragma once

using namespace System;

namespace Localization {
    public enum class Language {
        English,
        Turkish
    };

    void SetLanguage(Language language);
    Language GetLanguage();
    String^ T(String^ key);
}
