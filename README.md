# Adaptive BeTree - Advanced Databases Fall 2023 - Group 2 (Marcus Adair, Harnoor Bagga, Gabriel Kerr)

Implementatation of Adaptive B Epsilon Tree in C++ where Epsilon changes on a per-node basis based on the recent workload distribution.


## BUILDING THE TEST PROGRAM


You can compile the tests using the provided Makefile: `make clean`.


## RUNNING THE ORIGINAL STARTED CODE TEST PROGRAM

Included with the project is a text file that will run 10,000 inserts and queries followed by 400 queries (and their correct results) for the final tree. The file is named `test_inputs.txt`. To use this file to test your tree, use a command like so:

```bash
./test -m test -d your_temp_dir -i test_inputs.txt -t 10400
```

NOTE: `-c 50` will set the checkpoint granularity to 50, and `-p 200` will set the persistence granularity to 200.

You can also add an optional -o flag to make sure the program has gone through all 10,400 operations and see the results of the various queries (e.g. `-o test_outputs.txt`).

## RUNNING THE TEST SCRIPT

We have also included a test script that will test crashing the program and resuming operation.

You can run the script by calling `bash ./TestScript.sh`. This will run through the test of running a crash and recovery. It will end the first test by making 400 queries at the end of the program based on the correct final state of the program.

When this test finishes, it will give you some results regarding the percentage of incorrect queries. Due to the potential of a crash losing a portion of your checkpoint data (as part of the checkpoint granularity), the final percentages may not be zero. We are looking for a value as close to zero as possible.


## RUNNING OUR CUSTOM BENCHMARKS

To generate keys, use the Python command:

```
python3 generate_keys.py
```

Utilize the generated key file for benchmark execution with the following commands:

For a read-heavy workload: 
```
./testing_reads -m benchmark-queries -d tmpdir
```
For a write-heavy workload:
```
./testing_writes -m benchmark-upserts -d tmpdir
```

Make sure that tmpdir is cleared before running tests.
