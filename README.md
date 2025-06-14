# basic-shell
Basic unix shell implemented by following the [tutorial by Stephen Brennan](https://brennan.io/2015/01/16/write-a-shell-in-c/) with some additional features and tweaks.

## Compile and run
Compile and run the source code from the project directory after cloning.
```
gcc -o shell.out shell.c -Wall
./shell.out
```

## Additional Functionality
Some features added over the existing implementation from the tutorial are as follows:
1. Background process execution using `&` and management using additional helper functions and `sigchld_handler()`.

## Tweaks
Some changes compared to the original implementation of the tutorial:
1. Some additional error handling checks in the `launch()` function consistent with the documentation from the `man 2 wait` page.
2. Green colour for prompts.
