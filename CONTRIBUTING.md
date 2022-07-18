# Contributing to Goxel2


Thanks alot if you considered to contribute to goxel2, contributing to goxel2 isn't hard just ensure these things:

- Fake Patches Which Are Intended To Introduce New Bugs Will Not Be Tolerated
- Not Neccessary But If Possible Please Follow The Below Given Coding Style
- Not Neccessary But If Possible Make Sure To Use English in Comments or Anywhere in the Repository
- Make Sure you abide by the [Code of Conduct](CODE_OF_CONDUCT.md)

Contribution is not always has to be in form of Code, helping people in [Issues](https://github.com/pegvin/goxel2/issues) or in [Discussions](https://github.com/pegvin/goxel2/discussions).

all the contributors who made contributions in form of code will be list in the [AUTHORS](AUTHORS.md) file.

## Coding style

Try to follow the code style I used for Goxel, as I am unlikely to merge a pull
request that doesn't.  The coding style is almost the one used by the linux
kernel, but using four spaces for indentation (I also accept typedef).  When in
doubt, just look at other part of the code.  The most important rules are:

- Use tabs characters if possible.
- No trailing white space.
- Please Try to use [Camel Case](https://en.wikipedia.org/wiki/Camel_case) but others are allowed too
- Please Try to define functions and stuff like below given

```c
int my_func(void) {
    ...
    return 0;
}
```

- No space between function and argument parenthesis:

```c
func(10, 20) // Good
func (10, 20) // BAD!
```

- One space after keywords, except `sizeof`:

```c
if (something) // Good
if(something)  // BAD
```

- One space around binary operators, no space after unary operators and
  before postfix operators.

```c
x = 10 + 20 * 3; // Good
x = 10+20*3; // BAD
x++;  // Good
x ++; // BAD
y = &x; // Good
y = & x; // VERY BAD
```

## Git commit style

- Keep the summary line under about 50 characters.

- The rest of the commit message separated by a blank line.  Wrap lines at
  72 characters.

- Try to separate the commits into small logical parts.  For example if you
  need to add a new public function in order to fix a bug, the first commit
  should be about adding the function, and the second one about fixing the
  bug.
