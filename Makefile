asg.lib:    *.c *.h
	cl -nologo -Zi -fp:fast -WX -c -I .. *.c && lib -nologo -nodefaultlib -out:ags.lib *.obj
