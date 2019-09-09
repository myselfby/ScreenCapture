#include<windows.h>
#include<stdio.h>
#include<math.h>
#include<time.h>

#define EXIT_CAPTURE_KEY 0x30
#define START_CAPTURE_KEY 0x31
#define SIZEBOX_HALF 4
enum ECaptureStatus
{
	CS_NoCapture,
	CS_Normal,
	CS_DragCrop,
	CS_FinishDragCrop,
	CS_FixCrop,
	CS_FinishFixCrop,
	CS_FinishCapture
};

enum ESizeBoxLocation
{
	CL_None,
	CL_All,
	CL_LeftTop,
	CL_LeftMid,
	CL_LeftBot,
	CL_MidTop,
	CL_MidBot,
	CL_RightTop,
	CL_RightMid,
	CL_RightBot,
	CL_Max
};

LPCWSTR lpTitle = L"ScreenCapture";                  // 标题栏文本
LPCWSTR lpWindowClass = L"ScreenCapture";            // 主窗口类名

bool bAllwaysTopmost = false;//截图窗口是否始终处于最顶层，确保你能控制得住再启用bAllwaysTopmost
bool bCaptureRealtime = true;//是否截取实时桌面，截取实时桌面时窗口是半透明，能看到桌面上的变化，否则只是将桌面作为背景，看不到桌面的变化

bool CaptureScreenBitmap(int x, int y, int width, int height, unsigned char* outData);
bool CaptureFullScreenBitmap();
HWND CreateCropWindow(HINSTANCE hInstance);
void TileBitmapToWindow(HDC hDC);
bool SaveBitmap(const unsigned char* data, int width, int height, DWORD size, const char* filename);
void SaveCropRectImage();
void OnDragCropRect(int xPos, int yPos);
void DrawCropRectSizeBox(HDC hdc, const RECT& rect);
ESizeBoxLocation GetSizeBoxLocation(int x, int y);
void UpdateMouseCursor(HWND hWnd, ESizeBoxLocation location);
void OnMouseMove(HWND hWnd, int x, int y);

unsigned char* lpScreenBitmap = nullptr;
int screenWidth = 0;
int screenHeight = 0;

unsigned char* lpCropBitmap = nullptr;
RECT cropRect;

ECaptureStatus currentStatus = ECaptureStatus::CS_NoCapture;
ESizeBoxLocation curSizeBoxLocation = CL_None;
HCURSOR defaultCursor = NULL;
POINT prevMousePos;//上一次鼠标位置

LRESULT CALLBACK CropScreenProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = CropScreenProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));// (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = lpWindowClass;
	wcex.hIconSm = NULL;
	RegisterClassExW(&wcex);
	screenWidth = GetSystemMetrics(SM_CXSCREEN);
	screenHeight = GetSystemMetrics(SM_CYSCREEN);

	/*if (!CreateCropWindow(hInstance))
	{
		return FALSE;
	}*/

	ATOM hkExitCaptureId = GlobalAddAtomA("ExitCapture");
	if (!RegisterHotKey(NULL, hkExitCaptureId, MOD_CONTROL | MOD_NOREPEAT, EXIT_CAPTURE_KEY))
	{
		return FALSE;
	}
	ATOM hkStartCaptureId = GlobalAddAtomA("StartCapture");
	if (!RegisterHotKey(NULL, hkStartCaptureId, MOD_CONTROL | MOD_NOREPEAT, START_CAPTURE_KEY))
	{
		return FALSE;
	}
	MSG msg;
	HWND hwnd = NULL;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (msg.message == WM_HOTKEY)
		{
			int msgKey = HIWORD(msg.lParam);
			if (msgKey == START_CAPTURE_KEY && msg.wParam == hkStartCaptureId)
			{
				if (currentStatus != ECaptureStatus::CS_NoCapture && hwnd)
				{
					BringWindowToTop(hwnd);
				}
				else
				{
					hwnd = CreateCropWindow(hInstance);
					currentStatus = ECaptureStatus::CS_Normal;
				}
			}
			else if (msgKey == EXIT_CAPTURE_KEY && msg.wParam == hkExitCaptureId)
			{
				break;
			}
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	GlobalDeleteAtom(hkExitCaptureId);
	GlobalDeleteAtom(hkStartCaptureId);
	return (int)msg.wParam;
}


bool CaptureScreenBitmap(int x, int y, int width, int height, unsigned char* outData)
{
	//获取屏幕DC句柄以及创建兼容的内存DC
	HDC hdcScreen = GetDC(NULL);
	HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
	HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, width, height);

	//位图信息
	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = height;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = 0;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	//获取屏幕颜色信息
	SelectObject(hdcMemDC, hbmScreen);
	if (BitBlt(hdcMemDC, 0, 0, width, height, hdcScreen, x, y, SRCCOPY))
	{
		GetDIBits(hdcMemDC, hbmScreen, 0, height, outData, &bi, DIB_RGB_COLORS);
	}
	//释放资源
	DeleteObject(hbmScreen);
	DeleteObject(hdcMemDC);
	ReleaseDC(NULL, hdcScreen);
	return true;
}

bool CaptureFullScreenBitmap()
{
	int bitCount = 24;
	DWORD bmpSize = ((screenWidth * bitCount + 31) / 32) * 4 * screenHeight;
	lpScreenBitmap = new unsigned char[bmpSize];
	lpCropBitmap = new unsigned char[bmpSize];
	memset(lpScreenBitmap, 0, bmpSize);
	memset(lpCropBitmap, 0, bmpSize);
	if (!bCaptureRealtime)
	{
		CaptureScreenBitmap(0, 0, screenWidth, screenHeight, lpScreenBitmap);
		for (DWORD i = 0; i < bmpSize; i++)
		{
			lpCropBitmap[i] = lpScreenBitmap[i] / 2;
		}
	}
	return true;
}

HWND CreateCropWindow(HINSTANCE hInstance)
{
	CaptureFullScreenBitmap();

	DWORD dwExStyle = WS_EX_TOOLWINDOW;//不显示在任务栏中
	if (bAllwaysTopmost)
	{
		dwExStyle |= WS_EX_TOPMOST;//始终处于最顶层
	}
	if (bCaptureRealtime)
	{
		dwExStyle |= WS_EX_LAYERED;//要使窗口透明必须启用WS_EX_LAYERED
	}
	HWND hWnd = CreateWindowExW(dwExStyle, lpWindowClass, lpTitle, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
		0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);
	//HWND hWnd = CreateWindowW(lpWindowClass, lpTitle, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
	//	0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);
	//if(hWnd)::SetWindowPos(hWnd, HWND_TOPMOST,0,0,screenWidth,screenHeight, SWP_NOMOVE | SWP_NOSIZE);
	//if(hWnd)::SetWindowLong(hWnd, GWL_EXSTYLE,dwExStyle);
	if (hWnd)
	{
		if (!bAllwaysTopmost)
		{
			BringWindowToTop(hWnd);//第一次置于顶层
		}
		if (bCaptureRealtime)
		{
			//窗口中颜色为RGB(0, 0, 255)处都是透明，其他像素处不透明度为128
			::SetLayeredWindowAttributes(hWnd, RGB(0, 0, 255), 128, LWA_COLORKEY | LWA_ALPHA);
		}
		ShowWindow(hWnd, SW_SHOW);
		UpdateWindow(hWnd);
	}
	return hWnd;
}
void TileBitmapToWindow(HDC hDC)
{
	BITMAPINFO bi;
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = screenWidth;
	bi.bmiHeader.biHeight = screenHeight;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;
	bi.bmiHeader.biCompression = BI_RGB;
	bi.bmiHeader.biSizeImage = 0;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;
	SetDIBitsToDevice(hDC, 0, 0, screenWidth, screenHeight, 0, 0, 0, screenHeight, lpCropBitmap, &bi, DIB_RGB_COLORS);
}

bool SaveBitmap(const unsigned char* data, int width, int height, DWORD size, const char* filename)
{
	FILE* pFile = nullptr;
	fopen_s(&pFile, filename, "wb");
	if (pFile)
	{
		DWORD bmpDataSize = width * height * 3;//数据字节数

		BITMAPINFOHEADER   bmpInfoHeader;//bmp信息头
		bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpInfoHeader.biWidth = width;
		bmpInfoHeader.biHeight = height;
		bmpInfoHeader.biPlanes = 1;
		bmpInfoHeader.biBitCount = 24;
		bmpInfoHeader.biCompression = BI_RGB;
		bmpInfoHeader.biSizeImage = 0;
		bmpInfoHeader.biXPelsPerMeter = 0;
		bmpInfoHeader.biYPelsPerMeter = 0;
		bmpInfoHeader.biClrUsed = 0;
		bmpInfoHeader.biClrImportant = 0;

		BITMAPFILEHEADER   bmpFileHeader;//bmp文件头
		bmpFileHeader.bfType = 0x4D42; //BM bmp文件标识符
		bmpFileHeader.bfOffBits = sizeof(bmpFileHeader) + sizeof(bmpInfoHeader);//bmp颜色数据偏移
		bmpFileHeader.bfSize = size + sizeof(bmpFileHeader) + sizeof(bmpInfoHeader);//BMP文件大小

		//依次写入文件头，信息头和数据
		fwrite(&bmpFileHeader, sizeof(bmpFileHeader), 1, pFile);
		fwrite(&bmpInfoHeader, sizeof(bmpInfoHeader), 1, pFile);
		fwrite(data, 1, size, pFile);
		fclose(pFile);
	}
	return true;
}

void SaveCropRectImage()
{
	int left = min(cropRect.left, cropRect.right);
	int right = max(cropRect.left, cropRect.right);
	int top = min(cropRect.top, cropRect.bottom);
	int bottom = max(cropRect.top, cropRect.bottom);
	int width = right - left + 1;
	int height = bottom - top + 1;
	if (width > 0 && height > 0)
	{
		int saveLineBytes = ((width * 24 + 31) / 32) * 4;//每行字节数必须是4字节的整数倍
		int screenLineBytes = screenWidth * 3;
		int size = saveLineBytes * height;
		unsigned char* cropData = new unsigned char[size];
		if (bCaptureRealtime)
		{
			CaptureScreenBitmap(left, top, width, height, cropData);
		}
		else
		{
			for (int y = 0; y < height; y++)
			{
				int destIndex = y * saveLineBytes;
				int srcIndex = (screenHeight - bottom - 1 + y) * screenLineBytes + left * 3;
				memcpy(&cropData[destIndex], &lpScreenBitmap[srcIndex], saveLineBytes);
			}
		}
		time_t seconds = time(0);
		int s = seconds % 60;
		int m = (seconds % 3600) / 60;
		int h = (seconds % (3600 * 24)) / 3600 + 8;
		char timeBuf[128] = { 0 };
		sprintf_s(timeBuf, 128, "capture-%02d-%02d-%02d.bmp", h, m, s);

		SaveBitmap(cropData, width, height, size, timeBuf);
		delete[] cropData;
	}
}

void UpdateCropAreaData(const RECT& rect, bool bResetToMask)
{
	int minRow = min(rect.top, rect.bottom);
	int maxRow = max(rect.top, rect.bottom);
	int minCol = min(rect.left,rect.right);
	int maxCol = max(rect.left,rect.right);
	if (minRow <= 0)
	{
		minRow = 0;
	}
	if (maxRow >= screenHeight)
	{
		maxRow = screenHeight - 1;
	}
	if (minCol < 0)
	{
		minCol = 0;
	}
	if (maxCol > screenWidth)
	{
		maxCol = screenWidth - 1;
	}
	for (int y = minRow; y <= maxRow; y++)
	{
		for (int x = minCol; x <= maxCol; x++)
		{
			int index = (screenHeight - y - 1) * screenWidth * 3 + x * 3;
			if (bCaptureRealtime)
			{
				//截取实时图象时将截取区域外的像素颜色设为RGB(0,0,0)
				lpCropBitmap[index] = bResetToMask?0: 255;
				lpCropBitmap[index + 1] = 0;
				lpCropBitmap[index + 2] = 0;
			}
			else
			{
				if (bResetToMask)
				{
					lpCropBitmap[index] = lpScreenBitmap[index] / 2;
					lpCropBitmap[index + 1] = lpScreenBitmap[index + 1] / 2;
					lpCropBitmap[index + 2] = lpScreenBitmap[index + 2] / 2;
				}
				else
				{
					lpCropBitmap[index] = lpScreenBitmap[index];
					lpCropBitmap[index + 1] = lpScreenBitmap[index + 1];
					lpCropBitmap[index + 2] = lpScreenBitmap[index + 2];
				}
			}
		}
	}
}
void OnDragCropRect(int xPos, int yPos)
{
	UpdateCropAreaData(cropRect,true);
	cropRect.right = xPos < 0 ? 0 : (xPos >= screenWidth ? screenWidth - 1 : xPos);
	cropRect.bottom = yPos < 0 ? 0 : (yPos >= screenHeight ? screenHeight - 1 : yPos);
	UpdateCropAreaData(cropRect,false);
}

LRESULT CALLBACK CropScreenProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		cropRect.left = cropRect.right = cropRect.top = cropRect.bottom = 0;
		break;
	}
	case WM_LBUTTONDOWN:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		if (cropRect.right == cropRect.left)
		{
			cropRect.right = cropRect.left = x;
			cropRect.bottom = cropRect.top = y;
			currentStatus = ECaptureStatus::CS_DragCrop;
		}
		else if(currentStatus == ECaptureStatus::CS_FinishDragCrop || currentStatus == ECaptureStatus::CS_FinishFixCrop)
		{
			curSizeBoxLocation = GetSizeBoxLocation(x, y);
			UpdateMouseCursor(hWnd, curSizeBoxLocation);
			if (curSizeBoxLocation > CL_None && curSizeBoxLocation < CL_Max)
			{
				currentStatus = ECaptureStatus::CS_FixCrop;
			}
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		if (currentStatus == ECaptureStatus::CS_DragCrop)
		{
			currentStatus = ECaptureStatus::CS_FinishDragCrop;
			cropRect.left = min(cropRect.left, cropRect.right);
			cropRect.right = max(cropRect.left, cropRect.right);
			cropRect.top = min(cropRect.top, cropRect.bottom);
			cropRect.bottom = max(cropRect.top, cropRect.bottom);
			::InvalidateRect(hWnd, nullptr, FALSE);
		}
		else if (currentStatus == ECaptureStatus::CS_FixCrop)
		{
			currentStatus = ECaptureStatus::CS_FinishFixCrop;
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		OnMouseMove(hWnd,x,y);
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		TileBitmapToWindow(hdc);
		if (currentStatus == ECaptureStatus::CS_FinishDragCrop
			|| currentStatus == ECaptureStatus::CS_FixCrop
			|| currentStatus == ECaptureStatus::CS_FinishFixCrop)
		{
			DrawCropRectSizeBox(hdc,cropRect);
		}
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_KEYDOWN:
	{
		if (wParam == 0X0d)
		{
			currentStatus = ECaptureStatus::CS_FinishCapture;
			if (bCaptureRealtime)
			{
				ShowWindow(hWnd,SW_HIDE);
			}
			SaveCropRectImage();
			::DestroyWindow(hWnd);
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		currentStatus = ECaptureStatus::CS_FinishCapture;
		::DestroyWindow(hWnd);
		break;
	}
	case WM_DESTROY:
	{
		currentStatus = ECaptureStatus::CS_NoCapture;
		if (lpScreenBitmap)
		{
			delete[] lpScreenBitmap;
			lpScreenBitmap = nullptr;
		}
		if (lpCropBitmap)
		{
			delete[] lpCropBitmap;
			lpCropBitmap = nullptr;
		}
		//PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void OnMouseMove(HWND hWnd, int x, int y)
{
	if (currentStatus == ECaptureStatus::CS_DragCrop)
	{
		OnDragCropRect(x, y);
		::InvalidateRect(hWnd, nullptr, FALSE);
	}
	else if (currentStatus == ECaptureStatus::CS_FixCrop)
	{
		RECT oldRect = cropRect;
		oldRect.left -= SIZEBOX_HALF;
		oldRect.right += SIZEBOX_HALF;
		oldRect.top -= SIZEBOX_HALF;
		oldRect.bottom += SIZEBOX_HALF;
		UpdateCropAreaData(oldRect, true);
		int deltax = x - prevMousePos.x;
		int deltay = y - prevMousePos.y;
		int width = cropRect.right - cropRect.left;
		int height = cropRect.bottom - cropRect.top;
		switch (curSizeBoxLocation)
		{
		case CL_All:
		{
			cropRect.left += deltax;
			cropRect.top += deltay;
			cropRect.right += deltax;
			cropRect.bottom += deltay;
			if (cropRect.left < 0)
			{
				cropRect.left = 0;
			}
			else if (cropRect.right >= screenWidth)
			{
				cropRect.left = screenWidth - width - 1;
			}
			if (cropRect.top < 0)
			{
				cropRect.top = 0;
			}
			else if (cropRect.bottom >= screenHeight)
			{
				cropRect.top = screenHeight - height - 1;
			}
			cropRect.right = cropRect.left + width;
			cropRect.bottom = cropRect.top + height;
			break;
		}
		case CL_LeftTop:
		{
			cropRect.left += deltax;
			cropRect.top += deltay;
			if (cropRect.left < 0)
			{
				cropRect.left = 0;
			}
			else if (cropRect.left >= cropRect.right)
			{
				cropRect.left = cropRect.right - 1;
			}
			if (cropRect.top < 0)
			{
				cropRect.top = 0;
			}
			else if (cropRect.top >= cropRect.bottom)
			{
				cropRect.top = cropRect.bottom - 1;
			}
			break;
		}
		case CL_LeftMid:
		{
			cropRect.left += deltax;
			if (cropRect.left < 0)
			{
				cropRect.left = 0;
			}
			else if (cropRect.left >= cropRect.right)
			{
				cropRect.left = cropRect.right - 1;
			}
			break;
		}
		case CL_LeftBot:
		{
			cropRect.left += deltax;
			cropRect.bottom += deltay;
			if (cropRect.left < 0)
			{
				cropRect.left = 0;
			}
			else if (cropRect.left >= cropRect.right)
			{
				cropRect.left = cropRect.right - 1;
			}
			if (cropRect.bottom <= cropRect.top)
			{
				cropRect.bottom = cropRect.top + 1;
			}
			else if (cropRect.bottom >= screenHeight)
			{
				cropRect.bottom = screenHeight - 1;
			}
			break;
		}
		case CL_MidTop:
		{
			cropRect.top += deltay;
			if (cropRect.top < 0)
			{
				cropRect.top = 0;
			}
			else if (cropRect.top >= cropRect.bottom)
			{
				cropRect.top = cropRect.bottom - 1;
			}
			break;
		}
		case CL_MidBot:
		{
			cropRect.bottom += deltay;
			if (cropRect.bottom <= cropRect.top)
			{
				cropRect.bottom = cropRect.top + 1;
			}
			else if (cropRect.bottom >= screenHeight)
			{
				cropRect.bottom = screenHeight - 1;
			}
			break;
		}
		case CL_RightTop:
		{
			cropRect.top += deltay;
			cropRect.right += deltax;
			if (cropRect.top < 0)
			{
				cropRect.top = 0;
			}
			else if (cropRect.top >= cropRect.bottom)
			{
				cropRect.top = cropRect.bottom - 1;
			}
			if (cropRect.right <= cropRect.left)
			{
				cropRect.right = cropRect.left + 1;
			}
			else if (cropRect.right >= screenWidth)
			{
				cropRect.right = screenWidth - 1;
			}
			break;
		}
		case CL_RightMid:
		{
			cropRect.right += deltax;
			if (cropRect.right <= cropRect.left)
			{
				cropRect.right = cropRect.left + 1;
			}
			else if (cropRect.right >= screenWidth)
			{
				cropRect.right = screenWidth - 1;
			}
			break;
		}
		case CL_RightBot:
		{
			cropRect.right += deltax;
			cropRect.bottom += deltay;
			if (cropRect.right <= cropRect.left)
			{
				cropRect.right = cropRect.left + 1;
			}
			else if (cropRect.right >= screenWidth)
			{
				cropRect.right = screenWidth - 1;
			}
			if (cropRect.bottom <= cropRect.top)
			{
				cropRect.bottom = cropRect.top + 1;
			}
			else if (cropRect.bottom >= screenHeight)
			{
				cropRect.bottom = screenHeight - 1;
			}
			break;
		}
		default:
			break;
		}
		UpdateCropAreaData(cropRect, false);
		::InvalidateRect(hWnd, nullptr, FALSE);
	}
	else if(currentStatus == ECaptureStatus::CS_FinishDragCrop || currentStatus == ECaptureStatus::CS_FinishFixCrop)
	{
		UpdateMouseCursor(hWnd, GetSizeBoxLocation(x, y));
	}
	prevMousePos.x = x;
	prevMousePos.y = y;
}
void DrawRect(HDC hdc, const RECT& rect)
{
	MoveToEx(hdc, rect.left, rect.top, nullptr);
	LineTo(hdc, rect.left, rect.bottom);
	LineTo(hdc, rect.right, rect.bottom);
	LineTo(hdc, rect.right, rect.top);
	LineTo(hdc, rect.left, rect.top);
}
void DrawCropRectSizeBox(HDC hdc ,const RECT& rect)
{
	COLORREF color = RGB(70, 124, 212);

	HPEN hPen = CreatePen(PS_SOLID, 1, color);
	HPEN hOldPen = (HPEN)::SelectObject(hdc, hPen);

	int midy = (rect.top + rect.bottom) / 2;
	int midx = (rect.left + rect.right) / 2;
	DrawRect(hdc,rect);
	RECT cornel;
	cornel.left = rect.left - SIZEBOX_HALF;
	cornel.top = rect.top - SIZEBOX_HALF;
	cornel.right = rect.left + SIZEBOX_HALF;
	cornel.bottom = rect.top + SIZEBOX_HALF;
	DrawRect(hdc,cornel);

	cornel.top = midy - SIZEBOX_HALF;
	cornel.bottom = midy + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.top = rect.bottom - SIZEBOX_HALF;
	cornel.bottom = rect.bottom + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.left = midx - SIZEBOX_HALF;
	cornel.right = midx + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.left = rect.right - SIZEBOX_HALF;
	cornel.right = rect.right + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.top = midy - SIZEBOX_HALF;
	cornel.bottom = midy + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.top = rect.top - SIZEBOX_HALF;
	cornel.bottom = rect.top + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	cornel.left = midx - SIZEBOX_HALF;
	cornel.right = midx + SIZEBOX_HALF;
	DrawRect(hdc, cornel);

	SelectObject(hdc, hOldPen);
	DeleteObject(hPen);
}

ESizeBoxLocation GetSizeBoxLocation(int x, int y)
{
	int xleft = cropRect.left;
	int xright = cropRect.right;
	int xmid = (xleft + xright) / 2;
	int ytop = cropRect.top;
	int ybottom = cropRect.bottom;
	int ymid = (ytop + ybottom) / 2;
	ESizeBoxLocation location = CL_None;
	if (x > (xleft - SIZEBOX_HALF) && x<(xleft + SIZEBOX_HALF) && y>ytop - SIZEBOX_HALF && y < ytop + SIZEBOX_HALF)
	{
		location = CL_LeftTop;
	}
	else if (x > (xleft - SIZEBOX_HALF) && x<(xleft + SIZEBOX_HALF) && y>ymid- SIZEBOX_HALF && y < ymid + SIZEBOX_HALF)
	{
		location = CL_LeftMid;
	}
	else if (x > (xleft - SIZEBOX_HALF) && x<(xleft+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = CL_LeftBot;
	}
	else if (x > (xmid - SIZEBOX_HALF) && x<(xmid+ SIZEBOX_HALF) && y>ytop- SIZEBOX_HALF && y < ytop+ SIZEBOX_HALF)
	{
		location = CL_MidTop;
	}
	else if (x > (xmid- SIZEBOX_HALF) && x<(xmid+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = CL_MidBot;
	}
	else if (x > (xright - SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ytop- SIZEBOX_HALF && y < ytop+ SIZEBOX_HALF)
	{
		location = CL_RightTop;
	}
	else if (x > (xright- SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ymid- SIZEBOX_HALF && y < ymid+ SIZEBOX_HALF)
	{
		location = CL_RightMid;
	}
	else if (x > (xright - SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = CL_RightBot;
	}
	else if (x > xleft&&x<xright&&y>ytop&&y < ybottom)
	{
		location = CL_All;
	}
	return location;
}
void UpdateMouseCursor(HWND hWnd, ESizeBoxLocation location)
{
	if (!defaultCursor)
	{
		defaultCursor = ::GetCursor();
	}
	if (location == CL_All)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZEALL));
		//::SetCursor(LoadCursor(NULL, IDC_SIZEALL));
	}
	else if (location == CL_LeftTop || location == CL_RightBot)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENWSE));
		//::SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
	}
	else if (location == CL_LeftMid || location == CL_RightMid)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZEWE));
		//::SetCursor(LoadCursor(NULL, IDC_SIZEWE));
	}
	else if (location == CL_LeftBot || location == CL_RightTop)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENESW));
		//::SetCursor(LoadCursor(NULL, IDC_SIZENESW));
	}
	else if (location == CL_MidTop || location == CL_MidBot)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENS));
		//::SetCursor(LoadCursor(NULL, IDC_SIZENS));
	}
	else
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)defaultCursor);
		//::SetCursor(defaultCursor);
	}
}