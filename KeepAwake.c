#define UNICODE
#include <windows.h>
#include <shellapi.h>    // Shell_NotifyIcon, ShellExecute
#include "resource.h"    // 包含资源 ID 定义（IDI_TRAYICON, ID_TRAY_SHOW, ID_TRAY_HOME, ID_TRAY_EXIT, WM_TRAYICON）

// 定时器 ID
#define IDT_MOUSE_TIMER    1

// 全局变量
HINSTANCE       g_hInstance;
HWND            g_hWnd;         // 隐藏的消息窗口句柄
NOTIFYICONDATA  g_nid;          // 托盘图标结构

// 函数声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowTrayMenu(HWND hWnd, POINT pt);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void MouseNudge();  // 微微移动鼠标

// -----------------------------------------------------------------------------
// WinMain：程序入口
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    g_hInstance = hInstance;

    // 1. 注册一个“隐藏窗口”类（不显示任何界面，但用于接收定时器和托盘消息）
    const wchar_t CLASS_NAME[] = L"KeepAwakeMouseTrayClass";
    WNDCLASSEX wx = {0};
    wx.cbSize        = sizeof(WNDCLASSEX);
    wx.lpfnWndProc   = WndProc;
    wx.hInstance     = hInstance;
    wx.lpszClassName = CLASS_NAME;
    RegisterClassEx(&wx);

    // 2. 创建一个“隐藏窗口”（WS_EX_TOOLWINDOW + WS_POPUP，不显示在任务栏）
    g_hWnd = CreateWindowEx(
        WS_EX_TOOLWINDOW,      // 不在任务栏显示
        CLASS_NAME,            // 窗口类
        L"KeepAwakeMouseTrayWnd",   // 窗口名（无实际用处）
        WS_POPUP,              // 不可见
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );
    if (!g_hWnd) {
        MessageBox(NULL, L"创建隐藏窗口失败。", L"错误", MB_ICONERROR);
        return 0;
    }

    // 3. 首次调用一次微动鼠标，防止程序启动后系统立即休眠
    MouseNudge();

    // 4. 创建托盘图标
    AddTrayIcon(g_hWnd);

    // 5. 启动一个定时器：每 180000 毫秒（3 分钟）触发一次 WM_TIMER
    SetTimer(g_hWnd, IDT_MOUSE_TIMER, 180000, NULL);

    // 6. 进入消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// -----------------------------------------------------------------------------
// WndProc：隐藏窗口的消息回调
// -----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_TIMER:
        if (wParam == IDT_MOUSE_TIMER) {
            // 每次定时器触发都微动一次鼠标
            MouseNudge();
        }
        break;

    case WM_TRAYICON:
        // 托盘图标发来的消息：lParam 携带鼠标事件
        switch (LOWORD(lParam))
        {
        case WM_RBUTTONUP:
        {
            // 右键弹起：弹出菜单
            POINT pt;
            GetCursorPos(&pt);
            ShowTrayMenu(hWnd, pt);
        }
        break;

        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        {
            // 左键单击或双击：同样弹出菜单
            POINT pt2;
            GetCursorPos(&pt2);
            ShowTrayMenu(hWnd, pt2);
        }
        break;

        default:
            break;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_TRAY_SHOW:
            // 显示状态：提示程序正在后台运行
            MessageBox(NULL,
                L"KeepAwake 正在后台运行，每 3 分钟微动一次鼠标以防止系统休眠。",
                L"程序状态", MB_ICONINFORMATION);
            break;

        case ID_TRAY_HOME:
            // 打开项目主页
            ShellExecute(NULL,
                         L"open",
                         L"https://github.com/potapotapotato",
                         NULL, NULL, SW_SHOWNORMAL);
            break;

        case ID_TRAY_EXIT:
            // 退出程序：移除托盘图标并终止消息循环
            RemoveTrayIcon(hWnd);
            KillTimer(hWnd, IDT_MOUSE_TIMER);
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_DESTROY:
        // 确保托盘图标被移除
        RemoveTrayIcon(hWnd);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// MouseNudge：使用 mouse_event 将鼠标相对移动 1 像素后再移动回去
// -----------------------------------------------------------------------------
void MouseNudge()
{
    // 微微向右移动 1 像素
    mouse_event(MOUSEEVENTF_MOVE, 1, 0, 0, 0);
    Sleep(10);  // 确保移动生效
    // 再向左移动 1 像素，回到原位
    mouse_event(MOUSEEVENTF_MOVE, -1, 0, 0, 0);
}

// -----------------------------------------------------------------------------
// AddTrayIcon：在系统托盘区域添加图标并注册消息
// -----------------------------------------------------------------------------
void AddTrayIcon(HWND hWnd)
{
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(NOTIFYICONDATA);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = IDI_TRAYICON;                     // 唯一标识
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;                      // 自定义消息
    g_nid.hIcon            = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_TRAYICON));
    // 提示文本（鼠标悬停时显示），指定缓冲区长度
    wcscpy_s(g_nid.szTip, sizeof(g_nid.szTip)/sizeof(wchar_t), L"KeepAwake——点击查看菜单");

    // 向 Shell 注册托盘图标
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// -----------------------------------------------------------------------------
// RemoveTrayIcon：从托盘区域移除图标
// -----------------------------------------------------------------------------
void RemoveTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA nidDel = g_nid;
    Shell_NotifyIcon(NIM_DELETE, &nidDel);
}

// -----------------------------------------------------------------------------
// ShowTrayMenu：在指定屏幕坐标弹出上下文菜单
// -----------------------------------------------------------------------------
void ShowTrayMenu(HWND hWnd, POINT pt)
{
    // 创建一个临时菜单
    HMENU hMenu = CreatePopupMenu();
    if (hMenu)
    {
        // 添加菜单项：显示状态、打开主页、退出
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SHOW, L"显示状态");
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_HOME, L"打开项目主页");
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"退出程序");

        // 设置为前景窗口，以便菜单能够正常弹出并响应点击
        SetForegroundWindow(hWnd);

        // 在鼠标位置弹出菜单，使用右键触发
        TrackPopupMenu(
            hMenu,
            TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
            pt.x,
            pt.y,
            0,
            hWnd,
            NULL
        );
        DestroyMenu(hMenu);
    }
}
