#include <Windows.h>
#include <WinUser.h>
#include <stdio.h>
#include <stdlib.h>

static HWINEVENTHOOK win_event_hook = NULL;
static HHOOK mouse_hook = NULL;
static HHOOK keyboard_hook = NULL;

static HWND foreground_window = NULL;

static BOOL dragging = FALSE;
static BOOL win_down = FALSE;

static INT last_x = 0;
static INT last_y = 0;

static LRESULT CALLBACK keyboard_hook_proc(
  CONST INT n_code,
  WPARAM w_param,
  CONST LPARAM l_param
) {
  if (n_code >= 0) {
    KBDLLHOOKSTRUCT* kbd_struct = (KBDLLHOOKSTRUCT*)l_param;

    if (kbd_struct->vkCode == VK_LWIN || kbd_struct->vkCode == VK_RWIN) {
      if (w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN)
        win_down = TRUE;
      else if (w_param == WM_KEYUP || w_param == WM_SYSKEYUP)
        win_down = FALSE;
    }
  }

  return CallNextHookEx(keyboard_hook, n_code, w_param, l_param);
}

static LRESULT CALLBACK mouse_hook_proc(
  CONST INT n_code,
  CONST WPARAM w_param,
  CONST LPARAM l_param
) {
  if (n_code >= 0) {
    CONST MOUSEHOOKSTRUCT* p_mouse = (MOUSEHOOKSTRUCT*)l_param;

    CONST INT x = p_mouse->pt.x;
    CONST INT y = p_mouse->pt.y;

    if (!win_down)
      dragging = FALSE;
    else {
      if (w_param == WM_LBUTTONDOWN) {
        last_x = x;
        last_y = y;
        dragging = TRUE;
      }
      else if (w_param == WM_LBUTTONUP)
        dragging = FALSE;
    }

    if (dragging) {
      CONST INT delta_x = x - last_x;
      CONST INT delta_y = y - last_y;

      foreground_window = GetForegroundWindow();

      CONST DWORD dw_style = GetWindowLong(foreground_window, GWL_STYLE);
      CONST BOOL has_topbar =
        (dw_style & WS_CAPTION) != 0 ||
        (dw_style & WS_EX_TOPMOST) != 0 ||
        (dw_style & WS_EX_DLGMODALFRAME) != 0;

      if (!has_topbar) {
        foreground_window = NULL;

        return CallNextHookEx(NULL, n_code, w_param, l_param);
      }

      if ((dw_style & WS_MAXIMIZE) == 0) {
        RECT rect = { 0 };
        GetWindowRect(foreground_window, &rect);

        CONST INT pos_x = rect.left;
        CONST INT pos_y = rect.top;

        if (abs(delta_x) > 0 || abs(delta_y) > 0) {
          if (SetWindowPos(
            foreground_window, NULL,
            pos_x + delta_x,
            pos_y + delta_y,
            0, 0,
            SWP_NOSIZE | SWP_NOREDRAW | SWP_NOACTIVATE
          )) {
            last_x = x;
            last_y = y;
          }
        }
      }
    }
  }
  
  return CallNextHookEx(NULL, n_code, w_param, l_param);
}

INT main(
  INT argc,
  CHAR** argv
) {
  keyboard_hook = SetWindowsHookEx(
    WH_KEYBOARD_LL,
    keyboard_hook_proc,
    GetModuleHandle(NULL),
    0
  );

  if (keyboard_hook == NULL) {
    UnhookWinEvent(win_event_hook);
    win_event_hook = NULL;

    printf("Failed to register event hook for WH_KEYBOARD_LL\n");

    return -1;
  }

  mouse_hook = SetWindowsHookEx(
    WH_MOUSE_LL,
    mouse_hook_proc,
    GetModuleHandle(NULL),
    0
  );

  if (mouse_hook == NULL) {
    UnhookWindowsHookEx(keyboard_hook);
    keyboard_hook = NULL;
    
    UnhookWinEvent(win_event_hook);
    win_event_hook = NULL;

    printf("Failed to register event hook for WH_MOUSE_LL\n");

    return -1;
  }

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));

  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  UnhookWindowsHookEx(mouse_hook);
  mouse_hook = NULL;

  UnhookWindowsHookEx(keyboard_hook);
  keyboard_hook = NULL;
  
  UnhookWinEvent(win_event_hook);
  win_event_hook = NULL;

  return 0;
}