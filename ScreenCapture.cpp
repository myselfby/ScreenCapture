#include<windows.h>
#include<stdio.h>
#include<math.h>
#include<time.h>
#include<vector>

#define EXIT_CAPTURE_KEY 0x30  //数字0键(CTRL+0)退出截图
#define START_CAPTURE_KEY 0x31 //数字1键(CTRL+1)开始截图
#define SIZEBOX_HALF 4  //裁剪盒8个角的小矩形宽度一半
//截图状态
enum ECaptureStatus
{
	CS_NoCapture,
	CS_Normal,
	CS_DragCrop,
	CS_FinishDragCrop,
	CS_FixCrop,
	CS_FinishFixCrop,
	CS_DrawGraph,
	CS_FinishCapture
};
//裁剪盒位置
enum ESizeBoxLocation
{
	SBL_None,
	SBL_All,
	SBL_LeftTop,
	SBL_LeftMid,
	SBL_LeftBot,
	SBL_MidTop,
	SBL_MidBot,
	SBL_RightTop,
	SBL_RightMid,
	SBL_RightBot,
	SBL_Max
};
//按钮的状态
enum EButtonStatus
{
	BS_Normal,
	BS_HOVER,
	BS_PRESSED
};
//按钮ID
enum EToolbarButtonId
{
	TBID_Finish,
	TBID_Cancel,
	TBID_DrawLine,
	TBID_Max
};
enum EGraphType
{
	GL_None,
	GT_Line,
	GT_Ellipse,
	GT_Max
};
//工具条按钮结构
struct ToolbarButton
{
	int id;
	EButtonStatus status;
	HBITMAP bmpNormal;
	HBITMAP bmpHover;
	HBITMAP bmpPressed;
	RECT rect;
};
//图形
struct Graph
{
	std::vector<POINT> points;
	EGraphType type;
	bool bFinished;
};
LPCWSTR lpTitle = L"ScreenCapture";                  // 标题栏文本
LPCWSTR lpWindowClass = L"ScreenCapture";            // 主窗口类名

bool bAllwaysTopmost = false;//截图窗口是否始终处于最顶层，确保你能控制得住再启用bAllwaysTopmost
bool bCaptureRealtime = false;//是否截取实时桌面，截取实时桌面时窗口是半透明，能看到桌面上的变化，否则只是将桌面作为背景，看不到桌面的变化

bool CaptureScreenBitmap(int x, int y, int width, int height, unsigned char* outData);//捕获指定位置和大小的屏幕像素数据
bool CaptureFullScreenBitmap();//捕获全屏像素数据
HWND CreateCaptureWindow(HINSTANCE hInstance);//创匠屏幕截图窗口
void TileBitmapToWindow(HDC hDC);//将lpCropBitmap贴到窗口背景上
bool SaveBitmap(const unsigned char* data, int width, int height, DWORD size, const char* filename);//保存为BMP文件
void SaveCropRectImage(HDC hdc);//保存裁剪区域图片
void OnDragCropRect(int xPos, int yPos);//鼠标在屏幕上拖动的事件，此时的状态是CS_DragCrop
void DrawCropRectSizeBox(HDC hdc, const RECT& rect);//画裁剪盒
ESizeBoxLocation GetSizeBoxLocation(int x, int y);//找到点(x,y)在裁剪盒区域的位置
void UpdateMouseCursor(HWND hWnd, ESizeBoxLocation location);//根据鼠标在裁剪盒的位置更新鼠标形状
void OnMouseMove(HWND hWnd, int x, int y);
void OnLeftMouseButtonDown(HWND hWnd, int x, int y);
void OnLeftMouseButtonUp(HWND hWnd, int x, int y);
void OnToolbarCommand(HWND hWnd,int id);//Toolbar按钮点击事件
void DrawCropSizeText(HDC hdc);//长x宽文本
void DrawToolBar(HDC hdc);//画工具条
void DrawGraph(HDC hdc, const std::vector<Graph>& graphs);
int GetToolbarButtonAtPoint(int x, int y);//判断点(x,y)在哪个按钮区域
void OnDraw(HWND hWnd, HDC hdc);
unsigned char* lpScreenBitmap = nullptr;//创建窗口前捕获的屏幕数据
int screenWidth = 0;//屏幕宽
int screenHeight = 0;//屏幕高

unsigned char* lpCropBitmap = nullptr;//包含蒙版区域和裁剪区域的位图象素数据
RECT cropRect;//裁剪盒区域

ECaptureStatus currentStatus = ECaptureStatus::CS_NoCapture;//当前状态
ESizeBoxLocation curSizeBoxLocation = SBL_None;//鼠标所在的裁剪盒的位置
HCURSOR defaultCursor = NULL;//默认光标
POINT prevMousePos;//上一次鼠标位置

//Toolbar按钮的位图文件，顺序是normal、hover、pressed
const WCHAR* toolbarBmpFiles[] = {
	L"Images/finish.bmp",L"Images/finish.bmp",L"Images/finish.bmp",
	L"Images/cancel.bmp",L"Images/cancel.bmp",L"Images/cancel.bmp",
	L"Images/line.bmp",L"Images/line.bmp",L"Images/line.bmp",
	nullptr
};
ToolbarButton toolbarButtons[TBID_Max] = { 0 };//Toolbar按钮数组
RECT toolbarRect = {0};//Toolbar区域
HDC hTBCompatibleDC = NULL;//Toolbar按钮的内存DC
std::vector<Graph> graphLines;//绘制的直线
EGraphType currentDrawType = EGraphType::GL_None;//当前正在绘制图形的类型

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

	/*if (!CreateCaptureWindow(hInstance))
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
					hwnd = CreateCaptureWindow(hInstance);
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

template<typename T>
void Swap(T& elem1,T& elem2)
{
	T tmp = elem1;
	elem1 = elem2;
	elem2 = tmp;
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

HWND CreateCaptureWindow(HINSTANCE hInstance)
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

void SaveCropRectImage(HDC hdc)
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

		HDC hScreenDC = ::GetDC(NULL);
		HDC hMemDC = ::CreateCompatibleDC(hScreenDC);
		HBITMAP hBmp = ::CreateCompatibleBitmap(hScreenDC, width, height);
		::SelectObject(hMemDC, hBmp);
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
		::SetDIBitsToDevice(hMemDC, 0, 0, width, height, 0, 0, 0, height,cropData,&bi, DIB_RGB_COLORS);
		std::vector<Graph> graphs = graphLines;
		for (auto& graph : graphs)
		{
			for (auto& point : graph.points)
			{
				point.x -= left;
				point.y -= top;
			}
		}
		DrawGraph(hMemDC,graphs);
		::GetDIBits(hMemDC,hBmp,0,height,cropData,&bi, DIB_RGB_COLORS);
		::DeleteDC(hMemDC);
		::DeleteObject(hBmp);
		::ReleaseDC(NULL,hScreenDC);

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
		int index = TBID_Finish;
		for (int i = 0; toolbarBmpFiles[i] != nullptr && index < TBID_Max; i+=3)
		{
			ToolbarButton& button = toolbarButtons[index];
			button.id = index;
			index++;
			button.status = EButtonStatus::BS_Normal;
			HBITMAP hBmp = (HBITMAP)LoadImage(NULL, toolbarBmpFiles[i], IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
			button.bmpNormal = hBmp;
			hBmp = (HBITMAP)LoadImage(NULL, toolbarBmpFiles[i+1], IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
			button.bmpHover = hBmp;
			hBmp = (HBITMAP)LoadImage(NULL, toolbarBmpFiles[i + 2], IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
			button.bmpPressed = hBmp;
		}
		toolbarButtons[index].id = TBID_Max;
		break;
	}
	case WM_LBUTTONDOWN:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		OnLeftMouseButtonDown(hWnd,x,y);
		break;
	}
	case WM_LBUTTONUP:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		OnLeftMouseButtonUp(hWnd,x,y);
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
		OnDraw(hWnd,hdc);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_KEYDOWN:
	{
		if (wParam == 0X0d)
		{
			OnToolbarCommand(hWnd,TBID_Finish);
		}
		break;
	}
	case WM_RBUTTONUP:
	{
		OnToolbarCommand(hWnd,TBID_Cancel);
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
		for (int i = 0;; i++)
		{
			ToolbarButton& button = toolbarButtons[i];
			if (button.id >= TBID_Max)
			{
				break;
			}
			button.id = TBID_Max;
			DeleteObject(button.bmpNormal);
			DeleteObject(button.bmpHover);
			DeleteObject(button.bmpPressed);
		}
		if (hTBCompatibleDC)
		{
			::DeleteDC(hTBCompatibleDC);
			hTBCompatibleDC = nullptr;
		}
		graphLines.clear();
		//PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void OnDraw(HWND hWnd, HDC hdc)
{
	TileBitmapToWindow(hdc);
	if (currentStatus == ECaptureStatus::CS_FinishDragCrop
		|| currentStatus == ECaptureStatus::CS_FixCrop
		|| currentStatus == ECaptureStatus::CS_FinishFixCrop
		|| currentStatus == ECaptureStatus::CS_DrawGraph)
	{
		DrawCropRectSizeBox(hdc, cropRect);
	}
	if (currentStatus > ECaptureStatus::CS_Normal)
	{
		DrawCropSizeText(hdc);
	}
	if (currentStatus > ECaptureStatus::CS_DragCrop)
	{
		DrawToolBar(hdc);
	}
	DrawGraph(hdc,graphLines);
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
		case SBL_All:
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
		case SBL_LeftTop:
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
		case SBL_LeftMid:
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
		case SBL_LeftBot:
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
		case SBL_MidTop:
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
		case SBL_MidBot:
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
		case SBL_RightTop:
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
		case SBL_RightMid:
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
		case SBL_RightBot:
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
		int prevId = GetToolbarButtonAtPoint(prevMousePos.x,prevMousePos.y);
		int id = GetToolbarButtonAtPoint(x, y);
		if (prevId != id)
		{
			for (int i = 0;; i++)
			{
				ToolbarButton& button = toolbarButtons[i];
				if (button.id >= TBID_Max)
				{
					break;
				}
				if (button.id == id)
				{
					button.status = EButtonStatus::BS_HOVER;
				}
				else
				{
					button.status = EButtonStatus::BS_Normal;
				}
			}
			::InvalidateRect(hWnd, nullptr, FALSE);
		}
	}
	else if (currentStatus == ECaptureStatus::CS_DrawGraph)
	{
		if (currentDrawType == EGraphType::GT_Line)
		{
			if (graphLines.size() > 0)
			{
				Graph& last = graphLines.back();
				if (!last.bFinished)
				{
					if (last.points.size() < 2)
					{
						last.points.push_back(POINT());
					}
					last.points[1].x = x;
					last.points[1].y = y;
					InvalidateRect(hWnd, NULL, FALSE);
				}
			}
		}
	}
	prevMousePos.x = x;
	prevMousePos.y = y;
}
void OnLeftMouseButtonDown(HWND hWnd, int x, int y)
{
	if (cropRect.right == cropRect.left)
	{
		cropRect.right = cropRect.left = x;
		cropRect.bottom = cropRect.top = y;
		currentStatus = ECaptureStatus::CS_DragCrop;
	}
	else if (currentStatus == ECaptureStatus::CS_FinishDragCrop || currentStatus == ECaptureStatus::CS_FinishFixCrop)
	{
		curSizeBoxLocation = GetSizeBoxLocation(x, y);
		UpdateMouseCursor(hWnd, curSizeBoxLocation);
		if (curSizeBoxLocation > SBL_None && curSizeBoxLocation < SBL_Max)
		{
			currentStatus = ECaptureStatus::CS_FixCrop;
		}
		else
		{
			if (x >= toolbarRect.left && x < toolbarRect.right&&y >= toolbarRect.top&&y < toolbarRect.bottom)
			{
				int id = GetToolbarButtonAtPoint(x, y);
				if (id < TBID_Max)
				{
					for (int i = 0;; i++)
					{
						ToolbarButton& button = toolbarButtons[i];
						if (button.id >= TBID_Max)
						{
							break;
						}
						if (button.id == id)
						{
							button.status = EButtonStatus::BS_PRESSED;
						}
						else
						{
							button.status = EButtonStatus::BS_Normal;
						}
					}
					::InvalidateRect(hWnd, nullptr, FALSE);
				}
			}
		}
	}
	else if (currentStatus == CS_DrawGraph)
	{
		if (currentDrawType == EGraphType::GT_Line)
		{
			if (graphLines.size() == 0 || graphLines.back().bFinished)
			{
				Graph g;
				g.points.push_back({ x,y });
				g.bFinished = false;
				g.type = currentDrawType;
				graphLines.push_back(g);
			}
			else
			{
				Graph& last = graphLines.back();
				if (last.points.size() < 2)
				{
					last.points.push_back(POINT());
				}
				last.points[1].x = x;
				last.points[1].y = y;
				last.bFinished = true;
				currentStatus = CS_FinishFixCrop;
				InvalidateRect(hWnd, NULL, FALSE);
			}
		}
	}
	prevMousePos.x = x;
	prevMousePos.y = y;
}

void OnLeftMouseButtonUp(HWND hWnd,int x,int y)
{
	if (currentStatus == ECaptureStatus::CS_DragCrop)
	{
		currentStatus = ECaptureStatus::CS_FinishDragCrop;
		if (cropRect.left > cropRect.right)
		{
			Swap(cropRect.left, cropRect.right);
		}
		if (cropRect.top > cropRect.bottom)
		{
			Swap(cropRect.top, cropRect.bottom);
		}
		::InvalidateRect(hWnd, nullptr, FALSE);
	}
	else if (currentStatus == ECaptureStatus::CS_FixCrop)
	{
		currentStatus = ECaptureStatus::CS_FinishFixCrop;
	}
	else if (currentStatus == ECaptureStatus::CS_FinishDragCrop || currentStatus == ECaptureStatus::CS_FinishFixCrop)
	{
		int id = GetToolbarButtonAtPoint(x,y);
		if (id < TBID_Max)
		{
			for (int i = 0;; i++)
			{
				ToolbarButton& button = toolbarButtons[i];
				if (button.id >= TBID_Max)
				{
					break;
				}
				if (button.id == id)
				{
					button.status = EButtonStatus::BS_HOVER;
					OnToolbarCommand(hWnd, button.id);
				}
				else
				{
					button.status = EButtonStatus::BS_Normal;
				}
			}
			::InvalidateRect(hWnd, nullptr, FALSE);
		}
	}
	prevMousePos.x = x;
	prevMousePos.y = y;
}

void OnToolbarCommand(HWND hWnd,int id)
{
	EToolbarButtonId btnID = (EToolbarButtonId)id;
	switch (btnID)
	{
	case TBID_Finish:
	{
		currentStatus = ECaptureStatus::CS_FinishCapture;
		if (bCaptureRealtime)
		{
			ShowWindow(hWnd, SW_HIDE);
		}
		HDC hdc = ::GetDC(hWnd);
		SaveCropRectImage(hdc);
		::ReleaseDC(hWnd,hdc);
		::DestroyWindow(hWnd);
		break;
	}
	case TBID_Cancel:
	{
		currentStatus = ECaptureStatus::CS_FinishCapture;
		::DestroyWindow(hWnd);
		break;
	}
	case TBID_DrawLine:
	{
		currentStatus = ECaptureStatus::CS_DrawGraph;
		currentDrawType = EGraphType::GT_Line;
		break;
	}
	case TBID_Max:
		break;
	default:
		break;
	}
}

int GetToolbarButtonAtPoint(int x, int y)
{
	int id = TBID_Max;
	if (x >= toolbarRect.left && x < toolbarRect.right&&y >= toolbarRect.top&&y < toolbarRect.bottom)
	{
		bool bRedraw = false;
		for (int i = 0;; i++)
		{
			ToolbarButton& button = toolbarButtons[i];
			if (button.id >= TBID_Max)
			{
				break;
			}
			if (x >= button.rect.left && x < button.rect.right&&y >= button.rect.top&&y < button.rect.bottom)
			{
				id = button.id;
				break;
			}
		}
	}
	return id;
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
	ESizeBoxLocation location = SBL_None;
	if (x > (xleft - SIZEBOX_HALF) && x<(xleft + SIZEBOX_HALF) && y>ytop - SIZEBOX_HALF && y < ytop + SIZEBOX_HALF)
	{
		location = SBL_LeftTop;
	}
	else if (x > (xleft - SIZEBOX_HALF) && x<(xleft + SIZEBOX_HALF) && y>ymid- SIZEBOX_HALF && y < ymid + SIZEBOX_HALF)
	{
		location = SBL_LeftMid;
	}
	else if (x > (xleft - SIZEBOX_HALF) && x<(xleft+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = SBL_LeftBot;
	}
	else if (x > (xmid - SIZEBOX_HALF) && x<(xmid+ SIZEBOX_HALF) && y>ytop- SIZEBOX_HALF && y < ytop+ SIZEBOX_HALF)
	{
		location = SBL_MidTop;
	}
	else if (x > (xmid- SIZEBOX_HALF) && x<(xmid+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = SBL_MidBot;
	}
	else if (x > (xright - SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ytop- SIZEBOX_HALF && y < ytop+ SIZEBOX_HALF)
	{
		location = SBL_RightTop;
	}
	else if (x > (xright- SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ymid- SIZEBOX_HALF && y < ymid+ SIZEBOX_HALF)
	{
		location = SBL_RightMid;
	}
	else if (x > (xright - SIZEBOX_HALF) && x<(xright+ SIZEBOX_HALF) && y>ybottom- SIZEBOX_HALF && y < ybottom+ SIZEBOX_HALF)
	{
		location = SBL_RightBot;
	}
	else if (x > xleft&&x<xright&&y>ytop&&y < ybottom)
	{
		location = SBL_All;
	}
	return location;
}
void UpdateMouseCursor(HWND hWnd, ESizeBoxLocation location)
{
	if (!defaultCursor)
	{
		defaultCursor = ::GetCursor();
	}
#ifdef _WIN64
	if (location == SBL_All)
	{
		::SetCursor(LoadCursor(NULL, IDC_SIZEALL));
	}
	else if (location == SBL_LeftTop || location == SBL_RightBot)
	{
		::SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
	}
	else if (location == SBL_LeftMid || location == SBL_RightMid)
	{
		::SetCursor(LoadCursor(NULL, IDC_SIZEWE));
	}
	else if (location == SBL_LeftBot || location == SBL_RightTop)
	{
		::SetCursor(LoadCursor(NULL, IDC_SIZENESW));
	}
	else if (location == SBL_MidTop || location == SBL_MidBot)
	{
		::SetCursor(LoadCursor(NULL, IDC_SIZENS));
	}
	else
	{
		::SetCursor(defaultCursor);
	}
#else
	if (location == SBL_All)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZEALL));
	}
	else if (location == SBL_LeftTop || location == SBL_RightBot)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENWSE));
	}
	else if (location == SBL_LeftMid || location == SBL_RightMid)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZEWE));
	}
	else if (location == SBL_LeftBot || location == SBL_RightTop)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENESW));
	}
	else if (location == SBL_MidTop || location == SBL_MidBot)
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)LoadCursor(NULL, IDC_SIZENS));
	}
	else
	{
		SetClassLong(hWnd, GCL_HCURSOR, (long)defaultCursor);
	}
#endif
}

void DrawCropSizeText(HDC hdc)
{
	int width = fabs(cropRect.right - cropRect.left)+1;
	int height = fabs(cropRect.bottom - cropRect.top)+1;

	WCHAR text[100] = {0};
	wsprintf(text,L"%dx%d",width,height);

	//HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
	//	CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Times New Roman");
	//HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

	COLORREF oldTextColor = ::SetTextColor(hdc,RGB(200,200,200));
	COLORREF oldBkColor = ::SetBkColor(hdc,RGB(20,20,20));
	//int oldMode = ::SetBkMode(hdc, TRANSPARENT);

	int left = min(cropRect.left,cropRect.right);
	int top = min(cropRect.top, cropRect.bottom);
	RECT rc;
	rc.left = left;
	rc.right = left + 100;
	rc.top = top<25?top+1:top-25;
	rc.bottom = rc.top + 25;
	DrawText(hdc,text,-1,&rc, DT_LEFT);

	::SetTextColor(hdc, oldTextColor);
	::SetBkColor(hdc,oldBkColor);
	//::SetBkMode(hdc,oldMode);

	//SelectObject(hdc, hOldFont);
	//DeleteObject(hFont);
}
void DrawToolBar(HDC hdc)
{
	int x = cropRect.left+10;
	int y = cropRect.bottom+2;
	if (y + 40 > screenHeight)
	{
		y = cropRect.bottom - 45;
	}
	toolbarRect.left = x;
	toolbarRect.top = y;
	if (!hTBCompatibleDC)
	{
		hTBCompatibleDC = CreateCompatibleDC(hdc);
	}
	for (int i = 0;; i++)
	{
		int top = y;
		ToolbarButton& button = toolbarButtons[i];
		if (button.id >= TBID_Max)
		{
			break;
		}
		if (button.status == EButtonStatus::BS_Normal)
		{
			SelectObject(hTBCompatibleDC, button.bmpNormal);
		}
		else if (button.status == EButtonStatus::BS_HOVER )
		{
			SelectObject(hTBCompatibleDC, button.bmpHover);
			top -= 1;
		}
		else
		{
			SelectObject(hTBCompatibleDC, button.bmpPressed);
		}
		BitBlt(hdc, x, top, 32, 32, hTBCompatibleDC, 0, 0, SRCCOPY);
		button.rect = { x,top,x + 32,top + 32 };
		x += 32;
	}
	toolbarRect.right = x;
	toolbarRect.bottom = toolbarRect.top + 32;
}
void DrawGraph(HDC hdc,const std::vector<Graph>& graphs)
{
	HPEN hPen = CreatePen(PS_SOLID,1,RGB(255,0,0));
	HPEN hOldPen = (HPEN)::SelectObject(hdc,hPen);
	for (int i = 0; i < graphs.size(); i++)
	{
		auto& graph = graphs[i];
		if (graph.type == EGraphType::GT_Line)
		{
			if (graph.points.size() >= 2)
			{
				const POINT& pt1 = graph.points[0];
				MoveToEx(hdc, pt1.x, pt1.y, NULL);
				const POINT& pt2 = graph.points[1];
				LineTo(hdc, pt2.x, pt2.y);
			}
		}
	}
	::SelectObject(hdc,hOldPen);
}