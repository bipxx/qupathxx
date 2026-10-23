"""Hold startup input locked until the Win32 owner reaches a replayed resize."""
import ctypes
import os
from pathlib import Path
import sqlite3
import sys
import time


def main():
    source,ready,release,painted=map(Path,sys.argv[1:5]);owner,width,height=map(int,sys.argv[5:8])
    db=sqlite3.connect(source);db.execute("BEGIN EXCLUSIVE");ready.touch()
    try:
        if os.name!="nt":
            time.sleep(2)
            return
        from ctypes import wintypes
        user=ctypes.WinDLL("user32",use_last_error=True)
        callback_type=ctypes.WINFUNCTYPE(wintypes.BOOL,wintypes.HWND,wintypes.LPARAM)
        user.EnumWindows.argtypes=(callback_type,wintypes.LPARAM);user.EnumWindows.restype=wintypes.BOOL
        user.GetWindowThreadProcessId.argtypes=(wintypes.HWND,ctypes.POINTER(wintypes.DWORD));user.GetWindowThreadProcessId.restype=wintypes.DWORD
        user.GetClientRect.argtypes=(wintypes.HWND,ctypes.POINTER(wintypes.RECT));user.GetClientRect.restype=wintypes.BOOL
        user.IsWindowVisible.argtypes=(wintypes.HWND,);user.IsWindowVisible.restype=wintypes.BOOL
        deadline=time.monotonic()+20
        while not release.exists():
            reached=False
            def visit(window,_):
                nonlocal reached
                pid=wintypes.DWORD();user.GetWindowThreadProcessId(window,ctypes.byref(pid))
                if pid.value==owner and user.IsWindowVisible(window):
                    rect=wintypes.RECT()
                    if user.GetClientRect(window,ctypes.byref(rect)) and (rect.right-rect.left,rect.bottom-rect.top)==(width,height):
                        reached=True
                        return False
                return True
            callback=callback_type(visit);user.EnumWindows(callback,0)
            if reached:
                painted.touch()
                break
            if time.monotonic()>deadline:
                raise TimeoutError("owner did not reach the replayed window resize")
            time.sleep(.005)
    finally:
        db.rollback();db.close()

if __name__=="__main__":main()
