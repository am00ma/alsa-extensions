# Fifo

Following [FAbian's Realtime Box o' Tricks](https://github.com/hogliux/farbot)

read_or_writer:

- typename T
- bool is_writer
- bool single_consumer_producer
- bool overwrite_or_return_zero
- isize MAX_THREADS

1. Producers and consumers

| no  | single consumer or producer | failure mode       | posinfo | reserve | cas/rcu loop | comments     |
| --- | --------------------------- | ------------------ | ------- | ------- | ------------ | ------------ |
| 1   | true: single                | true: overwrite    | -       | yes     | -            | -            |
| 2   | false: multiple             | true: overwrite    | yes     | yes     | -            | -            |
| 1   | true: single                | false: return zero | -       | yes     | -            | -            |
| 2   | false: multiple             | false: return zero | yes     | yes     | yes          | default case |

2. Failure mode

| no  | failure mode | bool  |
| --- | ------------ | ----- |
| 1   | overwrite    | false |
| 1   | overwrite    | false |
| 2   | return zero  | true  |
| 2   | return zero  | true  |
