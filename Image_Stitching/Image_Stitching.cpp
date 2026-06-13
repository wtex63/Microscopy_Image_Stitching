#include "Image_Stitching.h"
#include "Localization.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThread]
void Main(array<String^>^ args)
{
	try {
		Application::EnableVisualStyles();
		Application::SetCompatibleTextRenderingDefault(false);
		Image_Stitching::Image_Stitching form;
		Application::Run(%form);
	}
	catch (Exception^ ex) {
		MessageBox::Show(
			String::Format(Localization::T("MsgAppError"), ex->Message, ex->StackTrace),
			Localization::T("MsgAppErrorTitle"));
	}
	catch (...) {
		MessageBox::Show(Localization::T("MsgUnknownError"), Localization::T("MsgAppErrorTitle"));
	}
}