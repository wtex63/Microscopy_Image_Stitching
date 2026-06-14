#pragma once
#include <atlstr.h>
#include <chrono>
#include <msclr\marshal_cppstd.h>

#include "Image.h"
#include "Process.h"
#include "Homography.h"
#include "Panorama.h"
#include "Correlation.h"
#include "Dots.h"
#include "Localization.h"


namespace Image_Stitching {
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Collections::Generic;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace System::IO;

	/// <summary>
	/// Summary for MyForm
	/// </summary>
	public ref class Image_Stitching : public System::Windows::Forms::Form
	{
	public:
		enum class BlendMode
		{
			Auto = 0,
			Quality = 1,
			Speed = 2
		};

		Image_Stitching(void)
		{
			InitializeComponent();
			Localization::SetLanguage(Localization::Language::English);
			ApplyLocalization();
			ConfigureStartupArguments(Environment::GetCommandLineArgs());
		}

		Image_Stitching(array<String^>^ args)
		{
			InitializeComponent();
			Localization::SetLanguage(Localization::Language::English);
			ApplyLocalization();
			ConfigureStartupArguments(args);
		}

    // Show the prepared full-resolution preview form prepared by ShowMatchVisualization
	System::Void OnViewFullResClick(System::Object^ sender, System::EventArgs^ e) {
		if (this->previewFullResForm != nullptr) {
			try {
				this->previewFullResForm->Show();
				this->previewFullResForm->BringToFront();
			}
			catch (...) { }
		}
	}

	// Close and dispose the full-resolution preview when the "Continue" button is pressed
	System::Void OnFullResContinueClick(System::Object^ sender, System::EventArgs^ e) {
		if (this->previewFullResForm != nullptr) {
			try { this->previewFullResForm->Close(); } catch (...) { }
			try { delete this->previewFullResForm; } catch (...) { }
			this->previewFullResForm = nullptr;
		}
	}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Image_Stitching()
		{
			if (LaplacePyramid1 != nullptr) {
				for (int i = 0; i < 4; i++) {
					if (LaplacePyramid1[i] != nullptr) {
						delete[] LaplacePyramid1[i];
						LaplacePyramid1[i] = nullptr;
					}
				}
			}

			if (LaplacePyramid2 != nullptr) {
				for (int i = 0; i < 4; i++) {
					if (LaplacePyramid2[i] != nullptr) {
						delete[] LaplacePyramid2[i];
						LaplacePyramid2[i] = nullptr;
					}
				}
			}

			buffer1 = DeleteByteArray(buffer1);
			buffer2 = DeleteByteArray(buffer2);
			panorama1 = DeleteByteArray(panorama1);
			panorama2 = DeleteByteArray(panorama2);
			intensity1 = DeleteByteArray(intensity1);
			intensity2 = DeleteByteArray(intensity2);
			latestColorOutput = DeleteByteArray(latestColorOutput);
			currPanoSize = DeleteXyObject(currPanoSize);
			currVec = DeleteXyObject(currVec);
			prevVec = DeleteXyObject(prevVec);
			pocDot = DeleteXyObject(pocDot);
			prevPanoSize = DeleteXyObject(prevPanoSize);

			if (outReal != nullptr) {
				for (int i = 0; i < 2; i++)
					outReal[i] = DeleteDoubleArray(outReal[i]);
				outReal = DeleteDoublePtrArray(outReal);
			}

			if (outImag != nullptr) {
				for (int i = 0; i < 2; i++)
					outImag[i] = DeleteDoubleArray(outImag[i]);
				outImag = DeleteDoublePtrArray(outImag);
			}

			if (match1 != nullptr) {
				for (int i = 0; i < 3; i++)
					match1[i] = DeleteDoubleArray(match1[i]);
				match1 = DeleteDoublePtrArray(match1);
			}

			if (match2 != nullptr) {
				for (int i = 0; i < 3; i++)
					match2[i] = DeleteDoubleArray(match2[i]);
				match2 = DeleteDoublePtrArray(match2);
			}

			LaplacePyramid1 = DeleteByte2PtrArray(LaplacePyramid1);
			LaplacePyramid2 = DeleteByte2PtrArray(LaplacePyramid2);

			if (components)
			{
				delete components;
			}
		}

	protected:

	private: System::Windows::Forms::MenuStrip^ menuStrip1;
	private: System::Windows::Forms::ToolStripMenuItem^ fileToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ openToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ languageToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ englishToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^ turkishToolStripMenuItem;
	private: System::Windows::Forms::OpenFileDialog^ openFileDialog1;
	private: System::Windows::Forms::Panel^ imagePanel;
	private: System::Windows::Forms::PictureBox^ pictureBox4;

	private: System::Windows::Forms::Label^ labelResultS;
	private: System::Windows::Forms::TextBox^ logTextBox;
	private: System::Windows::Forms::Panel^ statusPanel;
	private: System::Windows::Forms::Label^ statusTitleLbl;
	private: System::Windows::Forms::Label^ statusConfidenceLbl;
	private: System::Windows::Forms::Label^ statusConfidenceValLbl;
	private: System::Windows::Forms::Label^ statusInlierLbl;
	private: System::Windows::Forms::Label^ statusInlierValLbl;
	private: System::Windows::Forms::Label^ statusMatcherLbl;
	private: System::Windows::Forms::Label^ statusMatcherValLbl;
	private: System::Windows::Forms::Label^ statusElapsedLbl;
	private: System::Windows::Forms::Label^ statusElapsedValLbl;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container^ components;


#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		/// 
		BYTE* buffer1 = nullptr, * buffer2 = nullptr;
		bool isFirstLine = true;
		int currCornerID;
		xy* currVec = new xy;
		xy* prevVec = new xy;
		xy* pocDot = new xy;
		xy* prevPanoSize = new xy;
		xy* currPanoSize = nullptr;
		int lineCounter;
		BYTE2** LaplacePyramid1 = nullptr;
		BYTE2** LaplacePyramid2 = nullptr;
		BYTE* panorama1 = nullptr;
		BYTE* panorama2 = nullptr;
		int imgCounter;
		BYTE* intensity1 = nullptr;
		BYTE* intensity2 = nullptr;
		BYTE* latestColorOutput = nullptr;
		int latestColorWidth = 0;
		int latestColorHeight = 0;
		double statusPhaseConfidence = -1.0;
		double statusInlierRatio = -1.0;
		String^ statusMatcherKey = "StatusValueNA";
		DateTime stitchStartTime;
		bool stitchTimerActive = false;

		double** outReal = new double* [2]();
		double** outImag = new double* [2]();
		double** match1 = new double* [3]();
		double** match2 = new double* [3]();

	private: System::Windows::Forms::Label^ imgIndexLbl;
	private: System::Windows::Forms::Label^ label14;


	private: System::Windows::Forms::Button^ saveButton;
	private: System::Windows::Forms::CheckBox^ autoClosePreviewChk;
	private: System::Windows::Forms::CheckBox^ useGpuPhaseChk;
	private: System::Windows::Forms::Label^ blendModeLbl;
	private: System::Windows::Forms::ComboBox^ blendModeCombo;
	private: System::Windows::Forms::Form^ previewFullResForm = nullptr;
	private: array<String^>^ startupInputFiles = nullptr;
	private: String^ startupLogPath = nullptr;
	private: bool startupAutoRun = false;
	private: bool startupUseGpu = false;
	private: bool startupAutoClosePreview = false;
	private: bool startupExitWhenDone = false;
	private: bool startupRunStarted = false;
	private: String^ startupArgsError = nullptr;
	private: BlendMode startupBlendMode = BlendMode::Auto;

		   void FreePictureBox(PictureBox^ pictureBox) {
			   if (pictureBox->Image != nullptr) {
				   delete pictureBox->Image;
				   pictureBox->Image = nullptr;
			   }
		   }

		   String^ GetDefaultBatchLogPath() {
			   String^ mode = startupUseGpu ? "gpu" : "cpu";
			   String^ dir = Path::Combine(Application::StartupPath, "speed_logs");
			   String^ fileName = String::Format("stitch_{0}_{1}.log", mode, DateTime::Now.ToString("yyyyMMdd_HHmmss"));
			   return Path::Combine(dir, fileName);
		   }

		   String^ GetBlendModeName(BlendMode mode) {
			   switch (mode) {
			   case BlendMode::Quality:
				   return "quality";
			   case BlendMode::Speed:
				   return "speed";
			   default:
				   return "auto";
			   }
		   }

		   BlendMode GetRequestedBlendMode() {
			   if (blendModeCombo != nullptr) {
				   switch (blendModeCombo->SelectedIndex) {
				   case 1:
					   return BlendMode::Quality;
				   case 2:
					   return BlendMode::Speed;
				   default:
					   return BlendMode::Auto;
				   }
			   }
			   return startupBlendMode;
		   }

		   void SetBlendModeSelection(BlendMode mode) {
			   if (blendModeCombo == nullptr)
				   return;
			   switch (mode) {
			   case BlendMode::Quality:
				   blendModeCombo->SelectedIndex = 1;
				   break;
			   case BlendMode::Speed:
				   blendModeCombo->SelectedIndex = 2;
				   break;
			   default:
				   blendModeCombo->SelectedIndex = 0;
				   break;
			   }
		   }

		   BlendMode ResolveBlendMode(bool useTranslationModel) {
			   BlendMode requestedBlendMode = GetRequestedBlendMode();
			   if (requestedBlendMode == BlendMode::Quality)
				   return BlendMode::Quality;
			   if (requestedBlendMode == BlendMode::Speed)
				   return BlendMode::Speed;
			   if (useTranslationModel && (this->useGpuPhaseChk != nullptr && this->useGpuPhaseChk->Checked) && IsPhaseCorrelationGpuAvailable())
				   return BlendMode::Speed;
			   return BlendMode::Quality;
		   }

		   void ConfigureStartupArguments(array<String^>^ args) {
			   this->Shown += gcnew EventHandler(this, &Image_Stitching::OnStartupShown);
			   if (args == nullptr || args->Length <= 1)
				   return;

			   List<String^>^ files = gcnew List<String^>();
			   for (int i = 1; i < args->Length; i++) {
				   String^ arg = args[i];
				   if (String::IsNullOrWhiteSpace(arg))
					   continue;

				   if (String::Equals(arg, "--input", StringComparison::OrdinalIgnoreCase) || String::Equals(arg, "-i", StringComparison::OrdinalIgnoreCase)) {
					   if (i + 1 >= args->Length) {
						   startupArgsError = "Missing value for --input argument.";
						   break;
					   }
					   files->Add(args[++i]);
				   }
				   else if (String::Equals(arg, "--log", StringComparison::OrdinalIgnoreCase) || String::Equals(arg, "-l", StringComparison::OrdinalIgnoreCase)) {
					   if (i + 1 >= args->Length) {
						   startupArgsError = "Missing value for --log argument.";
						   break;
					   }
					   startupLogPath = args[++i];
				   }
				   else if (String::Equals(arg, "--autoclosepreview", StringComparison::OrdinalIgnoreCase)) {
					   startupAutoClosePreview = true;
				   }
				   else if (String::Equals(arg, "--gpu", StringComparison::OrdinalIgnoreCase)) {
					   startupUseGpu = true;
				   }
				   else if (String::Equals(arg, "--cpu", StringComparison::OrdinalIgnoreCase)) {
					   startupUseGpu = false;
				   }
				   else if (String::Equals(arg, "--exit", StringComparison::OrdinalIgnoreCase)) {
					   startupExitWhenDone = true;
				   }
				   else if (String::Equals(arg, "--blend", StringComparison::OrdinalIgnoreCase)) {
					   if (i + 1 >= args->Length) {
						   startupArgsError = "Missing value for --blend argument.";
						   break;
					   }
					   String^ blendValue = args[++i];
					   if (String::Equals(blendValue, "auto", StringComparison::OrdinalIgnoreCase))
						   startupBlendMode = BlendMode::Auto;
					   else if (String::Equals(blendValue, "quality", StringComparison::OrdinalIgnoreCase))
						   startupBlendMode = BlendMode::Quality;
					   else if (String::Equals(blendValue, "speed", StringComparison::OrdinalIgnoreCase))
						   startupBlendMode = BlendMode::Speed;
					   else {
						   startupArgsError = "Invalid value for --blend. Use auto, quality, or speed.";
						   break;
					   }
				   }
			   }

			   if (files->Count > 0) {
				   startupAutoRun = true;
				   startupInputFiles = files->ToArray();
				   if (String::IsNullOrWhiteSpace(startupLogPath))
					   startupLogPath = GetDefaultBatchLogPath();
			   }
		   }

		   System::Void OnStartupShown(System::Object^ sender, System::EventArgs^ e) {
			   if (!startupAutoRun || startupRunStarted)
				   return;

			   startupRunStarted = true;
			   if (this->autoClosePreviewChk != nullptr)
				   this->autoClosePreviewChk->Checked = startupAutoClosePreview;
			   if (this->useGpuPhaseChk != nullptr)
				   this->useGpuPhaseChk->Checked = startupUseGpu;
			   SetBlendModeSelection(startupBlendMode);

			   this->BeginInvoke(gcnew MethodInvoker(this, &Image_Stitching::RunStartupAutomation));
		   }

		   void RunStartupAutomation() {
			   openToolStripMenuItem_Click(nullptr, EventArgs::Empty);
		   }

		   void AppendLog(String^ message) {
			   if (logTextBox == nullptr) return;
			   String^ line = DateTime::Now.ToString("HH:mm:ss") + " - " + message;
			   logTextBox->AppendText(line + Environment::NewLine);
			   logTextBox->SelectionStart = logTextBox->TextLength;
			   logTextBox->ScrollToCaret();
			   if (startupAutoRun && !String::IsNullOrWhiteSpace(startupLogPath)) {
				   try {
					   String^ fullPath = Path::GetFullPath(startupLogPath);
					   String^ dir = Path::GetDirectoryName(fullPath);
					   if (!String::IsNullOrWhiteSpace(dir))
						   Directory::CreateDirectory(dir);
					   File::AppendAllText(fullPath, line + Environment::NewLine);
				   }
				   catch (...) {}
			   }
			   Application::DoEvents();
		   }

		   void OnAutoCloseMatchPreviewTick(System::Object^ sender, System::EventArgs^ e) {
			   Timer^ timer = dynamic_cast<Timer^>(sender);
			   if (timer != nullptr) {
				   timer->Stop();
				   Form^ form = dynamic_cast<Form^>(timer->Tag);
				   if (form != nullptr && !form->IsDisposed) {
					   form->Close();
				   }
			   }
		   }

		   BYTE* DeleteByteArray(BYTE* ptr) {
			   if (ptr != nullptr)
				   delete[] ptr;
			   return nullptr;
		   }

		   double* DeleteDoubleArray(double* ptr) {
			   if (ptr != nullptr)
				   delete[] ptr;
			   return nullptr;
		   }

		   double** DeleteDoublePtrArray(double** ptr) {
			   if (ptr != nullptr)
				   delete[] ptr;
			   return nullptr;
		   }

		   xy* DeleteXyArray(xy* ptr) {
			   if (ptr != nullptr)
				   delete[] ptr;
			   return nullptr;
		   }

		   xy* DeleteXyObject(xy* ptr) {
			   if (ptr != nullptr)
				   delete ptr;
			   return nullptr;
		   }

		   BYTE2** DeleteByte2PtrArray(BYTE2** ptr) {
			   if (ptr != nullptr)
				   delete[] ptr;
			   return nullptr;
		   }

		   void CacheLatestColorOutput(BYTE* image, int width, int height) {
			   if (image == nullptr || width <= 0 || height <= 0)
				   return;

			   latestColorOutput = DeleteByteArray(latestColorOutput);
			   long size = long(width) * long(height) * 3;
			   latestColorOutput = new BYTE[size];
			   memcpy(latestColorOutput, image, size);
			   latestColorWidth = width;
			   latestColorHeight = height;
		   }

		   void UpdateStatusPanel(String^ matcherKey, double phaseConfidence, double inlierRatio, bool hasInlierRatio) {
			   if (matcherKey != nullptr)
				   statusMatcherKey = matcherKey;

			   statusPhaseConfidence = phaseConfidence;
			   statusInlierRatio = hasInlierRatio ? inlierRatio : -1.0;

			   if (statusMatcherValLbl != nullptr)
				   statusMatcherValLbl->Text = Localization::T(statusMatcherKey);

			   if (statusConfidenceValLbl != nullptr) {
				   statusConfidenceValLbl->Text = (statusPhaseConfidence >= 0.0)
					   ? statusPhaseConfidence.ToString("F2")
					   : Localization::T("StatusValueNA");
			   }

			   if (statusInlierValLbl != nullptr) {
				   statusInlierValLbl->Text = (statusInlierRatio >= 0.0)
					   ? statusInlierRatio.ToString("P0")
					   : Localization::T("StatusValueNA");
			   }

			   if (statusElapsedValLbl != nullptr) {
				   TimeSpan elapsed = stitchTimerActive ? (DateTime::Now - stitchStartTime) : TimeSpan::Zero;
				   statusElapsedValLbl->Text = elapsed.ToString("mm\\:ss");
			   }
		   }

		   void ResetStatusPanel() {
			   statusMatcherKey = "StatusValueNA";
			   statusPhaseConfidence = -1.0;
			   statusInlierRatio = -1.0;
			   UpdateStatusPanel(statusMatcherKey, statusPhaseConfidence, statusInlierRatio, false);
		   }

		   double* PhaseCorrelationWithUiHeartbeat(double* fft1Real, double* fft1Imag, double* fft2Real, double* fft2Imag, int width, int height) {
			   return PhaseCorrelation(fft1Real, fft1Imag, fft2Real, fft2Imag, width, height);
		   }

		   double** CreateTranslationHomography(int cornerID, xy* vec) {
			   int tx = 0;
			   int ty = 0;
			   switch (cornerID)
			   {
			   case 0:
				   tx = vec->x;
				   ty = vec->y;
				   break;
			   case 1:
				   tx = -vec->x;
				   ty = vec->y;
				   break;
			   case 2:
				   tx = vec->x;
				   ty = -vec->y;
				   break;
			   case 3:
				   tx = -vec->x;
				   ty = -vec->y;
				   break;
			   default:
				   break;
			   }

			   double** H = new double* [3];
			   for (int i = 0; i < 3; i++)
				   H[i] = new double[3]();

			   H[0][0] = 1.0;
			   H[0][1] = 0.0;
			   H[0][2] = (double)tx;
			   H[1][0] = 0.0;
			   H[1][1] = 1.0;
			   H[1][2] = (double)ty;
			   H[2][0] = 0.0;
			   H[2][1] = 0.0;
			   H[2][2] = 1.0;
			   return H;
		   }

		   void ShowColorImage(BYTE* Image, int width, int height) {

			   if (Image == nullptr || width <= 0 || height <= 0) return;

			   CacheLatestColorOutput(Image, width, height);
			   FreePictureBox(pictureBox4);
			   Bitmap^ source = gcnew Bitmap(width, height, System::Drawing::Imaging::PixelFormat::Format24bppRgb);

			   Color c;
			   int psw, bufpos, row, column;
			   psw = width * 3;
			   for (row = 0; row < height; row++) {
				   for (column = 0; column < width; column++) {
					   bufpos = (height - row - 1) * psw + column * 3;
					   c = Color::FromArgb((int)Image[bufpos + 2], (int)Image[bufpos + 1], (int)Image[bufpos]);
					   source->SetPixel(column, row, c);
				   }
			   }

			   int panelW = imagePanel->ClientSize.Width > 0 ? imagePanel->ClientSize.Width : this->ClientSize.Width;
			   int panelH = imagePanel->ClientSize.Height > 0 ? imagePanel->ClientSize.Height : this->ClientSize.Height;
			   double scaleW = double(panelW) / double(width);
			   double scaleH = double(panelH) / double(height);
			   double scale = Math::Min(1.0, Math::Min(scaleW, scaleH));

			   int displayW = Math::Max(1, int(width * scale));
			   int displayH = Math::Max(1, int(height * scale));

			   Bitmap^ display = source;
			   if (displayW != width || displayH != height) {
				   Bitmap^ scaled = gcnew Bitmap(displayW, displayH, System::Drawing::Imaging::PixelFormat::Format24bppRgb);
				   Graphics^ g = Graphics::FromImage(scaled);
				   g->InterpolationMode = System::Drawing::Drawing2D::InterpolationMode::HighQualityBicubic;
				   g->DrawImage(source, System::Drawing::Rectangle(0, 0, displayW, displayH));
				   delete g;
				   delete source;
				   display = scaled;
			   }

			   pictureBox4->SizeMode = PictureBoxSizeMode::Normal;
			   pictureBox4->Location = System::Drawing::Point(0, 0);
			   pictureBox4->Width = displayW;
			   pictureBox4->Height = displayH;
			   pictureBox4->Image = display;

			   imagePanel->AutoScrollMinSize = System::Drawing::Size(displayW, displayH);
			   imagePanel->AutoScrollPosition = System::Drawing::Point(0, 0);
			   pictureBox4->Invalidate();
			   imagePanel->Invalidate();
			   imagePanel->Update();
			   pictureBox4->Refresh();
			   Application::DoEvents();
		   }

		   void MaxPOC(double* POC, xy* pocDot, int width, int height) {

			   double max = -10000000.0;
			   int i = 0;
			   for (int row = 0; row < height; row++)
				   for (int col = 0; col < width; col++)
				   {
					   i = row * width + col;

					   if (POC[i] > max) {
						   max = POC[i];
						   pocDot->y = row;
						   pocDot->x = col;
					   }
				   }
		   }

		   bool BuildDotsFromSignedShift(int width, int height, int dx, int dy, xy* img1Dots, xy* img2Dots) {
			   if (img1Dots == nullptr || img2Dots == nullptr || width <= 1 || height <= 1)
				   return false;

			   int xStart = dx > 0 ? dx : 0;
			   int yStart = dy > 0 ? dy : 0;
			   int xEnd = (width + dx - 1 < width - 1) ? (width + dx - 1) : (width - 1);
			   int yEnd = (height + dy - 1 < height - 1) ? (height + dy - 1) : (height - 1);

			   if (xStart >= xEnd || yStart >= yEnd)
				   return false;

			   int margin = 8;
			   xStart += margin;
			   yStart += margin;
			   xEnd -= margin;
			   yEnd -= margin;

			   if (xEnd - xStart < 12 || yEnd - yStart < 12)
				   return false;

			   img1Dots[0] = { xStart, yStart };
			   img1Dots[1] = { xEnd, yStart };
			   img1Dots[2] = { xStart, yEnd };
			   img1Dots[3] = { xEnd, yEnd };

			   for (int i = 0; i < 4; i++) {
				   img2Dots[i].x = img1Dots[i].x - dx;
				   img2Dots[i].y = img1Dots[i].y - dy;
				   if (img2Dots[i].x < 0 || img2Dots[i].x >= width || img2Dots[i].y < 0 || img2Dots[i].y >= height)
					   return false;
			   }

			   return true;
		   }

		   bool BuildDotsFromSignedShiftAndRotation(int width, int height, int dx, int dy, float rotationDeg, xy* img1Dots, xy* img2Dots) {
			   if (!BuildDotsFromSignedShift(width, height, dx, dy, img1Dots, img2Dots))
				   return false;

			   if (Math::Abs(rotationDeg) < 0.25f)
				   return true;

			   double rad = double(rotationDeg) * (3.14159265358979323846 / 180.0);
			   double ca = cos(rad);
			   double sa = sin(rad);
			   double cx = 0.5 * double(width - 1);
			   double cy = 0.5 * double(height - 1);

			   for (int i = 0; i < 4; i++) {
				   double px = double(img1Dots[i].x) - double(dx);
				   double py = double(img1Dots[i].y) - double(dy);
				   double tx = px - cx;
				   double ty = py - cy;
				   double rx = tx * ca + ty * sa;
				   double ry = -tx * sa + ty * ca;
				   int x2 = int(Math::Round(rx + cx));
				   int y2 = int(Math::Round(ry + cy));
				   if (x2 < 0 || x2 >= width || y2 < 0 || y2 >= height)
					   return false;
				   img2Dots[i].x = x2;
				   img2Dots[i].y = y2;
			   }

			   return true;
		   }

		   /*
		   * cornerID
		   *
			   0 ********* 1
			   * 		   *
			   * 		   *
			   * 		   *
			   2 ********* 3
		   */
		   void LineUpdate(int cornerID, xy* vec) {

			   switch (cornerID)
			   {
			   case 0:
				   lineCounter += vec->y;
				   break;

			   case 1:
				   lineCounter += vec->y;
				   break;

			   case 2:
				   lineCounter -= vec->y;
				   if (lineCounter < 0) lineCounter = 0;
				   break;

			   case 3:
				   lineCounter -= vec->y;
				   if (lineCounter < 0) lineCounter = 0;
				   break;

			   default:
				   break;
			   }
		   }

		   /*void ShowImageIntensity(BYTE* img1, BYTE* img2, int width, int height) {

			   Bitmap^ surface1 = gcnew Bitmap(width, height);
			   Bitmap^ surface2 = gcnew Bitmap(width, height);
			   pictureBox1->Image = surface1;
			   pictureBox2->Image = surface2;

			   Color c1, c2;
			   for (int row = 0; row < height; row++) {
				   for (int column = 0; column < width; column++) {

					   c1 = Color::FromArgb(img1[row * width + column], img1[row * width + column], img1[row * width + column]);
					   c2 = Color::FromArgb(img2[row * width + column], img2[row * width + column], img2[row * width + column]);
					   surface1->SetPixel(column, row, c1);
					   surface2->SetPixel(column, row, c2);
				   }
			   }

			   pictureBox1->Refresh();
			   pictureBox2->Refresh();

		   }*/

		   void ShowMatchVisualization(BYTE* img1, BYTE* img2, int width, int height, xy* img1Dots, xy* img2Dots) {
			   if (img1 == nullptr || img2 == nullptr || img1Dots == nullptr || img2Dots == nullptr) return;

               // Create visualization form
			   Form^ vizForm = gcnew Form();
			   vizForm->Text = "Image Comparison - Matched Points";
			   vizForm->StartPosition = FormStartPosition::CenterParent;
			   vizForm->Owner = this;

			   // Cap form size to the working area so it fits on screen
			   System::Drawing::Rectangle work = System::Windows::Forms::Screen::GetWorkingArea(this);
			   int maxFormW = work.Width - 40;
			   int maxFormH = work.Height - 80;
			   int desiredW = width * 2 + 40;
			   int desiredH = height + 100;
			   vizForm->Width = Math::Min(desiredW, maxFormW);
			   vizForm->Height = Math::Min(desiredH, maxFormH);

			   // Create panel for images (client area minus margins so scrollbars appear when needed)
			   Panel^ vizPanel = gcnew Panel();
			   vizPanel->Left = 10;
			   vizPanel->Top = 10;
			   vizPanel->Width = vizForm->ClientSize.Width - 20;
			   vizPanel->Height = vizForm->ClientSize.Height - 80;
			   vizPanel->BorderStyle = BorderStyle::FixedSingle;
			   vizPanel->AutoScroll = true;
			   vizPanel->BackColor = Color::White;

			   // Create combined bitmap (both images side-by-side)
			   Bitmap^ combined = gcnew Bitmap(width * 2 + 20, height + 10, System::Drawing::Imaging::PixelFormat::Format24bppRgb);
			   Graphics^ g = Graphics::FromImage(combined);
			   g->FillRectangle(Brushes::White, 0, 0, width * 2 + 20, height + 10);

			   // Convert grayscale to color and draw image 1
			   Bitmap^ bmp1 = gcnew Bitmap(width, height, System::Drawing::Imaging::PixelFormat::Format24bppRgb);
			   for (int row = 0; row < height; row++) {
				   for (int col = 0; col < width; col++) {
					   int idx = row * width + col;
					   int val = (int)img1[idx];
					   Color c = Color::FromArgb(val, val, val);
					   bmp1->SetPixel(col, row, c);
				   }
			   }
			   g->DrawImage(bmp1, 5, 5);
			   delete bmp1;

			   // Convert grayscale to color and draw image 2
			   Bitmap^ bmp2 = gcnew Bitmap(width, height, System::Drawing::Imaging::PixelFormat::Format24bppRgb);
			   for (int row = 0; row < height; row++) {
				   for (int col = 0; col < width; col++) {
					   int idx = row * width + col;
					   int val = (int)img2[idx];
					   Color c = Color::FromArgb(val, val, val);
					   bmp2->SetPixel(col, row, c);
				   }
			   }
			   g->DrawImage(bmp2, width + 15, 5);
			   delete bmp2;

			   // Draw matched points and connecting lines
			   Pen^ redPen = gcnew Pen(Color::Red, 2.0f);
			   Pen^ greenPen = gcnew Pen(Color::Lime, 2.0f);
			   Pen^ bluePen = gcnew Pen(Color::Cyan, 2.0f);
			   SolidBrush^ redBrush = gcnew SolidBrush(Color::Red);
			   SolidBrush^ greenBrush = gcnew SolidBrush(Color::Lime);

			   // Draw matched points with connecting lines
			   for (int i = 0; i < 4; i++) {
				   int x1 = img1Dots[i].x + 5;
				   int y1 = img1Dots[i].y + 5;
				   int x2 = img2Dots[i].x + width + 15;
				   int y2 = img2Dots[i].y + 5;

				   // Draw circles around points in image 1
				   g->DrawEllipse(redPen, x1 - 5, y1 - 5, 10, 10);
				   g->FillEllipse(redBrush, x1 - 2, y1 - 2, 4, 4);

				   // Draw circles around points in image 2
				   g->DrawEllipse(greenPen, x2 - 5, y2 - 5, 10, 10);
				   g->FillEllipse(greenBrush, x2 - 2, y2 - 2, 4, 4);

				   // Draw connecting line between matched points
				   g->DrawLine(bluePen, x1, y1, x2, y2);

				   // Draw point labels
				   g->DrawString(i.ToString(), gcnew System::Drawing::Font("Arial", 8), Brushes::Black, (float)(x1 + 8), (float)(y1 - 8));
				   g->DrawString(i.ToString(), gcnew System::Drawing::Font("Arial", 8), Brushes::Black, (float)(x2 + 8), (float)(y2 - 8));
			   }

			   delete redPen;
			   delete greenPen;
			   delete bluePen;
			   delete redBrush;
			   delete greenBrush;
			   delete g;

               // Create PictureBox to display combined image
			   PictureBox^ picBox = gcnew PictureBox();
			   picBox->Left = 0;
			   picBox->Top = 0;

			   // Decide whether to downscale to fit the panel
			   double panelW = (double)vizPanel->Width;
			   double panelH = (double)vizPanel->Height;
			   double scale = 1.0;
			   if (combined->Width > 0 && combined->Height > 0) {
				   scale = Math::Min(1.0, Math::Min(panelW / (double)combined->Width, panelH / (double)combined->Height));
			   }

			   if (scale < 1.0) {
				   int dispW = Math::Max(1, (int)System::Math::Round(combined->Width * scale));
				   int dispH = Math::Max(1, (int)System::Math::Round(combined->Height * scale));
				   Bitmap^ scaled = gcnew Bitmap(dispW, dispH, System::Drawing::Imaging::PixelFormat::Format24bppRgb);
				   Graphics^ g2 = Graphics::FromImage(scaled);
				   g2->InterpolationMode = System::Drawing::Drawing2D::InterpolationMode::HighQualityBicubic;
				   g2->DrawImage(combined, System::Drawing::Rectangle(0, 0, dispW, dispH));
				   delete g2;
				   picBox->Image = scaled;
				   picBox->SizeMode = PictureBoxSizeMode::Normal;
				   picBox->Width = dispW;
				   picBox->Height = dispH;
			   }
			   else {
				   picBox->Image = combined;
				   picBox->SizeMode = PictureBoxSizeMode::AutoSize;
			   }

			   vizPanel->Controls->Add(picBox);

               // Create close button
			   Button^ closeBtn = gcnew Button();
			   closeBtn->Text = "Continue";
			   closeBtn->Width = 150;
			   closeBtn->Height = 30;
               closeBtn->Left = (vizForm->ClientSize.Width - closeBtn->Width) / 2;
			   closeBtn->Top = vizForm->ClientSize.Height - closeBtn->Height - 10;
			   closeBtn->DialogResult = System::Windows::Forms::DialogResult::OK;

			   // Create label with match count
			   Label^ matchLbl = gcnew Label();
			   matchLbl->Text = "4 Points Matched";
			   matchLbl->Left = 10;
			   matchLbl->Top = height + 30;
			   matchLbl->AutoSize = true;

			   vizForm->Controls->Add(vizPanel);
			   vizForm->Controls->Add(closeBtn);
			   vizForm->Controls->Add(matchLbl);
			   vizForm->AcceptButton = closeBtn;

			   // Button to open full-resolution view
			   Button^ fullResBtn = gcnew Button();
			   fullResBtn->Text = "View full resolution";
			   fullResBtn->Width = 170;
			   fullResBtn->Height = 30;
               fullResBtn->Left = vizForm->ClientSize.Width - fullResBtn->Width - 24;
			   fullResBtn->Top = 10;
			   fullResBtn->Click += gcnew EventHandler(this, &Image_Stitching::OnViewFullResClick);
			   vizForm->Controls->Add(fullResBtn);

			   // Prepare full-resolution form (hidden) so user can view full-res when requested
			   Form^ fullForm = gcnew Form();
			   fullForm->Text = "Full Resolution Preview";
			   fullForm->StartPosition = FormStartPosition::CenterParent;
			   // size to working area
			   System::Drawing::Rectangle work2 = System::Windows::Forms::Screen::GetWorkingArea(this);
			   fullForm->Width = Math::Min(combined->Width + 40, work2.Width - 40);
			   fullForm->Height = Math::Min(combined->Height + 80, work2.Height - 80);
			   Panel^ fullPanel = gcnew Panel();
			   fullPanel->Left = 5; fullPanel->Top = 5;
			   fullPanel->Width = fullForm->ClientSize.Width - 10;
			   fullPanel->Height = fullForm->ClientSize.Height - 20;
			   fullPanel->AutoScroll = true;
			   fullPanel->BorderStyle = BorderStyle::FixedSingle;
               PictureBox^ fullPic = gcnew PictureBox();
			   // clone combined so disposing the preview dialog does not invalidate the full-res image
			   fullPic->Image = (Bitmap^)combined->Clone(); // full resolution image
			   fullPic->SizeMode = PictureBoxSizeMode::AutoSize;
			   fullPic->Left = 0; fullPic->Top = 0;
			   fullPanel->Controls->Add(fullPic);
			   fullForm->Controls->Add(fullPanel);

			   // Create a Continue button on the full-resolution view so user can close it conveniently
			   Button^ fullCloseBtn = gcnew Button();
			   fullCloseBtn->Text = "Continue";
			   fullCloseBtn->Width = 150;
			   fullCloseBtn->Height = 30;
			   fullCloseBtn->Left = (fullForm->ClientSize.Width - fullCloseBtn->Width) / 2;
			   fullCloseBtn->Top = Math::Min(fullForm->ClientSize.Height - fullCloseBtn->Height - 10, fullPanel->Top + fullPanel->Height + 5);
			   fullCloseBtn->Anchor = static_cast<AnchorStyles>(AnchorStyles::Bottom | AnchorStyles::Left);
			   fullCloseBtn->Click += gcnew EventHandler(this, &Image_Stitching::OnFullResContinueClick);
			   fullForm->Controls->Add(fullCloseBtn);
			   // store for Show on demand
			   this->previewFullResForm = fullForm;

			   // Auto-close preview only if the user enabled the option (checkbox)
			   Timer^ autoCloseTimer = nullptr;
			   if (this->autoClosePreviewChk != nullptr && this->autoClosePreviewChk->Checked) {
				   autoCloseTimer = gcnew Timer();
				   autoCloseTimer->Interval = 1500; // 1.5s
				   autoCloseTimer->Tag = vizForm;
				   autoCloseTimer->Tick += gcnew EventHandler(this, &Image_Stitching::OnAutoCloseMatchPreviewTick);
				   autoCloseTimer->Start();
			   }

			   // Show form as modal dialog
			   vizForm->ShowDialog(this);

			   // Cleanup
			   if (autoCloseTimer != nullptr) {
				   autoCloseTimer->Stop();
				   autoCloseTimer->Tick -= gcnew EventHandler(this, &Image_Stitching::OnAutoCloseMatchPreviewTick);
				   delete autoCloseTimer;
			   }
			   delete vizForm;
		   }

		   void InitializeComponent(void)
		   {
			   this->menuStrip1 = (gcnew System::Windows::Forms::MenuStrip());
			   this->fileToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->openToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->languageToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->englishToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->turkishToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			   this->openFileDialog1 = (gcnew System::Windows::Forms::OpenFileDialog());
			   this->imagePanel = (gcnew System::Windows::Forms::Panel());
			   this->pictureBox4 = (gcnew System::Windows::Forms::PictureBox());
			   this->labelResultS = (gcnew System::Windows::Forms::Label());
			   this->imgIndexLbl = (gcnew System::Windows::Forms::Label());
			   this->label14 = (gcnew System::Windows::Forms::Label());
			   this->saveButton = (gcnew System::Windows::Forms::Button());
               this->autoClosePreviewChk = (gcnew System::Windows::Forms::CheckBox());
			   this->useGpuPhaseChk = (gcnew System::Windows::Forms::CheckBox());
			   this->blendModeLbl = (gcnew System::Windows::Forms::Label());
			   this->blendModeCombo = (gcnew System::Windows::Forms::ComboBox());
			   this->statusPanel = (gcnew System::Windows::Forms::Panel());
			   this->statusTitleLbl = (gcnew System::Windows::Forms::Label());
			   this->statusConfidenceLbl = (gcnew System::Windows::Forms::Label());
			   this->statusConfidenceValLbl = (gcnew System::Windows::Forms::Label());
			   this->statusInlierLbl = (gcnew System::Windows::Forms::Label());
			   this->statusInlierValLbl = (gcnew System::Windows::Forms::Label());
			   this->statusMatcherLbl = (gcnew System::Windows::Forms::Label());
			   this->statusMatcherValLbl = (gcnew System::Windows::Forms::Label());
			   this->statusElapsedLbl = (gcnew System::Windows::Forms::Label());
			   this->statusElapsedValLbl = (gcnew System::Windows::Forms::Label());
			   this->logTextBox = (gcnew System::Windows::Forms::TextBox());
			   this->menuStrip1->SuspendLayout();
			   this->imagePanel->SuspendLayout();
			   this->statusPanel->SuspendLayout();
       // autoClosePreviewChk
		this->autoClosePreviewChk->Location = System::Drawing::Point(840, 46);
		this->autoClosePreviewChk->Name = L"autoClosePreviewChk";
		this->autoClosePreviewChk->Size = System::Drawing::Size(150, 24);
		this->autoClosePreviewChk->TabIndex = 38;
		this->autoClosePreviewChk->Text = Localization::T("AutoClosePreview");
		this->autoClosePreviewChk->AutoSize = true;
		this->autoClosePreviewChk->Checked = false;
		// useGpuPhaseChk
		this->useGpuPhaseChk->Location = System::Drawing::Point(1010, 46);
		this->useGpuPhaseChk->Name = L"useGpuPhaseChk";
		this->useGpuPhaseChk->Size = System::Drawing::Size(210, 24);
		this->useGpuPhaseChk->TabIndex = 39;
		this->useGpuPhaseChk->Text = L"Use GPU phase backend";
		this->useGpuPhaseChk->AutoSize = true;
		this->useGpuPhaseChk->Checked = false;
		// blendModeLbl
		this->blendModeLbl->AutoSize = true;
		this->blendModeLbl->Location = System::Drawing::Point(260, 50);
		this->blendModeLbl->Name = L"blendModeLbl";
		this->blendModeLbl->Size = System::Drawing::Size(43, 17);
		this->blendModeLbl->TabIndex = 40;
		this->blendModeLbl->Text = L"Blend";
		// blendModeCombo
		this->blendModeCombo->DropDownStyle = System::Windows::Forms::ComboBoxStyle::DropDownList;
		this->blendModeCombo->FormattingEnabled = true;
		this->blendModeCombo->Items->AddRange(gcnew cli::array< System::Object^  >(3) { L"Auto", L"Quality", L"Speed" });
		this->blendModeCombo->Location = System::Drawing::Point(315, 46);
		this->blendModeCombo->Name = L"blendModeCombo";
		this->blendModeCombo->Size = System::Drawing::Size(130, 24);
		this->blendModeCombo->TabIndex = 41;
		this->blendModeCombo->SelectedIndex = 0;
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox4))->BeginInit();
			   this->SuspendLayout();
			   // 
			   // menuStrip1
			   // 
			   this->menuStrip1->ImageScalingSize = System::Drawing::Size(20, 20);
			   this->menuStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) { this->fileToolStripMenuItem, this->languageToolStripMenuItem });
			   this->menuStrip1->Location = System::Drawing::Point(0, 0);
			   this->menuStrip1->Name = L"menuStrip1";
			   this->menuStrip1->Padding = System::Windows::Forms::Padding(5, 2, 0, 2);
			   this->menuStrip1->Size = System::Drawing::Size(1939, 28);
			   this->menuStrip1->TabIndex = 2;
			   this->menuStrip1->Text = L"menuStrip1";
			   // 
			   // fileToolStripMenuItem
			   // 
			   this->fileToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) { this->openToolStripMenuItem });
			   this->fileToolStripMenuItem->Name = L"fileToolStripMenuItem";
			   this->fileToolStripMenuItem->Size = System::Drawing::Size(46, 24);
			   this->fileToolStripMenuItem->Text = Localization::T("MenuFile");
			   // 
			   // openToolStripMenuItem
			   // 
			   this->openToolStripMenuItem->Name = L"openToolStripMenuItem";
			   this->openToolStripMenuItem->Size = System::Drawing::Size(128, 26);
			   this->openToolStripMenuItem->Text = Localization::T("MenuOpen");
			   this->openToolStripMenuItem->Click += gcnew System::EventHandler(this, &Image_Stitching::openToolStripMenuItem_Click);
			   // 
			   // languageToolStripMenuItem
			   // 
			   this->languageToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) { this->englishToolStripMenuItem, this->turkishToolStripMenuItem });
			   this->languageToolStripMenuItem->Name = L"languageToolStripMenuItem";
			   this->languageToolStripMenuItem->Size = System::Drawing::Size(90, 24);
			   this->languageToolStripMenuItem->Text = Localization::T("MenuLanguage");
			   // 
			   // englishToolStripMenuItem
			   // 
			   this->englishToolStripMenuItem->Name = L"englishToolStripMenuItem";
			   this->englishToolStripMenuItem->Size = System::Drawing::Size(153, 26);
			   this->englishToolStripMenuItem->Text = Localization::T("MenuEnglish");
			   this->englishToolStripMenuItem->Click += gcnew System::EventHandler(this, &Image_Stitching::englishToolStripMenuItem_Click);
			   // 
			   // turkishToolStripMenuItem
			   // 
			   this->turkishToolStripMenuItem->Name = L"turkishToolStripMenuItem";
			   this->turkishToolStripMenuItem->Size = System::Drawing::Size(153, 26);
			   this->turkishToolStripMenuItem->Text = Localization::T("MenuTurkish");
			   this->turkishToolStripMenuItem->Click += gcnew System::EventHandler(this, &Image_Stitching::turkishToolStripMenuItem_Click);
			   // 
			   // openFileDialog1
			   // 
			   this->openFileDialog1->FileName = String::Empty;
			   this->openFileDialog1->Filter = Localization::T("OpenDialogFilter");
			   this->openFileDialog1->Multiselect = true;
			   this->openFileDialog1->AutoUpgradeEnabled = false;
			   // 
			   // imagePanel
			   // 
			   this->imagePanel->AutoScroll = true;
			   this->imagePanel->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
			   this->imagePanel->Location = System::Drawing::Point(27, 81);
			   this->imagePanel->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->imagePanel->Name = L"imagePanel";
			   this->imagePanel->Size = System::Drawing::Size(1200, 754);
			   this->imagePanel->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Left | System::Windows::Forms::AnchorStyles::Right);
			   // 
			   // pictureBox4
			   // 
			   this->pictureBox4->Location = System::Drawing::Point(0, 0);
			   this->pictureBox4->Margin = System::Windows::Forms::Padding(3, 2, 3, 2);
			   this->pictureBox4->Name = L"pictureBox4";
			   this->pictureBox4->Size = System::Drawing::Size(800, 754);
			   this->pictureBox4->SizeMode = System::Windows::Forms::PictureBoxSizeMode::Normal;
			   this->pictureBox4->TabIndex = 7;
			   this->pictureBox4->TabStop = false;
			   // 
			   // labelResultS
			   // 
			   this->labelResultS->AutoSize = true;
			   this->labelResultS->Location = System::Drawing::Point(24, 50);
			   this->labelResultS->Name = L"labelResultS";
			   this->labelResultS->Size = System::Drawing::Size(20, 17);
			   this->labelResultS->TabIndex = 31;
			   this->labelResultS->Text = L"...";
			   // 
			   // imgIndexLbl
			   // 
			   this->imgIndexLbl->AutoSize = true;
			   this->imgIndexLbl->Location = System::Drawing::Point(211, 50);
			   this->imgIndexLbl->Name = L"imgIndexLbl";
			   this->imgIndexLbl->Size = System::Drawing::Size(20, 17);
			   this->imgIndexLbl->TabIndex = 35;
			   this->imgIndexLbl->Text = L"...";
			   // 
			   // label14
			   // 
			   this->label14->AutoSize = true;
			   this->label14->Location = System::Drawing::Point(147, 50);
			   this->label14->Name = L"label14";
			   this->label14->Size = System::Drawing::Size(58, 17);
			   this->label14->TabIndex = 34;
			   this->label14->Text = Localization::T("LabelImage");
			   // 
			   // saveButton
			   // 
			   this->saveButton->Location = System::Drawing::Point(755, 40);
			   this->saveButton->Name = L"saveButton";
			   this->saveButton->Size = System::Drawing::Size(72, 36);
			   this->saveButton->TabIndex = 36;
			   this->saveButton->Text = Localization::T("ButtonSave");
			   this->saveButton->UseVisualStyleBackColor = true;
			   this->saveButton->Click += gcnew System::EventHandler(this, &Image_Stitching::saveButton_Click);
			   // 
			   // statusPanel
			   // 
			   this->statusPanel->BorderStyle = System::Windows::Forms::BorderStyle::FixedSingle;
			   this->statusPanel->Controls->Add(this->statusElapsedValLbl);
			   this->statusPanel->Controls->Add(this->statusElapsedLbl);
			   this->statusPanel->Controls->Add(this->statusMatcherValLbl);
			   this->statusPanel->Controls->Add(this->statusMatcherLbl);
			   this->statusPanel->Controls->Add(this->statusInlierValLbl);
			   this->statusPanel->Controls->Add(this->statusInlierLbl);
			   this->statusPanel->Controls->Add(this->statusConfidenceValLbl);
			   this->statusPanel->Controls->Add(this->statusConfidenceLbl);
			   this->statusPanel->Controls->Add(this->statusTitleLbl);
			   this->statusPanel->Location = System::Drawing::Point(850, 35);
			   this->statusPanel->Name = L"statusPanel";
			   this->statusPanel->Size = System::Drawing::Size(377, 84);
			   this->statusPanel->TabIndex = 38;
			   this->statusPanel->Anchor = static_cast<System::Windows::Forms::AnchorStyles>((System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Right));
			   // 
			   // statusTitleLbl
			   // 
			   this->statusTitleLbl->AutoSize = true;
			   this->statusTitleLbl->Font = (gcnew System::Drawing::Font(L"Segoe UI", 8.5F, System::Drawing::FontStyle::Bold, System::Drawing::GraphicsUnit::Point,
				   static_cast<System::Byte>(0)));
			   this->statusTitleLbl->Location = System::Drawing::Point(10, 6);
			   this->statusTitleLbl->Name = L"statusTitleLbl";
			   this->statusTitleLbl->Size = System::Drawing::Size(50, 20);
			   this->statusTitleLbl->TabIndex = 0;
			   this->statusTitleLbl->Text = L"Status";
			   // 
			   // statusConfidenceLbl
			   // 
			   this->statusConfidenceLbl->AutoSize = true;
			   this->statusConfidenceLbl->Location = System::Drawing::Point(10, 31);
			   this->statusConfidenceLbl->Name = L"statusConfidenceLbl";
			   this->statusConfidenceLbl->Size = System::Drawing::Size(77, 16);
			   this->statusConfidenceLbl->TabIndex = 1;
			   this->statusConfidenceLbl->Text = L"Confidence:";
			   // 
			   // statusConfidenceValLbl
			   // 
			   this->statusConfidenceValLbl->AutoSize = true;
			   this->statusConfidenceValLbl->Location = System::Drawing::Point(96, 31);
			   this->statusConfidenceValLbl->Name = L"statusConfidenceValLbl";
			   this->statusConfidenceValLbl->Size = System::Drawing::Size(30, 16);
			   this->statusConfidenceValLbl->TabIndex = 2;
			   this->statusConfidenceValLbl->Text = L"N/A";
			   // 
			   // statusInlierLbl
			   // 
			   this->statusInlierLbl->AutoSize = true;
			   this->statusInlierLbl->Location = System::Drawing::Point(10, 54);
			   this->statusInlierLbl->Name = L"statusInlierLbl";
			   this->statusInlierLbl->Size = System::Drawing::Size(69, 16);
			   this->statusInlierLbl->TabIndex = 3;
			   this->statusInlierLbl->Text = L"Inlier ratio:";
			   // 
			   // statusInlierValLbl
			   // 
			   this->statusInlierValLbl->AutoSize = true;
			   this->statusInlierValLbl->Location = System::Drawing::Point(96, 54);
			   this->statusInlierValLbl->Name = L"statusInlierValLbl";
			   this->statusInlierValLbl->Size = System::Drawing::Size(30, 16);
			   this->statusInlierValLbl->TabIndex = 4;
			   this->statusInlierValLbl->Text = L"N/A";
			   // 
			   // statusMatcherLbl
			   // 
			   this->statusMatcherLbl->AutoSize = true;
			   this->statusMatcherLbl->Location = System::Drawing::Point(186, 31);
			   this->statusMatcherLbl->Name = L"statusMatcherLbl";
			   this->statusMatcherLbl->Size = System::Drawing::Size(56, 16);
			   this->statusMatcherLbl->TabIndex = 5;
			   this->statusMatcherLbl->Text = L"Matcher:";
			   // 
			   // statusMatcherValLbl
			   // 
			   this->statusMatcherValLbl->AutoSize = true;
			   this->statusMatcherValLbl->Location = System::Drawing::Point(253, 31);
			   this->statusMatcherValLbl->Name = L"statusMatcherValLbl";
			   this->statusMatcherValLbl->Size = System::Drawing::Size(30, 16);
			   this->statusMatcherValLbl->TabIndex = 6;
			   this->statusMatcherValLbl->Text = L"N/A";
			   // 
			   // statusElapsedLbl
			   // 
			   this->statusElapsedLbl->AutoSize = true;
			   this->statusElapsedLbl->Location = System::Drawing::Point(186, 54);
			   this->statusElapsedLbl->Name = L"statusElapsedLbl";
			   this->statusElapsedLbl->Size = System::Drawing::Size(54, 16);
			   this->statusElapsedLbl->TabIndex = 7;
			   this->statusElapsedLbl->Text = L"Elapsed:";
			   // 
			   // statusElapsedValLbl
			   // 
			   this->statusElapsedValLbl->AutoSize = true;
			   this->statusElapsedValLbl->Location = System::Drawing::Point(253, 54);
			   this->statusElapsedValLbl->Name = L"statusElapsedValLbl";
			   this->statusElapsedValLbl->Size = System::Drawing::Size(39, 16);
			   this->statusElapsedValLbl->TabIndex = 8;
			   this->statusElapsedValLbl->Text = L"00:00";
			   // 
			   // logTextBox
			   // 
			   this->logTextBox->Location = System::Drawing::Point(1245, 81);
			   this->logTextBox->Multiline = true;
			   this->logTextBox->Name = L"logTextBox";
			   this->logTextBox->ReadOnly = true;
			   this->logTextBox->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
			   this->logTextBox->Size = System::Drawing::Size(670, 754);
			   this->logTextBox->TabIndex = 37;
			   this->logTextBox->Anchor = static_cast<System::Windows::Forms::AnchorStyles>(System::Windows::Forms::AnchorStyles::Top | System::Windows::Forms::AnchorStyles::Bottom | System::Windows::Forms::AnchorStyles::Right);
			   // 
			   // Image_Stitching
			   // 
			   this->AutoScaleDimensions = System::Drawing::SizeF(8, 16);
			   this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			   this->ClientSize = System::Drawing::Size(1939, 889);
			   this->Controls->Add(this->logTextBox);
               this->Controls->Add(this->statusPanel);
			   this->Controls->Add(this->saveButton);
			   this->Controls->Add(this->autoClosePreviewChk);
			   this->Controls->Add(this->useGpuPhaseChk);
			   this->Controls->Add(this->blendModeCombo);
			   this->Controls->Add(this->blendModeLbl);
			   this->Controls->Add(this->imgIndexLbl);
			   this->Controls->Add(this->label14);
			   this->Controls->Add(this->labelResultS);
			   this->imagePanel->Controls->Add(this->pictureBox4);
			   this->Controls->Add(this->imagePanel);
			   this->Controls->Add(this->menuStrip1);
			   this->MainMenuStrip = this->menuStrip1;
			   this->Margin = System::Windows::Forms::Padding(4);
			   this->Name = L"Image_Stitching";
			   this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			   this->Text = Localization::T("FormTitle");
			   this->WindowState = System::Windows::Forms::FormWindowState::Maximized;
			   this->menuStrip1->ResumeLayout(false);
			   this->menuStrip1->PerformLayout();
			   this->imagePanel->ResumeLayout(false);
			   this->statusPanel->ResumeLayout(false);
			   this->statusPanel->PerformLayout();
			   (cli::safe_cast<System::ComponentModel::ISupportInitialize^>(this->pictureBox4))->EndInit();
			   this->ResumeLayout(false);
			   this->PerformLayout();

		   }
#pragma endregion

	private:
		void ApplyLocalization() {
			this->fileToolStripMenuItem->Text = Localization::T("MenuFile");
			this->openToolStripMenuItem->Text = Localization::T("MenuOpen");
			this->languageToolStripMenuItem->Text = Localization::T("MenuLanguage");
			this->englishToolStripMenuItem->Text = Localization::T("MenuEnglish");
			this->turkishToolStripMenuItem->Text = Localization::T("MenuTurkish");
			this->saveButton->Text = Localization::T("ButtonSave");
			this->label14->Text = Localization::T("LabelImage");
        if (this->autoClosePreviewChk != nullptr)
			this->autoClosePreviewChk->Text = Localization::T("AutoClosePreview");
		if (this->useGpuPhaseChk != nullptr)
			this->useGpuPhaseChk->Text = L"Use GPU phase backend";
		if (this->blendModeLbl != nullptr)
			this->blendModeLbl->Text = Localization::T("BlendModeLabel");
		if (this->blendModeCombo != nullptr) {
			int selectedIndex = this->blendModeCombo->SelectedIndex;
			this->blendModeCombo->Items->Clear();
			this->blendModeCombo->Items->Add(Localization::T("BlendModeAuto"));
			this->blendModeCombo->Items->Add(Localization::T("BlendModeQuality"));
			this->blendModeCombo->Items->Add(Localization::T("BlendModeSpeed"));
			if (selectedIndex < 0 || selectedIndex > 2)
				selectedIndex = 0;
			this->blendModeCombo->SelectedIndex = selectedIndex;
		}
			this->statusTitleLbl->Text = Localization::T("StatusTitle");
			this->statusConfidenceLbl->Text = Localization::T("StatusConfidence");
			this->statusInlierLbl->Text = Localization::T("StatusInlierRatio");
			this->statusMatcherLbl->Text = Localization::T("StatusMatcher");
			this->statusElapsedLbl->Text = Localization::T("StatusElapsed");
			if (statusMatcherValLbl != nullptr)
				statusMatcherValLbl->Text = Localization::T(statusMatcherKey);
			if (statusConfidenceValLbl != nullptr && statusPhaseConfidence < 0.0)
				statusConfidenceValLbl->Text = Localization::T("StatusValueNA");
			if (statusInlierValLbl != nullptr && statusInlierRatio < 0.0)
				statusInlierValLbl->Text = Localization::T("StatusValueNA");
			this->Text = Localization::T("FormTitle");
			this->openFileDialog1->FileName = String::Empty;
			this->openFileDialog1->Filter = Localization::T("OpenDialogFilter");
		}

	private: System::Void englishToolStripMenuItem_Click(System::Object^ sender, System::EventArgs^ e) {
		Localization::SetLanguage(Localization::Language::English);
		ApplyLocalization();
	}

	private: System::Void turkishToolStripMenuItem_Click(System::Object^ sender, System::EventArgs^ e) {
		Localization::SetLanguage(Localization::Language::Turkish);
		ApplyLocalization();
	}

	private: System::Void openToolStripMenuItem_Click(System::Object^ sender, System::EventArgs^ e) {
		try {
		this->UseWaitCursor = false;
		System::Windows::Forms::Cursor::Current = Cursors::Default;
		array<String^>^ selectedFiles = nullptr;
		if (startupAutoRun && startupInputFiles != nullptr && startupInputFiles->Length > 0) {
			selectedFiles = startupInputFiles;
			if (!String::IsNullOrWhiteSpace(startupArgsError))
				AppendLog(String::Format("Startup argument error: {0}", startupArgsError));
			if (logTextBox != nullptr) logTextBox->Clear();
			AppendLog(String::Format("Batch mode: inputs={0}, requested backend={1}, blend={2}, autoClosePreview={3}, log={4}",
				selectedFiles->Length,
				(this->useGpuPhaseChk != nullptr && this->useGpuPhaseChk->Checked) ? "GPU" : "CPU",
				GetBlendModeName(startupBlendMode),
				(this->autoClosePreviewChk != nullptr && this->autoClosePreviewChk->Checked) ? "true" : "false",
				Path::GetFullPath(startupLogPath)));
		}
		else {
			openFileDialog1->RestoreDirectory = true;
			openFileDialog1->CheckFileExists = true;
			openFileDialog1->CheckPathExists = true;
			openFileDialog1->ValidateNames = true;
			openFileDialog1->FileName = String::Empty;
			if (String::IsNullOrWhiteSpace(openFileDialog1->InitialDirectory)) {
				openFileDialog1->InitialDirectory = Environment::GetFolderPath(Environment::SpecialFolder::MyPictures);
			}

			if (openFileDialog1->ShowDialog(this) == System::Windows::Forms::DialogResult::OK) {
				selectedFiles = openFileDialog1->FileNames;
			}
		}

		if (selectedFiles != nullptr) {

			long size;
			int width, height;
			int width1, height1;
			int expectedWidth = 0, expectedHeight = 0;
			xy position;
			bool stitchAborted = false;
			String^ abortMessage = nullptr;
			int selectedCount = selectedFiles->GetLength(0);
			if (selectedCount < 2) {
				if (!startupAutoRun)
					MessageBox::Show("Please select at least two images to stitch.");
				else
					AppendLog("Please select at least two images to stitch.");
				return;
			}
			SetPhaseCorrelationBackend((this->useGpuPhaseChk != nullptr && this->useGpuPhaseChk->Checked)
				? PhaseCorrelationBackendGpu
				: PhaseCorrelationBackendCpu);
			imgCounter = 0;
			lineCounter = 0;
			int totalPairs = selectedCount - 1;
			if (logTextBox != nullptr) logTextBox->Clear();
			AppendLog(String::Format(Localization::T("LogStitchStart"), totalPairs));
			AppendLog(String::Format("Phase backend: {0}", gcnew String(GetPhaseCorrelationBackendName())));
			if (this->useGpuPhaseChk != nullptr && this->useGpuPhaseChk->Checked && !IsPhaseCorrelationGpuAvailable()) {
				AppendLog("GPU backend unavailable on this build/device (CUDA/OpenCL not active); using CPU fallback.");
			}
			latestColorOutput = DeleteByteArray(latestColorOutput);
			latestColorWidth = 0;
			latestColorHeight = 0;
			stitchStartTime = DateTime::Now;
			stitchTimerActive = true;
			ResetStatusPanel();

			buffer1 = nullptr;
			buffer2 = nullptr;
			panorama1 = nullptr;
			panorama2 = nullptr;
			intensity1 = nullptr;
			intensity2 = nullptr;
			currPanoSize = nullptr;
			for (int i = 0; i < 2; i++) {
				outReal[i] = nullptr;
				outImag[i] = nullptr;
			}

			LaplacePyramid1 = DeleteByte2PtrArray(LaplacePyramid1);
			LaplacePyramid2 = DeleteByte2PtrArray(LaplacePyramid2);
			LaplacePyramid1 = new BYTE2 * [4]();
			LaplacePyramid2 = new BYTE2 * [4]();

			for (int i = 0; i < 3; i++) {
				match1[i] = DeleteDoubleArray(match1[i]);
				match2[i] = DeleteDoubleArray(match2[i]);
				match1[i] = new double[4];
				match2[i] = new double[4];
			}

			while (imgCounter < selectedFiles->GetLength(0) - 1) {
				double pairFft1Ms = -1.0;
				double pairFft2Ms = -1.0;
				double pairPhaseMs = -1.0;
				double pairFallbackMs = -1.0;
				double pairFallbackLocalMs = -1.0;
				double pairFallbackGlobalMs = -1.0;
				double pairFallbackEdgeMs = -1.0;
				double pairBlendMs = -1.0;
				AppendLog(String::Format(Localization::T("LogProcessingPair"), imgCounter + 1, totalPairs));

				if (imgCounter == 0) {
					AppendLog(Localization::T("LogLoadingFirstTwo"));
					CString path1 = selectedFiles[imgCounter];
					buffer1 = LoadImage(width, height, size, (LPCTSTR)path1);
					if (buffer1 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to load image: {0}", selectedFiles[imgCounter]);
						AppendLog(abortMessage);
						break;
					}
					intensity1 = ConvertBMPToIntensity(buffer1, width, height);
					if (intensity1 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to convert image intensity: {0}", selectedFiles[imgCounter]);
						AppendLog(abortMessage);
						break;
					}
					expectedWidth = width;
					expectedHeight = height;

					outReal[0] = new double[width * height];
					outImag[0] = new double[width * height];
					AppendLog(Localization::T("LogFFTFirst"));
					auto tFft1Start = std::chrono::high_resolution_clock::now();
					FFT2D(intensity1, outReal[0], outImag[0], width, height);
					auto tFft1End = std::chrono::high_resolution_clock::now();
					pairFft1Ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tFft1End - tFft1Start).count();

					CString path2 = selectedFiles[imgCounter + 1];
					int nextWidth = 0, nextHeight = 0;
					buffer2 = LoadImage(nextWidth, nextHeight, size, (LPCTSTR)path2);
					if (buffer2 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to load image: {0}", selectedFiles[imgCounter + 1]);
						AppendLog(abortMessage);
						break;
					}
					if (nextWidth != expectedWidth || nextHeight != expectedHeight) {
						stitchAborted = true;
						abortMessage = String::Format("All images must have the same dimensions. Expected {0}x{1}, got {2}x{3}.", expectedWidth, expectedHeight, nextWidth, nextHeight);
						AppendLog(abortMessage);
						break;
					}
					intensity2 = ConvertBMPToIntensity(buffer2, expectedWidth, expectedHeight);
					if (intensity2 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to convert image intensity: {0}", selectedFiles[imgCounter + 1]);
						AppendLog(abortMessage);
						break;
					}
					width = expectedWidth;
					height = expectedHeight;

					outReal[1] = new double[width * height];
					outImag[1] = new double[width * height];
					AppendLog(Localization::T("LogFFTSecond"));
					auto tFft2Start = std::chrono::high_resolution_clock::now();
					FFT2D(intensity2, outReal[1], outImag[1], width, height);
					auto tFft2End = std::chrono::high_resolution_clock::now();
					pairFft2Ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tFft2End - tFft2Start).count();

					AppendLog(Localization::T("LogPhaseCorrelationRunning"));
					auto tPhaseStart = std::chrono::high_resolution_clock::now();
					double* POC = PhaseCorrelationWithUiHeartbeat(outReal[0], outImag[0], outReal[1], outImag[1], width, height);
					auto tPhaseEnd = std::chrono::high_resolution_clock::now();
					pairPhaseMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tPhaseEnd - tPhaseStart).count();
					AppendLog(Localization::T("LogPeakSearch"));
					MaxPOC(POC, pocDot, width, height);
					AppendLog(Localization::T("LogComputingShift"));

					outReal[0] = DeleteDoubleArray(outReal[0]);
					outImag[0] = DeleteDoubleArray(outImag[0]);
					POC = DeleteDoubleArray(POC);

					xy signedShift = { 0, 0 };
					float signedRotationDeg = 0.0f;
					float fallbackCandidateScore = -1.0f;
					float fallbackCandidateObjective = -1.0f;
					bool fallbackCandidateApplied = false;
					double fallbackLocalMs = -1.0;
					double fallbackGlobalMs = -1.0;
					double fallbackEdgeMs = -1.0;
					currCornerID = ZoneDetection(intensity1, intensity2, width, height, currVec, pocDot, &signedShift, &signedRotationDeg, &fallbackCandidateScore, &fallbackCandidateObjective, &fallbackCandidateApplied, &fallbackLocalMs, &fallbackGlobalMs, &fallbackEdgeMs);
					pairFallbackLocalMs = fallbackLocalMs;
					pairFallbackGlobalMs = fallbackGlobalMs;
					pairFallbackEdgeMs = fallbackEdgeMs;
					pairFallbackMs = 0.0;
					if (fallbackLocalMs > 0.0) pairFallbackMs += fallbackLocalMs;
					if (fallbackGlobalMs > 0.0) pairFallbackMs += fallbackGlobalMs;
					if (fallbackEdgeMs > 0.0) pairFallbackMs += fallbackEdgeMs;
					if (fallbackCandidateObjective >= -0.5f) {
						AppendLog(String::Format("Fallback candidate: score={0:F3}, objective={1:F3}, applied={2}", fallbackCandidateScore, fallbackCandidateObjective, fallbackCandidateApplied ? "yes" : "no"));
					}
					float phaseConfidence = PhaseShiftConfidence(intensity1, intensity2, width, height, pocDot);
					AppendLog(String::Format(Localization::T("LogPhaseConfidence"), phaseConfidence));
					UpdateStatusPanel("MatcherPhaseFast", phaseConfidence, -1.0, false);

					xy featureImg1[4] = {};
					xy featureImg2[4] = {};
					xy featureShift = { 0, 0 };
					float featureInlierRatio = 0.0f;
					bool usedFeatureFallback = false;
					bool useTranslationModel = false;

					if (phaseConfidence < 0.70f) {
						AppendLog(Localization::T("LogFeatureFallbackStart"));
						usedFeatureFallback = FindFeatureMatchesRansac(intensity1, intensity2, width, height, featureImg1, featureImg2, featureInlierRatio, &featureShift);
						if (usedFeatureFallback) {
							currVec->x = featureShift.x;
							currVec->y = featureShift.y;
							int signedDx = featureImg1[0].x - featureImg2[0].x;
							int signedDy = featureImg1[0].y - featureImg2[0].y;
							if (signedDx >= 0 && signedDy >= 0) currCornerID = 0;
							else if (signedDx < 0 && signedDy >= 0) currCornerID = 1;
							else if (signedDx >= 0 && signedDy < 0) currCornerID = 2;
							else currCornerID = 3;
							AppendLog(String::Format(Localization::T("LogFeatureFallbackUsed"), featureInlierRatio));
							UpdateStatusPanel("MatcherFeatureRansac", phaseConfidence, featureInlierRatio, true);
						}
						else {
							AppendLog(Localization::T("LogFeatureFallbackFailed"));
							UpdateStatusPanel("MatcherPhaseFast", phaseConfidence, -1.0, false);
							useTranslationModel = true;
						}
					}

					const bool reliableModel = (phaseConfidence >= 0.70f) || usedFeatureFallback || useTranslationModel;

					xy* img1Dots = nullptr;
					xy* img2Dots = nullptr;
					if (!reliableModel) {
						AppendLog(Localization::T("LogUnreliablePairRejected"));
					}
					else if (usedFeatureFallback) {
						img1Dots = new xy[4];
						img2Dots = new xy[4];
						for (int i = 0; i < 4; i++) {
							img1Dots[i] = featureImg1[i];
							img2Dots[i] = featureImg2[i];
						}
					}
					else {
						if (useTranslationModel) {
							img1Dots = new xy[4];
							img2Dots = new xy[4];
							if (!BuildDotsFromSignedShiftAndRotation(width, height, signedShift.x, signedShift.y, signedRotationDeg, img1Dots, img2Dots)) {
								img1Dots = DeleteXyArray(img1Dots);
								img2Dots = DeleteXyArray(img2Dots);
							}
						}
						else {
							img1Dots = Rand4Dots(currCornerID, pocDot, width, height);
							if (img1Dots != NULL)
								img2Dots = MatchingDots(currCornerID, img1Dots, currVec);
						}
					}

					bool useDirectTranslationHomography = useTranslationModel && (img1Dots == NULL || img2Dots == NULL);
					if (img1Dots == NULL || img2Dots == NULL) {
						if (!useDirectTranslationHomography) {
							AppendLog(Localization::T("LogMatchFailed"));
							if (!startupAutoRun)
								MessageBox::Show(Localization::T("MsgMissingMatches"));
						}
						else {
							AppendLog("Point-pair matching unavailable; using direct translation homography.");
						}
					}

					if (img1Dots != NULL || useDirectTranslationHomography) {
						double** homography = nullptr;
						if (!useDirectTranslationHomography) {
							AppendLog(Localization::T("LogMatchingHomography"));
							if (!startupAutoRun) {
								ShowMatchVisualization(intensity1, intensity2, width, height, img1Dots, img2Dots);
								AppendLog("Match visualization closed - proceeding with panorama generation");
							}
							else {
								AppendLog("Batch mode: skipping match visualization.");
							}

							for (int i = 0; i < 4; i++) {
								match1[0][i] = double(img1Dots[i].x);
								match1[1][i] = double(img1Dots[i].y);
								match1[2][i] = 1.0;
								match2[0][i] = double(img2Dots[i].x);
								match2[1][i] = double(img2Dots[i].y);
								match2[2][i] = 1.0;
							}
							homography = homography2d(match1, match2, 4);
						}
						else {
							homography = CreateTranslationHomography(currCornerID, currVec);
						}

						LineUpdate(currCornerID, currVec);
						if (useTranslationModel) {
							AppendLog(Localization::T("LogTranslationFallback"));
							AppendLog(String::Format("Translation shift used: dx={0}, dy={1}, corner={2}", signedShift.x, signedShift.y, currCornerID));
							AppendLog(String::Format("Estimated rotation used: {0:F2} deg", signedRotationDeg));
						}

						currPanoSize = ReSizePanorama(homography, width, height, width, height, position);
						if (currPanoSize != nullptr) {
							labelResultS->Text = currPanoSize->x + " x " + currPanoSize->y;

							if (lineCounter == 0)
								isFirstLine = true;
							else isFirstLine = false;

							AppendLog(Localization::T("LogBlendingPanorama"));
							auto tBlendStart = std::chrono::high_resolution_clock::now();
							BlendMode resolvedBlendMode = ResolveBlendMode(useTranslationModel);
							if (resolvedBlendMode == BlendMode::Speed) {
								AppendLog("Using fast translation blend path.");
								panorama1 = FastTranslationBlend(homography, buffer1, buffer2, width, height, width, height, *currPanoSize, position);
							}
							else {
								AppendLog("Using Laplacian blend path.");
								LaplacePyramid(homography, buffer1, buffer2, width, height, width, height, LaplacePyramid1, LaplacePyramid2, width1, height1, *currPanoSize, position);
								panorama1 = PanaromicImage(homography, width, height, *currPanoSize, position, LaplacePyramid1, LaplacePyramid2, width1, height1, width, height, isFirstLine, currCornerID, currVec);
							}
							if (!startupAutoRun)
								ShowColorImage(panorama1, currPanoSize->x, currPanoSize->y);
							auto tBlendEnd = std::chrono::high_resolution_clock::now();
							pairBlendMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tBlendEnd - tBlendStart).count();
							AppendLog(String::Format(Localization::T("LogPanoramaUpdated"), currPanoSize->x, currPanoSize->y));

							prevPanoSize->x = currPanoSize->x;
							prevPanoSize->y = currPanoSize->y;
							prevVec->x = 0;
							prevVec->y = 0;
							UpdatePrevVec(currCornerID, prevVec, currVec);
						}

						DeleteXyArray(img2Dots);
						for (size_t i = 0; i < 3; i++)
							DeleteDoubleArray(homography[i]);
						homography = DeleteDoublePtrArray(homography);
					}

					intensity1 = DeleteByteArray(intensity1);
					intensity1 = intensity2;
					intensity2 = nullptr;
					currPanoSize = DeleteXyObject(currPanoSize);
					buffer1 = DeleteByteArray(buffer1);
					img1Dots = DeleteXyArray(img1Dots);
				}
				else {
					AppendLog(Localization::T("LogLoadingNext"));
					buffer1 = buffer2;
					outReal[0] = outReal[1];
					outImag[0] = outImag[1];

					CString path = selectedFiles[imgCounter + 1];
					int nextWidth = 0, nextHeight = 0;
					buffer2 = LoadImage(nextWidth, nextHeight, size, (LPCTSTR)path);
					if (buffer2 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to load image: {0}", selectedFiles[imgCounter + 1]);
						AppendLog(abortMessage);
						break;
					}
					if (nextWidth != expectedWidth || nextHeight != expectedHeight) {
						stitchAborted = true;
						abortMessage = String::Format("All images must have the same dimensions. Expected {0}x{1}, got {2}x{3}.", expectedWidth, expectedHeight, nextWidth, nextHeight);
						AppendLog(abortMessage);
						break;
					}
					intensity2 = ConvertBMPToIntensity(buffer2, expectedWidth, expectedHeight);
					if (intensity2 == nullptr) {
						stitchAborted = true;
						abortMessage = String::Format("Failed to convert image intensity: {0}", selectedFiles[imgCounter + 1]);
						AppendLog(abortMessage);
						break;
					}
					width = expectedWidth;
					height = expectedHeight;

					outReal[1] = new double[width * height];
					outImag[1] = new double[width * height];

					AppendLog(Localization::T("LogFFTSecond"));
					auto tFft2Start = std::chrono::high_resolution_clock::now();
					FFT2D(intensity2, outReal[1], outImag[1], width, height);
					auto tFft2End = std::chrono::high_resolution_clock::now();
					pairFft2Ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tFft2End - tFft2Start).count();

					AppendLog(Localization::T("LogPhaseCorrelationRunning"));
					auto tPhaseStart = std::chrono::high_resolution_clock::now();
					double* POC = PhaseCorrelationWithUiHeartbeat(outReal[0], outImag[0], outReal[1], outImag[1], width, height);
					auto tPhaseEnd = std::chrono::high_resolution_clock::now();
					pairPhaseMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tPhaseEnd - tPhaseStart).count();
					AppendLog(Localization::T("LogPeakSearch"));
					MaxPOC(POC, pocDot, width, height);
					AppendLog(Localization::T("LogComputingShift"));

						outReal[0] = DeleteDoubleArray(outReal[0]);
						outImag[0] = DeleteDoubleArray(outImag[0]);
						POC = DeleteDoubleArray(POC);

						xy signedShift = { 0, 0 };
						float signedRotationDeg = 0.0f;
						float fallbackCandidateScore = -1.0f;
						float fallbackCandidateObjective = -1.0f;
						bool fallbackCandidateApplied = false;
						double fallbackLocalMs = -1.0;
						double fallbackGlobalMs = -1.0;
						double fallbackEdgeMs = -1.0;
						currCornerID = ZoneDetection(intensity1, intensity2, width, height, currVec, pocDot, &signedShift, &signedRotationDeg, &fallbackCandidateScore, &fallbackCandidateObjective, &fallbackCandidateApplied, &fallbackLocalMs, &fallbackGlobalMs, &fallbackEdgeMs);
						pairFallbackLocalMs = fallbackLocalMs;
						pairFallbackGlobalMs = fallbackGlobalMs;
						pairFallbackEdgeMs = fallbackEdgeMs;
						pairFallbackMs = 0.0;
						if (fallbackLocalMs > 0.0) pairFallbackMs += fallbackLocalMs;
						if (fallbackGlobalMs > 0.0) pairFallbackMs += fallbackGlobalMs;
						if (fallbackEdgeMs > 0.0) pairFallbackMs += fallbackEdgeMs;
						if (fallbackCandidateObjective >= -0.5f) {
							AppendLog(String::Format("Fallback candidate: score={0:F3}, objective={1:F3}, applied={2}", fallbackCandidateScore, fallbackCandidateObjective, fallbackCandidateApplied ? "yes" : "no"));
						}
						float phaseConfidence = PhaseShiftConfidence(intensity1, intensity2, width, height, pocDot);
						AppendLog(String::Format(Localization::T("LogPhaseConfidence"), phaseConfidence));
						UpdateStatusPanel("MatcherPhaseFast", phaseConfidence, -1.0, false);

						xy featureImg1[4] = {};
						xy featureImg2[4] = {};
						xy featureShift = { 0, 0 };
						float featureInlierRatio = 0.0f;
						bool usedFeatureFallback = false;
						bool useTranslationModel = false;

						if (phaseConfidence < 0.70f) {
							AppendLog(Localization::T("LogFeatureFallbackStart"));
							usedFeatureFallback = FindFeatureMatchesRansac(intensity1, intensity2, width, height, featureImg1, featureImg2, featureInlierRatio, &featureShift);
						if (usedFeatureFallback) {
							currVec->x = featureShift.x;
							currVec->y = featureShift.y;
							int signedDx = featureImg1[0].x - featureImg2[0].x;
							int signedDy = featureImg1[0].y - featureImg2[0].y;
							if (signedDx >= 0 && signedDy >= 0) currCornerID = 0;
							else if (signedDx < 0 && signedDy >= 0) currCornerID = 1;
							else if (signedDx >= 0 && signedDy < 0) currCornerID = 2;
							else currCornerID = 3;
							AppendLog(String::Format(Localization::T("LogFeatureFallbackUsed"), featureInlierRatio));
							UpdateStatusPanel("MatcherFeatureRansac", phaseConfidence, featureInlierRatio, true);
						}
						else {
							AppendLog(Localization::T("LogFeatureFallbackFailed"));
							UpdateStatusPanel("MatcherPhaseFast", phaseConfidence, -1.0, false);
							useTranslationModel = true;
						}
					}

					const bool reliableModel = (phaseConfidence >= 0.70f) || usedFeatureFallback || useTranslationModel;

					xy* img1Dots = nullptr;
					xy* img2Dots = nullptr;
					if (!reliableModel) {
						AppendLog(Localization::T("LogUnreliablePairRejected"));
					}
					else if (usedFeatureFallback) {
						img1Dots = new xy[4];
						img2Dots = new xy[4];
						for (int i = 0; i < 4; i++) {
							img1Dots[i] = featureImg1[i];
						 img2Dots[i] = featureImg2[i];
						}
					}
					else {
						if (useTranslationModel) {
							img1Dots = new xy[4];
						 img2Dots = new xy[4];
						 if (!BuildDotsFromSignedShiftAndRotation(width, height, signedShift.x, signedShift.y, signedRotationDeg, img1Dots, img2Dots)) {
							 img1Dots = DeleteXyArray(img1Dots);
							 img2Dots = DeleteXyArray(img2Dots);
						 }
						}
						else {
						 img1Dots = Rand4Dots(currCornerID, pocDot, width, height);
						 if (img1Dots != NULL)
							 img2Dots = MatchingDots(currCornerID, img1Dots, currVec);
						}
					}

					if (img1Dots == NULL || img2Dots == NULL) {
						AppendLog(Localization::T("LogMatchFailed"));
						if (!startupAutoRun)
							MessageBox::Show(Localization::T("MsgMissingMatches"));
					}
					else {
						AppendLog(Localization::T("LogMatchingHomography"));
						if (!startupAutoRun) {
							ShowMatchVisualization(intensity1, intensity2, width, height, img1Dots, img2Dots);
							AppendLog("Match visualization closed - proceeding with panorama generation");
						}
						else {
							AppendLog("Batch mode: skipping match visualization.");
						}
						LineUpdate(currCornerID, currVec);
						xy* img1PanoDots = PanoDots(prevVec, currCornerID, img1Dots);

						for (int i = 0; i < 4; i++) {
							match1[0][i] = double(img1PanoDots[i].x);
							match1[1][i] = double(img1PanoDots[i].y);
							match1[2][i] = 1.0;
							match2[0][i] = double(img2Dots[i].x);
							match2[1][i] = double(img2Dots[i].y);
							match2[2][i] = 1.0;
						}

						double** homography = homography2d(match1, match2, 4);
						if (useTranslationModel) {
							AppendLog(Localization::T("LogTranslationFallback"));
							AppendLog(String::Format("Translation shift used: dx={0}, dy={1}, corner={2}", signedShift.x, signedShift.y, currCornerID));
							AppendLog(String::Format("Estimated rotation used: {0:F2} deg", signedRotationDeg));
						}
						currPanoSize = ReSizePanorama(homography, prevPanoSize->x, prevPanoSize->y, width, height, position);
						if (currPanoSize != nullptr) {
							labelResultS->Text = currPanoSize->x + " x " + currPanoSize->y;

							if (lineCounter == 0)
								isFirstLine = true;
							else isFirstLine = false;

							AppendLog(Localization::T("LogBlendingPanorama"));
							auto tBlendStart = std::chrono::high_resolution_clock::now();
							BlendMode resolvedBlendMode = ResolveBlendMode(useTranslationModel);
							if (resolvedBlendMode == BlendMode::Speed) {
								AppendLog("Using fast translation blend path.");
								panorama2 = FastTranslationBlend(homography, panorama1, buffer2, prevPanoSize->x, prevPanoSize->y, width, height, *currPanoSize, position);
							}
							else {
								AppendLog("Using Laplacian blend path.");
								LaplacePyramid(homography, panorama1, buffer2, prevPanoSize->x, prevPanoSize->y, width, height, LaplacePyramid1, LaplacePyramid2, width1, height1, *currPanoSize, position);
								panorama2 = PanaromicImage(homography, prevPanoSize->x, prevPanoSize->y, *currPanoSize, position, LaplacePyramid1, LaplacePyramid2, width1, height1, width, height, isFirstLine, currCornerID, currVec);
							}
							if (!startupAutoRun)
								ShowColorImage(panorama2, currPanoSize->x, currPanoSize->y);
							auto tBlendEnd = std::chrono::high_resolution_clock::now();
							pairBlendMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(tBlendEnd - tBlendStart).count();
							AppendLog(String::Format(Localization::T("LogPanoramaUpdated"), currPanoSize->x, currPanoSize->y));
							UpdatePrevVec(currCornerID, prevVec, currVec);

							prevPanoSize->x = currPanoSize->x;
							prevPanoSize->y = currPanoSize->y;

							panorama1 = DeleteByteArray(panorama1);
							panorama1 = panorama2;
							panorama2 = nullptr;
						}

						img1PanoDots = DeleteXyArray(img1PanoDots);
						img2Dots = DeleteXyArray(img2Dots);

						for (int i = 0; i < 3; i++)
							homography[i] = DeleteDoubleArray(homography[i]);
						homography = DeleteDoublePtrArray(homography);
					}

					intensity1 = DeleteByteArray(intensity1);
					intensity1 = intensity2;
					intensity2 = nullptr;

					currPanoSize = DeleteXyObject(currPanoSize);
					buffer1 = DeleteByteArray(buffer1);
					img1Dots = DeleteXyArray(img1Dots);
				}
				imgCounter++;
				UpdateStatusPanel(nullptr, statusPhaseConfidence, statusInlierRatio, statusInlierRatio >= 0.0);
				String^ fft1Text = (pairFft1Ms >= 0.0) ? pairFft1Ms.ToString("F0") + " ms" : "n/a";
				String^ fft2Text = (pairFft2Ms >= 0.0) ? pairFft2Ms.ToString("F0") + " ms" : "n/a";
				String^ phaseText = (pairPhaseMs >= 0.0) ? pairPhaseMs.ToString("F0") + " ms" : "n/a";
				String^ fallbackText = (pairFallbackMs >= 0.0) ? pairFallbackMs.ToString("F0") + " ms" : "n/a";
				String^ fallbackLocalText = (pairFallbackLocalMs >= 0.0) ? pairFallbackLocalMs.ToString("F0") + " ms" : "n/a";
				String^ fallbackGlobalText = (pairFallbackGlobalMs >= 0.0) ? pairFallbackGlobalMs.ToString("F0") + " ms" : "n/a";
				String^ fallbackEdgeText = (pairFallbackEdgeMs >= 0.0) ? pairFallbackEdgeMs.ToString("F0") + " ms" : "n/a";
				String^ blendText = (pairBlendMs >= 0.0) ? pairBlendMs.ToString("F0") + " ms" : "n/a";
				AppendLog(String::Format("Pair timing summary: FFT1={0}, FFT2={1}, Phase={2}, Fallback={3}, Blend={4}", fft1Text, fft2Text, phaseText, fallbackText, blendText));
				AppendLog(String::Format("Fallback breakdown: local={0}, global={1}, edge/refine={2}", fallbackLocalText, fallbackGlobalText, fallbackEdgeText));
				AppendLog(String::Format(Localization::T("LogFinishedPair"), imgCounter, totalPairs));
				imgIndexLbl->Text = (imgCounter + 1).ToString();
				imgIndexLbl->Refresh();
			}

			if (panorama1 != nullptr && prevPanoSize->x > 0 && prevPanoSize->y > 0)
				ShowColorImage(panorama1, prevPanoSize->x, prevPanoSize->y);
			stitchTimerActive = false;
			UpdateStatusPanel(nullptr, statusPhaseConfidence, statusInlierRatio, statusInlierRatio >= 0.0);
			if (stitchAborted) {
				if (abortMessage != nullptr)
					if (!startupAutoRun)
						MessageBox::Show(abortMessage);
				AppendLog("Stitching aborted due to invalid input.");
			}
			else {
				AppendLog(Localization::T("LogCompleted"));
			}

			if (startupAutoRun && startupExitWhenDone) {
				AppendLog("Batch mode run complete. Exiting application.");
				Close();
			}

			panorama1 = DeleteByteArray(panorama1);
			buffer2 = DeleteByteArray(buffer2);
			intensity1 = DeleteByteArray(intensity1);
			intensity2 = DeleteByteArray(intensity2);
			outReal[1] = DeleteDoubleArray(outReal[1]);
			outImag[1] = DeleteDoubleArray(outImag[1]);
			for (size_t i = 0; i < 3; i++) {
				match1[i] = DeleteDoubleArray(match1[i]);
				match2[i] = DeleteDoubleArray(match2[i]);
			}
			LaplacePyramid1 = DeleteByte2PtrArray(LaplacePyramid1);
			LaplacePyramid2 = DeleteByte2PtrArray(LaplacePyramid2);
		}
		}
		catch (Exception^ ex) {
			stitchTimerActive = false;
			AppendLog(String::Format("Open/stitch exception: {0}", ex->Message));
			if (!startupAutoRun) {
				MessageBox::Show(
					String::Format(Localization::T("MsgAppError"), ex->Message, ex->StackTrace),
					Localization::T("MsgAppErrorTitle"));
			}
		}
		catch (...) {
			stitchTimerActive = false;
			AppendLog("Open/stitch exception: unknown native exception.");
			if (!startupAutoRun)
				MessageBox::Show(Localization::T("MsgUnknownError"), Localization::T("MsgAppErrorTitle"));
		}
	}

	private: System::Void saveButton_Click(System::Object^ sender, System::EventArgs^ e) {
		if (latestColorOutput == nullptr || latestColorWidth <= 0 || latestColorHeight <= 0) {
			AppendLog(Localization::T("LogSaveNoImage"));
			return;
		}

		String^ outName = "SavedImg_" + (imgCounter + 1).ToString() + ".bmp";
		String^ outPath = Path::Combine(Application::StartupPath, outName);
		CString nativePath(outPath);
		long rowStride = ((long(latestColorWidth) * 3L + 3L) / 4L) * 4L;
		long imageBytes = rowStride * long(latestColorHeight);
		if (SaveBMP(latestColorOutput, latestColorWidth, latestColorHeight, imageBytes, (LPCTSTR)nativePath)) {
			AppendLog(String::Format(Localization::T("LogSavedOutput"), (imgCounter + 1).ToString()));
			MessageBox::Show(String::Format(Localization::T("MsgSavedImage"), (imgCounter + 1).ToString()));
		}
		else {
			AppendLog(Localization::T("LogSaveFailed"));
		}
	}
	};
}
