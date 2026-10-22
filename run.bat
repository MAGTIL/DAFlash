@echo off
title Starting DA Flash
powershell -NoProfile -Command "if (-not (Get-Process WeaselServer -ErrorAction SilentlyContinue)) { Start-Process 'C:\Program Files\Rime\weasel-0.17.4\WeaselServer.exe'; Start-Sleep -Seconds 1 }; if (-not (Get-Process DAFlash -ErrorAction SilentlyContinue)) { Start-Process 'D:\DEV\DAFlash\build\DAFlash.exe' -WorkingDirectory 'D:\DEV\DAFlash\build' }"
