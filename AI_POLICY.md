# AI Policy

At UnrealIRCd we use AI and other tools ourselves to help with writing
code, doing security audits and finding other bugs. We also use AI
heavily when writing new tests for our test suite, `unrealircd-tests`.

This document contains the rules on AI use for everyone contributing to
UnrealIRCd itself, the `unrealircd` repository.

For 3rd party modules in `unrealircd-contrib` there are separate
rules, see [Use of AI](https://www.unrealircd.org/docs/Rules_for_3rd_party_modules_in_unrealircd-contrib)
there.

## Rules for contributors

Generic coding guidelines can be found on
[the wiki](https://www.unrealircd.org/docs/Dev:Coding_guidelines).
If you want to contribute then please start there.

Regarding AI, the following rules apply to contributions from others
(PRs/patches):

1. Only humans may submit contributions or discuss them with us:
   we do not talk to bots.
2. If your contribution is mostly AI generated with little input from
   you, OR if you did not review the code line by line afterwards, then
   you MUST disclose that in the PR.
3. You are normally expected to understand your own contribution. You are
   able to answer questions about what the code does and why and how it
   interacts with the rest of UnrealIRCd. All this without you relying on
   AI for an answer. If that is something you clearly cannot meet, for
   example because you are not really a C programmer and asked AI to create
   a useful bugfix, then you MUST disclose that in your PR, so we know.
4. Rules #2 and #3 are not here to punish people using AI. They are there
   so it is clear that we need to pay extra attention when reviewing the
   code. For example, there are some typical pitfalls for AI.
5. Any disclosure happens in the PR description. Just one or two
   sentences is enough, don't overthink it.

## FAQ

### I just asked AI to fix a bug. Can I submit this fix?

Yes. If it fixes an actual bug that you experienced, then such a bugfix
would be more than welcome.

If you asked AI to do it and you don't really understand the code change
itself, that is perfectly fine as well, provided you disclose this in
the PR (see rules #2 and #3). Just say you let AI fix a particular bug,
as simple as that. Also tell us how you tested the patch, so we know it
fixed the issue for you. All of this makes it clear that we should use
the patch as a good starting point and review it carefully ourselves,
and that we should not expect you to answer any C coding questions.

### What if I just vibecode something?

Honestly? A big vibecoded PR coming out of nowhere is not something we
typically get excited about:
* Feature suggestions or major cleaning/rewriting of code should be
  discussed first. See the
  [Discussing changes](https://www.unrealircd.org/docs/Contributing#Discussing_changes)
  section on the wiki.
* A large vibecoded PR puts a big burden on us as maintainers. We have to
  review all the submitted code line by line. Keep in mind that reviewing
  code we did not write is usually much harder than writing it ourselves.
  It gets even harder when the author cannot fully explain what the code
  does and why it was written that way.

A PR is meant for code that can actually be merged. A big chunk of code
that you cannot explain yourself does not belong there. If you want to
share it anyway, attach it to the issue on the bug tracker instead.

For smaller vibecoded changes that do make sense as a PR, you must disclose
the use of AI. You did not do a line by line review of the result (see
rule #2) and likely cannot answer the questions stated in rule #3 either.

### I know my way around UnrealIRCd source and used AI. Is that a problem?

No, it is not a problem, we use AI ourselves as well. And if you know
your way around the code, rule #3 should be no problem for you either.

However, if AI did most of the work with little input from you, or if you
did not review the resulting code line by line, then you must disclose AI
use in the PR. (Rule #2)

## Rules for UnrealIRCd coders with direct commit rights

You have commit rights, so we assume you know what you are doing.
However, direct commits are not reviewed by anyone else. When you use AI,
you should go through all code changes line by line and review them. If
you don't do that, then you MUST disclose AI use in the commit. That way
fellow coders know that a review in a particular file or area was not
done according to the usual standards. Preferably, you should not commit
such code at all, but this may be acceptable, e.g. during a particular
development stage when the final review is postponed to a later moment.

When you review a PR from a contributor, you go through the code line by
line, as always. You take into account any AI disclosure and whether the
contributor said they understand the code, which tells you how much extra
attention is needed. When you merge the PR, you write the final commit
message. It describes the change and refers to the PR (PR #xxx from xyz).
Any AI disclosure from the contributor stays in the PR, as you have dealt
with the risk via your line by line review. Do not leave any Assisted-by
or similar lines in the commit.

## See also

* `SECURITY.md` for reporting security issues (and AI use)
* [Rules for 3rd party modules](https://www.unrealircd.org/docs/Rules_for_3rd_party_modules_in_unrealircd-contrib)
  (including AI use there)
* Generic coding guidelines at
  [the wiki](https://www.unrealircd.org/docs/Dev:Coding_guidelines)
