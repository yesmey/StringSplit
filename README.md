# String Split

Strings splitted with SIMD. It's kind of silly actually.

It's possible to add several characters to be split by, and it will generate the appropriate code with some meta programming.

For example, a string split by 2 chars (passed through Args...) compiles into the following: (MSVC 14)

```asm
movdqa      xmm6,xmmword ptr [Args[0], Args[0], ...]
movdqa      xmm7,xmmword ptr [Args[1], Args[1], ...]
...
movdqu      xmm0,xmmword ptr [string ptr]
movdqa      xmm2,xmm6
pcmpeqb     xmm2,xmm0
movdqa      xmm1,xmm7
pcmpeqb     xmm1,xmm0
por         xmm1,xmm2
pmovmskb    edi,xmm1
test        edi,edi
```

Also, I've added a convenience class StringPiece to act like a string_view until it's no longer experimental.
