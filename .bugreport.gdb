echo Full backtrace:\n
echo ---------------\n
echo \n
bt full
echo \n
echo Backup parse() buffer:\n
echo ----------------------\n
echo \n
print backupbuf
echo \n
echo me output:\n
echo ----------------------\n
print me
echo \n
echo IRCstats:\n
echo ----------------------\n
print IRCstats
echo \n
echo Modules:\n
echo ----------------------\n
set $i = 0
while $i < 50
if Modules[$i++]
	p *Modules[$i - 1]
end
end
quit
