@echo off
title Restarting DA Flash
powershell -NoProfile -Command "Stop-Process -Name DAFlash, AntigravityApp, WeaselServer -Force -ErrorAction SilentlyContinue; Start-Process 'C:\Program Files\Rime\weasel-0.17.4\WeaselServer.exe'; Start-Sleep -Seconds 1; Start-Process 'D:\DEV\DAFlash\build\DAFlash.exe' -WorkingDirectory 'D:\DEV\DAFlash\build'"
