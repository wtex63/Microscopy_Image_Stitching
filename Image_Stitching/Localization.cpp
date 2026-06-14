#include "Localization.h"
#include <vcclr.h>

using namespace System::Globalization;
using namespace System::Reflection;
using namespace System::Resources;
using namespace System::Threading;

namespace {
	Localization::Language g_language = Localization::Language::English;
	gcroot<ResourceManager^> g_resourceManager;

	ResourceManager^ GetResourceManager()
	{
		ResourceManager^ resourceManager = g_resourceManager;
		if (resourceManager == nullptr) {
			resourceManager = gcnew ResourceManager("Image_Stitching.Strings", Assembly::GetExecutingAssembly());
			g_resourceManager = resourceManager;
		}
		return resourceManager;
	}

	CultureInfo^ GetCulture(Localization::Language language)
	{
		return language == Localization::Language::Turkish
			? CultureInfo::GetCultureInfo("tr")
			: CultureInfo::GetCultureInfo("en");
	}
}

namespace Localization {
	void SetLanguage(Language language)
	{
		g_language = language;
		CultureInfo^ culture = GetCulture(language);
		Thread::CurrentThread->CurrentUICulture = culture;
		Thread::CurrentThread->CurrentCulture = culture;
	}

	Language GetLanguage()
	{
		return g_language;
	}

	String^ T(String^ key)
	{
		CultureInfo^ culture = GetCulture(g_language);
		String^ value = GetResourceManager()->GetString(key, culture);
		if (String::IsNullOrEmpty(value)) {
			value = GetResourceManager()->GetString(key, CultureInfo::GetCultureInfo("en"));
		}

		return String::IsNullOrEmpty(value) ? key : value;
	}
}
