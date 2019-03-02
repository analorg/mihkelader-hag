# hag
Anagram finder for a competition

Linux program to find anagrams from http://www.eki.ee/tarkvara/wordlist/lemmad.zip

## Caveats
Heavily optimized for this exact wordlist. It assumes Windows newlines and the exact character set used in lemmad.txt.
Best used with multiple CPUs. Standard OpenMP environment variables can be used to tune multithreading if defaults are not suitable.

## Syntax
`hag <path-to-lemmad.txt> <word-to-find-anagrams-for>`
  
## Output
`<algorithm run time in microseconds>[,anagram]...`

`1822,sitkuma,kitsuma,tiksuma,timukas,tuiskam,mutikas`
  
## Building
gcc with O3, OpenMP and profiling recommended

```
rm hag.gcda
gcc -O3 -fopenmp -profile-generate hag.c
./a.out lemmad.txt teeneline
gcc -O3 -fopenmp -profile-use hag.c -o bin/hag
```
