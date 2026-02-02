' Запуск monitor.py без окна консоли. Рабочая папка = папка скрипта.
Set WshShell = CreateObject("WScript.Shell")
WshShell.CurrentDirectory = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.Run "pythonw monitor.py", 0, False
