# WiscKey

Separating Key and Values in SSD- Conscious Storage is a paper published in 14th USENIX Conference on File and Storage Technologies in Feb 2016. I have implemented this on top LevelDB to understands the performance improvements. 

The idea is to separate the key from values to minimize the I/O amplification.

## Contribution:

I have created a program called wisckey_test.cc which uses LevelDB architecture to save Key and Value Offset. The actual value is saved in a logfile inside wisckey_test_dir which get created at run time. 

## Credit

WiscKey: Separating Keys from Values in SSD-conscious Storage
Authors: Lanyue Lu, Thanumalayan Sankaranarayana Pillai, Andrea C. Arpaci-Dusseau, and Remzi H. Arpaci-Dusseau, University of Wisconsin—Madison

Prof Song Jiang, University of Texas at Arlington.

## Secure Deletion over WiscKey
### Compile
```
make
```
### Execute with arguments for flash simulator 
```
./wisckey_test Example.log 4 16384 256 2048 64 1500 800 60 25 5 FTL 10 ART RW N 0 10000 200 512 512 500 4 8 8 10 2 2 8 4 0.0001 N Y 4 4 4 4 4 256 32 800 N
```
