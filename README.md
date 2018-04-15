# incbot -- Automatically generate C include directives

### Description

`incbot` scans a C source code file,
extracts all identifiers used,
then generates the appropriate `#include` directives.


### Why not use iwyu?

The program, `iwyu`, (Include What You Use)
uses clang/LLVM and actually goes through much
of the process of compiling a program.
It is, therefore more reliable.
It would not be fooled by some non-standard use of an identifier.
So, why not just use that?

`iwyu` only works on complete, C programs
that compile correctly.
If it is given an incomplete, or otherwise incorrect
C program, then it just bails out.

It is good for criticizing and refining already
well developed code.

That is useful, as far as it goes.
I use `iwyu` for that purpose.

But, when I am composing a new C program,
I want something that will work on partial
programs.  I want something that will _generate_
include directives, from scratch,
based on the identifiers that are used.

For that service, I am willing to accept the restriction
that I do not use an identifier in a way that conflicts
with the `incbot` identifier database.

I often use `incbot` to get things started,
then later use `iwyu` to act as a sort of `lint`
for include directives.

### Minimum Requirements for C code

It is important that the C source code
be syntactically correct at least with respect
to comments, double quotes, and single quotes.
A runaway comment or string can cause `incbot`
to err in either direction,
either too much or too little.

Other than that, it is pretty reasonably accurate,
even for C code that is far from ready to actually compile.
And that is _precisely_ when I need `incbot` the most.
It is during early, rapid development,
at which time `iwyu` cannot help.

### Accuracy of the incbot identifier tables

Many of the tables used by `incbot` were generated
by scripts that parse man pages.  They are not 100%
accurate.  Man pages are meant to be read, not parsed
by any program.  But, the results are still quite useful.

Work is contnuing to refine the man page parser.

C keywords, and identifiers from ISO standards and POSIX
standards are more reliable.

Much of the information about these identifiers was
scrape from

  http://www.schweikhardt.net/identifiers.html


Although some identifiers will be incorrectly classified,
quite often, the correct `#include` directive will still
be generated.  It is just that the comment
with the reason for needing that include file could be wrong.
For example, an identifier could be classified as a function,
when it is really a macro or a constant or a variable.

I am working on it.  But, `incbot` is useful enough,
as it is.

## License

See the file `LICENSE.md`

-- Guy Shaw

   gshaw@acm.org

