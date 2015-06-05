# BitBuffer
Arduino BitBuffer a library to storing a large number of values within a certain range by keeping footprint in memory as small as possible.
Memory footprint reduction is achieved by defining a range for values and by only using up the space required for the defined range,
by performing internal bit shifting operations. BitBuffer API gives you a FIFO-based interface for which you don't have to care about
internal representation. Only thing you have to do is to define the range and the number of values kept for FIFO. Once this is done,
you can push values and pop them at a later point in time. Also index based access is possible with value remaining in store.
