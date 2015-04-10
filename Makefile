pg.lib:    *.c *.h
	cl -nologo -Zi -fp:fast -c -I .. *.c && lib -nologo -nodefaultlib -out:pg.lib *.obj
