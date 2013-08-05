@ECHO OFF
IF NOT EXIST deps\hiredis GOTO NOHIREDIS
	rmdir /Q /S deps\hiredis
:NOHIREDIS