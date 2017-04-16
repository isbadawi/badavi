# badavi

[![Build Status](https://travis-ci.org/isbadawi/badavi.svg?branch=master)](https://travis-ci.org/isbadawi/badavi)
[![codecov](https://codecov.io/gh/isbadawi/badavi/branch/master/graph/badge.svg)](https://codecov.io/gh/isbadawi/badavi)

`badavi` is a vi-like terminal mode text editor, implemented in C and using the
[`termbox`](https://github.com/nsf/termbox) library to draw to the terminal.

It's meant to be a learning exercise and fun project to hack on rather than a
serious day-to-day editor, although who knows where it'll end up.

### Features supported so far

* Normal, insert, and visual modes.

* Motions -- `h`, `j`, `k`, `l`, `0`, `$`, `^`, `{`, `}`, `b`, `B`, `w`, `W`,
`e`, `E`, `G`, `g_`, `ge`, `gE`, `gg`, `%`, and more. Motions can be prefixed
with an optional count.

* `:` commands -- `:w [path]`, `:wq`, `:q`, `:e path` and more.

* Split windows with `:split` (horizontal) and `:vsplit` (vertical). You can
navigate between them with `<C-w> hjkl`, resize the current window with
`<C-w> +` and `<C-w> -`, and equalize the layout with `<C-w> =`.

* Delete (`d`), change (`c`) and yank (`y`) operators, which can be applied to
any of the motions, or the visual mode selection. (Text objects aren't
implemented yet). The affected region is saved into the unnamed register, used
by `p` to paste text. Named registers from `a` to `z` are also implemented, and
can be specified by prefixing the operator (or `p`) with `"a` through `"z`.

* Undo (`u`) and redo (`<c-r>`) (only single-level for now, unlike vim).

* `ctags` support -- on startup badavi looks for a tags file called `tags` in
the current directory (`'tags'` option not supported yet). The `:tag` command
jumps to the specified tag, and `<c-]>` jumps to the tag of the word under
the cursor. `<c-t>` and `:tag` can be used to walk up and down the tag stack.
The `-t` command line option can also be passed in to start editing at the
given tag, as in e.g. `badavi -t main`.

* A small subset of the options are implemented. You can manipulate them with
`:set`, `:setlocal` and `:setglobal`. These commands accept a similar syntax
as vim, e.g. `:set number`, `:set number!`, `:set nonumber`, `:set number?`,
etc. Options can be read from a file with `:source path`. At startup a file
called `~/.badavimrc` is sourced.

* Search forwards with `/`, backwards with `?`. Standard POSIX regexes are
used, so the syntax is not exactly the same as vim's. For instance, word
boundaries are specified with `[[:<:]]` and `[[:>:]]` instead of `\<` and
`\>`. `n` and `N` can be used to cycle through matches. `*` and `#` can be
used to search forwards or backwards for the next occurrence of the word
under the cursor. Searching is a motion, so it works with the operators. The
`'incsearch'` and `'hlsearch'` options are also implemented.

### Building

Just run `make`.

### License

MIT -- see `LICENSE` file for details.
