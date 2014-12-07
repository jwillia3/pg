asg.lib:    *.c *.h
	cl -nologo -Zi -fp:fast -WX  -c *.c && lib -nologo -nodefaultlib -out:asg.lib *.obj
