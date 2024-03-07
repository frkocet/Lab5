@echo off
::This file was created automatically by CrossIDE to compile with C51.
C:
cd "\Users\ethan\OneDrive - UBC\Desktop\Lab5\"
"C:\CrossIDE\Call51\Bin\c51.exe" --use-stdout  "C:\Users\ethan\OneDrive - UBC\Desktop\Lab5\zero cross detection.c"
if not exist hex2mif.exe goto done
if exist zero cross detection.ihx hex2mif zero cross detection.ihx
if exist zero cross detection.hex hex2mif zero cross detection.hex
:done
echo done
echo Crosside_Action Set_Hex_File C:\Users\ethan\OneDrive - UBC\Desktop\Lab5\zero cross detection.hex
