# Security Policy

## Supported Versions
* The latest *stable* release of UnrealIRCd 6

See [UnrealIRCd releases](https://www.unrealircd.org/docs/UnrealIRCd_releases)
for information on older versions and End Of Life dates.

## Scope

In general, issues triggered by regular users involving memory safety issues
(such as OOB read/write or UAF), sensitive information disclosure, privilege elevation,
Denial of Service (e.g. a crash), or remote code execution fall within the scope of
this security policy.

Issues that require IRCOp rights, server-to-server traffic, or editing of config
files may still fall within scope, but are classified case by case depending on
the impact and circumstances.

Issues that require shell access as the same user running UnrealIRCd are not
considered security issues. See the
[full policy](https://www.unrealircd.org/docs/Policy:_Handling_of_security_issues)
for details.

## Use of AI or other tools

It is normal and acceptable to use tools for finding security vulnerabilities.
We use them ourselves as well: AI, static code analyzers, fuzzing. This is all fine.

If a tool flagged an issue then we ask only **one extra thing**: that you
**reproduce the issue** on your own local server. So: confirm the issue by
actually running UnrealIRCd with a reproducer (which usually means: by sending
IRC traffic to trigger the bug). This is because tools regularly flag something
as an issue but in practice it may be impossible to happen because of some extra
check somewhere or other requirements.

If you are trying to reproduce an issue, then we suggest running `./Config` and
answering `Yes` to the near-last question about AddressSanitizer (ASan). Please
include both the reproducer and the ASan output in the bug report. It helps us a
lot.

If you are submitting issues and fail to follow the procedure above, expect us
to ask you again to reproduce the issue. If you refuse to do so, don't respond
in a timely manner, or send in multiple reports without doing so, then we will
close the bug report and may proceed with putting you on ignore, banning or
deleting your account, or similar. Giving a reproducer is not a big ask and is
normal procedure nowadays. It should be part of your standard workflow if you
are a security researcher.

## Reporting a Vulnerability

Please report issues on the [bug tracker](https://bugs.unrealircd.org) and in
the bug submit form **set the 'View Status' to 'private'**.

Do not report security issues as a Pull Request, on the forums or in a public
IRC channel such as #unreal-support. If you insist on e-mail then you can use
syzop@unrealircd.org or security@unrealircd.org. Again, the bug tracker is
preferred.

If you found a real issue but are *unsure* if it is a security issue, then
report it at the bug tracker as a 'private' bug anyway. Better safe than sorry.
Do not ask around in public channels or forums.

You should get a response or at least an acknowledgement soon. If you don't hear
back within 24 hours, then please try to contact us again.

## Full policy
See https://www.unrealircd.org/docs/Policy:_Handling_of_security_issues for full information.
