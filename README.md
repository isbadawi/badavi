# badavi

`badavi` is a vi-like terminal mode text editor, implemented in C and using the
[`termbox`](https://github.com/nsf/termbox) library to draw to the terminal.

It's meant to be a learning exercise and fun project to hack on rather than a
serious day-to-day editor, although who knows where it'll end up.

### Features supported so far

* Basic motions -- `h`, `j`, `k`, `l`, `0`, `$`, `^`, `{`, `}`, `b`, `B`,
`w`, `W`, `e`, `E`, `G`, `g_`, `ge`, `gE`, `gg`. Motions can be prefixed with
an optional count.

* Basic `:` commands -- `:w [filename]` to save, `:wq` to save and quit, `:q`
to quit, `:q!` to force quit even if there are unsaved changes, and `:e path`
to edit another file.

* Delete (`d`), change (`c`) and yank (`y`) operators, which can be applied to
any of the motions. (Text objects aren't implemented yet). The affected
region is saved into the unnamed register, used by `p` to paste text. Named
registers from `a` to `z` are also implemented, and can be specified by
prefixing the operator (or `p`) with `"a` through `"z`.

* Regex search with `/` (only forward search is implemented for now). Standard
POSIX regexes are used, so the syntax is not exactly the same as vim's. For
instance, word boundaries are specified with `[[:<:]]` and `[[:>:]]` instead
of `\<` and `\>`. `n` can be used to cycle through matches.

### Building

Just run `make`. The only dependency is `termbox`, which is easy to find --
packages surely exist for your favorite OS. On OS X, `brew install termbox`
will do.

### License

MIT -- see `LICENSE` file for details.
