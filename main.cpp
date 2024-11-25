#include <windows.h>
#include <iostream>
#include <string>

HWND nextClipboardViewer;

bool SaveBitmapToFile(HBITMAP hBitmap, const std::wstring& filePath) {
    BITMAP bmp;
    PBITMAPINFO pbmi;
    WORD cClrBits;
    HANDLE hf;                  // file handle
    BITMAPFILEHEADER hdr;       // bitmap file-header
    PBITMAPINFOHEADER pbih;     // bitmap info-header
    LPBYTE lpBits;              // memory pointer
    DWORD cb;                   // incremental count of bytes
    BYTE *hp;                   // byte pointer
    DWORD dwTmp;

    if (!GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bmp)) {
        std::cerr << "Failed to get bitmap object" << std::endl;
        return false;
    }

    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
    if (cClrBits == 1)
        cClrBits = 1;
    else if (cClrBits <= 4)
        cClrBits = 4;
    else if (cClrBits <= 8)
        cClrBits = 8;
    else if (cClrBits <= 16)
        cClrBits = 16;
    else if (cClrBits <= 24)
        cClrBits = 24;
    else cClrBits = 32;

    if (cClrBits != 24)
        pbmi = (PBITMAPINFO) LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * (1 << cClrBits));
    else
        pbmi = (PBITMAPINFO) LocalAlloc(LPTR, sizeof(BITMAPINFOHEADER));

    pbih = (PBITMAPINFOHEADER) pbmi;
    pbih->biSize = sizeof(BITMAPINFOHEADER);
    pbih->biWidth = bmp.bmWidth;
    pbih->biHeight = bmp.bmHeight;
    pbih->biPlanes = bmp.bmPlanes;
    pbih->biBitCount = bmp.bmBitsPixel;
    if (cClrBits < 24)
        pbih->biClrUsed = (1 << cClrBits);

    pbih->biCompression = BI_RGB;
    pbih->biSizeImage = ((pbih->biWidth * cClrBits + 31) & ~31) / 8 * pbih->biHeight;
    pbih->biClrImportant = 0;

    lpBits = (LPBYTE) GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);

    if (!lpBits) {
        std::cerr << "Failed to allocate memory for bitmap" << std::endl;
        return false;
    }

    if (!GetDIBits(GetDC(nullptr), hBitmap, 0, (WORD) pbih->biHeight, lpBits, pbmi, DIB_RGB_COLORS)) {
        std::cerr << "Failed to get DIB bits" << std::endl;
        return false;
    }

    hf = CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, (DWORD) 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE) nullptr);

    if (hf == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create file" << std::endl;
        return false;
    }

    hdr.bfType = 0x4d42;
    hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD) + pbih->biSizeImage);
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + pbih->biSize + pbih->biClrUsed * sizeof(RGBQUAD);

    if (!WriteFile(hf, (LPVOID) &hdr, sizeof(BITMAPFILEHEADER), (LPDWORD) &dwTmp, nullptr)) {
        std::cerr << "Failed to write file header" << std::endl;
        return false;
    }

    if (!WriteFile(hf, (LPVOID) pbih, sizeof(BITMAPINFOHEADER) + pbih->biClrUsed * sizeof(RGBQUAD), (LPDWORD) &dwTmp, nullptr)) {
        std::cerr << "Failed to write info header" << std::endl;
        return false;
    }

    cb = pbih->biSizeImage;
    hp = lpBits;
    if (!WriteFile(hf, (LPSTR) hp, (int) cb, (LPDWORD) &dwTmp, nullptr)) {
        std::cerr << "Failed to write bitmap data" << std::endl;
        return false;
    }

    if (!CloseHandle(hf)) {
        std::cerr << "Failed to close file handle" << std::endl;
        return false;
    }

    GlobalFree((HGLOBAL)lpBits);
    LocalFree(pbmi);

    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            nextClipboardViewer = SetClipboardViewer(hwnd);
            return 0;
        case WM_CHANGECBCHAIN:
            if ((HWND)wParam == nextClipboardViewer) {
                nextClipboardViewer = (HWND)lParam;
            } else if (nextClipboardViewer != nullptr) {
                SendMessage(nextClipboardViewer, uMsg, wParam, lParam);
            }
            return 0;
        case WM_DRAWCLIPBOARD:
            if (OpenClipboard(nullptr)) {
                HANDLE hData = GetClipboardData(CF_BITMAP);
                if (hData) {
                    auto hBitmap = (HBITMAP)hData;
                    if (SaveBitmapToFile(hBitmap, L"clipboard_image.bmp")) {
                        std::wcout << L"Clipboard image saved to clipboard_image.bmp" << std::endl;
                    } else {
                        std::cerr << "Failed to save clipboard image" << std::endl;
                    }
                } else {
                    hData = GetClipboardData(CF_UNICODETEXT);
                    if (hData) {
                        auto* buffer = (wchar_t*)GlobalLock(hData);
                        if (buffer) {
                            // Convert wide string to UTF-8
                            int size_needed = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
                            std::string utf8_str(size_needed, 0);
                            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &utf8_str[0], size_needed, nullptr, nullptr);
                            std::cout << "Clipboard content changed: \n" << utf8_str << std::endl;
                            GlobalUnlock(hData);
                        } else {
                            std::cerr << "Failed to lock global memory" << std::endl;
                        }
                    } else {
                        std::cerr << "No text or bitmap data in clipboard" << std::endl;
                    }
                }
                CloseClipboard();
            } else {
                std::cerr << "Failed to open clipboard" << std::endl;
            }
            if (nextClipboardViewer != nullptr) {
                SendMessage(nextClipboardViewer, uMsg, wParam, lParam);
            }
            return 0;
        case WM_DESTROY:
            ChangeClipboardChain(hwnd, nextClipboardViewer);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ClipboardViewerClass";

    if (!RegisterClass(&wc)) {
        std::cerr << "Failed to register window class" << std::endl;
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, L"ClipboardViewerClass", L"Clipboard Viewer", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        std::cerr << "Failed to create window" << std::endl;
        return 1;
    }

    ShowWindow(hwnd, SW_HIDE);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}