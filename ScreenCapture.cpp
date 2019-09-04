#include<windows.h>
#include<stdio.h>
#include<math.h>
#include<time.h>

#define EXIT_CAPTURE_KEY 0x30
#define START_CAPTURE_KEY 0x31

LPCWSTR lpTitle= L"ScreenCapture";                  // 标题栏文本
LPCWSTR lpWindowClass = L"ScreenCapture" ;            // 主窗口类名

bool CaptureScreenBitmap();
HWND CreateCropWindow(HINSTANCE hInstance);
void TileBitmapToWindow(HDC hDC);
bool SaveBitmap(const unsigned char* data, int width, int height, DWORD size, const char* filename);
void SaveCropRectImage();
void OnDragCropRect(int xPos, int yPos);

unsigned char* lpScreenBitmap = nullptr;
int screenWidth = 0;
int screenHeight = 0;

unsigned char* lpCropBitmap = nullptr;
RECT cropRect;

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
	wcex.hbrBackground = NULL;
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
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (msg.message == WM_HOTKEY)
		{
			int msgKey = HIWORD(msg.lParam);
			if (msgKey == START_CAPTURE_KEY && msg.wParam == hkStartCaptureId)
			{
				CreateCropWindow(hInstance);
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

bool CaptureScreenBitmap()
{
	//获取屏幕DC句柄以及创建兼容的内存DC
	HDC hdcScreen = GetDC(NULL);
	HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
	HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);

	//位图信息
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

	DWORD bmpSize = ((screenWidth * bi.bmiHeader.biBitCount + 31) / 32) * 4 * screenHeight;
	lpScreenBitmap = new unsigned char[bmpSize];
	lpCropBitmap = new unsigned char[bmpSize];
	memset(lpScreenBitmap, 0, bmpSize);
	//获取屏幕颜色信息
	SelectObject(hdcMemDC, hbmScreen);
	if (BitBlt(hdcMemDC, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY))
	{
		GetDIBits(hdcMemDC, hbmScreen, 0, screenHeight, lpScreenBitmap, &bi, DIB_RGB_COLORS);
		for (DWORD i = 0; i < bmpSize; i++)
		{
			lpCropBitmap[i] = lpScreenBitmap[i] / 2;
		}
	}
	//释放资源
	DeleteObject(hbmScreen);
	DeleteObject(hdcMemDC);
	ReleaseDC(NULL, hdcScreen);
	return true;
}

HWND CreateCropWindow(HINSTANCE hInstance)
{
	bool bAllwaysTopmost = false;	//确保你能控制得住再启用bAllwaysTopmost
	CaptureScreenBitmap();

	DWORD dwExStyle = WS_EX_TOOLWINDOW;//不显示在任务栏中
	if (bAllwaysTopmost)
	{
		dwExStyle |= WS_EX_TOPMOST;//始终处于最顶层
	}
	HWND hWnd = CreateWindowExW(dwExStyle, lpWindowClass, lpTitle, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
		0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);
	//HWND hWnd = CreateWindowW(lpWindowClass, lpTitle, WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_POPUP,
	//	0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);
	//if(hWnd)::SetWindowPos(hWnd, HWND_TOPMOST,0,0,screenWidth,screenHeight, SWP_NOMOVE | SWP_NOSIZE);
	if (hWnd)
	{
		if (!bAllwaysTopmost)
		{
			BringWindowToTop(hWnd);//第一次置于顶层
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

bool SaveBitmap(const unsigned char* data, int width, int height,DWORD size, const char* filename)
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
		for (int y = 0; y < height; y++)
		{
			int destIndex = y * saveLineBytes;
			int srcIndex = (screenHeight - bottom - 1 + y) * screenLineBytes + left * 3;
			memcpy(&cropData[destIndex], &lpScreenBitmap[srcIndex], saveLineBytes);
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

void OnDragCropRect(int xPos,int yPos)
{
	int minRow = min(cropRect.top, cropRect.bottom);
	int maxRow = max(cropRect.top, cropRect.bottom);
	int minCol = min(cropRect.left, cropRect.right);
	int maxCol = max(cropRect.left, cropRect.right);
	for (int y = minRow; y <= maxRow; y++)
	{
		for (int x = minCol; x <= maxCol; x++)
		{
			int index = (screenHeight - y - 1) * screenWidth * 3 + x * 3;
			lpCropBitmap[index] = lpScreenBitmap[index] / 2;
			lpCropBitmap[index + 1] = lpScreenBitmap[index + 1] / 2;
			lpCropBitmap[index + 2] = lpScreenBitmap[index + 2] / 2;
		}
	}
	cropRect.right = xPos;
	cropRect.bottom = yPos;
	minRow = min(cropRect.top, cropRect.bottom);
	maxRow = max(cropRect.top, cropRect.bottom);
	minCol = min(cropRect.left, cropRect.right);
	maxCol = max(cropRect.left, cropRect.right);
	for (int y = minRow; y <= maxRow; y++)
	{
		for (int x = minCol; x <= maxCol; x++)
		{
			int index = (screenHeight - y - 1) * screenWidth * 3 + x * 3;
			lpCropBitmap[index] = lpScreenBitmap[index];
			lpCropBitmap[index + 1] = lpScreenBitmap[index + 1];
			lpCropBitmap[index + 2] = lpScreenBitmap[index + 2];
		}
	}
}
LRESULT CALLBACK CropScreenProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool bLeftMouseButtomDown = false;
	switch (message)
	{
	case WM_CREATE:
	{
		cropRect.left = cropRect.right = cropRect.top = cropRect.bottom = 0;
		break;
	}
	case WM_LBUTTONDOWN:
	{
		if (cropRect.right - cropRect.left == 0)
		{
			cropRect.right = cropRect.left = LOWORD(lParam);
			cropRect.bottom = cropRect.top = HIWORD(lParam);
			bLeftMouseButtomDown = true;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		bLeftMouseButtomDown = false;
		break;
	}
	case WM_MOUSEMOVE:
	{
		if (bLeftMouseButtomDown)
		{
			OnDragCropRect(LOWORD(lParam),HIWORD(lParam));
			::InvalidateRect(hWnd, nullptr, FALSE);
		}
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		TileBitmapToWindow(hdc);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_KEYDOWN:
	{
		if (wParam == 0X0d)
		{
			SaveCropRectImage();
			::DestroyWindow(hWnd);
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		::DestroyWindow(hWnd);
		break;
	}
	case WM_DESTROY:
	{
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