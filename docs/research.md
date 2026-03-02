# SPSC overwrite, just w inf

```
N = 3

[0, 1, 2]

w = 2
r = 1

PRODUCER THREAD                                     CONSUMER THEAD
---------------                                     --------------

buffer[1] = "X"
w = 2 (2)

                                                    w == 2 (2)
                                                    r == 1
                                                    buffer[1] == "X"

w == 2 (2)
buffer[2] = "A"
w = 3 (0)

buffer[0] = "B"
w = 4 (1)

buffer[1] = "C"

                                                    buffer[1] == bud "X" nebo "C"
                                                    w == 4; 4 - 2 == 2: not enough to decide "this has rotated" !

w = 5 (2)

buffer[2] = "D"
w = 6 (0)
```


# SPSC overwrite, seq var 1

```
N = 3

buffer[0, 1, 2]
s[?, ?, ?]

w = 2
r = 1

PRODUCER THREAD                                     CONSUMER THEAD
---------------                                     --------------

buffer[1] = "X"
seq[1] = 1
w = 2 (2)

                                                    w == 2 (2)
                                                    r == 1
                                                    s1 = seq[1] == 1

w == 2 (2)
buffer[2] = "A"
w = 3 (0)

w = 4 (1)
buffer[0] = "B"

w = 4x
buffer[1] = "C"

                                                    buffer[1] == bud "X" nebo "C"
                                                    s2 = seq[1] == 1
                                                    w == 4x we are writing, re-read s2 when w != 4x

seq[1] = 4
w = 5 (2)

                                                    w == 5
                                                    seq[1] == 4 --> re-read buffer[1]

buffer[2] = "D"
w = 6 (0)
```


# SPSC overwrite, alter with & 0x1 == writing flag (x suffix)

```
N = 3

buffer[0, 1, 2]

w = 2
r = 1

PRODUCER THREAD                                     CONSUMER THEAD
---------------                                     --------------

w = 2x
buffer[1] = "X"
w = 2 (2)

                                                    w == 2 (2)
                                                    r == 1

w == 2 (2)
w = 3x
buffer[2] = "A"
w = 3 (0)

w = 4 (1)
buffer[0] = "B"

                                                    buffer[1] == "X"
                                                >   1: w == 4 , 4 - 2 < 3 --> no loop --> safe to use, no torn
w = 4x  <-- bez tohohle budeme cist w == 4, z toho nelze nic usoudit.
buffer[1] = "C"
                                                    buffer[1] == bud "X" nebo "C"
                                                >   2: w == 4x we are writing, wait for w != 4x
w = 5 (2)
                                                >   3: w == 5 , 5 - 2 >= 3 --> loop, re-read buffer[1]

w = 6x

buffer[2] = "D"
w = 6 (0)
```

