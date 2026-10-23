"""Post wheel messages only to the test owner's resized GLFW window."""
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import sys
import time


def main():
    owner,width,height=map(int,sys.argv[1:4]);ready,sent=map(Path,sys.argv[4:6])
    user=ctypes.WinDLL('user32',use_last_error=True);callback_type=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
    user.EnumWindows.argtypes=(callback_type,wintypes.LPARAM);user.EnumWindows.restype=wintypes.BOOL
    user.GetWindowThreadProcessId.argtypes=(wintypes.HWND,ctypes.POINTER(wintypes.DWORD));user.GetWindowThreadProcessId.restype=wintypes.DWORD
    user.GetClientRect.argtypes=(wintypes.HWND,ctypes.POINTER(wintypes.RECT));user.GetClientRect.restype=wintypes.BOOL
    user.IsWindowVisible.argtypes=(wintypes.HWND,);user.IsWindowVisible.restype=wintypes.BOOL
    user.ClientToScreen.argtypes=(wintypes.HWND,ctypes.POINTER(wintypes.POINT));user.ClientToScreen.restype=wintypes.BOOL
    user.PostMessageW.argtypes=(wintypes.HWND,wintypes.UINT,wintypes.WPARAM,wintypes.LPARAM);user.PostMessageW.restype=wintypes.BOOL
    ready.touch();deadline=time.monotonic()+20
    while time.monotonic()<deadline:
        found=None
        def visit(window,_):
            nonlocal found
            pid=wintypes.DWORD();user.GetWindowThreadProcessId(window,ctypes.byref(pid));rect=wintypes.RECT()
            if pid.value==owner and user.IsWindowVisible(window) and user.GetClientRect(window,ctypes.byref(rect)) and (rect.right-rect.left,rect.bottom-rect.top)==(width,height):
                found=window;return False
            return True
        callback=callback_type(visit);user.EnumWindows(callback,0)
        if found:
            # Let resize/focus delivery and the scripted pointer settle first.
            time.sleep(.1)
            point=wintypes.POINT(width*3//4,height//2)
            if not user.ClientToScreen(found,ctypes.byref(point)):raise ctypes.WinError(ctypes.get_last_error())
            position=(point.x&0xffff)|((point.y&0xffff)<<16)
            for _ in range(3):
                if not user.PostMessageW(found,0x020A,((-120)&0xffff)<<16,position):raise ctypes.WinError(ctypes.get_last_error())
            sent.write_text(json.dumps(dict(messages=3,wheel_steps=-3,owner=owner)),encoding='utf-8');return
        time.sleep(.001)
    raise TimeoutError('owner did not reach the test resize')


if __name__=='__main__':main()
