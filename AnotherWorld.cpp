// AnotherWorld.cpp : Defines the entry point for the application.
//

#include <chrono>

#include "windowsx.h"
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
HFONT hFont;
uint8_t screen[320 * 200 / 2];
uint8_t palette[16][3];
uint8_t pixelSize;
std::chrono::steady_clock::time_point start;
uint8_t keys;

uint32_t now() {
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
  return (uint32_t)elapsed.count();
}

// from Erik Aronesty's response on https://stackoverflow.com/a/8098080
std::string string_format(const std::string fmt, va_list ap) {
  int size = ((int)fmt.size()) * 2 + 50;   // Use a rubric appropriate for your code
  std::string str;
  //va_list ap;
  while (1) {     // Maximum two passes on a POSIX system...
    str.resize(size);
    //va_start(ap, fmt);
    int n = vsnprintf((char*)str.data(), size, fmt.c_str(), ap);
    //va_end(ap);
    if (n > -1 && n < size) {  // Everything worked
      str.resize(n);
      return str;
    }
    if (n > -1)  // Needed size returned
      size = n + 1;   // For null char
    else
      size *= 2;      // Guess at a larger size (OS specific)
  }
  return str;
}

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

    hFont = CreateFontA(14, 0, 0, 0, 700, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, "Consolas");        

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ANOTHERWORLD));

    MSG msg;
    
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = 320;
    bmpInfo.bmiHeader.biHeight = 200;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    bmpRender = CreateDIBSection(NULL, &bmpInfo, DIB_RGB_COLORS, (void**)&pbmpRender, NULL, 0);


    

    another_world::read_file = [](std::string filename, uint32_t offset, uint32_t length, char* buffer) {     
      filename = "c:\\another-world-data\\" + filename;
      std::wstring wfilename(filename.begin(), filename.end());
      HANDLE fh = CreateFile(wfilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      SetFilePointer(fh, offset, NULL, FILE_BEGIN);
      DWORD bytes_read = NULL;
      BOOL result = ReadFile(fh, buffer, length, &bytes_read, NULL);
      CloseHandle(fh);
      return result && bytes_read == length;
    };

    another_world::debug_log = [](const char *fmt, ...) {
      static bool first = true; // if first run then open the file and truncate
      return;
      va_list args;
      std::wstring filename = L"vm.log";
      va_start(args, fmt);
      std::string line = string_format(fmt, args) + "\n";
      va_end(args);
      DWORD creation_mode = first ? CREATE_ALWAYS : OPEN_EXISTING;
      HANDLE fh = CreateFile(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, creation_mode, FILE_ATTRIBUTE_NORMAL, NULL);
      SetFilePointer(fh, 0, NULL, FILE_END);
      
      WriteFile(fh, line.c_str(), line.length(), NULL, NULL);

      CloseHandle(fh);

      first = false;
    };

    another_world::debug = [](const char *fmt, ...) {
      va_list args;
      va_start(args, fmt);
      std::string line = string_format(fmt, args) + "\n";
      va_end(args);

      std::wstring w(line.begin(), line.end());
      OutputDebugStringW(w.c_str());
    };

    another_world::update_screen = [](uint8_t *buffer) {
      memcpy(screen, buffer, 320 * 200 / 2);
    };

    another_world::set_palette = [](uint16_t* p) {
      // sixten palette entries in the format 0x0RGB
      for (uint8_t i = 0; i < 16; i++) {
        uint16_t color = p[i];

        uint8_t r = (color & 0b0000000000001111) >> 0;
        uint8_t g = (color & 0b1111000000000000) >> 12;
        uint8_t b = (color & 0b0000111100000000) >> 8;
        palette[i][0] = (r << 4) | r;
        palette[i][1] = (g << 4) | g;
        palette[i][2] = (b << 4) | b; 
      }
    };

    another_world::debug_yield = []() {
      InvalidateRect(hWnd, NULL, FALSE);
      UpdateWindow(hWnd);

    };

    load_resource_list();
    vm.init();

    // initialise the first chapter
    vm.initialise_chapter(16005);
    
    start = std::chrono::steady_clock::now();

    uint32_t last_frame_clock = now();

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
     
      uint32_t clock = now();

      // aim for 25 frames per second
      uint16_t pause_ms = 20 * vm.registers[0xff];
     // pause_ms = 0;
      if(clock - last_frame_clock > pause_ms) {
        uint32_t frame_start = now();
        debug("-------------------------------");


        debug("Key state %d", keys);
        
        vm.registers[0xE5] = 0;
        vm.registers[0xFB] = 0;
        vm.registers[0xFC] = 0;


        if (keys & 0b00001000) {
          vm.registers[0xE5] = -1;
          vm.registers[0xFB] = -1;
        }

        if (keys & 0b00000100) {
          vm.registers[0xE5] = 1;
          vm.registers[0xFB] = 1;
        }

        if (keys & 0b00000010) {
          vm.registers[0xFC] = -1;
        }

        if (keys & 0b00000001) {
          vm.registers[0xFC] = 1;
        }


        vm.registers[0xFD] = keys;
        vm.registers[0xFA] = keys & 0b10000000 ? 1 : 0;
        vm.registers[0xFE] = keys;

        
        vm.execute_threads();
        //debug("Frame took %dms", now() - frame_start);
        last_frame_clock = clock;
        InvalidateRect(hWnd, NULL, FALSE);
      }      
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
      100, 100, 320 * 4 + 100, 200 * 4 + 100, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void vram_to_bmp(LPBYTE pd, uint8_t* ps) {
  for (uint16_t y = 0; y < 200; y++) {
    uint8_t* ppd = &pd[(199 - y) * 320 * 3];

    for (uint16_t x = 0; x < 320; x += 2) {
      uint8_t p = *ps;
      uint8_t p1 = p >> 4;
      uint8_t p2 = p & 0x0f;

      *ppd++ = palette[p1][2];
      *ppd++ = palette[p1][1];
      *ppd++ = palette[p1][0];

      *ppd++ = palette[p2][2];
      *ppd++ = palette[p2][1];
      *ppd++ = palette[p2][0];

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
    case WM_KEYDOWN:
    {
      switch (wParam) {
        case VK_UP: {
          keys |= 0b00001000;
          break;
        }
        case VK_DOWN: {
          keys |= 0b00000100;
          break;
        }
        case VK_LEFT: {
          keys |= 0b00000010;
          break;
        }
        case VK_RIGHT: {
          keys |= 0b00000001;
          break;
        }
        case VK_SPACE: {
          keys |= 0b10000000;
          break;
        }
      }
    }break;
    case WM_KEYUP:
    {
      switch (wParam) {
        case VK_UP: {
          keys &= ~0b00001000;
          break;
        }
        case VK_DOWN: {
          keys &= ~0b00000100;
          break;
        }
        case VK_LEFT: {
          keys &= ~0b00000010;
          break;
        }
        case VK_RIGHT: {
          keys &= ~0b00000001;
          break;
        }
        case VK_SPACE: {
          keys &= ~0b10000000;
          break;
        }
      }
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
            uint16_t thumb_width = (rcView.right - rcView.left) / 4;
            uint16_t thumb_height = (rcView.bottom - rcView.top) / 4;

            RECT rcThumb;
            SetRect(&rcThumb, rcView.left, rcView.bottom - thumb_height, rcView.left + thumb_width, rcView.bottom);
            InflateRect(&rcThumb, -10, -10);

            vram_to_bmp(pbmpRender, screen); // background
            StretchDIBits(drawDC, rcView.left, rcView.top, rcView.right - rcView.left, rcView.bottom - rcView.top, 0, 0, 320, 200, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);
            
            vram_to_bmp(pbmpRender, vram0); // screen
            StretchDIBits(drawDC, rcThumb.left, rcThumb.top, rcThumb.right - rcThumb.left, rcThumb.bottom - rcThumb.top, 0, 0, 320, 200, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            rcThumb.left += thumb_width; rcThumb.right += thumb_width;

            vram_to_bmp(pbmpRender, vram1); // screen
            StretchDIBits(drawDC, rcThumb.left, rcThumb.top, rcThumb.right - rcThumb.left, rcThumb.bottom - rcThumb.top, 0, 0, 320, 200, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            rcThumb.left += thumb_width; rcThumb.right += thumb_width;

            vram_to_bmp(pbmpRender, vram2); // screen
            StretchDIBits(drawDC, rcThumb.left, rcThumb.top, rcThumb.right - rcThumb.left, rcThumb.bottom - rcThumb.top, 0, 0, 320, 200, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);

            rcThumb.left += thumb_width; rcThumb.right += thumb_width;

            vram_to_bmp(pbmpRender, vram3); // screen
            StretchDIBits(drawDC, rcThumb.left, rcThumb.top, rcThumb.right - rcThumb.left, rcThumb.bottom - rcThumb.top, 0, 0, 320, 200, pbmpRender, &bmpInfo, DIB_RGB_COLORS, SRCCOPY);
            
          }

          BitBlt(hdc, rcClient.left, rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, drawDC, 0, 0, SRCCOPY);

          std::wstring tick_count = std::to_wstring(vm.ticks);

          SelectFont(hdc, hFont);
          DrawText(hdc, tick_count.c_str(), -1, &rcClient, DT_TOP | DT_LEFT);

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
