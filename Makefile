pg.lib:    *.c *.h
	cl -nologo -we4013 -Zi -Ox -fp:fast -c -I .. *.c && lib -nologo -nodefaultlib -out:pg.lib *.obj
