// AnotherWorld.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "AnotherWorld.h"

#include "another-world/virtual-machine.hpp"

using namespace another_world;

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hWnd;
HACCEL hAccelTable;
RECT rcSurface;
BITMAPINFO bmpInfo;
LPBYTE pbmpRender;
HBITMAP bmpRender;
RECT rcView;
uint8_t screen[320 * 200 / 2];
uint8_t pixelSize;

VirtualMachine vm;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ANOTHERWORLD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    SetRect(&rcSurface, 0, 0, 320, 200);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ANOTHERWORLD));

    MSG msg;
    
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = rcSurface.right;
    bmpInfo.bmiHeader.biHeight = rcSurface.bottom;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    bmpRender = CreateDIBSection(NULL, &bmpInfo, DIB_RGB_COLORS, (void**)&pbmpRender, NULL, 0);


    

    another_world::read_file = [](std::wstring filename, uint32_t offset, uint32_t length, char* buffer) {     
      filename = L"c:\\another-world-data\\" + filename;
      HANDLE fh = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);      
      SetFilePointer(fh, offset, NULL, FILE_BEGIN);
      DWORD bytes_read = NULL;
      BOOL result = ReadFile(fh, buffer, length, &bytes_read, NULL);
      CloseHandle(fh);
      return result && bytes_read == length;
    };

    another_world::debug = [](std::string message) {      
      std::wstring w(message.begin(), message.end());
      //OutputDebugStringW(w.c_str());
      //OutputDebugStringW(L"\n");
    };

    another_world::update_screen = [](uint8_t* buffer) {
      memcpy(screen, buffer, 320 * 200 / 2);
    };

    another_world::tick_yield = [](uint32_t ticks) {
      //InvalidateRect(hWnd, NULL, FALSE);
      UpdateWindow(hWnd);
      /*MSG msg;
      while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
          break;
        }

        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }*/
    };

    load_resource_list();
    vm.init();

    /*
    resources[19]->state = Resource::State::NEEDS_LOADING;  // main screen
    resources[20]->state = Resource::State::NEEDS_LOADING;  // main screen palette
    load_needed_resources();
    */

    // initialise the first chapter
    vm.initialise_chapter(16001);
    
    while (true) {
      // if there is a message to process then do it
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
          break;
        }

        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
      }
     
      vm.tick();

      InvalidateRect(hWnd, NULL, FALSE);
    }


    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ANOTHERWORLD));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_ANOTHERWORLD);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void vram_to_bmp(LPBYTE pd, uint8_t* ps) {
  for (uint16_t y = 0; y < 320; y++) {
    for (uint16_t x = 0; x < 200; x += 2) {
      uint8_t p = *ps;
      uint8_t p1 = p >> 4;
      uint8_t p2 = p & 0x0f;

      p1 <<= 4;
      p2 <<= 4;

      *pd++ = BYTE(p1);
      *pd++ = BYTE(p1);
      *pd++ = BYTE(p1);

      *pd++ = BYTE(p2);
      *pd++ = BYTE(p2);
      *pd++ = BYTE(p2);

      ps++;
    }
  }
}
//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_SIZE:
    {
      uint16_t height = HIWORD(lParam);
      uint16_t width = LOWORD(lParam);

      uint16_t width_scale = floor(width / rcSurface.right);
      uint16_t height_scale = floor(height / rcSurface.bottom);
      pixelSize = (uint8_t)(width_scale < height_scale ? width_scale : height_scale);

      // scale and centre the view rectangle
      SetRect(&rcView, 0, 0, rcSurface.right * pixelSize, rcSurface.bottom * pixelSize);
      OffsetRect(&rcView, (width - rcView.right) / 2, (height - rcView.bottom) / 2);
    }break;
    case WM_PAINT:
        {
          PAINTSTRUCT ps;
          HDC hdc = BeginPaint(hWnd, &ps);

          RECT rcClient;
          GetClientRect(hWnd, &rcClient);

          // setup a back buffer device context to render into
          HDC drawDC = CreateCompatibleDC(hdc);
          HBITMAP bmpDraw = CreateCompatibleBitmap(hdc, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);
          HBITMAP hbmOld = (HBITMAP)SelectObject(drawDC, bmpDraw);


          HBRUSH backgroundBrush = CreateSolidBrush(RGB(30, 40, 50));
          FillRect(drawDC, &rcClient, backgroundBrush);

          if (bmpRender) {
            vram_to_bmp(pbmpRender, screen);
            StretchDIBits(drawDC, rcView.left, rcView.top, (rcView.right - rcView.left) / 2, (rcView.bottom - rcView.top) / 2, 0, rcSurface.bottom + 1, rcSurface.right, -rcSurface.bottom, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            vram_to_bmp(pbmpRender, vram[0]);
            StretchDIBits(drawDC, rcView.left + (rcView.right - rcView.left) / 2, rcView.top, (rcView.right - rcView.left) / 2, (rcView.bottom - rcView.top) / 2, 0, rcSurface.bottom + 1, rcSurface.right, -rcSurface.bottom, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            vram_to_bmp(pbmpRender, vram[1]);
            StretchDIBits(drawDC, rcView.left, rcView.top + (rcView.bottom - rcView.top) / 2, (rcView.right - rcView.left) / 2, (rcView.bottom - rcView.top) / 2, 0, rcSurface.bottom + 1, rcSurface.right, -rcSurface.bottom, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            vram_to_bmp(pbmpRender, vram[2]);
            StretchDIBits(drawDC, rcView.left + (rcView.right - rcView.left) / 2, rcView.top + (rcView.bottom - rcView.top) / 2, (rcView.right - rcView.left) / 2, (rcView.bottom - rcView.top) / 2, 0, rcSurface.bottom + 1, rcSurface.right, -rcSurface.bottom, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);
          }

          BitBlt(hdc, rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, drawDC, 0, 0, SRCCOPY);

          SelectObject(drawDC, hbmOld);
          DeleteDC(drawDC);
          DeleteObject(bmpDraw);
          DeleteObject(backgroundBrush);


            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
