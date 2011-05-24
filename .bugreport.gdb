#
# UnrealIRCd Bug Reporting Script
# Copyright (c) 2001, The UnrealIRCd Team
# All rights reserved
#
#   Redistribution and use in source and binary forms, with or without modification, are permitted
#   provided that the following conditions are met:
#   
#     * Redistributions of source code must retain the above copyright notice, this list of conditions
#       and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions
#       and the following disclaimer in the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of the The UnrealIRCd Team nor the names of its contributors may be used
#       to endorse or promote products derived from this software without specific prior written permission.
#     * The source code may not be redistributed for a fee or in closed source
#       programs, without expressed oral consent by the UnrealIRCd Team, however
#       for operating systems where binary distribution is required, if URL
#       is passed with the package to get the full source
#     * No warranty is given unless stated so by the The UnrealIRCd Team
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'' AND ANY EXPRESS OR
#   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
#   FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
#   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR  
#   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# First we define some nice settings and some nice functions
set print pretty on

# dumplist <linked list> <structure format>
define dumplist
echo Dumping linked list $arg0 in format $arg1\n
set $p = $arg0
while $p
	print *($arg1 *) $p
	set $p = $p->next
end
end

# dumparray <name> <size>
define dumparray
echo Dumping array $arg0 size $arg1\n
set $p = 0
while $p < $arg1
	if $arg0[$p]
		print *$arg0[$p]
	end
	set $p = $p + 1
end
end

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
dumparray Modules 50
quit
