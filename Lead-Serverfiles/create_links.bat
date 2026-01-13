@echo off

for %%x in (db, auth, channel1/game1, channel1/game2, channel99) do (

	@rmdir /q "C:/Lead-Git/Lead-Serverfiles/%%x/data" >nul 2>&1
	@del /Q "C:/Lead-Git/Lead-Serverfiles/%%x\data" >nul 2>&1
	@mklink /D "C:/Lead-Git/Lead-Serverfiles/%%x/data" "C:/Lead-Git/Lead-Serverfiles/share/data"

	rmdir /q "C:/Lead-Git/Lead-Serverfiles/%%x/locale" >nul 2>&1
	@del /Q "C:/Lead-Git/Lead-Serverfiles/%%x\locale" >nul 2>&1
	@mklink /D "C:/Lead-Git/Lead-Serverfiles/%%x/locale" "C:/Lead-Git/Lead-Serverfiles/share/locale"
)
