# System Programming: Threads and System V Semaphores

## Table of contents
* [General info](#general-info)
* [Setup](#setup)

## General info
This is my homework for System Programming class.

### Objective
There is one supplier thread and multiple consumer threads. The supplier brings materials, one by one. And the consumers consume them, two by two. Each actor is modeled by its own thread.

#### Input File
It contains in total NxC times the character ‘1’ and NxC times the character ‘2’ in an arbitrary order. Each character in the file corresponds to a type of material. ‘1’ corresponds to the first material, and ‘2’ corresponds to the second material.

#### Supplier
There is only one supplier thread. It read the input file’s contents, one character at a time and output a message concerning its activity. For every character/material read from the file, it posts a semaphore representing its amount. If it reads a ‘1’ it posts the semaphore representing the amount of ‘1’s read so far, and if it reads a ‘2’ it posts the semaphorere presenting the amount of ‘2’s read so far. It terminates once it reaches the end of the file. The supplier is a detached thread.

#### Consumers
Each consumer have its own thread (not detached). Its task is to loop N times. At each iteration it takes one ‘1’ and one ‘2’ by reducing the corresponding semaphores’ values. It will either take two items (one ‘1’ and one ‘2’) or wait until two (one ‘1’ and one ‘2’) are available. This way each consumer will consume in total exactly N x ‘1’s and N x ‘2’s.

## Setup
To run this project in terminal:

```
$ cd ../Threads-and-SystemVSemaphores
$ make
$ ./hw4 -C 10 -N 5 -F input
```
You can change C and N values. C is consumer count, N is loop count. (N > 1 and C > 4)
