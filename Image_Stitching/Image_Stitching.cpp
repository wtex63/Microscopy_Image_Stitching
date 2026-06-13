#include "Image_Stitching.h"
#include "Localization.h"
#include <Windows.h>
#include <Objbase.h>

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
int ManagedMain(array<String^>^ args)
{
	try {
		Application::EnableVisualStyles();
		Application::SetCompatibleTextRenderingDefault(false);
		Image_Stitching::Image_Stitching form;
		Application::Run(%form);
		return 0;
	}
	catch (Exception^ ex) {
		MessageBox::Show(
			String::Format(Localization::T("MsgAppError"), ex->Message, ex->StackTrace),
			Localization::T("MsgAppErrorTitle"));
		return -1;
	}
	catch (...) {
		MessageBox::Show(Localization::T("MsgUnknownError"), Localization::T("MsgAppErrorTitle"));
		return -2;
	}
}

[STAThread]
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	int exitCode = ManagedMain(Environment::GetCommandLineArgs());
	if (SUCCEEDED(hr)) {
		CoUninitialize();
	}
	return exitCode;
}
